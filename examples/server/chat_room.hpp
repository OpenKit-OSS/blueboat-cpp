#pragma once

#include "blueboat/room.hpp"

class ChatRoom : public blueboat::Room {
public:
  void on_create(const blueboat::Value &options) override;
  void on_join(blueboat::Client &client, const blueboat::Value &options) override;
  void on_message(blueboat::Client &client, const std::string &key, const blueboat::Value &data) override;
  void before_dispose() override;
  void on_leave(blueboat::Client &client, bool intentional) override;

private:
  void add_message(const std::string &text, const std::string &sender_id);
  void catch_up(blueboat::Client &client);
};
