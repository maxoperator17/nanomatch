#include "parser.h"
#include <cstdio>
#include <cstdlib>

static inline uint64_t readInt(const char*& p) {
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint64_t)(*p - '0');
        p++;
    }
    return v;
}

static inline uint32_t readPriceTicks(const char*& p) {
    uint32_t whole = (uint32_t)readInt(p);
    uint32_t cents = 0;
    if (*p == '.') {
        p++;
        if (*p >= '0' && *p <= '9') { cents = (uint32_t)(*p - '0') * 10; p++; }
        if (*p >= '0' && *p <= '9') { cents += (uint32_t)(*p - '0');     p++; }
        while (*p >= '0' && *p <= '9') p++;
    }
    return whole * 100 + cents;
}

static inline void skipComma(const char*& p) {
    while (*p && *p != ',') p++;
    if (*p == ',') p++;
}

std::vector<Order> parseOrders(const std::string& filename) {
    std::vector<Order> orders;

    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) {
        printf("Error: could not open %s\n", filename.c_str());
        return orders;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)size + 1);
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);

    orders.reserve((size_t)size / 24 + 16);

    const char* p = buf;

    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;

    while (*p) {
        Order o;
        o.order_id  = readInt(p);            skipComma(p);
        o.timestamp = (uint32_t)readInt(p);  skipComma(p);
        o.price     = readPriceTicks(p);     skipComma(p);
        o.quantity  = (uint32_t)readInt(p);  skipComma(p);

        o.side = (*p == 'B') ? BUY : SELL;
        skipComma(p);

        if (*p == 'L')      o.type = LIMIT;
        else if (*p == 'M') o.type = MARKET;
        else                o.type = CANCEL;

        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        orders.push_back(o);
    }

    free(buf);
    printf("Loaded %zu orders from %s\n", orders.size(), filename.c_str());
    return orders;
}
