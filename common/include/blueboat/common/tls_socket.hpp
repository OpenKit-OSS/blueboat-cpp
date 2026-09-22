#pragma once

#include <memory>
#include <string>

#include "blueboat/common/socket.hpp"

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;

namespace blueboat {

class TlsContext {
public:
  static std::shared_ptr<TlsContext> create_server(const std::string &cert_file, const std::string &key_file);
  ~TlsContext();

  TlsContext(const TlsContext &) = delete;
  TlsContext &operator=(const TlsContext &) = delete;

  SSL_CTX *native() const { return ctx_; }

private:
  explicit TlsContext(SSL_CTX *ctx) : ctx_(ctx) {}

  SSL_CTX *ctx_;
};

class TlsSocket : public Socket {
public:
  TlsSocket(int fd, std::shared_ptr<TlsContext> ctx);
  ~TlsSocket() override;

  TlsSocket(const TlsSocket &) = delete;
  TlsSocket &operator=(const TlsSocket &) = delete;

  bool accept_server();

  long read(void *buf, std::size_t len) override;
  long write(const void *buf, std::size_t len) override;
  void shutdown_close() override;
  int raw_fd() const override { return fd_; }

private:
  int fd_;
  std::shared_ptr<TlsContext> ctx_;
  SSL *ssl_ = nullptr;
};

} // namespace blueboat
