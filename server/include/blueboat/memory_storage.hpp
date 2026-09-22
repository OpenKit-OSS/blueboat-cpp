#pragma once

#include <chrono>
#include <mutex>
#include <unordered_map>

#include "blueboat/storage.hpp"

namespace blueboat {

class MemoryStorage : public Storage {
public:
  std::optional<std::string> get(const std::string &key) override;
  void set(const std::string &key, const std::string &value, bool no_expiration) override;
  void remove(const std::string &key) override;
  std::vector<std::string> fetch_keys(const std::string &prefix) override;

private:
  struct Entry {
    std::string value;
    std::optional<std::chrono::steady_clock::time_point> expires_at;
  };

  bool expired(const Entry &entry) const;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, Entry> entries_;
};

} // namespace blueboat
