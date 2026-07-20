# nanomatch

An ultra-low latency limit order matching engine in C++17, built for the
FEC DIY '26 "Nanomatch" project. Single-threaded, allocation-free on the
hot path, and designed around how CPU caches actually work.

## Results (1M synthetic orders, Release build, -O3 -march=native)

| Engine    | Throughput       | p50    | p90    | p99    |
|-----------|------------------|--------|--------|--------|
| nanomatch | ~21M orders/sec  | 27 ns  | 109 ns | 237 ns |
| baseline  | ~8.7M orders/sec | 113 ns | 218 ns | 462 ns |

~2.5x overall, ~4x at the median. Both engines process the same stream
and produce identical trade counts and volume, so the comparison is
apples to apples. Numbers are from a laptop; rerun on your machine.

## Why it is fast

**Prices as integer ticks.** A price is a `uint32_t` in cents, never a
double. Integer compares are exact and cheap, and orders shrink to 24
bytes, so more of them fit in every cache line.

**Flat price ladder instead of std::map.** All price levels live in one
contiguous array. Price -> level is a single subtraction. std::map puts
every level in its own heap node, so walking the book means chasing
pointers all over memory, and every hop is a potential cache miss. Here
the best bid and the level below it are literally neighbours in RAM.

**Bitmap for occupied levels.** One bit per price level says "has resting
orders". When the best level empties, the next best is found with
count-trailing-zeros on a 64-bit word, so one instruction scans 64
prices at once.

**One order pool, zero allocation on the hot path.** Resting orders are
16-byte nodes in a single pre-reserved vector, linked into per-level
FIFO queues by integer index. No new/delete while the engine runs, so
no allocator jitter and no heap fragmentation.

**Flat open-addressing hash map for cancels.** order_id -> node lookup is
a linear probe over one array (kept under 50% full), typically touching
a single cache line. std::unordered_map allocates a node per entry and
chains through buckets, that is two or three dependent memory loads.

**Lazy cancels.** Cancelling zeroes the order's quantity, O(1). The match
loop unlinks dead nodes when it walks past them, so nothing ever
shuffles a queue around the way the naive rebuild-the-queue approach
does.

## Layout

    src/order.h           the 24-byte order message
    src/order_book.h/.cpp the optimized engine (ladder + pool + bitmap)
    src/baseline_book.h/.cpp textbook STL engine, for the benchmark
    src/parser.cpp        single-read, zero-allocation CSV parser
    src/main.cpp          benchmark harness (rdtsc percentiles)
    tools/generate_orders.py synthetic order stream generator
    data/orders.csv       tiny hand-written smoke test

## Build and run

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/nanomatch data/orders.csv

Or directly with g++:

    g++ -std=c++17 -O3 -march=native -o nanomatch \
        src/main.cpp src/order_book.cpp src/baseline_book.cpp src/parser.cpp

Generate a bigger dataset and benchmark:

    python tools/generate_orders.py 1000000 data/orders_big.csv
    ./nanomatch data/orders_big.csv

## Input format

CSV with a header row:

    order_id,timestamp,price,quantity,side,type

side is BUY or SELL, type is LIMIT, MARKET or CANCEL. For a CANCEL the
order_id names the order to pull; price and quantity are ignored.

## Matching rules

Strict price-time priority. An incoming order first sweeps the opposite
side of the book from the best price inward; a limit order stops at its
own price and rests whatever is left, a market order fills what it can
and the remainder is dropped.

## Measurement notes

Throughput is one timer around the whole loop. Percentiles come from a
second pass with rdtsc around each addOrder call (calibrated against
the wall clock), because chrono's resolution is too coarse to see a
30 ns median. The instrumented pass runs on a fresh book so timing
overhead never pollutes the throughput number.
