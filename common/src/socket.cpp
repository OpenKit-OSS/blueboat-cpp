#include "blueboat/common/socket.hpp"

#include <sys/socket.h>
#include <unistd.h>

namespace blueboat {

PlainSocket::~PlainSocket() {
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

long PlainSocket::read(void *buf, std::size_t len) { return ::recv(fd_, buf, len, 0); }

long PlainSocket::write(const void *buf, std::size_t len) { return ::send(fd_, buf, len, MSG_NOSIGNAL); }

void PlainSocket::shutdown_close() {
  if (fd_ >= 0) {
    ::shutdown(fd_, SHUT_RDWR);
  }
}

} // namespace blueboat
