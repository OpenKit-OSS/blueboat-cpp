#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace blueboat {

class TimerHandle {
public:
  TimerHandle() = default;
  explicit TimerHandle(std::shared_ptr<std::atomic<bool>> cancelled) : cancelled_(std::move(cancelled)) {}

  void clear() {
    if (cancelled_) {
      cancelled_->store(true);
    }
  }

private:
  std::shared_ptr<std::atomic<bool>> cancelled_;
};

class Scheduler {
public:
  static Scheduler &instance();

  TimerHandle set_timeout(std::chrono::milliseconds delay, std::function<void()> callback);

  Scheduler(const Scheduler &) = delete;
  Scheduler &operator=(const Scheduler &) = delete;

private:
  Scheduler();
  ~Scheduler();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace blueboat
