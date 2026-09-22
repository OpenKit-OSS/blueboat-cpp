#pragma once

#include <string>

#include "blueboat/common/callback.hpp"
#include "blueboat/common/json.hpp"

namespace blueboat_client {

class Client;

class Room {
public:
  std::string id;
  bool joined = false;
  blueboat::Value initial_join_options = blueboat::Value::object();

  blueboat::Callback<> on_join_attempt;
  blueboat::Callback<std::string> on_create; // roomId
  blueboat::Callback<> on_join;
  blueboat::Callback<blueboat::Value> on_join_error;
  blueboat::Callback<std::string, blueboat::Value> on_message; // key, data
  blueboat::Callback<> on_leave;

  void send(const std::string &key, const blueboat::Value &data = blueboat::Value());

private:
  friend class Client;
  Room(Client *client, blueboat::Value options, std::string room_id = "");

  Client *client_;
};

} // namespace blueboat_client
