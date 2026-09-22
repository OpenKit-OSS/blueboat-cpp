#pragma once

#include <wslay/wslay.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "blueboat/common/socket.hpp"

namespace blueboat {

class WsConnection {
public:
  using CloseHandler = std::function<void()>;

  WsConnection(std::unique_ptr<Socket> socket, bool is_client);
  ~WsConnection();

  WsConnection(const WsConnection &) = delete;
  WsConnection &operator=(const WsConnection &) = delete;

  using RawMessageHandler = std::function<void(bool is_binary, const std::string &payload)>;

  void set_message_handler(RawMessageHandler handler) { on_message_ = std::move(handler); }
  void set_close_handler(CloseHandler handler) { on_close_ = std::move(handler); }

  bool send_text(const std::string &text);
  bool send_binary(const std::string &bytes);

  void close(uint16_t code = 1000);

  void run_recv_loop(std::function<void()> on_idle = nullptr);

  int fd() const { return socket_->raw_fd(); }

private:
  static ssize_t recv_cb(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, int flags, void *user_data);
  static ssize_t send_cb(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data);
  static int genmask_cb(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data);
  static void on_msg_recv_cb(wslay_event_context_ptr ctx, const wslay_event_on_msg_recv_arg *arg, void *user_data);

  std::unique_ptr<Socket> socket_;
  bool is_client_;
  wslay_event_context_ptr ctx_ = nullptr;
  std::recursive_mutex io_mutex_;
  std::string pending_recv_;
  std::vector<std::pair<uint8_t, std::string>> pending_messages_;
  bool closed_ = false;

  RawMessageHandler on_message_;
  CloseHandler on_close_;
};

} // namespace blueboat
