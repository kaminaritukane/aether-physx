#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <aether/common/hash.hh>

namespace aether {

namespace netcode {

class entity_handle {
private:
    size_t offset;
    uint64_t id;

    std::tuple<const size_t&, const uint64_t&> as_tuple() const {
        return { offset, id };
    }

public:
    template<typename T> friend class entity_store;

    entity_handle(size_t _offset, uint64_t _id) : offset(_offset), id(_id) {
    }

    bool operator==(const entity_handle &o) const {
        return as_tuple() == o.as_tuple();
    }

    bool operator<(const entity_handle &o) const {
        return as_tuple() < o.as_tuple();
    }

    uint64_t get_id() const {
        return id;
    }

    size_t hash_value() const {
        aether::hash::hasher hasher;
        hasher(offset);
        hasher(id);
        return hasher.get_value();
    }
};

template<typename Entity>
class entity_store {
public:
    using time_point = std::chrono::high_resolution_clock::time_point;
    using entity_type = Entity;

    struct metadata_type {
        uint64_t tick;
        time_point time;
        uint64_t worker_id;
    };

private:
    struct entity_info {
        metadata_type metadata;
        bool valid = false;
        uint64_t entity_id;
        entity_type value;
    };

    std::unordered_map<uint64_t, size_t> entity_offsets;
    std::vector<size_t> unused_entity_offsets;
    std::vector<entity_info> entities;

public:
    entity_store() = default;

    std::optional<entity_handle> find_entity(const uint64_t entity_id) const {
        auto offset_iter = entity_offsets.find(entity_id);
        if (offset_iter == entity_offsets.end()) {
            return std::nullopt;
        } else {
            return { { offset_iter->second, entity_id } };
        }
    }

    uint64_t get_entity_id(const entity_handle& handle) const {
        return handle.id;
    }

    entity_handle new_entity(const metadata_type &metadata, const uint64_t entity_id, const entity_type &entity) {
        assert(entity_offsets.find(entity_id) == entity_offsets.end());
        size_t offset;
        if (unused_entity_offsets.empty()) {
            assert(entities.size() == entity_offsets.size());
            offset = entity_offsets.size();
            entities.resize(entities.size() + 1);
        } else {
            offset = unused_entity_offsets.back();
            unused_entity_offsets.pop_back();
        }
        entity_offsets.insert({ entity_id, offset });
        assert(entities.size() == unused_entity_offsets.size() + entity_offsets.size());
        auto &info = entities[offset];
        info.metadata = metadata;
        info.valid = true;
        info.entity_id = entity_id;
        info.value = entity;
        return { offset, entity_id };
    }

    void update_entity(const metadata_type &metadata, const entity_handle &handle, const entity_type &entity) {
        assert(is_valid(handle) && "Invalid entity handle");
        auto &info = entities[handle.offset];
        info.metadata = metadata;
        info.value = entity;
    }

    const entity_type &get(const entity_handle &handle) const {
        assert(is_valid(handle) && "Invalid entity handle");
        return entities[handle.offset].value;
    }

    void drop(const entity_handle &handle) {
        assert(is_valid(handle) && "Invalid entity handle");
        auto &info = entities[handle.offset];
        info.valid = false;
        entity_offsets.erase(info.entity_id);
        unused_entity_offsets.push_back(handle.offset);
    }

    bool is_valid(const entity_handle &handle) const {
        const auto offset = handle.offset;
        if (offset >= entities.size()) { return false; }
        auto &info = entities[offset];
        if (!info.valid) { return false; }
        return info.entity_id == handle.id;
    }

    time_point last_updated_time(const entity_handle &handle) const {
        assert(is_valid(handle) && "Invalid entity handle");
        return entities[handle.offset].metadata.time;
    }

    uint64_t last_updated_tick(const entity_handle &handle) const {
        assert(is_valid(handle) && "Invalid entity handle");
        return entities[handle.offset].metadata.tick;
    }

    uint64_t last_worker(const entity_handle &handle) const {
        assert(is_valid(handle) && "Invalid entity handle");
        return entities[handle.offset].metadata.worker_id;
    }

    std::vector<entity_handle> get_older_than(const uint64_t tick) const {
        std::vector<entity_handle> result;
        for(size_t i = 0; i < entities.size(); ++i) {
            const auto &info = entities[i];
            if (info.valid && info.metadata.tick < tick) {
                result.push_back({ i, info.entity_id });
            }
        }
        return result;
    }

    std::optional<entity_handle> first() const {
        if (entities.size() != 0) {
            const entity_info &info = entities[0];
            return { {0, info.entity_id} };
        }
        return std::nullopt;
    }

    std::optional<entity_handle> next(const entity_handle &handle) const {
        size_t next_offset = handle.offset + 1;
        while(next_offset < entities.size()) {
            const entity_info &info = entities[next_offset];
            if (info.valid) {
                return { {next_offset, info.entity_id} };
            }
            ++next_offset;
        }
        return std::nullopt;
    }
};

}

}

template<>
struct std::hash<aether::netcode::entity_handle> {
    using argument_type = aether::netcode::entity_handle;
    using result_type   = std::size_t;
    result_type operator()(const argument_type &handle) const noexcept {
        return handle.hash_value();
    }
};
