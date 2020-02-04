#pragma once
#include <aether/common/base_protocol.hh>
#include <aether/generic-netcode/trivial_marshalling.hh>

struct trivial_marshalling_traits {
   using per_worker_data_type = protocol::base::client_message;
   using entity_type = protocol::base::net_point_3d;
   using static_data_type = std::monostate;
};

using marshalling_factory = aether::netcode::trivial_marshalling<trivial_marshalling_traits>;
