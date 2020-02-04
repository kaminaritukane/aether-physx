#pragma once

#include <type_traits>

#define AETHER_SERDE_DERIVE_TRIVIAL_SERIALIZE(T) \
    template<typename SD> \
    void serde_load(SD &sd, T &value) { \
        sd.visit_bytes(&value, sizeof(T)); \
    } \

#define AETHER_SERDE_DERIVE_TRIVIAL_DESERIALIZE(T) \
    template<typename SD> \
    void serde_save(SD &sd, T &value) { \
        sd.visit_bytes(&value, sizeof(T)); \
    }

#define AETHER_SERDE_DERIVE_TRIVIAL(T) \
    AETHER_SERDE_DERIVE_TRIVIAL_SERIALIZE(T) \
    AETHER_SERDE_DERIVE_TRIVIAL_DESERIALIZE(T)
