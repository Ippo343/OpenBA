// code/Game/Component/immigrationcomponent_states.cpp
// The IMMIGRATION (nomad arrival) state machine of the town hall — ported 2026-09-05
// for the Immigrants achievement (Ach27, stat 28 = nomads admitted).
//
// ImmigrationComponent (0x68, a ControllerComponent on Template\TownHall.rsc) is the
// town hall's nomad-arrival controller. In retail its behaviour is the state table
// registered once by ImmigrationDescription_Setup -> helper_e6c0 (0x14004e6c0):
//
//   state 2    IDLE        enter d290 / tick d220 : if CanAccept -> 0xf
//                          transitions -> {3, 0xf}
//   state 0xf  WAITING     enter d1a0 : timer = (24 + rand*48) months
//                          tick  d0d0 : timer -= dt; when expired AND month in
//                                       [desc+0x50, desc+0x54] (spring) -> 0x10 if
//                                       CanAccept else back to 2
//                          transitions -> {3, 0x10, 2}
//   state 0x10 ARRIVING    enter d090 : reset + Spawn the group at a map edge
//                          tick  d070 : re-spawn if the group is empty
//                          transitions -> {3 (dismiss on the way out), 0x11}
//                          event 6  cff0 : one nomad reached the hall; all there -> 0x11
//   state 0x11 WAITING-FOR-ANSWER
//                          enter ceb0 : event-log "nomads have arrived" (type 0xe)
//                          tick  ce00 : month >= desc+0x58 (autumn) -> dismiss -> 2
//                          transitions -> {3 (dismiss), 2}
//                          event 7  cd90 : ALLOW  = Accept (convert to citizens) -> 2
//                          event 8  cd20 : DENY   = Dismiss (send them home)     -> 2
//
// The Allow/Deny buttons of the town hall's "Nomads" tab (ImmigrationUI_Slot9,
// immigrationui.cpp) post events 7/8 through the StateMachineComponent node — the
// same node-vtable slot 2 (PostEvent) the Dispatcher below implements.
//
// Component layout (retail ctor 0x14004e640):
//   +0x10 owner entity IBP (the town hall)   +0x18 ImmigrationDescription
//   +0x28 embedded state node (0x20 bytes; node+0x18 = this)
//   +0x48 Time  wait timer                    +0x50 int   nomads arrived at the hall
//   +0x58 IBP[] the nomad entities           +0x60 count  +0x64 capacity
// ImmigrationDescription (immigrationdescription.cpp):
//   +0x20 nomad entity template   +0x28 min population (80)
//   +0x30/+0x38 required building templates (must be built: townMgr+0x160 count > 0)
//   +0x40/+0x44 group size as a fraction of the population (0.05 .. 0.15)
//   +0x48/+0x4c wait timer base/range in MONTHS (24 / 48; one month = 300 s)
//   +0x50/+0x54 arrival month window (1..3)   +0x58 leave month (7)
//   +0x5c spawn distance from the map edge (10)
//   +0x60/+0x64 disease seed written to townMgr+0x1a8 / +0x1a0 on accept
//
// Opt-in: BANISHED_LIVE_IMMIGRATION (or the global live-components switch). Default off keeps
// the component inert exactly as before this port.
//
// Test fixtures (all env-gated, byte-unchanged when unset — NOT retail behaviour):
//   BANISHED_NOMAD_TRACE=1            stderr trace of every state/event/spawn/accept
//   BANISHED_NOMAD_TIMER_SCALE=<f>    multiply the 2..6-year wait timer (0.001 = now)
//   BANISHED_NOMAD_ANY_SEASON=1       ignore the spring window and the autumn leave
//   BANISHED_NOMAD_AUTOACCEPT=<n>     n ticks after the group is waiting, post the
//                                     ALLOW event (7) through the SM node, exactly
//                                     as the Allow button does; a NEGATIVE n posts
//                                     DENY (event 8) after |n| ticks instead
//
// Faithful offset-by-offset port of 0x14004cd00 .. 0x14004e4b0 and 0x140050030 /
// 0x1400501a0 (nomad -> citizen conversion).

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Mesh.h"
#include "Component/ResidenceComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/NomadDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/NomadComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/InteractDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/ImmigrationDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Action/exe_bodies.h"            // Pf_*, ActCNR_NR_SetVariant_1253d0
#include "Component/exe_bodies.h"         // CitizenComp_DeferredInit_117080
#include "Game/exe_bodies.h"              // Reimpl_MapGetTerrainPos
#include "Player/exe_bodies.h"            // PlayerPath_FindComponent_3ea0, ToolTile_SetTransformFromYaw_3870
#include "Work/exe_bodies.h"              // WorkBase_FindComponent_2ed0
#include "Entity/componententity_funcs.h" // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"
#include "Component/controller_state_table.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);

namespace {

// ---------------------------------------------------------------- constants
constexpr float kSecondsPerMonth = 300.0f;   // DAT_140172234
constexpr float kSecondsPerYear  = 3600.0f;  // DAT_14017225c
constexpr float kMonthsPerYear   = 12.0f;    // DAT_1401721b8
constexpr float kHalfCell        = 0.5f;     // DAT_140172114
constexpr float kYawRange        = 180.0f;   // DAT_140172220

// ---------------------------------------------------------------- helpers, by name (B3R)
//   0x140003ea0 PlayerPath_FindComponent_3ea0 (the StateMachineComponent)
//   0x140122e20 Pf_EntityResolvePos_2e20 (Entity_impl_2e20)   0x140123070 Pf_RecursiveChildSum_123070
//   0x1400c3720 ComponentEntity_CtorFromDescription (ComponentEntity_CopyCtor)
//   0x1400c48a0 EntityList_RegisterEntity_Impl                 0x14009c480 Pf_ZoneRefresh_9c480
//   0x140003870 ToolTile_SetTransformFromYaw_3870              0x1401253d0 ActCNR_NR_SetVariant_1253d0
//   0x140117080 CitizenComp_DeferredInit_117080
//   0x140002ed0 WorkBase_FindComponent_2ed0 (the InteractComponent; the engine_funcs global
//               that bound it was misnamed fn_Entity_FindComponent_Position)
// 0x140003110 was Collection::Array<void*>::SetSize(n, fill) monomorphised
// into the exe (ENDGAME B.3.2 step 2) — converted at its call site below.

// Type ids by name, resolved lazily from the live factory tree (the exe .rdata
// globals are not all registered in standalone; see the ImmigrationUI stub note).
uint32_t TidImmDesc()   { static uint32_t t = 0; if (!t) t = ImmigrationDescription::TypeId(); return t; }
uint32_t TidNomadDesc() { static uint32_t t = 0; if (!t) t = NomadDescription::TypeId();       return t; }
uint32_t TidNomadComp() { static uint32_t t = 0; if (!t) t = NomadComponent::TypeId();         return t; }
uint32_t TidResidence() { static uint32_t t = 0; if (!t) t = ResidenceComponent::TypeId();     return t; }
uint32_t TidCitDesc()   { static uint32_t t = 0; if (!t) t = CitizenDescription::TypeId();     return t; }
// cell 0x1401c7938 (TypeId_InteractDescription_FC) always fell back to
// InteractDescription::TypeId() unconditionally once filled; converting by cell
// (D5 exception) is mechanically equivalent.
uint32_t TidInteractDesc() { static uint32_t t = 0; if (!t) t = InteractDescription::TypeId(); return t; }

// ---------------------------------------------------------------- small accessors
inline uint8_t* GW() { return (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) : nullptr; }
inline uint8_t* TownMgr(uint8_t* gw) { return *reinterpret_cast<uint8_t**>(gw + 0xf0); }
inline void*    Rng(uint8_t* gw)     { return *reinterpret_cast<void**>(gw + 0xd8); }
inline int*     Grid(uint8_t* gw)    { return *reinterpret_cast<int**>(gw + 0xf8); }
inline uintptr_t Tiles(int* grid)    { return *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80); }

inline void MakeIBP(void* dst, void* obj) { new (dst) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge

// desc = comp+0x18 if IsDerivedFrom(ImmigrationDescription) else null — retail
// dereferences the null; a town hall without its description is a broken template.
uint8_t* ImmDesc(uintptr_t comp) {
    void* d = *reinterpret_cast<void**>(comp + 0x18);
    if (d && ::Object::IsDerivedFromNullSafe(d, TidImmDesc())) return reinterpret_cast<uint8_t*>(d);
    static bool warned = false;
    if (!warned) { warned = true; std::fprintf(stderr, "[ImmCtrl] comp=%p has no ImmigrationDescription (desc=%p tid=%08x)\n", (void*)comp, d, TidImmDesc()); std::fflush(stderr); }
    return nullptr;
}

// The month of the in-game year: the clock at gw+0x128 holds {year int, Time-in-year}.
int MonthOfYear(uint8_t* gw) {
    void* clock = *reinterpret_cast<void**>(gw + 0x128);
    if (!clock) return 0;
    float secs = reinterpret_cast<const System::Time*>(reinterpret_cast<uint8_t*>(clock) + 8)->AsSeconds();   // B.3.4b bridge
    int m = static_cast<int>((secs / kSecondsPerYear) * kMonthsPerYear);
    if (m > 11) m = 0;
    return m;
}

// Scan an entity's component list for the component whose GetTypeId == tid.
void* FindComponentByTid(void* entity, uint32_t tid) {
    if (!entity || !tid) return nullptr;
    uintptr_t e = reinterpret_cast<uintptr_t>(entity);
    int n = *reinterpret_cast<int*>(e + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(e + 0x30);
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* c = *slot;
        if (!c) continue;
        uint32_t got = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge (slot 2)
        if (got != tid) continue;
        c = *slot;
        return (::Object::IsDerivedFromNullSafe(c, tid)) ? c : nullptr;
    }
    return nullptr;
}

// Post an event to an entity's StateMachineComponent through its node vtable slot 2,
// the exact call shape retail uses: node->PostEvent(node, &sm, curIdx, eventId).
void PostEventToEntity(void* entity, int eventId) {
    if (!entity) return;
    uint64_t smcIBP = 0;
    PlayerPath_FindComponent_3ea0(reinterpret_cast<uintptr_t>(entity), &smcIBP);   // 0x140003ea0
    uintptr_t smc = static_cast<uintptr_t>(smcIBP);
    if (!smc) return;
    void* node = *reinterpret_cast<void**>(smc + 0x48);
    if (!node) return;
    void** vt = *reinterpret_cast<void***>(node);
    typedef char (*PostEvent_t)(void*, void*, int, int);
    reinterpret_cast<PostEvent_t>(vt[2])(node, reinterpret_cast<void*>(smc + 0x48),
                                        static_cast<int>(*reinterpret_cast<int8_t*>(smc + 0x51)), eventId);
}

}  // namespace

// ============================================================
// CanAccept — port of 0x14004e3f0
// population >= desc+0x28 AND every required template in desc+0x30[0..+0x38) has a
// built instance (townMgr+0x160 BST: key@+0, count@+0x10, left@+0x20, right@+0x28).
// ============================================================
extern "C" bool Immigration_CanAccept(void* compV) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return false;
    uint8_t* tm = TownMgr(gw);
    if (*reinterpret_cast<int*>(tm + 0xa8) < *reinterpret_cast<int*>(desc + 0x28)) return false;
    int req = *reinterpret_cast<int*>(desc + 0x38);
    if (req < 1) return true;
    uint64_t* root = *reinterpret_cast<uint64_t**>(tm + 0x160);
    uint64_t* keys = *reinterpret_cast<uint64_t**>(desc + 0x30);
    for (int i = 0; i < req; ++i) {
        uint64_t key = keys[i];
        uint64_t* n = root;
        while (n) {
            if (key < n[0])       n = reinterpret_cast<uint64_t*>(n[4]);
            else if (key <= n[0]) break;
            else                  n = reinterpret_cast<uint64_t*>(n[5]);
        }
        if (!n || *reinterpret_cast<int*>(&n[2]) == 0) return false;
    }
    return true;
}

// ============================================================
// FindSpawnCell — port of 0x14004e270
// Up to 50 tries: a random cell within desc+0x5c of (x, y) that is walkable
// (flag bit in 0xe07) and — after a zone refresh — in the same pathing zone
// (tile+0x26) as the town hall's cell, or on a zone-less bridge/road class (0x1e0).
// ============================================================
extern "C" int Immigration_FindSpawnCell(void* compV, int x, int y, int hallCell) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return -1;
    void* rng = Rng(gw);
    int* grid = Grid(gw);
    const int W = *grid;
    const int reach = *reinterpret_cast<int*>(desc + 0x5c);
    for (int t = 0; t < 50; ++t) {
        int cx = x + reinterpret_cast<System::Random*>(rng)->RandTol(static_cast<int32_t>(reach));   // B.3.4b bridge
        int cy = y + reinterpret_cast<System::Random*>(rng)->RandTol(static_cast<int32_t>(reach));   // B.3.4b bridge
        if (cx < 0) cx = 0; else if (cx >= W) cx = W - 1;
        if (cy < 0) cy = 0; else if (cy >= W) cy = W - 1;
        int c = W * cy + cx;
        uintptr_t tiles = Tiles(grid);
        uint32_t fc = 1u << (*reinterpret_cast<uint8_t*>(tiles + static_cast<uintptr_t>(c) * 0x30 + 0x29) & 0x1f);
        if ((fc & 0xe07u) == 0) continue;
        Pf_ZoneRefresh_9c480(grid, nullptr, nullptr, hallCell);   // 0x14009c480
        tiles = Tiles(grid);
        uintptr_t tc = tiles + static_cast<uintptr_t>(c) * 0x30;
        uintptr_t th = tiles + static_cast<uintptr_t>(hallCell) * 0x30;
        uint32_t mh = 1u << (*reinterpret_cast<uint8_t*>(th + 0x29) & 0x1f);
        uint32_t mc = 1u << (*reinterpret_cast<uint8_t*>(tc + 0x29) & 0x1f);
        uint32_t u = mc | mh;
        uint32_t u8 = u | 0xe1fu;
        if ((u8 & mc) && (u8 & mh)) {
            if (u & 0x1e0u) return c;
            int16_t zc = *reinterpret_cast<int16_t*>(tc + 0x26);
            int16_t zh = *reinterpret_cast<int16_t*>(th + 0x26);
            if (zc != 0 && zh != 0 && zc == zh) return c;
        }
    }
    return -1;
}

// ============================================================
// Nomad_Init — port of 0x140050030
// Bind a freshly cloned nomad to its town hall: target (+0x28), spawn position
// (+0x30), meeting position (+0x3c), gender (+0x50); place the model at the spawn
// point with a random yaw, pick the mesh variant and material for the gender.
// ============================================================
extern "C" void Nomad_Init(void* nomadV, void* ownerIBP, const float* spawnPos, const float* meetPos) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(nomadV);
    uint8_t* gw = GW();
    if (!gw) return;
    void* nd = *reinterpret_cast<void**>(nomad + 0x18);
    uint8_t* ndesc = (nd && ::Object::IsDerivedFromNullSafe(nd, TidNomadDesc())) ? reinterpret_cast<uint8_t*>(nd) : nullptr;
    *reinterpret_cast<uint64_t*>(nomad + 0x28) = *reinterpret_cast<uint64_t*>(ownerIBP);
    float* sp = reinterpret_cast<float*>(nomad + 0x30);
    sp[0] = spawnPos[0]; sp[1] = spawnPos[1]; sp[2] = spawnPos[2];
    float* mp = reinterpret_cast<float*>(nomad + 0x3c);
    mp[0] = meetPos[0]; mp[1] = meetPos[1]; mp[2] = meetPos[2];
    void* rng = Rng(gw);
    *reinterpret_cast<uint32_t*>(nomad + 0x50) = reinterpret_cast<System::Random*>(rng)->Rand(2u);   // B.3.4b bridge

    void* entity = *reinterpret_cast<void**>(nomad + 0x10);
    uint64_t modelIBP = 0;
    fn_FindComponent_Character(entity, &modelIBP);   // ModelComponent (DAT_1401c7a50)
    uintptr_t model = static_cast<uintptr_t>(modelIBP);
    if (!model) return;
    float yaw = fn_Random_RandFloatTol(rng) * kYawRange;
    ToolTile_SetTransformFromYaw_3870(model, reinterpret_cast<void*>(nomad + 0x30), yaw);   // 0x140003870
    ActCNR_NR_SetVariant_1253d0(reinterpret_cast<int64_t*>(model),   // 0x1401253d0
                                static_cast<uint16_t>(*reinterpret_cast<uint32_t*>(nomad + 0x50)), 0);
    if (ndesc) {
        void* mat = *reinterpret_cast<void**>(ndesc + ((*reinterpret_cast<uint32_t*>(nomad + 0x50) == 0) ? 0x28 : 0x30));
        reinterpret_cast<Graphics::MeshInstance*>(model + 0x28)
            ->SetMaterialInstance(0, Graphics::ExternalPtrHandle{mat});   // B.3.5 bridge
    }
}

// ============================================================
// Spawn — port of 0x14004dbe0
// Pick a map edge, find a walkable edge cell in the hall's pathing zone, size the
// group from the population (5..15%) plus a random share of the empty house slots
// (minimum 2), then clone the nomad template once per member at that edge and
// register each into the world.
// ============================================================
extern "C" void Immigration_Spawn(void* compV) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return;
    void* rng = Rng(gw);
    int* grid = Grid(gw);
    const int W = *grid;
    const int edge = *reinterpret_cast<int*>(desc + 0x5c);

    uint64_t posIBP = 0;
    WorkBase_FindComponent_2ed0(reinterpret_cast<uintptr_t>(*reinterpret_cast<void**>(comp + 0x10)), &posIBP);   // 0x140002ed0

    int x = 0, y = 0, hallCell = 0, cell = -1;
    for (int t = 0; t < 10; ++t) {
        uint32_t r = reinterpret_cast<System::Random*>(rng)->Rand(4u);   // B.3.4b bridge
        if (static_cast<int>(r) < 2) {
            x = (r == 0) ? edge : (W - edge) - 1;
            y = static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(W)));   // B.3.4b bridge
        } else {
            x = static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(W)));   // B.3.4b bridge
            y = (r == 2) ? edge : (W - edge) - 1;
        }
        float hp[3] = {0, 0, 0};
        Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posIBP), hp, 0, 0, nullptr);
        int cx = static_cast<int>(hp[0] + static_cast<float>(W >> 1));
        int cy = static_cast<int>(hp[1] + static_cast<float>(W >> 1));
        if (cx < 0) cx = 0;
        if (cx >= W) cx = W - 1;
        if (cy < 0) cy = 0;
        if (cy >= W) cy = W - 1;
        hallCell = cy * W + cx;
        cell = Immigration_FindSpawnCell(compV, x, y, hallCell);
        if (cell >= 0) break;
    }
    if (cell < 0) {
        return;
    }

    uint8_t* tm = TownMgr(gw);
    const int pop = *reinterpret_cast<int*>(tm + 0xa8);
    const float fLo = *reinterpret_cast<float*>(desc + 0x40);
    const float fRand = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
    const float fHi = *reinterpret_cast<float*>(desc + 0x44);

    // Empty household slots across every registered house (townMgr+0x20/+0x28).
    int emptySlots = 0;
    {
        int nh = *reinterpret_cast<int*>(tm + 0x28);
        uintptr_t houses = *reinterpret_cast<uintptr_t*>(tm + 0x20);
        for (int i = 0; i < nh; ++i) {
            uint64_t h = *reinterpret_cast<uint64_t*>(houses + static_cast<uintptr_t>(i) * 8);
            void* res = FindComponentByTid(reinterpret_cast<void*>(h), TidResidence());
            uint64_t rIBP = 0; MakeIBP(&rIBP, res);
            uintptr_t rc = static_cast<uintptr_t>(rIBP);
            if (!rc) continue;
            int slots = *reinterpret_cast<int*>(rc + 0x58);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(rc + 0x50);
            for (int s = 0; s < slots; ++s)
                if (*reinterpret_cast<int*>(arr + static_cast<uintptr_t>(s) * 0x10 + 8) == 0) ++emptySlots;
        }
    }
    // Retail calls Rand(emptySlots*2) unguarded; skipped only when the range is 0.
    int extra = (emptySlots > 0) ? static_cast<int>(reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(emptySlots * 2))) : 0;   // B.3.4b bridge
    int n = extra + static_cast<int>(static_cast<float>(pop) * (fRand * (fHi - fLo) + fLo));
    if (n < 2) n = 2;


    for (int k = 0; k < n; ++k) {
        int c = Immigration_FindSpawnCell(compV, x, y, hallCell);
        if (c < 0) c = cell;

        // A random "use point" on the town hall for this nomad to stand at.
        uintptr_t pc = static_cast<uintptr_t>(posIBP);
        void* idesc = *reinterpret_cast<void**>(pc + 0x18);
        uint8_t* id = (idesc && ::Object::IsDerivedFromNullSafe(idesc, TidInteractDesc())) ? reinterpret_cast<uint8_t*>(idesc) : nullptr;
        uint32_t nPts = id ? *reinterpret_cast<uint32_t*>(id + 0x50) : 0;
        int nChildren = *reinterpret_cast<int*>(pc + 0x30);
        for (int ci = 0; ci < nChildren; ++ci) {
            int64_t* child = reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(pc + 0x28) + static_cast<uintptr_t>(ci) * 8);
            nPts += static_cast<uint32_t>(Pf_RecursiveChildSum_123070(*child, 2));   // 0x140123070
        }
        uint32_t ptIdx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(nPts));   // B.3.4b bridge
        float meet[3] = {0, 0, 0};
        Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posIBP), meet, 2, static_cast<int>(ptIdx), nullptr);

        // Clone the nomad template.
        void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
        void* ent = mem ? ComponentEntity_CtorFromDescription(   // 0x1400c3720
                          reinterpret_cast<ComponentEntity*>(mem), *reinterpret_cast<void**>(desc + 0x20)) : nullptr;
        uint64_t entIBP = 0; MakeIBP(&entIBP, ent);

        // World position of the spawn cell, snapped to the terrain.
        float half = static_cast<float>(W >> 1) - kHalfCell;
        float cellPos[3] = { static_cast<float>(c % W) - half, static_cast<float>(c / W) - half, 0.0f };
        float spawn[3] = {0, 0, 0};
        Reimpl_MapGetTerrainPos(grid, spawn, cellPos, '\0', nullptr);

        // Bind its NomadComponent to this hall and drop it into the world.
        void* nomad = FindComponentByTid(reinterpret_cast<void*>(entIBP), TidNomadComp());
        uint64_t nomadIBP = 0; MakeIBP(&nomadIBP, nomad);
        if (nomadIBP) Nomad_Init(reinterpret_cast<void*>(nomadIBP), reinterpret_cast<void*>(comp + 0x10), spawn, meet);
        uintptr_t e = static_cast<uintptr_t>(entIBP);
        if (e) {
            int category = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(e + 0x28) + 0x20);
            uint64_t regIBP = entIBP;
            EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(gw + 0xe8), &regIBP, category);
        }
        // Append to the group array (+0x58/+0x60/+0x64) — retail inlines this
        // DynArray8 push; the registered resize does the same growth policy.
        int count = *reinterpret_cast<int*>(comp + 0x60);
        uint64_t pushIBP = entIBP;
        // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
        GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(comp + 0x58))
            .SetSizeFill(count + 1, &pushIBP);
    }
    *reinterpret_cast<uint32_t*>(comp + 0x50) = 0;
}

// ============================================================
// Dismiss — port of 0x14004d300: event 1 (LEAVE) to every nomad, drop the group.
// ============================================================
extern "C" void Immigration_Dismiss(void* compV) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    int n = *reinterpret_cast<int*>(comp + 0x60);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(comp + 0x58);
    for (int i = 0; i < n; ++i) {
        uint64_t* slot = reinterpret_cast<uint64_t*>(arr + static_cast<uintptr_t>(i) * 8);
        PostEventToEntity(reinterpret_cast<void*>(*slot), 1);
    }
    if (arr) System::MemoryAllocator::MemFree(reinterpret_cast<void*>(arr));
    *reinterpret_cast<uint64_t*>(comp + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(comp + 0x60) = 0;
    *reinterpret_cast<uint32_t*>(comp + 0x50) = 0;
}

// ============================================================
// Convert — port of 0x1400501a0: clone the citizen template for one nomad at the
// nomad's current position and run the per-citizen deferred init (name, age,
// gender, family link). Returns the new CitizenComponent (out IBP).
// ============================================================
extern "C" void* Nomad_ConvertToCitizen(void* nomadV, uint64_t* outIBP, uint64_t* parentIBP, int gender, char isChild) {
    uintptr_t nomad = reinterpret_cast<uintptr_t>(nomadV);
    uint8_t* gw = GW();
    *outIBP = 0;
    if (!gw) return nullptr;
    void* nd = *reinterpret_cast<void**>(nomad + 0x18);
    uint8_t* ndesc = (nd && ::Object::IsDerivedFromNullSafe(nd, TidNomadDesc())) ? reinterpret_cast<uint8_t*>(nd) : nullptr;
    if (!ndesc) return nullptr;
    void* rng = Rng(gw);

    void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* ent = mem ? ComponentEntity_CtorFromDescription(   // 0x1400c3720
                          reinterpret_cast<ComponentEntity*>(mem), *reinterpret_cast<void**>(ndesc + 0x20)) : nullptr;
    uint64_t entIBP = 0; MakeIBP(&entIBP, ent);
    fn_FindComponent_Citizen(reinterpret_cast<void*>(entIBP), outIBP);
    uintptr_t cit = static_cast<uintptr_t>(*outIBP);
    if (!cit) return nullptr;
    void* cd = *reinterpret_cast<void**>(cit + 0x18);
    uint8_t* cdesc = (cd && ::Object::IsDerivedFromNullSafe(cd, TidCitDesc())) ? reinterpret_cast<uint8_t*>(cd) : nullptr;

    // Spawn the citizen where the nomad stands (its model transform translation).
    uint64_t modelIBP = 0;
    fn_FindComponent_Character(*reinterpret_cast<void**>(nomad + 0x10), &modelIBP);
    float pos[3] = {0, 0, 0};
    if (modelIBP) {
        const float* t = reinterpret_cast<const float*>(static_cast<uintptr_t>(modelIBP) + 0x158);
        pos[0] = t[0]; pos[1] = t[1]; pos[2] = t[2];
    }
    float age;
    float aLo = cdesc ? *reinterpret_cast<float*>(cdesc + 0x50) : 0.0f;
    float aHi = cdesc ? *reinterpret_cast<float*>(cdesc + 0x54) : 0.0f;
    if (isChild == 0) age = reinterpret_cast<System::Random*>(rng)->RandFloat() * (aHi - aLo) + aLo;   // B.3.4b bridge
    else              age = reinterpret_cast<System::Random*>(rng)->RandFloat() * aLo;   // B.3.4b bridge
    uint32_t r = reinterpret_cast<System::Random*>(rng)->Rand(1u);   // B.3.4b bridge
    char flag = (r == 0 || isChild != 0) ? 0 : 1;
    int64_t key = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(gw + 0xb0) + 0x30);
    int64_t parent = static_cast<int64_t>(*parentIBP);
    CitizenComp_DeferredInit_117080(static_cast<int64_t>(*outIBP), pos, static_cast<uint32_t>(gender), isChild, age, flag, &parent, key);

    uint64_t regIBP = entIBP;
    {
        int category = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(ndesc + 0x20) + 0x20);
        EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(gw + 0xe8), &regIBP, category);
    }
    return reinterpret_cast<void*>(*outIBP);
}

// ============================================================
// Accept — port of 0x14004d450 (the ALLOW handler's body).
// Seeds the town's disease clock (townMgr+0x1a8 chance, +0x1a0 duration), adds the
// group to the NOMADS-ADMITTED counter at townMgr+0x1b0 (achievement stat 28), then
// converts the group in fours: adult (gender 1), adult (gender 0) married to it,
// and two children of the second adult; every converted nomad gets event 0
// (DESTROY). Finally frees the group array.
// ============================================================
extern "C" void Immigration_Accept(void* compV) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return;
    uint8_t* tm = TownMgr(gw);

    float chance = *reinterpret_cast<float*>(desc + 0x60);
    if (*reinterpret_cast<float*>(tm + 0x1a8) < chance) *reinterpret_cast<float*>(tm + 0x1a8) = chance;
    int64_t dur = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&dur), static_cast<float>(*reinterpret_cast<int*>(desc + 0x64)) * kSecondsPerMonth);
    if (*reinterpret_cast<int64_t*>(tm + 0x1a0) < dur) *reinterpret_cast<int64_t*>(tm + 0x1a0) = dur;

    const int n = *reinterpret_cast<int*>(comp + 0x60);
    *reinterpret_cast<int*>(TownMgr(gw) + 0x1b0) += n;

    uintptr_t arr = *reinterpret_cast<uintptr_t*>(comp + 0x58);
    auto nomadAt = [&](int i) -> void* {
        uint64_t* slot = reinterpret_cast<uint64_t*>(arr + static_cast<uintptr_t>(i) * 8);
        return *reinterpret_cast<void**>(slot);
    };
    auto convert = [&](int i, uint64_t* parent, int gender, char child) -> uintptr_t {
        void* ent = nomadAt(i);
        void* nomad = FindComponentByTid(ent, TidNomadComp());
        uint64_t nIBP = 0; MakeIBP(&nIBP, nomad);
        uint64_t citIBP = 0;
        Nomad_ConvertToCitizen(reinterpret_cast<void*>(nIBP), &citIBP, parent, gender, child);
        PostEventToEntity(nomadAt(i), 0);
        return static_cast<uintptr_t>(citIBP);
    };

    int genderC = 0;
    for (int i = 0; i < n; ) {
        uint64_t nullIBP = 0; MakeIBP(&nullIBP, nullptr);
        uintptr_t a = convert(i, &nullIBP, 1, 0);
        if (i + 1 >= n) break;
        uint64_t nullIBP2 = 0; MakeIBP(&nullIBP2, nullptr);
        uintptr_t b = convert(i + 1, &nullIBP2, 0, 0);
        if (b) *reinterpret_cast<uintptr_t*>(b + 0x250) = a;
        if (a) *reinterpret_cast<uintptr_t*>(a + 0x250) = b;
        if (i + 2 >= n) break;
        uint64_t parent = b;
        convert(i + 2, &parent, genderC, 1);
        if (i + 3 >= n) break;
        int genderD = (genderC == 0) ? 1 : 0;
        uint64_t parent2 = b;
        convert(i + 3, &parent2, genderD, 1);
        genderC = (genderD == 0) ? 1 : 0;
        i += 4;
    }
    if (arr) System::MemoryAllocator::MemFree(reinterpret_cast<void*>(arr));
    *reinterpret_cast<uint64_t*>(comp + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(comp + 0x60) = 0;
    *reinterpret_cast<uint32_t*>(comp + 0x50) = 0;
}

// ============================================================
// State handlers (data = the ImmigrationComponent, sm = its StateMachine)
// ============================================================
namespace {
}  // namespace

// state 2 IDLE — enter 0x14004d290 (returns 0), tick 0x14004d220 (returns 1)
extern "C" char Imm_State2_Enter(void* data, void* sm) {
    if (Immigration_CanAccept(data)) ::ControllerLive::RequestStateTransition(sm, 0xf);
    return 0;
}
extern "C" char Imm_State2_Tick(void* data, void* sm, void* /*time*/) {
    if (Immigration_CanAccept(data)) ::ControllerLive::RequestStateTransition(sm, 0xf);
    return 1;
}

// state 0xf WAITING — enter 0x14004d1a0: timer = (base + rand*range) months
extern "C" char Imm_StateF_Enter(void* data, void* /*sm*/) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return 0;
    float r = reinterpret_cast<System::Random*>(Rng(gw))->RandFloat();   // B.3.4b bridge
    float months = r * *reinterpret_cast<float*>(desc + 0x4c) + *reinterpret_cast<float*>(desc + 0x48);
    float secs = months * kSecondsPerMonth;
    int64_t t = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), secs);
    *reinterpret_cast<int64_t*>(comp + 0x48) = t;
    return 0;
}
// tick 0x14004d0d0: countdown; in season -> 0x10 if CanAccept else 2
extern "C" char Imm_StateF_Tick(void* data, void* sm, void* time) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return 1;
    int64_t* timer = reinterpret_cast<int64_t*>(comp + 0x48);
    *timer -= time ? *reinterpret_cast<int64_t*>(time) : 0;
    int month = MonthOfYear(gw);
    bool inSeason =
        (*reinterpret_cast<int*>(desc + 0x50) <= month && month <= *reinterpret_cast<int*>(desc + 0x54));
    if (*timer < 1 && inSeason) {
        bool ok = Immigration_CanAccept(data);
        ::ControllerLive::RequestStateTransition(sm, ok ? 0x10 : 2);
    }
    return 1;
}

// state 0x10 ARRIVING — enter 0x14004d090: reset the group and spawn it
extern "C" char Imm_State10_Enter(void* data, void* /*sm*/) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    *reinterpret_cast<uint32_t*>(comp + 0x50) = 0;
    void* old = *reinterpret_cast<void**>(comp + 0x58);
    if (old) System::MemoryAllocator::MemFree(old);
    *reinterpret_cast<uint64_t*>(comp + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(comp + 0x60) = 0;
    Immigration_Spawn(data);
    return 1;
}
// tick 0x14004d070: spawn again if the group is empty
extern "C" char Imm_State10_Tick(void* data, void* /*sm*/, void* /*time*/) {
    if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(data) + 0x60) == 0) Immigration_Spawn(data);
    return 1;
}
// event 6 0x14004cff0: a nomad reached the hall
extern "C" char Imm_State10_Event6_Arrived(void* data, void* sm) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    uint32_t arrived = ++*reinterpret_cast<uint32_t*>(comp + 0x50);
    if (arrived == *reinterpret_cast<uint32_t*>(comp + 0x60)) ::ControllerLive::RequestStateTransition(sm, 0x11);
    return 1;
}
// transition 0x10/0x11 -> 3 handler 0x14004cd00: dismiss on the way out
extern "C" void Imm_Trans3_Dismiss(void* data) {
    Immigration_Dismiss(data);
}

// state 0x11 WAITING FOR AN ANSWER — enter 0x14004ceb0: the event-log line
extern "C" char Imm_State11_Enter(void* data, void* /*sm*/) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    uint8_t* gw = GW();
    if (gw) {
        void* eventLog = *reinterpret_cast<void**>(gw + 0x138);
        int64_t ownerIBP = *reinterpret_cast<int64_t*>(comp + 0x10);
        EventLog_AddEvent(eventLog, 0xe, 2, &ownerIBP);
    }
    return 1;
}
// tick 0x14004ce00: past the leave month -> dismiss -> 2
extern "C" char Imm_State11_Tick(void* data, void* sm, void* /*time*/) {
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    uint8_t* gw = GW();
    uint8_t* desc = ImmDesc(comp);
    if (!gw || !desc) return 1;
    if (*reinterpret_cast<int*>(desc + 0x58) <= MonthOfYear(gw)) {
        Immigration_Dismiss(data);
        ::ControllerLive::RequestStateTransition(sm, 2);
    }
    return 1;
}
// event 7 0x14004cd90: ALLOW
extern "C" char Imm_Event7_Allow(void* data, void* sm) {
    Immigration_Accept(data);
    ::ControllerLive::RequestStateTransition(sm, 2);
    return 1;
}
// event 8 0x14004cd20: DENY
extern "C" char Imm_Event8_Deny(void* data, void* sm) {
    Immigration_Dismiss(data);
    ::ControllerLive::RequestStateTransition(sm, 2);
    return 1;
}

// 0x1400a9ac0 — request a transition on a StateMachine (shared idiom).
extern "C" void Imm_RequestStateTransition_9ac0(void* sm, int stateId) {
    ::ControllerLive::RequestStateTransition(sm, stateId);
}

// ============================================================
// Dispatcher + table (replicates ImmigrationDescription_helper_e6c0, 0x14004e6c0)
// ============================================================
// ENDGAME B.3.8 MI-A: the tag lives with the class (its controller member is
// ControllerLive::TableController<ImmigrationCtrlTag>, ImmigrationComponent.h).
#include "Component/ImmigrationComponent.h"
using ImmigrationDispatch = ::ControllerLive::Dispatcher<ImmigrationCtrlTag>;

extern "C" void ImmigrationCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = ImmigrationDispatch::table;
    if (t.count != 0) return;
    // sub_7ba0(id, ENTER, p3, p4, TICK) -> {id, enter@+0x08, trans38@+0x18}
    TableRegisterState(t, 2, reinterpret_cast<void*>(&Imm_State2_Enter), nullptr,
                       reinterpret_cast<void*>(&Imm_State2_Tick), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 0xf, nullptr);

    TableRegisterState(t, 0xf, reinterpret_cast<void*>(&Imm_StateF_Enter), nullptr,
                       reinterpret_cast<void*>(&Imm_StateF_Tick), nullptr);
    TableAddTransition(t, 3, nullptr);
    TableAddTransition(t, 0x10, nullptr);
    TableAddTransition(t, 2, nullptr);

    TableRegisterState(t, 0x10, reinterpret_cast<void*>(&Imm_State10_Enter), nullptr,
                       reinterpret_cast<void*>(&Imm_State10_Tick), nullptr);
    TableAddTransition(t, 3, reinterpret_cast<void*>(&Imm_Trans3_Dismiss));
    TableAddTransition(t, 0x11, nullptr);
    TableAddEvent(t, 6, reinterpret_cast<void*>(&Imm_State10_Event6_Arrived));

    TableRegisterState(t, 0x11, reinterpret_cast<void*>(&Imm_State11_Enter), nullptr,
                       reinterpret_cast<void*>(&Imm_State11_Tick), nullptr);
    TableAddTransition(t, 3, reinterpret_cast<void*>(&Imm_Trans3_Dismiss));
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 7, reinterpret_cast<void*>(&Imm_Event7_Allow));
    TableAddEvent(t, 8, reinterpret_cast<void*>(&Imm_Event8_Deny));
}
