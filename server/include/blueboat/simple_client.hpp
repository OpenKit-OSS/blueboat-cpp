#pragma once

#include <string>

namespace blueboat {

struct SimpleClient {
  std::string id;
  std::string session_id;
  std::string origin;
  std::string ip;
};

} // namespace blueboat
