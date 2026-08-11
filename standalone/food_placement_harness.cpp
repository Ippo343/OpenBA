// standalone/food_placement_harness.cpp
// BANISHED_FOOD_PLACE=<class>|all — placement + SLOT-COVERAGE harness for the six
// food-production classes. The counterpart to BANISHED_FOOD_PANEL (which covers the
// dialog side); this one covers the building side.
//
// It answers two questions the demo spawners alone cannot:
//
//  1. DID THE BUILDING LAND CORRECTLY? entity, tile, footprint, whether it carries a
//     StateMachineComponent and a WorkPlaceComponent, its live state index, staffing
//     and accepting flag.
//
//  2. WHICH VTABLE SLOTS ARE ACTUALLY LIVE? Every one of these components is created
//     through MakeInertTyped, which fills all 48 slots with one shared stub and then
//     lets InstallLive overwrite the handful we have ported. So a slot still holding
//     the stub address is a slot that is NOT reimplemented, no matter what the source
//     tree suggests. Comparing each slot against g_InertComponentVtable turns "every
//     slot decompiled and reimplemented" from a claim into a measurement, per class,
//     printed as a map.
//
// Retail's component vtables for this family are 22 slots wide (0..21) with a real body
// in every one — the RTTI/type-name string starts immediately after slot 21, which is
// how the width was established. So the target is 22/22 live; anything less is the
// remaining work, and the map says exactly which indices.
//
// Known slot meanings for this family (recovered this wave):
//   0..3   dtor family + Shutdown          13  CanCommand / command mask
//   9      Update (Hunt only, ctor bit 8)  14  Command
//   16     GetDefaultState                 15  OnMessage
//   17     CreateState                     18  CreateWork (pure virtual in the RP base)
//   19/20  OnEnter/OnExitProducing         21  ProduceTick
//
// Test-only, opt-in, default off. Fires once at a settle frame
// (BANISHED_FOOD_PLACE_FRAME, default 900).

#include "resolve.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>


namespace {

struct FoodClass {
    const char* key;
    const char* component;
    const char* demoEnv;     // the spawner that puts one in the world
};

const FoodClass kClasses[] = {
    { "cropfield", "CropFieldComponent", "BANISHED_BUILD_DEMO_CROPFIELD"   },
    { "orchard",   "OrchardComponent",   "BANISHED_BUILD_DEMO_ORCHARD"     },
    { "pasture",   "PastureComponent",   "BANISHED_BUILD_DEMO_PASTURE"     },
    { "fishing",   "FishingComponent",   "BANISHED_BUILD_DEMO_FISHINGDOCK" },
    { "hunting",   "HuntComponent",      "BANISHED_BUILD_DEMO_HUNTERLODGE" },
    { "gatherer",  "HerbalistComponent", "BANISHED_BUILD_DEMO_GATHERERHUT" },
};
const int kClassCount = (int)(sizeof(kClasses) / sizeof(kClasses[0]));
const int kSlots = 22;      // retail width for this component family

bool s_init = false, s_fired = false;
char s_want[32] = {0};
int  s_fireFrame = 900;

void Init() {
    if (s_init) return;
    s_init = true;
    const char* e = std::getenv("BANISHED_FOOD_PLACE");
    if (!e || !e[0] || std::strcmp(e, "0") == 0) return;
    std::snprintf(s_want, sizeof(s_want), "%s", e);
    if (const char* f = std::getenv("BANISHED_FOOD_PLACE_FRAME")) {
        int v = std::atoi(f);
        if (v > 0) s_fireFrame = v;
    }
    std::fprintf(stderr, "[FoodPlace] ENABLED target=\"%s\" fireFrame=%d\n", s_want, s_fireFrame);
    std::fflush(stderr);
}

uint32_t Tid(const char* n) { return n ? ::Object::GetTypeIdByName(n) : 0; }

void* FindComp(void* ent, uint32_t tid) {
    if (!ent || !tid) return nullptr;
    const int n = *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ent) + 0x38);
    uintptr_t a = *reinterpret_cast<uintptr_t*>(static_cast<uint8_t*>(ent) + 0x30);
    for (int i = 0; i < n && a; ++i) {
        void** cs = reinterpret_cast<void**>(a + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(cs);
        void* c = *cs;
        if (!c) continue;
        void** vt = *reinterpret_cast<void***>(c);
        if (!vt || !vt[2]) continue;
        if (reinterpret_cast<uint32_t(*)(void*)>(vt[2])(c) == tid) return c;
    }
    return nullptr;
}

void ProbeClass(const FoodClass& fc) {
    const uint32_t tid = Tid(fc.component);
    if (!tid) {
        std::fprintf(stderr, "[FoodPlace] %-19s TYPE ID UNRESOLVED\n", fc.component);
        return;
    }
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) return;
    uintptr_t el = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(gw) + 0xe8);
    if (!el) return;
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(el + 0x08);
    const int n = *reinterpret_cast<int32_t*>(el + 0x10);

    int instances = 0;
    void* ent = nullptr; void* comp = nullptr;
    for (int i = 0; i < n && arr; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* e = *slot;
        if (!e) continue;
        if (void* c = FindComp(e, tid)) {
            ++instances;
            if (!ent) { ent = e; comp = c; }
        }
    }
    if (!ent) {
        std::fprintf(stderr, "[FoodPlace] %-19s NOT PLACED — 0 instance(s); set %s=1\n",
                     fc.component, fc.demoEnv);
        return;
    }

    // ── placement facts ───────────────────────────────────────────────────────────
    int tx = -1, ty = -1, fw = -1, fh = -1;
    if (void* mc = FindComp(ent, Tid("MapComponent"))) {
        uintptr_t m = reinterpret_cast<uintptr_t>(mc);
        tx = *reinterpret_cast<int16_t*>(m + 0x28);
        ty = *reinterpret_cast<int16_t*>(m + 0x2a);
        fw = *reinterpret_cast<int16_t*>(m + 0x2c);
        fh = *reinterpret_cast<int16_t*>(m + 0x2e);
    }
    int state = -1;
    const bool hasSM = (FindComp(ent, Tid("StateMachineComponent")) != nullptr);
    if (void* smc = FindComp(ent, Tid("StateMachineComponent")))
        state = *reinterpret_cast<int8_t*>(reinterpret_cast<uintptr_t>(smc) + 0x51);
    int staffed = -1, accepting = -1;
    if (void* wpc = FindComp(ent, Tid("WorkPlaceComponent"))) {
        uintptr_t w = reinterpret_cast<uintptr_t>(wpc);
        staffed   = *reinterpret_cast<int32_t*>(w + 0x60);
        accepting = *reinterpret_cast<uint8_t*>(w + 0x50);
    }

    std::fprintf(stderr, "[FoodPlace] %-19s PLACED x%d  ent=%p comp=%p tile=(%d,%d) fp=%dx%d "
                 "SM=%s stateIdx=%d staffed=%d accepting=%d\n",
                 fc.component, instances, ent, comp, tx, ty, fw, fh,
                 hasSM ? "yes" : "NO", state, staffed, accepting);

    // ── slot coverage ─────────────────────────────────────────────────────────────
    // A slot still equal to the shared inert stub is NOT reimplemented. This is the
    // measurement behind "every slot": the source tree can look complete while
    // FillTypedVtable has quietly left a slot holding the stub.
    void** vt = *reinterpret_cast<void***>(comp);
    if (!vt) { std::fprintf(stderr, "[FoodPlace]   no vtable?!\n"); return; }
    void* stub = ::ComponentFactory::InertStubSlot();
    char map[kSlots + 1];
    int live = 0;
    for (int i = 0; i < kSlots; ++i) {
        const bool isStub = (vt[i] == stub) || (vt[i] == nullptr);
        map[i] = isStub ? '.' : 'W';
        if (!isStub) ++live;
    }
    map[kSlots] = 0;
    std::fprintf(stderr, "[FoodPlace]   slots 0..21 [%s]  %d/%d live\n", map, live, kSlots);
    if (live < kSlots) {
        std::fprintf(stderr, "[FoodPlace]   still stubbed:");
        for (int i = 0; i < kSlots; ++i) if (map[i] == '.') std::fprintf(stderr, " %d", i);
        std::fprintf(stderr, "\n");
    }
    std::fflush(stderr);
}

}  // namespace

extern "C" void FoodPlace_Tick(int frameNum) {
    Init();
    if (!s_want[0] || s_fired) return;
    if (frameNum < s_fireFrame) return;
    s_fired = true;
    const bool all = (std::strcmp(s_want, "all") == 0);
    int probed = 0;
    for (int i = 0; i < kClassCount; ++i)
        if (all || std::strcmp(s_want, kClasses[i].key) == 0) { ProbeClass(kClasses[i]); ++probed; }
    if (probed == 0)
        std::fprintf(stderr, "[FoodPlace] unknown target \"%s\"\n", s_want);
    std::fflush(stderr);
}
