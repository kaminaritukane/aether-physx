#pragma once
#include <aether/common/io/in_memory.hh>
#include "marshalling.hh"
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <type_traits>

namespace aether {

namespace netcode {

namespace detail {

static const uint64_t TRIVIAL_MARSHALLER_MAGIC = 0x251f2c5fc5d019d6ull;
static const uint16_t TRIVIAL_MARSHALLER_VERSION = 0;

enum class blob_type : unsigned char {
    static_data,
    worker_data,
    entity_data,
};

struct blob_header {
    detail::blob_type type;
    uint32_t count;
    uint32_t size;
};

}

template<typename Traits>
class trivial_marshaller : public marshaller<Traits> {
public:
    using entity_type = typename Traits::entity_type;
    using static_data_type = typename Traits::static_data_type;
    using per_worker_data_type = typename Traits::per_worker_data_type;

private:
    std::optional<static_data_type> static_data;
    std::vector<entity_type> entities;
    std::unordered_map<uint64_t, per_worker_data_type> worker_data;

    template<typename Writer>
    static int write_header(Writer &writer, const detail::blob_header &header) {
        int ret;
        ret = write_all(writer, &header.type, sizeof(header.type));
        if (ret != 0) { return ret; }
        ret = write_all(writer, &header.count, sizeof(header.count));
        if (ret != 0) { return ret; }
        ret = write_all(writer, &header.size, sizeof(header.size));
        return ret;
    }

public:
    void set_static_data(const static_data_type &data) override {
        static_data = data;
    }

    void add_entity(const entity_type &entity) override {
        entities.push_back(entity);
    }

    void add_worker_data(uint64_t worker_id, const per_worker_data_type &data) override {
        worker_data.insert({ worker_id, data });
    }

    void reserve(const size_t num_entities) override {
        entities.reserve(entities.size() + num_entities);
    }

    std::vector<char> encode() const override {
        std::vector<char> data;
        in_memory_writer<char> writer(data);
        write_all(writer, &detail::TRIVIAL_MARSHALLER_MAGIC, sizeof(detail::TRIVIAL_MARSHALLER_MAGIC));
        write_all(writer, &detail::TRIVIAL_MARSHALLER_VERSION, sizeof(detail::TRIVIAL_MARSHALLER_VERSION));

        const uint16_t num_headers = 3;
        write_all(writer, &num_headers, sizeof(num_headers));
        detail::blob_header blob_header;

        blob_header.type = detail::blob_type::static_data;
        blob_header.count = static_data.has_value() ? 1 : 0;
        blob_header.size = sizeof(static_data_type);
        write_header(writer, blob_header);

        blob_header.type = detail::blob_type::worker_data;
        blob_header.count = worker_data.size();
        blob_header.size = sizeof(uint64_t) + sizeof(per_worker_data_type);
        write_header(writer, blob_header);

        blob_header.type = detail::blob_type::entity_data;
        blob_header.count = entities.size();
        blob_header.size = sizeof(entity_type);
        write_header(writer, blob_header);

        if (static_data.has_value()) {
            write_all(writer, &static_data.value(), sizeof(static_data_type));
        }

        for(const auto &[id, worker_info] : worker_data) {
            write_all(writer, &id, sizeof(id));
            write_all(writer, &worker_info, sizeof(worker_info));
        }

        for(const auto &entity : entities) {
            write_all(writer, &entity, sizeof(entity));
        }

        return data;
    }
};

template<typename Traits>
class trivial_demarshaller : public demarshaller<Traits> {
public:
    using entity_type = typename Traits::entity_type;
    using static_data_type = typename Traits::static_data_type;
    using per_worker_data_type = typename Traits::per_worker_data_type;

private:
    std::optional<static_data_type> static_data;
    std::vector<entity_type> entities;
    std::unordered_map<uint64_t, per_worker_data_type> worker_data;

    template<typename Reader>
    int read_headers(Reader &reader, std::vector<detail::blob_header> &headers) {
        headers.clear();
        int ret;
        uint16_t num_headers;
        ret = read_exact(reader, &num_headers, sizeof(num_headers));
        if (ret != 0) { return ret; }
        headers.reserve(num_headers);

        for(size_t i = 0; i < num_headers; ++i) {
            detail::blob_header header;
            ret = read_exact(reader, &header.type, sizeof(header.type));
            if (ret != 0) { return ret; }
            ret = read_exact(reader, &header.count, sizeof(header.count));
            if (ret != 0) { return ret; }
            ret = read_exact(reader, &header.size, sizeof(header.size));
            if (ret != 0) { return ret; }

            headers.push_back(header);
        }

        return 0;
    }

public:
    bool decode(const void *data, size_t count) override {
        in_memory_reader reader(data, count);
        std::remove_cv<decltype(detail::TRIVIAL_MARSHALLER_MAGIC)>::type magic;
        std::remove_cv<decltype(detail::TRIVIAL_MARSHALLER_VERSION)>::type version;

        read_exact(reader, &magic, sizeof(magic));
        assert(magic == detail::TRIVIAL_MARSHALLER_MAGIC && "Data not written using trivial marshaller");

        read_exact(reader, &version, sizeof(version));
        assert(version == detail::TRIVIAL_MARSHALLER_VERSION && "Decoding using wrong version of trivial marshaller");

        std::vector<detail::blob_header> headers;
        read_headers(reader, headers);

        for(const auto &header : headers) {
            const size_t count = header.count;
            const size_t blob_size = header.size;

            switch(header.type) {
                case detail::blob_type::static_data: {
                    assert(blob_size == sizeof(static_data_type) && "Mismatch in static data size");
                    for(size_t i = 0; i < count; ++i) {
                        assert(!static_data.has_value() && "Multiple static datas in message");
                        static_data_type data;
                        read_exact(reader, &data, sizeof(data));
                        static_data = { data };
                    }
                    break;
                }
                case detail::blob_type::worker_data: {
                    assert((blob_size == sizeof(uint64_t) + sizeof(per_worker_data_type)) &&
                        "Mismatch in per-worker data size");
                    uint64_t worker_id;
                    per_worker_data_type data;
                    for(size_t i = 0; i < count; ++i) {
                        read_exact(reader, &worker_id, sizeof(worker_id));
                        read_exact(reader, &data, sizeof(data));
                        worker_data[worker_id] = data;
                    }
                    break;
                }
                case detail::blob_type::entity_data: {
                    assert(blob_size == sizeof(entity_type) && "Mismatch in entity data size");
                    entity_type entity;
                    entities.reserve(count);
                    for(size_t i = 0; i < count; ++i) {
                        read_exact(reader, &entity, sizeof(entity));
                        entities.push_back(entity);
                    }
                    break;
                }
                default: {
                    assert(false && "Unknown blob type");
                    return false;
                }
            }
        }

        return true;
    }

    std::vector<entity_type> get_entities() const override {
        return entities;
    }

    std::optional<static_data_type> get_static_data() const override {
        return static_data;
    }

    std::unordered_map<uint64_t, per_worker_data_type> get_worker_data() const override {
        return worker_data;
    }
};

template<typename Traits>
class trivial_marshalling : public marshalling_factory<trivial_marshaller<Traits>, trivial_demarshaller<Traits>> {
public:
    using traits_type = Traits;
    using entity_type = typename Traits::entity_type;
    using static_data_type = typename Traits::static_data_type;
    using per_worker_data_type = typename Traits::per_worker_data_type;

    trivial_marshaller<traits_type> create_marshaller() const override {
        return {};
    }

    trivial_demarshaller<traits_type> create_demarshaller() const override {
        return {};
    }
};

}

}
