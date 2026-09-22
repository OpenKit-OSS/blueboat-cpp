#include "blueboat/common/tls_socket.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <mutex>
#include <stdexcept>

namespace blueboat {

namespace {
void ensure_openssl_init() {
  static std::once_flag once;
  std::call_once(once, [] {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
  });
}
} // namespace

std::shared_ptr<TlsContext> TlsContext::create_server(const std::string &cert_file, const std::string &key_file) {
  ensure_openssl_init();

  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
  if (!ctx) {
    throw std::runtime_error("blueboat: failed to create SSL_CTX");
  }
  if (SSL_CTX_use_certificate_chain_file(ctx, cert_file.c_str()) <= 0) {
    SSL_CTX_free(ctx);
    throw std::runtime_error("blueboat: failed to load TLS certificate: " + cert_file);
  }
  if (SSL_CTX_use_PrivateKey_file(ctx, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
    SSL_CTX_free(ctx);
    throw std::runtime_error("blueboat: failed to load TLS private key: " + key_file);
  }
  if (!SSL_CTX_check_private_key(ctx)) {
    SSL_CTX_free(ctx);
    throw std::runtime_error("blueboat: TLS private key does not match certificate: " + key_file);
  }

  return std::shared_ptr<TlsContext>(new TlsContext(ctx));
}

TlsContext::~TlsContext() {
  if (ctx_) {
    SSL_CTX_free(ctx_);
  }
}

TlsSocket::TlsSocket(int fd, std::shared_ptr<TlsContext> ctx) : fd_(fd), ctx_(std::move(ctx)) {
  ssl_ = SSL_new(ctx_->native());
  SSL_set_fd(ssl_, fd_);
}

TlsSocket::~TlsSocket() {
  if (ssl_) {
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
  }
  if (fd_ >= 0) {
    ::close(fd_);
  }
}

bool TlsSocket::accept_server() { return SSL_accept(ssl_) == 1; }

long TlsSocket::read(void *buf, std::size_t len) {
  int n = SSL_read(ssl_, buf, static_cast<int>(len));
  if (n > 0) {
    return n;
  }
  int err = SSL_get_error(ssl_, n);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    errno = EAGAIN;
    return -1;
  }
  if (err == SSL_ERROR_ZERO_RETURN) {
    return 0;
  }
  return -1;
}

long TlsSocket::write(const void *buf, std::size_t len) {
  int n = SSL_write(ssl_, buf, static_cast<int>(len));
  if (n > 0) {
    return n;
  }
  int err = SSL_get_error(ssl_, n);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    errno = EAGAIN;
    return -1;
  }
  return -1;
}

void TlsSocket::shutdown_close() {
  if (fd_ >= 0) {
    ::shutdown(fd_, SHUT_RDWR);
  }
}

} // namespace blueboat
