// src/game/Entity/path_cost_index.cpp -- NOT RETAIL (HITCH_PAIRSPREAD): the PathGraph
// region-pair cost map (path_cost_index.h). Open addressing with linear probing; a slot is
// live when its stamp is the current one, so Clear is one increment. One table: the game
// has one PathGraph (CitizenBehavior+0x1B8); a different PathGraph starts it over.
#include "Entity/path_cost_index.h"
#include <cstddef>
#include <vector>

namespace {

struct Slot {
    uint64_t key;
    int32_t  value;
    uint32_t stamp;
};

struct Table {
    std::vector<Slot> slots;
    uint32_t stamp = 1;
    int bits = 0;
    int live = 0;
    const PathGraph* owner = nullptr;

    static uint64_t Key(int high, int low) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(high)) << 32) | static_cast<uint32_t>(low);
    }
    size_t Home(uint64_t key) const {
        return static_cast<size_t>((key * 0x9E3779B97F4A7C15ull) >> (64 - bits));
    }
    void Drop() {
        live = 0;
        if (++stamp == 0) {
            for (Slot& s : slots) s.stamp = 0;
            stamp = 1;
        }
    }
    void Own(const PathGraph* pg) {
        if (pg != owner) {
            owner = pg;
            Drop();
        }
    }
    void Grow() {
        std::vector<Slot> old;
        old.swap(slots);
        const uint32_t oldStamp = stamp;
        bits = bits == 0 ? 14 : bits + 1;
        slots.assign(static_cast<size_t>(1) << bits, Slot{0, 0, 0});
        stamp = 1;
        live = 0;
        const size_t mask = slots.size() - 1;
        for (const Slot& s : old) {
            if (s.stamp != oldStamp) continue;
            size_t i = Home(s.key);
            while (slots[i].stamp == stamp) i = (i + 1) & mask;
            slots[i] = Slot{s.key, s.value, stamp};
            ++live;
        }
    }
};

Table s_table;

}  // namespace

namespace PathCostIndex {

int32_t* Find(const PathGraph* pg, int high, int low) {
    Table& t = s_table;
    t.Own(pg);
    if (t.live == 0) return nullptr;
    const uint64_t key = Table::Key(high, low);
    const size_t mask = t.slots.size() - 1;
    for (size_t i = t.Home(key);; i = (i + 1) & mask) {
        Slot& s = t.slots[i];
        if (s.stamp != t.stamp) return nullptr;
        if (s.key == key) return &s.value;
    }
}

int32_t* Insert(const PathGraph* pg, int high, int low, int32_t value) {
    Table& t = s_table;
    t.Own(pg);
    if (int32_t* found = Find(pg, high, low)) return found;
    // A map, never dropped for size: grow while over half full (the stress town holds
    // ~10k pairs; a pair is 16 bytes).
    if (t.bits == 0 || (t.live + 1) * 2 > static_cast<int>(t.slots.size())) t.Grow();
    const uint64_t key = Table::Key(high, low);
    const size_t mask = t.slots.size() - 1;
    size_t i = t.Home(key);
    while (t.slots[i].stamp == t.stamp) i = (i + 1) & mask;
    t.slots[i] = Slot{key, value, t.stamp};
    ++t.live;
    return &t.slots[i].value;
}

void Clear(const PathGraph* pg) {
    s_table.Own(pg);
    s_table.Drop();
}

int Count() { return s_table.live; }

}  // namespace PathCostIndex
