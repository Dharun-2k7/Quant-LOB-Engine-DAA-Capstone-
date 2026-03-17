#include "analytics/dp_optimizer.h"

#include <algorithm>

namespace analytics {

std::int64_t max_profit_weighted_intervals(std::vector<Opportunity> ops) {
  std::sort(ops.begin(), ops.end(), [](const Opportunity& a, const Opportunity& b) {
    if (a.end != b.end) return a.end < b.end;
    return a.start < b.start;
  });

  const int n = static_cast<int>(ops.size());
  std::vector<int> ends(n);
  for (int i = 0; i < n; i++) ends[i] = ops[i].end;

  std::vector<std::int64_t> dp(n + 1, 0);
  for (int i = 1; i <= n; i++) {
    const auto& cur = ops[i - 1];
    const int j = static_cast<int>(
        std::upper_bound(ends.begin(), ends.begin() + (i - 1), cur.start) - ends.begin());
    dp[i] = std::max(dp[i - 1], dp[j] + cur.profit);
  }

  return dp[n];
}

}  // namespace analytics

