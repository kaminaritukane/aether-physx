#pragma once

#include "AABB.hh"
#include "region.hh"
#include "interval.hh"
#include "encoding.hh"

namespace aether {

namespace morton {

// FIXME: This should be deleted once we start parametering AABBs and
// intervals on Morton types instead of dimension and bits per dimension.

template<typename M>
struct code_traits {
private:
    using morton_type = M;
    static const size_t max_level = morton_type::max_level;

public:
    static const size_t dimension = morton_type::dimension;
    using aabb = aether::morton::AABB<morton_type>;

    template<typename S>
    using region = aether::morton::region<morton_type, S>;

    template<typename S>
    using interval = aether::morton::detail::interval<morton_type, S>;
};

template<size_t D>
struct dimension_traits {
};

template<>
struct dimension_traits<2> {
    using default_code = morton_code<2, 32>;
};

template<>
struct dimension_traits<3> {
    using default_code = morton_code<3, 21>;
};

}

}
