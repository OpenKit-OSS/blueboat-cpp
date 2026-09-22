#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "blueboat/common/callback.hpp"
#include "blueboat/common/json.hpp"
#include "blueboat_client/room.hpp"

namespace blueboat {
class WsConnection;
}

namespace blueboat_client {

class Client {
public:
  Client(std::string host, int port, std::string persisted_id = "");
  ~Client();

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  std::string id;
  std::string session_id;

  blueboat::Callback<> on_connect;
  blueboat::Callback<blueboat::Value> on_connect_error;
  blueboat::Callback<std::string> on_disconnect;
  blueboat::Callback<int> on_reconnect;
  blueboat::Callback<int> on_reconnect_attempt;

  void set_id_saved_handler(std::function<void(const std::string &)> handler) { on_id_saved_ = std::move(handler); }

  std::shared_ptr<Room> create_room(const std::string &room_name, const blueboat::Value &options = blueboat::Value::object());
  std::shared_ptr<Room> join_room(const std::string &room_id, const blueboat::Value &options = blueboat::Value::object());

  void disconnect();

private:
  friend class Room;

  void send_frame(const std::string &event, const blueboat::Value &data);
  void send_room_message(const std::string &room_id, const std::string &key, const blueboat::Value &data);

  std::shared_ptr<Room> join_room_shared(const std::string &room_id, const blueboat::Value &options, std::shared_ptr<Room> existing);
  void register_room_events(const std::shared_ptr<Room> &room);
  void handle_client_id_set(const blueboat::Value &new_id);
  void handle_ws_payload(bool is_binary, const std::string &payload);

  void connection_manager_loop();

  std::string host_;
  int port_;
  int ping_interval_ms_ = 25000;
  std::atomic<bool> running_{true};
  std::thread manager_thread_;

  std::mutex mutex_;
  std::unique_ptr<blueboat::WsConnection> ws_;
  std::vector<std::shared_ptr<Room>> rooms_;
  std::unordered_map<std::string, std::function<void(const blueboat::Value &)>> event_handlers_;
  std::function<void(const std::string &)> on_id_saved_;
};

} // namespace blueboat_client
