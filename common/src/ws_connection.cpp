#include "blueboat/common/ws_connection.hpp"

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <random>
#include <sys/socket.h>

namespace blueboat {

WsConnection::WsConnection(int fd, bool is_client) : fd_(fd), is_client_(is_client) {
  wslay_event_callbacks callbacks{};
  callbacks.recv_callback = &WsConnection::recv_cb;
  callbacks.send_callback = &WsConnection::send_cb;
  callbacks.genmask_callback = &WsConnection::genmask_cb;
  callbacks.on_msg_recv_callback = &WsConnection::on_msg_recv_cb;

  if (is_client_) {
    wslay_event_context_client_init(&ctx_, &callbacks, this);
  } else {
    wslay_event_context_server_init(&ctx_, &callbacks, this);
  }
}

WsConnection::~WsConnection() {
  if (ctx_) {
    wslay_event_context_free(ctx_);
    ctx_ = nullptr;
  }
  if (fd_ >= 0) {
    ::shutdown(fd_, SHUT_RDWR);
    ::close(fd_);
    fd_ = -1;
  }
}

bool WsConnection::send_text(const std::string &text) {
  std::lock_guard<std::recursive_mutex> lock(io_mutex_);
  if (closed_) {
    return false;
  }
  wslay_event_msg msg{WSLAY_TEXT_FRAME, reinterpret_cast<const uint8_t *>(text.data()), text.size()};
  if (wslay_event_queue_msg(ctx_, &msg) != 0) {
    return false;
  }
  wslay_event_send(ctx_);
  return true;
}

bool WsConnection::send_binary(const std::string &bytes) {
  std::lock_guard<std::recursive_mutex> lock(io_mutex_);
  if (closed_) {
    return false;
  }
  wslay_event_msg msg{WSLAY_BINARY_FRAME, reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()};
  if (wslay_event_queue_msg(ctx_, &msg) != 0) {
    return false;
  }
  wslay_event_send(ctx_);
  return true;
}

void WsConnection::close(uint16_t code) {
  std::lock_guard<std::recursive_mutex> lock(io_mutex_);
  if (!closed_) {
    wslay_event_queue_close(ctx_, code, nullptr, 0);
    wslay_event_send(ctx_);
  }
  if (fd_ >= 0) {
    ::shutdown(fd_, SHUT_RDWR);
  }
}

void WsConnection::run_recv_loop(std::function<void()> on_idle) {
  if (on_idle) {
    timeval tv{1, 0};
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  std::vector<char> buf(65536);
  while (true) {
    ssize_t n = ::recv(fd_, buf.data(), buf.size(), 0);
    if (n < 0) {
      if (on_idle && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        on_idle();
        continue;
      }
      break;
    }
    if (n == 0) {
      break;
    }

    std::vector<std::pair<bool, std::string>> dispatch;
    bool stop = false;
    {
      std::lock_guard<std::recursive_mutex> lock(io_mutex_);
      if (closed_) {
        stop = true;
      } else {
        pending_recv_.append(buf.data(), static_cast<std::size_t>(n));
        int rv = wslay_event_recv(ctx_);
        wslay_event_send(ctx_);

        for (auto &entry : pending_messages_) {
          if (entry.first == WSLAY_TEXT_FRAME) {
            dispatch.emplace_back(false, std::move(entry.second));
          } else if (entry.first == WSLAY_BINARY_FRAME) {
            dispatch.emplace_back(true, std::move(entry.second));
          }
        }
        pending_messages_.clear();

        if (rv != 0 || !wslay_event_get_read_enabled(ctx_)) {
          stop = true;
        }
      }
    }

    for (auto &[is_binary, message] : dispatch) {
      if (on_message_) {
        on_message_(is_binary, message);
      }
    }
    if (stop) {
      break;
    }
  }

  {
    std::lock_guard<std::recursive_mutex> lock(io_mutex_);
    closed_ = true;
  }
  if (on_close_) {
    on_close_();
  }
}

ssize_t WsConnection::recv_cb(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, int, void *user_data) {
  auto *self = static_cast<WsConnection *>(user_data);
  if (self->pending_recv_.empty()) {
    wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
    return -1;
  }
  std::size_t take = std::min(len, self->pending_recv_.size());
  std::memcpy(buf, self->pending_recv_.data(), take);
  self->pending_recv_.erase(0, take);
  return static_cast<ssize_t>(take);
}

ssize_t WsConnection::send_cb(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int, void *user_data) {
  auto *self = static_cast<WsConnection *>(user_data);
  ssize_t sent = ::send(self->fd_, data, len, MSG_NOSIGNAL);
  if (sent <= 0) {
    wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
    return -1;
  }
  return sent;
}

int WsConnection::genmask_cb(wslay_event_context_ptr, uint8_t *buf, size_t len, void *) {
  static thread_local std::random_device rd;
  for (std::size_t i = 0; i < len; i++) {
    buf[i] = static_cast<uint8_t>(rd() & 0xFF);
  }
  return 0;
}

void WsConnection::on_msg_recv_cb(wslay_event_context_ptr, const wslay_event_on_msg_recv_arg *arg, void *user_data) {
  auto *self = static_cast<WsConnection *>(user_data);
  self->pending_messages_.emplace_back(arg->opcode, std::string(reinterpret_cast<const char *>(arg->msg), arg->msg_length));
}

} // namespace blueboat
