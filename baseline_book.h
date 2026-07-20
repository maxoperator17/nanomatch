#pragma once
#include <cstdint>
#include <map>
#include <deque>
#include <unordered_map>
#include <functional>
#include "order.h"

class BaselineBook {
public:
    void addOrder(const Order& o);

    uint64_t trades() const { return trade_count_; }
    uint64_t volume() const { return volume_; }

private:
    struct Resting {
        uint64_t order_id;
        uint32_t qty;
    };

    std::map<uint32_t, std::deque<Resting>, std::greater<uint32_t>> bids_;
    std::map<uint32_t, std::deque<Resting>> asks_;
    std::unordered_map<uint64_t, uint32_t> price_of_;

    uint64_t trade_count_ = 0;
    uint64_t volume_ = 0;

    template <class Book>
    void matchAgainst(Book& book, Order& o, bool is_buy);
    void cancel(uint64_t order_id);
};
