#pragma once

#include "blueboat/common/json.hpp"
#include "blueboat/storage.hpp"

namespace blueboat {

class GameValues {
public:
  explicit GameValues(Storage &storage) : storage_(storage) {}

  void set_all(const Value &new_values);
  Value get_all() const;

  Value get(const std::string &key, const Value &default_value = Value()) const;
  void set(const std::string &key, const Value &value);
  void reset();

private:
  Storage &storage_;
};

} // namespace blueboat
