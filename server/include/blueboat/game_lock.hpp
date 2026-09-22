#pragma once

#include <mutex>

namespace blueboat::detail {

inline std::mutex &game_mutex() {
  static std::mutex m;
  return m;
}

inline std::unique_lock<std::mutex> *&current_lock_slot() {
  thread_local std::unique_lock<std::mutex> *ptr = nullptr;
  return ptr;
}

inline std::unique_lock<std::mutex> *current_lock() { return current_lock_slot(); }

class GameLockGuard {
public:
  GameLockGuard() : lock_(game_mutex()) {
    previous_ = current_lock_slot();
    current_lock_slot() = &lock_;
  }
  ~GameLockGuard() { current_lock_slot() = previous_; }

  GameLockGuard(const GameLockGuard &) = delete;
  GameLockGuard &operator=(const GameLockGuard &) = delete;

private:
  std::unique_lock<std::mutex> lock_;
  std::unique_lock<std::mutex> *previous_;
};

} // namespace blueboat::detail
