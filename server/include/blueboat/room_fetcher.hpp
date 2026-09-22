#pragma once

#include <vector>

#include "blueboat/room_snapshot.hpp"
#include "blueboat/storage.hpp"

namespace blueboat {

class RoomFetcher {
public:
  explicit RoomFetcher(Storage &storage) : storage_(storage) {}

  std::vector<std::string> get_list_of_rooms();
  std::vector<RoomSnapshot> get_list_of_rooms_with_data();

  RoomSnapshot find_room_by_id(const std::string &room_id);

  void set_room_metadata(const std::string &room_id, const Value &new_metadata);
  void add_room(const RoomSnapshot &room);
  void remove_room(const std::string &room_id);

private:
  Storage &storage_;
};

} // namespace blueboat
