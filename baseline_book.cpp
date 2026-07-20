#include "baseline_book.h"

template <class Book>
void BaselineBook::matchAgainst(Book& book, Order& o, bool is_buy) {
    while (o.quantity > 0 && !book.empty()) {
        auto it = book.begin();
        uint32_t best_price = it->first;

        if (o.type == LIMIT) {
            if (is_buy  && o.price < best_price) break;
            if (!is_buy && o.price > best_price) break;
        }

        auto& queue = it->second;
        while (o.quantity > 0 && !queue.empty()) {
            Resting& r = queue.front();
            uint32_t fill = (o.quantity < r.qty) ? o.quantity : r.qty;
            o.quantity -= fill;
            r.qty -= fill;
            trade_count_++;
            volume_ += fill;
            if (r.qty == 0) {
                price_of_.erase(r.order_id);
                queue.pop_front();
            }
        }
        if (queue.empty()) book.erase(it);
    }
}

void BaselineBook::addOrder(const Order& o) {
    if (o.type == CANCEL) {
        cancel(o.order_id);
        return;
    }

    Order in = o;
    if (in.side == BUY) matchAgainst(asks_, in, true);
    else                matchAgainst(bids_, in, false);

    if (in.type == LIMIT && in.quantity > 0) {
        if (in.side == BUY) bids_[in.price].push_back({in.order_id, in.quantity});
        else                asks_[in.price].push_back({in.order_id, in.quantity});
        price_of_[in.order_id] = in.price;
    }
}

void BaselineBook::cancel(uint64_t order_id) {
    auto it = price_of_.find(order_id);
    if (it == price_of_.end()) return;
    uint32_t price = it->second;
    price_of_.erase(it);

    auto scrub = [&](auto& book) {
        auto lit = book.find(price);
        if (lit == book.end()) return false;
        auto& queue = lit->second;
        for (auto qit = queue.begin(); qit != queue.end(); ++qit) {
            if (qit->order_id == order_id) {
                queue.erase(qit);
                if (queue.empty()) book.erase(lit);
                return true;
            }
        }
        return false;
    };

    if (!scrub(bids_)) scrub(asks_);
}
