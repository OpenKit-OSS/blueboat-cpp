#include "blueboat_client/client.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <random>

#include "blueboat/common/protocol.hpp"
#include "blueboat/common/random_id.hpp"
#include "blueboat/common/sio_protocol.hpp"
#include "blueboat/common/tcp.hpp"
#include "blueboat/common/ws_connection.hpp"
#include "blueboat/common/ws_handshake.hpp"

namespace blueboat_client {

using blueboat::Value;

namespace {

std::string read_response_headers(int fd) {
  std::string buf;
  char c;
  while (buf.size() < 64 * 1024) {
    ssize_t n = ::recv(fd, &c, 1, 0);
    if (n <= 0) {
      return "";
    }
    buf.push_back(c);
    if (buf.size() >= 4 && buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0) {
      return buf;
    }
  }
  return "";
}

std::chrono::milliseconds backoff_delay(int attempt) {
  static thread_local std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<int> dist(500, 1500);
  (void)attempt;
  return std::chrono::milliseconds(dist(rng));
}

} // namespace

Client::Client(std::string host, int port, std::string persisted_id) : host_(std::move(host)), port_(port) {
  id = std::move(persisted_id);
  manager_thread_ = std::thread(&Client::connection_manager_loop, this);
}

Client::~Client() { disconnect(); }

void Client::disconnect() {
  bool was_running = running_.exchange(false);
  if (!was_running) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ws_) {
      ws_->close();
    }
  }
  if (manager_thread_.joinable()) {
    manager_thread_.join();
  }
}

void Client::send_frame(const std::string &event, const Value &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ws_) {
    ws_->send_binary(blueboat::sio::build_event_frame(event, data));
  }
}

void Client::send_room_message(const std::string &room_id, const std::string &key, const Value &data) {
  send_frame(blueboat::protocol::ClientActions::sendMessage, Value{{"room", room_id}, {"key", key}, {"data", data}});
}

void Client::register_room_events(const std::shared_ptr<Room> &room) {
  std::lock_guard<std::mutex> lock(mutex_);
  event_handlers_[room->id + "-error"] = [room](const Value &error) { room->on_join_error.call(error); };
  event_handlers_[std::string("message-") + room->id] = [room](const Value &envelope) {
    if (!envelope.contains("key")) {
      return;
    }
    std::string key = envelope.at("key").get<std::string>();
    Value data = envelope.value("data", Value());
    if (key == blueboat::protocol::ServerActions::joinedRoom) {
      room->joined = true;
      room->on_join.call();
      return;
    }
    if (key == blueboat::protocol::ServerActions::removedFromRoom) {
      room->on_leave.call();
      room->joined = false;
      return;
    }
    room->on_message.call(key, data);
  };
}

std::shared_ptr<Room> Client::join_room_shared(const std::string &room_id, const Value &options, std::shared_ptr<Room> existing) {
  std::shared_ptr<Room> room = existing;
  if (!room) {
    room = std::shared_ptr<Room>(new Room(this, options, room_id));
    register_room_events(room);
  }

  room->on_join_attempt.call();
  if (!room->joined) {
    send_frame(blueboat::protocol::ClientActions::joinRoom, Value{{"roomId", room_id}, {"options", options}});
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    bool exists = std::any_of(rooms_.begin(), rooms_.end(), [&](const std::shared_ptr<Room> &r) { return r->id == room_id; });
    if (!exists) {
      rooms_.push_back(room);
    }
  }
  return room;
}

std::shared_ptr<Room> Client::create_room(const std::string &room_name, const Value &options) {
  std::string unique_request_id = blueboat::random_id();
  auto room = std::shared_ptr<Room>(new Room(this, options));

  {
    std::lock_guard<std::mutex> lock(mutex_);
    event_handlers_[unique_request_id + "-create"] = [this, room, options, unique_request_id](const Value &data) {
      std::string room_id = data.get<std::string>();
      room->id = room_id;
      register_room_events(room);
      {
        std::lock_guard<std::mutex> lock2(mutex_);
        event_handlers_.erase(unique_request_id + "-create");
        event_handlers_.erase(unique_request_id + "-error");
      }
      room->on_create.call(room_id);
      join_room_shared(room_id, options, room);
    };
    event_handlers_[unique_request_id + "-error"] = [this, room, unique_request_id](const Value &error) {
      room->on_join_error.call(error);
      std::lock_guard<std::mutex> lock2(mutex_);
      event_handlers_.erase(unique_request_id + "-create");
      event_handlers_.erase(unique_request_id + "-error");
    };
  }

  send_frame(blueboat::protocol::ClientActions::createNewRoom, Value{{"type", room_name}, {"options", options}, {"uniqueRequestId", unique_request_id}});
  return room;
}

std::shared_ptr<Room> Client::join_room(const std::string &room_id, const Value &options) { return join_room_shared(room_id, options, nullptr); }

void Client::handle_client_id_set(const Value &new_id) {
  id = new_id.get<std::string>();
  if (on_id_saved_) {
    on_id_saved_(id);
  }

  std::vector<std::shared_ptr<Room>> rooms_snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    rooms_snapshot = rooms_;
  }
  for (auto &room : rooms_snapshot) {
    if (!room->id.empty()) {
      join_room_shared(room->id, room->initial_join_options, room);
    }
  }

  on_connect.call();
}

void Client::handle_ws_payload(bool is_binary, const std::string &payload) {
  auto frame = blueboat::sio::parse_frame(is_binary, payload);

  if (frame.kind == blueboat::sio::FrameKind::Open) {
    session_id = frame.sid;
    ping_interval_ms_ = frame.ping_interval_ms;
    return;
  }

  if (frame.kind != blueboat::sio::FrameKind::Event) {
    return;
  }

  if (frame.event == blueboat::protocol::ServerActions::clientIdSet) {
    handle_client_id_set(frame.data);
    return;
  }

  std::function<void(const Value &)> handler;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = event_handlers_.find(frame.event);
    if (it != event_handlers_.end()) {
      handler = it->second;
    }
  }
  if (handler) {
    handler(frame.data);
  }
}

void Client::connection_manager_loop() {
  int attempt = 0;
  while (running_.load()) {
    int fd = blueboat::tcp::connect_to(host_, port_);
    if (fd < 0) {
      on_connect_error.call(Value{{"message", "failed to connect to " + host_ + ":" + std::to_string(port_)}});
      attempt++;
      on_reconnect_attempt.call(attempt);
      std::this_thread::sleep_for(backoff_delay(attempt));
      continue;
    }

    std::string key = blueboat::ws_handshake::generate_client_key();
    std::string path = "/blueboat/?EIO=3&transport=websocket";
    if (!id.empty()) {
      path += "&id=" + id;
    }
    std::string request = "GET " + path +
                          " HTTP/1.1\r\n"
                          "Host: " +
                          host_ +
                          "\r\n"
                          "Upgrade: websocket\r\n"
                          "Connection: Upgrade\r\n"
                          "Sec-WebSocket-Key: " +
                          key +
                          "\r\n"
                          "Sec-WebSocket-Version: 13\r\n\r\n";
    if (::send(fd, request.data(), request.size(), MSG_NOSIGNAL) < 0) {
      ::close(fd);
      on_connect_error.call(Value{{"message", "failed to send handshake"}});
      attempt++;
      on_reconnect_attempt.call(attempt);
      std::this_thread::sleep_for(backoff_delay(attempt));
      continue;
    }

    std::string response_headers = read_response_headers(fd);
    if (response_headers.rfind("HTTP/1.1 101", 0) != 0 && response_headers.rfind("HTTP/1.0 101", 0) != 0) {
      ::close(fd);
      on_connect_error.call(Value{{"message", "server rejected the WebSocket upgrade"}});
      attempt++;
      on_reconnect_attempt.call(attempt);
      std::this_thread::sleep_for(backoff_delay(attempt));
      continue;
    }

    bool was_reconnect = attempt > 0;
    int successful_attempt = attempt;
    attempt = 0;

    ping_interval_ms_ = 25000;

    auto ws = std::make_unique<blueboat::WsConnection>(std::make_unique<blueboat::PlainSocket>(fd), /*is_client=*/true);
    ws->set_message_handler([this](bool is_binary, const std::string &payload) { handle_ws_payload(is_binary, payload); });
    ws->set_close_handler([] {});

    bool already_disconnecting = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      ws_ = std::move(ws);
      if (!running_.load()) {
        ws_->close();
        already_disconnecting = true;
      }
    }

    if (was_reconnect && !already_disconnecting) {
      on_reconnect.call(successful_attempt);
    }

    auto last_ping = std::chrono::steady_clock::now();
    ws_->run_recv_loop([this, &last_ping] {
      auto now = std::chrono::steady_clock::now();
      if (now - last_ping >= std::chrono::milliseconds(ping_interval_ms_)) {
        ws_->send_text(blueboat::sio::PING_PACKET);
        last_ping = now;
      }
    });

    {
      std::lock_guard<std::mutex> lock(mutex_);
      ws_.reset();
    }

    if (!running_.load()) {
      break;
    }

    on_disconnect.call("transport close");
    std::vector<std::shared_ptr<Room>> rooms_snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      rooms_snapshot = rooms_;
    }
    for (auto &room : rooms_snapshot) {
      room->joined = false;
      room->on_leave.call();
    }

    attempt++;
    on_reconnect_attempt.call(attempt);
    std::this_thread::sleep_for(backoff_delay(attempt));
  }
}

} // namespace blueboat_client
