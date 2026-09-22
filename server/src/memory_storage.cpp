#include "blueboat/memory_storage.hpp"

namespace blueboat {

namespace {
constexpr auto THREE_HOURS = std::chrono::hours(3);
}

bool MemoryStorage::expired(const Entry &entry) const { return entry.expires_at.has_value() && std::chrono::steady_clock::now() > *entry.expires_at; }

std::optional<std::string> MemoryStorage::get(const std::string &key) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = entries_.find(key);
  if (it == entries_.end() || expired(it->second)) {
    return std::nullopt;
  }
  return it->second.value;
}

void MemoryStorage::set(const std::string &key, const std::string &value, bool no_expiration) {
  std::lock_guard<std::mutex> lock(mutex_);
  Entry entry;
  entry.value = value;
  if (!no_expiration) {
    entry.expires_at = std::chrono::steady_clock::now() + THREE_HOURS;
  }
  entries_[key] = std::move(entry);
}

void MemoryStorage::remove(const std::string &key) {
  std::lock_guard<std::mutex> lock(mutex_);
  entries_.erase(key);
}

std::vector<std::string> MemoryStorage::fetch_keys(const std::string &prefix) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> result;
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (expired(it->second)) {
      it = entries_.erase(it);
      continue;
    }
    if (it->first.rfind(prefix, 0) == 0) {
      result.push_back(it->first.substr(prefix.size()));
    }
    ++it;
  }
  return result;
}

} // namespace blueboat
