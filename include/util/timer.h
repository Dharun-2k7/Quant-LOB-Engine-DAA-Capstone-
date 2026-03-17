#pragma once

#include <chrono>
#include <cstdint>

namespace util {

inline std::uint64_t now_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch())
          .count());
}

}  // namespace util

