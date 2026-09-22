#include "blueboat/common/tcp.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace blueboat::tcp {

int listen_on(int port, int backlog) {
  int fd = ::socket(AF_INET6, SOCK_STREAM, 0);
  bool ipv6 = fd >= 0;
  if (fd < 0) {
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      return -1;
    }
  }

  int yes = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  int no = 0;
  if (ipv6) {
    ::setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
  }

  if (ipv6) {
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
      ::close(fd);
      return -1;
    }
  } else {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
      ::close(fd);
      return -1;
    }
  }

  if (::listen(fd, backlog) < 0) {
    ::close(fd);
    return -1;
  }
  return fd;
}

int accept_connection(int listen_fd) {
  int fd = ::accept(listen_fd, nullptr, nullptr);
  if (fd >= 0) {
    int yes = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  }
  return fd;
}

int connect_to(const std::string &host, int port) {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo *result = nullptr;
  std::string port_str = std::to_string(port);
  if (::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
    return -1;
  }

  int fd = -1;
  for (addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
    fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0) {
      continue;
    }
    if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    ::close(fd);
    fd = -1;
  }
  ::freeaddrinfo(result);

  if (fd >= 0) {
    int yes = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  }
  return fd;
}

} // namespace blueboat::tcp
