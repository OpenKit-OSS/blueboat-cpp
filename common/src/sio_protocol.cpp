#include "blueboat/common/sio_protocol.hpp"

#include "blueboat/common/msgpack_codec.hpp"

namespace blueboat::sio {

namespace {
enum SioType { CONNECT = 0, DISCONNECT = 1, EVENT = 2, ACK = 3, ERROR_ = 4, BINARY_EVENT = 5, BINARY_ACK = 6 };
}

std::string build_open_frame(const std::string &sid, int ping_interval_ms, int ping_timeout_ms) {
  Value payload{{"sid", sid}, {"upgrades", Value::array()}, {"pingInterval", ping_interval_ms}, {"pingTimeout", ping_timeout_ms}};
  return std::string("0") + payload.dump();
}

std::string build_connect_ack_frame() { return std::string("4") + Value{{"type", CONNECT}, {"nsp", "/"}}.dump(); }

std::string build_disconnect_frame() { return std::string("4") + Value{{"type", DISCONNECT}, {"nsp", "/"}}.dump(); }

std::string build_event_frame(const std::string &event_name, const Value &data) {
  Value args = Value::array();
  args.push_back(event_name);
  if (!data.is_null()) {
    args.push_back(data);
  }
  Value packet{{"type", EVENT}, {"nsp", "/"}, {"data", args}};

  std::string encoded = encode_msgpack(packet);
  std::string frame;
  frame.reserve(1 + encoded.size());
  frame.push_back(static_cast<char>(4));
  frame += encoded;
  return frame;
}

ParsedFrame parse_frame(bool is_binary, const std::string &payload) {
  ParsedFrame result;
  if (payload.empty()) {
    return result;
  }

  if (is_binary) {
    if (static_cast<unsigned char>(payload[0]) != 4) {
      return result;
    }
    Value packet;
    try {
      packet = decode_msgpack(payload.substr(1));
    } catch (const std::exception &) {
      return result;
    }
    if (!packet.is_object()) {
      return result;
    }
    int type = packet.value("type", -1);
    if (type == EVENT || type == BINARY_EVENT) {
      Value args = packet.value("data", Value::array());
      if (args.is_array() && !args.empty() && args[0].is_string()) {
        result.kind = FrameKind::Event;
        result.event = args[0].get<std::string>();
        if (args.size() > 1) {
          result.data = args[1];
        }
      }
    }
    return result;
  }

  char type_char = payload[0];
  std::string rest = payload.substr(1);

  switch (type_char) {
  case '0':
    try {
      Value open = Value::parse(rest);
      result.kind = FrameKind::Open;
      result.sid = open.value("sid", "");
      result.ping_interval_ms = open.value("pingInterval", 25000);
      result.ping_timeout_ms = open.value("pingTimeout", 5000);
    } catch (const std::exception &) {
    }
    break;
  case '1':
    result.kind = FrameKind::Close;
    break;
  case '2':
    result.kind = FrameKind::Ping;
    break;
  case '3':
    result.kind = FrameKind::Pong;
    break;
  case '4':
    try {
      Value packet = Value::parse(rest);
      int type = packet.value("type", -1);
      if (type == CONNECT) {
        result.kind = FrameKind::Connect;
      } else if (type == DISCONNECT) {
        result.kind = FrameKind::Disconnect;
      } else if (type == ERROR_) {
        result.kind = FrameKind::Error;
      }
    } catch (const std::exception &) {
    }
    break;
  default:
    break;
  }

  return result;
}

} // namespace blueboat::sio
