#include "blueboat/room.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>

#include "blueboat/common/protocol.hpp"
#include "blueboat/game_lock.hpp"

namespace blueboat {

namespace {
constexpr auto AUTO_DISPOSE_AFTER = std::chrono::hours(2) + std::chrono::minutes(30);
}

void Room::initialize(RoomInitOptions init) {
  room_id = init.room_id;
  room_type = init.room_type;
  storage_ = init.storage;
  pubsub_ = init.pubsub;
  room_fetcher_ = init.room_fetcher;
  game_values = init.game_values;
  owner = init.owner;
  options = init.options;
  creator_options = init.creator_options;
  initial_game_values = init.initial_game_values;
  on_room_disposed_ = std::move(init.on_room_disposed);
  send_frame_to_client_ = std::move(init.send_frame_to_client);

  try {
    on_create(options);
  } catch (...) {
    dispose();
    throw;
  }

  auto_dispose_timer_ = Scheduler::instance().set_timeout(std::chrono::duration_cast<std::chrono::milliseconds>(AUTO_DISPOSE_AFTER), [this] {
    detail::GameLockGuard guard;
    dispose();
  });

  pub_sub_listener();
}

void Room::broadcast(const std::string &key, const Value &data) {
  for (auto &client : clients_) {
    client->send(key, data);
  }
}

void Room::set_metadata(const Value &new_metadata) {
  if (room_fetcher_) {
    room_fetcher_->set_room_metadata(room_id, new_metadata);
  }
  metadata = new_metadata;
}

void Room::dispose() {
  if (disposing_) {
    return;
  }
  disposing_ = true;

  auto clients_snapshot = std::vector<std::string>{};
  clients_snapshot.reserve(clients_.size());
  for (auto &c : clients_) {
    clients_snapshot.push_back(c->session_id);
  }
  for (auto &session_id : clients_snapshot) {
    remove_client(session_id, true);
  }

  before_dispose();
  auto_dispose_timer_.clear();
  if (room_fetcher_) {
    room_fetcher_->remove_room(room_id);
  }
  game_message_sub_.unsubscribe();
  player_left_sub_.unsubscribe();
  on_dispose();
  if (on_room_disposed_) {
    on_room_disposed_(room_id);
  }
}

bool Room::allow_reconnection(const Client &client, int seconds) {
  if (disposing_) {
    return false;
  }

  std::string client_id = client.id;
  auto promise = std::make_shared<std::promise<bool>>();
  std::shared_future<bool> future(promise->get_future());
  auto resolved = std::make_shared<std::atomic<bool>>(false);
  auto join_sub = std::make_shared<Callback<Client *>::Subscription>();
  auto timeout = std::make_shared<TimerHandle>();

  *join_sub = on_join_listeners.add([this, promise, resolved, client_id, join_sub, timeout](Client *joined) {
    if (joined->id != client_id) {
      return;
    }
    if (!resolved->exchange(true)) {
      promise->set_value(true);
    }
    timeout->clear();
    join_sub->clear();
  });

  *timeout = Scheduler::instance().set_timeout(std::chrono::seconds(seconds), [this, promise, resolved, client_id, join_sub] {
    detail::GameLockGuard guard;
    bool reconnected = std::any_of(clients_.begin(), clients_.end(), [&](const std::unique_ptr<Client> &c) { return c->id == client_id; });
    if (!resolved->exchange(true)) {
      promise->set_value(reconnected);
    }
    join_sub->clear();
  });

  auto *lock = detail::current_lock();
  if (lock) {
    lock->unlock();
    bool result = future.get();
    lock->lock();
    return result;
  }
  return future.get();
}

void Room::add_client(const SimpleClient &prejoined, const Value &options_in) {
  clients_.push_back(
    std::make_unique<Client>(
      prejoined.id,
      prejoined.session_id,
      prejoined.origin,
      prejoined.ip,
      [this](const std::string &session_id, const std::string &key, const Value &data) {
        send_frame_to_client_(session_id, "message-" + room_id, Value{{"key", key}, {"data", data}});
      },
      [this](const std::string &session_id, bool intentional) { remove_client(session_id, intentional); }
    )
  );
  client_has_joined(*clients_.back(), options_in);
}

void Room::client_has_joined(Client &client, const Value &options_in) {
  if (!owner.id.empty() && client.id == owner.id) {
    owner.session_id = client.session_id;
  }
  client.send(protocol::ServerActions::joinedRoom);
  on_join_listeners.call(&client);
  on_join(client, options_in);
}

void Room::remove_client(const std::string &session_id, bool intentional) {
  auto it = std::find_if(clients_.begin(), clients_.end(), [&](const std::unique_ptr<Client> &c) { return c->session_id == session_id; });
  if (it == clients_.end()) {
    return;
  }
  std::unique_ptr<Client> client = std::move(*it);
  clients_.erase(it);

  client->send(protocol::ServerActions::removedFromRoom);
  on_leave_listeners.call(client.get(), intentional);
  on_leave(*client, intentional);

  if (clients_.empty()) {
    dispose();
  }
}

void Room::pub_sub_listener() {
  game_message_sub_ = pubsub_->on(room_id, [this](const Value &payload) {
    if (!payload.is_object() || !payload.contains("action")) {
      return;
    }
    std::string action = payload.at("action").get<std::string>();

    if (action == protocol::pubsub_listeners::REQUEST_INFO) {
      Value clients_json = Value::array();
      for (auto &c : clients_) {
        clients_json.push_back(Value{{"id", c->id}, {"sessionId", c->session_id}, {"origin", c->origin}, {"ip", c->ip}});
      }
      pubsub_->publish(protocol::pubsub_listeners::REQUEST_INFO, Value{{"clients", clients_json}, {"state", state}});
      return;
    }

    if (action == protocol::pubsub_listeners::EXTERNAL_MESSAGE) {
      Value data = payload.value("data", Value::object());
      if (data.contains("key")) {
        on_external_message(data.at("key").get<std::string>(), data.value("data", Value()));
      }
      return;
    }

    if (!payload.contains("client")) {
      return;
    }
    SimpleClient client = payload.at("client").get<SimpleClient>();

    if (action == protocol::ClientActions::joinRoom) {
      Value join_options = payload.value("data", Value::object()).value("options", Value::object());
      try {
        can_client_join(client, join_options);
        add_client(client, join_options);
      } catch (const std::exception &e) {
        send_frame_to_client_(client.session_id, room_id + "-error", Value{{"message", std::string(e.what())}});
      }
      return;
    }

    if (action == protocol::ClientActions::sendMessage) {
      auto it = std::find_if(clients_.begin(), clients_.end(), [&](const std::unique_ptr<Client> &c) { return c->session_id == client.session_id; });
      if (it == clients_.end()) {
        return;
      }
      Value data = payload.value("data", Value::object());
      on_message(**it, data.value("key", std::string()), data.value("data", Value()));
    }
  });

  player_left_sub_ = pubsub_->on(protocol::pubsub_listeners::PLAYER_LEFT, [this](const Value &payload) {
    std::string session_id = payload.get<std::string>();
    remove_client(session_id, false);
  });
}

} // namespace blueboat
