#pragma once

#include <cstddef>

namespace blueboat {

class Socket {
public:
  virtual ~Socket() = default;

  virtual long read(void *buf, std::size_t len) = 0;
  virtual long write(const void *buf, std::size_t len) = 0;

  virtual void shutdown_close() = 0;
  virtual int raw_fd() const = 0;
};

class PlainSocket : public Socket {
public:
  explicit PlainSocket(int fd) : fd_(fd) {}
  ~PlainSocket() override;

  PlainSocket(const PlainSocket &) = delete;
  PlainSocket &operator=(const PlainSocket &) = delete;

  long read(void *buf, std::size_t len) override;
  long write(const void *buf, std::size_t len) override;
  void shutdown_close() override;
  int raw_fd() const override { return fd_; }

private:
  int fd_;
};

} // namespace blueboat
