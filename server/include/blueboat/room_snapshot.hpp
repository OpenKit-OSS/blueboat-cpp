#pragma once

#include <cstdint>
#include <string>

#include "blueboat/common/json.hpp"
#include "blueboat/simple_client.hpp"

namespace blueboat {

struct RoomSnapshot {
  std::string id;
  std::string type;
  SimpleClient owner;
  Value metadata = Value::object();
  std::int64_t created_at = 0;
};

inline void to_json(Value &j, const SimpleClient &c) { j = Value{{"id", c.id}, {"sessionId", c.session_id}, {"origin", c.origin}, {"ip", c.ip}}; }

inline void from_json(const Value &j, SimpleClient &c) {
  c.id = j.value("id", "");
  c.session_id = j.value("sessionId", "");
  c.origin = j.value("origin", "");
  c.ip = j.value("ip", "");
}

inline void to_json(Value &j, const RoomSnapshot &s) { j = Value{{"id", s.id}, {"type", s.type}, {"owner", s.owner}, {"metadata", s.metadata}, {"createdAt", s.created_at}}; }

inline void from_json(const Value &j, RoomSnapshot &s) {
  s.id = j.value("id", "");
  s.type = j.value("type", "");
  if (j.contains("owner")) {
    s.owner = j.at("owner").get<SimpleClient>();
  }
  s.metadata = j.value("metadata", Value::object());
  s.created_at = j.value("createdAt", static_cast<std::int64_t>(0));
}

} // namespace blueboat
