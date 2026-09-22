#pragma once

#include <functional>
#include <string>

#include "blueboat/common/json.hpp"

namespace blueboat {

class PubSubSubscription {
public:
  PubSubSubscription() = default;
  explicit PubSubSubscription(std::function<void()> unsubscribe) : unsubscribe_(std::move(unsubscribe)) {}
  void unsubscribe() {
    if (unsubscribe_) {
      unsubscribe_();
      unsubscribe_ = nullptr;
    }
  }

private:
  std::function<void()> unsubscribe_;
};

class PubSub {
public:
  using Handler = std::function<void(const Value &)>;

  virtual ~PubSub() = default;
  virtual PubSubSubscription on(const std::string &key, Handler handler) = 0;
  virtual void publish(const std::string &key, const Value &data) = 0;
};

} // namespace blueboat
