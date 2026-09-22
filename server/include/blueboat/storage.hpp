#pragma once

#include <optional>
#include <string>
#include <vector>

namespace blueboat {

class Storage {
public:
  virtual ~Storage() = default;

  virtual std::optional<std::string> get(const std::string &key) = 0;

  virtual void set(const std::string &key, const std::string &value, bool no_expiration = false) = 0;

  virtual void remove(const std::string &key) = 0;

  virtual std::vector<std::string> fetch_keys(const std::string &prefix) = 0;
};

} // namespace blueboat
