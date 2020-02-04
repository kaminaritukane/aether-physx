#pragma once

#include <cassert>
#include <algorithm>
#include <cstring>
#include <type_traits>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <aether/common/hadean_platform.hh>
#include <aether/common/span.hh>

namespace aether {

namespace container {

template<typename T>
class ring_buffer {
private:
    static_assert(std::is_trivial<T>::value, "Only trivial types are supported");
    using value_type = T;
    size_t start;
    size_t length;
    size_t cap;

    struct free_deleter {
        void operator()(void *x) { ::free(x); }
    };

    using holder_type = std::unique_ptr<value_type[], free_deleter>;
    holder_type data;

    void make_contiguous() {
        if (start + length <= cap) { return; }
        const size_t left_size = start + length - cap;
        const size_t right_size = length - left_size;
        memmove(&data[left_size], &data[start], right_size * sizeof(value_type));
        std::rotate(&data[0], &data[left_size], &data[length]);
        start = 0;
    }

    void make_space(size_t count = 1) {
        const size_t MIN_EXTEND = 4096;
        if (free() < count) {
            const size_t new_cap = length + std::max(count, std::max(MIN_EXTEND, length));
            reserve(new_cap);
        }
    }

public:
    template<typename R> friend int read_to_end(R&, ring_buffer<char>&);
    template<typename R> friend int extend_from_reader(R&, ring_buffer<char>&, size_t);

    ring_buffer() : ring_buffer(0) {
    }

    ring_buffer(const size_t capacity) : start(0), length(0), cap(0), data(nullptr) {
        reserve(capacity);
    }

    void clear() {
        start = 0;
        length = 0;
    }

    bool is_empty() const {
        return length == 0;
    }

    bool is_full() const {
        return free() == 0;
    }

    bool has_space() const {
        return free() != 0;
    }

    size_t size() const {
        return length;
    }

    size_t free() const {
        return cap - length;
    }

    void reserve(const size_t new_capacity) {
        if (new_capacity > cap) {
            make_contiguous();
            value_type *ptr = data.release();
            ptr = static_cast<value_type*>(realloc(ptr, new_capacity * sizeof(value_type)));
            if (ptr == nullptr) {
                // Would be nice to use the logger here, but it's C++17
                perror("ring_buffer<T>::reserve():");
                abort();
            }
            cap = new_capacity;
            data.reset(ptr);
        }
    }

    void shift_to_front() {
        make_contiguous();
        if (start != 0) {
            memmove(&data[0], &data[start], length * sizeof(value_type));
            start = 0;
        }
    }

    size_t capacity() const {
        return cap;
    }

    ssize_t try_write(const value_type *new_data, const size_t data_length) {
        size_t copied = 0;
        while (has_space() && copied < data_length) {
            const auto unallocated = get_unallocated();
            const auto copy_size = std::min(data_length - copied, unallocated.size());
            memcpy(unallocated.data(), new_data + copied, copy_size * sizeof(value_type));
            move_tail(copy_size);
            copied += copy_size;
        }
        return copied;
    }

    int flush() {
        return 0;
    }

    ssize_t write(const value_type *new_data, const size_t data_length) {
        if (data_length == 0) { return 0; }
        make_space();
        return try_write(new_data, data_length);
    }

    void extend(const value_type *new_data, const size_t data_length) {
        make_space(data_length);
        size_t copied = 0;
        while (copied < data_length) {
            const auto copy_size = try_write(new_data + copied, data_length - copied);
            assert(copy_size != 0);
            copied += copy_size;
        }
    }

    ssize_t read(value_type *out, const size_t out_length) {
        return try_read(out, out_length);
    }

    ssize_t try_read(value_type *out, const size_t out_length) {
        size_t copied = 0;
        while(!is_empty() && copied < out_length) {
            const auto head = get_head();
            const auto copy_size = std::min(out_length - copied, head.size());
            memcpy(out + copied, head.data(), copy_size * sizeof(value_type));
            move_head(copy_size);
            copied += copy_size;
        }
        return copied;
    }

    span<const value_type> get_head() const {
        const size_t len = std::min(length, cap - start);
        return span<const value_type>(&data[start], len);
    }

    span<value_type> get_unallocated() {
        size_t ustart = start + length;
        ustart = ustart >= cap ? ustart - cap : ustart;

        const size_t unused = cap - length;
        const size_t ulength = std::min(unused, cap - ustart);
        return span<value_type>(&data[ustart], ulength);
    }

    span<const value_type> get_from_offset(const size_t offset) const {
        assert(offset <= length);
        size_t ustart = start + offset;
        ustart = ustart >= cap ? ustart - cap : ustart;
        const size_t ulength = std::min(length - offset, cap - ustart);
        return span<const value_type>(&data[ustart], ulength);
    }

    span<const value_type> as_contiguous() {
        make_contiguous();
        const auto head = get_head();
        assert(head.size() == length);
        return head;
    }

    void move_head(const size_t count) {
        assert(count <= length);
        length -= count;
        if (length == 0) {
            start = 0;
        } else {
            start += count;
            start = start >= cap ? start - cap : start;
        }
    }

    void move_tail(const size_t count) {
        length += count;
        assert(length <= cap);
    }
};

template<typename R>
int extend_from_reader(R &reader, ring_buffer<char> &buffer, size_t num_bytes) {
    while (num_bytes > 0) {
        buffer.make_space();
        const auto unallocated = buffer.get_unallocated();
        const auto ret = reader.read(unallocated.data(), std::min(num_bytes, unallocated.size()));
        if (ret <= 0) { return ret; }
        buffer.move_tail(ret);
        num_bytes -= ret;
    }
    return 0;
}

template<typename R>
int read_to_end(R &reader, ring_buffer<char> &buffer) {
    while (true) {
        buffer.make_space();
        const auto unallocated = buffer.get_unallocated();
        const auto ret = reader.read(unallocated.data(), unallocated.size());
        if (ret <= 0) { return ret; }
        buffer.move_tail(ret);
    }
}

}

}
