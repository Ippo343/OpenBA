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
#include "Component/GrowthComponent.h"
#include "Component/NaturalResourceComponent.h"
#include "Component/MapComponent.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
#include "Component/coll_bridge.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "system/Timer.h"   // ENDGAME B.3.9 SS2: System::Timer::_secondsPerTick
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"          // ActCNR_SetupModelAtCell_126540, ActCNR_SetGrowthProgress_120db0
#include "Work/exe_bodies.h"            // WorkLeaf_FootprintOwnerCheck_120c30
#include "Player/exe_bodies.h"          // PlayerPath_CharComp_f5d0
#include "Entity/componententity_funcs.h"   // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Game/exe_constants.h"

// ============================================================
// Helpers
// ============================================================

static constexpr float SECS_PER_SEASON = 300.0f;

// 0x1400052c0 FindComponent<NaturalResourceComponent> — the owner's growth
// 0x140055780 SpawnComponent::DoSpawn (below) — performs actual spawn operation

// FUN_140001bb0 — dynamic Time-array resize (stride 8). Sizes the per-entry timer
// array in the OnCreate/Startup body-init. ENDGAME B.3.2 step 2:
// converted to GameColl::AsArray<GameColl::Ptr8>(h).SetSize(n) (same stride/align/
// zero-tail as Collection::Array<void*>::SetSize).

// The stream serializer (vtable slot [3]) validates with ?IsDerivedFrom@Base@Object@@
// (import slot 0x140152a98), called on the object.

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
// SpawnComponent initial seed — SpawnComponent::InitialSpawn (0x140055cc0)
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
void SpawnComponent::InitialSpawn() {
    // Sibling NaturalResourceComponent (FindComponent 0x1400052c0) — carries the
    // growth value at +0x28.
    NaturalResourceComponent* natRes = _entity->FindComponent<NaturalResourceComponent>();
    if (natRes == nullptr) return;
    const float growth = natRes->resourceScale;

    // SpawnDescription at +0x18 (IsDerivedFromOrTrust — same policy as Update).
    Description* baseObj = _description;
    if (!baseObj) return;
    SpawnDescription* spawnDesc = static_cast<SpawnDescription*>(baseObj);

    const int entryCount = spawnDesc->_entryCount;
    const SpawnDescription::Entry* entries = spawnDesc->_entries;
    if (entryCount <= 0 || !entries) return;

    for (int i = 0; i < entryCount; ++i) {
        const SpawnDescription::Entry& entry = entries[i];
        if (growth < entry._minSpawnAge || growth > entry._maxSpawnAge) continue;

        // The spawn template entity; find its GrowthDescription for the divisor.
        const ComponentDescription* host = entry._template;
        if (!host) continue;
        GrowthDescription* growthDesc = nullptr;
        const int compCount = host->_subDescCount;
        Description** compArr = host->_subDescs;
        for (int j = 0; j < compCount && compArr; ++j) {
            Description* c = compArr[j];
            if (!c) continue;
            if (c->Type() == GrowthDescription::TypeId()) {
                growthDesc = static_cast<GrowthDescription*>(c);   // trust it; IsDerivedFrom is unreliable here
                break;
            }
        }
        if (!growthDesc) continue;

        const float perSpawn = growthDesc->_f20;
        if (!(perSpawn > 0.0f)) continue;
        const int repeats = (int)(growth / perSpawn) + 1;
        for (int r = 0; r < repeats; ++r) {
            DoSpawn(spawnDesc, i, 1);   // flag=1 => random orientation
        }
    }
}

// B4 FORWARD: 1 outside caller (Game/starttown.cpp) -> SpawnComponent::InitialSpawn
void SpawnComponent_InitialSpawn(SpawnComponent* self) {
    if (!self) return;
    self->InitialSpawn();
}


// ============================================================
// SpawnComponent::DoSpawn (0x140055780; ComponentEntity_impl_5780): probabilistically
//   spawn one child entity (animal/resource) near the owner from a spawn-table entry
// ============================================================
// Faithful port of the Ghidra decompilation. desc->_entries holds the spawn
// table; entryIndex selects the template. A random roll gated by the
// template's density (g_fUVScale / GrowthDescription+0x48) decides whether to spawn; the tile is
// chosen by RandTol jitter around the owner tile and validated, then a copy of the
// template entity is instantiated, placed, oriented, and registered.
// The DoSpawn closure, by retail address:
//   0x140004680 FindComponent<MapComponent>       (the owner's map component)
//   0x14009f5d0 PlayerPath_CharComp_f5d0          (can-place test)
//   0x140120c30 WorkLeaf_FootprintOwnerCheck_120c30 (tile-free test; the door read AL)
//   0x1400c3720 ComponentEntity_CtorFromDescription
//   0x1400052c0 FindComponent<NaturalResourceComponent>  0x14000c460 FindComponent<GrowthComponent>
//   0x140126540 ActCNR_SetupModelAtCell_126540    0x140120db0 ActCNR_SetGrowthProgress_120db0
//   0x1400c48a0 EntityList_RegisterEntity_Impl
//   .rdata 0x14017214c = 1.0 (g_fUVScale), 0x140172140 = 0.98, 0x140172248 = 1000.0
static float SC_RdataF32(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

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
void SpawnComponent::DoSpawn(SpawnDescription* desc, int entryIndex, char randomGrowth) {
    ++g_doSpawn.calls;
    if (GameUpdateLoop::_singleton == nullptr) { ++g_doSpawn.noWorld; return; }
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;

    // LEFT: GameUpdateLoop::m_game is void* (GAME-a): the map goes to the
    // can-place test as the int* it has always been.
    int* grid = reinterpret_cast<int*>(gw->m_game);
    System::Random* random = gw->m_random;
    if ((gw->m_gameFlags >> 10) & 1) { ++g_doSpawn.worldBitSet; return; }

    const SpawnDescription::Entry* entry = &desc->_entries[entryIndex];
    const ComponentDescription* host = entry->_template;

    // Find the template's GrowthDescription (cell DAT_1401c77e8) among its sub-descriptions
    GrowthDescription* templ = nullptr;
    {
        int count = host->_subDescCount;
        Description** arr = host->_subDescs;
        for (int i = 0; i < count; i++) {
            Description* comp = arr[i];
            if (comp->Type() == GrowthDescription::TypeId()) {
                if (Object::IsDerivedFromNullSafe(comp, GrowthDescription::TypeId()))
                    templ = static_cast<GrowthDescription*>(comp);
                break;
            }
        }
    }
    if (!templ) { ++g_doSpawn.noTemplate; return; }

    // Density roll
    float roll = random->RandFloat();
    float density = kRdata_14017214c / (float)templ->_i48;
    if (roll > density) { ++g_doSpawn.densityRoll; return; }

    // Owner tile (FindComponent<MapComponent> 0x140004680)
    MapComponent* map = _entity->FindComponent<MapComponent>();
    int16_t baseX = map->tileX0;
    int16_t baseY = map->tileY0;

    int tol = templ->_i4c;
    int x = baseX + random->RandTol(static_cast<int32_t>(tol));
    int y = baseY + random->RandTol(static_cast<int32_t>(tol));
    if (x < 0 || y < 0) { ++g_doSpawn.offMap; return; }
    int mapSize = *grid;
    if (x >= mapSize || y >= mapSize) { ++g_doSpawn.offMap; return; }

    float ca = SC_RdataF32(kRdata_140172140);
    float cb = SC_RdataF32(kRdata_140172248);
    bool canPlace =
        PlayerPath_CharComp_f5d0(grid, x, y, 4, ca, cb, 0.0f, '\0', '\0', 0);
    if (!canPlace) { ++g_doSpawn.cannotPlace; return; }
    if (!(static_cast<char>(WorkLeaf_FootprintOwnerCheck_120c30(
              reinterpret_cast<intptr_t>(templ), x, y)) != '\0')) { ++g_doSpawn.tileTaken; return; }

    int placeMapSize = *grid;
    ComponentEntity* mem = static_cast<ComponentEntity*>(
        System::MemoryAllocator::MemAlloc(0x48, 8));
    ComponentEntity* newEntity = nullptr;
    if (mem) newEntity = ComponentEntity_CtorFromDescription(mem, entry->_template);
    // Retail carries a null entity through the remaining steps (every one of them
    // is individually null-guarded, and the final Register would file a null IBP).
    // Bailing here reaches the same end state without adding a junk entry.
    if (!newEntity) { ++g_doSpawn.allocFailed; return; }

    Object::InternalPtr<ComponentEntity> entityIbp(newEntity);

    // Place at tile index (NaturalResourceComponent, FindComponent 0x1400052c0)
    NaturalResourceComponent* natRes = newEntity->FindComponent<NaturalResourceComponent>();
    ActCNR_SetupModelAtCell_126540(reinterpret_cast<int64_t>(natRes), placeMapSize * y + x);

    // Reset the growth (GrowthComponent, FindComponent 0x14000c460)
    GrowthComponent* growth = newEntity->FindComponent<GrowthComponent>();
    ActCNR_SetGrowthProgress_120db0(reinterpret_cast<int64_t>(growth), 0.0f);

    // Register with world entity list
    Object::InternalPtr<ComponentEntity> regIbp = entityIbp;
    {
        // EntityList::RegisterEntity (0x1400c48a0) opens with
        //   if (*(char*)((char*)list + 0xE0)) { InternalBasePtr::DeletePtr(ibp); return; }
        // i.e. a closed list DESTROYS the entity we just constructed instead of
        // filing it. Counting the call as `created` would then be a counter that
        // also counts the non-event, so read the flag first and tally separately —
        // a deleted entity that already joined Graphics::Scene during its ctor is
        // precisely the dangling scene node Scene::Clean faults on.
        // LEFT: GameUpdateLoop::m_entityList is void* (GAME-a): its +0xE0 byte by offset.
        void* list = gw->m_entityList;
        const bool closed = list && reinterpret_cast<const char*>(list)[0xE0] != '\0';
        EntityList_RegisterEntity_Impl(list, &regIbp, _entity->_description->_category);
        // Measurement only — the tail below stays byte-faithful to retail, which
        // also carries on through the deleted IBP (its finders return null there).
        if (closed) ++g_doSpawn.listClosed; else ++g_doSpawn.created;
    }

    // Optional random growth
    if (randomGrowth != '\0') {
        if (newEntity->FindComponent<GrowthComponent>() != nullptr) {
            GrowthComponent* g3 = newEntity->FindComponent<GrowthComponent>();
            float rf = random->RandFloat();
            ActCNR_SetGrowthProgress_120db0(reinterpret_cast<int64_t>(g3), rf);
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
SpawnComponent::SpawnComponent() : timers(nullptr), timerCount(0), timerCapacity(0) {
    OrFlagsInCtor(0x88);
}

// ============================================================
// SpawnComponent::Serialize (vtable slot [3], +0x18, original 0x140055ee0 —
// Ghidra mislabels it SpawnComponent_Shutdown; the body is a stream
// serializer, not a dtor).
// ============================================================
// Fields (verified against retail decompile/disassembly at 0x140055ee0):
//   timerCount  per-entry timer count (also written BACK by the resize helper
//               itself, not by an explicit store).
//   timers      Time[count] per-entry accumulation timers, load-side resized via
//               the same stride-8 resizer (0x140001bb0,
//               GameColl::AsArray<GameColl::Ptr8>::SetSize) used by Startup.
//   On load only, retail then reconciles the restored count against the LIVE
//   SpawnDescription's entry count (desc->_entryCount, desc validated by
//   IsDerivedFrom(desc, SpawnDescription typeid) — DAT_1401c4918): if they
//   differ, resize again to match the description currently in effect.
void SpawnComponent::Persist(void* stream) {
    if (!stream) return;
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // Persist's stream is void*

    int count = timerCount;
    ser.Serialize(count, nullptr);
    if (static_cast<int>(ser.GetDirection()) == 0)   // IO::Serializer::GetDirection, retail 0x180006210
        GameColl::AsArray<GameColl::Ptr8>(&timers).SetSize(count);  // also writes timerCount = count

    System::Time* timer = timers;
    int n = timerCount;
    for (int i = 0; i < n && timer; ++i, ++timer)
        timer->Serialize(ser);

    // LEFT (a tree defect, listed): this test reads the STREAM as a Serializer, so it
    // answers from the stream's vtable word and is never "loading"; retail tests the
    // stream's direction here (0x140055ee0) and reconciles on every load.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(stream)->GetDirection()) == 0) {
        Description* desc = _description;
        SpawnDescription* validated = nullptr;
        if (desc && desc->IsDerivedFrom(SpawnDescription::TypeId()))
            validated = static_cast<SpawnDescription*>(desc);
        int descCount = validated ? validated->_entryCount : 0;
        if (timerCount != descCount)
            GameColl::AsArray<GameColl::Ptr8>(&timers).SetSize(descCount);
    }
}
// ============================================================
// SpawnComponent::Startup (vtable slot [4], +0x20, original SpawnComponent_Serialize
// / OnCreate 0x140055e90)
// ============================================================
// Body-init: size the per-entry timer array to the SpawnDescription's entry count,
// via the stride-8 Time-array resizer (FUN_140001bb0). Called at spawn by
// ComponentEntity_StartupComponents (SpawnComponent flags 0x88 have bit 7).
//
// The original gates the count on IsDerivedFrom(_description, SpawnDescription); in
// standalone that can be unreliable, so we trust _description (the description the
// copy-ctor stored), matching growthcomponent.cpp's documented precedent.
void SpawnComponent::Startup() {
    Description* desc = _description;
    if (!desc) return;                                     // not yet wired to a description

    int entryCount = static_cast<SpawnDescription*>(desc)->_entryCount;
    if (entryCount <= 0) return;

    // Resize the timer array. Zero-initialised entries.
    GameColl::AsArray<GameColl::Ptr8>(&timers).SetSize(entryCount);
}

// [9] 0x140055ad0. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
void SpawnComponent::Update(void* timeV) {
    const System::Time* pDeltaTime = reinterpret_cast<const System::Time*>(timeV);   // B.3.8 bridge (Update takes void*)

    // [LIVE-COMPONENT BRING-UP GUARD] On an inert-spawned standalone component the
    // body is zeroed unless the OnCreate/Startup ran. The per-entry timer array is
    // allocated by SpawnComponent::Startup, which the spawn pipeline invokes via
    // vtable+0x20 in ComponentEntity_StartupComponents. As belt-and-suspenders, if
    // the timers are still null but the description is present, run the body-init
    // now (idempotent). The loop below indexes the timers unconditionally, so bail
    // if still unset.
    if (timers == nullptr) {
        if (_description != nullptr)
            SpawnComponent::Startup();
        if (timers == nullptr) {
            return;
        }
    }

    // The owner's NaturalResourceComponent (FindComponent 0x1400052c0)
    NaturalResourceComponent* natRes = _entity->FindComponent<NaturalResourceComponent>();

    // The SpawnDescription. Retail validates it with IsDerivedFrom; [STANDALONE
    // TRUST] the factory tree can be incomplete in standalone, making IsDerivedFrom
    // on a description unreliable (see growthcomponent.cpp Startup), so the tree
    // trusts the description the copy-ctor stored (its answer never changed the value).
    Description* desc = _description;
    SpawnDescription* resourceData = static_cast<SpawnDescription*>(desc);
    if (!resourceData) {
        return;
    }

    // [BRING-UP GUARD] The owner needs a sibling NaturalResourceComponent for the
    // growth read; bail rather than null-deref if a spawner lacks one in standalone.
    if (natRes == nullptr) {
        return;
    }
    float currentGrowth = natRes->resourceScale;


    int entryCount = resourceData->_entryCount;
    if (entryCount <= 0) {
        return;
    }

    const SpawnDescription::Entry* entries = resourceData->_entries;

    for (int i = 0; i < entryCount; i++) {
        const SpawnDescription::Entry& entry = entries[i];

        if (currentGrowth < entry._minSpawnAge || currentGrowth > entry._maxSpawnAge) {
            continue;
        }

        // Accumulate delta time
        timers[i]._ticks += pDeltaTime->_ticks;


        // The spawn template
        const ComponentDescription* templateData = entry._template;

        // Search the template's sub-descriptions for its GrowthDescription
        int componentCount = templateData->_subDescCount;
        GrowthDescription* spawnIntervalComp = nullptr;

        for (int j = 0; j < componentCount; j++) {
            Description* comp = templateData->_subDescs[j];

            if (comp->Type() == GrowthDescription::TypeId()) {
                if (Object::IsDerivedFromNullSafe(comp, GrowthDescription::TypeId())) {
                    spawnIntervalComp = static_cast<GrowthDescription*>(comp);
                }
                break;
            }
        }

        if (!spawnIntervalComp) {
            continue;
        }

        // Convert spawn interval to ticks
        float intervalSecs = spawnIntervalComp->_f28 * SECS_PER_SEASON;
        int64_t intervalTicks = (int64_t)((double)intervalSecs / ::System::Timer::_secondsPerTick);

        while (intervalTicks > 0 && timers[i]._ticks >= intervalTicks) {
            timers[i]._ticks -= intervalTicks;
            {
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
                DoSpawn(resourceData, i, 0);
            }
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

DEFINE_EXE_TYPE(SpawnComponent, "Component")
