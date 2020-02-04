#pragma once

#include <optional>
#include <cstdint>
#include <cassert>
#include <vector>
#include <variant>
#include <optional>
#include <tuple>
#include <ostream>

#include "encoding.hh"
#include "util.hh"
#include <immintrin.h>

namespace aether {

namespace morton {

namespace detail {

template<typename MortonCode, typename T = std::monostate>
struct interval {
    using morton_type = MortonCode;
    static const size_t dimension = morton_type::dimension;
    morton_type start, end;
    T data {};
    interval(const morton_type &_start, const morton_type &_end): start(_start), end(_end) {};
    interval(const morton_type &_start, const morton_type &_end, const T &_t): start(_start), end(_end), data(_t) {};

    template<typename M>
    friend bool operator==(const interval& lhs, const interval<morton_type, M>& rhs) {
        if constexpr (std::is_same<T, std::monostate>::value || std::is_same<M, std::monostate>::value) {
            return std::tie(lhs.start, lhs.end) == std::tie(rhs.start, rhs.end);
        } else if constexpr (std::is_same<T, M>::value) {
            return std::tie(lhs.start, lhs.end, lhs.data) == std::tie(rhs.start, rhs.end, rhs.data);
        } else {
            static_assert(std::is_same<T, std::monostate>::value || std::is_same<M, std::monostate>::value || std::is_same<T, M>::value, "the data types on intervals should be either the same or std::monostate");
        }
    }

    friend bool operator!=(const interval& lhs, const interval& rhs) {
        return !(lhs == rhs);
    }

    template<typename M>
    friend bool operator<(const interval& lhs, const interval<morton_type, M>& rhs) {
        if constexpr (std::is_same<T, std::monostate>::value || std::is_same<M, std::monostate>::value) {
            return std::tie(lhs.start, lhs.end) < std::tie(rhs.start, rhs.end);
        } else if constexpr (std::is_same<T, M>::value) {
            return std::tie(lhs.start, lhs.end, lhs.data) < std::tie(rhs.start, rhs.end, rhs.data);
        } else {
            static_assert(std::is_same<T, std::monostate>::value || std::is_same<M, std::monostate>::value || std::is_same<T, M>::value, "the data types on intervals should be either the same or std::monostate");
        }
    }

    friend bool operator>(const interval& lhs, const interval& rhs) {
        return rhs < lhs;
    }

    friend bool operator<=(const interval& lhs, const interval& rhs) {
        return !(lhs > rhs);
    }

    friend bool operator>=(const interval& lhs, const interval& rhs) {
        return !(lhs < rhs);
    }

    bool data_equals(const interval& rhs) const;

    std::optional<interval> intersect(const interval& rhs) const;

    bool contains(const morton_type &c) const {
        return c >= start && c <= end;
    }

    uint64_t area() const;

    uint64_t start_alignment() const;

    uint64_t end_alignment() const;

    std::vector<interval> to_cells() const;

    std::vector<interval> to_cells(size_t max_level) const;
    // this returns an sorted map of the cells and size.
    // e.g. 3 cells of size 1, 2 cells of size 2, and one cell of size 3.
    // where size 1 = 1 on each side, size 2 = 2, size 3  = 4
    std::vector<std::pair<uint64_t,uint64_t>> count_cells() const;
};

template<typename MortonCode, typename T>
bool interval<MortonCode, T>::data_equals(const detail::interval<MortonCode, T>& rhs) const {
    if constexpr (std::is_same<T, std::monostate>::value) {
        return true;
    } else {
        return data == rhs.data;
    }
}

template<typename MortonCode, typename T>
std::optional<interval<MortonCode, T>> interval<MortonCode, T>::intersect(const interval<MortonCode, T>& rhs) const {
    uint64_t i_start = std::max(start, rhs.start);
    uint64_t i_end = std::min(end, rhs.end);
    if (i_start > i_end) {
      return std::nullopt;
    }
    return std::optional{interval<MortonCode>{i_start,i_end,data}};
}

template<typename MortonCode, typename T>
uint64_t interval<MortonCode, T>::area() const {
    assert(start <= end);
    return end + 1 - start;
}

template<typename MortonCode, typename T>
uint64_t interval<MortonCode, T>::start_alignment() const {
    return start != 0 ? __builtin_ctzll(start) / dimension : std::numeric_limits<uint64_t>::max();
}

template<typename MortonCode, typename T>
uint64_t interval<MortonCode, T>::end_alignment() const {
    return end != 0 ? __builtin_ctzll(end) / dimension : std::numeric_limits<uint64_t>::max();
}

template<typename MortonCode, typename T>
std::vector<interval<MortonCode, T>> interval<MortonCode, T>::to_cells() const {
    uint64_t s = start;
    std::vector<interval<MortonCode, T>> v = {};
    while(s <= end){
        auto amax = get_align_max<dimension, MortonCode::max_level>(s,end);
        v.push_back({s,amax,data});
        s = amax+1;
    }
    return v;
}

template<typename MortonCode, typename T>
std::vector<interval<MortonCode, T>> interval<MortonCode, T>::to_cells(size_t max_level) const {
    uint64_t s = start;
    std::vector<interval<MortonCode, T>> v = {};
    while(s <= end){
        auto amax = std::min(s + (1 << max_level), get_align_max<MortonCode::dimension, MortonCode::max_level>(s,end));
        v.push_back({s,amax,data});
        s = amax+1;
    }
    return v;
}
// this returns an sorted map of the cells and size.
// e.g. 3 cells of size 1, 2 cells of size 2, and one cell of size 3.
// where size 1 = 1 on each side, size 2 = 2, size 3  = 4
template<typename MortonCode, typename T>
std::vector<std::pair<uint64_t,uint64_t>> interval<MortonCode, T>::count_cells() const {
    uint64_t s = start;
    std::vector<std::pair<uint64_t,uint64_t>> v = {};
    while(s <= end){
        auto amax = get_align_max<MortonCode::dimension, MortonCode::max_level>(s,end);
        auto diff = fast_log2(1 + amax - s)/dimension;
        size_t i = 0;
        bool found = false;
        for(; i < v.size(); i++){
            if (v[i].first == diff) {
                found = true;
                break;
            }
        }
        if (found) {
            v[i].second += 1;
        } else {
            v.push_back({diff,1});
        }
        s = amax+1;
    }
    std::sort(v.begin(), v.end());
    return v;
}

template<typename MortonCode, typename T>
std::ostream &operator<<(std::ostream &o, const interval<MortonCode, T> &i) {
    o << "interval { start: " << i.start << ", end: " << i.end << ", data: ";
    if constexpr (std::is_same<T, std::monostate>::value) {
        o << "{}";
    } else {
        o << i.data;
    }
    o << "}";
    return o;
}

} //::detail

} //::morton

} //::aether
