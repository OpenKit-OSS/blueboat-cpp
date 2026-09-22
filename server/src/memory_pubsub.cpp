#include "blueboat/memory_pubsub.hpp"

#include <algorithm>

#include "blueboat/common/random_id.hpp"

namespace blueboat {

PubSubSubscription MemoryPubSub::on(const std::string &key, Handler handler) {
  std::string id = random_id();
  listeners_[key].push_back(Listener{id, std::move(handler)});
  return PubSubSubscription([this, key, id] {
    auto it = listeners_.find(key);
    if (it == listeners_.end()) {
      return;
    }
    auto &vec = it->second;
    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const Listener &l) { return l.id == id; }), vec.end());
    if (vec.empty()) {
      listeners_.erase(it);
    }
  });
}

void MemoryPubSub::publish(const std::string &key, const Value &data) {
  auto it = listeners_.find(key);
  if (it == listeners_.end()) {
    return;
  }
  auto listeners_copy = it->second;
  for (auto &listener : listeners_copy) {
    listener.handler(data);
  }
}

} // namespace blueboat
