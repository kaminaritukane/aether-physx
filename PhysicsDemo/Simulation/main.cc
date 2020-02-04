#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <aether/common/timer.hh>
#include <aether/arguments.hh>

#include "simulate.hh"
#include <aether/cell_state.hh>
#include <aether/octree_params.hh>
#include <aether/manager.hh>

#include <sys/prctl.h>

#include <fmt/format.h>

namespace timer = aether::timer;
using millis = std::chrono::duration<float, std::milli>;

int main(int argc, char *argv[]) {
    hadean::init();
    const char *process_name = "AE_Manager";
    prctl(PR_SET_NAME, process_name);
    aether::log::init(process_name, hadean::pid::get());
    hadean::log::set_level(hadean::log::level::INFO);

    // Here we set up the initial arguments to Aether
    struct arguments arguments;
    // initial workers to spawn
    arguments.workers = 8;
    // how many ticks to run for (0 = unlimited ticks)
    arguments.ticks = 0;
    // how many ticks per second
    arguments.tickrate = 15;
    arguments.realtime = true;
    // how large the initial cell is in morton code terms - the side length of a cell is = 2^cell_level, volume = (2^dimension)^cell_level
    arguments.cell_level = 6;
    argument_parse(argc, argv, &arguments);

    auto static_args = arguments.to_octree_params<octree_traits>();
    static_args.feature_flags = OPTIMISE_AABBS | FAST_MODE | PHASE_BARRIERS;

    // in build_user_state we assign the functions to create and destroy cells ((de)initialise_cell)
    // serialise a cell to the client (serialize_to_client)
    // initial world state, which is run once on the first worker - (initialise_world)
    // the tick function for the simulation - (cell_tick)
    // the events handler for client input (handle_events)
    // ================================================================================================================
    // we also define some functions for determining entity state - 
    // agent_aabb - this determines the size of an entity for Aether - the larger this is the further away an entity is
    // visible in neighbouring cells, in this case it is a cube 2.0f on each side.
    // agent_centre - this determines the centre of an entity for Aether - the centre determines when ownership of an
    // entity is handed over to a neighbouring cell. If this centre coordinate is outside the agent_aabb then the 
    // behaviour of aether is undefined and entities may despawn
    static_args.build_user_state = [](const aether_cell_state<octree_traits> &aether_state) -> std::unique_ptr<user_state<octree_traits>> {
        using user_state_type = entity_store_wrapper<entity_store_traits<octree_traits>>;
        user_state_type::params_type params{};

        params.initialise_cell = &initialise_cell;
        params.deinitialise_cell = &deinitialise_cell;
        params.serialize_to_client = &cell_state_serialize;
        params.initialise_world = &initialise_world;
        params.cell_tick = &cell_tick;
        params.handle_events = &handle_events;

        params.agent_aabb = [](const auto &aether_state, const user_cell_state& state, user_cell_state::agent_reference agent) -> auto {
            physx::PxBounds3 b = agent.get_dynamic<c_physx>()->actor->getWorldBounds();
            physx::PxVec3 p = agent.get_dynamic<c_physx>()->actor->getGlobalPose().p;
            const auto rnd = 1.0f;
            return aether::morton::AABB<morton_code<3, 21>>{
                morton_3_encode(vec3f{p.x - rnd, p.y - rnd, p.z - rnd}),
                morton_3_encode(vec3f{p.x + rnd, p.y + rnd, p.z + rnd}),
            };
        };

        params.agent_center = [](const auto &aether_state, const user_cell_state& state, user_cell_state::agent_reference agent) -> auto {
            physx::PxVec3 p = agent.get_dynamic<c_physx>()->actor->getGlobalPose().p;
            return morton_3_encode(vec3f{p.x, p.y,p.z});
        };

        return std::unique_ptr<user_state<octree_traits>>(
            new user_state_type(params, aether_state)
        );
    };


    using octree_params_type = octree_params_default<octree_traits>;
    aether::octree<octree_params_type> octree(arguments.workers, static_args);
    // Here we add muxers to the simulation - muxers are responsible for sending messages from the simulation to the
    // client
    for (const auto &muxer : arguments.muxers) {
        octree.add_muxer(muxer);
    }

    // Set the log level for aether - possible values are
    //  Fatal
    //  Error
    //  Warning
    //  Info
    //  Debug
    //  Trace
    //  It is not advised to use Trace as it may cause excessive logs.
    aether::log::set_level(aether::log::level::INFO);

    // Here we run the core simulation loop, this is essentially just a call to master_tick()
    for (uint64_t tick = 0; arguments.ticks == 0 || tick < arguments.ticks; tick++) {
        auto loop_time = timer::get();
        AETHER_LOG(INFO)(fmt::format("Hello from tick {}", tick+1));
        octree.master_tick();
        if (arguments.realtime) {
            // Here we track the time spent of each section of a tick, this is printed to the main logs
            using namespace std::chrono_literals;
            loop_time = timer::add(loop_time, static_cast<std::chrono::nanoseconds>(1s) / static_args.ticks_per_second);
            timer::sleep_until(loop_time);
        }
    }
    printf("PhysicsDemo completed successfully\n"); fflush(stdout);
    return 0;
}
