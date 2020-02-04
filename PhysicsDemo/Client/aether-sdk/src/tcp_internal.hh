#pragma once
#include <aether/common/tcp.hh>
#include <utility>
#include <optional>

namespace aether {

namespace tcp {

std::pair<os_socket, int> initiate_connection(const char* host, const char* port, bool non_blocking);
int set_important_socket_options(const os_socket sockfd);

}

}
