#pragma once

#include "../math_utils.hh"

#include <cstdint>
#include <cassert>
#include <immintrin.h>
#include <array>
#include <tuple>
#include <ostream>

#include "../vector.hh"
#include "util.hh"

template<uint32_t Dimension, uint32_t BitsPerDimension>
struct morton_code {
    static_assert((Dimension == 2 && BitsPerDimension == 32) || (Dimension == 3 && BitsPerDimension == 21),
        "only 2D and 32 bits, or 3D and 21 bits are currently supported.");
};

using aether::morton::__morton_2_x_mask;
using aether::morton::__morton_2_y_mask;
using aether::morton::__morton_3_x_mask;
using aether::morton::__morton_3_y_mask;
using aether::morton::__morton_3_z_mask;
using aether::morton::expand_bits_2;
using aether::morton::compact_bits_2;
using aether::morton::expand_bits_3;
using aether::morton::compact_bits_3;

template<>
struct morton_code<2, 32> {
private:
    static uint32_t encode_signed(int32_t v) {
        const uint32_t result = (static_cast<uint32_t>(1) << 31) + static_cast<uint32_t>(v);
        return result;
    }

    static int32_t decode_unsigned(uint32_t v) {
        return static_cast<int32_t>(v - (static_cast<uint32_t>(1) << 31));
    }

public:
    uint64_t data;
    static constexpr uint32_t dimension = 2;
    static constexpr uint32_t max_level = 32;

    operator uint64_t() const {
        return data;
    }

    uint64_t as_u64() const {
        return data;
    }

    morton_code() : data(0) {}

    morton_code(uint64_t _data): data(_data) {}

    size_t get_index_msb() const {
        assert(data != 0);
        return sizeof(data) * CHAR_BIT - 1 - __builtin_clzll(data);
    }

    void clear_lsb(const size_t num_bits) {
        if (num_bits >= sizeof(data) * CHAR_BIT) {
            data = 0;
        } else {
            data >>= num_bits;
            data <<= num_bits;
        }
    }

    void set_component(const size_t idx, const int32_t value) {
        set_component_raw(idx, encode_signed(value));
    }

    int32_t extract_component(const size_t idx) const {
        return decode_unsigned(extract_component_raw(idx));
    }

    void set_component_raw(const size_t idx, const uint32_t value) {
        data &= ~(__morton_2_x_mask << idx);
        data |= (expand_bits_2<uint64_t>(value) << idx);
    }

    uint32_t extract_component_raw(const size_t idx) const {
        return static_cast<uint32_t>(compact_bits_2<uint64_t>(data >> idx));
    }

    static morton_code encode(const std::array<int32_t, 2> &p) {
        morton_code result;
        result.set_component(0, std::get<0>(p));
        result.set_component(1, std::get<1>(p));
        return result;
    }

    friend std::array<int32_t, 2> decode(const morton_code &code) {
        return {{
            code.extract_component(0),
            code.extract_component(1),
        }};
    }

    friend morton_code &operator-=(morton_code &lhs, const morton_code &rhs) {
        uint64_t x = (lhs.data & __morton_2_x_mask) - (rhs.data & __morton_2_x_mask);
        uint64_t y = (lhs.data & __morton_2_y_mask) - (rhs.data & __morton_2_y_mask);
        lhs.data = (x & __morton_2_x_mask) | (y & __morton_2_y_mask);
        return lhs;
    }

    friend morton_code &operator+=(morton_code &lhs, const morton_code &rhs) {
        uint64_t x = (lhs.data | ~__morton_2_x_mask) + (rhs.data & __morton_2_x_mask);
        uint64_t y = (lhs.data | ~__morton_2_y_mask) + (rhs.data & __morton_2_y_mask);
        lhs.data = (x & __morton_2_x_mask) | (y & __morton_2_y_mask);
        return lhs;
    }

    friend morton_code operator^(const morton_code &lhs, const morton_code &rhs) {
        auto result(lhs);
        result ^= rhs;
        return result;
    }

    friend morton_code &operator^=(morton_code &lhs, const morton_code &rhs) {
        lhs.data ^= rhs.data;
        return lhs;
    }

    size_t get_index_at_level(const size_t level) const {
        auto result = data;
        const auto mask = (static_cast<decltype(data)>(1) << dimension) - 1;
        result >>= (level * dimension);
        result &= mask;
        return result;
    }
};

template<>
struct morton_code<3, 21> {
private:
    static uint32_t encode_signed(int32_t v) {
        const uint32_t result = (static_cast<uint32_t>(1) << 20) + static_cast<uint32_t>(v);
        return result;
    }

    static int32_t decode_unsigned(uint32_t v) {
        return static_cast<int32_t>(v - (static_cast<uint32_t>(1) << 20));
    }

public:
    uint64_t data;
    static constexpr uint32_t dimension = 3;
    static constexpr uint32_t max_level = 21;

    operator uint64_t() const {
        return data;
    }

    uint64_t as_u64() const {
        return data;
    }

    morton_code(uint64_t _data): data(_data) {};

    morton_code(): data(0) {};

    size_t get_index_msb() const {
        assert(data != 0);
        return sizeof(data) * CHAR_BIT - 1 - __builtin_clzll(data);
    }

    void clear_lsb(const size_t num_bits) {
        if (num_bits >= sizeof(data) * CHAR_BIT) {
            data = 0;
        } else {
            data >>= num_bits;
            data <<= num_bits;
        }
    }

    void set_component(const size_t idx, const int32_t value) {
        set_component_raw(idx, encode_signed(value));
    }

    void set_component_raw(const size_t idx, const uint32_t value) {
        data &= ~(__morton_3_x_mask << idx);
        data |= (expand_bits_3<uint64_t>(value) << idx);
    }

    int32_t extract_component(const size_t idx) const {
        return decode_unsigned(extract_component_raw(idx));
    }

    uint32_t extract_component_raw(const size_t idx) const {
        return static_cast<uint32_t>(compact_bits_3<uint64_t>(data >> idx));
    }

    static morton_code encode(const std::array<int32_t, dimension> &p) {
        morton_code result;
        result.set_component(0, std::get<0>(p));
        result.set_component(1, std::get<1>(p));
        result.set_component(2, std::get<2>(p));
        return result;
    }

    friend std::array<int32_t, dimension> decode(const morton_code &code) {
        return {{
            code.extract_component(0),
            code.extract_component(1),
            code.extract_component(2),
        }};
    }

    friend void operator+=(morton_code &lhs, const morton_code &rhs) {
        uint64_t x = (lhs.data | ~__morton_3_x_mask) + (rhs.data & __morton_3_x_mask);
        uint64_t y = (lhs.data | ~__morton_3_y_mask) + (rhs.data & __morton_3_y_mask);
        uint64_t z = (lhs.data | ~__morton_3_z_mask) + (rhs.data & __morton_3_z_mask);
        lhs.data = (x & __morton_3_x_mask) | (y & __morton_3_y_mask) | (z & __morton_3_z_mask);
    }

    friend void operator-=(morton_code &lhs, const morton_code &rhs) {
        uint64_t x = (lhs.data & __morton_3_x_mask) - (rhs.data & __morton_3_x_mask);
        uint64_t y = (lhs.data & __morton_3_y_mask) - (rhs.data & __morton_3_y_mask);
        uint64_t z = (lhs.data & __morton_3_z_mask) - (rhs.data & __morton_3_z_mask);
        lhs.data = (x & __morton_3_x_mask) | (y & __morton_3_y_mask) | (z & __morton_3_z_mask);
    }

    bool operator==(const morton_code &other) const {
        return data == other.data;
    }

    bool operator<(const morton_code &other) const {
        return data < other.data;
    }

    friend morton_code operator^(const morton_code &lhs, const morton_code &rhs) {
        auto result(lhs);
        result ^= rhs;
        return result;
    }

    friend morton_code &operator^=(morton_code &lhs, const morton_code &rhs) {
        lhs.data ^= rhs.data;
        return lhs;
    }

    size_t get_index_at_level(const size_t level) const {
        auto result = data;
        const auto mask = (static_cast<decltype(data)>(1) << dimension) - 1;
        result >>= (level * dimension);
        result &= mask;
        return result;
    }

};

template<size_t Dimension, size_t BitsPerDimension>
std::ostream &operator<<(std::ostream &o, const morton_code<Dimension, BitsPerDimension> &c) {
    o << "morton_code<" << Dimension << ", " << BitsPerDimension << ">(" << c.data << ")";
    return o;
}

inline morton_code<3, 21> morton_3_encode(const vec3f &v) {
    return morton_code<3, 21>::encode({{
        static_cast<int32_t>(floorf(v.x)),
        static_cast<int32_t>(floorf(v.y)),
        static_cast<int32_t>(floorf(v.z)),
    }});
}

inline vec3f morton_3_decode(const morton_code<3, 21> &m) {
    const auto p = decode(m);
    return {
        static_cast<float>(std::get<0>(p)),
        static_cast<float>(std::get<1>(p)),
        static_cast<float>(std::get<2>(p)),
    };
}

inline vec2f morton_2_decode(const morton_code<2, 32> &m) {
    const auto p = decode(m);
    return {
        static_cast<float>(std::get<0>(p)),
        static_cast<float>(std::get<1>(p)),
    };
}

inline morton_code<2, 32> morton_2_encode(const vec2f &v) {
    return morton_code<2, 32>::encode({{
        static_cast<int32_t>(floorf(v.x)),
        static_cast<int32_t>(floorf(v.y)),
    }});
}
