#pragma once

#include <functional>
#include <string>

#include "blueboat/common/json.hpp"

namespace blueboat {

class Client {
public:
  using SendFn = std::function<void(const std::string &session_id, const std::string &key, const Value &data)>;
  using RemoveFn = std::function<void(const std::string &session_id, bool intentional)>;

  Client(std::string id, std::string session_id, std::string origin, std::string ip, SendFn send_fn, RemoveFn remove_fn)
      : id(std::move(id)), session_id(std::move(session_id)), origin(std::move(origin)), ip(std::move(ip)), send_fn_(std::move(send_fn)), remove_fn_(std::move(remove_fn)) {}

  const std::string id;
  const std::string session_id;
  const std::string origin;
  const std::string ip;

  void send(const std::string &key, const Value &data = Value()) const { send_fn_(session_id, key, data); }
  void remove_from_room() const { remove_fn_(session_id, true); }

private:
  SendFn send_fn_;
  RemoveFn remove_fn_;
};

} // namespace blueboat
