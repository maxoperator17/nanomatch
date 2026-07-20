#include "order_book.h"
#include <cstdio>

OrderBook::OrderBook(uint32_t min_price, uint32_t num_levels, size_t expected_orders) {
    min_price_ = min_price;

    num_levels_ = (num_levels + 63) & ~63u;

    levels_.assign(num_levels_, Level{NIL, NIL, 0});
    bid_bits_.assign(num_levels_ / 64, 0);
    ask_bits_.assign(num_levels_ / 64, 0);
    best_bid_ = NIL;
    best_ask_ = NIL;

    pool_.reserve(expected_orders);
    free_head_ = NIL;

    size_t cap = 64;
    while (cap < expected_orders * 2) cap <<= 1;
    map_keys_.assign(cap, 0);
    map_vals_.assign(cap, 0);
    map_mask_ = cap - 1;

    trade_count_ = 0;
    volume_ = 0;
}

uint32_t OrderBook::allocNode() {
    if (free_head_ != NIL) {
        uint32_t n = free_head_;
        free_head_ = pool_[n].next;
        return n;
    }
    pool_.push_back(Node{0, 0, NIL});
    return (uint32_t)(pool_.size() - 1);
}

void OrderBook::freeNode(uint32_t n) {
    pool_[n].next = free_head_;
    free_head_ = n;
}

static inline uint64_t mix(uint64_t x) {
    x *= 0x9E3779B97F4A7C15ull;
    return x ^ (x >> 32);
}

void OrderBook::mapPut(uint64_t id, uint64_t val) {
    uint64_t i = mix(id) & map_mask_;
    while (map_keys_[i] != 0) i = (i + 1) & map_mask_;
    map_keys_[i] = id;
    map_vals_[i] = val;
}

bool OrderBook::mapTake(uint64_t id, uint64_t* val) {
    uint64_t i = mix(id) & map_mask_;
    while (map_keys_[i] != id) {
        if (map_keys_[i] == 0) return false;
        i = (i + 1) & map_mask_;
    }
    *val = map_vals_[i];

    uint64_t hole = i;
    uint64_t j = i;
    while (true) {
        j = (j + 1) & map_mask_;
        uint64_t k = map_keys_[j];
        if (k == 0) break;
        uint64_t home = mix(k) & map_mask_;
        bool between = (hole <= j) ? (hole < home && home <= j)
                                   : (hole < home || home <= j);
        if (!between) {
            map_keys_[hole] = k;
            map_vals_[hole] = map_vals_[j];
            hole = j;
        }
    }
    map_keys_[hole] = 0;
    return true;
}

void OrderBook::mapErase(uint64_t id) {
    uint64_t dummy;
    mapTake(id, &dummy);
}

void OrderBook::setBit(std::vector<uint64_t>& bits, uint32_t idx) {
    bits[idx >> 6] |= 1ull << (idx & 63);
}

void OrderBook::clearBit(std::vector<uint64_t>& bits, uint32_t idx) {
    bits[idx >> 6] &= ~(1ull << (idx & 63));
}

uint32_t OrderBook::highestBit(const std::vector<uint64_t>& bits, uint32_t from) const {
    if (from == NIL || from >= num_levels_) {
        if (num_levels_ == 0) return NIL;
        from = num_levels_ - 1;
    }
    int w = (int)(from >> 6);
    uint64_t word = bits[w];
    uint32_t r = from & 63;
    if (r != 63) word &= (1ull << (r + 1)) - 1;
    while (true) {
        if (word) return (uint32_t)(w * 64 + 63 - __builtin_clzll(word));
        if (w == 0) return NIL;
        word = bits[--w];
    }
}

uint32_t OrderBook::lowestBit(const std::vector<uint64_t>& bits, uint32_t from) const {
    if (from >= num_levels_) return NIL;
    size_t w = from >> 6;
    uint64_t word = bits[w] & (~0ull << (from & 63));
    while (true) {
        if (word) return (uint32_t)(w * 64 + __builtin_ctzll(word));
        if (++w == bits.size()) return NIL;
        word = bits[w];
    }
}

void OrderBook::addOrder(const Order& o) {
    if (o.type == CANCEL) {
        cancel(o.order_id);
        return;
    }

    Order in = o;
    if (in.side == BUY) matchBuy(in);
    else                matchSell(in);

    if (in.type == LIMIT && in.quantity > 0) rest(in);
}

void OrderBook::matchBuy(Order& o) {
    while (o.quantity > 0 && best_ask_ != NIL) {
        uint32_t idx = best_ask_;
        if (o.type == LIMIT && toIndex(o.price) < idx) break;

        Level& lv = levels_[idx];
        uint32_t n = lv.head;
        while (o.quantity > 0 && n != NIL) {
            Node& nd = pool_[n];
            if (nd.qty == 0) {
                uint32_t nx = nd.next;
                freeNode(n);
                n = nx;
                lv.head = n;
                continue;
            }
            uint32_t fill = (o.quantity < nd.qty) ? o.quantity : nd.qty;
            o.quantity -= fill;
            nd.qty -= fill;
            lv.total_qty -= fill;
            trade_count_++;
            volume_ += fill;
            if (nd.qty == 0) {
                mapErase(nd.order_id);
                uint32_t nx = nd.next;
                freeNode(n);
                n = nx;
                lv.head = n;
            }
        }
        if (lv.head == NIL) {
            lv.tail = NIL;
            clearBit(ask_bits_, idx);
            best_ask_ = lowestBit(ask_bits_, idx + 1);
        }
    }
}

void OrderBook::matchSell(Order& o) {
    while (o.quantity > 0 && best_bid_ != NIL) {
        uint32_t idx = best_bid_;
        if (o.type == LIMIT && toIndex(o.price) > idx) break;

        Level& lv = levels_[idx];
        uint32_t n = lv.head;
        while (o.quantity > 0 && n != NIL) {
            Node& nd = pool_[n];
            if (nd.qty == 0) {
                uint32_t nx = nd.next;
                freeNode(n);
                n = nx;
                lv.head = n;
                continue;
            }
            uint32_t fill = (o.quantity < nd.qty) ? o.quantity : nd.qty;
            o.quantity -= fill;
            nd.qty -= fill;
            lv.total_qty -= fill;
            trade_count_++;
            volume_ += fill;
            if (nd.qty == 0) {
                mapErase(nd.order_id);
                uint32_t nx = nd.next;
                freeNode(n);
                n = nx;
                lv.head = n;
            }
        }
        if (lv.head == NIL) {
            lv.tail = NIL;
            clearBit(bid_bits_, idx);
            best_bid_ = (idx == 0) ? NIL : highestBit(bid_bits_, idx - 1);
        }
    }
}

void OrderBook::rest(const Order& o) {
    uint32_t idx = toIndex(o.price);
    uint32_t n = allocNode();
    pool_[n].order_id = o.order_id;
    pool_[n].qty = o.quantity;
    pool_[n].next = NIL;

    Level& lv = levels_[idx];
    if (lv.head == NIL) {
        lv.head = lv.tail = n;
    } else {
        pool_[lv.tail].next = n;
        lv.tail = n;
    }
    lv.total_qty += o.quantity;

    if (o.side == BUY) {
        setBit(bid_bits_, idx);
        if (best_bid_ == NIL || idx > best_bid_) best_bid_ = idx;
    } else {
        setBit(ask_bits_, idx);
        if (best_ask_ == NIL || idx < best_ask_) best_ask_ = idx;
    }

    mapPut(o.order_id, ((uint64_t)idx << 32) | n);
}

void OrderBook::cancel(uint64_t order_id) {
    uint64_t val;
    if (!mapTake(order_id, &val)) return;

    uint32_t idx = (uint32_t)(val >> 32);
    uint32_t n = (uint32_t)val;

    Level& lv = levels_[idx];
    lv.total_qty -= pool_[n].qty;
    pool_[n].qty = 0;

    if (lv.total_qty == 0) {
        uint32_t m = lv.head;
        while (m != NIL) {
            uint32_t nx = pool_[m].next;
            freeNode(m);
            m = nx;
        }
        lv.head = lv.tail = NIL;

        bool is_bid = (bid_bits_[idx >> 6] >> (idx & 63)) & 1;
        if (is_bid) {
            clearBit(bid_bits_, idx);
            if (idx == best_bid_)
                best_bid_ = (idx == 0) ? NIL : highestBit(bid_bits_, idx - 1);
        } else {
            clearBit(ask_bits_, idx);
            if (idx == best_ask_)
                best_ask_ = lowestBit(ask_bits_, idx + 1);
        }
    }
}

void OrderBook::printBook(int depth) const {
    printf("\n=== ORDER BOOK (top %d) ===\n", depth);

    uint32_t asks[64];
    int na = 0;
    uint32_t idx = best_ask_;
    while (idx != NIL && na < depth) {
        asks[na++] = idx;
        idx = lowestBit(ask_bits_, idx + 1);
    }
    printf("--- ASKS ---\n");
    for (int i = na - 1; i >= 0; i--) {
        uint32_t p = min_price_ + asks[i];
        printf("  $%u.%02u | qty %u\n", p / 100, p % 100, levels_[asks[i]].total_qty);
    }

    printf("--- BIDS ---\n");
    idx = best_bid_;
    for (int i = 0; idx != NIL && i < depth; i++) {
        uint32_t p = min_price_ + idx;
        printf("  $%u.%02u | qty %u\n", p / 100, p % 100, levels_[idx].total_qty);
        idx = (idx == 0) ? NIL : highestBit(bid_bits_, idx - 1);
    }
    printf("===========================\n");
}
