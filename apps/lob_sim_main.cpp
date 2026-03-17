#include <iostream>
#include <string>
#include <vector>

#include "analytics/dp_optimizer.h"
#include "lob/matching_engine.h"
#include "lob/order_book.h"
#include "sim/event_generator.h"
#include "util/timer.h"

struct BenchResult {
  std::string name;
  double seconds{};
  std::size_t events{};
  std::size_t trades{};
  int best_bid{};
  int best_ask{};
  std::size_t live_orders{};
};

static BenchResult run_bench(const std::string& name, const std::vector<sim::Event>& ev,
                             lob::OrderBook::MatchPolicy policy) {
  lob::OrderBook book(ev.size());
  book.set_policy(policy);
  lob::MatchingEngine eng(book);

  const auto t0 = util::now_ns();
  for (const auto& e : ev) {
    if (e.is_cancel) {
      eng.on_cancel(e.id);
    } else {
      eng.on_limit(e.id, e.side, e.price, e.qty, e.t);
    }
  }
  const auto t1 = util::now_ns();

  const double sec = (t1 - t0) / 1e9;
  return BenchResult{name, sec, ev.size(), book.trades().size(), book.best_bid_price(),
                     book.best_ask_price(), book.live_orders()};
}

int main(int argc, char** argv) {
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  sim::EventGenConfig cfg;
  if (argc >= 2) cfg.n_events = static_cast<std::size_t>(std::stoull(argv[1]));
  if (argc >= 3) cfg.seed = static_cast<std::uint64_t>(std::stoull(argv[2]));

  const auto events = sim::generate_events(cfg);

  const auto r1 = run_bench("price-time", events, lob::OrderBook::MatchPolicy::PriceTime);
  const auto r2 = run_bench("pro-rata", events, lob::OrderBook::MatchPolicy::ProRata);

  std::cout << "Benchmark\n";
  std::cout << "  events: " << cfg.n_events << "\n";
  std::cout << "  " << r1.name << ": " << r1.seconds << " s"
            << " | " << (double)r1.events / std::max(1e-9, r1.seconds) << " ev/s"
            << " | trades=" << r1.trades << " | live=" << r1.live_orders << "\n";
  std::cout << "  " << r2.name << ": " << r2.seconds << " s"
            << " | " << (double)r2.events / std::max(1e-9, r2.seconds) << " ev/s"
            << " | trades=" << r2.trades << " | live=" << r2.live_orders << "\n";

  // DP demo (O(n log n)): weighted interval scheduling.
  {
    std::vector<analytics::Opportunity> ops;
    ops.reserve(50000);
    for (int i = 0; i < 50000; i++) {
      const int s = i * 2;
      const int e = s + 5 + (i % 23);
      const std::int64_t p = 1 + (i % 100);
      ops.push_back({s, e, p});
    }
    const auto t0 = util::now_ns();
    const auto best = analytics::max_profit_weighted_intervals(ops);
    const auto t1 = util::now_ns();
    std::cout << "DP demo\n";
    std::cout << "  opportunities: " << ops.size() << "\n";
    std::cout << "  best_profit: " << best << "\n";
    std::cout << "  time: " << (t1 - t0) / 1e6 << " ms\n";
  }

  return 0;
}

