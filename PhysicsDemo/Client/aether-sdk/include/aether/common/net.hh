#pragma once

#include <cstdint>
#include <cmath>

#include "colour.hh"
#include <aether/common/packing.hh>

inline uint8_t float_to_u8(float v) {
    v = v < 0.0 ? 0.0 : v;
    v = v > 1.0 ? 1.0 : v;
    return (uint8_t) round(v * 255.0);
}

inline float u8_to_float(uint8_t v) {
    return  (v * 1.0) / 255.0;
}

inline uint32_t net_encode_color(struct colour c) {
    uint32_t result = 0;
    result += ((uint32_t) float_to_u8(c.r)) << 16;
    result += ((uint32_t) float_to_u8(c.g)) << 8;
    result += ((uint32_t) float_to_u8(c.b));
    return result;
}

inline struct colour net_decode_color(uint32_t c) {
    struct colour result;
    result.r = u8_to_float((c >> 16) & 255);
    result.g = u8_to_float((c >> 8) & 255);
    result.b = u8_to_float(c & 255);
    return result;
}

HADEAN_PACK(struct net_quat {
    // don't change the order without adjusting all usages
    float x, y, z, w;
});
