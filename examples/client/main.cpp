#include <iostream>
#include <mutex>
#include <string>

#include "blueboat_client/client.hpp"

int main(int argc, char **argv) {
  std::string host = argc > 1 ? argv[1] : "localhost";
  int port = argc > 2 ? std::stoi(argv[2]) : 4000;
  std::string existing_room_id = argc > 3 ? argv[3] : "";

  blueboat_client::Client client(host, port);

  std::mutex room_mutex;
  std::shared_ptr<blueboat_client::Room> room;

  client.on_connect_error.add([](const blueboat::Value &e) { std::cerr << "[connect error] " << e.dump() << std::endl; });
  client.on_disconnect.add([](const std::string &reason) { std::cerr << "[disconnected] " << reason << std::endl; });
  client.on_reconnect.add([](int attempt) { std::cerr << "[reconnected after " << attempt << " attempt(s)]" << std::endl; });

  client.on_connect.add([&] {
    std::cout << "[connected] client id: " << client.id << std::endl;

    auto new_room = existing_room_id.empty() ? client.create_room("Chat") : client.join_room(existing_room_id);
    new_room->on_create.add([](const std::string &room_id) { std::cout << "[created room] " << room_id << std::endl; });
    new_room->on_join_error.add([](const blueboat::Value &e) { std::cerr << "[join error] " << e.dump() << std::endl; });
    new_room->on_join.add([] { std::cout << "[joined chat room]" << std::endl; });
    new_room->on_message.add([](const std::string &key, const blueboat::Value &data) {
      if (key == "MESSAGES") {
        for (auto &message : data) {
          std::cout << message.value("senderId", "?") << ": " << message.value("message", "") << std::endl;
        }
      } else if (key == "MESSAGE") {
        std::cout << data.value("senderId", "?") << ": " << data.value("message", "") << std::endl;
      }
    });

    std::lock_guard<std::mutex> lock(room_mutex);
    room = new_room;
  });

  std::cout << "Type a message and press enter to chat, or 'quit' to exit." << std::endl;
  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "quit") {
      break;
    }
    std::shared_ptr<blueboat_client::Room> current;
    {
      std::lock_guard<std::mutex> lock(room_mutex);
      current = room;
    }
    if (current && current->joined) {
      current->send("CHAT", line);
    } else {
      std::cerr << "[not joined yet]" << std::endl;
    }
  }

  client.disconnect();
  return 0;
}
