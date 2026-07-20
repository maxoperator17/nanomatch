#pragma once
#include <cstdint>

enum Side : uint8_t { BUY = 0, SELL = 1 };

enum MsgType : uint8_t { LIMIT = 0, MARKET = 1, CANCEL = 2 };

struct Order {
    uint64_t order_id;
    uint32_t timestamp;
    uint32_t price;
    uint32_t quantity;
    Side     side;
    MsgType  type;
};
