#include "blueboat/common/ws_handshake.hpp"

#include <openssl/evp.h>

#include <array>
#include <random>

namespace blueboat::ws_handshake {

namespace {
constexpr const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string base64_encode(const unsigned char *data, int length) {
  std::string out;
  out.resize(4 * ((length + 2) / 3) + 1);
  int written = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(out.data()), data, length);
  out.resize(written);
  return out;
}
} // namespace

std::string generate_client_key() {
  std::array<unsigned char, 16> bytes{};
  std::random_device rd;
  for (auto &byte : bytes) {
    byte = static_cast<unsigned char>(rd() & 0xFF);
  }
  return base64_encode(bytes.data(), static_cast<int>(bytes.size()));
}

std::string compute_accept(const std::string &client_key) {
  std::string input = client_key + WS_GUID;
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_len = 0;
  EVP_Digest(input.data(), input.size(), digest.data(), &digest_len, EVP_sha1(), nullptr);
  return base64_encode(digest.data(), static_cast<int>(digest_len));
}

} // namespace blueboat::ws_handshake
