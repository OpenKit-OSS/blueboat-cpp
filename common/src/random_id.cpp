#include "blueboat/common/random_id.hpp"

#include <array>
#include <mutex>
#include <random>

namespace blueboat {

namespace {
std::mt19937_64 &rng() {
  thread_local std::mt19937_64 engine([] {
    std::random_device rd;
    std::seed_seq seed{rd(), rd(), rd(), rd()};
    return std::mt19937_64(seed);
  }());
  return engine;
}
} // namespace

std::string random_id(std::size_t length) {
  static constexpr char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_-";
  static constexpr std::size_t alphabet_size = sizeof(alphabet) - 1;

  std::uniform_int_distribution<std::size_t> dist(0, alphabet_size - 1);
  std::string id;
  id.reserve(length);
  for (std::size_t i = 0; i < length; i++) {
    id.push_back(alphabet[dist(rng())]);
  }
  return id;
}

} // namespace blueboat
