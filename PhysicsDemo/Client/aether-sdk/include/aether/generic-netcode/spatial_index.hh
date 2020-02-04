#pragma once

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <aether/common/hash.hh>
#include <aether/common/vector.hh>
#include "entity_store.hh"

namespace aether {

namespace netcode {

namespace detail {

struct spatial_index_bucket {
private:
    std::vector<entity_handle> entities;
    std::vector<entity_handle> to_add;
    std::vector<entity_handle> to_remove;

public:
    using const_iterator = std::vector<entity_handle>::const_iterator;
    using iterator = const_iterator;

    const_iterator begin() const {
        return entities.begin();
    }

    const_iterator end() const {
        return entities.end();
    }

    void add(const entity_handle &handle) {
        to_add.push_back(handle);
    }

    void remove(const entity_handle &handle) {
        to_remove.push_back(handle);
    }

    size_t empty() const {
        return entities.empty();
    }

    void commit() {
        std::sort(to_add.begin(), to_add.end());
        std::sort(to_remove.begin(), to_remove.end());

        // Add new entities
        std::vector<entity_handle> entities_tmp;
        entities_tmp.reserve(entities.size() + to_add.size());
        std::merge(entities.begin(), entities.end(), to_add.begin(), to_add.end(),
            std::back_inserter(entities_tmp));
        to_add.clear();

        // Remove old entities
        entities.clear();
        std::set_difference(entities_tmp.begin(), entities_tmp.end(),
            to_remove.begin(), to_remove.end(), std::back_inserter(entities));
        to_remove.clear();
    }
};

template<typename Point>
struct spatial_bucket {
    using component_type = int64_t;
    using point_type = Point;
    component_type width;
    component_type x;
    component_type y;
    component_type z;

    static std::optional<component_type> discretize_component(const double f, const size_t _width) {
        const auto width = static_cast<component_type>(_width);
        if (!std::isfinite(f)) {
            return std::nullopt;
        } else {
            const double floored = std::floor(f);
            auto converted = static_cast<component_type>(floored);
            if (static_cast<double>(converted) == floored) {
                // We want to round to a multiple of the bucket size but need
                // to ensure that we always round towards negative infinity.
                // Otherwise, points below zero get rounded up and buckets on
                // an axis become larger than they should be.
                if (converted < 0 &&
                    std::numeric_limits<component_type>::min() + (width - 1) <= converted) {
                    converted -= (width - 1);
                }
                converted /= width;
                converted *= width;
                return { converted };
            } else {
                return std::nullopt;
            }
        }
    }

    static std::optional<spatial_bucket> encode_bucket(const vec3f &position, size_t width) {
        const auto c_x = discretize_component(position.x, width);
        const auto c_y = discretize_component(position.y, width);
        const auto c_z = discretize_component(position.z, width);
        if (!c_x || !c_y || !c_z) {
            return std::nullopt;
        } else {
            return { { width, *c_x, *c_y,  *c_z } };
        }
    }

    bool operator==(const spatial_bucket &o) const {
        return x == o.x && y == o.y && z == o.z && width == o.width;
    }

    bool operator!=(const spatial_bucket &o) const {
        return !(*this == o);
    }

    size_t hash_value() const {
        aether::hash::hasher hasher;
        hasher(x);
        hasher(y);
        hasher(z);
        hasher(width);
        return hasher.get_value();
    }

    boost::geometry::model::box<point_type> to_box() const {
        const point_type lower = {x, y, z};
        const point_type upper = {
            lower.template get<0>() + width,
            lower.template get<1>() + width,
            lower.template get<2>() + width,
        };
        return { lower, upper };
    }
};

}

}

}

template<typename T>
struct std::hash<aether::netcode::detail::spatial_bucket<T>> {
    using argument_type = aether::netcode::detail::spatial_bucket<T>;
    using result_type   = std::size_t;
    result_type operator()(const argument_type &handle) const noexcept {
        return handle.hash_value();
    }
};

namespace aether {

namespace netcode {

template<typename Store>
class spatial_index {
private:
    const size_t bucket_width = 1 << 4;
    using entity_store = Store;
    using entity_type = typename entity_store::entity_type;

    using bg_point_type = boost::geometry::model::point<double, 3, boost::geometry::cs::cartesian>;
    using bg_box_type = boost::geometry::model::box<bg_point_type>;
    using bucket_index_type = detail::spatial_bucket<bg_point_type>;
    using bucket_type = detail::spatial_index_bucket;
    using rtree_value = std::pair<bg_box_type, bucket_index_type>;

    entity_store &store;
    std::unordered_set<bucket_index_type> modified_buckets;
    std::unordered_map<entity_handle, bucket_index_type> entity_buckets;
    std::unordered_map<bucket_index_type, bucket_type> buckets;
    boost::geometry::index::rtree<rtree_value, boost::geometry::index::linear<16>> rtree;

    std::optional<bucket_index_type> position_to_index(const vec3f &position) {
        return bucket_index_type::encode_bucket(position, bucket_width);
    }

    static vec3f promote_to_vec3f(const vec3f &pos) {
        return pos;
    }

    static vec3f promote_to_vec3f(const vec2f &pos) {
        vec3f result(pos.x, pos.y, 0.0f);
        return result;
    }

    rtree_value index_to_rtree_value(const bucket_index_type &bucket_index) const {
        return { bucket_index.to_box(), bucket_index };
    }

public:
    spatial_index(entity_store &_store) : store(_store) {
    }

    bool update_entity(const entity_handle &handle) {
        assert(store.is_valid(handle));
        const auto id = store.get_entity_id(handle);
        const auto &entity = store.get(handle);
        const auto position = promote_to_vec3f(get_position(entity));
        const auto maybe_new_bucket = position_to_index(position);
        if (!maybe_new_bucket) { return false; }
        const auto new_bucket = maybe_new_bucket.value();

        // Determine old bucket (if there was one) then update recorded
        // bucket for entity
        std::optional<bucket_index_type> old_bucket;
        {
            const auto index_iter = entity_buckets.find(handle);
            if (index_iter != entity_buckets.end()) {
                old_bucket = { index_iter->second };
                index_iter->second = new_bucket;
            } else {
                entity_buckets.insert({ handle, new_bucket });
            }
        }

        // Remove the entity from the old bucket if it has changed bucket
        const bool moved = old_bucket.has_value() && old_bucket.value() != new_bucket;
        if (moved) {
            const auto bucket_iter = buckets.find(old_bucket.value());
            assert(bucket_iter != buckets.end());
            bucket_iter->second.remove(handle);
            modified_buckets.insert(old_bucket.value());
        }

        // Insert entity into new bucket
        const bool is_new = !old_bucket.has_value();
        if (moved || is_new) {
            auto bucket_iter = buckets.find(new_bucket);
            if (bucket_iter == buckets.end()) {
                const auto [iter, inserted] = buckets.emplace(std::make_pair(new_bucket, bucket_type{}));
                assert(inserted);
                bucket_iter = iter;
                rtree.insert(index_to_rtree_value(new_bucket));
            }
            bucket_iter->second.add(handle);
            modified_buckets.insert(new_bucket);
        }

        return true;
    }

    bool drop_entity(const entity_handle &handle) {
        const auto index_iter = entity_buckets.find(handle);
        if (index_iter != entity_buckets.end()) {
            const auto bucket_iter = buckets.find(index_iter->second);
            assert(bucket_iter != buckets.end() && "Recorded bucket for entity is unexpectedly missing");
            bucket_iter->second.remove(handle);
            modified_buckets.insert(index_iter->second);
            entity_buckets.erase(index_iter);
            return true;
        } else {
            return false;
        }
    }

    void commit() {
        for(const auto &bucket_index : modified_buckets) {
            const auto bucket_iter = buckets.find(bucket_index);
            assert(bucket_iter != buckets.end());
            auto &bucket = bucket_iter->second;
            bucket.commit();
            if (bucket.empty()) {
                const size_t remove_count = rtree.remove(index_to_rtree_value(bucket_index));
                assert(remove_count == 1 && "Bucket unexpectedly missing from r-tree");
                buckets.erase(bucket_iter);
            }
        }
        modified_buckets.clear();
    }

    std::vector<entity_handle> find_entities_approximate(const vec3f &position, const double radius) const {
        assert(radius >= 0.0 && "Radius must not negative");
        std::vector<entity_handle> result;
        const bg_point_type centre = { position.x, position.y, position.z };
        const bg_box_type box = {
            { position.x - radius, position.y - radius, position.z - radius },
            { position.x + radius, position.y + radius, position.z + radius },
        };
        std::vector<rtree_value> bucket_values;
        rtree.query(boost::geometry::index::intersects(box), std::back_inserter(bucket_values));
        for(const auto &[_, bucket_index] : bucket_values) {
            const auto bucket_iter = buckets.find(bucket_index);
            assert(bucket_iter != buckets.end() && "Unexpectedly missing bucket");
            result.insert(result.end(), bucket_iter->second.begin(), bucket_iter->second.end());
        }
        return result;
    }

    std::vector<entity_handle> find_entities_exact(const vec3f &position, const double radius) const {
        auto entities = find_entities_approximate(position, radius);
        const auto radius_sq = radius * radius;
        const auto too_far = [this, radius_sq, &position](const entity_handle& handle) -> bool {
            const auto e_position = promote_to_vec3f(get_position(store.get(handle)));
            const auto delta = position - e_position;
            const auto dist_sq = delta.dot(delta);
            return dist_sq > radius_sq;
        };
        const auto new_end = std::remove_if(entities.begin(), entities.end(), too_far);
        entities.erase(new_end, entities.end());
        return entities;
    }

    const entity_store &get_store() const {
        return store;
    }
};

}

}
