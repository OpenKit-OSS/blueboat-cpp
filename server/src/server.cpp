#include "blueboat/server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/evp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>

#include "blueboat/common/http.hpp"
#include "blueboat/common/protocol.hpp"
#include "blueboat/common/random_id.hpp"
#include "blueboat/common/sio_protocol.hpp"
#include "blueboat/common/socket.hpp"
#include "blueboat/common/tcp.hpp"
#include "blueboat/common/tls_socket.hpp"
#include "blueboat/common/ws_connection.hpp"
#include "blueboat/common/ws_handshake.hpp"
#include "blueboat/game_lock.hpp"

namespace blueboat {

namespace {

std::string to_lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool header_contains_token(const std::string &value, const std::string &token) { return to_lower(value).find(to_lower(token)) != std::string::npos; }

std::string peer_ip(int fd) {
  sockaddr_storage addr{};
  socklen_t len = sizeof(addr);
  if (::getpeername(fd, reinterpret_cast<sockaddr *>(&addr), &len) != 0) {
    return "";
  }
  char buf[INET6_ADDRSTRLEN] = {0};
  if (addr.ss_family == AF_INET) {
    auto *in4 = reinterpret_cast<sockaddr_in *>(&addr);
    ::inet_ntop(AF_INET, &in4->sin_addr, buf, sizeof(buf));
  } else if (addr.ss_family == AF_INET6) {
    auto *in6 = reinterpret_cast<sockaddr_in6 *>(&addr);
    ::inet_ntop(AF_INET6, &in6->sin6_addr, buf, sizeof(buf));
  }
  return std::string(buf);
}

std::int64_t now_millis() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }

std::int64_t steady_now_ms() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

constexpr int PING_INTERVAL_MS = 25000;
constexpr int PING_TIMEOUT_MS = 5000;

std::string base64_decode(const std::string &in) {
  if (in.empty() || in.size() % 4 != 0) {
    return "";
  }
  std::string out;
  out.resize(in.size());
  int written = EVP_DecodeBlock(reinterpret_cast<unsigned char *>(out.data()), reinterpret_cast<const unsigned char *>(in.data()), static_cast<int>(in.size()));
  if (written < 0) {
    return "";
  }
  int padding = 0;
  if (in.size() >= 1 && in[in.size() - 1] == '=') {
    padding++;
  }
  if (in.size() >= 2 && in[in.size() - 2] == '=') {
    padding++;
  }
  out.resize(static_cast<std::size_t>(written) - padding);
  return out;
}

} // namespace

Server::Server(ServerOptions options)
    : storage_(std::move(options.storage)), pubsub_(std::move(options.pubsub)), game_values_(*storage_), room_fetcher_(*storage_), admins_(std::move(options.admins)),
      custom_room_id_generator_(std::move(options.custom_room_id_generator)), on_dispose_(std::move(options.on_dispose)) {
  if (options.tls) {
    tls_ctx_ = TlsContext::create_server(options.tls->cert_file, options.tls->key_file);
  }
}

Server::~Server() {
  if (!shutting_down_.load()) {
    shutdown();
  }
}

void Server::register_room(const std::string &room_name, RoomFactory factory, Value room_options) {
  detail::GameLockGuard guard;
  if (registered_rooms_.count(room_name)) {
    return;
  }
  registered_rooms_[room_name] = RegisteredRoomType{std::move(factory), std::move(room_options)};
}

void Server::listen(int port) {
  listen_fd_ = tcp::listen_on(port);
  if (listen_fd_ < 0) {
    throw std::runtime_error("blueboat: failed to bind port " + std::to_string(port));
  }
  accept_thread_ = std::thread([this] { accept_loop(); });
}

void Server::run_forever() {
  std::unique_lock<std::mutex> lock(run_forever_mutex_);
  run_forever_cv_.wait(lock, [this] { return shutting_down_.load(); });
}

void Server::accept_loop() {
  while (!shutting_down_.load()) {
    int fd = tcp::accept_connection(listen_fd_);
    if (fd < 0) {
      continue;
    }

    std::unique_ptr<Socket> sock;
    if (tls_ctx_) {
      auto tls_sock = std::make_unique<TlsSocket>(fd, tls_ctx_);
      if (!tls_sock->accept_server()) {
        continue;
      }
      sock = std::move(tls_sock);
    } else {
      sock = std::make_unique<PlainSocket>(fd);
    }

    std::thread(&Server::handle_connection, this, std::move(sock)).detach();
  }
}

void Server::shutdown() {
  shutting_down_.store(true);
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }

  {
    detail::GameLockGuard guard;
    for (auto &[id, conn] : connections_) {
      conn.ws->close();
    }
    std::vector<std::string> room_ids;
    for (auto &[id, room] : managing_rooms_) {
      room_ids.push_back(id);
    }
    for (auto &id : room_ids) {
      auto it = managing_rooms_.find(id);
      if (it != managing_rooms_.end()) {
        it->second->dispose();
      }
    }
  }

  if (on_dispose_) {
    on_dispose_();
  }

  run_forever_cv_.notify_all();
}

int Server::get_room_count() {
  detail::GameLockGuard guard;
  return static_cast<int>(room_fetcher_.get_list_of_rooms().size());
}

std::vector<RoomSnapshot> Server::get_rooms() {
  detail::GameLockGuard guard;
  return room_fetcher_.get_list_of_rooms_with_data();
}

std::size_t Server::get_number_of_connected_clients() {
  detail::GameLockGuard guard;
  return connections_.size();
}

void Server::send_frame_to_client(const std::string &session_id, const std::string &event, const Value &data) {
  auto it = connections_.find(session_id);
  if (it == connections_.end()) {
    return;
  }
  it->second.ws->send_binary(sio::build_event_frame(event, data));
}

void Server::on_room_disposed(const std::string &room_id) { managing_rooms_.erase(room_id); }

Room *Server::create_new_room(const SimpleClient &client, const std::string &room_name, const Value &creator_options, const std::string &room_id_override) {
  auto type_it = registered_rooms_.find(room_name);
  if (type_it == registered_rooms_.end()) {
    throw std::runtime_error(room_name + " does not have a room handler");
  }

  auto existing_ids = room_fetcher_.get_list_of_rooms();
  std::string room_id;
  if (!room_id_override.empty()) {
    if (std::find(existing_ids.begin(), existing_ids.end(), room_id_override) != existing_ids.end()) {
      throw std::runtime_error("Room ID already in use: " + room_id_override);
    }
    room_id = room_id_override;
  } else {
    for (int i = 0; i < 3 && room_id.empty(); i++) {
      std::string candidate = custom_room_id_generator_ ? custom_room_id_generator_(room_name, type_it->second.options, creator_options) : random_id();
      if (std::find(existing_ids.begin(), existing_ids.end(), candidate) == existing_ids.end()) {
        room_id = candidate;
      }
    }
    if (room_id.empty()) {
      throw std::runtime_error("Failed to create room with unique ID");
    }
  }

  auto room = type_it->second.factory();

  RoomInitOptions init;
  init.room_id = room_id;
  init.room_type = room_name;
  init.storage = storage_.get();
  init.pubsub = pubsub_.get();
  init.room_fetcher = &room_fetcher_;
  init.game_values = &game_values_;
  init.owner = client;
  init.options = type_it->second.options;
  init.creator_options = creator_options;
  init.initial_game_values = game_values_.get_all();
  init.on_room_disposed = [this](const std::string &id) { on_room_disposed(id); };
  init.send_frame_to_client = [this](const std::string &sid, const std::string &event, const Value &data) { send_frame_to_client(sid, event, data); };

  room->initialize(std::move(init));

  RoomSnapshot snapshot{room_id, room_name, client, Value::object(), now_millis()};
  room_fetcher_.add_room(snapshot);

  Room *raw = room.get();
  managing_rooms_[room_id] = std::move(room);
  return raw;
}

std::string Server::create_room(const std::string &room_name, const std::string &room_id, const Value &creator_options) {
  detail::GameLockGuard guard;
  SimpleClient synthetic_owner{"", "", "", ""};
  Room *room = create_new_room(synthetic_owner, room_name, creator_options, room_id);
  return room->room_id;
}

void Server::handle_client_message(const SimpleClient &client, const std::string &event, const Value &data_in) {
  detail::GameLockGuard guard;
  const Value data = data_in.is_object() ? data_in : Value::object();

  if (event == protocol::ClientActions::createNewRoom) {
    std::string unique_request_id = data.value("uniqueRequestId", "");
    std::string room_name = data.value("type", "");
    if (room_name.empty() || unique_request_id.empty()) {
      return;
    }
    try {
      Room *room = create_new_room(client, room_name, data.value("options", Value::object()));
      send_frame_to_client(client.session_id, unique_request_id + "-create", room->room_id);
    } catch (const std::exception &e) {
      send_frame_to_client(client.session_id, unique_request_id + "-error", Value{{"message", std::string(e.what())}});
    }
    return;
  }

  if (event == protocol::ClientActions::joinRoom) {
    std::string room_id = data.value("roomId", "");
    if (room_id.empty()) {
      return;
    }
    try {
      room_fetcher_.find_room_by_id(room_id);
      pubsub_->publish(
        room_id,
        Value{{"action", protocol::ClientActions::joinRoom}, {"client", client}, {"data", Value{{"options", data.value("options", Value::object())}}}}
      );
    } catch (const std::exception &e) {
      send_frame_to_client(client.session_id, room_id + "-error", Value{{"message", std::string(e.what())}});
    }
    return;
  }

  if (event == protocol::ClientActions::sendMessage) {
    std::string room = data.value("room", "");
    if (room.empty() || !data.contains("key")) {
      return;
    }
    pubsub_->publish(
      room,
      Value{{"client", client}, {"action", protocol::ClientActions::sendMessage}, {"data", Value{{"key", data.at("key")}, {"data", data.value("data", Value())}}}}
    );
    return;
  }
}

void Server::handle_client_disconnect(const std::string &session_id) {
  detail::GameLockGuard guard;
  connections_.erase(session_id);
  pubsub_->publish(protocol::pubsub_listeners::PLAYER_LEFT, session_id);
}

void Server::handle_connection(std::unique_ptr<Socket> sock) {
  auto request = http::read_request(*sock);
  if (!request) {
    return;
  }

  if (request->path != "/blueboat" && request->path != "/blueboat/") {
    handle_admin_request(*sock, *request);
    return;
  }

  auto upgrade_it = request->headers.find("upgrade");
  auto key_it = request->headers.find("sec-websocket-key");
  if (request->method != "GET" || upgrade_it == request->headers.end() || !header_contains_token(upgrade_it->second, "websocket") || key_it == request->headers.end()) {
    http::write_response(*sock, 400, "Bad Request", "text/plain", "Expected a WebSocket upgrade request");
    return;
  }

  std::string accept = ws_handshake::compute_accept(key_it->second);
  std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: " +
                         accept + "\r\n\r\n";
  if (sock->write(response.data(), response.size()) < 0) {
    return;
  }

  auto query = http::parse_query(request->query);
  std::string id = query.count("id") && !query["id"].empty() ? query["id"] : random_id();
  std::string session_id = random_id();
  std::string origin = request->headers.count("origin") ? request->headers.at("origin") : "";

  SimpleClient client_info{id, session_id, origin, peer_ip(sock->raw_fd())};

  WsConnection ws(std::move(sock), /*is_client=*/false);
  {
    detail::GameLockGuard guard;
    connections_[session_id] = Connection{&ws, client_info};
  }

  ws.send_text(sio::build_open_frame(session_id, PING_INTERVAL_MS, PING_TIMEOUT_MS));
  ws.send_text(sio::build_connect_ack_frame());
  ws.send_binary(sio::build_event_frame(protocol::ServerActions::clientIdSet, client_info.id));

  ws.set_message_handler([this, client_info](bool is_binary, const std::string &payload) { handle_ws_payload(client_info, is_binary, payload); });
  ws.set_close_handler([this, session_id] { handle_client_disconnect(session_id); });

  auto last_activity_ms = std::make_shared<std::atomic<std::int64_t>>(steady_now_ms());
  {
    detail::GameLockGuard guard;
    last_activity_by_session_[session_id] = last_activity_ms;
  }

  ws.run_recv_loop([&] {
    if (steady_now_ms() - last_activity_ms->load() >= PING_INTERVAL_MS + PING_TIMEOUT_MS) {
      ws.close();
    }
  });

  {
    detail::GameLockGuard guard;
    last_activity_by_session_.erase(session_id);
  }
}

void Server::handle_ws_payload(const SimpleClient &client, bool is_binary, const std::string &payload) {
  {
    detail::GameLockGuard guard;
    auto it = last_activity_by_session_.find(client.session_id);
    if (it != last_activity_by_session_.end()) {
      it->second->store(steady_now_ms());
    }
  }

  auto frame = sio::parse_frame(is_binary, payload);
  switch (frame.kind) {
  case sio::FrameKind::Ping: {
    detail::GameLockGuard guard;
    auto it = connections_.find(client.session_id);
    if (it != connections_.end()) {
      it->second.ws->send_text(sio::PONG_PACKET);
    }
    return;
  }
  case sio::FrameKind::Disconnect:
    handle_client_disconnect(client.session_id);
    return;
  case sio::FrameKind::Event:
    handle_client_message(client, frame.event, frame.data);
    return;
  default:
    return;
  }
}

namespace {
constexpr const char *ADMIN_PREFIX = "/blueboat-panel";

std::string json_error(const std::string &message) { return Value{{"message", message}}.dump(); }
} // namespace

bool Server::check_basic_auth(const http::Request &request) const {
  if (admins_.empty()) {
    return true;
  }
  auto it = request.headers.find("authorization");
  if (it == request.headers.end() || it->second.rfind("Basic ", 0) != 0) {
    return false;
  }
  std::string decoded = base64_decode(it->second.substr(6));
  auto colon = decoded.find(':');
  if (colon == std::string::npos) {
    return false;
  }
  std::string user = decoded.substr(0, colon);
  std::string pass = decoded.substr(colon + 1);
  auto admin_it = admins_.find(user);
  return admin_it != admins_.end() && admin_it->second == pass;
}

void Server::handle_admin_request(Socket &sock, const http::Request &request) {
  if (request.path.rfind(ADMIN_PREFIX, 0) != 0) {
    http::write_response(sock, 404, "Not Found", "text/plain", "Not found");
    return;
  }

  if (!check_basic_auth(request)) {
    http::write_response(sock, 401, "Unauthorized", "text/plain", "Authentication required", {{"WWW-Authenticate", "Basic realm=\"blueboat\""}});
    return;
  }

  std::string route = request.path.substr(std::string(ADMIN_PREFIX).size());

  if (request.method == "GET" && (route.empty() || route == "/")) {
    http::write_response(sock, 200, "OK", "application/json", Value{{"message", "blueboat-cpp admin API. The bundled dashboard UI was not ported."}}.dump());
  } else if (request.method == "GET" && route == "/rooms") {
    Value rooms = Value::array();
    for (auto &room : get_rooms()) {
      rooms.push_back(room);
    }
    http::write_response(sock, 200, "OK", "application/json", rooms.dump());
  } else if (request.method == "GET" && route.rfind("/rooms/", 0) == 0) {
    std::string room_id = route.substr(std::string("/rooms/").size());

    Value result;
    bool got = false;
    std::mutex wait_mutex;
    std::condition_variable wait_cv;
    PubSubSubscription sub;
    {
      detail::GameLockGuard guard;
      sub = pubsub_->on(protocol::pubsub_listeners::REQUEST_INFO, [&](const Value &info) {
        result = info;
        {
          std::lock_guard<std::mutex> lock(wait_mutex);
          got = true;
        }
        wait_cv.notify_all();
      });
      pubsub_->publish(room_id, Value{{"action", protocol::pubsub_listeners::REQUEST_INFO}});
    }
    {
      std::unique_lock<std::mutex> lock(wait_mutex);
      wait_cv.wait_for(lock, std::chrono::seconds(5), [&] { return got; });
    }
    {
      detail::GameLockGuard guard;
      sub.unsubscribe();
    }
    if (!got) {
      http::write_response(sock, 404, "Not Found", "text/plain", "No room found");
    } else {
      http::write_response(sock, 200, "OK", "application/json", result.dump());
    }
  } else if (request.method == "GET" && route == "/gameValues") {
    detail::GameLockGuard guard;
    http::write_response(sock, 200, "OK", "application/json", game_values_.get_all().dump());
  } else if (request.method == "POST" && route == "/gameValues") {
    try {
      Value body = Value::parse(request.body);
      detail::GameLockGuard guard;
      game_values_.set_all(body);
      http::write_response(sock, 200, "OK", "text/plain", "OK");
    } catch (const std::exception &e) {
      http::write_response(sock, 500, "Internal Server Error", "application/json", json_error(e.what()));
    }
  } else if (request.method == "POST" && route == "/external-message") {
    try {
      Value body = Value::parse(request.body);
      if (!body.contains("key") || !body["key"].is_string()) {
        http::write_response(sock, 500, "Internal Server Error", "text/plain", "Key property required for external messages");
      } else if (!body.contains("room") || !body["room"].is_string()) {
        http::write_response(sock, 500, "Internal Server Error", "text/plain", "Room property required for external messages");
      } else {
        detail::GameLockGuard guard;
        pubsub_->publish(
          body["room"].get<std::string>(),
          Value{{"action", protocol::pubsub_listeners::EXTERNAL_MESSAGE}, {"data", Value{{"key", body["key"]}, {"data", body.value("data", Value())}}}}
        );
        http::write_response(sock, 200, "OK", "text/plain", "OK");
      }
    } catch (const std::exception &e) {
      http::write_response(sock, 500, "Internal Server Error", "application/json", json_error(e.what()));
    }
  } else {
    http::write_response(sock, 404, "Not Found", "text/plain", "Not found");
  }

}

} // namespace blueboat
