#pragma once

namespace blueboat::protocol {

struct ClientActions {
  static constexpr const char *createNewRoom = "blueboat_CREATE_NEW_ROOM";
  static constexpr const char *joinRoom = "blueboat_JOIN_ROOM";
  static constexpr const char *sendMessage = "blueboat_SEND_MESSAGE";
  static constexpr const char *listen = "blueboat_LISTEN_STATE";
  static constexpr const char *requestAvailableRooms = "blueboat_AVAILABLE_ROOMS";
};

struct ServerActions {
  static constexpr const char *clientIdSet = "CLIENT_ID_SET";
  static constexpr const char *joinedRoom = "blueboat_JOINED_ROOM";
  static constexpr const char *statePatch = "STATE_PATCH";
  static constexpr const char *removedFromRoom = "blueboat_REMOVED_FROM_ROOM";
};

namespace pubsub_listeners {
inline constexpr const char *PLAYER_LEFT = "blueboat-player-left";
inline constexpr const char *REQUEST_INFO = "blueboat-request-room-info";
inline constexpr const char *EXTERNAL_MESSAGE = "blueboat-external-message";
} // namespace pubsub_listeners

namespace storage_keys {
inline constexpr const char *ROOM_PREFIX = "room:";
inline constexpr const char *GAME_VALUES = "game-values";
} // namespace storage_keys

} // namespace blueboat::protocol
