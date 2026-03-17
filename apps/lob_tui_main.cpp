#include <chrono>
#include <climits>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "lob/matching_engine.h"
#include "lob/order_book.h"
#include "util/rng.h"

namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_sigint(int) { g_stop = 1; }

void clear_screen() {
  // ANSI: clear + move cursor home (faster than system("clear"))
  std::cout << "\x1b[2J\x1b[H";
}

void hide_cursor() { std::cout << "\x1b[?25l"; }
void show_cursor() { std::cout << "\x1b[?25h"; }

struct LiveConfig {
  std::uint64_t seed{42};
  int start_price{10000};
  int tick_ms{120};
  int depth{10};
  double cancel_prob{0.10};
  lob::OrderBook::MatchPolicy policy{lob::OrderBook::MatchPolicy::PriceTime};
};

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGINT, on_sigint);

  LiveConfig cfg;
  if (argc >= 2) cfg.seed = static_cast<std::uint64_t>(std::stoull(argv[1]));
  if (argc >= 3) cfg.tick_ms = std::stoi(argv[2]);
  if (argc >= 4) cfg.depth = std::stoi(argv[3]);
  if (argc >= 5) {
    const std::string p = argv[4];
    cfg.policy = (p == "pro") ? lob::OrderBook::MatchPolicy::ProRata
                              : lob::OrderBook::MatchPolicy::PriceTime;
  }

  util::Rng rng(cfg.seed);
  lob::OrderBook book(200000);
  book.set_policy(cfg.policy);
  lob::MatchingEngine eng(book);

  int mid = cfg.start_price;
  lob::OrderId next_id = 1;
  std::vector<lob::OrderId> live_ids;
  live_ids.reserve(200000);

  std::size_t last_trade_count = 0;
  lob::Trade last_trade{};
  std::uint64_t step = 0;

  hide_cursor();
  auto restore_cursor = []() { show_cursor(); };
  std::atexit(+[]() { show_cursor(); });

  while (!g_stop) {
    ++step;

    // random walk mid
    mid += rng.next_int(-2, 2);
    if (mid < 1) mid = 1;

    // generate 1 event per tick (can scale up later)
    const bool do_cancel = (!live_ids.empty() && rng.next_double01() < cfg.cancel_prob);
    if (do_cancel) {
      const int pos = rng.next_int(0, static_cast<int>(live_ids.size()) - 1);
      const lob::OrderId id = live_ids[pos];
      live_ids[pos] = live_ids.back();
      live_ids.pop_back();
      eng.on_cancel(id);
    } else {
      const bool is_buy = (rng.next_u32() & 1u) != 0;
      const int px_offset = rng.next_int(0, 6);
      const lob::Price price = is_buy ? (mid + px_offset) : (mid - px_offset);
      const lob::Qty qty = rng.next_int(1, 50);
      const lob::OrderId id = next_id++;
      live_ids.push_back(id);
      eng.on_limit(id, is_buy ? lob::Side::Buy : lob::Side::Sell, price, qty, step);
    }

    // update last trade if any new
    const auto& tr = book.trades();
    if (tr.size() != last_trade_count && !tr.empty()) {
      last_trade = tr.back();
      last_trade_count = tr.size();
    }

    // render dashboard
    clear_screen();
    const auto bids = book.top_levels(lob::Side::Buy, static_cast<std::size_t>(cfg.depth));
    const auto asks = book.top_levels(lob::Side::Sell, static_cast<std::size_t>(cfg.depth));

    const int best_bid = book.best_bid_price();
    const int best_ask = book.best_ask_price();
    const long long spread = (best_bid == INT_MIN || best_ask == INT_MAX) ? -1 : (best_ask - best_bid);

    std::cout << "==============================\n";
    std::cout << " LIVE LOB TUI (Ctrl+C to stop)\n";
    std::cout << "==============================\n";
    std::cout << "seed=" << cfg.seed
              << " | tick_ms=" << cfg.tick_ms
              << " | depth=" << cfg.depth
              << " | policy=" << ((cfg.policy == lob::OrderBook::MatchPolicy::ProRata) ? "pro-rata" : "price-time")
              << "\n";
    std::cout << "step=" << step
              << " | mid=" << mid
              << " | best_bid=" << (best_bid == INT_MIN ? 0 : best_bid)
              << " | best_ask=" << (best_ask == INT_MAX ? 0 : best_ask)
              << " | spread=" << spread
              << " | live_orders=" << book.live_orders()
              << " | trades=" << tr.size()
              << "\n\n";

    if (last_trade_count > 0) {
      std::cout << "Last trade: qty=" << last_trade.qty << " @ " << last_trade.price
                << " (maker=" << last_trade.maker_id << ", taker=" << last_trade.taker_id << ")\n\n";
    } else {
      std::cout << "Last trade: (none yet)\n\n";
    }

    std::cout << "   BIDS (price,qty)              ASKS (price,qty)\n";
    std::cout << "------------------------------------------------------\n";
    const std::size_t rows = std::max(bids.size(), asks.size());
    for (std::size_t i = 0; i < rows; i++) {
      if (i < bids.size()) {
        std::cout << std::setw(8) << bids[i].first << " " << std::setw(8) << bids[i].second;
      } else {
        std::cout << std::setw(8) << " " << " " << std::setw(8) << " ";
      }
      std::cout << "            ";
      if (i < asks.size()) {
        std::cout << std::setw(8) << asks[i].first << " " << std::setw(8) << asks[i].second;
      }
      std::cout << "\n";
    }

    std::cout.flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(cfg.tick_ms));
  }

  restore_cursor();
  std::cout << "\nStopped.\n";
  return 0;
}

