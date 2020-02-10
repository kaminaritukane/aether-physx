#include "simulate.hh"
#include "protocol.hh"

#include <aether/cell_state.hh>
#include <aether/common/net.hh>
#include <aether/common/base_protocol.hh>
#include <aether/common/random.hh>
#include <unordered_map>
#include<typeinfo>
#include<cstring>


using physx::PxVec3, physx::PxPlane, physx::PxU32, physx::PxTransform,
      physx::PxReal, physx::PxCreateStatic, physx::PxQuat;

vec3f transform_to_vec3f(physx::PxTransform t) {
    return vec3f_new(t.p.x, t.p.y, t.p.z);
} 

static physx::PxFilterFlags BasicFilterShader( physx::PxFilterObjectAttributes,
                                               physx::PxFilterData,
                                               physx::PxFilterObjectAttributes,
                                               physx::PxFilterData,
                                               physx::PxPairFlags& pairFlags,
                                               const void*,
                                               PxU32) {
    // generate contacts for all
    pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
    // trigger the contact callback for any collision
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;

    return physx::PxFilterFlag::eDEFAULT;
}

// This is an ECS system, it calls the PhysX simulate tick on the worker once per tick.
struct physx_update_system {
    using accessed_components = std::tuple<c_physx, c_trivial>;
    using ecs_type = aether::constrained_ecs<user_cell_state, accessed_components>;
    void operator()(const aether_cell_state<octree_traits> &aether_state, ecs_type &state, float delta_time) {
        aether::physx::physx_state *physx_state = static_cast<aether::physx::physx_state*>(state.user_data);
        physx_state->scene->simulate(delta_time);
        physx_state->scene->fetchResults(true);
    }
};

std::unordered_map<physx::PxActor*, user_cell_state::agent_reference> gPxActorToEntity;

class ContactReportCallback: public ::physx::PxSimulationEventCallback
{
	void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count)	{ PX_UNUSED(constraints); PX_UNUSED(count); }
	void onWake(physx::PxActor** actors, physx::PxU32 count)							{ PX_UNUSED(actors); PX_UNUSED(count); }
	void onSleep(physx::PxActor** actors, physx::PxU32 count)							{ PX_UNUSED(actors); PX_UNUSED(count); }
	void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)					{ PX_UNUSED(pairs); PX_UNUSED(count); }
	void onAdvance(const physx::PxRigidBody*const*, const physx::PxTransform*, const physx::PxU32) {}
	void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) 
	{
		for(physx::PxU32 i=0; i < nbPairs; i++)
        {
            const physx::PxContactPair& cp = pairs[i];

            if(cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
            {
                physx::PxActor* actor0 = pairHeader.actors[0];
                physx::PxActor* actor1 = pairHeader.actors[1];
                printf("actor0: %d, actor1: %d\n", actor0, actor1);

                auto it0 = gPxActorToEntity.find(actor0);
                printf("it0 found: %d\n", it0 != gPxActorToEntity.end());
                if ( it0 != gPxActorToEntity.end() )
                {
                    auto agent0 = it0->second;
                    auto triComp = agent0.get_dynamic<c_trivial>();
                    triComp->agent_colour = {0.0f, 0.0f, 1.0f};
                    //agent0.set
                    printf("Set agent0: %d color\n", triComp->id);
                }

                auto it1 = gPxActorToEntity.find(actor1);
                printf("it1 found: %d\n", it1 != gPxActorToEntity.end());
                if ( it1 != gPxActorToEntity.end() )
                {
                    auto agent1 = it1->second;
                    auto triComp = agent1.get_dynamic<c_trivial>();
                    triComp->agent_colour =  {0.0f, 0.0f, 1.0f};
                    printf("Set agent1: %d color\n", triComp->id);
                }
            }
        }
	}
};

ContactReportCallback gContactReportCallback;

// This is the cell tick function as described in main.cc. It calls the ECS internal tick function
void cell_tick(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state, float delta_time) {
    state.tick(aether_state, delta_time);
}

// This is called once when a new worker is spawned into the simualation and assigned an area of simulation space to 
// cover, In this simulation we have the world bounds on the cubes stored in each worker rather than as entities and 
// we use this function to ensure each worker has a copy.
void initialise_cell(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state) {
    state.user_data = new aether::physx::physx_state(static_cast<::physx::PxSimulationEventCallback*>(state.user_data));
    state.add_system<physx_update_system>();
    aether::physx::physx_state *physx_state = static_cast<aether::physx::physx_state*>(state.user_data);;

    // Creating the world bounds. The world is bounded by 6 planes Making a box
    // You must ensure that you release the material after it is used to create a shape
    physx::PxMaterial* zPositiveMaterial = physx_state->physics->createMaterial(0.0f,0.0f,1.0f);
    auto zPositivePlane = physx::PxCreatePlane(*physx_state->physics, PxPlane(0,0,1,150), *zPositiveMaterial);
    zPositiveMaterial->release();
    physx_state->scene->addActor(*zPositivePlane);

    physx::PxMaterial* zNegativeMaterial = physx_state->physics->createMaterial(0.0f,0.0f,1.0f);
    auto zNegativePlane = physx::PxCreatePlane(*physx_state->physics, PxPlane(0,0,-1,150), *zNegativeMaterial);
    zNegativeMaterial->release();
    physx_state->scene->addActor(*zNegativePlane);

    physx::PxMaterial* yPositiveMaterial = physx_state->physics->createMaterial(0.0f,0.0f,1.0f);
    auto yPositivePlane = physx::PxCreatePlane(*physx_state->physics, PxPlane(0,1,0,150), *yPositiveMaterial);
    yPositiveMaterial->release();
    physx_state->scene->addActor(*yPositivePlane);

    physx::PxMaterial* yNegativeMaterial = physx_state->physics->createMaterial(0.0f,0.0f,1.0f);
    auto yNegativePlane = physx::PxCreatePlane(*physx_state->physics, PxPlane(0,-1,0,150), *yNegativeMaterial);
    yNegativeMaterial->release();
    physx_state->scene->addActor(*yNegativePlane);

    physx::PxMaterial* xPositiveMaterial = physx_state->physics->createMaterial(0.0f,0.0f,1.0f);
    auto xPositivePlane = physx::PxCreatePlane(*physx_state->physics, PxPlane(1,0,0,150), *xPositiveMaterial);
    xPositiveMaterial->release();
    physx_state->scene->addActor(*xPositivePlane);

    physx::PxMaterial* xNegativeMaterial = physx_state->physics->createMaterial(0.0f,0.0f,1.0f);
    auto xNegativePlane = physx::PxCreatePlane(*physx_state->physics, PxPlane(-1,0,0,150), *xNegativeMaterial);
    xNegativeMaterial->release();
    physx_state->scene->addActor(*xNegativePlane);
}

void deinitialise_cell(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state) {
    state.clear();
    delete static_cast<aether::physx::physx_state*>(state.user_data);
}

// This function is called once at the start of the simulation, it is called on the initial worker. Any entities it 
// creates are then sent to workers that cover their area of space. 
// For this demo we create 500 PhysX cubes with random rotation, position and velocity
void initialise_world(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state) {
    const auto cell = aether_state.get_cell();
    aether::physx::physx_state *physx_state = static_cast<aether::physx::physx_state*>(state.user_data);;

    physx_state->scene->setSimulationEventCallback(&gContactReportCallback);

    // static friction, dynamic friction, restitution; COR = 1 means perfectly elastic collision
    // NOTE: currently serializer requires every entity to have a separate material and shape, so identical materials
    // and shapes must be defined for each entity
    for(int i = 0; i < 10; i++){
      //float size_rnd = generate_random_f32();
      float size = 1.0f;//(size_rnd*3) + 1;
      PxReal extent(size);
      physx::PxMaterial* material = physx_state->physics->createMaterial(0.0f, 0.0f, 1.0f);
      physx::PxShape* actor_shape = physx_state->physics->createShape(physx::PxBoxGeometry(extent, extent, extent), *material);

      physx::PxTransform actor_position = physx::PxTransform(physx::PxVec3((generate_random_f32()-0.5f)*200, 
        (generate_random_f32()-0.5f)*200, 
        (generate_random_f32()-0.5f)*200)
      );
      physx::PxRigidDynamic* actor =  physx_state->physics->createRigidDynamic(actor_position);
      actor->attachShape(*actor_shape);
      physx::PxRigidBodyExt::updateMassAndInertia(*actor, 10.0f);
      actor->setLinearVelocity(4.0f*PxVec3((generate_random_f32()-0.5f)*10,(generate_random_f32()-0.5f)*40,(generate_random_f32()-0.5f)*10));

      auto update = state.create_update_set();

      auto agent = update.new_entity_local();
      auto physx = agent.create_component<c_physx>();
      physx->add_actor(*actor);
      physx->add_to_simulation();
      auto trivial = agent.create_component<c_trivial>();
      trivial->id = i;
      trivial->size = size;
      //trivial->agent_colour.r *= size_rnd;

      //const std::type_info& typeOfAgent = typeid(agent);
      //printf("agent type: %s\n", typeOfAgent.name());

      gPxActorToEntity.insert({ dynamic_cast<physx::PxActor*>(actor), agent});

      // Unlike most physX applications, we need to maintain a shape and material for each entity,
      // rather than using the same material across many entities.
      material->release();
      actor_shape->release();
    }
}

// This code handles how data leaves the simulation to the client, as described in main.cc. 
// Packets consist of a header describing the aether cell, and then a list of entities in the 
// format protocol::base::net_point_3d
void cell_state_serialize(const aether_cell_state<octree_traits>& aether_state, const user_cell_state &state, client_writer_type &writer) {
    protocol::base::client_message header;
    const auto cell = aether_state.get_cell();

    auto marshaller = marshalling_factory().create_marshaller();
    marshaller.reserve(state.num_agents_local());

    header.cell = cell;
    header.cell.dimension = 3;
    header.cell.pid = static_cast<uint64_t>(hadean::pid::get());
    header.stats.num_agents = state.num_agents_local();
    header.stats.num_agents_ghost = state.num_agents_ghost();
    header.cell_dying = aether_state.is_cell_dying();
    marshaller.add_worker_data(aether_state.get_worker().as_u64(), header);

    for (auto agent: state.local_entities<c_physx, c_trivial>()) {
        auto physx = agent.get<c_physx>();
        auto trivial = agent.get<c_trivial>();

        PxTransform t = physx->actor->getGlobalPose();
        vec3f position = transform_to_vec3f(t);
        protocol::base::net_quat q;
        q.x = t.q.x;
        q.y = t.q.y;
        q.z = t.q.z;
        q.w = t.q.w;

        protocol::base::net_point_3d point;
        point.net_encoded_position = position;
        point.net_encoded_color = net_encode_color(trivial->agent_colour);
        point.net_encoded_orientation = q;
        point.id = trivial->id;
        point.size = trivial->size;
        marshaller.add_entity(point);
    }

    const auto data = marshaller.encode();
    writer.push_bytes(&data[0], data.size());
    writer.send();
}

// We have no input in this simulation, so this remains empty
void handle_events(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state, message_reader_type &reader) {
    //printf("11111111.\n");
}
