// code/Game/Component/spawncomponent.cpp
// Clean reimplementation of SpawnComponent::Update from Application-x64.exe.
//
// Periodic entity spawning system. For each spawn entry in the resource data:
//   1. Check if current growth rate falls within the entry's [minGrowth, maxGrowth] range.
//   2. If in range, accumulate delta time into per-entry timer.
//   3. Look up spawn interval from linked template data.
//   4. When accumulated time exceeds interval, subtract interval and call
//      the spawn function. Repeat while timer still exceeds interval.
//
// Original: 0x140055ad0 (114 instructions)

#include "Component/spawncomponent_funcs.h"
#include "Component/SpawnDescription.h"
#include "Component/GrowthDescription.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Resolved helpers
// ============================================================

static constexpr float SECS_PER_SEASON = 300.0f;

// FUN_1400052c0 — gets linked object from entity owner
typedef void* (*fn_GetLinkedObject_t)(void* entity, void* outPtr);
static fn_GetLinkedObject_t s_fn_GetLinkedObject = nullptr;

// FUN_140055780 — performs actual spawn operation
typedef void (*fn_DoSpawn_t)(void* self, void* resourceData, int entryIndex, char flag);
static fn_DoSpawn_t s_fn_DoSpawn = nullptr;

// FUN_140001bb0 — dynamic Time-array resize (stride 8). Sizes the per-entry timer
// array at self+0x28 in the OnCreate/Startup body-init. ENDGAME B.3.2 step 2:
// converted to GameColl::AsArray<GameColl::Ptr8>(h).SetSize(n) (same stride/align/
// zero-tail as Collection::Array<void*>::SetSize).

// ---- stream-serialize helpers (vtable slot [3], see SpawnComponent_Serialize) ----
typedef bool (*fn_IsDerived_t)(void* obj, uint32_t typeId);
static fn_IsDerived_t          s_fn_IsDerived           = nullptr;  // slot 0x140152a98

static bool s_initialized = false;

// ============================================================
// SpawnComponent::Update (0x140055ad0)
// ============================================================
namespace {
// Set once the worldgen scatter has reported, so a later dispatch is provably
// PER-TICK rather than seed-pass. Needed as the graduation positive assertion for
// the per-tick spawn arm: every worldgen-time line is indistinguishable
bool g_worldgenDone = false;


}  // namespace


// ============================================================
// SpawnComponent initial seed — ComponentEntity_impl_5cc0 (0x140055cc0)
// ============================================================
// Called once per scattered natural resource by FUN_1400a12b0 (the worldgen
// scatter), right after the growth/age init and before the model-variant pick —
// the retail call site is 0x1400a1cb2, guarded on the entity actually carrying a
// SpawnComponent. This is what makes a NEW MAP start with wild food already on
// the ground rather than growing it over months of sim: for every spawn entry
// whose [minSpawnAge, maxSpawnAge] window contains the tree's freshly-randomized
// growth, it dispatches DoSpawn `growth / GrowthDescription+0x20 + 1` times, so
// an old tree gets proportionally more sub-resources under it than a sapling.
//
// This is the SOLE producer of the four resources the Gatherer's Hut accepts
// (Onion / Blueberry / Mushroom / Root — see NaturalResourceTree.rsc's
// SpawnDescription against GathererShelter.rsc's HerbalistDescription): they are
// not a worldgen pool of their own. Deviation #4 in notes/generation_deviations.md
// deferred this pass "until GrowthComponent is live"; BANISHED_LIVE_GROWTH is now
// default-on, so the precondition is met and the pass is restored here.
//
// Standalone deviations, both bail-instead-of-crash on a shape retail guarantees:
//   * the GrowthDescription lookup on the spawn template can miss in our partial
//     factory tree — retail dereferences the result unconditionally;
//   * a zero divisor at GrowthDescription+0x20 would make the repeat count
//     meaningless (retail's data never has one).
// The dead component-search over the OWNER's list at the top of 5cc0 (its result
// is computed and discarded) is not reproduced.
static void EnsureSpawnHelpers();   // defined with the DoSpawn closure below

void SpawnComponent_InitialSpawn(SpawnComponent* self) {
    SpawnComponent_InitFuncs();
    EnsureSpawnHelpers();
    if (!self || !s_fn_DoSpawn) return;

    uintptr_t comp = reinterpret_cast<uintptr_t>(self);

    // Sibling NaturalResourceComponent — carries the growth value at +0x28.
    uintptr_t ownerIBP = comp + 0x10;
    uintptr_t natRes = 0;
    if (s_fn_GetLinkedObject) s_fn_GetLinkedObject(*(void**)ownerIBP, &natRes);
    if (natRes == 0) return;
    const float growth = *(float*)(natRes + 0x28);

    // SpawnDescription at +0x18 (IsDerivedFromOrTrust — same policy as Update).
    void* baseObj = self->_description;
    if (!baseObj) return;
    void* spawnDesc = baseObj;
    if (fn_IsDerivedFrom && fn_IsDerivedFrom(baseObj, SpawnDescription::TypeId()))
        spawnDesc = baseObj;

    const int entryCount = *(int*)((uintptr_t)spawnDesc + 0x28);
    const uintptr_t entryArrayBase = *(uintptr_t*)((uintptr_t)spawnDesc + 0x20);
    if (entryCount <= 0 || !entryArrayBase) return;

    for (int i = 0; i < entryCount; ++i) {
        const uintptr_t entry = entryArrayBase + (uintptr_t)i * 0x10;
        const float minAge = *(float*)(entry + 0x08);
        const float maxAge = *(float*)(entry + 0x0C);
        if (growth < minAge || growth > maxAge) continue;

        // The spawn template entity; find its GrowthDescription for the divisor.
        const int64_t host = *(int64_t*)entry;
        if (!host) continue;
        void* growthDesc = nullptr;
        const int compCount = *(int*)(host + 0x30);
        const int64_t compArr = *(int64_t*)(host + 0x28);
        for (int j = 0; j < compCount && compArr; ++j) {
            void* c = *(void**)(compArr + (int64_t)j * 8);
            if (!c) continue;
            const uint32_t tid = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.8 bridge
            if (tid == GrowthDescription::TypeId()) {
                growthDesc = c;   // trust it; IsDerivedFrom is unreliable here
                break;
            }
        }
        if (!growthDesc) continue;

        const float perSpawn = *(float*)((uintptr_t)growthDesc + 0x20);
        if (!(perSpawn > 0.0f)) continue;
        const int repeats = (int)(growth / perSpawn) + 1;
        for (int r = 0; r < repeats; ++r) {
            s_fn_DoSpawn(self, spawnDesc, i, 1);   // flag=1 => random orientation
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void SpawnComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_GetLinkedObject = reinterpret_cast<fn_GetLinkedObject_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_DoSpawn = reinterpret_cast<fn_DoSpawn_t>(
        ResolveAddr(Module::Exe, 0x140055780));

    {
        uintptr_t slot;
        slot = ResolveAddr(Module::Exe, 0x140152a98);
        if (slot) s_fn_IsDerived = *reinterpret_cast<fn_IsDerived_t*>(slot);
    }

    s_initialized = true;
}


// 0x140055ee0 door: registered by address (standalone/reg_growth_spawn_serialize.cpp).
extern "C" void SpawnComponent_Serialize(void* self, void* stream) {
    if (!self) return;
    reinterpret_cast<SpawnComponent*>(self)->SpawnComponent::Persist(stream);   // B.3.8 bridge
}


// ============================================================
// Reimpl of FUN_140055780 (ComponentEntity_impl_5780: probabilistically spawn one
//   child entity (animal/resource) near the owner from a spawn-table entry)
// ============================================================
// Faithful port of the Ghidra decompilation. resourceData+0x20 holds the spawn
// table (0x10 stride); entryIndex selects the template. A random roll gated by the
// template's density (g_fUVScale / templ+0x48) decides whether to spawn; the tile is
// chosen by RandTol jitter around the owner tile and validated, then a copy of the
// template entity is instantiated, placed, oriented, and registered.
typedef void* (*fn_SC_FindMap_t)(uintptr_t entity, void* retbuf); // 0x140004680
typedef char  (*fn_SC_CanPlace_t)(int* grid, int x, int y, int kind,
                                  float a, float b, float c,
                                  char d, char e, int f);          // 0x14009f5d0
typedef char  (*fn_SC_TileFree_t)(void* templ, int x, int y);     // 0x140120c30
typedef void* (*fn_SC_CopyCtor_t)(void* dst, int64_t srcEntity);  // 0x1400c3720
typedef void* (*fn_SC_FindLinkA_t)(int64_t ibp, void* retbuf);    // 0x1400052c0
typedef void* (*fn_SC_FindLinkB_t)(int64_t ibp, void* retbuf);    // 0x14000c460
typedef void  (*fn_SC_SetTile_t)(int64_t entity, int tileIndex);  // 0x140126540
typedef void  (*fn_SC_SetOrient_t)(int64_t entity, float v);      // 0x140120db0
typedef void  (*fn_SC_Register_t)(void* list, void* ibp, int idx); // 0x1400c48a0
static fn_SC_FindMap_t   s_sc_FindMap = nullptr;
static fn_SC_CanPlace_t  s_sc_CanPlace = nullptr;
static fn_SC_TileFree_t  s_sc_TileFree = nullptr;
static fn_SC_CopyCtor_t  s_sc_CopyCtor = nullptr;
static fn_SC_FindLinkA_t s_sc_FindLinkA = nullptr;
static fn_SC_FindLinkB_t s_sc_FindLinkB = nullptr;
static fn_SC_SetTile_t   s_sc_SetTile = nullptr;
static fn_SC_SetOrient_t s_sc_SetOrient = nullptr;
static fn_SC_Register_t  s_sc_Register = nullptr;
static float* s_sc_pUVScale = nullptr;   // 0x14017214c
static float* s_sc_pConstA = nullptr;    // 0x140172140
static float* s_sc_pConstB = nullptr;    // 0x140172248
static bool s_scSpawnResolved = false;
static void EnsureSpawnHelpers() {
    if (s_scSpawnResolved) return;
    s_sc_FindMap   = reinterpret_cast<fn_SC_FindMap_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_sc_CanPlace  = reinterpret_cast<fn_SC_CanPlace_t>(ResolveAddr(Module::Exe, 0x14009f5d0));
    s_sc_TileFree  = reinterpret_cast<fn_SC_TileFree_t>(ResolveAddr(Module::Exe, 0x140120c30));
    s_sc_CopyCtor  = reinterpret_cast<fn_SC_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_sc_FindLinkA = reinterpret_cast<fn_SC_FindLinkA_t>(ResolveAddr(Module::Exe, 0x1400052c0));
    s_sc_FindLinkB = reinterpret_cast<fn_SC_FindLinkB_t>(ResolveAddr(Module::Exe, 0x14000c460));
    s_sc_SetTile   = reinterpret_cast<fn_SC_SetTile_t>(ResolveAddr(Module::Exe, 0x140126540));
    s_sc_SetOrient = reinterpret_cast<fn_SC_SetOrient_t>(ResolveAddr(Module::Exe, 0x140120db0));
    s_sc_Register  = reinterpret_cast<fn_SC_Register_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_sc_pUVScale  = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_sc_pConstA   = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172140));
    s_sc_pConstB   = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172248));
    s_scSpawnResolved = true;
}

// DoSpawn outcome census — `dispatched` above only counts CALLS, and DoSpawn
// bails on six separate conditions before an entity exists. Counting the call is
// not evidence of a spawn (project lesson: a counter that also counts the
// non-events is not evidence), so track where each attempt actually ends.
namespace {
struct DoSpawnCensus {
    long calls = 0;
    long noWorld = 0, worldBitSet = 0, noTemplate = 0;
    long densityRoll = 0;    // lost the density roll (the dominant EXPECTED outcome)
    long offMap = 0;         // jittered tile outside the map
    long cannotPlace = 0;    // terrain rejects it
    long tileTaken = 0;      // TileFree said no
    long allocFailed = 0;
    long created = 0;        // entity constructed AND registered
    long listClosed = 0;     // EntityList+0xE0 set -> 0x1400c48a0 DELETES instead of files
} g_doSpawn;
void DoSpawnReportForced(const char* tag) {
    const DoSpawnCensus& d = g_doSpawn;
    fprintf(stderr, "[DoSpawnCensus:%s] ", tag);
    fprintf(stderr,
        "calls=%ld -> CREATED=%ld | bail: noWorld=%ld worldBit=%ld noTemplate=%ld "
        "densityRoll=%ld offMap=%ld cannotPlace=%ld tileTaken=%ld allocFailed=%ld "
        "listClosed=%ld\n",
        d.calls, d.created, d.noWorld, d.worldBitSet, d.noTemplate, d.densityRoll,
        d.offMap, d.cannotPlace, d.tileTaken, d.allocFailed, d.listClosed);
    fflush(stderr);
}
}  // namespace

// Snapshot the DoSpawn outcome tallies at a named point (used by worldgen to
// separate the initial 5cc0 seed from later per-tick spawning).
extern "C" void SpawnComponent_ReportDoSpawnCensus(const char* tag) {
    DoSpawnReportForced(tag ? tag : "?");
    g_worldgenDone = true;
}

// NOTE for whoever debugs the seed-pass crash next: 0x140120db0 is NOT "set
// orientation" (the typedef name here is inherited and wrong) — it is
// SetGrowthProgress, which derives the MESH SCALE through the GrowthComponent's
// sibling links. Guarding both call sites on those links being populated (the test
// StartTown_GrowthInitResource uses) was tried on 2026-08-13 and is NOT the cause:
// BANISHED_LIVE_GROWTH is default-on, so the links ARE populated, the guard never
// fired, and the crash reproduced byte-identically. Ruled out — don't re-spend it.
void SpawnComponent_DoSpawn(void* selfP, void* resourceData, int entryIndex, char flag) {
    EnsureSpawnHelpers();
    uintptr_t self = (uintptr_t)selfP;
    ++g_doSpawn.calls;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) { ++g_doSpawn.noWorld; return; }
    uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;

    int* grid = *(int**)(gw + 0xf8);
    void* random = *(void**)(gw + 0xd8);
    if ((*(uint32_t*)(gw + 0xd0) >> 10) & 1) { ++g_doSpawn.worldBitSet; return; }

    int64_t* entry = (int64_t*)((int64_t)entryIndex * 0x10 + *(int64_t*)((uintptr_t)resourceData + 0x20));
    int64_t host = *entry;

    // Find the template component (type DAT_1401c77e8) on the entry entity
    void* templ = nullptr;
    {
        int count = *(int*)(host + 0x30);
        int64_t arr = *(int64_t*)(host + 0x28);
        for (int i = 0; i < count; i++) {
            void* comp = *(void**)(arr + (int64_t)i * 8);
            uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
            if (tid == GrowthDescription::TypeId()) {
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, GrowthDescription::TypeId())) templ = comp;
                break;
            }
        }
    }
    if (!templ) { ++g_doSpawn.noTemplate; return; }

    // Density roll
    float roll = reinterpret_cast<System::Random*>(random)->RandFloat();   // B.3.4b bridge
    float density = s_sc_pUVScale ? (*s_sc_pUVScale / (float)*(int*)((uintptr_t)templ + 0x48)) : 0.0f;
    if (roll > density) { ++g_doSpawn.densityRoll; return; }

    // Owner tile
    uint8_t mapBuf[8] = {0};
    if (s_sc_FindMap) s_sc_FindMap(*(uintptr_t*)(self + 0x10), mapBuf);
    int64_t mapComp = *(int64_t*)mapBuf;
    int16_t baseX = *(int16_t*)(mapComp + 0x28);
    int16_t baseY = *(int16_t*)(mapComp + 0x2a);

    int tol = *(int*)((uintptr_t)templ + 0x4c);
    int x = baseX + reinterpret_cast<System::Random*>(random)->RandTol(static_cast<int32_t>(tol));   // B.3.4b bridge
    int y = baseY + reinterpret_cast<System::Random*>(random)->RandTol(static_cast<int32_t>(tol));   // B.3.4b bridge
    if (x < 0 || y < 0) { ++g_doSpawn.offMap; return; }
    int mapSize = *grid;
    if (x >= mapSize || y >= mapSize) { ++g_doSpawn.offMap; return; }

    float ca = s_sc_pConstA ? *s_sc_pConstA : 0.0f;
    float cb = s_sc_pConstB ? *s_sc_pConstB : 0.0f;
    bool canPlace = s_sc_CanPlace &&
        s_sc_CanPlace(grid, x, y, 4, ca, cb, 0.0f, '\0', '\0', 0) != '\0';
    if (!canPlace) { ++g_doSpawn.cannotPlace; return; }
    if (!(s_sc_TileFree && s_sc_TileFree(templ, x, y) != '\0')) { ++g_doSpawn.tileTaken; return; }

    int placeMapSize = *grid;
    void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    void* newEntity = nullptr;
    if (mem && s_sc_CopyCtor) newEntity = s_sc_CopyCtor(mem, *entry);
    // Retail carries a null entity through the remaining steps (every one of them
    // is individually null-guarded, and the final Register would file a null IBP).
    // Bailing here reaches the same end state without adding a junk entry.
    if (!newEntity) { ++g_doSpawn.allocFailed; return; }

    int64_t entityIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&entityIbp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(newEntity));   // B.3.3 bridge

    // Place at tile index
    int64_t linkBufA = 0;
    void* pa = s_sc_FindLinkA ? s_sc_FindLinkA(entityIbp, &linkBufA) : nullptr;
    if (pa) {
        if (s_sc_SetTile) s_sc_SetTile(*(int64_t*)pa, placeMapSize * y + x);
    }

    // Reset orientation
    int64_t linkBufB = 0;
    void* pb = s_sc_FindLinkB ? s_sc_FindLinkB(entityIbp, &linkBufB) : nullptr;
    if (pb) {
        if (s_sc_SetOrient) s_sc_SetOrient(*(int64_t*)pb, 0.0f);
    }

    // Register with world entity list
    int64_t regIbp = entityIbp;
    if (s_sc_Register) {
        // EntityList::RegisterEntity (0x1400c48a0) opens with
        //   if (*(char*)((char*)list + 0xE0)) { InternalBasePtr::DeletePtr(ibp); return; }
        // i.e. a closed list DESTROYS the entity we just constructed instead of
        // filing it. Counting the call as `created` would then be a counter that
        // also counts the non-event, so read the flag first and tally separately —
        // a deleted entity that already joined Graphics::Scene during its ctor is
        // precisely the dangling scene node Scene::Clean faults on.
        void* list = *(void**)(gw + 0xe8);
        const bool closed = list && *((char*)list + 0xE0) != '\0';
        s_sc_Register(list, &regIbp,
                      *(int*)(*(int64_t*)(*(int64_t*)(self + 0x10) + 0x28) + 0x20));
        // Measurement only — the tail below stays byte-faithful to retail, which
        // also carries on through the deleted IBP (its finders return null there).
        if (closed) ++g_doSpawn.listClosed; else ++g_doSpawn.created;
    }

    // Optional random orientation
    if (flag != '\0') {
        int64_t b2 = 0;
        void* p2 = s_sc_FindLinkB ? s_sc_FindLinkB(entityIbp, &b2) : nullptr;
        if (p2 && *(int64_t*)p2 != 0) {
            int64_t b3 = 0;
            void* p3 = s_sc_FindLinkB ? s_sc_FindLinkB(entityIbp, &b3) : nullptr;
            float rf = reinterpret_cast<System::Random*>(random)->RandFloat();   // B.3.4b bridge
            if (p3 && s_sc_SetOrient) s_sc_SetOrient(*(int64_t*)p3, rf);
        }
    }
}

// ===== ENDGAME B.3.8 CIT — SpawnComponent as a REAL class (SpawnComponent.h) =====
// Before: a 48-slot hand-built SpawnComponent_LiveVtable filled at every create
// (slots 3 Serialize, 4 Startup, 9 Update), Inert_ScalarDtor in slot 0. After:
// the compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2).
#include "Object/factory_registration.h"
#include "Component/SpawnComponent.h"

// 0x140056030. Component() is the CALL 0x1401188b0; the 0x14015a670 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 0x88.
SpawnComponent::SpawnComponent() : field28(nullptr), field30(nullptr) {
    OrFlagsInCtor(0x88);
}

// [0] 0x14000c350. PORT GAP (retail frees the +0x28 timer array first) — the
// tree's Inert_ScalarDtor never did; kept (SpawnComponent.h).
Object::Base* SpawnComponent::DeletingDtor(unsigned flags) {
    this->~SpawnComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// SpawnComponent::Serialize (vtable slot [3], +0x18, original 0x140055ee0 —
// Ghidra mislabels it SpawnComponent_Shutdown; the body is a stream
// serializer, not a dtor).
// ============================================================
// Fields (verified against retail decompile/disassembly at 0x140055ee0):
//   +0x30  int             per-entry timer count (also written BACK by the
//                           resize helper itself, not by an explicit store).
//   +0x28  Time[count]     per-entry accumulation timers, stride 8 (see
//                           SpawnComponent_Startup / SpawnComponent_Update),
//                           load-side resized via the same stride-8 resizer
//                           (0x140001bb0, GameColl::AsArray<GameColl::Ptr8>::SetSize)
//                           used by Startup's body-init.
//   On load only, retail then reconciles the restored count against the LIVE
//   SpawnDescription's entry count (desc+0x28, desc validated by
//   IsDerivedFrom(desc, SpawnDescription typeid) — DAT_1401c4918, already
//   resolved as s_pSpawnResourceTypeId): if they differ, resize again to
//   match the description currently in effect.
void SpawnComponent::Persist(void* stream) {
    SpawnComponent_InitFuncs();
    if (!stream) return;
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    uint8_t* comp = reinterpret_cast<uint8_t*>(this);
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // True when the serializer is READING (retail reads the stream's own +8
    // field for this component, equivalent to the serializer's first field).
    auto Loading = [](void* s) {
        return static_cast<int>(reinterpret_cast<IO::Serializer*>(s)->GetDirection()) == 0;   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
    };

    int count = *reinterpret_cast<int*>(comp + 0x30);
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(count, nullptr);   // B.3.4 bridge
    if (Loading(ser))
        GameColl::AsArray<GameColl::Ptr8>(comp + 0x28).SetSize(count);  // also writes comp+0x30 = count

    uint8_t* timer = *reinterpret_cast<uint8_t**>(comp + 0x28);
    int n = *reinterpret_cast<int*>(comp + 0x30);
    for (int i = 0; i < n && timer; ++i, timer += 8)
        reinterpret_cast<System::Time*>(timer)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge

    if (Loading(stream)) {
        void* desc = *reinterpret_cast<void**>(comp + 0x18);
        void* validated = nullptr;
        if (desc && s_fn_IsDerived &&
            s_fn_IsDerived(desc, SpawnDescription::TypeId()))
            validated = desc;
        int descCount = validated
            ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(validated) + 0x28)
            : 0;
        if (*reinterpret_cast<int*>(comp + 0x30) != descCount)
            GameColl::AsArray<GameColl::Ptr8>(comp + 0x28).SetSize(descCount);
    }

    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}
// ============================================================
// SpawnComponent::Startup (vtable slot [4], +0x20, original SpawnComponent_Serialize
// / OnCreate 0x140055e90)
// ============================================================
// Body-init: size the per-entry timer array at self+0x28 to the SpawnDescription's
// entry count (desc+0x28), via the registered stride-8 Time-array resizer
// (FUN_140001bb0). The array descriptor occupies self+0x28 (data ptr) / +0x30
// (count) / +0x34 (capacity) — the two pointers the Ctor zeroes. Called at spawn
// by ComponentEntity_StartupComponents (SpawnComponent flags 0x88 have bit 7).
//
// The original gates the count on IsDerivedFrom(_unk18, SpawnDescription); in
// standalone that can be unreliable, so we trust comp+0x18 (the description the
// copy-ctor stored), matching growthcomponent.cpp's documented precedent.
void SpawnComponent::Startup() {
    SpawnComponent_InitFuncs();
    uint8_t* comp = reinterpret_cast<uint8_t*>(this);

    void* desc = *reinterpret_cast<void**>(comp + 0x18);   // _unk18 = SpawnDescription
    if (!desc) return;                                     // not yet wired to a description

    int entryCount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x28);
    if (entryCount <= 0) return;

    // Resize the timer array (descriptor at comp+0x28). Zero-initialised entries.
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(comp + 0x28)).SetSize(entryCount);
}

// [9] 0x140055ad0. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void SpawnComponent::Update(void* timeV) {
    int64_t* pDeltaTime = static_cast<int64_t*>(timeV);
    SpawnComponent_InitFuncs();

    uint8_t* comp = reinterpret_cast<uint8_t*>(this);

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless the OnCreate/Startup ran. The per-entry timer array at
    // +0x28 (data ptr) / +0x30 (count) is allocated by SpawnComponent_Startup (step
    // 2 of the recipe), which the spawn pipeline invokes via vtable+0x20 in
    // ComponentEntity_StartupComponents. As belt-and-suspenders, if +0x28 is still
    // null but the description(+0x18) is present, run the body-init now (idempotent).
    // The loop below indexes *(self+0x28) unconditionally, so bail if still unset.
    if (*(uintptr_t*)(comp + 0x28) == 0) {
        if (*(uintptr_t*)(comp + 0x18) != 0)
            SpawnComponent::Startup();
        if (*(uintptr_t*)(comp + 0x28) == 0) {
            return;
        }
    }

    // CheckPtr on owner IBP, then get linked object
    uintptr_t ownerIBP = (uintptr_t)this + 0x10;
    uintptr_t linkedObjPtr = 0;
    if (s_fn_GetLinkedObject)
        s_fn_GetLinkedObject(*(void**)ownerIBP, &linkedObjPtr);

    // Get resource data via IsDerivedFrom check on _unk18
    void* baseObj = _description;
    if (!fn_IsDerivedFrom) {
        return;
    }
    bool isDerived = fn_IsDerivedFrom(baseObj, SpawnDescription::TypeId());
    void* resourceData = isDerived ? baseObj : nullptr;
    // [STANDALONE TRUST] The factory tree can be incomplete in standalone, making
    // IsDerivedFrom on a description unreliable (see growthcomponent.cpp Startup).
    // _unk18 is the SpawnDescription the copy-ctor stored at +0x18 — trust it.
    if (!resourceData && baseObj) resourceData = baseObj;
    if (!resourceData) {
        return;
    }

    // CheckPtr on linked object IBP before reading growth
    // [BRING-UP GUARD] The owner needs a sibling NaturalResourceComponent (found by
    // GetLinkedObject 0x1400052c0) for the growth read; bail rather than null-deref
    // if a spawner lacks one in standalone.
    if (linkedObjPtr == 0) {
        return;
    }
    float currentGrowth = *(float*)(linkedObjPtr + 0x28);


    // Get spawn entry count from resourceData+0x28
    int entryCount = *(int*)((uintptr_t)resourceData + 0x28);
    if (entryCount <= 0) {
        return;
    }

    uintptr_t entryArrayBase = *(uintptr_t*)((uintptr_t)resourceData + 0x20);

    for (int i = 0; i < entryCount; i++) {
        uintptr_t entry = entryArrayBase + (uintptr_t)i * 0x10;

        float minGrowth = *(float*)(entry + 0x08);
        float maxGrowth = *(float*)(entry + 0x0C);

        if (currentGrowth < minGrowth || currentGrowth > maxGrowth) {
            continue;
        }

        // Accumulate delta time
        int64_t byteOffset = (int64_t)i * 8;
        int64_t* pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)this + 0x28) + byteOffset);
        *pTimer += *pDeltaTime;


        // Get spawn template data from the entry
        uintptr_t templateData = *(uintptr_t*)entry;

        // Search template's component list for the spawn interval component
        int componentCount = *(int*)(templateData + 0x30);
        void* spawnIntervalComp = nullptr;

        for (int j = 0; j < componentCount; j++) {
            void* comp = *(void**)(*(uintptr_t*)(templateData + 0x28) + (uintptr_t)j * 8);
            uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge

            if (typeId == GrowthDescription::TypeId()) {
                if (fn_IsDerivedFrom(comp, GrowthDescription::TypeId())) {
                    spawnIntervalComp = comp;
                }
                break;
            }
        }

        if (!spawnIntervalComp) {
            continue;
        }

        // Convert spawn interval to ticks
        float intervalSecs = *(float*)((uintptr_t)spawnIntervalComp + 0x28) * SECS_PER_SEASON;
        int64_t intervalTicks = (int64_t)((double)intervalSecs / *g_Timer_secondsPerTick);

        pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)this + 0x28) + byteOffset);
        while (intervalTicks > 0 && *pTimer >= intervalTicks) {
            *pTimer -= intervalTicks;
            if (s_fn_DoSpawn) {
                // Positive assertion: only the per-tick arm can print this,
                // one-shot.
                if (g_worldgenDone) {
                    static bool s_firstTick = true;
                    if (s_firstTick) {
                        s_firstTick = false;
                        fprintf(stderr, "[SpawnLive] first PER-TICK DoSpawn dispatch\n");
                        fflush(stderr);
                    }
                }
                s_fn_DoSpawn(this, resourceData, i, 0);
            }
            pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)this + 0x28) + byteOffset);
        }
    }
}

// create 0x14000c170: MemAlloc(0x38, 8), then the ctor.
void* SpawnComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SpawnComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) SpawnComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* SpawnComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) SpawnComponent());
}

DEFINE_EXE_VTABLE(SpawnComponent, "_ZTV14SpawnComponent")
DEFINE_EXE_TYPE(SpawnComponent, "Component")
