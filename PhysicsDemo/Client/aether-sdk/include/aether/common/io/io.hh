#pragma once

#include <cstddef>
#include <cerrno>
#include <aether/common/container/ring_buffer.hh>

namespace aether {

struct reader {
    virtual ssize_t read(void *buffer, size_t count) = 0;
    virtual ~reader() {}
};

struct writer {
    virtual ssize_t write(const void *buffer, size_t count) = 0;
    virtual int flush() = 0;
    virtual ~writer() {}
};

template<typename R>
int read_exact(R &reader, void *_data, size_t count) {
    size_t bytes_read = 0;
    while(bytes_read < count) {
        const ssize_t read_size = reader.read(static_cast<char*>(_data) + bytes_read, count - bytes_read);
        if (read_size < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        }
        if (read_size == 0) {
            return -1;
        }
        bytes_read += read_size;
    }
    return 0;
}

template<typename W>
int write_all(W &writer, const void * data, size_t count) {
    size_t bytes_written = 0;
    while(bytes_written < count) {
        const ssize_t write_size = writer.write(static_cast<const char*>(data) + bytes_written, count - bytes_written);
        if (write_size < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            return -1;
        }
        if (write_size == 0) {
            return -1;
        }
        bytes_written += write_size;
    }
    return 0;
}

template<> int write_all(container::ring_buffer<char> &buffer, const void *data, size_t count);

} //aether
