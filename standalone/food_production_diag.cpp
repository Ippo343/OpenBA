// standalone/food_production_diag.cpp
// Behavioural-equivalence probe for the six food-production classes.
//
// BANISHED_FOOD_DIAG=<periodFrames>  (0/unset = off, "1" = default 600)
//
// Every period it prints, for each of CropFieldComponent / OrchardComponent /
// PastureComponent / FishingComponent / HuntComponent / HerbalistComponent:
//   * how many instances exist,
//   * each one's state-machine state id and its workplace staffing/accepting bits,
//   * for the fishing dock, its computed rate (+0x54) and registered latch (+0x50),
// and then the TOWN STORAGE totals broken down by resource type.
//
// The storage total is the only number that answers "does this building feed the
// town". Counting works posted, or piles on the ground, does not: a work nobody
// claims and a pile nobody hauls are both a yield of zero. This deliberately reports
// zero counts too — a class with no instances prints "0 instance(s)" rather than
// nothing, so a silent section can never be mistaken for a passing one.
//
// HARNESS ONLY: reads published fields, writes nothing. Default off.

#include "engine_funcs.h"
#include "resolve.h"
#include "options.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Component/CropFieldComponent.h"
#include "Component/FishingComponent.h"
#include "Component/HerbalistComponent.h"
#include "Component/HerbalistDescription.h"
#include "Component/HuntComponent.h"
#include "Component/MapComponent.h"
#include "Component/OrchardComponent.h"
#include "Component/PastureComponent.h"
#include "Component/RadiusDescription.h"
#include "Component/StateMachineComponent.h"
#include "Component/StorageComponent.h"
#include "Component/WorkPlaceComponent.h"

namespace {

int  s_period  = 0;
bool s_init    = false;

void Init() {
    if (s_init) return;
    s_init = true;
    const char* e = Opt().foodDiag;
    if (!e || !e[0] || std::strcmp(e, "0") == 0) { s_period = 0; return; }
    s_period = std::atoi(e);
    if (s_period <= 1) s_period = 600;
    std::fprintf(stderr, "[FoodDiag] ENABLED — dumping every %d frames\n", s_period);
    std::fflush(stderr);
}

struct ClassRow {
    const char* name;
    uint32_t    tid;
    int         count;
};

// The component's embedded state node lives at comp+0x28 for every class in this
// wave (ControllerComponent layout); the owning StateMachine carries the current
// state id in its +0x0a byte. We read the StateMachineComponent instead, which is
// where the id is actually published — comp+0x28 is only the node.
// Find a component of type `tid` on an entity (the +0x30 array / +0x38 count pair).
void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        void* c = *cs;
        if (!c) continue;
        if (reinterpret_cast<Object::Base*>(c)->Type() == tid) return c;   // B.3.8 bridge
    }
    return nullptr;
}

void DumpStorage(int frame, uintptr_t arr, int n) {
    const uint32_t tidStorage = StorageComponent::TypeId();
    if (!tidStorage || !arr || n <= 0) {
        std::fprintf(stderr, "[FoodDiag] frame=%d storage: CANNOT SCAN "
                     "(tid=%u entities=%d)\n", frame, tidStorage, n);
        return;
    }
    struct Stack { uintptr_t rt; long long ct; };
    Stack byType[32] = {};
    int types = 0, storages = 0, nonEmpty = 0;
    long long grand = 0;

    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;
        const int cN = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
        uintptr_t cA = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
        for (int c = 0; c < cN && cA; ++c) {
            void** cs = reinterpret_cast<void**>(cA + static_cast<uintptr_t>(c) * 8);
            void* comp = *cs;
            if (!comp) continue;
            if (reinterpret_cast<Object::Base*>(comp)->Type() != tidStorage) continue;   // B.3.8 bridge
            ++storages;
            uintptr_t st = reinterpret_cast<uintptr_t>(comp);
            const int stacks = *reinterpret_cast<int32_t*>(st + 0x30);
            uintptr_t inv = *reinterpret_cast<uintptr_t*>(st + 0x28);
            if (stacks > 0) ++nonEmpty;
            for (int k = 0; inv && k < stacks; ++k) {
                uintptr_t e = inv + static_cast<uintptr_t>(k) * 0x10;
                uintptr_t rt = *reinterpret_cast<uintptr_t*>(e);
                const int ct = *reinterpret_cast<int32_t*>(e + 8);
                grand += ct;
                int f = -1;
                for (int t = 0; t < types; ++t) if (byType[t].rt == rt) { f = t; break; }
                if (f < 0 && types < 32) { f = types++; byType[f].rt = rt; byType[f].ct = 0; }
                if (f >= 0) byType[f].ct += ct;
            }
        }
    }
    std::fprintf(stderr, "[FoodDiag] frame=%d storage: %d component(s), %d holding, "
                 "%d type(s), TOTAL ITEMS=%lld\n",
                 frame, storages, nonEmpty, types, grand);
    for (int t = 0; t < types; ++t)
        std::fprintf(stderr, "[FoodDiag]     resType=%p count=%lld\n",
                     reinterpret_cast<void*>(byType[t].rt), byType[t].ct);
}

void DumpAll(int frame) {
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) {
        std::fprintf(stderr, "[FoodDiag] frame=%d: gameWorld null\n", frame);
        return;
    }
    uintptr_t entityList = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!entityList) {
        std::fprintf(stderr, "[FoodDiag] frame=%d: EntityList (gw+0xe8) null\n", frame);
        return;
    }
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entityList + 0x08);
    int       n   = *reinterpret_cast<int32_t*>(entityList + 0x10);

    ClassRow rows[] = {
        { "CropFieldComponent", CropFieldComponent::TypeId(), 0 },
        { "OrchardComponent",   OrchardComponent::TypeId(),   0 },
        { "PastureComponent",   PastureComponent::TypeId(),   0 },
        { "FishingComponent",   FishingComponent::TypeId(),   0 },
        { "HuntComponent",      HuntComponent::TypeId(),      0 },
        { "HerbalistComponent", HerbalistComponent::TypeId(), 0 },
    };
    const int nRows = (int)(sizeof(rows) / sizeof(rows[0]));
    const uint32_t tidSM  = StateMachineComponent::TypeId();
    const uint32_t tidWPC = WorkPlaceComponent::TypeId();

    for (int i = 0; i < n && arr; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* ent = *slot;
        if (!ent) continue;

        for (int r = 0; r < nRows; ++r) {
            if (!rows[r].tid) continue;
            void* comp = FindComp(ent, rows[r].tid);
            if (!comp) continue;
            ++rows[r].count;

            // StateMachineComponent: the live state id is the +0x51 byte of the
            // component (the node-local index the orchard panel path already reads).
            int stateId = -1;
            if (void* smc = FindComp(ent, tidSM))
                stateId = *reinterpret_cast<int8_t*>(reinterpret_cast<uintptr_t>(smc) + 0x51);

            int staffed = -1, accepting = -1;
            if (void* wpc = FindComp(ent, tidWPC)) {
                uintptr_t w = reinterpret_cast<uintptr_t>(wpc);
                staffed   = *reinterpret_cast<int32_t*>(w + 0x60);
                accepting = *reinterpret_cast<uint8_t*>(w + 0x50);
            }

            std::fprintf(stderr, "[FoodDiag] frame=%d %-19s #%d comp=%p smStateIdx=%d "
                         "staffed=%d accepting=%d",
                         frame, rows[r].name, rows[r].count, comp, stateId, staffed, accepting);
            if (std::strcmp(rows[r].name, "FishingComponent") == 0) {
                uintptr_t f = reinterpret_cast<uintptr_t>(comp);
                std::fprintf(stderr, " registered(+0x50)=%u rate(+0x54)=%.3f",
                             (unsigned)*reinterpret_cast<uint8_t*>(f + 0x50),
                             *reinterpret_cast<float*>(f + 0x54));
            }
            // Gatherer's Hut and Hunter Lodge both search their RADIUS for something
            // that already exists in the world: HerbalistWork::Slot4 walks random cells
            // in radius looking for a NaturalResource entity (cell+8) whose description
            // is in the hut's herb list and whose growth clears the threshold; the hunt
            // work looks for game. A building on cleared ground therefore executes its
            // work, finds nothing, and produces nothing — which is indistinguishable
            // from a broken port unless the resource count in range is reported.
            if (std::strcmp(rows[r].name, "HerbalistComponent") == 0 ||
                std::strcmp(rows[r].name, "HuntComponent") == 0) {
                int resInRange = -1;
                void* gwp = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                          ? *g_pGameWorldSingleton : nullptr;
                uint8_t* mapObj = gwp ? *reinterpret_cast<uint8_t**>(
                    reinterpret_cast<uintptr_t>(gwp) + 0xf8) : nullptr;
                uint8_t* grid = mapObj ? *reinterpret_cast<uint8_t**>(mapObj + 0x80) : nullptr;
                const int mw = mapObj ? *reinterpret_cast<int32_t*>(mapObj) : 0;
                // Radius lives on the RadiusDescription in the entity's description
                // list (entity+0x28), at +0x28 — the same read the work does.
                int radius = 0;
                const uint32_t tidRadius = RadiusDescription::TypeId();
                uintptr_t dlist = *reinterpret_cast<uintptr_t*>(
                    static_cast<uint8_t*>(ent) + 0x28);
                if (dlist && tidRadius) {
                    const int dn = *reinterpret_cast<int32_t*>(dlist + 0x30);
                    void** da = *reinterpret_cast<void***>(dlist + 0x28);
                    for (int d = 0; d < dn && da; ++d)
                        if (da[d] && fn_IsDerivedFrom && fn_IsDerivedFrom(da[d], tidRadius)) {
                            radius = *reinterpret_cast<int32_t*>(
                                reinterpret_cast<uintptr_t>(da[d]) + 0x28);
                            break;
                        }
                }
                if (grid && mw > 0 && radius > 0) {
                    // Building tile centre, as the works compute it.
                    const uint32_t tidMap = MapComponent::TypeId();
                    void* mc = FindComp(ent, tidMap);
                    if (mc) {
                        uintptr_t m = reinterpret_cast<uintptr_t>(mc);
                        const int cx = (*reinterpret_cast<int16_t*>(m + 0x2c) >> 1) +
                                        *reinterpret_cast<int16_t*>(m + 0x28);
                        const int cy = (*reinterpret_cast<int16_t*>(m + 0x2e) >> 1) +
                                        *reinterpret_cast<int16_t*>(m + 0x2a);
                        resInRange = 0;
                        for (int dy = -radius; dy < radius; ++dy)
                            for (int dx = -radius; dx < radius; ++dx) {
                                if (dx * dx + dy * dy > radius * radius) continue;
                                const int x = cx + dx, y = cy + dy;
                                if (x < 0 || y < 0 || x >= mw || y >= mw) continue;
                                if (*reinterpret_cast<uintptr_t*>(
                                        grid + (size_t)(y * mw + x) * 0x30 + 8) != 0)
                                    ++resInRange;
                            }
                    }
                }
                std::fprintf(stderr, " radius=%d resourceCellsInRange=%d", radius, resInRange);

                // resourceCellsInRange counts ANY entity parked in a cell — overwhelmingly
                // TREES. HerbalistWork::Slot4 only accepts a resource whose entity+0x28
                // matches an entry in the hut's own accept list (HerbalistDescription+0x20,
                // count +0x28), so the all-resources number says nothing about whether the
                // hut can forage. Count the MATCHING ones — that is the number the work
                // actually gates on, and the one worth siting the building by.
                if (std::strcmp(rows[r].name, "HerbalistComponent") == 0 &&
                    grid && mw > 0 && radius > 0) {
                    // Retail resolves the list off the HERBALIST component's own
                    // description (comp+0x18), NOT the WorkPlaceComponent's — the work's
                    // finder uses type id 0x1401c43a0, which is HerbalistComponent despite
                    // the port naming its local `s_typeId_WorkplaceComponent`. `comp` here
                    // is already that component.
                    const uint32_t tidHerbDesc = HerbalistDescription::TypeId();
                    void* hdesc = nullptr;
                    {
                        void* d = *reinterpret_cast<void**>(
                            reinterpret_cast<uintptr_t>(comp) + 0x18);
                        if (d && tidHerbDesc && fn_IsDerivedFrom &&
                            fn_IsDerivedFrom(d, tidHerbDesc)) hdesc = d;
                    }
                    int accepted = -1, listN = 0;
                    if (hdesc) {
                        uintptr_t hd = reinterpret_cast<uintptr_t>(hdesc);
                        listN = *reinterpret_cast<int32_t*>(hd + 0x28);
                        uintptr_t* list = *reinterpret_cast<uintptr_t**>(hd + 0x20);
                        const uint32_t tidMap2 = MapComponent::TypeId();
                        void* mc2 = FindComp(ent, tidMap2);
                        if (list && listN > 0 && mc2) {
                            uintptr_t m2 = reinterpret_cast<uintptr_t>(mc2);
                            const int cx = (*reinterpret_cast<int16_t*>(m2 + 0x2c) >> 1) +
                                            *reinterpret_cast<int16_t*>(m2 + 0x28);
                            const int cy = (*reinterpret_cast<int16_t*>(m2 + 0x2e) >> 1) +
                                            *reinterpret_cast<int16_t*>(m2 + 0x2a);
                            accepted = 0;
                            for (int dy = -radius; dy < radius; ++dy)
                                for (int dx = -radius; dx < radius; ++dx) {
                                    if (dx * dx + dy * dy > radius * radius) continue;
                                    const int x = cx + dx, y = cy + dy;
                                    if (x < 0 || y < 0 || x >= mw || y >= mw) continue;
                                    uintptr_t re = *reinterpret_cast<uintptr_t*>(
                                        grid + (size_t)(y * mw + x) * 0x30 + 8);
                                    if (!re) continue;
                                    const uintptr_t rd = *reinterpret_cast<uintptr_t*>(re + 0x28);
                                    for (int t = 0; t < listN; ++t)
                                        if (list[t] == rd) { ++accepted; break; }
                                }
                        }
                    }
                    std::fprintf(stderr, " acceptList=%d GATHERABLE=%d", listN, accepted);
                }
            }
            if (std::strcmp(rows[r].name, "HuntComponent") == 0) {
                uintptr_t h = reinterpret_cast<uintptr_t>(comp);
                std::fprintf(stderr, " progress(+0x50)=%lld",
                             (long long)*reinterpret_cast<int64_t*>(h + 0x50));
            }
            std::fprintf(stderr, "\n");
        }
    }

    // Report the empty classes too — "no line" must never read as "fine".
    for (int r = 0; r < nRows; ++r)
        if (rows[r].count == 0)
            std::fprintf(stderr, "[FoodDiag] frame=%d %-19s 0 instance(s)%s\n",
                         frame, rows[r].name,
                         rows[r].tid ? "" : "  (TYPE ID UNRESOLVED)");

    DumpStorage(frame, arr, n);
    std::fflush(stderr);
}

}  // namespace

extern "C" void FoodDiag_Tick(int frameNum) {
    Init();
    if (s_period <= 0) return;
    if (frameNum <= 0 || (frameNum % s_period) != 0) return;
    DumpAll(frameNum);
}
