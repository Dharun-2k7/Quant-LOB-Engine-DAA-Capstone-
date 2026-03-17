#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sim/event.h"

namespace sim {

struct EventGenConfig {
  std::size_t n_events{100000};
  std::uint64_t seed{42};
  double cancel_prob{0.10};

  int px_min{9900};
  int px_max{10100};
  int qty_min{1};
  int qty_max{50};
};

std::vector<Event> generate_events(const EventGenConfig& cfg);

}  // namespace sim

