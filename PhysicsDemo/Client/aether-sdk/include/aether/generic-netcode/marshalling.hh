#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>

namespace aether {

namespace netcode {

template<typename Traits>
class marshaller {
public:
    using entity_type = typename Traits::entity_type;
    using static_data_type = typename Traits::static_data_type;
    using per_worker_data_type = typename Traits::per_worker_data_type;

    virtual void set_static_data(const static_data_type &data) = 0;
    virtual void reserve(const size_t count) = 0;
    virtual void add_entity(const entity_type &entity) = 0;
    virtual void add_worker_data(uint64_t worker_id, const per_worker_data_type &data) = 0;
    virtual std::vector<char> encode() const = 0;
};

template<typename Traits>
class demarshaller {
public:
    using entity_type = typename Traits::entity_type;
    using static_data_type = typename Traits::static_data_type;
    using per_worker_data_type = typename Traits::per_worker_data_type;

    virtual bool decode(const void *data, size_t count) = 0;
    virtual std::vector<entity_type> get_entities() const = 0;
    virtual std::optional<static_data_type> get_static_data() const = 0;
    virtual std::unordered_map<uint64_t, per_worker_data_type> get_worker_data() const = 0;
    virtual ~demarshaller() {}
};

template<typename Marshaller, typename Demarshaller>
class marshalling_factory {
public:
    using marshaller_type = Marshaller;
    using demarshaller_type = Demarshaller;
    using entity_type = typename marshaller_type::entity_type;
    using static_data_type = typename marshaller_type::static_data_type;
    using per_worker_data_type = typename marshaller_type::per_worker_data_type;

    virtual marshaller_type create_marshaller() const = 0;
    virtual demarshaller_type create_demarshaller() const = 0;
    virtual ~marshalling_factory() {}
};

}

}
