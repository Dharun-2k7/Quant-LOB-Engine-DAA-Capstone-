#pragma once

#include <map>
#include <unordered_map>
#include <vector>

#include "lob/trade.h"
#include "lob/types.h"

namespace lob {

struct OrderNode {
  OrderId id{-1};
  Side side{Side::Buy};
  Price price{0};
  Qty qty{0};

  int prev{-1};
  int next{-1};
  bool alive{false};
};

struct Level {
  int head{-1};
  int tail{-1};
  long long total_qty{0};

  bool empty() const { return head == -1; }
};

class OrderBook {
 public:
  explicit OrderBook(std::size_t reserve_orders = 0);

  enum class MatchPolicy : std::uint8_t { PriceTime = 0, ProRata = 1 };

  void set_policy(MatchPolicy p) { policy_ = p; }
  MatchPolicy policy() const { return policy_; }

  void process_limit(OrderId id, Side side, Price price, Qty qty, TimeNs t);
  bool cancel(OrderId id);

  Price best_bid_price() const;
  Price best_ask_price() const;

  const std::vector<Trade>& trades() const { return trades_; }
  std::size_t live_orders() const { return live_orders_; }

  // For dashboards/analytics: aggregated price levels near top-of-book.
  // Returns (price, total_qty) pairs.
  std::vector<std::pair<Price, long long>> top_levels(Side side, std::size_t depth) const;

 private:
  std::map<Price, Level> bids_;
  std::map<Price, Level> asks_;

  std::vector<OrderNode> orders_;
  std::unordered_map<OrderId, int> id_to_index_;
  int next_index_{0};

  std::vector<Trade> trades_;
  std::size_t live_orders_{0};
  MatchPolicy policy_{MatchPolicy::PriceTime};

  int add_resting(OrderId id, Side side, Price price, Qty qty);
  void erase_level_if_empty(Side side, Price price);

  void level_push(Level& lvl, int idx);
  void level_remove(Level& lvl, int idx);

  void match_price_time(OrderId taker_id, Side taker_side, Qty& taker_qty, Price limit_price, TimeNs t);
  void match_pro_rata(OrderId taker_id, Side taker_side, Qty& taker_qty, Price limit_price, TimeNs t);
};

}  // namespace lob

