#pragma once

#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#include <optional>
#include "eigen_serde.hh"

namespace aether {

namespace serde {

template<typename T>
T& declref() noexcept;

template<typename T>
struct is_memcopyable {
    static constexpr bool value =
        std::is_default_constructible<T>::value &&
        std::is_trivially_copyable<T>::value &&
        (std::is_arithmetic<T>::value || std::is_enum<T>::value);
};

template<typename SD, typename T, typename = void>
struct serde_load_helper {
    void operator()(SD &sd, T &value) const {
        static_assert(is_memcopyable<T>::value, "serde_load() must be implemented for specified type");
        sd.visit_bytes(&value, sizeof(T));
    }
};

template<typename SD, typename T>
struct serde_load_helper<SD, T,
        typename std::enable_if<
            std::is_same<
                decltype(std::declval<T>().serde_load(declref<SD>())),
                void
            >::value
        >::type
    > {
    void operator()(SD &sd, T &value) const {
        value.serde_load(sd);
    }
};

template<typename SD, typename T>
struct serde_load_helper<SD, T,
        typename std::enable_if<
            std::is_same<
                decltype(std::declval<T>().serde_visit(declref<SD>())),
                void
            >::value
        >::type
    > {
    void operator()(SD &sd, T &value) const {
        value.serde_visit(sd);
    }
};

template<typename SD, typename T>
struct serde_load_helper<SD, T,
        typename std::enable_if<
            std::is_same<
                decltype(serde_visit(declref<SD>(), declref<T>())),
                void
            >::value
        >::type
    > {
    void operator()(SD &sd, T &value) const {
        serde_visit(sd, value);
    }
};

template<typename SD, typename T, typename = void>
struct serde_save_helper {
    void operator()(SD &sd, T &value) const {
        static_assert(is_memcopyable<T>::value, "serde_save() must be implemented for specified type");
        sd.visit_bytes(&value, sizeof(T));
    }
};

template<typename SD, typename T>
struct serde_save_helper<SD, T,
        typename std::enable_if<
            std::is_same<
                decltype(std::declval<T>().serde_save(declref<SD>())),
                void
            >::value
        >::type
    > {
    void operator()(SD &sd, T &value) const {
        value.serde_save(sd);
    }
};

template<typename SD, typename T>
struct serde_save_helper<SD, T,
        typename std::enable_if<
            std::is_same<
                decltype(std::declval<T>().serde_visit(declref<SD>())),
                void
            >::value
        >::type
    > {
    void operator()(SD &sd, T &value) const {
        value.serde_visit(sd);
    }
};

template<typename SD, typename T>
struct serde_save_helper<SD, T,
        typename std::enable_if<
            std::is_same<
                decltype(serde_visit(declref<SD>(), declref<T>())),
                void
            >::value
        >::type
    > {
    void operator()(SD &sd, T &value) const {
        serde_visit(sd,value);
    }
};


template<typename SD, typename T>
void serde_save(SD &sd, T &value) {
    serde_save_helper<SD, T>()(sd, value);
}

template<typename SD, typename T>
void serde_load(SD &sd, T &value) {
    serde_load_helper<SD, T>()(sd, value);
}

template<typename SD, typename T, typename = void>
struct serde_save_seq {
    void operator()(SD &sd, T *elems, size_t count) const {
        sd.visit_size(count);
        for (size_t i = 0; i < count; ++i) {
            serde_save(sd, elems[i]);
        }
    }
};

template<typename SD, typename T, typename = void>
struct serde_load_seq {
    void operator()(SD &sd, T *elems, size_t count) const {
        for(size_t i = 0; i < count; ++i) {
            serde_load(sd, elems[i]);
        }
    }
};

template<typename SD, typename T>
struct serde_save_seq<SD, T, typename std::enable_if<is_memcopyable<T>::value>::type> {
    void operator()(SD &sd, T *elems, size_t count) const {
        sd.visit_size(count);
        sd.visit_bytes(elems, sizeof(T) * count);
    }
};

template<typename SD, typename T>
struct serde_load_seq<SD, T, typename std::enable_if<is_memcopyable<T>::value>::type> {
    void operator()(SD &sd, T *elems, size_t count) const {
        sd.visit_bytes(elems, sizeof(T) * count);
    }
};

template<typename SD>
void serde_visit(SD &sd, std::monostate&) {
}

template<typename SD, typename T>
void serde_save(SD &sd, std::optional<T> &opt) {
    const bool present = opt.has_value();
    serde_save(sd, present);
    if (present) {
        serde_save(sd, opt.value());
    }
}

template<typename SD, typename T>
void serde_load(SD &sd, std::optional<T> &opt) {
    bool present;
    serde_load(sd, present);
    if (present) {
        T value;
        serde_load(sd, value);
        opt.emplace(std::move(value));
    }
}

template<typename SD, typename T>
void serde_save(SD &sd, std::vector<T> &vec) {
    serde_save_seq<SD, T>()(sd, &vec[0], vec.size());
}

template<typename SD, typename T>
void serde_load(SD &sd, std::vector<T> &vec) {
    const auto size = sd.visit_size();
    vec.resize(size);
    serde_load_seq<SD, T>()(sd, &vec[0], vec.size());
}

template<typename SD, typename T, std::size_t N>
void serde_save(SD &sd, std::array<T, N> &arr) {
    serde_save_seq<SD, T>()(sd, &arr[0], arr.size());
}

template<typename SD, typename T, std::size_t N>
void serde_load(SD &sd, std::array<T, N> &arr) {
    const auto size = sd.visit_size();
    assert(size == arr.size() && "Incorrect number of elements in array");
    serde_load_seq<SD, T>()(sd, &arr[0], arr.size());
}

template<typename SD, typename CharT, typename Traits, typename Allocator>
void serde_save(SD &sd, std::basic_string<CharT, Traits, Allocator> &str) {
    serde_save_seq<SD, CharT>()(sd, str.data(), str.size());
}

template<typename SD, typename CharT, typename Traits, typename Allocator>
void serde_load(SD &sd, std::basic_string<CharT, Traits, Allocator> &str) {
    const auto size = sd.visit_size();
    str.resize(size);
    serde_load_seq<SD, CharT>()(sd, str.data(), size);
}

template<typename Writer>
class writer_serializer {
private:
    Writer &inner;
    int error = 0;

public:
    writer_serializer(Writer &_inner) : inner(_inner) {
    }

    template<typename T>
    writer_serializer &operator&(T &value) {
        if (error == 0) {
            serde_save(*this, value);
        }
        return *this;
    }

    void visit_bytes(const void *data, const size_t count) {
        if (error == 0) {
            error = write_all(inner, data, count);
        }
    }

    void visit_size(const size_t count) {
        uint64_t data = count;
        serde_save(*this, data);
    }

    template<typename I>
    void visit_varint(I value) {
        static_assert(std::is_unsigned<I>::value, "Varint type must be unsigned integral");
        uint8_t buffer[sizeof(I) * 2];
        size_t length = 0;
        bool finished = false;
        while(!finished && error == 0) {
            buffer[length] = static_cast<uint8_t>(value & 127);
            value >>= 7;
            finished = (value == 0);
            if (!finished) { buffer[length] |= static_cast<uint8_t>(128); }
            ++length;
        }
        visit_bytes(&buffer[0], length);
    }

    int get_error() const {
        return error;
    }
};

template<typename Reader>
class reader_deserializer {
private:
    Reader &inner;
    int error = 0;

public:
    reader_deserializer(Reader &_inner) : inner(_inner) {
    }

    template<typename T>
    reader_deserializer &operator&(T &value) {
        if (error == 0) {
            serde_load(*this, value);
        }
        return *this;
    }

    void visit_bytes(void *data, const size_t count) {
        if (error == 0) {
            error = read_exact(inner, data, count);
        }
    }

    size_t visit_size() {
        if (error == 0) {
            uint64_t data;
            serde_load(*this, data);
            return static_cast<size_t>(data);
        } else {
            return 0;
        }
    }

    template<typename I>
    void visit_varint(I &value) {
        static_assert(std::is_unsigned<I>::value, "Varint type must be unsigned integral");
        value = 0;
        uint8_t component;
        bool finished = false;
        size_t shift = 0;
        while(!finished && error == 0) {
            visit_bytes(&component, 1);
            const auto unshifted = static_cast<I>(component & 127);
            const auto shifted = unshifted << shift;
            assert(unshifted == (shifted >> shift) && "reader_deserializer::visit_varint: type too small");
            value |= shifted;
            shift += 7;
            finished = (component & 128) == 0;
        }
    }

    int get_error() const {
        return error;
    }
};

template<typename Writer, size_t BufferSize = 64>
struct fixed_size_buffered_writer {
    Writer &inner;
    std::array<char, BufferSize> buffer;
    size_t length = 0;

    fixed_size_buffered_writer(Writer &_inner) : inner(_inner) {
        static_assert(BufferSize > 0, "Buffer must be at least 1 byte");
    }

    int write(const void *data, size_t count) {
        if (length == buffer.size()) {
            const auto res = flush();
            if (res != 0) { return res; }
        }
        const size_t copy_size = std::min(count, buffer.size() - length);
        memcpy(&buffer[length], data, copy_size);
        length += copy_size;
        return copy_size;
    }

    int flush() {
        if (length == 0) { return 0; }
        const auto res = write_all(inner, &buffer[0], length);
        if (res != 0) { return res; }
        length = 0;
        return 0;
    }

    ~fixed_size_buffered_writer() {
        flush();
    }
};

template<typename Writer, typename Value>
int write_msg(Writer &writer, const Value &value) {
    fixed_size_buffered_writer<Writer> buffered_writer(writer);
    {
        writer_serializer<decltype(buffered_writer)> serializer(buffered_writer);
        serializer & const_cast<Value&>(value);
        if (serializer.get_error() != 0) { return serializer.get_error(); }
    }
    const auto res = buffered_writer.flush();
    return res;
}


template<typename Reader, typename Value>
int read_msg(Reader &reader, Value &value) {
    reader_deserializer<Reader> deserializer(reader);
    deserializer & value;
    return deserializer.get_error();
}

}

}
