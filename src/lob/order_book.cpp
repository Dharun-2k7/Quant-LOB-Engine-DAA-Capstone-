#include "lob/order_book.h"

#include <algorithm>
#include <climits>

namespace lob {

OrderBook::OrderBook(std::size_t reserve_orders) {
  orders_.reserve(reserve_orders);
  trades_.reserve(reserve_orders / 2);
  id_to_index_.reserve(reserve_orders * 2);
}

Price OrderBook::best_bid_price() const {
  if (bids_.empty()) return INT_MIN;
  return bids_.rbegin()->first;
}

Price OrderBook::best_ask_price() const {
  if (asks_.empty()) return INT_MAX;
  return asks_.begin()->first;
}

std::vector<std::pair<Price, long long>> OrderBook::top_levels(Side side, std::size_t depth) const {
  std::vector<std::pair<Price, long long>> out;
  out.reserve(depth);

  if (depth == 0) return out;

  if (side == Side::Buy) {
    // best -> worst
    for (auto it = bids_.rbegin(); it != bids_.rend() && out.size() < depth; ++it) {
      out.push_back({it->first, it->second.total_qty});
    }
  } else {
    // best -> worst
    for (auto it = asks_.begin(); it != asks_.end() && out.size() < depth; ++it) {
      out.push_back({it->first, it->second.total_qty});
    }
  }
  return out;
}

void OrderBook::level_push(Level& lvl, int idx) {
  orders_[idx].prev = lvl.tail;
  orders_[idx].next = -1;
  if (lvl.tail != -1) orders_[lvl.tail].next = idx;
  else lvl.head = idx;
  lvl.tail = idx;
  lvl.total_qty += orders_[idx].qty;
}

void OrderBook::level_remove(Level& lvl, int idx) {
  const int p = orders_[idx].prev;
  const int n = orders_[idx].next;
  if (p != -1) orders_[p].next = n;
  else lvl.head = n;
  if (n != -1) orders_[n].prev = p;
  else lvl.tail = p;

  orders_[idx].prev = -1;
  orders_[idx].next = -1;
}

void OrderBook::erase_level_if_empty(Side side, Price price) {
  if (side == Side::Buy) {
    auto it = bids_.find(price);
    if (it != bids_.end() && it->second.empty()) bids_.erase(it);
  } else {
    auto it = asks_.find(price);
    if (it != asks_.end() && it->second.empty()) asks_.erase(it);
  }
}

int OrderBook::add_resting(OrderId id, Side side, Price price, Qty qty) {
  OrderNode node;
  node.id = id;
  node.side = side;
  node.price = price;
  node.qty = qty;
  node.prev = -1;
  node.next = -1;
  node.alive = true;

  const int idx = next_index_++;
  if (static_cast<int>(orders_.size()) <= idx) orders_.push_back(node);
  else orders_[idx] = node;

  id_to_index_[id] = idx;

  if (side == Side::Buy) {
    Level& lvl = bids_[price];
    level_push(lvl, idx);
  } else {
    Level& lvl = asks_[price];
    level_push(lvl, idx);
  }

  ++live_orders_;
  return idx;
}

bool OrderBook::cancel(OrderId id) {
  auto it = id_to_index_.find(id);
  if (it == id_to_index_.end()) return false;
  const int idx = it->second;
  if (idx < 0 || idx >= static_cast<int>(orders_.size())) return false;
  if (!orders_[idx].alive || orders_[idx].qty <= 0) return false;

  const Side side = orders_[idx].side;
  const Price price = orders_[idx].price;
  const Qty qty = orders_[idx].qty;

  if (side == Side::Buy) {
    auto lvl_it = bids_.find(price);
    if (lvl_it == bids_.end()) return false;
    Level& lvl = lvl_it->second;
    lvl.total_qty -= qty;
    level_remove(lvl, idx);
    orders_[idx].qty = 0;
    orders_[idx].alive = false;
    erase_level_if_empty(side, price);
  } else {
    auto lvl_it = asks_.find(price);
    if (lvl_it == asks_.end()) return false;
    Level& lvl = lvl_it->second;
    lvl.total_qty -= qty;
    level_remove(lvl, idx);
    orders_[idx].qty = 0;
    orders_[idx].alive = false;
    erase_level_if_empty(side, price);
  }

  --live_orders_;
  return true;
}

void OrderBook::match_price_time(OrderId taker_id, Side taker_side, Qty& taker_qty, Price limit_price, TimeNs t) {
  while (taker_qty > 0) {
    if (taker_side == Side::Buy) {
      if (asks_.empty()) break;
      const Price best_price = asks_.begin()->first;
      if (best_price > limit_price) break;

      Level& lvl = asks_.begin()->second;
      int maker_idx = lvl.head;
      if (maker_idx == -1) {
        asks_.erase(asks_.begin());
        continue;
      }

      const Qty exec_qty = std::min(taker_qty, orders_[maker_idx].qty);
      trades_.push_back(Trade{orders_[maker_idx].id, taker_id, best_price, exec_qty, t});

      taker_qty -= exec_qty;
      orders_[maker_idx].qty -= exec_qty;
      lvl.total_qty -= exec_qty;

      if (orders_[maker_idx].qty == 0) {
        orders_[maker_idx].alive = false;
        level_remove(lvl, maker_idx);
        --live_orders_;
      }
      if (lvl.empty()) asks_.erase(asks_.begin());
    } else {
      if (bids_.empty()) break;
      const Price best_price = bids_.rbegin()->first;
      if (best_price < limit_price) break;

      auto it = std::prev(bids_.end());
      Level& lvl = it->second;
      int maker_idx = lvl.head;
      if (maker_idx == -1) {
        bids_.erase(it);
        continue;
      }

      const Qty exec_qty = std::min(taker_qty, orders_[maker_idx].qty);
      trades_.push_back(Trade{orders_[maker_idx].id, taker_id, best_price, exec_qty, t});

      taker_qty -= exec_qty;
      orders_[maker_idx].qty -= exec_qty;
      lvl.total_qty -= exec_qty;

      if (orders_[maker_idx].qty == 0) {
        orders_[maker_idx].alive = false;
        level_remove(lvl, maker_idx);
        --live_orders_;
      }
      if (lvl.empty()) bids_.erase(it);
    }
  }
}

void OrderBook::match_pro_rata(OrderId taker_id, Side taker_side, Qty& taker_qty, Price limit_price, TimeNs t) {
  // Pro-rata at the best price level only, repeatedly.
  while (taker_qty > 0) {
    if (taker_side == Side::Buy) {
      if (asks_.empty()) break;
      const Price best_price = asks_.begin()->first;
      if (best_price > limit_price) break;

      Level& lvl = asks_.begin()->second;
      if (lvl.head == -1) {
        asks_.erase(asks_.begin());
        continue;
      }

      const long long available = lvl.total_qty;
      if (available <= 0) {
        asks_.erase(asks_.begin());
        continue;
      }

      const Qty fill = static_cast<Qty>(std::min<long long>(taker_qty, available));
      if (fill <= 0) break;

      std::vector<int> makers;
      makers.reserve(64);
      for (int cur = lvl.head; cur != -1; cur = orders_[cur].next) {
        if (orders_[cur].qty > 0) makers.push_back(cur);
      }
      if (makers.empty()) {
        asks_.erase(asks_.begin());
        continue;
      }

      long long sum_qty = 0;
      for (int idx : makers) sum_qty += orders_[idx].qty;
      if (sum_qty <= 0) {
        asks_.erase(asks_.begin());
        continue;
      }

      std::vector<Qty> alloc(makers.size(), 0);
      long long allocated = 0;
      for (std::size_t i = 0; i < makers.size(); i++) {
        const long long q = orders_[makers[i]].qty;
        const long long a = (q * fill) / sum_qty;
        alloc[i] = static_cast<Qty>(a);
        allocated += a;
      }

      Qty rem = fill - static_cast<Qty>(allocated);
      for (std::size_t i = 0; i < makers.size() && rem > 0; i++) {
        const int idx = makers[i];
        const Qty can = orders_[idx].qty - alloc[i];
        if (can <= 0) continue;
        const Qty take = std::min(can, rem);
        alloc[i] += take;
        rem -= take;
      }

      for (std::size_t i = 0; i < makers.size() && taker_qty > 0; i++) {
        const int idx = makers[i];
        const Qty exec_qty = std::min(alloc[i], orders_[idx].qty);
        if (exec_qty <= 0) continue;

        trades_.push_back(Trade{orders_[idx].id, taker_id, best_price, exec_qty, t});

        orders_[idx].qty -= exec_qty;
        lvl.total_qty -= exec_qty;
        taker_qty -= exec_qty;

        if (orders_[idx].qty == 0) {
          orders_[idx].alive = false;
          level_remove(lvl, idx);
          --live_orders_;
        }
      }

      if (lvl.empty()) asks_.erase(asks_.begin());
    } else {
      if (bids_.empty()) break;
      const Price best_price = bids_.rbegin()->first;
      if (best_price < limit_price) break;

      auto it = std::prev(bids_.end());
      Level& lvl = it->second;
      if (lvl.head == -1) {
        bids_.erase(it);
        continue;
      }

      const long long available = lvl.total_qty;
      if (available <= 0) {
        bids_.erase(it);
        continue;
      }

      const Qty fill = static_cast<Qty>(std::min<long long>(taker_qty, available));
      if (fill <= 0) break;

      std::vector<int> makers;
      makers.reserve(64);
      for (int cur = lvl.head; cur != -1; cur = orders_[cur].next) {
        if (orders_[cur].qty > 0) makers.push_back(cur);
      }
      if (makers.empty()) {
        bids_.erase(it);
        continue;
      }

      long long sum_qty = 0;
      for (int idx : makers) sum_qty += orders_[idx].qty;
      if (sum_qty <= 0) {
        bids_.erase(it);
        continue;
      }

      std::vector<Qty> alloc(makers.size(), 0);
      long long allocated = 0;
      for (std::size_t i = 0; i < makers.size(); i++) {
        const long long q = orders_[makers[i]].qty;
        const long long a = (q * fill) / sum_qty;
        alloc[i] = static_cast<Qty>(a);
        allocated += a;
      }

      Qty rem = fill - static_cast<Qty>(allocated);
      for (std::size_t i = 0; i < makers.size() && rem > 0; i++) {
        const int idx = makers[i];
        const Qty can = orders_[idx].qty - alloc[i];
        if (can <= 0) continue;
        const Qty take = std::min(can, rem);
        alloc[i] += take;
        rem -= take;
      }

      for (std::size_t i = 0; i < makers.size() && taker_qty > 0; i++) {
        const int idx = makers[i];
        const Qty exec_qty = std::min(alloc[i], orders_[idx].qty);
        if (exec_qty <= 0) continue;

        trades_.push_back(Trade{orders_[idx].id, taker_id, best_price, exec_qty, t});

        orders_[idx].qty -= exec_qty;
        lvl.total_qty -= exec_qty;
        taker_qty -= exec_qty;

        if (orders_[idx].qty == 0) {
          orders_[idx].alive = false;
          level_remove(lvl, idx);
          --live_orders_;
        }
      }

      if (lvl.empty()) bids_.erase(it);
    }
  }
}

void OrderBook::process_limit(OrderId id, Side side, Price price, Qty qty, TimeNs t) {
  Qty taker_qty = qty;
  if (policy_ == MatchPolicy::PriceTime) {
    match_price_time(id, side, taker_qty, price, t);
  } else {
    match_pro_rata(id, side, taker_qty, price, t);
  }

  if (taker_qty > 0) add_resting(id, side, price, taker_qty);
}

}  // namespace lob

