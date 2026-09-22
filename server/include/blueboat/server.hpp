#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "blueboat/common/json.hpp"
#include "blueboat/game_values.hpp"
#include "blueboat/pubsub.hpp"
#include "blueboat/room.hpp"
#include "blueboat/room_fetcher.hpp"
#include "blueboat/room_snapshot.hpp"
#include "blueboat/storage.hpp"

namespace blueboat::http {
struct Request;
}

namespace blueboat {

class WsConnection;

struct ServerOptions {
  std::unique_ptr<Storage> storage;
  std::unique_ptr<PubSub> pubsub;
  std::map<std::string, std::string> admins;
  std::function<std::string(const std::string &room_name, const Value &room_options, const Value &creator_options)> custom_room_id_generator;
  std::function<void()> on_dispose;
};

using RoomFactory = std::function<std::unique_ptr<Room>()>;

class Server {
public:
  explicit Server(ServerOptions options);
  ~Server();

  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  void register_room(const std::string &room_name, RoomFactory factory, Value room_options = Value::object());

  void listen(int port);

  void run_forever();

  void shutdown();

  int get_room_count();
  std::vector<RoomSnapshot> get_rooms();
  std::size_t get_number_of_connected_clients();

  GameValues &game_values() { return game_values_; }

private:
  struct RegisteredRoomType {
    RoomFactory factory;
    Value options;
  };

  struct Connection {
    WsConnection *ws;
    SimpleClient info;
  };

  void accept_loop();
  void handle_connection(int fd);
  void handle_ws_payload(const SimpleClient &client, bool is_binary, const std::string &payload);
  void handle_client_message(const SimpleClient &client, const std::string &event, const Value &data);
  void handle_client_disconnect(const std::string &session_id);

  Room *create_new_room(const SimpleClient &client, const std::string &room_name, const Value &creator_options);
  void on_room_disposed(const std::string &room_id);
  void send_frame_to_client(const std::string &session_id, const std::string &event, const Value &data);

  void handle_admin_request(int fd, const http::Request &request);
  bool check_basic_auth(const http::Request &request) const;

  std::unique_ptr<Storage> storage_;
  std::unique_ptr<PubSub> pubsub_;
  GameValues game_values_;
  RoomFetcher room_fetcher_;
  std::map<std::string, std::string> admins_;
  std::function<std::string(const std::string &, const Value &, const Value &)> custom_room_id_generator_;
  std::function<void()> on_dispose_;

  std::unordered_map<std::string, RegisteredRoomType> registered_rooms_;
  std::unordered_map<std::string, std::unique_ptr<Room>> managing_rooms_;
  std::unordered_map<std::string, Connection> connections_;
  std::unordered_map<std::string, std::shared_ptr<std::atomic<std::int64_t>>> last_activity_by_session_;

  int listen_fd_ = -1;
  std::thread accept_thread_;
  std::atomic<bool> shutting_down_{false};

  std::mutex run_forever_mutex_;
  std::condition_variable run_forever_cv_;
};

} // namespace blueboat
