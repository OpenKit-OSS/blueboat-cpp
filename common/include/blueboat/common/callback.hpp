#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "blueboat/common/random_id.hpp"

namespace blueboat {

template <typename... Args> class Callback {
public:
  class Subscription {
  public:
    Subscription() = default;
    explicit Subscription(std::function<void()> clear) : clear_(std::move(clear)) {}
    void clear() {
      if (clear_) {
        clear_();
        clear_ = nullptr;
      }
    }

  private:
    std::function<void()> clear_;
  };

  Subscription add(std::function<void(Args...)> callback, bool only_call_once = false) {
    std::string id = random_id();
    entries_.push_back(Entry{std::move(callback), only_call_once, false, id});
    return Subscription([this, id] { remove(id); });
  }

  void clear() { entries_.clear(); }

  void call(Args... args) {
    for (auto &entry : entries_) {
      if (entry.called_once && entry.once) {
        continue;
      }
      entry.callback(args...);
      entry.called_once = true;
    }
    if (std::any_of(entries_.begin(), entries_.end(), [](const Entry &e) { return e.once && e.called_once; })) {
      remove_called_once();
    }
  }

private:
  struct Entry {
    std::function<void(Args...)> callback;
    bool once;
    bool called_once;
    std::string id;
  };

  void remove(const std::string &id) {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const Entry &e) { return e.id == id; }), entries_.end());
  }

  void remove_called_once() {
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [](const Entry &e) { return e.once && e.called_once; }), entries_.end());
  }

  std::vector<Entry> entries_;
};

} // namespace blueboat
