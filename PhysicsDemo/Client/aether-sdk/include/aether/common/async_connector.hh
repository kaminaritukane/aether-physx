#pragma once

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_map>
#include <fcntl.h>
#include <sys/types.h>

#include <aether/common/hadean_platform.hh>
#include <aether/common/timer.hh>

namespace aether {

namespace tcp {

struct connect_result {
    uint64_t token;
    int error = 0;
    int fd = -1;
};

class async_connector {
private:
    size_t next_id = 0;
    std::unordered_map<size_t, connect_result> connections;

public:
    async_connector() = default;
    void connect(uint64_t token, const char *host, const char *port);
    std::optional<connect_result> poll();
    ~async_connector();
};

}

}
