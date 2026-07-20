"""Synthetic order generator for nanomatch.

Produces a CSV with a mix of limit orders, market orders and cancels
around a random-walking mid price, so the book gets realistic churn.

Usage: python tools/generate_orders.py [count] [output.csv]
"""

import random
import sys


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 1_000_000
    out = sys.argv[2] if len(sys.argv) > 2 else "data/orders_big.csv"

    random.seed(42)  # reproducible runs

    mid = 250000  # $2500.00 in cents
    next_id = 1
    live = []  # ids of resting orders we can cancel

    with open(out, "w", newline="") as f:
        f.write("order_id,timestamp,price,quantity,side,type\n")
        for ts in range(count):
            mid += random.randint(-3, 3)  # drifting mid price
            r = random.random()

            if r < 0.10 and live:
                # cancel a random resting order
                oid = live.pop(random.randrange(len(live)))
                f.write(f"{oid},{ts},0,0,BUY,CANCEL\n")
                continue

            side = "BUY" if random.random() < 0.5 else "SELL"
            qty = random.randint(1, 500)

            if r < 0.15:
                # market order: crosses the book, price ignored
                f.write(f"{next_id},{ts},{mid/100:.2f},{qty},{side},MARKET\n")
            else:
                # limit order placed a few ticks away from mid
                offset = random.randint(1, 50)
                price = mid - offset if side == "BUY" else mid + offset
                f.write(f"{next_id},{ts},{price/100:.2f},{qty},{side},LIMIT\n")
                live.append(next_id)
                if len(live) > 200_000:
                    live = live[-100_000:]  # keep memory in check

            next_id += 1

    print(f"Wrote {count} messages to {out}")


if __name__ == "__main__":
    main()
