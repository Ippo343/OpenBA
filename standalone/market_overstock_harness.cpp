// standalone/market_overstock_harness.cpp
// TEST RIG ONLY — no retail function is ported here.
//
// Purpose: exercise the ONE arm of MarketWork slot 4 (0x1400eff80) that has never
// executed — stage 3's SURPLUS SHED (retail 0x1400f09eb..0x1400f0be0, our
// marketwork.cpp "gate=SURPLUS_SHED"). That arm fires only when
//
//     want * 2.0  <  inMarket * (1/capacity)              [0x140172174 = 2.0]
//
// i.e. the market holds MORE THAN TWICE its target share of one resource, and it
// then hands the excess to the hauler to carry AWAY to some other storage.
// Haulers stock the market TO its target and stop, so in a normal game the test
// can never be true — the only way to see the arm run is to put the goods there
// by force. That is all this file does.
//
// ⚠ Two ceilings have to be respected at once, or the arm still never runs:
//   1. Stage 1's STOCKED gate (0x1400f01dd): if
//      QueryCount(marketStorage)/capacity >= MarketDescription+0x20 the function
//      returns 1 long BEFORE stage 3. So the injected volume must keep the
//      market's TOTAL fill under that threshold while pushing ONE resource over
//      2x its own share. Overstocking too hard is just as invisible as not
//      overstocking at all.
//   2. inMarket is VOLUME, not item count: loop A accumulates
//      RawMaterialDescription+0x48 (unit size) * available (0x1400f04f?), so the
//      item count to add is desiredVolume / unitSize.
// The harness picks the description entry with the SMALLEST `want` precisely
// because that is the cheapest one to push over 2x while staying under (1).
//
// Injection uses the same faithful primitive the retail resource seeder uses,
// AddItem 0x14012d000 (registered) — which, as town_storage_seed.cpp records,
// checks nothing about capacity. The market's own accept mask (+0x3c) IS
// honoured here so we never park a material in a slot the market would refuse.
//
// Env:
//   BANISHED_MARKET_OVERSTOCK        enable; optional value = target satisfaction
//                                    x100 (default 260 => aim for 2.6x, clear of
//                                    the 2.0 trigger)
//   BANISHED_MARKET_OVERSTOCK_FRAME  frame to inject on (default 2400 — after the
//                                    market is built, staffed and hauling)
//   BANISHED_MARKET_OVERSTOCK_PERIOD re-inject every N frames (default 0 = one
//                                    shot). The shed arm carries the surplus off,
//                                    so a period lets the arm fire repeatedly.
//
// Trace lines are [MarketOverstock]; pair with BANISHED_MARKETBST_TRACE=1 to see
// the resulting "gate=SURPLUS_SHED" from marketwork.cpp.

#include "engine_funcs.h"
#include "resolve.h"
#include "options.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Env
// ============================================================
static int OverstockPct() {          // -1 = disabled
    static int v = -2;
    if (v == -2) {
        const char* e = Opt().marketOverstock;
        if (!e || !*e || e[0] == '0') { v = -1; }
        else { int n = atoi(e); v = (n > 100) ? n : 260; }
    }
    return v;
}
static int OverstockFrame() {
    static int v = -1;
    if (v < 0) { const char* e = Opt().marketOverstockFrame;
                 v = (e && *e) ? atoi(e) : 2400; if (v < 1) v = 1; }
    return v;
}
static int OverstockPeriod() {
    static int v = -1;
    if (v < 0) { const char* e = Opt().marketOverstockPeriod;
                 v = (e && *e) ? atoi(e) : 0; if (v < 0) v = 0; }
    return v;
}

#define MO_LOG(...)                                            \
    do { fprintf(stderr, "[MarketOverstock] " __VA_ARGS__);     \
         fprintf(stderr, "\n"); fflush(stderr); } while (0)

// ============================================================
// Type-ids — raw .rdata preferred, GetTypeIdByName fallback
// (the documented project pattern; both market ids are registered since
//  standalone/reg_market_typeid.cpp, the rest were already there)
// ============================================================
static uint32_t Tid(uintptr_t rdataAddr, const char* name) {
    const uint32_t* p =
        reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, rdataAddr));
    if (p && *p) return *p;
    return ::Object::GetTypeIdByName(name);
}

static inline uint32_t GetTypeIdOf(void* obj) {
    typedef uint32_t (*fn_GetTypeId_t)(void*);
    return (*reinterpret_cast<fn_GetTypeId_t*>(*reinterpret_cast<uintptr_t*>(obj) + 0x10))(obj);
}

// Component scan over a LIVE entity (+0x30 IBP array / +0x38 count).
static void* FindComponent(int64_t entity, uint32_t tid) {
    if (!entity || !tid) return nullptr;
    const int count = *reinterpret_cast<int*>(entity + 0x38);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(
            *reinterpret_cast<int64_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
        void* comp = *slot;
        if (!comp) continue;
        if (GetTypeIdOf(comp) == tid)
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, tid)) ? comp : nullptr;
    }
    return nullptr;
}

// Description scan over a TEMPLATE entity (+0x28 raw array / +0x30 count).
static void* FindDescOnTemplate(int64_t tmpl, uint32_t tid) {
    if (!tmpl || !tid || !fn_IsDerivedFrom) return nullptr;
    const int count = *reinterpret_cast<int*>(tmpl + 0x30);
    const int64_t arr = *reinterpret_cast<int64_t*>(tmpl + 0x28);
    for (int i = 0; i < count; ++i) {
        void* d = *reinterpret_cast<void**>(arr + static_cast<int64_t>(i) * 8);
        if (d && GetTypeIdOf(d) == tid)
            return fn_IsDerivedFrom(d, tid) ? d : nullptr;
    }
    return nullptr;
}

// StorageComponent::QueryCount (0x14012cf10) — volume-weighted total.
static int QueryCount(int64_t sc, uint32_t tidRawMat) {
    int total = 0;
    const int slots = *reinterpret_cast<int*>(sc + 0x30);
    const int64_t arr = *reinterpret_cast<int64_t*>(sc + 0x28);
    for (int i = 0; i < slots; ++i) {
        const int64_t slot = arr + static_cast<int64_t>(i) * 0x10;
        void* d = FindDescOnTemplate(*reinterpret_cast<int64_t*>(slot), tidRawMat);
        const int unitVol = d ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(d) + 0x48) : 0;
        total += unitVol * *reinterpret_cast<int*>(slot + 0x08);
    }
    return total;
}

// The same accumulation stage 2's LOOP A performs for one record mask:
// sum of unitSize * (slot total - slot reserved) over the market's own slots.
static int InMarketVolumeForMask(int64_t sc, uint32_t mask, uint32_t tidRawMat) {
    int total = 0;
    const int slots = *reinterpret_cast<int*>(sc + 0x30);
    const int64_t arr = *reinterpret_cast<int64_t*>(sc + 0x28);
    for (int i = 0; i < slots; ++i) {
        const int64_t slot = arr + static_cast<int64_t>(i) * 0x10;
        void* d = FindDescOnTemplate(*reinterpret_cast<int64_t*>(slot), tidRawMat);
        if (!d) continue;
        const uintptr_t dp = reinterpret_cast<uintptr_t>(d);
        if ((*reinterpret_cast<uint32_t*>(dp + 0x5c) & mask) == 0) continue;
        const int available = *reinterpret_cast<int*>(slot + 0x08) -
                              *reinterpret_cast<int*>(slot + 0x0c);
        total += *reinterpret_cast<int*>(dp + 0x48) * available;
    }
    return total;
}

static inline void* TownMgr() {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return nullptr;
    return *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0);
}

// ============================================================
// The injection
// ============================================================
static void DoOverstock(int frameNum) {
    typedef void (*fn_AddItem_t)(uintptr_t storage, int64_t material, int n);  // 0x14012d000
    static fn_AddItem_t addItem = nullptr;
    if (!addItem)
        addItem = reinterpret_cast<fn_AddItem_t>(ResolveAddr(Module::Exe, 0x14012d000));
    if (!addItem) { MO_LOG("AddItem(0x14012d000) unresolved — nothing injected"); return; }

    uintptr_t town = reinterpret_cast<uintptr_t>(TownMgr());
    if (!town) { MO_LOG("no town manager yet (frame %d)", frameNum); return; }

    const uint32_t tidMarketComp   = Tid(0x1401c44f0, "MarketComponent");
    const uint32_t tidMarketDesc   = Tid(0x1401c4528, "MarketDescription");
    const uint32_t tidStorageComp  = Tid(0x1401c7e78, "StorageComponent");
    const uint32_t tidRawMat       = Tid(0x1401c7cb8, "RawMaterialDescription");
    if (!tidMarketComp || !tidMarketDesc || !tidStorageComp || !tidRawMat) {
        MO_LOG("type-id resolve failed (market=%u desc=%u storage=%u rawmat=%u)",
               tidMarketComp, tidMarketDesc, tidStorageComp, tidRawMat);
        return;
    }

    // ---- find the first market in the WORLD entity list (gw+0xe8: array +0x08,
    // int32 count +0x10) ----------------------------------------------------
    // ⚠ NOT townMgr+0x00. That list is the STORAGE-LOCATION registry (see
    // town_storage_seed.cpp); a market is not in it, which is exactly why the
    // first cut of this harness reported "no MarketComponent in town" while the
    // MarketBST trace showed slot 4 servicing that very market on the same frame.
    int64_t marketEntity = 0;
    void* marketComp = nullptr;
    {
        const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
        const uintptr_t el = *reinterpret_cast<uintptr_t*>(gw + 0xe8);
        const uintptr_t arr = el ? *reinterpret_cast<uintptr_t*>(el + 0x08) : 0;
        const int entCount = el ? *reinterpret_cast<int*>(el + 0x10) : 0;
        for (int e = 0; e < entCount && arr; ++e) {
            void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(e) * 8);
            int64_t ent = reinterpret_cast<int64_t>(*slot);
            if (!ent) continue;
            void* mc = FindComponent(ent, tidMarketComp);
            if (mc) { marketEntity = ent; marketComp = mc; break; }
        }
    }
    if (!marketComp) { MO_LOG("no MarketComponent in the world entity list (frame %d)", frameNum); return; }

    void* descRaw = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(marketComp) + 0x18);
    void* marketDesc = (descRaw && fn_IsDerivedFrom &&
                        fn_IsDerivedFrom(descRaw, tidMarketDesc)) ? descRaw : nullptr;
    void* storageComp = FindComponent(marketEntity, tidStorageComp);
    if (!marketDesc || !storageComp) {
        MO_LOG("market %p incomplete (desc=%p storage=%p)",
               reinterpret_cast<void*>(marketEntity), marketDesc, storageComp);
        return;
    }
    const uintptr_t dp = reinterpret_cast<uintptr_t>(marketDesc);
    const int64_t   sc = reinterpret_cast<int64_t>(storageComp);

    const int   capacity  = *reinterpret_cast<int*>(sc + 0x38);
    const float threshold = *reinterpret_cast<float*>(dp + 0x20);   // stage 1's STOCKED gate
    const uint32_t acceptMask = *reinterpret_cast<uint32_t*>(sc + 0x3c);
    const int   entryCount = *reinterpret_cast<int*>(dp + 0x30);
    const uintptr_t entries = *reinterpret_cast<uintptr_t*>(dp + 0x28);
    if (capacity <= 0 || entryCount <= 0) {
        MO_LOG("market %p capacity=%d entries=%d — cannot overstock",
               reinterpret_cast<void*>(marketEntity), capacity, entryCount);
        return;
    }

    const int  currentTotal = QueryCount(sc, tidRawMat);
    const float targetSat   = static_cast<float>(OverstockPct()) / 100.0f;
    // Stage 1 declines at fill >= threshold, so this is the volume head-room the
    // injection must stay inside. One volume unit of slack keeps the compare strict.
    const int  headroom = static_cast<int>(threshold * static_cast<float>(capacity))
                        - currentTotal - 1;

    MO_LOG("frame %d market=%p storage=%p capacity=%d filled=%d fillThreshold=%.3f "
           "headroom=%d acceptMask=0x%08x entries=%d targetSat=%.2f",
           frameNum, reinterpret_cast<void*>(marketEntity), storageComp,
           capacity, currentTotal, threshold, headroom, acceptMask, entryCount, targetSat);
    if (headroom <= 0) {
        MO_LOG("market already at/over stage 1's STOCKED gate — slot 4 returns 1 before "
               "stage 3 ever runs; nothing injected");
        return;
    }

    // ---- choose the entry: smallest non-zero want that we can actually push ----
    int   bestEntry = -1;
    float bestWant  = 0.0f;
    int   bestNeed  = 0;      // volume still to add
    for (int r = 0; r < entryCount; ++r) {
        const uint32_t mask = *reinterpret_cast<uint32_t*>(entries + static_cast<int64_t>(r) * 8);
        const float    want = *reinterpret_cast<float*>(entries + static_cast<int64_t>(r) * 8 + 4);
        if (!(want > 0.0f)) continue;
        if ((mask & acceptMask) == 0) continue;   // the market would refuse the goods
        const int have = InMarketVolumeForMask(sc, mask, tidRawMat);
        const int need = static_cast<int>(targetSat * want * static_cast<float>(capacity)) - have;
        if (need <= 0) continue;                  // already past target for this one
        if (need > headroom) continue;            // would trip stage 1's gate instead
        if (bestEntry < 0 || want < bestWant) { bestEntry = r; bestWant = want; bestNeed = need; }
    }
    if (bestEntry < 0) {
        MO_LOG("no description entry can be pushed over %.2fx want within the %d volume "
               "of headroom — raise BANISHED_MARKET_OVERSTOCK? nothing injected", targetSat, headroom);
        return;
    }
    const uint32_t mask = *reinterpret_cast<uint32_t*>(
        entries + static_cast<int64_t>(bestEntry) * 8);

    // ---- pick a material template carrying that mask (town+0x110 / +0x118) ----
    int64_t tmpl = 0;
    int unitSize = 0;
    {
        const int tc = *reinterpret_cast<int*>(town + 0x118);
        const int64_t ta = *reinterpret_cast<int64_t*>(town + 0x110);
        for (int t = 0; t < tc; ++t) {
            const int64_t cand = *reinterpret_cast<int64_t*>(ta + static_cast<int64_t>(t) * 8);
            void* d = FindDescOnTemplate(cand, tidRawMat);
            if (!d) continue;
            const uintptr_t dd = reinterpret_cast<uintptr_t>(d);
            if ((*reinterpret_cast<uint32_t*>(dd + 0x5c) & mask) == 0) continue;
            const int us = *reinterpret_cast<int*>(dd + 0x48);
            if (us <= 0) continue;
            tmpl = cand; unitSize = us; break;
        }
    }
    if (!tmpl) {
        MO_LOG("entry %d mask=0x%08x want=%.4f: no raw-material template carries that mask; "
               "nothing injected", bestEntry, mask, bestWant);
        return;
    }

    const int units = (bestNeed + unitSize - 1) / unitSize;   // round UP past the trigger
    addItem(reinterpret_cast<uintptr_t>(storageComp), tmpl, units);

    const int afterTotal = QueryCount(sc, tidRawMat);
    const int afterMask  = InMarketVolumeForMask(sc, mask, tidRawMat);
    const float sat  = static_cast<float>(afterMask) / (static_cast<float>(capacity) * bestWant);
    const float fill = static_cast<float>(afterTotal) / static_cast<float>(capacity);
    MO_LOG("INJECTED entry=%d mask=0x%08x want=%.4f unitSize=%d units=%d volume=%d "
           "-> inMarket=%d satisfaction=%.3f (trigger >2.0) | fill=%.3f (gate %.3f) %s",
           bestEntry, mask, bestWant, unitSize, units, units * unitSize,
           afterMask, sat, fill, threshold,
           (sat > 2.0f && fill < threshold) ? "ARMED" : "NOT ARMED — see the two numbers above");
}

// ============================================================
// Tick — called from GameUpdateLoop::Update
// ============================================================
extern "C" void MarketOverstock_Tick(int frameNum) {
    if (OverstockPct() < 0) return;
    const int first = OverstockFrame();
    if (frameNum < first) return;
    const int period = OverstockPeriod();
    if (frameNum == first) { DoOverstock(frameNum); return; }
    if (period > 0 && ((frameNum - first) % period) == 0) DoOverstock(frameNum);
}
