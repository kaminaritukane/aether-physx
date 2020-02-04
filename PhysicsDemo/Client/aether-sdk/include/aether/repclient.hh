#pragma once

// Support checks
#if defined(__linux__) || defined(_WIN32)
#else
#error unknown platform
#endif
#if defined(__GNUC__) || defined(_MSC_VER)
#else
#error unknown compiler
#endif

#include <aether/common/hadean_platform.hh>
#include <aether/common/container/ring_buffer.hh>
#include <aether/common/tcp.hh>
#include <aether/common/timer.hh>
#include <aether/common/client_message.hh>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <array>

namespace aether {

namespace repclient {

namespace detail {

struct msgbuf {
    size_t pos = 0;
    size_t len = 0; // actually 'endpos'
    std::vector<uint8_t> buf;

    void reserve(size_t bytes);
    void *consume_message(size_t *length);
};

}

}

}

class repclient_protocol {
public:
    using stream_id = uint64_t;

    aether::container::ring_buffer<char>& get_send_buffer();
    aether::container::ring_buffer<char>& get_receive_buffer();

    void *tick(stream_id *, uint64_t *msg_size);
    bool try_send(const void *data, size_t length);
    bool try_send_authentication_payload(const void *data, size_t length);
    bool try_authenticate_player_id(uint64_t id);
    bool try_authenticate_player_id_with_token(uint64_t id, const std::array<unsigned char, 32>& token);

private:
    using msgbuf = aether::repclient::detail::msgbuf;
    static constexpr size_t SEND_BUFFER_SIZE = 4 * 1024;
    static constexpr size_t RECEIVE_BUFFER_SIZE = 512 * 1024;
    size_t try_fill_buf_simulation(void *buf, size_t wanted);

    HADEAN_PACK(struct multiplexer_header {
        stream_id id;
        uint64_t len;
    }) cur_header = {0, 0};

    bool try_send_message(aether::message_encoding::client::message_type ty, const void *data, size_t len);
    size_t cur_header_got = 0;
    std::unordered_map<stream_id, msgbuf> msgbufs;
    aether::container::ring_buffer<char> receive_buffer{RECEIVE_BUFFER_SIZE};
    aether::container::ring_buffer<char> send_buffer{SEND_BUFFER_SIZE};
};

class repclient {
public:
    using stream_id = repclient_protocol::stream_id;
    using duration_type = std::chrono::duration<double>;

private:
    static constexpr int MAX_SHUTDOWN_TIME_SECONDS = 1; // WARNING: 0 -> busy looping on select()
    static constexpr const char *DEFAULT_DUMP_FILE = "aether_recording.dump";
    enum class mode { LIVE, RECORD, PLAYBACK };
    static void do_sends(repclient&);
    static void do_receives(repclient&);
    static aether::tcp::os_socket construct_socket(const char *host, const char *port);

    mode client_mode;
    aether::tcp::os_socket socket = INVALID_SOCKET;
    repclient_protocol impl;
    std::atomic_bool alive{true};
    size_t doing_send = 0;
    std::thread send_thread;
    std::thread receive_thread;
    std::mutex send_mutex;
    std::mutex receive_mutex;
    std::condition_variable send_condition;
    std::condition_variable receive_condition;
    FILE *rec_file = nullptr;
    aether::timer::time_type start_time {};
    float current_packet_time = 0.0;
    aether::repclient::detail::msgbuf playback_buf;

    void setup_threads();
    void *tick_live(stream_id *id, uint64_t *msg_size);
    void *tick_record(stream_id *id, uint64_t *msg_size);
    void *tick_playback(stream_id *id, uint64_t *msg_size);
    template<typename F> void do_sending_operation(F &op);

public:
    repclient(const char *host, const char *port);
    repclient(const char *host, const char *port, const char *path);
    repclient(const char *path);
    repclient(const repclient &) = delete;
    repclient &operator=(const repclient &) = delete;
    void *tick(size_t *msg_size);
    void send(const void *data, size_t length);
    void authenticate_player_id(const uint64_t id);
    void authenticate_player_id_with_token(const uint64_t id, const std::array<unsigned char, 32>& token);
    void send_authentication_payload(const void *data, size_t len);
    duration_type last_packet_time() const;
    ~repclient();
};
