#pragma once

#include <cstdint>

namespace lob {

using OrderId = int;
using Price = int;
using Qty = int;
using TimeNs = std::uint64_t;

enum class Side : std::uint8_t { Buy = 0, Sell = 1 };

}  // namespace lob

