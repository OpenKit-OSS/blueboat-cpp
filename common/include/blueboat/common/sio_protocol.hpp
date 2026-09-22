#pragma once

#include <string>

#include "blueboat/common/json.hpp"

namespace blueboat::sio {

inline constexpr const char *PING_PACKET = "2";
inline constexpr const char *PONG_PACKET = "3";
inline constexpr const char *CLOSE_PACKET = "1";

std::string build_open_frame(const std::string &sid, int ping_interval_ms, int ping_timeout_ms);

std::string build_connect_ack_frame();

std::string build_disconnect_frame();

std::string build_event_frame(const std::string &event_name, const Value &data = Value());

enum class FrameKind { Open, Connect, Disconnect, Event, Ping, Pong, Close, Error, Other };

struct ParsedFrame {
  FrameKind kind = FrameKind::Other;

  std::string sid;
  int ping_interval_ms = 25000;
  int ping_timeout_ms = 5000;

  std::string event;
  Value data;
};

ParsedFrame parse_frame(bool is_binary, const std::string &payload);

} // namespace blueboat::sio
