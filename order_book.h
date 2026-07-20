#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "order.h"

class OrderBook {
public:
    OrderBook(uint32_t min_price, uint32_t num_levels, size_t expected_orders);

    void addOrder(const Order& o);
    void printBook(int depth) const;

    uint64_t trades()  const { return trade_count_; }
    uint64_t volume()  const { return volume_; }

private:
    static const uint32_t NIL = 0xFFFFFFFF;

    struct Node {
        uint64_t order_id;
        uint32_t qty;
        uint32_t next;
    };

    struct Level {
        uint32_t head;
        uint32_t tail;
        uint32_t total_qty;
    };

    uint32_t min_price_;
    uint32_t num_levels_;

    std::vector<Level> levels_;
    std::vector<uint64_t> bid_bits_;
    std::vector<uint64_t> ask_bits_;
    uint32_t best_bid_;
    uint32_t best_ask_;

    std::vector<Node> pool_;
    uint32_t free_head_;

    std::vector<uint64_t> map_keys_;
    std::vector<uint64_t> map_vals_;
    uint64_t map_mask_;

    uint64_t trade_count_;
    uint64_t volume_;

    uint32_t toIndex(uint32_t price) const { return price - min_price_; }

    uint32_t allocNode();
    void freeNode(uint32_t n);

    void mapPut(uint64_t id, uint64_t val);
    bool mapTake(uint64_t id, uint64_t* val);
    void mapErase(uint64_t id);

    void setBit(std::vector<uint64_t>& bits, uint32_t idx);
    void clearBit(std::vector<uint64_t>& bits, uint32_t idx);
    uint32_t highestBit(const std::vector<uint64_t>& bits, uint32_t from) const;
    uint32_t lowestBit(const std::vector<uint64_t>& bits, uint32_t from) const;

    void matchBuy(Order& o);
    void matchSell(Order& o);
    void rest(const Order& o);
    void cancel(uint64_t order_id);
};
