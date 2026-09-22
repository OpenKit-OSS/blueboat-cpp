#include "blueboat/game_values.hpp"

#include "blueboat/common/protocol.hpp"

namespace blueboat {

void GameValues::set_all(const Value &new_values) { storage_.set(protocol::storage_keys::GAME_VALUES, new_values.dump(), true); }

Value GameValues::get_all() const {
  auto raw = const_cast<Storage &>(storage_).get(protocol::storage_keys::GAME_VALUES);
  if (!raw) {
    return Value::object();
  }
  try {
    return Value::parse(*raw);
  } catch (const std::exception &) {
    return Value::object();
  }
}

Value GameValues::get(const std::string &key, const Value &default_value) const {
  Value values = get_all();
  if (values.contains(key)) {
    return values[key];
  }
  return default_value;
}

void GameValues::set(const std::string &key, const Value &value) {
  Value values = get_all();
  values[key] = value;
  set_all(values);
}

void GameValues::reset() { set_all(Value::object()); }

} // namespace blueboat
