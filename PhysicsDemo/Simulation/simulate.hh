#pragma once
#include <cstdlib>
#include <memory>
#include <set>

#include <aether/types.hh>
#include <aether/handover.hh>
#include <aether/octree_params.hh>
#include <aether/demo/physx/physx.hh>
#include <aether/demo/ecs/ecs.hh>
using c_physx = aether::physx::physx_c;
#include <aether/common/serde_derive.hh>

#include <PxPhysics.h>
#include <PxPhysicsAPI.h>
#include <PxRigidDynamic.h>
#include <PxShape.h>
#include <characterkinematic/PxControllerManager.h>
#include <common/PxCollection.h>
#include <extensions/PxSerialization.h>
#include <extensions/PxDefaultStreams.h>
#include <common/PxSerialFramework.h>

#include <signal.h>

// A common simple component used by all entities in this demo. We give it a colour to see 
struct c_trivial {
    struct colour agent_colour = {1.0f, 0.0f, 0.0f};
    uint64_t id;
    float size;
};
// This derives a trivial serializer/deserializer for pod components
// This should only be used for memcpy-able components (no lists)
// It is necessary for each component to have a serializer/deserializer
// for Aether to function
AETHER_SERDE_DERIVE_TRIVIAL(c_trivial);

// Setting up the ECS - we list the possible component types that may appear in the ECS,
// and the morton dimension (3,21 for 3d simmulations, 2,32 for 2d)
using component_types = std::tuple<c_physx, c_trivial>;
using octree_traits = aether::octree_traits<morton_code<3, 21>>;
using user_cell_state = aether::ecs<octree_traits, component_types>;

using octree_params_type = octree_params_default<octree_traits>;

// These are the functions used to initalise the world and setup Aether in main.cc, they are described in more detail in main.cc 
void initialise(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state);
void initialise_world(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state);
void initialise_cell(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state);
void handle_events(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state, message_reader_type &reader);
void cell_tick(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state, float delta_time);
void cell_state_serialize(const aether_cell_state<octree_traits>& aether_state, const user_cell_state &state, client_writer_type &writer);
void deinitialise_cell(const aether_cell_state<octree_traits> &aether_state, user_cell_state &state);


//The following is some default setup code, it is explained in more detail in the Aether Documentation
template<typename Writer>
struct agent_serializer {
    using writer_type = Writer;
    user_cell_state &state;
    writer_type &writer;
    user_cell_state::serialization_context<writer_type> serialization_context;

    agent_serializer(user_cell_state&, writer_type &_writer);
    int serialize(user_cell_state::agent_reference agent);
};

template<typename Reader>
struct agent_deserializer {
    using reader_type = Reader;
    user_cell_state &state;
    reader_type &reader;
    user_cell_state::deserialization_context<reader_type> deserialization_context;

    agent_deserializer(user_cell_state&, reader_type &_reader);
    user_cell_state::agent_reference deserialize();
};

template<typename Reader>
agent_deserializer<Reader>::agent_deserializer(user_cell_state &_state, reader_type &_reader): state(_state), reader(_reader), deserialization_context(state.create_deserialization_context(reader)) {
}

template<typename Reader>
user_cell_state::agent_reference agent_deserializer<Reader>::deserialize() {
    return deserialization_context.deserialize_entity();
}

template<typename Writer>
agent_serializer<Writer>::agent_serializer(user_cell_state &_state, writer_type &_writer) :
    state(_state), writer(_writer), serialization_context(state.create_serialization_context(writer)) {}

template<typename Writer>
int agent_serializer<Writer>::serialize(user_cell_state::agent_reference entity) {
    return serialization_context.serialize_entity(entity);
}

template<typename OctreeTraits>
struct entity_store_traits {
    using octree_traits = OctreeTraits;
    using store_type = user_cell_state;
    using handover_type = aether::default_handover;
    template<typename Writer>
    using serializer_type = agent_serializer<Writer>;
    template<typename Reader>
    using deserializer_type = agent_deserializer<Reader>;
};
