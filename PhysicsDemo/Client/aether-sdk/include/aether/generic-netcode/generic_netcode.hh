#pragma once
#include <aether/common/morton/cell.hh>
#include <aether/common/morton/traits.hh>
#include <aether/common/span.hh>
#include <aether/muxer/netcode.hh>
#include <aether/common/container/max_heap.hh>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "interest_policy.hh"
#include "entity_store.hh"
#include "spatial_index.hh"

namespace aether {

namespace netcode {

static constexpr size_t HISTORY_SIZE = 1;
static constexpr double MIN_SIMULATION_HZ = 5.0f;

static vec3f promote_to_vec3f(const vec3f &pos) {
    return pos;
}

static vec3f promote_to_vec3f(const vec2f &pos) {
    vec3f result(pos.x, pos.y, 0.0f);
    return result;
}

//! Represents an entity controlled by an external client
struct controlled_entity {
    uint64_t tick;
    uint64_t player_id;
    uint64_t entity_id;
    vec3f position;
};

using clock_type = std::chrono::high_resolution_clock;
using time_point = clock_type::time_point;
using controlled_entity_map = std::unordered_map<uint64_t, std::unordered_map<uint64_t, controlled_entity>>;

//! Represents the priority of a message to be sent
struct packet_priority {
    time_point time; //! The time the message should be sent

    packet_priority(const time_point &_time) : time(_time) {
    }

    bool operator==(const packet_priority &other) const {
        return other.time == time;
    }

    bool operator<(const packet_priority &other) const {
        return other.time < time;
    }

    bool operator>(const packet_priority &other) const {
        return other.time > time;
    }
};

//! State associated with a worker
template<typename Marshalling>
struct worker_state {
    using marshalling_type = Marshalling;
    time_point last_updated;
    std::vector<typename marshalling_type::per_worker_data_type> headers;
};

//! State associated with a connection to a client
template<typename Marshalling>
class connection_state {
private:
    using marshalling_type = Marshalling;
    using entity_type = typename marshalling_type::entity_type;
    using per_worker_data_type = typename marshalling_type::per_worker_data_type;

    struct scheduled_entity_info {
        int64_t bucket_id;
        std::optional<uint64_t> last_sent_tick;
    };

    void *conn_ctx; //! Opaque connection context
    uint64_t player_id; //! The ID of the player for this connection
    aether::container::max_heap<uint64_t, packet_priority> worker_send_priorities; //! Priorities for groups of entities to be sent
    std::unordered_map<uint64_t, bool> worker_headers_changed; //! Records whether worker headers have changed since last being sent
    generic_interest_policy interest_policy; //! The interest management policy for sending data to the client

    aether::container::max_heap<int64_t, packet_priority> send_priorities; //! sorted heap of bucket idx based on the time in packet_priority
    std::map<int64_t, std::unordered_set<entity_handle>> send_buckets; //! Time-grouped buckets of entities entities to be sent
    time_point created; //! The time this connection was created

    connection_state(const connection_state&) = delete;

    std::unordered_map<uint64_t, scheduled_entity_info> scheduled_entities; //< entities that are currently scheduled
    spatial_index<entity_store<entity_type>> drop_entities_spatial; //< spatial index with the entities outside the interest area of the player

    //! Maps a time to a bucket identifier
    std::tuple<uint64_t, time_point> get_temporal_bucket(const time_point &time) const;

    //! Pops an identifier for any buckets that should be sent
    std::optional<uint64_t> pop_best_bucket(const time_point &now);


    //! Pops a worker_id for any workers whose headers should be sent
    std::optional<uint64_t> pop_best_per_worker(const time_point &now);

public:
    connection_state(void *_conn_ctx, const generic_interest_policy &policy, entity_store<entity_type> &global_store);
    connection_state(connection_state&&) = default;

    //! Returns true if the entity is scheduled to be sent at some point in the future
    bool is_scheduled(const entity_handle &handle) const;

    //! Returns the opaque context used to identify this connection to the muxer
    void *get_context() const;

    //! Schedules an entity to be sent in some future tick
    //!
    //! \param time the optional time this entity will be sent. If empty, the entity will be
    //! descheduled
    //! \param handle the entity
    //! \param update_last_sent If true, the current tick of the entity will be recorded in
    //! `scheduled_entities`. This enables entity versioning so that a non-updated entity
    //! is not sent multiple times.
    void schedule_entity(const entity_store<entity_type> &store, const entity_handle &handle,
        const std::optional<time_point> &time, bool update_last_sent = false);

    //! Returns the version of the entity (identified by tick) that was last sent to the client.
    //! `std::nullopt` is returned if the entity has not been recently scheduled.
    std::optional<uint64_t> last_sent_tick(const entity_handle &handle) const;

    //! This is called by the generic netcode to inform the client state that a new message
    //! has been received from the simulation.
    //!
    //! \param muxer the muxer context
    //! \param spatial_index all entities in the simulation indexed spatially
    //! \param controlled a map of entities controlled by external clients
    void new_simulation_message(void *muxer,
        const spatial_index<entity_store<entity_type>> &spatial_index,
        const controlled_entity_map &controlled);

    //! This is called by the generic netcode to inform the client state that more data may
    //! be written to the client connection
    //!
    //! \param muxer the muxer context
    //! \param worker_states state information about all simulation workers
    //! \param spatial_index all entities in the simulation indexed spatially
    //! \param controlled a map of entities controlled by external clients
    //! \param marshalling_factory a factory that be used to produce a marshaller which
    //! will construct data in the right format to be sent to a client
    void notify_writable(void *muxer,
        const std::unordered_map<uint64_t, worker_state<marshalling_type>> &worker_states,
        const spatial_index<entity_store<entity_type>> &spatial_index,
        const controlled_entity_map &controlled,
        const marshalling_type &marshalling_factory);

    //! Notifies the client state that a new worker has been registered with the muxer
    void new_worker(void *muxer, uint64_t worker_id);

    //! Inform the client state of new headers received from a specified worker
    void new_per_worker_data(void *muxer, uint64_t worker_id,
        const aether::span<const per_worker_data_type> &data);
};

//! The state associated with a muxer thread
template<typename Marshalling>
class generic_netcode {
private:
    using marshalling_type = Marshalling;
    using entity_type = typename marshalling_type::entity_type;
    using per_worker_data_type = typename marshalling_type::per_worker_data_type;

    uint64_t latest_tick = 0;
    std::unordered_map<uint64_t, connection_state<marshalling_type>> connection_states;
    std::unordered_map<uint64_t, worker_state<marshalling_type>> worker_states;
    entity_store<entity_type> entity_store;
    spatial_index<decltype(entity_store)> spatial_index;
    controlled_entity_map controlled_entities;
    marshalling_type marshalling_factory;
    generic_interest_policy interest_policy;

    static bool has_valid_position(const entity_type &entity);
    void process_payload(void *muxer, uint64_t worker_id, uint64_t tick, const void *data, size_t length);
    void prune();

public:
    generic_netcode(const generic_interest_policy &policy = generic_interest_policy(),
        const marshalling_type &_factory = marshalling_type());
    generic_netcode(const generic_netcode&) = delete;

    //! Informs the netcode of a new connection
    //!
    //! \param connection the opaque connection context
    //! \param id an integer identifier for this connection
    void new_connection(void *muxer, void *connection, uint64_t id);

    //! Informs the netcode of a new message from the simulation
    //!
    //! \param muxer the muxer context
    //! \param worker_id the id of the worker that sent the message
    //! \param tick the tick of the message
    //! \param the raw message data
    //! \param data_len the message length in bytes
    void new_simulation_message(void *muxer, uint64_t worker_id, uint64_t tick, const void *data, size_t data_len);

    //! Notifies the netcode that the specified connection is writable
    void notify_writable(void *muxer, uint64_t id);

    //! Notifies the netcode that the specified connection has been dropped
    void drop_connection(void *muxer, uint64_t id);
};

template<typename Marshaller>
bool generic_netcode<Marshaller>::has_valid_position(const entity_type &entity) {
    const auto position = promote_to_vec3f(get_position(entity));
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z);
}

template<typename Marshaller>
generic_netcode<Marshaller>::generic_netcode(const generic_interest_policy &_policy, const marshalling_type &_factory) :
    marshalling_factory(_factory), interest_policy(_policy), spatial_index(entity_store) {
}

template<typename Marshaller>
void generic_netcode<Marshaller>::new_connection(void *muxer, void *connection, uint64_t id) {
    const auto [iter, inserted] = connection_states.emplace(id, connection_state<marshalling_type>(connection, interest_policy, entity_store));
    assert(inserted);
    auto &connection_state = iter->second;
    for(const auto &[wid, worker_state] : worker_states) {
        connection_state.new_worker(muxer, wid);
    }
}

template<typename Marshaller>
void generic_netcode<Marshaller>::notify_writable(void *muxer, uint64_t id) {
    const auto it = connection_states.find(id);
    if(it != connection_states.end()) {
        auto &conn = it->second;
        aether::netcode::connection_notify_writable(conn.get_context(), muxer);
        if (aether::netcode::connection_is_drained(conn.get_context())) {
           conn.notify_writable(muxer, worker_states, spatial_index, controlled_entities, marshalling_factory);
           const bool wrote_data = !aether::netcode::connection_is_drained(conn.get_context());
            aether::netcode::connection_subscribe_writable(conn.get_context(), muxer, wrote_data);
        }
    } else {
        fprintf(stderr, "Received notify_writable for dead connection\n");
    }
}

template<typename Marshaller>
void generic_netcode<Marshaller>::drop_connection(void *muxer, uint64_t id) {
    const auto it = connection_states.find(id);
    if(it != connection_states.end()) {
        aether::netcode::release_connection(it->second.get_context());
        connection_states.erase(it);
    }
}

template<typename Marshaller>
void generic_netcode<Marshaller>::new_simulation_message(void *muxer, uint64_t worker_id, uint64_t tick, const void *data, size_t data_len) {
    {
        const auto old_tick = latest_tick;
        latest_tick = std::max(tick, latest_tick);
        if (latest_tick != old_tick) { prune(); }
    }

    // Save new packet
    const bool new_worker = worker_states.find(worker_id) == worker_states.end();
    auto &worker_state = worker_states[worker_id];
    worker_state.last_updated = clock_type::now();
    if (new_worker) {
        for (auto &[_, connection_state] : connection_states) {
            connection_state.new_worker(muxer, worker_id);
        }
    }
    process_payload(muxer, worker_id, tick, data, data_len);
}

template<typename Marshaller>
void generic_netcode<Marshaller>::process_payload(void *muxer, uint64_t worker_id, uint64_t tick, const void *data, size_t length) {
    const auto now = clock_type::now();
    const auto worker_iter = worker_states.find(worker_id);
    assert(worker_iter != worker_states.end());
    auto &worker = worker_iter->second;
    auto demarshaller = marshalling_factory.create_demarshaller();
    {
        demarshaller.decode(data, length);
        const auto worker_data = demarshaller.get_worker_data();
        if (!worker_data.empty()) {
            worker.headers.clear();
            for(const auto &[_, header] : worker_data) {
                worker.headers.push_back(header);
            }
            for (auto &[_, connection_state] : connection_states) {
                connection_state.new_per_worker_data(muxer,
                    worker_id, aether::span<const per_worker_data_type>(worker.headers));
            }
        }
    }
    const auto entities = demarshaller.get_entities();
    const typename decltype(entity_store)::metadata_type metadata = { tick, now, worker_id };

    for(const auto &entity : entities) {
        // To be robust against invalid positions we ignore entities that have them
        if (!has_valid_position(entity)) { continue; }

        const auto entity_id = get_entity_id(entity);
        if (auto owner = get_owner_id(entity)) {
            controlled_entity ce;
            ce.tick = tick;
            ce.player_id = owner.value();
            ce.entity_id = entity_id;
            ce.position = promote_to_vec3f(get_position(entity));
            controlled_entities[ce.player_id][ce.entity_id] = ce;
        }

        auto entity_handle = entity_store.find_entity(entity_id);
        const bool is_new = !entity_handle.has_value();
        if (is_new) {
            entity_handle = { entity_store.new_entity(metadata, entity_id, entity) };
        } else {
            entity_store.update_entity(metadata, entity_handle.value(), entity);
        }
        spatial_index.update_entity(entity_handle.value());
    }
    spatial_index.commit();

    for (auto &[_, connection_state] : connection_states) {
        connection_state.new_simulation_message(muxer, spatial_index, controlled_entities);
    }
}

template<typename Marshaller>
void generic_netcode<Marshaller>::prune() {
    const uint64_t min_tick = latest_tick > HISTORY_SIZE ? latest_tick - HISTORY_SIZE : 0;

    // Prune dead controlled entities
    for(auto &[_, player_entities] : controlled_entities) {
        auto entity_it = player_entities.begin();
        while(entity_it != player_entities.end()) {
            const auto &entity = entity_it->second;
            if (entity.tick < min_tick) {
                entity_it = player_entities.erase(entity_it);
            } else {
                ++entity_it;
            }
        }
    }

    // Prune dead entities
    const auto dead = entity_store.get_older_than(min_tick);
    for(const auto &handle : dead) {
        spatial_index.drop_entity(handle);
        entity_store.drop(handle);
    }
    spatial_index.commit();
}

template<typename Marshalling>
void *connection_state<Marshalling>::get_context() const {
    return conn_ctx;
}

template<typename Marshalling>
void connection_state<Marshalling>::new_worker(void *muxer, uint64_t worker_id) {
    worker_send_priorities.push(worker_id, clock_type::now());
    aether::netcode::connection_subscribe_writable(conn_ctx, muxer, true);
}

template<typename Marshalling>
void connection_state<Marshalling>::new_per_worker_data(void *muxer, uint64_t worker_id,
    const aether::span<const per_worker_data_type>& data) {
    worker_headers_changed[worker_id] = true;
}

template<typename Marshalling>
bool connection_state<Marshalling>::is_scheduled(const entity_handle &handle) const {
    return scheduled_entities.find(handle.get_id()) != scheduled_entities.end();
}

template<typename Marshalling>
std::optional<uint64_t> connection_state<Marshalling>::last_sent_tick(const entity_handle &handle) const {
    const auto scheduled_iter = scheduled_entities.find(handle.get_id());
    if (scheduled_iter == scheduled_entities.end()) {
        return std::nullopt;
    } else {
        return scheduled_iter->second.last_sent_tick;
    }
}

template<typename Marshalling>
void connection_state<Marshalling>::schedule_entity(const entity_store<entity_type> &store, const entity_handle &handle,
    const std::optional<time_point> &maybe_time, const bool update_last_sent) {

    if (maybe_time.has_value()) {
        const auto &time = maybe_time.value();
        const auto [new_bucket_idx, new_bucket_expiration] = get_temporal_bucket(time);
        bool insert = true;
        auto scheduled_iter = scheduled_entities.find(handle.get_id());
        if (scheduled_iter != scheduled_entities.end()) {
            // Entity is already scheduled
            const auto old_bucket = scheduled_iter->second.bucket_id;
            if (old_bucket != new_bucket_idx) {
                // Entity has changed bucket so remove it from old bucket
                const auto bucket_iter = send_buckets.find(scheduled_iter->second.bucket_id);
                if (bucket_iter != send_buckets.end()) {
                    bucket_iter->second.erase(handle);
                }
            } else {
                // Entity remains in the same bucket so there is nothing more to do
                insert = false;
            }
        } else {
            // Entity is new
            const auto [iter, inserted] = scheduled_entities.emplace(handle.get_id(), scheduled_entity_info{});
            assert(inserted);
            scheduled_iter = iter;
        }

        assert(scheduled_iter != scheduled_entities.end());

        if (insert) {
            // Schedule the new bucket if necessary
            if (send_buckets.count(new_bucket_idx) == 0) {
                send_priorities.push(new_bucket_idx, new_bucket_expiration);
            }
            send_buckets[new_bucket_idx].insert(handle);
            scheduled_iter->second.bucket_id = new_bucket_idx;
        }

        if (update_last_sent) {
            assert(store.is_valid(handle));
            scheduled_iter->second.last_sent_tick = { store.last_updated_tick(handle) };
        }
    } else {
        const auto scheduled_iter = scheduled_entities.find(handle.get_id());
        if (scheduled_iter != scheduled_entities.end()) {
            const auto bucket_id = scheduled_iter->second.bucket_id;
            scheduled_entities.erase(scheduled_iter);
            const auto bucket_iter = send_buckets.find(bucket_id);
            if (bucket_iter != send_buckets.end()) {
                bucket_iter->second.erase(handle);
            }
        }
    }
}

/**
 * @brief queries agents in the proximity of the player and schedules them to be sent if they have not already been.
 *
 * @sa interest_policy
 */
template<typename Marshalling>
void connection_state<Marshalling>::new_simulation_message(void *muxer, const spatial_index<entity_store<entity_type>> &spatial_index,
    const controlled_entity_map &controlled) {

    const auto now = clock_type::now();
    const auto &store = spatial_index.get_store();

    if (interest_policy.no_player_simulation) {
        std::optional<entity_handle> maybe_entity = store.first();
        while (maybe_entity) {
            const entity_handle entity = maybe_entity.value();
            if (!is_scheduled(entity)) {
                schedule_entity(store, entity, { now });
            }
            maybe_entity = store.next(maybe_entity.value());
        }
    } else {
        // Get all agents owned by this player
        std::vector<std::reference_wrapper<const controlled_entity>> player_entities;
        {
            const auto player_iter = controlled.find(player_id);
            if (player_iter != controlled.end()) {
                player_entities.reserve(player_iter->second.size());
                for(const auto &[_, entity] : player_iter->second) {
                    player_entities.push_back(entity);
                }
            }
        }
        for (const controlled_entity &player_entity : player_entities) {
            // we query the spatial index to get a list of all the entities that are inside the interest are
            // of the player defined in the interest_policy.
            // If performance is a concern spatial_index.find_entities_approximate can be used. See spatial_index
            // implementation for a detailed explanation.
            auto nearby = spatial_index.find_entities_exact(player_entity.position, interest_policy.get_cut_off());
            for(const entity_handle &h_entity : nearby) {
                // only update the entities that are not already scheduled. Because it is a new entity we schedule it
                // right away.
                if (!is_scheduled(h_entity)) {
                    schedule_entity(store, h_entity, { now });
                }
            }

            // Finally we check for dropped entities that are inside the interest area. Any entities found
            // are scheduled to be re-examined immediately since they may be dead, or may have moved
            // on to another area of the simulation outside our interest radius.
            nearby = drop_entities_spatial.find_entities_approximate(player_entity.position,
                                                                     interest_policy.get_cut_off());
            for(const entity_handle &h_entity : nearby) {
                if (!is_scheduled(h_entity)) {
                    schedule_entity(store, h_entity, { now });
                }
                drop_entities_spatial.drop_entity(h_entity);
            }
        }
        drop_entities_spatial.commit();
    }

    // notify that there is something to be sent
    const auto top = send_priorities.peek();
    if (top.has_value() && now >= top.value().second.time) {
        aether::netcode::connection_subscribe_writable(conn_ctx, muxer, true);
    }
}

template<typename Marshalling>
connection_state<Marshalling>::connection_state(void *_conn_ctx, const generic_interest_policy &policy, entity_store<entity_type> &store)
    : conn_ctx(_conn_ctx)
    , interest_policy(policy)
    , created(clock_type::now())
    , drop_entities_spatial(store) {
    player_id = aether::netcode::connection_get_player_id(conn_ctx);
}

template<typename Marshalling>
void connection_state<Marshalling>::notify_writable(void *muxer,
        const std::unordered_map<uint64_t, worker_state<marshalling_type>> &worker_states,
        const spatial_index<entity_store<entity_type>> &spatial_index,
        const controlled_entity_map &controlled,
        const marshalling_type &marshalling_factory) {

    // Get all agents owned by this player
    std::vector<std::reference_wrapper<const controlled_entity>> player_entities;
    {
        const auto player_iter = controlled.find(player_id);
        if (player_iter != controlled.end()) {
            player_entities.reserve(player_iter->second.size());
            for(const auto &[_, entity] : player_iter->second) {
                player_entities.push_back(entity);
            }
        }
    }

    const auto now = clock_type::now();
    bool has_useful_data = false;
    std::unordered_set<uint64_t> worker_headers_to_send;
    auto marshaller = marshalling_factory.create_marshaller();
    // We send headers for all workers at relatively high intervals, but will
    // send up-to-date headers for any workers that contain an entity that we also
    // send. This ensures that clients can track tick numbers effectively.
    while(const auto maybe_best = pop_best_per_worker(now)) {
        const auto wid = maybe_best.value();
        worker_headers_to_send.insert(wid);
    }

    const auto &store = spatial_index.get_store();

    while(const auto maybe_best = pop_best_bucket(now)) {
        const uint64_t bucket_idx = maybe_best.value();
        const auto bucket_iter = send_buckets.find(bucket_idx);
        assert(bucket_iter != send_buckets.end());
        const auto send_bucket = std::move(bucket_iter->second);
        send_buckets.erase(bucket_iter);

        for (const entity_handle &h_entity : send_bucket) {
            std::optional<time_point> next_time;
            if (store.is_valid(h_entity)) {
                entity_type entity = store.get(h_entity);
                if (interest_policy.no_player_simulation) {
                    next_time = { now };
                } else {
                    // one the entity has been added to the packet we reschedule it by finding the smallest
                    // distance to any of the player controlled entities and then evaluating it.
                    float min_distance = std::numeric_limits<float>::infinity();

                    for (const controlled_entity &player_entity : player_entities) {
                        vec3f distance = promote_to_vec3f(get_position(entity)) - player_entity.position;
                        min_distance = std::min(static_cast<float>(sqrt(distance.dot(distance))), static_cast<float>(min_distance));
                    }
                    next_time = interest_policy.evaluate(now, min_distance);

                    // if we got a time this means that the entity is inside the interest area so we put it to the
                    // corresponding bucket
                    if (!next_time) {
                        // if we did not get a time, this means that the entity is outside the interest area so we sent
                        // a drop message and add it to drop_entity_spatial
                        synthesize_drop_entity(entity);
                        drop_entities_spatial.update_entity(h_entity);
                    }
                }
                // Only send the entity if it has changed or it will be dropped
                if (!next_time ||
                    std::optional<uint64_t>(store.last_updated_tick(h_entity)) != last_sent_tick(h_entity)) {
                    marshaller.add_entity(entity);
                    // Ensure the header for the worker associated with this entity
                    // is up to date.
                    worker_headers_to_send.insert(store.last_worker(h_entity));
                }
            } else {
                // the entity is not valid anymore. This means it is dead.
                entity_type dead_entity;
                synthesize_dead_entity(store.get_entity_id(h_entity), dead_entity);
                marshaller.add_entity(dead_entity);
            }
            has_useful_data = true;
            schedule_entity(store, h_entity, next_time, true);
        }
    }
    drop_entities_spatial.commit();

    if (has_useful_data) {
        for(const auto &wid : worker_headers_to_send) {
            if (worker_headers_changed[wid]) {
                const auto worker_state_iter = worker_states.find(wid);
                if (worker_state_iter != worker_states.end()) {
                    const auto &headers = worker_state_iter->second.headers;
                    for(const auto &header : headers) {
                        marshaller.add_worker_data(wid, header);
                    }
                }
                worker_headers_changed[wid] = false;
            }
        }
        const auto packet = marshaller.encode();
        aether::netcode::connection_push_packet(
            conn_ctx, muxer, 0,
            packet.data(), packet.size());
    }
}

/**
 * this functions returns the bucket index and the expiration time for the corresponding bucket of a given time. The expiration
 * point is calculated rounding up the current to the granularity defined in interest_policy.scheduling_granularity_hz
 *
 * @param[in]  time i       time that an entity should be schedule
 *
 * @return std::tuple containing the bucket index and the expiration time
 */
template<typename Marshalling>
std::tuple<uint64_t, time_point> connection_state<Marshalling>::get_temporal_bucket(const time_point &time) const {
    const auto duration = time - created;
    const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    const int64_t us_per_bucket = static_cast<int64_t>(1000 * 1000 / interest_policy.scheduling_granularity_hz);
    uint64_t bucket_idx = duration_us.count() / us_per_bucket;
    int64_t expiration_us = ((duration_us.count() + us_per_bucket)/us_per_bucket) * us_per_bucket;
    time_point expiration = created + std::chrono::microseconds(expiration_us);
    return {bucket_idx, expiration};
}

/**
 * this functions returns the bucket_idx for the oldest bucket that has expired. If none has
 * expired nullopt is returned.
 *
 * @param[in] now expiration threshold. Typically current time.
 *
 * @return nullops if no buckets have expired, bucket_idx of the oldest expired otherwise
 */
template<typename Marshalling>

std::optional<uint64_t> connection_state<Marshalling>::pop_best_bucket(const time_point &now) {
    const auto top = send_priorities.peek();
    if (!top.has_value()) {
        return std::nullopt;
    }
    std::optional<uint64_t> bucket_idx;
    if (now >= top.value().second.time) {
        bucket_idx.emplace(top.value().first);
        send_priorities.pop();
    }

    return bucket_idx;
}

template<typename Marshalling>
std::optional<uint64_t> connection_state<Marshalling>::pop_best_per_worker(const time_point &now) {
    const auto top = worker_send_priorities.peek();
    if (!top.has_value()) { return std::nullopt; }
    std::optional<uint64_t> result;
    if (now >= top.value().second.time) {
        const auto wid = top.value().first;
        result.emplace(wid);

        // Schedule the next header update
        const auto wait_microseconds = 1e6 / interest_policy.per_worker_metadata_frequency_hz;
        const auto next = now + std::chrono::microseconds(static_cast<long>(wait_microseconds));
        worker_send_priorities.push(wid, next);
    }
    return result;
}

}

}
