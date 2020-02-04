#pragma once

#include "../math_utils.hh"

#include <cstdint>
#include <cassert>

#include "encoding.hh"
#include <immintrin.h>

namespace aether {

namespace morton {

inline uint64_t fast_log2(const uint64_t x) {
    assert(x != 0);
    return sizeof(x) * 8 - 1 - __builtin_clzll(x);
}

template<uint32_t Dimension, uint32_t BitsPerDimension>
// This gives the maximum alignment level possible for a given number,
// e.g. given 2 it returns 0, given 8 it returns 1, given 0 it returns 32.
uint64_t get_max_align_level(const uint64_t code){
    return code == 0 ? BitsPerDimension : __builtin_ctzll(code) / Dimension;
}

template<uint32_t Dimension>
// Returns the level size required for two points to be in the same cell
uint64_t get_unifying_level(const uint64_t a, const uint64_t b){
    // the gets the maximum allowed for this range, max+1
    return a == b ? 0 : (fast_log2(a ^ b)/Dimension) +1;
}

template<uint32_t Dimension>
// Returns the  morton code for a given level, starting from 0;
uint64_t get_morton_code(const uint64_t level) {
    return (1LLU << level * Dimension) -1;
}

template<uint32_t Dimension>
// given a morton value it will return the morton aligned value of size level that contains it.
// e.g. given 14,1 it will give 12. given 14,2 it will give 0.
uint64_t get_parent_morton_aligned(const uint64_t code, uint32_t level) {
    return (code >> (Dimension * level)) << (Dimension * level);
}

template<uint32_t Dimension, uint32_t BitsPerDimension>
// given a range this will return the next morton aligned value
uint64_t get_align_max(const uint64_t min, const uint64_t max) {
    assert(max >= min);
    if (max == min) return min;
    // this gets the maximum allowed for this value
    uint64_t align_max = (min != 0) ?
          min + get_morton_code<Dimension>(get_max_align_level<Dimension,BitsPerDimension>(min))
        : std::numeric_limits<uint64_t>::max();
    uint64_t max_align = min + get_morton_code<Dimension>((fast_log2(max+1 - min)/Dimension));
    return std::min(align_max,max_align);
}


static const uint64_t __morton_2_x_mask = 0x5555555555555555;
static const uint64_t __morton_2_y_mask = 0xaaaaaaaaaaaaaaaa;

static const uint64_t __morton_3_x_mask = 0x1249249249249249;
static const uint64_t __morton_3_y_mask = 0x2492492492492492;
static const uint64_t __morton_3_z_mask = 0x4924924924924924;

template<typename Integer>
constexpr Integer pdep(Integer v, Integer mask) {
    static_assert(sizeof(Integer) == 4 || sizeof(Integer) == 8);
    if constexpr (sizeof(Integer) == 4)
        return _pdep_u32(v, mask);
    else if constexpr (sizeof(Integer) == 8)
        return _pdep_u64(v, mask);
}

template<typename Integer>
constexpr Integer pext(Integer v, Integer mask) {
    static_assert(sizeof(Integer) == 4 || sizeof(Integer) == 8);
    if constexpr (sizeof(Integer) == 4)
        return _pext_u32(v, mask);
    else if constexpr (sizeof(Integer) == 8)
        return _pext_u64(v, mask);
}

template<typename Integer>
constexpr Integer expand_bits_2(Integer v) {
#ifdef __BMI2__
    return pdep(v, __morton_2_x_mask);
#else
    static_assert(sizeof(Integer) == 8, "Efficient non-64-bit non-BMI2 expand_bits_2 is not yet implemented");
    v = (v ^ (v << 16)) & 0x0000ffff0000ffff;
    v = (v ^ (v << 8))  & 0x00ff00ff00ff00ff;
    v = (v ^ (v << 4))  & 0x0f0f0f0f0f0f0f0f;
    v = (v ^ (v << 2))  & 0x3333333333333333;
    v = (v ^ (v << 1))  & 0x5555555555555555;
    return v;
#endif
}

template<typename Integer>
constexpr Integer compact_bits_2(Integer v) {
#ifdef __BMI2__
    return pext(v, __morton_2_x_mask);
#else
    static_assert(sizeof(Integer) == 8, "Efficient non-64-bit non-BMI2 compact_bits_2 is not yet implemented");
    v &= 0x5555555555555555;
    v = (v ^ (v >>  1))  & 0x3333333333333333;
    v = (v ^ (v >>  2))  & 0x0f0f0f0f0f0f0f0f;
    v = (v ^ (v >>  4))  & 0x00ff00ff00ff00ff;
    v = (v ^ (v >>  8))  & 0x0000ffff0000ffff;
    v = (v ^ (v >>  16)) & 0x00000000ffffffff;
    return v;
#endif
}

template<typename Integer>
constexpr Integer expand_bits_3(Integer v) {
#ifdef __BMI2__
    return pdep(v, __morton_3_x_mask);
#else
    v = v & 0x00000000001fffff;
    v = (v | v << 32) & 0x001f00000000ffff;
    v = (v | v << 16) & 0x001f0000ff0000ff;
    v = (v | v << 8) &  0x100f00f00f00f00f;
    v = (v | v << 4) &  0x10c30c30c30c30c3;
    v = (v | v << 2) &  0x1249249249249249;
    return v;
#endif
}

template<typename Integer>
constexpr Integer compact_bits_3(Integer v) {
#ifdef __BMI2__
    return pext(v, __morton_3_x_mask);
#else
	v &= 0x1249249249249249;
	v = (v ^ (v >> 2))  & 0x30c30c30c30c30c3;
	v = (v ^ (v >> 4))  & 0xf00f00f00f00f00f;
	v = (v ^ (v >> 8))  & 0x00ff0000ff0000ff;
	v = (v ^ (v >> 16)) & 0x00ff00000000ffff;
	v = (v ^ (v >> 32)) & 0x00000000001fffff;
	return v;
#endif
}

} //::morton

} //::aether
