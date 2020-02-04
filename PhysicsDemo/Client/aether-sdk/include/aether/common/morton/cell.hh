#pragma once

#include <cassert>
#include <cstdint>
#include <variant>
#include <array>
#include "AABB.hh"
#include "traits.hh"

//The coordinate of an octree cell.
template<typename MortonCode>
struct tree_cell {
private:
    using morton_type = MortonCode;
    using aabb_type = typename aether::morton::code_traits<morton_type>::aabb;

    template<typename T>
    using region_type = typename aether::morton::code_traits<morton_type>::template region<T>;

    template<typename T>
    using interval_type = typename aether::morton::code_traits<morton_type>::template interval<T>;

    static inline uint64_t num_points(const uint64_t level) {
        if (level * dimension == sizeof(uint64_t) * CHAR_BIT) {
            // This enables us to construct the largest cell in the 2D case
            // since the range is inclusive.
            return 0;
        } else {
            return static_cast<uint64_t>(1) << (level * dimension);
        }
    }

    inline uint64_t num_points() const {
        return num_points(level);
    }

public:
    static constexpr size_t dimension = morton_type::dimension;
    static constexpr size_t child_count = (1 << dimension);

    morton_type code;
    uint64_t level;

    tree_cell() = default;

    tree_cell(const morton_type &_code, uint64_t _level) : code(_code), level(_level) {
        fix_code();
    }

    void fix_code() {
        code.clear_lsb(dimension * level);
    };

    bool check_overlap(const tree_cell &y) const {
        uint64_t l = (level > y.level) ? level : y.level;
        return (code.as_u64() >> (dimension * l)) == (y.code.as_u64() >> (dimension * l));
    }

    bool contains(const morton_type &c) const {
        // We use clear_lsb to avoid issues with undefined behaviour for bit shifts
        auto c1(code);
        auto c2(c);
        c1.clear_lsb(level * dimension);
        c2.clear_lsb(level * dimension);
        return c1 == c2;
    }

    bool contains(const tree_cell &other) const {
        return level >= other.level && contains(other.code);
    }

    region_type<std::monostate> to_region() const {
        return to_region(std::monostate{});
    }

    template<typename T>
    region_type<T> to_region(const T &data) const {
        return {
            {
                interval_type<T> { code.as_u64(), code.as_u64() + num_points() - 1, data },
            },
        };
    }

    aabb_type to_aabb() const {
        return {
            code,
            code.as_u64() + num_points() - 1,
        };
    }

    size_t side_length() const {
        return 1 << level;
    }

    bool operator==(const tree_cell &other) const {
        return level == other.level && code == other.code;
    }

    size_t get_level() const {
        return level;
    }

    morton_type get_corner() const {
        return code;
    }

    tree_cell get_parent() const {
        auto result(*this);
        ++result.level;
        result.fix_code();
        return result;
    }
};
