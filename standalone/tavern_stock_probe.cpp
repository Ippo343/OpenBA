// standalone/tavern_stock_probe.cpp
//
// "Which ale can this town actually brew right now?"
//
// The tavern stalls at "no materials" whenever the selected recipe's input does not
// exist anywhere in town, and the authored default (recipe 0, "Ale [Apple]") is exactly
// that case on an autostart map -- no orchard, so no apples, ever. Guessing a
// replacement recipe from the label is unreliable: a gatherer hut accepts FOUR wild-food
// types and only one of them is Berries, so "place a gatherer and pick Berries" is a
// hope, not a plan.
//
// This answers it by IDENTITY rather than by name. A recipe's accepted input (recipe
// entry +0x00, stride 0x10) and a StorageComponent slot's stored good (slot +0x00,
// stride 0x10) are the same kind of object -- a material description -- and
// ConsumeProduce::Execute itself matches them by POINTER COMPARISON, not by flags or
// name. So the probe walks every StorageComponent in the world, collects the distinct
// stored-good pointers, and reports for each of the seven recipes whether its input is
// among them. A recipe marked AVAILABLE is one the town can brew today.
//
// Matching by pointer also sidesteps the naming trap: an input's
// RawMaterialDescription has a BLANK sprite name at +0x38, so every input prints as "?"
// if you try to identify it that way (the readable label lives on the recipe entry at
// +0x20, against the string table at desc+0x20).
//
// Enable with BANISHED_TAVERN_STOCK=<frame interval> (default off; 1 = every 900
// frames). Diagnostic only -- it reads, it never writes.

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const int kDefaultInterval = 900;
int s_interval = -2;

void Init() {
    if (s_interval != -2) return;
    const char* e = std::getenv("BANISHED_TAVERN_STOCK");
    if (!e || !e[0] || e[0] == '0') { s_interval = -1; return; }
    const int v = std::atoi(e);
    s_interval = (v > 1) ? v : kDefaultInterval;
}

uint32_t Tid(const char* name) {
    return ::Object::GetTypeIdByName(name);
}

// Component lookup on a ComponentEntity: array at +0x30, count at +0x38.
void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const uintptr_t e = reinterpret_cast<uintptr_t>(ent);
    const int n = *reinterpret_cast<int32_t*>(e + 0x38);
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    if (!arr || n <= 0 || n > 4096) return nullptr;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* c = *slot;
        if (!c) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        const uint32_t t = (reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(c) + 0x10)))(c);
        if (t == tid) return (!fn_IsDerivedFrom || fn_IsDerivedFrom(c, tid)) ? c : nullptr;
    }
    return nullptr;
}

unsigned RawFlags(uintptr_t matDesc) {
    if (!matDesc || !g_TypeId_RawMaterialDescription) return 0;
    typedef uint32_t (*GetTypeId_t)(void*);
    void* self = reinterpret_cast<void*>(matDesc);
    const uint32_t tid = (reinterpret_cast<GetTypeId_t>(
        *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(self) + 0x10)))(self);
    if (tid == *g_TypeId_RawMaterialDescription)
        return *reinterpret_cast<unsigned*>(matDesc + 0x5c);
    const int n = *reinterpret_cast<int*>(matDesc + 0x30);
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(matDesc + 0x28);
    if (!arr || n <= 0 || n > 4096) return 0;
    for (int j = 0; j < n; ++j) {
        void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(j) * 8);
        if (!c) continue;
        const uint32_t t = (reinterpret_cast<GetTypeId_t>(
            *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(c) + 0x10)))(c);
        if (t == *g_TypeId_RawMaterialDescription)
            return *reinterpret_cast<unsigned*>(reinterpret_cast<uintptr_t>(c) + 0x5c);
    }
    return 0;
}

}  // namespace

extern "C" const char* ConsumeProduce_RecipeLabelAt(void* cpDesc, int index,
                                                    char* buf, int cap);

extern "C" void TavernStockProbe_Tick(int frameNum) {
    Init();
    if (s_interval <= 0) return;
    if (frameNum < 900 || (frameNum % s_interval) != 0) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    const uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) return;
    const uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);
    if (!arr || n <= 0) return;

    const uint32_t tidStorage = Tid("StorageComponent");
    const uint32_t tidCp      = Tid("ConsumeProduceComponent");
    if (!tidStorage) return;

    // Distinct stored-good descriptions across the whole town, with a unit tally.
    uintptr_t goods[64];
    int       counts[64];
    int       nGoods = 0;

    void* cpComp = nullptr;   // any ConsumeProduce building will do; they share a description
    void* cpStorage = nullptr;   // ...and that same building's StorageComponent, to seed

    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* ent = *slot;
        if (!ent) continue;
        if (!cpComp && tidCp) {
            cpComp = FindComp(ent, tidCp);
            if (cpComp) cpStorage = FindComp(ent, tidStorage);
        }

        void* st = FindComp(ent, tidStorage);
        if (!st) continue;
        const uintptr_t s = reinterpret_cast<uintptr_t>(st);
        const int cnt = *reinterpret_cast<int32_t*>(s + 0x30);
        const uintptr_t recs = *reinterpret_cast<uintptr_t*>(s + 0x28);
        if (!recs || cnt <= 0 || cnt > 4096) continue;
        for (int k = 0; k < cnt; ++k) {
            // slot record: +0x00 description, +0x08 capacity, +0x0c stored
            const uintptr_t g = *reinterpret_cast<uintptr_t*>(
                recs + static_cast<uintptr_t>(k) * 0x10);
            const int held = *reinterpret_cast<int32_t*>(
                recs + static_cast<uintptr_t>(k) * 0x10 + 0x0c);
            if (!g || held <= 0) continue;
            int f = -1;
            for (int q = 0; q < nGoods; ++q) if (goods[q] == g) { f = q; break; }
            if (f < 0) {
                if (nGoods >= 64) continue;
                goods[nGoods] = g; counts[nGoods] = 0; f = nGoods++;
            }
            counts[f] += held;
        }
    }

    fprintf(stderr, "[Stock] frame %d: %d distinct good(s) held in town\n", frameNum, nGoods);
    for (int q = 0; q < nGoods; ++q)
        fprintf(stderr, "[Stock]   good ptr=%p flags=0x%x units=%d\n",
                reinterpret_cast<void*>(goods[q]), RawFlags(goods[q]), counts[q]);

    if (!cpComp) { fflush(stderr); return; }
    const uintptr_t cpDesc = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(cpComp) + 0x18);
    if (!cpDesc) { fflush(stderr); return; }
    const int rc = *reinterpret_cast<int32_t*>(cpDesc + 0x30);
    const uintptr_t rbase = *reinterpret_cast<uintptr_t*>(cpDesc + 0x28);
    const int sel = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(cpComp) + 0x50);
    if (!rbase || rc <= 0 || rc > 256) { fflush(stderr); return; }

    for (int r = 0; r < rc; ++r) {
        const uintptr_t e = rbase + static_cast<uintptr_t>(r) * 0x28;
        const uintptr_t inArr = *reinterpret_cast<uintptr_t*>(e + 0x00);
        const int inCount = *reinterpret_cast<int32_t*>(e + 0x08);
        bool avail = inCount > 0;
        int units = 0;
        for (int k = 0; k < inCount && inArr; ++k) {
            const uintptr_t want = *reinterpret_cast<uintptr_t*>(
                inArr + static_cast<uintptr_t>(k) * 0x10);
            bool found = false;
            for (int q = 0; q < nGoods; ++q)
                if (goods[q] == want) { found = true; units += counts[q]; break; }
            if (!found) { avail = false; break; }
        }
        char label[128];
        ConsumeProduce_RecipeLabelAt(reinterpret_cast<void*>(cpDesc), r, label, sizeof(label));
        fprintf(stderr, "[Stock]   recipe[%d]%s %-18s %s%s\n", r,
                (r == sel ? " <==SEL" : "       "), label,
                avail ? "AVAILABLE units=" : "no input in town", "");
        if (avail) fprintf(stderr, "[Stock]        (input units held: %d)\n", units);
    }

    // BANISHED_TAVERN_SEED=<units> -- put the SELECTED recipe's input straight into the
    // tavern's own storage.
    //
    // This exists because the supply chain is not the thing under test. Measured on this
    // map: the founding town holds exactly ONE good (potato, flags 0x22, 100 units) which
    // is an input to none of the seven recipes, and by frame 1800 the citizens have eaten
    // even that -- 0 distinct goods in the whole town. So NO recipe selection can ever be
    // brewable here, and the gatherer's ten wild-food cells are a coin flip on whether any
    // of them are berries. Waiting for agriculture to deliver would be measuring the farm,
    // not the happiness goods arm.
    //
    // It uses the same AddItem (0x14012d000) that StartTown_SeedHouseInventory uses to
    // seed house food, and it hands AddItem the recipe's OWN input pointer, so what lands
    // in storage is by construction the exact object Execute pointer-compares against.
    // One shot, then it stops.
    {
        static bool s_seeded = false;
        const char* se = std::getenv("BANISHED_TAVERN_SEED");
        const int want = (se && se[0]) ? std::atoi(se) : 0;
        if (!s_seeded && want > 0 && cpStorage && sel >= 0 && sel < rc) {
            const uintptr_t e = rbase + static_cast<uintptr_t>(sel) * 0x28;
            const uintptr_t inArr = *reinterpret_cast<uintptr_t*>(e + 0x00);
            const int inCount = *reinterpret_cast<int32_t*>(e + 0x08);
            typedef void (*fn_AddItem_t)(int64_t storage, int64_t material, int n);
            fn_AddItem_t addItem = reinterpret_cast<fn_AddItem_t>(
                ResolveAddr(Module::Exe, 0x14012d000));
            if (addItem && inArr && inCount > 0) {
                const uintptr_t mat = *reinterpret_cast<uintptr_t*>(inArr);
                addItem(reinterpret_cast<int64_t>(cpStorage),
                        static_cast<int64_t>(mat), want);
                s_seeded = true;
                fprintf(stderr, "[Stock] SEED: recipe[%d] input ptr=%p flags=0x%x "
                        "-> %d units into tavern storage=%p\n",
                        sel, reinterpret_cast<void*>(mat), RawFlags(mat), want, cpStorage);
            } else {
                fprintf(stderr, "[Stock] SEED skipped: addItem=%p inArr=%p inCount=%d\n",
                        reinterpret_cast<void*>(addItem),
                        reinterpret_cast<void*>(inArr), inCount);
            }
        }
    }
    fflush(stderr);
}
