#pragma once
#include <climits>
#include <functional>
#include <type_traits>
#include <cstdint>

namespace aether {

namespace hash {

// A hasher based on FNV-1a
struct hasher {
    template<typename T> struct constants { };

    template<>
    struct constants<uint64_t> {
        static constexpr size_t basis = 0xcbf29ce484222325ul;
        static constexpr size_t prime = 0x100000001b3ul;
    };

    template<>
    struct constants<uint32_t> {
        static constexpr size_t basis = 0x811c9dc5ul;
        static constexpr size_t prime = 0x1000193ul;
    };

    static constexpr size_t get_basis() {
        return constants<size_t>::basis;
    }

    static constexpr size_t get_prime() {
        return constants<size_t>::prime;
    }

    size_t value = get_basis();

    template<typename T, std::enable_if_t<
        std::is_integral<T>::value &&
        std::is_fundamental<T>::value,
        int
        > = 0
    >
    void operator()(const T &v) {
        using unsigned_type = typename std::make_unsigned<T>::type;
        auto remaining_value = static_cast<unsigned_type>(v);
        static constexpr size_t iterations = (sizeof(remaining_value) * CHAR_BIT + 7) / 8;
        static constexpr size_t lsb_mask = 0xff;
        for(size_t i = 0; i < iterations; ++i) {
            const auto lsb = static_cast<size_t>(remaining_value & lsb_mask);
            value = (value ^ lsb) * get_prime();
            if constexpr (iterations > 1) {
                // This shift will be invalid for a single character
                remaining_value >>= 8;
            }
        }
    }

    template<typename T, std::enable_if_t<
        !std::is_integral<T>::value ||
        !std::is_fundamental<T>::value,
        int
        > = 0
    >
    void operator()(const T &v) {
        const auto value_hash = std::hash<T>()(v);
        (*this)(value_hash);
    }

    template<typename T1, typename T2>
    void operator()(const std::pair<T1, T2> &pair) {
        static constexpr uint8_t PAIR_MAGIC = 183;
        (*this)(PAIR_MAGIC);
        (*this)(pair.first);
        (*this)(pair.second);
    }

    size_t get_value() const {
        return value;
    }
};

// The C++ standard library has no pair hashing functionality
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator() (const std::pair<T1, T2> &pair) const {
        hasher hasher;
        hasher(pair);
        return hasher.get_value();
    }
};

}

}
