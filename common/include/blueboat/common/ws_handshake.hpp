#pragma once

#include <string>

namespace blueboat::ws_handshake {

std::string generate_client_key();

std::string compute_accept(const std::string &client_key);

} // namespace blueboat::ws_handshake
