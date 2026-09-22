#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "blueboat/client.hpp"
#include "blueboat/common/callback.hpp"
#include "blueboat/common/json.hpp"
#include "blueboat/common/scheduler.hpp"
#include "blueboat/game_values.hpp"
#include "blueboat/pubsub.hpp"
#include "blueboat/room_fetcher.hpp"
#include "blueboat/simple_client.hpp"
#include "blueboat/storage.hpp"

namespace blueboat {

struct RoomInitOptions {
  std::string room_id;
  std::string room_type;
  Storage *storage = nullptr;
  PubSub *pubsub = nullptr;
  RoomFetcher *room_fetcher = nullptr;
  GameValues *game_values = nullptr;
  SimpleClient owner;
  Value options = Value::object();
  Value creator_options = Value::object();
  Value initial_game_values = Value::object();
  std::function<void(const std::string &)> on_room_disposed;
  std::function<void(const std::string &session_id, const std::string &event, const Value &data)> send_frame_to_client;
};

class Room {
public:
  virtual ~Room() = default;

  Value state = Value::object();
  Value initial_game_values = Value::object();
  std::string room_id;
  std::string room_type;
  Value options = Value::object();
  Value creator_options = Value::object();
  Value metadata = Value::object();
  SimpleClient owner;
  GameValues *game_values = nullptr;

  Callback<Client *> on_join_listeners;
  Callback<Client *, bool> on_leave_listeners;

  void initialize(RoomInitOptions init);

  virtual void on_create(const Value &options) {}
  virtual void can_client_join(const SimpleClient &client, const Value &options) {}
  virtual void on_join(Client &client, const Value &options) {}
  virtual void on_message(Client &client, const std::string &key, const Value &data) {}
  virtual void on_leave(Client &client, bool intentional) {}
  virtual void before_dispose() {}
  virtual void on_dispose() {}
  virtual void on_external_message(const std::string &key, const Value &data) {}

  void set_state(const Value &new_state) { state = new_state; }
  void broadcast(const std::string &key, const Value &data = Value());
  void set_metadata(const Value &new_metadata);
  void dispose();

  bool allow_reconnection(const Client &client, int seconds);

  const std::vector<std::unique_ptr<Client>> &clients() const { return clients_; }

private:
  void client_has_joined(Client &client, const Value &options);
  void add_client(const SimpleClient &prejoined, const Value &options);
  void remove_client(const std::string &session_id, bool intentional);
  void pub_sub_listener();

  Storage *storage_ = nullptr;
  PubSub *pubsub_ = nullptr;
  RoomFetcher *room_fetcher_ = nullptr;
  std::function<void(const std::string &)> on_room_disposed_;
  std::function<void(const std::string &session_id, const std::string &event, const Value &data)> send_frame_to_client_;

  std::vector<std::unique_ptr<Client>> clients_;
  bool disposing_ = false;

  PubSubSubscription game_message_sub_;
  PubSubSubscription player_left_sub_;
  TimerHandle auto_dispose_timer_;
};

} // namespace blueboat
