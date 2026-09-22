#include "blueboat/room_fetcher.hpp"

#include <stdexcept>

#include "blueboat/common/protocol.hpp"

namespace blueboat {

using protocol::storage_keys::ROOM_PREFIX;

std::vector<std::string> RoomFetcher::get_list_of_rooms() { return storage_.fetch_keys(ROOM_PREFIX); }

std::vector<RoomSnapshot> RoomFetcher::get_list_of_rooms_with_data() {
  std::vector<RoomSnapshot> rooms;
  for (auto &id : get_list_of_rooms()) {
    auto raw = storage_.get(std::string(ROOM_PREFIX) + id);
    if (!raw) {
      continue;
    }
    try {
      rooms.push_back(Value::parse(*raw).get<RoomSnapshot>());
    } catch (const std::exception &) {
      continue;
    }
  }
  return rooms;
}

RoomSnapshot RoomFetcher::find_room_by_id(const std::string &room_id) {
  auto raw = storage_.get(std::string(ROOM_PREFIX) + room_id);
  if (!raw) {
    throw std::runtime_error(std::string(ROOM_PREFIX) + room_id + " - No data received");
  }
  return Value::parse(*raw).get<RoomSnapshot>();
}

void RoomFetcher::set_room_metadata(const std::string &room_id, const Value &new_metadata) {
  RoomSnapshot room = find_room_by_id(room_id);
  room.metadata = new_metadata;
  storage_.set(std::string(ROOM_PREFIX) + room.id, Value(room).dump());
}

void RoomFetcher::add_room(const RoomSnapshot &room) { storage_.set(std::string(ROOM_PREFIX) + room.id, Value(room).dump()); }

void RoomFetcher::remove_room(const std::string &room_id) { storage_.remove(std::string(ROOM_PREFIX) + room_id); }

} // namespace blueboat
