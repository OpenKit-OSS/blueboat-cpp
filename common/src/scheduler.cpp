#include "blueboat/common/scheduler.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace blueboat {

namespace {
struct Task {
  std::chrono::steady_clock::time_point due;
  std::function<void()> callback;
  std::shared_ptr<std::atomic<bool>> cancelled;
};
} // namespace

struct Scheduler::Impl {
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<Task> tasks;
  bool stopping = false;
  std::thread worker;

  Impl() {
    worker = std::thread([this] { run(); });
  }

  ~Impl() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
    }
    cv.notify_all();
    if (worker.joinable()) {
      worker.join();
    }
  }

  void run() {
    std::unique_lock<std::mutex> lock(mutex);
    while (!stopping) {
      if (tasks.empty()) {
        cv.wait(lock);
        continue;
      }

      auto next = std::min_element(tasks.begin(), tasks.end(), [](const Task &a, const Task &b) { return a.due < b.due; });
      auto now = std::chrono::steady_clock::now();
      if (next->due > now) {
        cv.wait_until(lock, next->due);
        continue;
      }

      Task task = std::move(*next);
      tasks.erase(next);

      if (task.cancelled->load()) {
        continue;
      }

      lock.unlock();
      task.callback();
      lock.lock();
    }
  }
};

Scheduler::Scheduler() : impl_(std::make_unique<Impl>()) {}
Scheduler::~Scheduler() = default;

Scheduler &Scheduler::instance() {
  static Scheduler scheduler;
  return scheduler;
}

TimerHandle Scheduler::set_timeout(std::chrono::milliseconds delay, std::function<void()> callback) {
  auto cancelled = std::make_shared<std::atomic<bool>>(false);
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->tasks.push_back(Task{std::chrono::steady_clock::now() + delay, std::move(callback), cancelled});
  }
  impl_->cv.notify_all();
  return TimerHandle(cancelled);
}

} // namespace blueboat
