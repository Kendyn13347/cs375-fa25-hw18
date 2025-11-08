#include <cstdint>
#include <iostream>
#include <algorithm>  // for std::min_element

struct TLBEntry {
    uint32_t vpn, pfn;
    uint64_t timestamp;
    bool valid;
};

class TLB {
    TLBEntry entries[16];
    uint64_t clock = 0;

public:
    TLB() {
        for (auto &e : entries) e.valid = false;
    }

    // Return true if found; fills pfn with physical frame
    bool lookup(uint32_t vpn, uint32_t &pfn) {
        clock++;
        for (auto &e : entries) {
            if (e.valid && e.vpn == vpn) {
                e.timestamp = clock;
                pfn = e.pfn;
                return true;
            }
        }
        return false;
    }

    // Insert new mapping with LRU replacement
    void insert(uint32_t vpn, uint32_t pfn) {
        clock++;
        // Try to find an invalid slot
        for (auto &e : entries) {
            if (!e.valid) {
                e = {vpn, pfn, clock, true};
                return;
            }
        }
        // If all valid → replace least recently used
        auto lru = std::min_element(std::begin(entries), std::end(entries),
            [](const TLBEntry &a, const TLBEntry &b) { return a.timestamp < b.timestamp; });
        *lru = {vpn, pfn, clock, true};
    }
};
