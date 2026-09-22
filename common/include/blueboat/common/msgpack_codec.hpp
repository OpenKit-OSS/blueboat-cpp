#pragma once

#include <string>

#include "blueboat/common/json.hpp"

namespace blueboat {

std::string encode_msgpack(const Value &value);
Value decode_msgpack(const std::string &bytes);

} // namespace blueboat
