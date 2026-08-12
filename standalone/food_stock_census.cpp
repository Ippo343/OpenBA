// standalone/food_stock_census.cpp
// BANISHED_FOOD_STOCK — periodic town-stock census, the generic form of the meter that
// proved the crop field's deposit.
//
// WHY: every food class in this wave ends the same way — a raw material has to appear in
// TOWN STOCK. Each class was being judged by a different proxy (slots reaching a state, a
// work Execute running, an action trace firing), and every one of those proxies produced
// at least one wrong conclusion this wave: an absent [ORGANIC-PRODUCE] line was read as
// "nothing is produced" when the producer simply took a different code path, and slot
// state 2 was read as "harvested" when it is the stale-slot drop. Stock is the outcome
// itself, so it cannot be wrong about whether food arrived.
//
// The table is the same one HarvestArea's own capacity test reads:
//   gameWorld+0xf0 -> +0x170 = entry array, stride 0x20, entry+0x00 = current stock,
//   entry+0x08 = limit.
// Only entries whose stock CHANGED since the last dump are printed, with the delta, so a
// long soak stays readable and a class that deposits nothing is visibly silent rather
// than lost in noise.
//
// Value: 1/on = dump every 600 frames, or a frame count. Test-only, opt-in.

#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" void** g_pGameWorldSingleton;

namespace {

constexpr int kMaxMaterials = 64;

bool s_init   = false;
int  s_period = 0;
int  s_prev[kMaxMaterials];
bool s_seeded = false;

void Init() {
    if (s_init) return;
    s_init = true;
    const char* e = std::getenv("BANISHED_FOOD_STOCK");
    if (!e || !e[0] || std::strcmp(e, "0") == 0) return;
    s_period = std::atoi(e);
    if (s_period <= 1) s_period = 600;
    std::fprintf(stderr, "[FoodStock] ENABLED — town stock census every %d frames\n", s_period);
    std::fflush(stderr);
}

}  // namespace

extern "C" void FoodStockCensus_Tick(int frame) {
    Init();
    if (!s_period || (frame % s_period) != 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t tracker = *reinterpret_cast<uintptr_t*>(gw + 0xf0);
    if (!tracker) return;
    const uintptr_t table = *reinterpret_cast<uintptr_t*>(tracker + 0x170);
    if (!table) return;

    int changed = 0;
    for (int i = 0; i < kMaxMaterials; ++i) {
        const uintptr_t e = table + static_cast<uintptr_t>(i) * 0x20;
        const int cur = *reinterpret_cast<int32_t*>(e);
        const int lim = *reinterpret_cast<int32_t*>(e + 8);
        // Reject entries that are not real materials. Scanning a fixed 64 walks off the
        // end of the table and the tail decodes as nonsense (measured: limits like
        // -2100900192 "rising" in lockstep), which would read as a dozen materials
        // depositing at once. A real entry has a sane positive limit and a stock within
        // it. Bound by the DATA rather than by a guessed count.
        if (lim <= 0 || lim > 1000000) { s_prev[i] = cur; continue; }
        if (cur < 0 || cur > lim) { s_prev[i] = cur; continue; }
        if (!s_seeded) { s_prev[i] = cur; continue; }
        if (cur != s_prev[i]) {
            std::fprintf(stderr, "[FoodStock] material %2d: %d -> %d  (%+d, limit %d)\n",
                         i, s_prev[i], cur, cur - s_prev[i], lim);
            s_prev[i] = cur;
            ++changed;
        }
    }
    if (!s_seeded) {
        s_seeded = true;
        std::fprintf(stderr, "[FoodStock] baseline captured\n");
    } else if (!changed) {
        // Print in BOTH arms: an empty census must never be mistaken for "not running".
        std::fprintf(stderr, "[FoodStock] no material changed this interval\n");
    }
    std::fflush(stderr);
}
