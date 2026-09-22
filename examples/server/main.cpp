#include <csignal>
#include <iostream>
#include <memory>

#include "blueboat/memory_pubsub.hpp"
#include "blueboat/memory_storage.hpp"
#include "blueboat/server.hpp"
#include "chat_room.hpp"

namespace {
std::unique_ptr<blueboat::Server> g_server;

void handle_signal(int) {
  if (g_server) {
    g_server->shutdown();
  }
}
} // namespace

int main() {
  blueboat::ServerOptions options;
  options.storage = std::make_unique<blueboat::MemoryStorage>();
  options.pubsub = std::make_unique<blueboat::MemoryPubSub>();
  options.admins = {{"blueboat", "pass"}};

  g_server = std::make_unique<blueboat::Server>(std::move(options));
  g_server->register_room("Chat", [] { return std::make_unique<ChatRoom>(); });

  std::signal(SIGINT, handle_signal);
  std::signal(SIGTERM, handle_signal);

  g_server->listen(4000);
  std::cout << "Server listening on port 4000" << std::endl;

  g_server->run_forever();
  return 0;
}
