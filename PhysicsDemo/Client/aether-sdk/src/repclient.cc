// One might expect This include to be "repclient.hh", but we do it this way
// so that our windows tooling works better. It would be nice to fix
#include <aether/repclient.hh>
#include <aether/common/client_message.hh>
#include <aether/common/logging.hh>

#include <array>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sys/types.h>
#include <thread>
#include <array>

#include <aether/common/tcp.hh>
#include <aether/common/timer.hh>

#if defined(__linux__)

#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>

#elif defined(_WIN32)

// Silence warnings about fopen
#define _CRT_SECURE_NO_WARNINGS

#include <basetsd.h>
using ssize_t = SSIZE_T;

#endif

namespace tcp = aether::tcp;
namespace timer = aether::timer;
namespace client_encoding = aether::message_encoding::client;

static constexpr size_t MIN_SLACK_BYTES = 128;
static constexpr size_t MIN_BUF_SIZE = 1024;

namespace aether {

namespace repclient {

namespace detail {

void msgbuf::reserve(const size_t bytes) {
    if (buf.size() < bytes) {
        buf.resize(std::max(buf.size() * 2, std::max(bytes, MIN_BUF_SIZE)));
    }
}

void *msgbuf::consume_message(size_t *const length) {
    using prefix_t = uint32_t;
    const size_t remaining = len - pos;
    if (remaining < sizeof(prefix_t)) { return nullptr; }
    const prefix_t message_size = *(reinterpret_cast<const prefix_t*>(&buf[pos]));
    if (remaining < sizeof(prefix_t) + message_size) { return nullptr; }
    *length = message_size;
    const auto old_pos = pos;
    pos += sizeof(prefix_t) + message_size;
    return &buf[old_pos + sizeof(prefix_t)];
}

}

}

}

static void close_file(FILE *const f) {
    assert(f != nullptr);
    const auto ret = fclose(f);
    if (ret != 0) {
        perror("fclose");
        abort();
    }
}

void repclient::do_receives(repclient &client) {
    auto &receive_buffer = client.impl.get_receive_buffer();
    fd_set set;
    FD_ZERO(&set);
#if defined(__linux__)
    const int nfds = client.socket + 1;
    assert(nfds <= FD_SETSIZE);
#elif defined(_WIN32)
    const int nfds = 0;
#endif
    bool alive = true;
    while (alive) {
        {
            std::unique_lock<std::mutex> lock(client.receive_mutex);
            client.receive_condition.wait(lock, [&client, &receive_buffer] {
                return receive_buffer.has_space() || !client.alive;
            });
            alive &= client.alive;
        }
        FD_SET(client.socket, &set);
        // select() can change the timeout!
        timeval timeout;
        timeout.tv_sec = MAX_SHUTDOWN_TIME_SECONDS;
        timeout.tv_usec = 0;
        const auto ret = select(nfds, &set, nullptr, nullptr, &timeout);
        if (ret == SOCKET_ERROR) {
            alive = false;
            AETHER_LOG(ERROR)("Error receiving, killing receive thread.");
        }
        if (FD_ISSET(client.socket, &set)) {
            std::unique_lock<std::mutex> lock(client.receive_mutex);
            const auto unallocated = receive_buffer.get_unallocated();
            const ssize_t bytes_read = recv(client.socket,
                unallocated.data(),
                unallocated.size(),
                0);
            if (bytes_read < 0) {
                alive = false;
                AETHER_LOG(ERROR)("Error receiving, killing receive thread.");
            } else {
                receive_buffer.move_tail(bytes_read);
            }
        }
    }
}

void repclient::do_sends(repclient &client) {
    auto &send_buffer = client.impl.get_send_buffer();
    fd_set set;
    FD_ZERO(&set);
#if defined(__linux__)
    const int nfds = client.socket + 1;
    assert(nfds <= FD_SETSIZE);
#elif defined(_WIN32)
    const int nfds = 0;
#endif
    bool alive = true;
    while(alive) {
        {
            std::unique_lock<std::mutex> lock(client.send_mutex);
            client.send_condition.wait(lock, [&client, &send_buffer] {
                return !send_buffer.is_empty() || !client.alive;
            });
            alive &= client.alive;
        }
        FD_SET(client.socket, &set);
        // select() can change the timeout!
        timeval timeout;
        timeout.tv_sec = MAX_SHUTDOWN_TIME_SECONDS;
        timeout.tv_usec = 0;
        const auto ret = select(nfds, nullptr, &set, nullptr, &timeout);
        if (ret == SOCKET_ERROR) {
            alive = false;
            AETHER_LOG(ERROR)("Error sending, killing send thread.");
        }
        bool doing_send = false;
        ssize_t bytes_written = 0;
        if (FD_ISSET(client.socket, &set)) {
            std::unique_lock<std::mutex> lock(client.send_mutex);
            doing_send = client.doing_send > 0;
            const auto head = send_buffer.get_head();
            bytes_written = sendto(client.socket,
                head.data(),
                head.size(),
                0,
                nullptr, 0);
            if (bytes_written > 0) {
                send_buffer.move_head(bytes_written);
            }
        }
        if (bytes_written < 0) {
            alive = false;
            AETHER_LOG(ERROR)("Error sending, killing send thread.");
        } else if (doing_send && bytes_written > 0) {
            client.send_condition.notify_all();
        }
    }
}

size_t repclient_protocol::try_fill_buf_simulation(void *const buf, const size_t wanted) {
    size_t copied = 0;
    {
        bool progress = true;
        while (progress && copied < wanted) {
            const size_t copy_size = receive_buffer.try_read(static_cast<char*>(buf) + copied, wanted - copied);
            copied += copy_size;
            progress = copy_size > 0;
        }
    }
    return copied;
}

typename repclient::duration_type repclient::last_packet_time() const {
    return std::chrono::duration<double>(current_packet_time);
}

aether::container::ring_buffer<char> &repclient_protocol::get_send_buffer() {
    return send_buffer;
}

aether::container::ring_buffer<char> &repclient_protocol::get_receive_buffer() {
    return receive_buffer;
}

bool repclient_protocol::try_send(const void *const data, const size_t length) {
    return try_send_message(client_encoding::INTERACTION, data, length);
}

bool repclient_protocol::try_send_authentication_payload(const void *const data, const size_t length) {
    return try_send_message(client_encoding::AUTHENTICATE, data, length);
}

bool repclient_protocol::try_authenticate_player_id(const uint64_t id) {
    return try_send_authentication_payload(&id, sizeof(id));
}

bool repclient_protocol::try_authenticate_player_id_with_token(uint64_t id, const std::array<unsigned char, 32>& token) {
    std::array<unsigned char, 40> payload;
    for(int i = 0; i < 8; ++i) {
        payload[i] = (id >> (i*8)) & 0xFF;
    }
    std::copy(token.begin(), token.end(), payload.begin() + 8);
    return try_send_authentication_payload(payload.data(), 40);
}

bool repclient_protocol::try_send_message(client_encoding::message_type ty, const void *const data, const size_t length) {
    client_encoding::header header;
    header.msg_type = ty;
    header.payload_size = length;
    assert(header.payload_size == length && "Interaction message too large");
    constexpr size_t header_size = sizeof(header);

    if (header_size + length <= send_buffer.free()) {
        send_buffer.extend(
            static_cast<const char*>(static_cast<const void*>(&header)),
            header_size);
        send_buffer.extend(static_cast<const char*>(data), length);
        return true;
    } else if (header_size + length <= send_buffer.capacity()) {
        return false;
    } else {
        fprintf(stderr, "Attempted to send interaction packet larger than buffer");
        abort();
    }
}


// Supports both file fds and socket fds, both blocking and nonblocking
static size_t _internal_try_fill_buf(FILE *const file, void *const buf, const size_t wanted) {
    if (wanted == 0) { return 0; }
    const size_t n = fread(buf, sizeof(char), wanted, file);
    if (n == wanted || feof(file)) {
        return n;
    } else {
#if defined(__linux__)
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            clearerr(file);
            return 0;
        }
#endif
        perror("fread");
        abort();
    }
}

static size_t try_fill_buf_file(FILE *const file, void *const buf, const size_t wanted) {
    return _internal_try_fill_buf(file, buf, wanted);
}

static void write_all(FILE *const file, const void *const data, const size_t size) {
    const size_t n = fwrite(data, sizeof(char), size, file);
    if (n != size) {
        perror("fwrite");
        abort();
    }
}

void *repclient::tick(size_t *const length) {
    void *result = nullptr;
    // We want length to be a size_t but we use uint64_t lengths for the
    // helper functions since they use the length parameter to infer the
    // size of the length value in the header.
    stream_id worker_id;
    uint64_t length_u64 = 0;
    switch (client_mode) {
        case mode::LIVE:
            result = tick_live(&worker_id, &length_u64);
            break;
        case mode::RECORD:
            result = tick_record(&worker_id, &length_u64);
            break;
        case mode::PLAYBACK:
            result = tick_playback(&worker_id, &length_u64);
            break;
        default:
            abort();
    }
    *length = static_cast<size_t>(length_u64);
    assert(*length == length_u64 && "Packet too large");
    return result;
}

void *repclient::tick_record(uint64_t *const worker_id, uint64_t *const length) {
    assert(rec_file != nullptr);
    if (auto *const buf = static_cast<char *>(tick_live(worker_id, length))) {
        write_all(rec_file, worker_id, sizeof(*worker_id));
        write_all(rec_file, &current_packet_time, sizeof(current_packet_time));
        write_all(rec_file, length, sizeof(*length));
        write_all(rec_file, buf, *length);
        const auto ret = fflush(rec_file);
        if (ret != 0) {
            perror("fflush");
            abort();
        };
        return buf;
    }
    return nullptr;
}

void *repclient::tick_playback(uint64_t *const worker_id, uint64_t *const length) {
    // Note that the playback file could be truncated at any point, and we'd really like
    // to just return NULLs once we reach the 'end', even if it's not been correctly closed
    assert(rec_file != nullptr);
    const size_t headersize = sizeof(*worker_id) + sizeof(current_packet_time) + sizeof(*length);
    auto &playbuf = playback_buf;
    playbuf.reserve(headersize);

    while (playbuf.len < headersize) {
        const size_t n = try_fill_buf_file(rec_file, &playbuf.buf[playbuf.len], headersize - playbuf.len);
        playbuf.len += n;
        if (n == 0) { return nullptr; }
    }
    memcpy(worker_id, &playbuf.buf[0], sizeof(*worker_id));
    memcpy(&current_packet_time, &playbuf.buf[sizeof(*worker_id)], sizeof(current_packet_time));
    memcpy(length, &playbuf.buf[sizeof(*worker_id) + sizeof(current_packet_time)], sizeof(*length));

    playbuf.reserve(headersize + *length);
    while (playbuf.len != *length + headersize) {
        const size_t n = try_fill_buf_file(rec_file, &playbuf.buf[playbuf.len], headersize + *length - playbuf.len);
        playbuf.len += n;
        if (n == 0) { return nullptr; }
    }

    if (start_time == timer::time_type{}) { start_time = timer::get(); }
    if (timer::diff(timer::get(), start_time) < current_packet_time) {
        return nullptr;
    } else {
        playbuf.len = 0;
        return &playbuf.buf[headersize];
    }
}

// Although this code exits as soon as a read is too short, it should be asking the OS
// how many bytes are available and use this
void *repclient_protocol::tick(uint64_t *const worker_id, uint64_t *const length) {
    while (true) {
        // try to recv the multiplexer header
        if (cur_header_got < sizeof(cur_header)) {
            const size_t wanted = sizeof(cur_header) - cur_header_got;
            const size_t n = try_fill_buf_simulation(reinterpret_cast<uint8_t *>(&cur_header) + cur_header_got, wanted);
            cur_header_got += n;
            if (cur_header_got != sizeof(cur_header)) {
                return nullptr;
            }
        }

        // Add new connection if necessary
        const uint64_t wid = cur_header.id;

        // Fill the client buffer
        auto &msgbuf = msgbufs[wid];
        if (cur_header.len) {
            // shift the buf back to the beginning since we're going to be receiving network data
            if (msgbuf.pos > 0) {
                memmove(&msgbuf.buf[0], &msgbuf.buf[msgbuf.pos], msgbuf.len - msgbuf.pos);
                msgbuf.len -= msgbuf.pos;
                msgbuf.pos = 0;
            }
            // Initialise buf for first time, or give enough slack to avoid reads of tiny numbers of bytes
            if (msgbuf.buf.size() - msgbuf.len < MIN_SLACK_BYTES) {
                msgbuf.reserve(std::max(MIN_BUF_SIZE, msgbuf.buf.size() * 2));
            }
            const size_t wanted = std::min(cur_header.len, msgbuf.buf.size() - msgbuf.len);
            const size_t n = try_fill_buf_simulation(&msgbuf.buf[msgbuf.len], wanted);
            cur_header.len -= n;
            msgbuf.len += n;
        }

        // Return a message if present
        void *const msg = msgbuf.consume_message(length);
        if (msg != nullptr) {
            *worker_id = wid;
            return msg;
        } else if (cur_header.len > 0) {
            return nullptr;
        } else {
            // no more messages to drain and no more multiplexer message to recv, prep for next multiplexer header
            cur_header_got = 0;
        }
    }
}

aether::tcp::os_socket repclient::construct_socket(const char *host, const char *port) {
#if defined(_WIN32)
    WSADATA wsa_data;
    const auto result = WSAStartup(MAKEWORD(2,2), &wsa_data);
    assert(result == 0 && "Unable to start Windows sockets");
#endif
    auto socket = tcp::connect_to_host_port_with_timeout(host, port);
    assert(socket != INVALID_SOCKET);
    tcp::socket_set_nonblocking(socket);
    return socket;
}

repclient::repclient(const char *host, const char *port) : client_mode(mode::LIVE) {
    socket = construct_socket(host, port);
    setup_threads();
}

repclient::repclient(const char *host, const char *port, const char *path) :
    client_mode(mode::RECORD) {
    rec_file = fopen(path ? path : DEFAULT_DUMP_FILE, "wb");
    if (rec_file == nullptr) {
        perror("fopen");
        abort();
    }
    socket = construct_socket(host, port);
    setup_threads();
}

repclient::repclient(const char *path) : client_mode(mode::PLAYBACK) {
    rec_file = fopen(path ? path : DEFAULT_DUMP_FILE, "rb");
    if (rec_file == nullptr) {
        perror("fopen");
        abort();
    }
#if defined(__linux__)
    const auto fd = fileno(rec_file);
    tcp::socket_set_nonblocking(fd);
#endif
    playback_buf.buf.resize(4096);
}

void *repclient::tick_live(stream_id *worker_id, uint64_t *msg_size) {
    bool was_full;
    bool is_full;
    void *packet;
    {
        std::unique_lock<std::mutex> lock(receive_mutex);
        was_full = impl.get_receive_buffer().is_full();
        packet = impl.tick(worker_id, msg_size);
        is_full = impl.get_receive_buffer().is_full();
    }
    if (was_full && !is_full) {
        receive_condition.notify_one();
    }
    if (packet != nullptr) {
        if (start_time == timer::time_type{}) { start_time = timer::get(); }
        current_packet_time = timer::diff(timer::get(), start_time);
    }
    return packet;
}

template<typename F>
void repclient::do_sending_operation(F &operation) {
    if (client_mode != mode::LIVE && client_mode != mode::RECORD) {
        return;
    }

    bool was_empty;
    bool is_empty;
    {
        std::unique_lock<std::mutex> lock(send_mutex);
        was_empty = impl.get_send_buffer().is_empty();
        ++doing_send;
        bool sent = false;
        int retry = 5;
        while (!sent && retry > 0) {
            send_condition.wait(lock, [this] {
                return impl.get_send_buffer().has_space();
            });
            sent = operation(impl);
            retry--;
        }
        --doing_send;
        is_empty = impl.get_send_buffer().is_empty();
    }
    if (was_empty && !is_empty) {
        send_condition.notify_one();
    }
}

void repclient::send(const void *data, size_t length) {
    const auto op = [data, length](repclient_protocol &protocol) {
        return protocol.try_send(data, length);
    };
    do_sending_operation(op);
}

void repclient::authenticate_player_id(const uint64_t id) {
    const auto op = [id](repclient_protocol &protocol) {
        return protocol.try_authenticate_player_id(id);
    };
    do_sending_operation(op);
}

void repclient::authenticate_player_id_with_token(const uint64_t id, const std::array<unsigned char, 32>& token) {
    const auto op = [id, token](repclient_protocol &protocol) {
        return protocol.try_authenticate_player_id_with_token(id, token);
    };
    do_sending_operation(op);
}

void repclient::send_authentication_payload(const void *data, size_t length) {
    const auto op = [data, length](repclient_protocol &protocol) {
        return protocol.try_send_authentication_payload(data, length);
    };
    do_sending_operation(op);
}


void repclient::setup_threads() {
    assert(socket != INVALID_SOCKET);
    send_thread = std::thread(do_sends, std::ref(*this));
    receive_thread = std::thread(do_receives, std::ref(*this));
}

repclient::~repclient() {
    const bool using_socket = client_mode == mode::LIVE ||
        client_mode == mode::RECORD;

    const bool using_recfile = client_mode == mode::RECORD ||
        client_mode == mode::PLAYBACK;

    if (using_recfile) {
        close_file(rec_file);
    }

    if (using_socket) {
        alive = false;
        send_condition.notify_one();
        receive_condition.notify_one();
        send_thread.join();
        receive_thread.join();
        tcp::close_socket(socket);

#if defined(_WIN32)
        const auto result = WSACleanup();
        if (result != 0) {
            AETHER_LOG(ERROR)("Unable to close Windows sockets");
        }
#endif
    }
}
