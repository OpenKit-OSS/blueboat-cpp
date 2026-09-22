#pragma once

#include <string>

namespace blueboat::tcp {

int listen_on(int port, int backlog = 128);

int accept_connection(int listen_fd);

int connect_to(const std::string &host, int port);

} // namespace blueboat::tcp
