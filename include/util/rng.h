#pragma once

#include <cstdint>

namespace util {

class Rng {
 public:
  explicit Rng(std::uint64_t seed = 88172645463325252ull) : x_(seed) {}

  std::uint64_t next_u64() {
    x_ ^= x_ << 7;
    x_ ^= x_ >> 9;
    return x_;
  }

  std::uint32_t next_u32() { return static_cast<std::uint32_t>(next_u64()); }

  int next_int(int lo_inclusive, int hi_inclusive) {
    const auto span = static_cast<std::uint64_t>(hi_inclusive - lo_inclusive + 1);
    return lo_inclusive + static_cast<int>(next_u64() % span);
  }

  double next_double01() {
    return (next_u64() >> 11) * (1.0 / (1ull << 53));
  }

 private:
  std::uint64_t x_;
};

}  // namespace util

