#include "sim/event_generator.h"

#include <algorithm>

#include "util/rng.h"

namespace sim {

std::vector<Event> generate_events(const EventGenConfig& cfg) {
  util::Rng rng(cfg.seed);
  std::vector<Event> ev;
  ev.reserve(cfg.n_events);

  int next_id = 1;
  std::vector<int> live_ids;
  live_ids.reserve(cfg.n_events / 2);

  lob::TimeNs t = 0;
  for (std::size_t i = 0; i < cfg.n_events; i++) {
    t += static_cast<lob::TimeNs>(rng.next_int(0, 3));

    const bool do_cancel = (!live_ids.empty() && rng.next_double01() < cfg.cancel_prob);
    if (do_cancel) {
      const int pos = rng.next_int(0, static_cast<int>(live_ids.size()) - 1);
      const int id = live_ids[pos];
      live_ids[pos] = live_ids.back();
      live_ids.pop_back();

      ev.push_back(Event{t, id, 0, 0, lob::Side::Buy, true});
    } else {
      const int id = next_id++;
      live_ids.push_back(id);

      const lob::Side side = (rng.next_u32() & 1u) ? lob::Side::Buy : lob::Side::Sell;
      const int price = rng.next_int(cfg.px_min, cfg.px_max);
      const int qty = rng.next_int(cfg.qty_min, cfg.qty_max);
      ev.push_back(Event{t, id, price, qty, side, false});
    }
  }

  std::sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) {
    if (a.t != b.t) return a.t < b.t;
    return a.id < b.id;
  });
  return ev;
}

}  // namespace sim

