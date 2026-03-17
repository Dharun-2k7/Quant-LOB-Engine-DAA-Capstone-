#pragma once

#include "lob/types.h"

namespace sim {

struct Event {
  lob::TimeNs t{};
  lob::OrderId id{};
  lob::Price price{};
  lob::Qty qty{};
  lob::Side side{lob::Side::Buy};
  bool is_cancel{false};
};

}  // namespace sim

