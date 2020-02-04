#pragma once

#include <chrono>
#include <cstdint>
#include "vector.hh"
#include "morton/encoding.hh"
#include "morton/cell.hh"
#include <aether/common/packing.hh>

namespace protocol {

namespace base {

using net_position_2d = vec2f;
using net_position_3d = vec3f;

enum entity_flags : uint32_t {
    is_owned   = (1 << 0),
    is_dropped = (1 << 1),
    is_dead    = (1 << 2)
};

HADEAN_PACK(struct net_tree_cell {
    uint64_t code;
    uint64_t level;
    uint64_t pid;
    uint8_t dimension;

    net_tree_cell() = default;

    template<typename MortonCode>
    net_tree_cell(const tree_cell<MortonCode> &cell) :
      code(cell.code), level(cell.level), dimension(MortonCode::dimension) {
    }
});

HADEAN_PACK(struct net_point_2d {
    net_position_2d net_encoded_position;
    uint32_t net_encoded_color;
    uint64_t id;
    uint32_t owner_id;
    uint32_t flags = 0;
});

static uint64_t get_entity_id(const net_point_2d &entity) {
    return entity.id;
}

static std::optional<uint64_t> get_owner_id(const net_point_2d &entity) {
    if ((entity.flags & entity_flags::is_owned) != 0) {
      return { entity.owner_id };
    } else {
      return std::nullopt;
    }
}

static vec2f get_position(const net_point_2d &entity) {
    return entity.net_encoded_position;
}

static void synthesize_dead_entity(const uint64_t id, struct net_point_2d &entity) {
    entity.id = id;
    entity.flags |= entity_flags::is_dead;
}

static bool is_entity_dead(const struct net_point_2d &entity) {
    return (entity.flags & entity_flags::is_dead) != 0;
}

static void synthesize_drop_entity(struct net_point_2d &entity) {
    entity.flags |= entity_flags::is_dropped;
}

static bool is_entity_dropped(const struct net_point_2d &entity) {
    return (entity.flags & entity_flags::is_dropped) != 0;
}

HADEAN_PACK(struct net_quat {
    float x, y, z, w;
});

HADEAN_PACK(struct net_point_3d {
    net_position_3d net_encoded_position;
    net_quat net_encoded_orientation;
    uint32_t net_encoded_color;
    uint64_t id;
    uint32_t owner_id;
    float size;
    uint32_t flags = 0;
});

static uint64_t get_entity_id(const net_point_3d &entity) {
    return entity.id;
}

static std::optional<uint64_t> get_owner_id(const net_point_3d &entity) {
    if ((entity.flags & entity_flags::is_owned) != 0) {
      return { entity.owner_id };
    } else {
      return std::nullopt;
    }
}

static vec3f get_position(const net_point_3d &entity) {
    return entity.net_encoded_position;
}

static void synthesize_dead_entity(const uint64_t id, struct net_point_3d &entity) {
    entity.id = id;
    entity.flags |= entity_flags::is_dead;
}

static bool is_entity_dead(const struct net_point_3d &entity) {
    return (entity.flags & entity_flags::is_dead) != 0;
}

static void synthesize_drop_entity(struct net_point_3d &entity) {
    entity.flags |= entity_flags::is_dropped;
}

static bool is_entity_dropped(const struct net_point_3d &entity) {
    return (entity.flags & entity_flags::is_dropped) != 0;
}

HADEAN_PACK(struct client_stats {
    uint64_t num_agents;
    uint64_t num_agents_ghost;
});

HADEAN_PACK(struct client_message {
    net_tree_cell cell;
    bool cell_dying;
    client_stats stats;
});

static net_position_2d net_encode_position_2f(const vec2f &v, const net_tree_cell &cell) {
    return v;
}

static net_position_3d net_encode_position_3f(vec3f v, const net_tree_cell &cell) {
    return v;
}

static vec2f net_decode_position_2f(const net_position_2d &p) {
    return p;
}

static vec3f net_decode_position_3f(const net_position_3d &p) {
    return p;
}

enum aether_event_type_t {
  EVENT_CURSOR_MOVE = 0,
  EVENT_MOUSE_CLICK = 1,
  EVENT_DEL_AGENT = 2,
};

enum aether_button_action_t {
  BUTTON_PRESSED  = 0,
  BUTTON_RELEASED = 1,
};

HADEAN_PACK(struct aether_screen_pos_t {
  float x;
  float y;
});

HADEAN_PACK(struct aether_mouse_click_t {
  uint8_t button;
  uint8_t action;
  aether_screen_pos_t position;
});

HADEAN_PACK(struct aether_cursor_move_t {
  aether_screen_pos_t position;
});

HADEAN_PACK(struct aether_del_agent_t {
  uint32_t id;
});

typedef HADEAN_PACK(struct aether_event_t {
  aether_event_type_t type;
  union {
    aether_mouse_click_t mouse_click;
    aether_cursor_move_t cursor_move;
    aether_del_agent_t del_agent;
  };
});

static_assert(std::is_trivially_copyable<net_point_2d>::value, "net structs must be trivially copyable");
static_assert(std::is_trivially_copyable<net_point_3d>::value, "net structs must be trivially copyable");
static_assert(std::is_trivially_copyable<client_message>::value, "net structs must be trivially copyable");

}

}
