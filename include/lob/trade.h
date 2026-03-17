#pragma once

#include "lob/types.h"

namespace lob {

struct Trade {
  OrderId maker_id{};
  OrderId taker_id{};
  Price price{};
  Qty qty{};
  TimeNs time{};
};

}  // namespace lob

