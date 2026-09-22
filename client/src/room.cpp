#include "blueboat_client/room.hpp"

#include "blueboat_client/client.hpp"

namespace blueboat_client {

Room::Room(Client *client, blueboat::Value options, std::string room_id) : id(std::move(room_id)), client_(client) {
  if (!options.is_null()) {
    initial_join_options = std::move(options);
  }
}

void Room::send(const std::string &key, const blueboat::Value &data) { client_->send_room_message(id, key, data); }

} // namespace blueboat_client
