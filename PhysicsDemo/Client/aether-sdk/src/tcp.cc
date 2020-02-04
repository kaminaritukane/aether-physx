#include "tcp_internal.hh"
#include <aether/common/hadean_platform.hh>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <aether/common/timer.hh>
#include <aether/common/logging.hh>

namespace aether {

namespace tcp {

void socket_set_nonblocking(const os_socket sockfd) {
#if defined(__linux__)
    const int flags = fcntl(sockfd, F_GETFL, 0);
    assert(flags != -1);
    const int ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    assert(ret == 0);
#elif defined(_WIN32)
    unsigned long i_mode = 1;
    const auto result = ioctlsocket(sockfd, FIONBIO, &i_mode);
    assert(result == NO_ERROR);
#endif
}

void close_socket(const os_socket sockfd) {
#if defined(__linux__)
    const auto err = close(sockfd);
#elif defined(_WIN32)
    const auto err = closesocket(sockfd);
#endif
    assert(err != SOCKET_ERROR);
}

template<typename T>
static void setsockopt_abort(int fd, int level, int optname, const T value, const char *msg) {
#if defined(__linux__)
    const auto opt_ptr = &value;
#elif defined(_WIN32)
    const auto opt_ptr = static_cast<const char*>(static_cast<const void*>(&value));
#endif

    const auto ret = setsockopt(fd, level, optname, opt_ptr, sizeof(value));
    if (ret != 0) {
        perror(msg);
        abort();
    }
}

std::pair<os_socket, int> initiate_connection(const char* host, const char* port, bool non_blocking) {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *servinfo;
    int rv = getaddrinfo(host, port, &hints, &servinfo);
    int error = 0;
    if (rv != 0) {
        AETHER_LOG(ERROR)("getaddrinfo:", gai_strerror(rv));
        return { INVALID_SOCKET, errno };
    }
    addrinfo *p = nullptr;
    os_socket sockfd = INVALID_SOCKET;
    for (p = servinfo; p != nullptr; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == INVALID_SOCKET) {
            error = errno;
            AETHER_LOG(ERROR)("socket:", std::strerror(errno));
            continue;
        } else {
            error = 0;
        }
        if (non_blocking) {
            socket_set_nonblocking(sockfd);
        }
        const auto connect_result = connect(sockfd, p->ai_addr, p->ai_addrlen);
        if (connect_result == SOCKET_ERROR) {
            error = errno;
            if (error == EINPROGRESS && non_blocking) { break; }
            AETHER_LOG(ERROR)("connect:", std::strerror(errno));
            close_socket(sockfd);
            sockfd = INVALID_SOCKET;
            continue;
        }
        break;
    }
    if (!p) {
        AETHER_LOG(ERROR)("failed to connect");
    }
    freeaddrinfo(servinfo);
    return { sockfd, error };
}

int set_important_socket_options(const os_socket sockfd) {
    // enable TCP NoDelay
    setsockopt_abort<int>(sockfd, IPPROTO_TCP, TCP_NODELAY, 1, "setsockopt(TCP_NODELAY) failed");

#if defined(__linux__)
    //enable the KeepAlive option
    setsockopt_abort<int>(sockfd, SOL_SOCKET,  SO_KEEPALIVE, 1, "setsockopt(SO_KEEPALIVE) failed");

    //seconds until we start sending probes
    setsockopt_abort<int>(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, 120, "setsockopt(TCP_KEEPIDLE) failed");

    //seconds between each probe
    setsockopt_abort<int>(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, 10, "setsockopt(TCP_KEEPINTVL) failed");

    //number of probes before we assume the connection is dead
    setsockopt_abort<int>(sockfd, IPPROTO_TCP, TCP_KEEPCNT, 6, "setsockopt(TCP_KEEPCNT) failed");
#endif

    return 0;
}

os_socket connect_to_host_port(const char* host, const char* port) {
    const auto [sockfd, error] = initiate_connection(host, port, false);
    if (sockfd != INVALID_SOCKET) {
        set_important_socket_options(sockfd);
    }
    return sockfd;
}

os_socket connect_to_host_port_with_timeout(const char* host, const char* port, int seconds) {
    using namespace std::chrono_literals;
    const auto end = timer::add(timer::get(), 1s * seconds);
    bool first_try = true;
    while (first_try || timer::diff(timer::get(), end) < 0) {
        const auto r = connect_to_host_port(host, port);
        if (r != INVALID_SOCKET) { return r; }
        first_try = false;
        std::this_thread::sleep_for(1s);
    }
    AETHER_LOG(ERROR)("timed out connecting to:", host, port);
    return INVALID_SOCKET;
}

}

}
