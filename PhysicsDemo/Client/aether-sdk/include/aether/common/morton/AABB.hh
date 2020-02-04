#pragma once

#include <cstdint>
#include <cassert>

#include <algorithm>
#include <vector>
#include <tuple>
#include <type_traits>
#include <range/v3/view/generate.hpp>
#include <range/v3/view/take_while.hpp>
#include <range/v3/view/transform.hpp>

#include "encoding.hh"
#include "region.hh"
#include <immintrin.h>

namespace aether {

namespace morton {

template<typename MortonCode>
struct AABB {
    // AABBs can have min = max represent a single interval.
    // That is min and max are inclusive values
    using morton_type = MortonCode;
    static constexpr size_t dimension = morton_type::dimension;
    using interval_type = morton::detail::interval<morton_type>;
    morton_type min, max;

    AABB(const morton_type  &_min, const morton_type &_max): min(_min), max(_max) {
        assert(max >= min);
    };

    friend bool operator==(const AABB& lhs, const AABB& rhs){
        return lhs.min == rhs.min && lhs.max == rhs.max;
    }

    bool contains(const AABB& rhs) const;

    bool is_morton_aligned() const;

    morton::detail::interval<morton_type> to_cell() const;

    // This generates a list of all morton aligned intervals (morton cells)
    // that are within the AABB
    // it generates them in a sorted order, from lowest interval to highest.
    region<morton_type> to_cells() const;

    // This generates a list of all contiguous morton intervals (these are not necessarily aligned)
    // that are within the AABB
    // it generates them in a sorted order, from lowest interval to highest.
    region<morton_type> to_region() const;

    auto to_intervals_range() const;

    //morton_get_next_address is complex, read these for more detail
    //https://en.wikipedia.org/wiki/Z-order_curve#Use_with_one-dimensional_data_structures_for_range_searching
    //https://stackoverflow.com/questions/30170783/how-to-use-morton-orderz-order-curve-in-range-search/34956693#34956693
    //https://raima.com/wp-content/uploads/COTS_embedded_database_solving_dynamic_pois_2012.pdf
    //http://cppedinburgh.uk/slides/201603-zcurves.pdf
    std::pair<morton_type, morton_type> morton_get_next_address() const;
};

template<typename MortonCode>
bool AABB<MortonCode>::contains(const AABB& rhs) const {
    return min <= rhs.min && max >= rhs.max;
}

template<typename MortonCode>
bool AABB<MortonCode>::is_morton_aligned() const {
    assert(max >= min);
    uint64_t align_max = min != 0 ? __builtin_ctzll(min) : std::numeric_limits<uint64_t>::max();
    uint64_t diff = max - min + 1;
    uint64_t align = __builtin_ctzll(diff);
    return
        align / dimension <= align_max / dimension &&
        __builtin_popcountll(diff) == 1 &&
        align % dimension == 0;
}

template<typename MortonCode>
morton::detail::interval<MortonCode> AABB<MortonCode>::to_cell() const {
    assert(this->is_morton_aligned());
    return interval_type{min, max};
}

// This generates a list of all morton aligned intervals (morton cells)
// that are within the AABB
// it generates them in a sorted order, from lowest interval to highest.
template<typename MortonCode>
region<MortonCode> AABB<MortonCode>::to_cells() const {
    assert(max >= min);
    std::vector<AABB> inputs {*this};
    std::vector<morton::detail::interval<MortonCode>> outputs;
    while (!inputs.empty()) {
        AABB aabb = inputs.back();
        inputs.pop_back();
        if (aabb.is_morton_aligned()) {
            outputs.push_back(aabb.to_cell());
            continue;
        }
        auto [litmax, bigmin] = aabb.morton_get_next_address();
        AABB first = {aabb.min, litmax};
        AABB second = {bigmin, aabb.max};
        assert(first.max >= first.min);
        assert(second.max >= second.min);
        inputs.push_back(second);
        inputs.push_back(first);
    }
    return { std::move(outputs) };
}

// This generates a list of all contiguous morton intervals (these are not necessarily aligned)
// that are within the AABB
// it generates them in a sorted order, from lowest interval to highest.
template<typename MortonCode>
region<MortonCode> AABB<MortonCode>::to_region() const {
    std::vector<interval_type> outputs;
    auto range = to_intervals_range();
    for(const auto &i : range) {
        outputs.push_back(i);
    }
    return { std::move(outputs) };
}

template<typename MortonCode>
auto AABB<MortonCode>::to_intervals_range() const {
    using namespace ranges::v3;
    assert(max >= min);
    std::vector<AABB> inputs {*this};
    std::optional<interval_type> output;
    const auto f = [inputs = std::move(inputs), output]() mutable -> std::optional<interval_type> {
        while (!inputs.empty()) {
            const AABB aabb = inputs.back();
            inputs.pop_back();
            if (aabb.is_morton_aligned()) {
                //if the cell generated connects to the previous cell, merge.
                if (output.has_value() && output.value().end + 1 == aabb.min) {
                    output.value().end = aabb.max;
                } else {
                    const auto old_output = output;
                    output = { aabb.to_cell() };
                    if (old_output.has_value()) {
                        return old_output;
                    }
                }
                continue;
            }
            const auto [litmax, bigmin] = aabb.morton_get_next_address();
            const AABB first = {aabb.min, litmax};
            const AABB second = {bigmin, aabb.max};
            assert(first.max >= first.min);
            assert(second.max >= second.min);
            inputs.push_back(second);
            inputs.push_back(first);
        }
        const auto old_output = output;
        output = std::nullopt;
        return old_output;
    };
    auto generator = view::generate(f);
    auto yielder = view::take_while(generator, std::mem_fn(&std::optional<interval_type>::has_value));
    auto unwrapped = view::transform(yielder, [](const std::optional<interval_type> &i) -> interval_type {
        return i.value();
    });
    return unwrapped;
}

//morton_get_next_address is complex, read these for more detail
//https://en.wikipedia.org/wiki/Z-order_curve#Use_with_one-dimensional_data_structures_for_range_searching
//https://stackoverflow.com/questions/30170783/how-to-use-morton-orderz-order-curve-in-range-search/34956693#34956693
//https://raima.com/wp-content/uploads/COTS_embedded_database_solving_dynamic_pois_2012.pdf
//http://cppedinburgh.uk/slides/201603-zcurves.pdf
template<typename MortonCode>
std::pair<MortonCode, MortonCode> AABB<MortonCode>::morton_get_next_address()  const {
    auto litmax = max;
    auto bigmin = min;
    const size_t expanded_index = (min ^ max).get_index_msb();
    const size_t index = expanded_index / dimension;

    const uint64_t mask = ~((1LLU << (index + 1)) - 1);
    // A mask that zeroes out all the lower bits including the mismatch bit

    const uint64_t inc = 1LLU << index;
    // A mask that has 1 set at the mismatch bit

    const size_t relevant_dimension = expanded_index % dimension;
    // The dimension in which the first non-zero was found in the XOR

    uint32_t part = (min.extract_component_raw(relevant_dimension) & mask) + inc;
    // Extract dimension-specific part of min and set mismatch bit to 1

    bigmin.set_component_raw(relevant_dimension, part);
    // Replace dimension specific part of bigmin

    --part;
    // Decrement part (perhaps due to this being an inclusive range?)

    litmax.set_component_raw(relevant_dimension, part);
    // Replace dimension specific part of bigmin

    return std::make_pair(litmax, bigmin);
}

} //::morton

} //::aether
