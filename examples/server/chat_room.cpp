#include "chat_room.hpp"

using blueboat::Value;

void ChatRoom::on_create(const Value &) {
  std::string initial_message = initial_game_values.value("initialBotMessage", std::string("Welcome to the chat!"));
  set_state(Value{{"messages", Value::array({Value{{"message", initial_message}, {"senderId", "Botsy"}}})}});
}

void ChatRoom::on_join(blueboat::Client &client, const Value &) { catch_up(client); }

void ChatRoom::on_message(blueboat::Client &client, const std::string &key, const Value &data) {
  if (key.empty()) {
    return;
  }
  if (key == "LATENCY") {
    client.send("LATENCY", data);
  }
  if (key == "CHAT") {
    add_message(data.is_string() ? data.get<std::string>() : data.dump(), client.id);
  }
}

void ChatRoom::before_dispose() { broadcast("DISPOSED"); }

void ChatRoom::on_leave(blueboat::Client &client, bool) {
  bool reconnected = allow_reconnection(client, 30);
  if (reconnected) {
    return;
  }
  add_message(client.id + " has left the room", "Botsy");
}

void ChatRoom::add_message(const std::string &text, const std::string &sender_id) {
  Value message{{"message", text}, {"senderId", sender_id}};
  state["messages"].push_back(message);
  broadcast("MESSAGE", message);
}

void ChatRoom::catch_up(blueboat::Client &client) { client.send("MESSAGES", state["messages"]); }
