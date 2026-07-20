#include <cstdio>
#include <chrono>
#include <vector>
#include <algorithm>
#include <x86intrin.h>
#include "order_book.h"
#include "baseline_book.h"
#include "parser.h"

using Clock = std::chrono::steady_clock;

static double tsc_ghz;

static void calibrateTsc() {
    auto t0 = Clock::now();
    uint64_t c0 = __rdtsc();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               Clock::now() - t0).count() < 50) { }
    uint64_t c1 = __rdtsc();
    double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
        Clock::now() - t0).count();
    tsc_ghz = (double)(c1 - c0) / ns;
}

struct Stats {
    double secs;
    double p50, p90, p99;
};

template <class MakeBook>
static Stats run(MakeBook make, const std::vector<Order>& orders,
                 uint64_t* trades, uint64_t* volume) {
    Stats s;

    {
        auto book = make();
        auto t0 = Clock::now();
        for (const Order& o : orders) book.addOrder(o);
        s.secs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                     Clock::now() - t0).count() / 1e9;
        *trades = book.trades();
        *volume = book.volume();
    }

    {
        auto book = make();
        std::vector<uint64_t> cyc(orders.size());
        for (size_t i = 0; i < orders.size(); i++) {
            uint64_t c0 = __rdtsc();
            book.addOrder(orders[i]);
            cyc[i] = __rdtsc() - c0;
        }
        std::sort(cyc.begin(), cyc.end());
        size_t n = cyc.size();
        s.p50 = (double)cyc[n / 2] / tsc_ghz;
        s.p90 = (double)cyc[n * 90 / 100] / tsc_ghz;
        s.p99 = (double)cyc[n * 99 / 100] / tsc_ghz;
    }
    return s;
}

static void report(const char* name, const Stats& s, size_t n) {
    double secs = (s.secs > 1e-6) ? s.secs : 1e-6;
    printf("%-10s | %11.0f orders/sec | p50 %6.0f ns | p90 %6.0f ns | p99 %6.0f ns\n",
           name, (double)n / secs, s.p50, s.p90, s.p99);
}

int main(int argc, char** argv) {
    const char* file = (argc > 1) ? argv[1] : "data/orders.csv";

    auto orders = parseOrders(file);
    if (orders.empty()) return 1;

    calibrateTsc();
    printf("TSC calibrated at %.2f GHz\n", tsc_ghz);

    uint32_t lo = 0xFFFFFFFF, hi = 0;
    for (const Order& o : orders) {
        if (o.type == CANCEL) continue;
        if (o.price < lo) lo = o.price;
        if (o.price > hi) hi = o.price;
    }
    uint32_t pad = 200;
    uint32_t min_price = (lo > pad) ? lo - pad : 0;
    uint32_t num_levels = hi - min_price + pad;

    printf("Price range: $%u.%02u - $%u.%02u (%u levels)\n\n",
           lo / 100, lo % 100, hi / 100, hi % 100, num_levels);

    uint64_t ft, fv, bt, bv;

    Stats fs = run([&] { return OrderBook(min_price, num_levels, orders.size()); },
                   orders, &ft, &fv);
    report("nanomatch", fs, orders.size());

    Stats bs = run([] { return BaselineBook(); }, orders, &bt, &bv);
    report("baseline", bs, orders.size());

    if (fs.secs > 1e-6)
        printf("\nSpeedup: %.2fx overall, %.2fx at p50\n",
               bs.secs / fs.secs, bs.p50 / fs.p50);
    else
        printf("\nSpeedup: %.2fx at p50 (file too small for a throughput number)\n",
               bs.p50 / fs.p50);
    printf("Trades: %llu vs %llu, volume: %llu vs %llu %s\n",
           (unsigned long long)ft, (unsigned long long)bt,
           (unsigned long long)fv, (unsigned long long)bv,
           (ft == bt && fv == bv) ? "(engines agree)" : "(MISMATCH!)");

    OrderBook book(min_price, num_levels, orders.size());
    for (const Order& o : orders) book.addOrder(o);
    book.printBook(5);
    return 0;
}
