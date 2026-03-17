#pragma once

#include <cstdint>
#include <vector>

namespace analytics {

struct Opportunity {
  int start{};
  int end{};
  std::int64_t profit{};
};

// Weighted interval scheduling (classic DAA upgrade): O(n log n).
std::int64_t max_profit_weighted_intervals(std::vector<Opportunity> ops);

}  // namespace analytics

