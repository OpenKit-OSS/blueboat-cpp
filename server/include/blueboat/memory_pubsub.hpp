#pragma once

#include <unordered_map>
#include <vector>

#include "blueboat/pubsub.hpp"

namespace blueboat {

class MemoryPubSub : public PubSub {
public:
  PubSubSubscription on(const std::string &key, Handler handler) override;
  void publish(const std::string &key, const Value &data) override;

private:
  struct Listener {
    std::string id;
    Handler handler;
  };

  std::unordered_map<std::string, std::vector<Listener>> listeners_;
};

} // namespace blueboat
