#pragma once

#include <inttypes.h>
#include "packing.hh"

namespace aether {

namespace message_encoding {

namespace client {

enum message_type {
    AUTHENTICATE,
    INTERACTION,
};

HADEAN_PACK(
struct header {
    message_type msg_type;
    uint32_t payload_size;
});

}

}

}
