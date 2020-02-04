#pragma once

#include <variant>
#include "AABB.hh"
#include "region.hh"
#include <aether/common/span.hh>

namespace aether {

namespace morton {

template<typename MortonCode>
struct aabb_merger {
    using morton_type = MortonCode;
    using aabb_type = aether::morton::AABB<morton_type>;
    using region_type = aether::morton::region<morton_type, std::monostate>;

    region_type operator()(const aether::span<const aabb_type>& aabbs) const;
};

}

}
