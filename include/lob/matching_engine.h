#pragma once

#include "lob/order_book.h"

namespace lob {

class MatchingEngine {
 public:
  explicit MatchingEngine(OrderBook& book) : book_(book) {}

  void set_policy(OrderBook::MatchPolicy p) { book_.set_policy(p); }
  OrderBook::MatchPolicy policy() const { return book_.policy(); }

  void on_limit(OrderId id, Side side, Price price, Qty qty, TimeNs t) {
    book_.process_limit(id, side, price, qty, t);
  }

  bool on_cancel(OrderId id) { return book_.cancel(id); }

 private:
  OrderBook& book_;
};

}  // namespace lob

