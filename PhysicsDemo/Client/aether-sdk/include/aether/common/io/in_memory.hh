#pragma once
#include <aether/common/io/io.hh>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <utility>
#include <type_traits>

namespace aether {

struct in_memory_reader final : public aether::reader {
  private:
    const char *storage;
    size_t offset, size;

  public:
    ssize_t read(void *out, size_t len) final {
        if (len > size - offset) {
            len = size - offset;
        }
        memcpy(out, storage + offset, len);
        offset += len;
        return len;
    }

    in_memory_reader(const void *buf, const size_t len)
        : storage(static_cast<const char*>(buf)), offset(0), size(len) {
    }
};

template<typename Storage>
struct in_memory_writer final : public aether::writer {
  private:
    typedef Storage storage_type;
    std::vector<storage_type> &storage;

  public:
    ssize_t write(const void *in, size_t len) final {
        storage.insert(storage.end(), reinterpret_cast<const char*>(in), reinterpret_cast<const char*>(in) + len);
        return len;
    }

    int flush() final {
        return 0;
    }

    in_memory_writer(std::vector<storage_type> &buf) : storage(buf) {
        static_assert(std::is_trivial<storage_type>::value, "Storage type must be single-char type");
        static_assert(sizeof(storage_type) == 1, "Storage type must be single-char type");
    }
};

}
