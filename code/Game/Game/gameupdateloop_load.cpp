// code/Game/Game/gameupdateloop_load.cpp
// GameUpdateLoop::Load (exe 0x140096b00) — the in-game WORLD BUILD (M5).
//
// This is the Stage-C cliff from project_newgame_path_roadmap: the loader chain
// reaches GameUpdateLoop::Load on "New Game → OK", and this function constructs
// the ~28 in-game subsystems that GameUpdateLoop::{Startup,Update,Draw} operate on.
// Until it ran, m_game (+0xF8) was null and those methods held behind a guard.
//
// FIRST SLICE (milestone 5a): build every subsystem object so m_game is non-null
// and the loop runs. DEFERRED to later slices:
//   - the Object::ExternalList::Find resource loads (Material/Environment/MusicList/
//     SkyMesh/StringTables) — left null; consumers guarded. (exe-typed ones are
//     factory-cascade landmines; Runtime-typed ones are cheap to re-add once the
//     skeleton is proven.) See project_gameupdateloop_load_map.
//     NO LONGER deferred: Diseases (step 1, +0xA8), Profession (step 2, +0xB0),
//     HelpTopics (step 4, +0xA0) — each behind its own env gate.
//   - the SkyMesh MeshInstance (+0x108, step 28) — needs vtable 0x14015e240 registered.
//   - the deep worldgen (step 31): FUN_1400b0230 / CreateNaturalResource_impl_ef20 /
//     FUN_1400a3300 — real terrain + citizens; milestone 5b.
//
// All subsystem ctor bodies below were verified against Ghidra disassembly
// (m5-load-ctor-drafts workflow, 2026-05-30) — exact field offsets in comments.

#include "GameUpdateLoop.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
#include "Component/component_factory_base.h"  // ComponentFactory::the live gate
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <atomic>
#include <windows.h>  // GetCurrentThreadId (double-dispatch diagnostic)
#include "rt_port.h"                        // RT_DATA_SLOT (never caches a null resolve)
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Externs from sibling reimpl TUs (already registered/defined)
// ============================================================
extern "C" void* Reimpl_Geometry_Init(void* self);            // FUN_1400b00e0 (serialize_arrays.cpp)
extern "C" void  Reimpl_Array_Resize_Stride04(void*, int);    // FUN_1400021d0 (serialize_resize_helpers.cpp)
extern "C" void  WorldGen_BuildTerrain(void* wg, void* gen, int sizeIdx, uint32_t seed); // worldgen.cpp (M5 5b)
extern "C" void  MapTileGrid_Build(void* game, int dim);  // maptilegrid.cpp (M5 5c — tile grid Game+0x80)
extern "C" void  Pathfinder_Seed(void* game, int dim);    // pathfinder_seed.cpp (task #34 — pathfinder @ game+0x68)
extern "C" void  Region_Seed(void* game, int dim);        // region_seed.cpp (keystone A — region map @ game+0x70)
extern "C" void  CharManager_Seed();                      // charmanager_seed.cpp (move-tick tail — char spatial-hash mgr @ GW+0x88)
extern "C" void  StartTown_ComputeCamera(void* startConfig, void* game, float camPosOut[3]); // starttown.cpp (M5 5c — camera)
extern "C" void  StartTown_SpawnEntities(void* startConfig, void* world);                    // starttown.cpp (M5 5c — ECS spawn)

// ============================================================
// Load-specific Runtime resolves not yet in engine_funcs.h
// ============================================================

// --- Step 32 (AchievementInstance) -----------------------------------------
// ??0AchievementInstance@Achievement@@QEAA@XZ            (Runtime 0x18009e3b0)
// ?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z
//                                                        (Runtime 0x18009e440)
// ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A      (BaseTypeInfo; +0 is the type id)
typedef void* (*fn_AchievementInstance_Ctor_t)(void* self);
typedef void  (*fn_AchievementInstance_SetAchievement_t)(void* self, void* achievement);
static fn_AchievementInstance_Ctor_t           s_fn_AchievementInstance_Ctor = nullptr;
static fn_AchievementInstance_SetAchievement_t s_fn_AchievementInstance_SetAchievement = nullptr;
static const uint32_t*                         s_AchievementTypeId = nullptr;

static void EnsureAchievementFuncs() {
    static bool done = false;
    if (done) return;
    done = true;
    s_fn_AchievementInstance_Ctor = (fn_AchievementInstance_Ctor_t)
        ResolveExport("??0AchievementInstance@Achievement@@QEAA@XZ");
    s_fn_AchievementInstance_SetAchievement = (fn_AchievementInstance_SetAchievement_t)
        ResolveExport("?SetAchievement@AchievementInstance@Achievement@@QEAAX"
                      "V?$ExternalPtr@VAchievement@1@@Object@@@Z");
    s_AchievementTypeId = (const uint32_t*)
        ResolveExport("?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A");
}

typedef void* (*fn_Random_Seeded_Ctor_t)(void* self, uint32_t seed);
static fn_Random_Seeded_Ctor_t s_fn_Random_Seeded_Ctor = nullptr;   // ??0Random@System@@QEAA@I@Z
typedef void* (*fn_Picking_Scene_Ctor_t)(void* self);
static fn_Picking_Scene_Ctor_t s_fn_Picking_Scene_Ctor = nullptr;   // ??0Scene@Picking@@QEAA@XZ
typedef void* (*fn_UI_Ctor_t)(void* self);
static fn_UI_Ctor_t s_fn_UI_Ctor = nullptr;                         // ??0UserInterface@UI@@QEAA@XZ
typedef void* (*fn_AnimMgr_Ctor_t)(void* self);
static fn_AnimMgr_Ctor_t s_fn_AnimMgr_Ctor = nullptr;               // ??0AnimationManager@Graphics@@QEAA@XZ
typedef void* (*fn_SaveLoad_Ctor_t)(void* self);
static fn_SaveLoad_Ctor_t s_fn_SaveLoad_Ctor = nullptr;             // ??0SaveLoad@Save@@QEAA@XZ
typedef uint32_t (*fn_Random_RandFull_t)(void* self);
static fn_Random_RandFull_t s_fn_Random_RandFull = nullptr;         // ?Rand@Random@System@@QEAAIXZ (no-arg)
typedef void (*fn_SavedState_Dtor_t)(void* self);
static fn_SavedState_Dtor_t s_fn_SavedState_Dtor = nullptr;         // ??1SavedState@Save@@QEAA@XZ

// code/Game/Game/gameupdateloop_serialize.cpp — the ported vtable +0x38 slot
// (SaveRestore); the LOAD-FROM-SAVE branch below calls it directly.
extern "C" void GameUpdateLoop_SaveRestore(void* self, char save,
                                           void* savedState, int* outParam);
extern "C" void GameUpdateLoop_SaveRoundtripCheck(void* self, void* loadedState);

// code/Game/Game/eventlog_achievement.cpp — the achievement award TOAST (exe
// 0x140096960). Installed on the AchievementManager at step 30 below.
extern "C" void EventLog_OnAchievementAwarded(int achievementId);

// ============================================================
// Step 2 — ProfessionList load (WaveH2; env BANISHED_LIVE_PROFESSIONLIST)
// ============================================================
// The original GameUpdateLoop::Load (step 2, @0x140096baa) loads the ProfessionList
// from "Game/Profession/Profession.rsc" via Object::ExternalList::Find and stores it
// at +0xB0 (param_1[0x16]). This single object is the CONVERGENT KEYSTONE for the
// whole citizen economy: it carries the DEFAULT PROFESSION at +0x30 (→ citizen+0x1f0
// at spawn, FUN_140117080) and profRef2 at +0x38 (the workplace-registration gate key
// in WorkPlaceComponent_impl_f670). Every economy path (housing/workplace finders,
// citizen spawn) derefs gw+0xb0; with it null in standalone they all bail/crash.
//
// The ProfessionList External (vtable 0x14015ef50, factory 0x14001e5c0) + its Serialize
// (loads the 4 ExternalPtr<Profession> refs) are already ported (professionlist.cpp);
// they were just never DRIVEN. We load it with ExternalFactory::FindOrLoad — the same
// primitive StartTown_ResolveTemplate / LoadDialogResource use successfully — rather
// than ExternalList::Find(param_1[1]); the global-factory load works in standalone and
// Profession resource resolution is already proven (CropField/Orchard/Pasture).
// The ProfessionList keystone loads crash-free (the real blocker was a missing
// fn-pointer null-check in GameUpdateLoop_Startup, not the object — see
// professionlist.cpp). gw+0xb0 carries the default profession (+0x30) + profRef2
// (+0x38) the citizen-economy finders key on.

// Standalone ProfessionList factory (professionlist.cpp) — allocates the 0x50
// External object + Init (vtable + zeroed fields), NO FindOrLoad/Serialize.
extern "C" void* ProfessionList_Factory(void);

static void LoadProfessionList_Step2(uint8_t* p) {

    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        *(void**)(p + 0xB0) = nullptr;
        fprintf(stderr, "[GUL::ProfList] SKIP — factory unavailable (factory=%p)\n",
                g_pExternalFactorySingleton ? *g_pExternalFactorySingleton : nullptr);
        fflush(stderr);
        return;
    }

    void* factory = *g_pExternalFactorySingleton;
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guard
    // above still gates the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    System::Filename fname("Game/Profession/Profession.rsc");
    void* profList = const_cast<::Object::External*>(
        efactory->FindOrLoad(fname));                                       // B.3.3 bridge
    *(void**)(p + 0xB0) = profList;

    void* defProf  = profList ? *(void**)((uint8_t*)profList + 0x30) : nullptr;
    void* profRef2 = profList ? *(void**)((uint8_t*)profList + 0x38) : nullptr;
    fprintf(stderr, "[GUL::ProfList] gw+0xb0=%p defaultProf(+0x30)=%p profRef2(+0x38)=%p\n",
            profList, defProf, profRef2);
    fflush(stderr);
}

// ============================================================
// Step 4 — HelpTopics load (help-dialog bring-up 2026-07-31)
// ============================================================
// Retail (0x140096c80..0x140096cdd) does
//   ExternalList::Find(param_1[1], &out, "Game/Help/HelpTopics.rsc", ExternalList typeId)
// and stores the result at +0xA0 — an Object::ExternalList whose resources are the
// per-topic UI::StringTables (text 0 = link key, 1 = combo title, 2 = page body).
// That store was deferred with the rest of the step 1-4 Finds, so gw+0xa0 stayed null
// and HelpUI::Slot4 populated an empty combo — the reason the in-game help dialog
// renders blank (verified live: `[HelpUI] Slot4 ... topics=0000000000000000`).
//
// Loaded through ExternalFactory::FindOrLoad rather than ExternalList::Find on the
// resource list at +0x08, for the same reason as ProfessionList above: the global
// factory load is the primitive that is proven to work in standalone. The
// IsDerivedFrom(ExternalList) check is retail's (it stores null on a type mismatch).
// ============================================================
// Step 3 — UI material (+0xB8): "Material/UI/UIMaterialNoBlend.rsc"
// ============================================================
// Retail step 3 FindOrLoads the no-blend UI material and stores the
// ExternalPtr<Material> at gw+0xB8. It was the last deferred step 1-4 Find.
// Sole consumer today is GUL::Draw step 8 — SaveLoad::Draw(ctx, material) —
// which only runs while a save/load operation is active; with the slot null,
// SaveLoad::Draw crashed in SpriteContext::Flush -> MaterialInstance::Setup
// (read of 0+0x28) the frame after an autosave started (2026-08-27).
static void LoadUIMaterial_Step3(uint8_t* p) {
    *(void**)(p + 0xB8) = nullptr;

    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton)
        return;

    System::Filename fname("Material/UI/UIMaterialNoBlend.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* mat = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)
            ->FindOrLoad(fname));                                          // B.3.3 bridge

    // Retail's type gate (exe .rdata Material typeinfo slot is unfilled in SA —
    // resolve Graphics::Material's BaseTypeInfo from the Runtime export).
    const uint32_t* s_matTypeInfo = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A");
    bool isMat = false;
    if (mat && s_matTypeInfo && fn_IsDerivedFrom)
        isMat = fn_IsDerivedFrom(mat, *s_matTypeInfo);
    if (isMat) *(void**)(p + 0xB8) = mat;

    fprintf(stderr, "[GUL::UIMat] gw+0xb8=%p (loaded=%p isMaterial=%d)\n",
            *(void**)(p + 0xB8), mat, (int)isMat);
    fflush(stderr);
}

static void LoadHelpTopics_Step4(uint8_t* p) {
    *(void**)(p + 0xA0) = nullptr;

    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        fprintf(stderr, "[GUL::Help] SKIP — factory unavailable (factory=%p)\n",
                g_pExternalFactorySingleton ? *g_pExternalFactorySingleton : nullptr);
        fflush(stderr);
        return;
    }

    System::Filename fname("Game/Help/HelpTopics.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* list = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)
            ->FindOrLoad(fname));                                          // B.3.3 bridge

    // Retail's type gate. Resolve Object::ExternalList's own BaseTypeInfo from the
    // Runtime (the exe .rdata slot 0x140151ef0 that retail reads is unfilled in SA).
    const uint32_t* s_extListTypeInfo = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A");
    bool isList = false;
    if (list && s_extListTypeInfo && fn_IsDerivedFrom)
        isList = fn_IsDerivedFrom(list, *s_extListTypeInfo);
    if (isList) *(void**)(p + 0xA0) = list;

    fprintf(stderr, "[GUL::Help] gw+0xa0=%p (loaded=%p isExternalList=%d topics=%d)\n",
            *(void**)(p + 0xA0), list, (int)isList,
            (isList && fn_ExternalList_GetResourceCount)
                ? fn_ExternalList_GetResourceCount(list) : -1);
    fflush(stderr);
}

// ============================================================
// Step 1 — Diseases load (citizen-plague bring-up 2026-08-22)
// ============================================================
// Retail's step 1 does ExternalList::Find(param_1[1], &out,
// "Game/Disease/Diseases.rsc", ExternalList typeId) and stores the result at +0xA8.
// The string is in the exe at 0x14015e428. That store was deferred with the rest of
// the step 1-4 Finds, and it is the SINGLE reason no citizen can ever fall ill:
// CitizenComponent::CatchDisease (0x140112620, citizen_updatehealth.cpp:378-379)
// reads gw+0xA8 and returns immediately when it is null. Everything downstream —
// CatchDisease -> Infect -> SpreadDisease -> FindCareBuilding -> RecoverOrDie — is
// already ported, registered and wired; it has simply never had a disease to draw.
// DebugUI's citizen-plague trigger (0x14005ff30) reads the same slot.
//
// Modelled on LoadHelpTopics_Step4 rather than LoadProfessionList_Step2: gw+0xA8 is
// an Object::ExternalList (CatchDisease walks `*(list+0x20)` by index and calls
// ExternalList::GetResourceCount on it), exactly like gw+0xA0 — whereas gw+0xB0 is a
// ProfessionList External. Same FindOrLoad primitive, same retail type gate.
//
// Retail always performs this Find — leaving gw+0xA8 null is
// a deviation, and it silently deletes every reader of the list: CatchDisease returns
// immediately, and DebugUI::Slot4's diseaseCombo loop is bounded by
// ExternalList::GetResourceCount(null-guarded to 0), so the debug panel's Disaster tab
// showed an EMPTY disease dropdown with no error anywhere.

static void LoadDiseaseList_Step1(uint8_t* p) {
    *(void**)(p + 0xA8) = nullptr;

    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        fprintf(stderr, "[GUL::DiseaseList] SKIP — factory unavailable (factory=%p)\n",
                g_pExternalFactorySingleton ? *g_pExternalFactorySingleton : nullptr);
        fflush(stderr);
        return;
    }

    System::Filename fname("Game/Disease/Diseases.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* list = const_cast<::Object::External*>(
        reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)
            ->FindOrLoad(fname));                                          // B.3.3 bridge

    // Retail's type gate (same Runtime-side BaseTypeInfo as the HelpTopics list; the
    // exe .rdata slot retail reads is unfilled in standalone).
    const uint32_t* s_extListTypeInfo = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A");
    bool isList = false;
    if (list && s_extListTypeInfo && fn_IsDerivedFrom)
        isList = fn_IsDerivedFrom(list, *s_extListTypeInfo);
    if (isList) *(void**)(p + 0xA8) = list;

    // The `fn_ExternalList_GetResourceCount` test is KEPT even though the call
    // is now a member call: it is an existing behavioural gate and the pointer
    // keeps its typedef/extern/initialiser until step 7's closing pass.
    int count = (isList && fn_ExternalList_GetResourceCount)
                    ? reinterpret_cast<const ::Object::ExternalList*>(list)  // B.3.3 bridge
                          ->GetResourceCount()
                    : -1;
    fprintf(stderr, "[GUL::DiseaseList] gw+0xa8=%p (loaded=%p isExternalList=%d diseases=%d)\n",
            *(void**)(p + 0xA8), list, (int)isList, count);

    // Per-entry tuning dump. Disease+0x40 is the RECIPROCAL mortality — RecoverOrDie
    // computes (1/x)/careMult, so a zero here is an infinite death chance (every sick
    // citizen dies the instant the illness runs out). The factory default IS 0
    // (disease.cpp:82), so an entry reading 0 means the .rsc never wrote the field.
    // Report it; do not clamp it — that would hide a real load failure.
    if (isList && count > 0) {
        uintptr_t arr = *(uintptr_t*)((uintptr_t)list + 0x20);
        for (int i = 0; i < count && arr; i++) {
            uint8_t* d = *(uint8_t**)(arr + (uintptr_t)i * 8);
            if (!d) { fprintf(stderr, "[GUL::DiseaseList]   [%d] NULL entry\n", i); continue; }
            fprintf(stderr, "[GUL::DiseaseList]   [%d] %p spreadMonths(+0x30)=%.3f "
                    "contagion(+0x34)=%.3f radius(+0x38)=%.3f weight(+0x3c)=%.3f "
                    "recipMortality(+0x40)=%.3f%s\n",
                    i, (void*)d, *(float*)(d + 0x30), *(float*)(d + 0x34),
                    *(float*)(d + 0x38), *(float*)(d + 0x3c), *(float*)(d + 0x40),
                    (*(float*)(d + 0x40) == 0.0f) ? "  <== ZERO: deathChance is INFINITE" : "");
        }
    }
    fflush(stderr);
}

static bool s_loadResolved = false;
static void LoadEnsureInit() {
    if (s_loadResolved) return;
    s_loadResolved = true;
    s_fn_Random_Seeded_Ctor = (fn_Random_Seeded_Ctor_t)ResolveExport("??0Random@System@@QEAA@I@Z");
    s_fn_Picking_Scene_Ctor = (fn_Picking_Scene_Ctor_t)ResolveExport("??0Scene@Picking@@QEAA@XZ");
    s_fn_UI_Ctor            = (fn_UI_Ctor_t)ResolveExport("??0UserInterface@UI@@QEAA@XZ");
    s_fn_AnimMgr_Ctor       = (fn_AnimMgr_Ctor_t)ResolveExport("??0AnimationManager@Graphics@@QEAA@XZ");
    s_fn_SaveLoad_Ctor      = (fn_SaveLoad_Ctor_t)ResolveExport("??0SaveLoad@Save@@QEAA@XZ");
    s_fn_SavedState_Dtor    = (fn_SavedState_Dtor_t)ResolveExport("??1SavedState@Save@@QEAA@XZ");
    s_fn_Random_RandFull    = (fn_Random_RandFull_t)ResolveExport("?Rand@Random@System@@QEAAIXZ");
    fprintf(stderr, "[GUL::Load] resolves: RandSeed=%p Picking=%p UI=%p Anim=%p SaveLoad=%p RandFull=%p\n",
            (void*)s_fn_Random_Seeded_Ctor, (void*)s_fn_Picking_Scene_Ctor, (void*)s_fn_UI_Ctor,
            (void*)s_fn_AnimMgr_Ctor, (void*)s_fn_SaveLoad_Ctor, (void*)s_fn_Random_RandFull);
    fflush(stderr);
}

// ============================================================
// Array-resize leaf helpers (verified, m5-load-ctor-drafts)
// Header layout: +0x00 ptr, +0x08 int count, +0x0C int capacity.
// NB: the stride-8 ZERO-FILL variant (FUN_1400032e0) is reached as
// Collection::Array<GameColl::Ptr8>::SetSize — not redefined here.
// ============================================================

// ENDGAME B.3.2 step 2: the five resize bodies in this block (0x1400191f0,
// 0x14002e000, 0x140017640, 0x1400177c0, 0x14000f090) WERE hand-written copies
// of `rtcoll::SetSizeCore` (runtime/src/collection/array.cpp). Each is now one
// `Collection::Array<>::SetSize` driven by an `ArraySpec` row spelling out the
// site's element ops. Compared arm for arm before the swap: exact-equal early
// return, `_size` stored before the capacity test,
// treble-on-single-append-from-non-empty, copy the preserved prefix only when
// the block moved, init the new tail (or leave it — 0x1400191f0), destroy the
// truncated tail in place / the whole old buffer on a realloc, then free. The
// one deliberate difference: the allocation now goes through the LOCKING
// static `MemAlloc` (0x1800c7a50) instead of the unlocked instance `Allocate`
// these bodies reached through `fn_MemAlloc` — same allocator, same heap, plus
// the critical section retail's own Collection code takes. The 0x14000f9e0
// deep copy is an `operator=` (rtcoll::ArrayAssignCore), not a grow, and is
// kept as the element-assign of the 0x140017640 row.
namespace {

// 0x1400191f0 element: one 8-byte pointer, align 8. Copy = 8-byte copy; the
// new tail is left UNINITIALISED (retail's empty else arm) — init is null on
// purpose; trivially destructible.
void AssignPtr8(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d) = *reinterpret_cast<const uint64_t*>(s);
}
const rtcoll::ArraySpec kSpec_Ptr8NoInit = {             // exe 0x1400191f0
    "Array<void*> (no tail init)", 8, 8,
    &AssignPtr8, nullptr, nullptr, true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using Ptr8NoInitArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                          Collection::ExplicitSpec<&kSpec_Ptr8NoInit>>;

// 0x14002e000 element: camera view {Vector3 pos, float a, float b, float c,
// bool used}, stride 0x1c, align 4. Copy writes +0x00..+0x18 (the 4x-unrolled
// dword loop plus the bool) and leaves +0x19..+0x1b; init writes the same
// bytes with zero. Partial copy, so byteCopy=false; trivially destructible.
void AssignCamView(uint8_t* d, const uint8_t* s) {
    for (int b = 0; b < 0x18; b += 4) *reinterpret_cast<uint32_t*>(d + b) = *reinterpret_cast<const uint32_t*>(s + b);
    d[0x18] = s[0x18];
}
void InitCamView(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0; *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x10) = 0; d[0x18] = 0;
}
const rtcoll::ArraySpec kSpec_CamViewLoad = {            // exe 0x14002e000
    "Array<CameraView>", 0x1c, 4,
    &AssignCamView, &InitCamView, nullptr, false,
    /*assign unwritten*/ 0x19, 0x1c,
    /*init   unwritten*/ 0x19, 0x1c,
    /*seed zero       */ 0,    0 };
using CamViewLoadArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                           Collection::ExplicitSpec<&kSpec_CamViewLoad>>;

}  // namespace

// FUN_1400191f0 — stride-8 pointer-array resize; copies existing, does NOT zero new slots.
static void* Reimpl_FUN_1400191f0(void* hdr, int n) {
    // B.3.2 bridge: the +0x50 header is still raw bytes in an unconverted struct.
    reinterpret_cast<Ptr8NoInitArray*>(hdr)->SetSize(n);
    return hdr;
}

// FUN_14002e000 — stride-0x1c (28-byte) struct-array resize; copies existing, zero-fills new.
static void* Reimpl_FUN_14002e000(void* hdr, int n) {
    // B.3.2 bridge: EntityUI+0x70 is still a raw header.
    reinterpret_cast<CamViewLoadArray*>(hdr)->SetSize(n);
    return hdr;
}

// helper @ 0x14000f9e0 — deep-copy of an inner stride-4 (uint32) array header (16 bytes).
static void* Reimpl_TradeComponent_helper_f9e0(void* dstElem, void* srcElem) {
    if (dstElem == srcElem) return dstElem;
    uint8_t* d = (uint8_t*)dstElem;
    uint8_t* s = (uint8_t*)srcElem;
    void* old = *(void**)(d + 0x00);
    if (old) fn_MemFreeStatic(old);
    *(int32_t*)(d + 0x08) = 0;
    *(void**)(d + 0x00) = 0;
    int32_t cnt = *(int32_t*)(s + 0x08);
    *(int32_t*)(d + 0x08) = cnt;
    if (cnt > 0) {
        *(int32_t*)(d + 0x0C) = cnt;
        *(void**)(d + 0x00) = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)cnt << 2, 4);
    }
    int32_t newCnt = *(int32_t*)(d + 0x08);
    if (newCnt > 0) {
        int32_t* dp = *(int32_t**)(d + 0x00);
        int32_t* sp = *(int32_t**)(s + 0x00);
        for (int32_t i = 0; i < newCnt; ++i) dp[i] = sp[i];
    }
    return dstElem;
}

namespace {

// 0x140017640 element: one inner {int32* data, int32 count, int32 cap}
// header, stride 0x10, align 8. Copy-construct = zero the header then the
// 0x14000f9e0 deep copy (an operator= over a zeroed lhs); init = zero the
// header; destroy = free the inner buffer and zero the header — this body
// zeroed on BOTH the truncation and the realloc arm, so the row is exact.
void AssignIntAssocOuterL(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
    Reimpl_TradeComponent_helper_f9e0(d, const_cast<uint8_t*>(s));
}
void InitIntAssocOuterL(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
}
void DestroyIntAssocOuterL(void* /*ignoredThis*/, uint8_t* base, int count) {
    for (int i = 0; i < count; ++i) {
        uint8_t* e = base + static_cast<int64_t>(i) * 0x10;
        fn_MemFreeStatic(*reinterpret_cast<void**>(e + 0x00));
        *reinterpret_cast<uint64_t*>(e + 0x00) = 0;
        *reinterpret_cast<uint64_t*>(e + 0x08) = 0;
    }
}
const rtcoll::ArraySpec kSpec_IntAssocOuterL = {         // exe 0x140017640
    "Array<Array<int32>>", 0x10, 8,
    &AssignIntAssocOuterL, &InitIntAssocOuterL, &DestroyIntAssocOuterL, false,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using IntAssocOuterLArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                              Collection::ExplicitSpec<&kSpec_IntAssocOuterL>>;

// 0x1400177c0 element: ProfessionResourceEntry, stride 0x20, align 8. Copy =
// four qwords (the whole stride); init = {0, 100, 0, 0} written as u64@0,
// u64@8, u32@0x10, u64@0x18 so +0x14..+0x17 is left untouched; trivially
// destructible.
void AssignProfResEntryL(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = *reinterpret_cast<const uint64_t*>(s + 0x00);
    *reinterpret_cast<uint64_t*>(d + 0x08) = *reinterpret_cast<const uint64_t*>(s + 0x08);
    *reinterpret_cast<uint64_t*>(d + 0x10) = *reinterpret_cast<const uint64_t*>(s + 0x10);
    *reinterpret_cast<uint64_t*>(d + 0x18) = *reinterpret_cast<const uint64_t*>(s + 0x18);
}
void InitProfResEntryL(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x08) = 100;   // +0x08 default = 100
    *reinterpret_cast<uint32_t*>(d + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(d + 0x18) = 0;
}
const rtcoll::ArraySpec kSpec_ProfResEntryL = {          // exe 0x1400177c0
    "Array<ProfessionResourceEntry>", 0x20, 8,
    &AssignProfResEntryL, &InitProfResEntryL, nullptr, true,
    /*assign unwritten*/ 0,    0,
    /*init   unwritten*/ 0x14, 0x18,
    /*seed zero       */ 0,    0 };
using ProfResEntryLArray = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                             Collection::ExplicitSpec<&kSpec_ProfResEntryL>>;

// 0x14000f090 element: two qwords, stride 0x10, align 8. Copy = whole
// stride; init = zero; trivially destructible.
void AssignPod16(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = *reinterpret_cast<const uint64_t*>(s + 0x00);
    *reinterpret_cast<uint64_t*>(d + 0x08) = *reinterpret_cast<const uint64_t*>(s + 0x08);
}
void InitPod16(uint8_t* d) {
    *reinterpret_cast<uint64_t*>(d + 0x00) = 0; *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
}
const rtcoll::ArraySpec kSpec_Pod16 = {                  // exe 0x14000f090
    "Array<{u64,u64}>", 0x10, 8,
    &AssignPod16, &InitPod16, nullptr, true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0, 0 };
using Pod16Array = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                     Collection::ExplicitSpec<&kSpec_Pod16>>;

}  // namespace

// FUN_140017640 (TradeComponent_impl_f760_sub_7640) — stride-0x10 array of inner-array headers.
static void* Reimpl_FUN_140017640(void* hdr, int n) {
    // B.3.2 bridge: CitizenBehavior+0x150 / TownManager+0x140 are still raw headers.
    reinterpret_cast<IntAssocOuterLArray*>(hdr)->SetSize(n);
    return hdr;
}

// TradeComponent_impl_f760_sub_77c0 @ 0x1400177c0 — stride-0x20 resize; new slots {0,100,0,0}.
static void* Reimpl_TradeComponent_sub_77c0(void* hdr, int n) {
    // B.3.2 bridge: CitizenBehavior+0x170 is still a raw header.
    reinterpret_cast<ProfResEntryLArray*>(hdr)->SetSize(n);
    return hdr;
}

// FUN_14000f090 — stride-0x10 POD resize; copies 2x u64, zero-fills new.
static void* Reimpl_FUN_14000f090(void* hdr, int n) {
    // B.3.2 bridge: CitizenBehavior+0x180 is still a raw header.
    reinterpret_cast<Pod16Array*>(hdr)->SetSize(n);
    return hdr;
}

// ============================================================
// Subsystem constructors (verified bodies)
// ============================================================

// FUN_1400983f0 — PlayerController/Camera lookAt-transform builder (helper_83f0).
// Exposed (extern "C") + registered at 0x1400983f0 so gameupdateloop.cpp's
// fn_PlayerController_CalcTransform (resolved via ResolveAddr) finds it in standalone.
extern "C" void* Reimpl_FUN_1400983f0(void* self, void* outTransform) {
    uint8_t* p = (uint8_t*)self;
    float yaw = *(float*)(p + 0x28);
    float pitch = *(float*)(p + 0x2c);
    float distance = *(float*)(p + 0x30);
    // ?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z  retail 0x1800220b0
    Math::Vector3 dir = Math::Vector3::FromYawPitch(yaw, pitch);
    // The camera focus point lives inline at self+0x1c (Transform::translation).
    const Math::Vector3& pos = *reinterpret_cast<const Math::Vector3*>(p + 0x1c);
    Math::Vector3 eye(pos.x - dir.x * distance,
                      pos.y - dir.y * distance,
                      pos.z - dir.z * distance);
    // ?FromLookAt@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z  retail 0x18007dbb0
    Math::Transform lookAt = Math::Transform::FromLookAt(eye, pos);
    *reinterpret_cast<Math::Transform*>(p) = lookAt;              // self[+0x00..+0x1b]
    *reinterpret_cast<Math::Transform*>(outTransform) = lookAt;
    return outTransform;
}

// FUN_1400985b0 — PlayerController camera-system ctor (+0xE0, 0x10c).
static void* Reimpl_FUN_1400985b0(void* selfMem, void* frustum, float yaw, float pitch, float distance) {
    uint8_t* p = (uint8_t*)selfMem;
    if (!fn_Graphics_Camera_Ctor)
        return selfMem;
    // ??0Transform@Math@@QEAA@XZ  retail 0x180015080
    *reinterpret_cast<Math::Transform*>(selfMem) = Math::Transform();
    *(uint64_t*)(p + 0x1c) = 0;
    *(uint32_t*)(p + 0x24) = 0;
    *(float*)(p + 0x28) = yaw;
    *(float*)(p + 0x2c) = pitch;
    *(float*)(p + 0x30) = distance;
    Math::Transform localTransform;
    void* lt = Reimpl_FUN_1400983f0(selfMem, &localTransform);
    if (!lt) lt = &localTransform;
    Math::Transform3 t3 = reinterpret_cast<const Math::Transform*>(lt)->AsTransform3();
    fn_Graphics_Camera_Ctor((void*)(p + 0x34), frustum, &t3, true);
    return selfMem;
}

// FUN_1400c3f90 — EntityList ctor (+0xE8, 0xe8). memset + the 5 non-zero defaults.
static void* Reimpl_FUN_1400c3f90(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    memset(p, 0, 0xe8);                       // zeroes header + the 5x16 array @+0x18 + two {ptr,count} @+0x68/+0x78
    *(int32_t*)(p + 0xa8) = 0x32;             // 50
    *(int32_t*)(p + 0xac) = 0xf;              // 15
    *(int32_t*)(p + 0xb0) = 8;
    *(int32_t*)(p + 0xb4) = 5;
    *(uint64_t*)(p + 0xb8) = 2;
    return selfMem;
}

// TradeComponent_impl_0a30 @ 0x140090a30 — town/trade manager ctor (+0xF0, 0x1c0).
static void* Reimpl_TradeComponent_impl_0a30(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    for (int i = 0; i < 0x1b4; ++i) p[i] = 0;
    Reimpl_TradeComponent_sub_77c0(p + 0x170, 0x15);
    uint8_t* prices = *(uint8_t**)(p + 0x170);
    if (prices) {
        static const int32_t kDefaultPrices[21] = {
            200, 200, 200, 200, 50, 5000, 100, 50, 500, 200, 200,
            1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000
        };
        for (int i = 0; i < 21; ++i)
            *(int32_t*)(prices + (size_t)i * 0x20 + 0x8) = kDefaultPrices[i];
    }
    Reimpl_FUN_14000f090(p + 0x180, 2);
    Reimpl_FUN_140017640(p + 0x150, 0x47);
    *(int32_t*)(p + 0x1a8) = 0;
    void* sub = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 4);
    if (sub) {
        uint8_t* s = (uint8_t*)sub;
        *(void**)(s + 0x00) = nullptr;
        *(void**)(s + 0x08) = nullptr;
        *(int32_t*)(s + 0x10) = -1;
        *(void**)(s + 0x18) = nullptr;
        *(int32_t*)(s + 0x20) = 0;
    }
    *(void**)(p + 0x1b8) = sub;
    return selfMem;
}

// FUN_14009ee70 — REAL Game ctor (+0xF8, 0xf8). Embedded Picking::Scene @ +8.
static void* Reimpl_FUN_14009ee70(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    if (s_fn_Picking_Scene_Ctor) s_fn_Picking_Scene_Ctor((void*)(p + 8));
    memset(p + 0x68, 0, 0xf8 - 0x68);         // zero +0x68..+0xf8 (preserves the embedded Scene at +8)
    *(uint8_t*)(p + 0xa2) = 1;
    *(uint64_t*)(p + 0xd0) = 8;
    return selfMem;
}

// MainMenuDialog_helper_9180 @ 0x1400f9180 — Tutorial-ish ctor (+0x118, 0x138).
static void* Reimpl_FUN_1400f9180(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    memset(p, 0, 0x138);                      // zeroes header + the 15x16 array @+0x30
    *(int32_t*)(p + 0x08) = 1;
    return selfMem;
}

// FUN_140093f60 — in-game Weather SIMULATION ctor (+0x130, 0x78). PerlinNoise-driven.
static void* Reimpl_FUN_140093f60(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    *(uint64_t*)(p + 0x00) = 0;
    *(uint64_t*)(p + 0x08) = 0;
    *(uint32_t*)(p + 0x10) = 0;
    *(uint32_t*)(p + 0x14) = 4;               // default count = 4
    *(uint32_t*)(p + 0x18) = 0;
    *(uint64_t*)(p + 0x1c) = 0;
    *(uint64_t*)(p + 0x24) = 0;
    *(uint64_t*)(p + 0x2c) = 0;
    uint8_t rng1[2520];
    uint8_t rng2[2520];
    uint32_t seed1 = 0, seed2 = 0;
    if (fn_Random_Default_Ctor) {
        fn_Random_Default_Ctor(rng1);
        seed1 = s_fn_Random_RandFull ? reinterpret_cast<System::Random*>(rng1)->Rand()   // B.3.4b bridge
                : (fn_Random_Rand ? reinterpret_cast<System::Random*>(rng1)->Rand(0xffffffffu) : 0u);   // B.3.4b bridge
        fn_Random_Default_Ctor(rng2);
        seed2 = s_fn_Random_RandFull ? reinterpret_cast<System::Random*>(rng2)->Rand()   // B.3.4b bridge
                : (fn_Random_Rand ? reinterpret_cast<System::Random*>(rng2)->Rand(0xffffffffu) : 0u);   // B.3.4b bridge
    }
    if (fn_PerlinNoise_Ctor)
        fn_PerlinNoise_Ctor((void*)(p + 0x34), 0.25f, 4.0f, (int)seed2, (int)seed1);
    *(uint64_t*)(p + 0x44) = 0;
    *(uint32_t*)(p + 0x4c) = 0xffffffffu;
    *(uint64_t*)(p + 0x50) = 0;
    *(uint64_t*)(p + 0x58) = 0;
    *(uint64_t*)(p + 0x60) = 0;
    *(uint64_t*)(p + 0x68) = 0;
    *(uint64_t*)(p + 0x70) = 0;
    return selfMem;
}

// FUN_140093e30 — weather env-config helper. (envRes may be null on the deferred path.)
static void* Reimpl_FUN_140093e30(void* weatherObj, void* envRes, void* climateGen) {
    uint8_t* p = (uint8_t*)weatherObj;
    *(void**)(p + 0x00) = envRes;
    *(void**)(p + 0x08) = climateGen;
    if (envRes) {
        int count = *(int32_t*)((uint8_t*)envRes + 0x30);
        Reimpl_FUN_1400191f0((void*)(p + 0x50), count);
        // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
        // new tail zeroed. Element is an 8-byte object handle (byte-copy, zeroing).
        GameColl::ArrayAt<GameColl::Ptr8>(p, 0x60).SetSize(count);
        int n0 = *(int32_t*)(p + 0x58);
        void** arr0 = *(void***)(p + 0x50);
        for (int i = 0; i < n0; ++i) arr0[i] = nullptr;
    }
    if (climateGen) {
        float v = *(float*)((uint8_t*)climateGen + 0x34);
        *(float*)(p + 0x2c) = v;
        *(float*)(p + 0x30) = v;
    }
    return weatherObj;
}

// FUN_1400182b0 - CharacterGrid ctor (+0x88, 0x60). A loose Morton (Z-order)
// quadtree over characters, NOT a SkyMesh transform (that older name came from a
// wrong plate comment on retail Load; corrected 2026-08-30 - see
// engine/types/GameUpdateLoop.h +0x88).
static void* Reimpl_FUN_1400182b0(void* selfMem, float param2) {
    uint8_t* p = (uint8_t*)selfMem;
    memset(p, 0, 0x60);
    *(float*)(p + 0x00) = param2;
    *(int32_t*)(p + 0x0C) = 8;
    float scale = param2 * 0.5f;
    *(float*)(p + 0x04) = scale;
    *(float*)(p + 0x08) = scale;
    Reimpl_Array_Resize_Stride04((void*)(p + 0x40), 8);
    {
        int32_t* lod = *(int32_t**)(p + 0x40);
        if (lod) {
            lod[0] = 0x0; lod[1] = 0x1; lod[2] = 0x5; lod[3] = 0x15;
            lod[4] = 0x55; lod[5] = 0x155; lod[6] = 0x555; lod[7] = 0x1555;
        }
    }
    Reimpl_Array_Resize_Stride04((void*)(p + 0x30), 0x5555);
    Reimpl_Array_Resize_Stride04((void*)(p + 0x50), 0x5555);
    {
        int32_t* aFF = *(int32_t**)(p + 0x30);
        int32_t* aZ = *(int32_t**)(p + 0x50);
        if (aFF && aZ) {
            for (int idx = 0; idx < 0x5555; ++idx) { aFF[idx] = (int32_t)0xffffffff; aZ[idx] = 0; }
        }
    }
    return selfMem;
}

// FUN_140098660 — PlayerController::UpdateCamera (builds projection frustum on camera+0x34).
// Exposed + registered at 0x140098660 (gameupdateloop.cpp Draw + Load step 33 call it).
extern "C" void* Reimpl_FUN_140098660(void* camera, float fovY, float nearZ, float farZ) {
    int width = 1920, height = 1080;          // default 16:9 if viewport singleton unavailable
    if (g_pSingletonExref_151a80 && *g_pSingletonExref_151a80) {
        uint8_t* vp = *(uint8_t**)(*g_pSingletonExref_151a80);
        if (vp) { width = *(int32_t*)(vp + 4); height = *(int32_t*)(vp + 8); }
    }
    const float kAspectThresh = 1.7777778f, kHalf = 0.5f, kDeg2Rad = 0.017453292f,
                kTwo = 2.0f, kRad2Deg = 57.29578f;
    float aspect = (height != 0) ? (float)width / (float)height : kAspectThresh;
    float fovX = aspect * fovY;
    if (aspect > kAspectThresh) {
        float t = tanf(fovY * kAspectThresh * kHalf * kDeg2Rad);
        float den = (height * kAspectThresh * kHalf) / t;
        float halfW = (float)(width / 2);
        fovX = atanf(halfW / den) * kTwo * kRad2Deg;
    }
    uint8_t frustum[0xC0];
    void* fr = fn_Volume_Frustum_Ctor ? fn_Volume_Frustum_Ctor(frustum, &nearZ, &farZ, &fovX, &fovY) : nullptr;
    if (fn_Graphics_Camera_SetFrustum)
        fn_Graphics_Camera_SetFrustum((void*)((uint8_t*)camera + 0x34), fr ? fr : frustum);
    return camera;
}

// FUN_1401025c0 (helper_25c0) — in-game EntityUI-style ctor (+0x120, 0xb0).
static void* Reimpl_FUN_1401025c0(void* selfMem) {
    uint8_t* p = (uint8_t*)selfMem;
    new ((void*)(p + 0x00)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *(int64_t*)(p + 0x10) = 0;
    *(int32_t*)(p + 0x18) = 0;
    *(int64_t*)(p + 0x20) = 0;
    new ((void*)(p + 0x28)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *(uint8_t*)(p + 0x30) = 0;
    *(int32_t*)(p + 0x34) = 1;
    *(uint8_t*)(p + 0x38) = 0;
    *(int64_t*)(p + 0x3c) = 0;
    *(float*)(p + 0x44) = 0.75f;
    *(int64_t*)(p + 0x48) = 0;
    *(int64_t*)(p + 0x50) = 0;
    *(int32_t*)(p + 0x58) = 0;
    *(int64_t*)(p + 0x60) = 0;
    *(int32_t*)(p + 0x68) = 0;
    *(int64_t*)(p + 0x70) = 0;
    *(int64_t*)(p + 0x78) = 0;
    *(uint8_t*)(p + 0x80) = 0;
    *(int64_t*)(p + 0x88) = 0;
    *(int64_t*)(p + 0x90) = 0;
    *(int64_t*)(p + 0x98) = -1;
    *(int32_t*)(p + 0xa0) = -1;
    *(uint8_t*)(p + 0xa4) = 0;
    Reimpl_FUN_14002e000((void*)(p + 0x70), 5);
    return selfMem;
}

// ============================================================
// Small allocation helper
// ============================================================
static inline void* AllocSub(uint64_t size, int align) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    return fn_MemAlloc(g_pMemAllocSingleton, size, align);
}

// ============================================================
// Step 28 helper — SkyMesh MeshInstance (+0x108): the sky `_clear_` dome.
// ============================================================
//
// Faithful port of the exe Load's sky-build (0x140097356..0x140097420):
// MemAlloc a Graphics::MeshInstance (0x120 bytes), run its Runtime ctor (which
// installs the Runtime default vtable — the exe vtable 0x14015e240 is only 9
// slots: slot0 = exe scalar-deleting dtor, slots 1-8 are FF25 IAT thunks
// straight to the Runtime MeshInstance methods, so the Runtime default the ctor
// installs is render-identical), FindOrLoad "Models/Sky/SkyMesh.rsc", SetMesh
// it, run Startup via vtable[+8](skyMesh, Time(0)), zero +0x28. The resource
// loads in standalone with no factory cascade (SkyMesh GraphicsMesh is
// Runtime-self-registered). GameUpdateLoop::Shutdown already tears m_skyMesh
// down via vtable[0].
//
// This is the fix for "dark/flat water": without the sky as a Scene global
// object, the engine's reflection pass has only terrain to mirror (reflection
// RT blue mean ~0.18 vs retail ~0.68), so the water — whose shader is
// sky-reflection-dominated on the tilted camera — renders dark navy. Adding the
// sky as a Scene global object (step-33 tail) feeds the reflection pass for
// free, exactly like retail. Opt out for A/B with BANISHED_NO_SKYMESH=1.
typedef void (*fn_MeshInstance_Ctor_t)(void* self);
typedef void (*fn_MeshInstance_SetMesh_t)(void* self, void* meshExtPtr);
typedef void (*fn_Time_Ctor_t)(void* self, float seconds);

static void BuildSkyMesh_Step28(uint8_t* p) {
    static bool s_res = false;
    static fn_MeshInstance_Ctor_t    s_Ctor     = nullptr;
    static fn_MeshInstance_SetMesh_t s_SetMesh  = nullptr;
    static fn_Time_Ctor_t            s_TimeCtor = nullptr;
    if (!s_res) {
        s_res = true;
        s_Ctor    = (fn_MeshInstance_Ctor_t)ResolveExport("??0MeshInstance@Graphics@@QEAA@XZ");
        s_SetMesh = (fn_MeshInstance_SetMesh_t)ResolveExport(
            "?SetMesh@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VGraphicsMesh@Graphics@@@Object@@@Z");
        s_TimeCtor = (fn_Time_Ctor_t)ResolveExport("??0Time@System@@QEAA@M@Z");
    }

    void* mem = AllocSub(0x120, 8);
    *(void**)(p + 0x108) = mem;
    if (!mem || !s_Ctor) {
        fprintf(stderr, "[GUL::SkyMesh] SKIP — mem=%p ctor=%p\n", mem, (void*)s_Ctor);
        fflush(stderr);
        return;
    }
    s_Ctor(mem);

    // Load the sky dome resource (the `_clear_`-material mesh) via the global factory.
    void* meshRes = nullptr;
    if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton) {
        void* factory = *g_pExternalFactorySingleton;
        // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
        ::Object::ExternalFactory* efactory =
            reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge
        System::Filename fname("Models/Sky/SkyMesh.rsc");
        meshRes = const_cast<::Object::External*>(efactory->FindOrLoad(fname));  // B.3.3 bridge
    }

    if (meshRes && s_SetMesh) s_SetMesh(mem, meshRes);

    // Startup: vtable[+8](skyMesh, Time(0)) — matches the original Load's
    // `(*(code**)(*skyMesh + 8))(skyMesh, &Time0)`.
    if (s_TimeCtor) {
        uint64_t timeObj = 0;
        s_TimeCtor(&timeObj, 0.0f);
        void* vt = *(void**)mem;
        if (vt && *(void**)((uint8_t*)vt + 8))
            (*(void(**)(void*, void*))((uint8_t*)vt + 8))(mem, &timeObj);
    }
    *(uint32_t*)((uint8_t*)mem + 0x28) = 0;

    fprintf(stderr, "[GUL::SkyMesh] built skyMesh=%p meshRes=%p (SetMesh=%p TimeCtor=%p)\n",
            mem, meshRes, (void*)s_SetMesh, (void*)s_TimeCtor);
    fflush(stderr);
}

// ============================================================
// GameUpdateLoop::Load (0x140096b00) — orchestration
// ============================================================

// World-build completion signal. GameUpdateLoop::Load runs on the Runtime
// Engine::Loader thread spawned inside fn_DialogLoaderLoopT_Startup -- the
// only dispatcher there is (see the guard below). The loading-screen loop
// (splash_loop_stubs.cpp) polls GameUpdateLoop_IsWorldBuilt(next) to hold the
// loading screen up until the build genuinely finishes, so GameUpdateLoop::Draw
// never renders a half-built world (torn scene reads) and the screen shows a
// real loading screen + tips for the whole build, like retail. Distinct from the
// s_loadingLoop *ownership* guard below (which is set at build START).
static std::atomic<void*> s_worldBuiltLoop{nullptr};

extern "C" bool GameUpdateLoop_IsWorldBuilt(void* loop) {
    return loop != nullptr &&
           s_worldBuiltLoop.load(std::memory_order_acquire) == loop;
}

extern "C" void GameUpdateLoop_Load(void* loopVoid) {
    // Single-owner build guard — NOW AN ASSERTION, NOT A RACE FIX.
    //
    // ENDGAME B.3.4a closing, comment only. This block used to describe the
    // loop's Load (+0x30) being dispatched TWICE, concurrently, on the SAME
    // GameUpdateLoop (confirmed 2026-06-17, two distinct tids per game-load):
    //   (1) our standalone AsyncLoader worker, from Stub_Startup step 3; and
    //   (2) the Runtime Engine::Loader thread, spawned inside
    //       fn_DialogLoaderLoopT_Startup's Loader::Load(m_nextLoop).
    // Both ran identical idempotent build code, so the OUTCOME was correct
    // whichever won, but unguarded they double-spawned into the entity buckets
    // (null entries -> crash in EntityList_Update). The note that stopping
    // dispatcher (1) at its source was "blocked" is obsolete.
    //
    // DISPATCHER (1) IS GONE. `standalone/async_loader.{h,cpp}` were deleted in
    // 96c17f47 and `Engine::Loader` is the only dispatcher. So this guard no
    // longer resolves a race; it ASSERTS that there is only one. A
    // "[GUL::Load] redundant dispatch" line reappearing in the log means a
    // SECOND DISPATCHER IS BACK -- treat it as a regression to find, not as the
    // guard doing its job. It is kept for exactly that reason.
    static std::atomic<void*> s_loadingLoop{nullptr};
    unsigned long _tid = (unsigned long)GetCurrentThreadId();
    if (s_loadingLoop.exchange(loopVoid) == loopVoid) {
        fprintf(stderr, "[GUL::Load] redundant dispatch for loop=%p tid=%lu — "
                "rejected (loop already owned/built by the first dispatcher)\n", loopVoid, _tid);
        fflush(stderr);
        return;
    }
    fprintf(stderr, "[GUL::Load] build owner = tid=%lu for loop=%p\n", _tid, loopVoid);
    fflush(stderr);

    LoadEnsureInit();
    uint8_t* p = (uint8_t*)loopVoid;

    fprintf(stderr, "[GUL::Load] ENTER self=%p seed=%d mapSize=%d climateGen=%p terrainGen=%p saved=%p\n",
            loopVoid, *(int32_t*)(p + 0x30), *(int32_t*)(p + 0x34),
            *(void**)(p + 0x50), *(void**)(p + 0x48), *(void**)(p + 0x58));
    fflush(stderr);

    // g_pGameWorldSingleton = this loop (read by AddGlobalObject / Audio::Pause at the tail).
    if (g_pGameWorldSingleton) *g_pGameWorldSingleton = loopVoid;

    // Steps 1-4: resource Finds (Diseases/Profession/Material/Help). Only step 3
    // (UI material) is still DEFERRED (left null; consumers guarded).
    //   step 1 Diseases    → +0xA8, opt-in BANISHED_LIVE_DISEASELIST (unblocks citizen illness)
    //   step 2 Professions  → +0xB0, the citizen-economy keystone (BANISHED_LIVE_PROFESSIONLIST)
    //   step 4 HelpTopics   → +0xA0, consumed only by the help dialog
    LoadDiseaseList_Step1(p);       // step 1 → +0xA8 (diseases); env-gated, else null
    LoadProfessionList_Step2(p);    // step 2 → +0xB0 (professions); env-gated, else null
    LoadUIMaterial_Step3(p);        // step 3 → +0xB8 (UI material — SaveLoad::Draw overlay)
    LoadHelpTopics_Step4(p);        // step 4 → +0xA0 (help topics)

    // Step 5 — the gameplay Random (+0xD8, 0x9c4) seeded from gameLoop+0x30.
    //
    // ⚠ CORRECTED 2026-07-31: this said "seeded with mapSizeIndex (+0x30)". WRONG, and it
    // cost real time. `+0x30` is THE MAP SEED — `newgamedialog.cpp:718` writes the seed
    // there and `:719` writes terrainSize to `+0x34`. So the full chain is
    //   BANISHED_SEED -> seed UI element (newgamedialog.cpp:229-236)
    //                 -> gameLoop+0x30   (newgamedialog.cpp:718)
    //                 -> Random(seed) at gw+0xD8   (here)
    // The gameplay RNG IS seeded. Do NOT "fix" this by pinning gw+0xD8 — it is already
    // pinned, and an investigation was launched on the belief that it wasn't.
    //
    // Runs with an identical seed still diverge (gender splits 14/13, 15/12, 13/14), so
    // the divergence is in the ORDER or COUNT of RNG consumption, or a second entropy
    // source — NOT in seeding. Leading suspect: iteration over a container keyed by heap
    // POINTERS (the profession BST is confirmed pointer-keyed), which ASLR reorders.
    {
        void* mem = AllocSub(0x9c4, 4);
        if (mem && s_fn_Random_Seeded_Ctor) s_fn_Random_Seeded_Ctor(mem, *(uint32_t*)(p + 0x30));
        *(void**)(p + 0xD8) = mem;
    }

    // Step 6 — PlayerController/Camera (+0xE0, 0x10c). Initial frustum (overwritten by step 33).
    {
        int width = 1920, height = 1080;
        if (g_pSingletonExref_151a80 && *g_pSingletonExref_151a80) {
            uint8_t* vp = *(uint8_t**)(*g_pSingletonExref_151a80);
            if (vp) { width = *(int32_t*)(vp + 8); height = *(int32_t*)(vp + 4); }  // step6 reads +8/+4
        }
        float aspect = (height != 0) ? (float)width / (float)height : 1.7777778f;
        void* mem = AllocSub(0x10c, 4);
        if (mem) {
            float nearZ = 2.0f, farZ = 70.0f, fovX = 60.0f, fovY = aspect * 60.0f;
            uint8_t frustum[0xC0] = {};
            if (fn_Volume_Frustum_Ctor) fn_Volume_Frustum_Ctor(frustum, &nearZ, &farZ, &fovX, &fovY);
            Reimpl_FUN_1400985b0(mem, frustum, 0.0f, -40.0f, 25.0f);
        }
        *(void**)(p + 0xE0) = mem;
    }

    // Step 7 — Renderer (+0x90, 0x50).
    {
        void* mem = AllocSub(0x50, 4);
        if (mem && fn_Graphics_Renderer_Ctor) fn_Graphics_Renderer_Ctor(mem);
        *(void**)(p + 0x90) = mem;
    }

    // Step 8 — EntityList (+0xE8, 0xe8).
    {
        void* mem = AllocSub(0xe8, 4);
        if (mem) Reimpl_FUN_1400c3f90(mem);
        *(void**)(p + 0xE8) = mem;
    }

    // Step 9 — UserInterface (+0x98, 0x128).
    {
        void* mem = AllocSub(0x128, 4);
        if (mem && s_fn_UI_Ctor) s_fn_UI_Ctor(mem);
        *(void**)(p + 0x98) = mem;
    }

    // Step 10 — town/trade manager (+0xF0, 0x1c0) — exe ctor 0x140090a30.
    {
        void* mem = AllocSub(0x1c0, 4);
        if (mem) Reimpl_TradeComponent_impl_0a30(mem);
        *(void**)(p + 0xF0) = mem;
    }

    // Step 11 — Game (+0xF8, 0xf8) — CRITICAL: clears the m_game==null Stage-C guards.
    {
        void* mem = AllocSub(0xf8, 8);
        if (mem) Reimpl_FUN_14009ee70(mem);
        *(void**)(p + 0xF8) = mem;
    }

    // Step 12 — Tutorial-ish (+0x118, 0x138).
    {
        void* mem = AllocSub(0x138, 4);
        if (mem) Reimpl_FUN_1400f9180(mem);
        *(void**)(p + 0x118) = mem;
    }

    // Step 13 — EntityUI (+0x120, 0xb0).
    {
        void* mem = AllocSub(0xb0, 4);
        if (mem) Reimpl_FUN_1401025c0(mem);
        *(void**)(p + 0x120) = mem;
    }

    // Step 14 — DayCounter Time struct (+0x128, 0x18): flag=1, Time(+8,0.0), Time(+0x10,3600.0).
    {
        void* mem = AllocSub(0x18, 8);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            *(uint32_t*)d = 1;
            if (fn_Time_Ctor_Float) {
                fn_Time_Ctor_Float(d + 8, 0.0f);
                fn_Time_Ctor_Float(d + 0x10, 3600.0f);
            }
        }
        *(void**)(p + 0x128) = mem;
    }

    // Step 15 — Weather sim (+0x130, 0x78).
    {
        void* mem = AllocSub(0x78, 8);
        if (mem) Reimpl_FUN_140093f60(mem);
        *(void**)(p + 0x130) = mem;
    }

    // Step 16 — env config: bind (envRes, climateGen) into the weather sim.
    //
    // envRes = EnvironmentDescription loaded from Environment/Environment.rsc —
    // it holds the 12 lighting keyframes AND the rain/snow Weather descriptor
    // array (envRes+0x28) that the particle engine builds effects from. Retail's
    // GameUpdateLoop::Load FindOrLoads it and passes it to FUN_140093e30; we do
    // the same. A partial/failed load degrades gracefully to null (the weather
    // sim still runs its temperature/snow half).
    if (*(void**)(p + 0x130)) {
        void* envRes = nullptr;
        if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton) {
            System::Filename fname("Environment/Environment.rsc");
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            envRes = const_cast<::Object::External*>(
                reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)
                    ->FindOrLoad(fname));                                    // B.3.3 bridge
            int wxCount = envRes ? *(int*)((uint8_t*)envRes + 0x30) : 0;
            fprintf(stderr, "[GUL::Weather] Environment.rsc envRes=%p weatherDescCount=%d\n",
                    envRes, wxCount);
            fflush(stderr);
        }
        Reimpl_FUN_140093e30(*(void**)(p + 0x130), envRes, /*climateGen*/*(void**)(p + 0x50));
    }

    // Step 17 — StringTable-pair struct (+0x138, 0x28) zero + flag bytes.
    {
        void* mem = AllocSub(0x28, 4);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            memset(d, 0, 0x28);
            *(uint32_t*)(d + 0x10) = 0x01010101;
            *(uint16_t*)(d + 0x14) = 0x0101;
            *(uint8_t*)(d + 0x10) = 0;   // clear low byte (matches exe)
        }
        *(void**)(p + 0x138) = mem;
    }

    // Step 18 — Graphics::Scene (+0x60, 0x78).
    {
        void* mem = AllocSub(0x78, 8);
        if (mem && fn_Graphics_Scene_Ctor) fn_Graphics_Scene_Ctor(mem);
        *(void**)(p + 0x60) = mem;
    }

    // Step 19 — Audio::Scene (+0x78, 0x38).
    {
        void* mem = AllocSub(0x38, 8);
        if (mem && fn_AudioScene_Ctor) fn_AudioScene_Ctor(mem);
        *(void**)(p + 0x78) = mem;
    }

    // Step 20 — Picking::Scene (+0x70, 0x60).
    {
        void* mem = AllocSub(0x60, 8);
        if (mem && s_fn_Picking_Scene_Ctor) s_fn_Picking_Scene_Ctor(mem);
        *(void**)(p + 0x70) = mem;
    }

    // Step 21 - CharacterGrid: loose Morton quadtree over characters (+0x88, 0x60).
    {
        void* mem = AllocSub(0x60, 8);
        if (mem) Reimpl_FUN_1400182b0(mem, 2048.0f);
        *(void**)(p + 0x88) = mem;
    }

    // Step 22 — AnimationManager (+0x68, 0x9d8).
    {
        void* mem = AllocSub(0x9d8, 8);
        if (mem && s_fn_AnimMgr_Ctor) s_fn_AnimMgr_Ctor(mem);
        *(void**)(p + 0x68) = mem;
    }

    // Step 23 — WorldGeometry (+0x110, 0x230) via the registered Reimpl_Geometry_Init.
    {
        void* mem = AllocSub(0x230, 8);
        if (mem) Reimpl_Geometry_Init(mem);
        *(void**)(p + 0x110) = mem;
    }

    // Step 24 — SunLight (+0x100, 0x108): ctor + Set(dir,col1,col2). No exe vtable override
    // (matches the proven menu path, which uses the Runtime default SunLight vtable).
    {
        void* mem = AllocSub(0x108, 8);
        if (mem && fn_Graphics_SunLight_Ctor) {
            fn_Graphics_SunLight_Ctor(mem);
            // Initial sun dir = normalize(0.5, 0.33, 1.33) then negate (faithful to
            // the original Load, exe .rdata DAT_140172114/_10c/_15c). This is just a
            // bootstrap: in standalone Weather_SunUpdate (gameupdateloop.cpp Draw
            // step 5) overrides the SunLight every frame with the native time-of-day
            // day cycle, so this value only matters for the first pre-Draw frame.
            float dir[3] = { 0.5f, 0.33f, 1.33f };
            float len = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
            if (len > 0.0f) { dir[0] = -dir[0]/len; dir[1] = -dir[1]/len; dir[2] = -dir[2]/len; }
            float col1[4] = { 0.8f, 0.8f, 0.8f, 0.85f };
            float col2[4] = { 0.37f, 0.37f, 0.4f, 1.0f };
            if (fn_Graphics_SunLight_Set) fn_Graphics_SunLight_Set(mem, dir, col1, col2);
            *(uint32_t*)((uint8_t*)mem + 0x28) = 0;
        }
        *(void**)(p + 0x100) = mem;
    }
    if (*(void**)(p + 0x110)) *(uint32_t*)(*(uint8_t**)(p + 0x110) + 0x28) = 1;  // worldGeom +0x28 = 1

    // Step 25 — SaveLoad (+0x140, 0x90).
    {
        void* mem = AllocSub(0x90, 4);
        if (mem && s_fn_SaveLoad_Ctor) s_fn_SaveLoad_Ctor(mem);
        *(void**)(p + 0x140) = mem;
    }

    // Step 26 — KeyState/map-input struct (+0x148, 0x38) zero-init.
    {
        void* mem = AllocSub(0x38, 4);
        if (mem) memset(mem, 0, 0x38);
        *(void**)(p + 0x148) = mem;
    }

    // Step 27 — MusicTracks struct (+0x80, 0x18): {idx=-1, res=null (Find deferred), 0}.
    {
        void* mem = AllocSub(0x18, 4);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            *(int32_t*)(d + 0x00) = -1;
            *(void**)(d + 0x08) = nullptr;     // MusicTrackList - DEFERRED.
            // BLOCKER for porting MusicPlayer::Update (0x1400a40e0): that
            // function reads *(int*)(musicPlayer[+0x08] + 0x28) as the track
            // count with no null check, so it MUST NOT be wired until this
            // Find("Game/MusicTracks.rsc") is done. See LANE1 report row 39.
            *(uint64_t*)(d + 0x10) = 0;
        }
        *(void**)(p + 0x80) = mem;
    }

    // Step 28 — SkyMesh MeshInstance (+0x108): the sky `_clear_` dome (helper above).
    BuildSkyMesh_Step28(p);

    // Step 29 — Seasons/Events StringTables. Fill the +0x138 event-log struct's
    // StringTable slots: +0x00 = events (message templates, indexed by eventType),
    // +0x08 = seasons. AddEvent (FUN_140095580) formats every event's text from
    // these tables, so leaving them null yields empty event text AND risks a
    // null-deref in the season/date formatter (FUN_140095230 dereferences +0x08).
    // Load via ExternalFactory::FindOrLoad (m_resourceList is empty in standalone),
    // mirroring the Environment/SkyMesh finds above. The ":events"/":seasons"
    // suffix selects the named sub-resource within Dialog/StringTable.rsc.
    {
        void* evStruct = *(void**)(p + 0x138);
        if (evStruct && g_pExternalFactorySingleton && *g_pExternalFactorySingleton) {
            void* factory = *g_pExternalFactorySingleton;
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            ::Object::ExternalFactory* efactory =
                reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

            // Lazy-resolve the StringTable type-id from the factory tree (its exe
            // .rdata global is null in standalone). Same as optionsdialog.cpp's
            // LoadStringTable — the IsDerivedFrom validation is what makes FindOrLoad
            // return the actual :events/:seasons sub-table instead of the wrong level.
            static uint32_t s_stTid = 0;
            if (!g_TypeId_StringTable && s_stTid == 0 &&
                g_pFactorySingleton && *g_pFactorySingleton) {
                void* factoryObj = *(void**)*g_pFactorySingleton;
                if (factoryObj) s_stTid = reinterpret_cast<const ::Object::Factory*>(factoryObj)->GetTypeIdFromName(
                    "StringTable", true);   // B.3.3 bridge
                if (s_stTid) g_TypeId_StringTable = &s_stTid;
            }
            uint32_t stTid = g_TypeId_StringTable ? *g_TypeId_StringTable : 0;

            // Load a StringTable sub-resource. AdjustDelimiters normalizes the
            // "rsc:subname" delimiter so FindOrLoad resolves the named sub-table.
            auto loadStringTable = [&](const char* name) -> void* {
                // Retail: String(name) then Filename::AdjustDelimiters on it —
                // Filename(const char*) is exactly Set + AdjustDelimiters.
                System::Filename fname(name);
                void* ext = const_cast<::Object::External*>(
                    efactory->FindOrLoad(fname));                            // B.3.3 bridge
                if (!ext) return nullptr;
                if (stTid != 0 && fn_IsDerivedFrom && !fn_IsDerivedFrom(ext, stTid)) return nullptr;
                return ext;
            };

            void* evTable = loadStringTable("Dialog/StringTable.rsc:events");
            void* seTable = loadStringTable("Dialog/StringTable.rsc:seasons");
            *(void**)((uint8_t*)evStruct + 0x00) = evTable;
            *(void**)((uint8_t*)evStruct + 0x08) = seTable;

            fprintf(stderr, "[GUL::Load] step29 event StringTables: events=%p seasons=%p (tid=%u)\n",
                    evTable, seTable, stTid);
            fflush(stderr);
        }
    }

    // Step 30 — AchievementManager::SetEventHandler (retail 0x14009752e).
    //
    // The old note here said the singleton slot 0x1401514f0 was "not yet registered in
    // standalone, so g_ppAchievementManager is null → skip cleanly". That has not been
    // true since the slot was wired (engine_funcs.cpp) — the Achievements dialog reads
    // the same manager through it. The handler retail installs is exe 0x140096960, the
    // award TOAST (it formats the achievement title into an event-log entry, type 0x19).
    // PORTED 2026-09-06 (exe close-out lane C): code/Game/Game/eventlog_achievement.cpp,
    // registered by standalone/reg_closeout_lane_c.cpp. Until then the handler was left
    // null, which cost only the on-screen announcement — Runtime's award bookkeeping and
    // the "Ach" registry value never depended on it.
    //
    // Retail: RAX = *0x1401514f0 (AchievementManager::_singleton storage);
    //         RCX = *RAX; RDX = 0x140096960; CALL SetEventHandler.
    // We call our port directly rather than through ResolveAddr — the address IS
    // registered for any retail-shaped caller, but this is the only call site.
    {
        typedef void (*fn_SetEventHandler_t)(void* self, void (*handler)(int));
        static fn_SetEventHandler_t s_setEventHandler = nullptr;
        if (!s_setEventHandler)
            s_setEventHandler = reinterpret_cast<fn_SetEventHandler_t>(
                ResolveExport("?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z"));
        void* achMgr = (g_ppAchievementManager && *g_ppAchievementManager)
                           ? **g_ppAchievementManager : nullptr;
        if (s_setEventHandler && achMgr) {
            s_setEventHandler(achMgr, &EventLog_OnAchievementAwarded);
            fprintf(stderr, "[GUL::Load] step30 AchievementManager::SetEventHandler "
                            "(mgr=%p handler=EventLog_OnAchievementAwarded)\n", achMgr);
        } else {
            fprintf(stderr, "[GUL::Load] step30 SetEventHandler SKIPPED (mgr=%p fn=%p)\n",
                    achMgr, (void*)s_setEventHandler);
        }
        fflush(stderr);
    }

    // Retail reads +0x15C into a register HERE, before either branch, and writes it
    // back after step 32 (0x1400976d7). That ordering is load-bearing on the
    // restore-from-save path: GameUpdateLoop's own serializer round-trips +0x15C
    // (gameupdateloop_serialize.cpp), so the SaveRestore below overwrites the field
    // with whatever the .sav held — the id the SCENARIO DIALOG chose has to survive.
    const int achievementId = *(int32_t*)(p + 0x15C);

    // Step 31 — worldgen (new-game branch, savedState==0). M5 5b first-light:
    // build the terrain mesh (base perlin island) + the gameplay tile-grid map
    // arrays + the TownManager coarse cell index (0x1400975c7). DEFERRED to later 5b slices:
    // CreateNaturalResource (trees/stone — spawns worker threads), and
    // FUN_1400a3300 start-town + initial-citizen spawn (which also sets the
    // camera position — until then the camera stays at the ctor default, framing
    // the map centre at the origin). See code/Game/Game/worldgen.cpp.
    // Step 30b — AchievementManager::Reset (retail 0x14009757d), the ELSE arm of the
    // same `if (self+0x58)` branch worldgen sits in: a NEW GAME resets the manager, a
    // load-from-save restores it through AchievementManager::Persist instead
    // (gameupdateloop_serialize.cpp, already ported).
    //
    // Reset is not a "clear the awards" call — it is the INSTANCE BUILDER. Runtime
    // 0x18009e5c0 resizes the array at mgr+0x18 to the achievement count and fills it
    // with Achievement::CreateInstance for every entry. Nothing else ever writes that
    // array. Without it mgr+0x18 stayed NULL and mgr+0x20 zero, and AchievementManager::
    // Update -- which we do call every tick -- returned immediately on its `count <= 0`
    // guard. Result: every goal evaluation, every award and every "Ach" registry write
    // was unreachable, and all 36 achievements sat at 0% forever with nothing in the log
    // to say why. The dialog looked healthy the whole time because it reads the
    // ACHIEVEMENT list (mgr+0x00), not the instance list.
    if (*(void**)(p + 0x58) == nullptr /*new game*/) {
        typedef void (*AchMgrReset_t)(void* self);
        static AchMgrReset_t s_fn_AchMgr_Reset = reinterpret_cast<AchMgrReset_t>(
            ResolveExport("?Reset@AchievementManager@Achievement@@QEAAXXZ"));
        void* mgr = (g_ppAchievementManager && *g_ppAchievementManager)
                        ? **g_ppAchievementManager : nullptr;
        if (s_fn_AchMgr_Reset && mgr) {
            s_fn_AchMgr_Reset(mgr);
            fprintf(stderr, "[GUL::Load] step30b AchievementManager::Reset(mgr=%p) -> "
                            "instances=%d\n", mgr, *(int32_t*)((uintptr_t)mgr + 0x20));
        } else {
            fprintf(stderr, "[GUL::Load] step30b Reset SKIPPED (fn=%p mgr=%p) — "
                            "no achievement can ever be awarded\n",
                    reinterpret_cast<void*>(s_fn_AchMgr_Reset), mgr);
        }
        fflush(stderr);
    }

    static const bool kEnableWorldgen = true;
    if (kEnableWorldgen && *(void**)(p + 0x58) == nullptr /*new game*/) {
        void* worldGeom  = *(void**)(p + 0x110);
        void* terrainGen = *(void**)(p + 0x48);
        int   sizeIdx    = *(int32_t*)(p + 0x34);
        uint32_t seed    = *(uint32_t*)(p + 0x30);
        if (worldGeom && terrainGen) {
            WorldGen_BuildTerrain(worldGeom, terrainGen, sizeIdx, seed);

            // Map dimension from the terrain size-table row (sizeRow[0]).
            int32_t* sizeTable = *(int32_t**)((uint8_t*)terrainGen + 0x80);
            uint32_t dim = sizeTable ? *(uint32_t*)((uint8_t*)sizeTable + (size_t)sizeIdx * 0x44) : 0;

            // Retail call site 0x1400975c7, immediately after the map dimension is
            // read and BEFORE the +0x148 tile arrays: size the TownManager's coarse
            // cell index to (mapWidth >> 3)^2 entries.
            //
            //     rcx = *(GameWorld+0xF0) + 0x140 ; edx = (W>>3) * (W>>3)
            //     call 0x140017640                ; array-of-arrays resize
            //
            // The array is TownManager+0x140 (header {ptr, count, cap}; each of the
            // count entries is a {int* ids, int count} bucket, stride 0x10), indexed
            // by COARSE cell = (row>>3)*(W>>3) + (col>>3). It is the spatial index the
            // FireWork water search walks: FireWork_SpiralCell_8730 does
            // `*(townMgr+0x140) + coarseCell*0x10` with no null test, exactly as retail
            // does, because retail sized it here. Left unsized the base stayed null and
            // the first ring probe faulted reading 0x2510 (= coarse cell 593 * 0x10).
            // This was the "TradeComponent trade-map resize" deferred at Step 31.
            {
                uint8_t* townMgr = *(uint8_t**)(p + 0xF0);
                if (townMgr && dim > 0) {
                    const int coarse = (int)dim >> 3;
                    Reimpl_FUN_140017640(townMgr + 0x140, coarse * coarse);
                    fprintf(stderr, "[TownMgr] coarse cell index sized: dim=%u coarse=%d "
                            "buckets=%d base=%p (FireWork water search indexes this; a "
                            "null base faults at coarseCell*0x10)\n",
                            dim, coarse, coarse * coarse,
                            *(void**)(townMgr + 0x140));
                    fflush(stderr);
                }
            }

            // Gameplay tile-grid arrays at the map struct (+0x148): store dim,
            // allocate two dim*dim arrays (+0x18/+0x28), zero the +0x28 one.
            void* mapStruct = *(void**)(p + 0x148);
            if (mapStruct && dim > 0) {
                *(uint32_t*)((uint8_t*)mapStruct + 4) = dim;
                int n = (int)(dim * dim);
                Reimpl_Array_Resize_Stride04((uint8_t*)mapStruct + 0x28, n);
                Reimpl_Array_Resize_Stride04((uint8_t*)mapStruct + 0x18, n);
                void* arr = *(void**)((uint8_t*)mapStruct + 0x28);
                if (arr) memset(arr, 0, (size_t)n * 4);
            }

            // M5 5c — build the gameplay TILE GRID at Game+0x80 (front-half of
            // CreateNaturalResource): classify every cell into a terrain type
            // (buildable / shore / water / mountain). Prerequisite for the
            // start-town spiral spawn-finder. See code/Game/Game/maptilegrid.cpp.
            void* game = *(void**)(p + 0xf8);
            if (game && dim > 0)
                MapTileGrid_Build(game, (int)dim);

            // task #34 — seed the map PATHFINDER worker object at game+0x68 (the real
            // game builds it in CreateNaturalResource_impl_sub_e8c0). Reads the cell
            // grid built just above, so it must run AFTER MapTileGrid_Build. Gated
            // behind BANISHED_LIVE_CHARACTER_MOVETICK (the only consumer); default no-op.
            if (game && dim > 0)
                Pathfinder_Seed(game, (int)dim);

            // keystone A — build the coarse REGION-CONNECTIVITY MAP (Map+0x70 graph +
            // per-cell regionId at cell+0x18 + Map+0x78 pair cache). Reads the terrain
            // cell grid built by MapTileGrid_Build above, so it must run AFTER it. Lets
            // Map_CalcDistance/Pf_IsDstReachable return realistic distances instead of
            // the euclidean+gridW fallback (which always exceeds work radii). Gated
            // behind BANISHED_LIVE_REGIONMAP (default no-op). See region_seed.cpp.
            if (game && dim > 0)
                Region_Seed(game, (int)dim);

            // move-tick tail — seed the CHARACTER SPATIAL-HASH MANAGER at GameWorld+0x88.
            // CharacterComponent::OnAdd allocates each citizen's char-index out of this; that
            // index (CharacterComponent+0x40 >= 0) is what un-gates the move-tick transform
            // tail. Reads GW/map itself, so no args. Runs before the citizen ECS spawn below,
            // so each citizen's Startup-time OnAdd finds a live manager. Same env gate.
            CharManager_Seed();

            // M5 5c — camera: spiral-search the buildable tile grid for the
            // start-town spawn tile and frame it. Writes the look-at target to
            // PlayerController+0x1c/+0x20/+0x24, exactly as Load does after
            // FUN_1400a3300 (call site 0x14009761d). The ECS spawn at the same
            // tile is deferred. See code/Game/Game/starttown.cpp.
            {
                void* startCfg   = *(void**)(p + 0x40);   // mapGen
                void* playerCtrl = *(void**)(p + 0xe0);
                if (startCfg && playerCtrl && game && dim > 0) {
                    float camPos[3] = { 0, 0, 0 };
                    StartTown_ComputeCamera(startCfg, game, camPos);
                    *(float*)((uint8_t*)playerCtrl + 0x1c) = camPos[0];
                    *(float*)((uint8_t*)playerCtrl + 0x20) = camPos[1];
                    *(float*)((uint8_t*)playerCtrl + 0x24) = camPos[2];
                    fprintf(stderr, "[startTown::DIAG]   camPos = (%.2f, %.2f, %.2f)\n",
                            camPos[0], camPos[1], camPos[2]);
                    fflush(stderr);

                    // M5 5c increment #3 — ECS spawn (FUN_1400a3300 steps 4-7).
                    // world base = the GameUpdateLoop (= g_pGameWorldSingleton):
                    // world+0xf8 = Game, world+0xe8 = entity list.
                    StartTown_SpawnEntities(startCfg, p);
                }
            }

            // [M5 5c DIAG] Start-town (FUN_1400a3300) prerequisite probe. Dumps the
            // state of startConfig (mapGen = loop+0x40), the Game tile grid (Game+0x80),
            // and the camera target, WITHOUT calling FUN_1400a3300 yet. Remove once
            // start-town is implemented. See project_newgame_path_roadmap / task #6/#7.
            {
                uint8_t* mapGen = *(uint8_t**)(p + 0x40);   // startConfig (MapGenerator rsc)
                uint8_t* game   = *(uint8_t**)(p + 0xf8);
                uint8_t* wgeom  = *(uint8_t**)(p + 0x110);
                uint8_t* pctrl  = *(uint8_t**)(p + 0xe0);
                fprintf(stderr, "[startTown::DIAG] mapGen(loop+0x40)=%p game=%p worldGeom=%p playerCtrl=%p\n",
                        mapGen, game, wgeom, pctrl);
                if (mapGen) {
                    fprintf(stderr, "[startTown::DIAG]   mapGen+0x3c(townSize)=%d\n",
                            *(int32_t*)(mapGen + 0x3c));
                    fprintf(stderr, "[startTown::DIAG]   pools: a0=%p a8(cnt)=%d | b0=%p b8(cnt)=%d | "
                            "c0=%p c8(cnt)=%d | d0=%p d8(cnt)=%d | e0=%d e4=%d e8=%d\n",
                            *(void**)(mapGen + 0xa0), *(int32_t*)(mapGen + 0xa8),
                            *(void**)(mapGen + 0xb0), *(int32_t*)(mapGen + 0xb8),
                            *(void**)(mapGen + 0xc0), *(int32_t*)(mapGen + 0xc8),
                            *(void**)(mapGen + 0xd0), *(int32_t*)(mapGen + 0xd8),
                            *(int32_t*)(mapGen + 0xe0), *(int32_t*)(mapGen + 0xe4),
                            *(int32_t*)(mapGen + 0xe8));
                }
                if (game) {
                    int gdim = *(int32_t*)game;
                    uint8_t* cells = *(uint8_t**)(game + 0x80);
                    fprintf(stderr, "[startTown::DIAG]   *game(dim)=%d  tileGrid(*(game+0x80))=%p  cnt=%d\n",
                            gdim, cells, *(int32_t*)(game + 0x88));
                    // Tile-type histogram (2=buildable 6/10=mountain 7=shore 8=deep water).
                    if (cells && gdim > 0) {
                        int hist[16] = {0};
                        int total = gdim * gdim;
                        for (int i = 0; i < total; ++i) {
                            uint8_t t = cells[(size_t)i * 0x30 + 0x29];
                            hist[t < 16 ? t : 0]++;
                        }
                        fprintf(stderr, "[startTown::DIAG]   tiles: buildable(2)=%d shore(7)=%d "
                                "deepWater(8)=%d mtnLo(10)=%d mtnHi(6)=%d\n",
                                hist[2], hist[7], hist[8], hist[10], hist[6]);
                    }
                }
                if (wgeom)
                    fprintf(stderr, "[startTown::DIAG]   worldGeom+0xc0(dim)=%d\n", *(int32_t*)(wgeom + 0xc0));
                // Keystone sanity: sample terrain height via the new fn_MapGetTerrainPos
                // (FUN_14009f3e0) at map centre + a few offsets. Banished valley => centre
                // should be low/buildable, edges higher.
                if (fn_MapGetTerrainPos) {
                    const float pts[4][2] = {{0,0},{-100,0},{100,100},{-120,-120}};
                    for (int i = 0; i < 4; ++i) {
                        float in[3] = { pts[i][0], pts[i][1], 0.0f };
                        float out[3] = {0,0,0};
                        fn_MapGetTerrainPos(game, out, in, 1, nullptr);
                        fprintf(stderr, "[startTown::DIAG]   height@(%.0f,%.0f) = %.3f\n",
                                pts[i][0], pts[i][1], out[2]);
                    }
                }
                fflush(stderr);
            }
        }
    }

    // Step 31-LOAD — restore-from-save branch (save/load wave 2026-08-27).
    // Retail Load's tail: `if (m_savedState) { this->vtbl[+0x38](this, 0,
    // savedState, &out); ~SavedState(); MemFree; m_savedState = 0; }` (vcall at
    // 0x14009754c) — the ENTIRE world state deserializes here instead of the
    // worldgen/StartTown branch above. We call the ported SaveRestore directly
    // (it IS our vtable +0x38 slot).
    if (*(void**)(p + 0x58) != nullptr) {
        void* savedState = *(void**)(p + 0x58);
        SetUnregPhase("load");
        fprintf(stderr, "[GUL::Load] LOAD-FROM-SAVE branch: SaveRestore(load) state=%p\n",
                savedState);
        fflush(stderr);
        // Char spatial-hash manager must exist BEFORE deserialize: citizen
        // CharacterComponents are persist-created during SaveRestore and their
        // OnAdd/enrolment allocates a char-index out of it (worldgen path seeds
        // it before the citizen spawn for the same reason).
        CharManager_Seed();
        int loadOut = 0;
        GameUpdateLoop_SaveRestore(loopVoid, 0, savedState, &loadOut);
        // BANISHED_SAVE_TEST=roundtrip: serialize the just-restored world again
        // and compare the raw bytes (gameupdateloop_serialize.cpp). No-op otherwise.
        GameUpdateLoop_SaveRoundtripCheck(loopVoid, savedState);
        if (s_fn_SavedState_Dtor) s_fn_SavedState_Dtor(savedState);
        if (fn_MemFreeStatic) fn_MemFreeStatic(savedState);
        *(void**)(p + 0x58) = nullptr;
        SetUnregPhase("postload");
        fprintf(stderr, "[GUL::Load] LOAD-FROM-SAVE done (out=%d)\n", loadOut);
        fflush(stderr);

        // Post-deserialize world-derived rebuilds. Retail runs the equivalent
        // (CreateNaturalResource_impl_sub_e8c0 + region flush) from the GRID
        // serializer's load tail (0x14009f100 -> 0x14009e8c0) — deferred there
        // as a documented deviation, done here instead, after ALL subsystems
        // (terrain heightmap, grid cells, entities) are restored. Same builders
        // + same order as the worldgen branch above; dim comes from the
        // deserialized map object itself (+0x00, what e8c0's *param_1 reads).
        {
            void* game = *(void**)(p + 0xf8);
            int dim = game ? *(int32_t*)game : 0;
            fprintf(stderr, "[GUL::Load] post-load rebuild: game=%p dim=%d\n", game, dim);
            fflush(stderr);
            if (game && dim > 0) {
                // MapTileGrid_Build is a WORLDGEN-ONLY step and must NOT run here.
                // It reallocates the stride-0x30 cell array and reclassifies it from
                // terrain, and the resize helper zeroes +0x26 for every "brand-new"
                // element -- so calling it after SaveRestore threw away the grid the
                // save had just restored, per-cell REGION IDS included (measured: the
                // array pointer changes and ids 3/3/3/2 become 0/0/0/0 across the call).
                //
                // Those ids are the last gate in FUN_1400a00f0, which is what
                // CitizenComponent::CheckWorkplaceValid uses: with every id 0 the test
                // fails even for a citizen standing ON its own workplace, so
                // CitizenComponent::Update unassigned every citizen from its job a few
                // hundred frames after the load. The town then stood still and starved.
                // The grid is serialized (GameGrid_SerializeCellArray_ac10 round-trips
                // +0x26), so on the load path there is nothing to rebuild.
                Pathfinder_Seed(game, dim);     // A* worker object (game+0x68)
                Region_Seed(game, dim);         // region graph + pair cache (+0x70/+0x78)

                // Regression canary. The per-cell region id (tile+0x26) is the last
                // gate in FUN_1400a00f0; if a future rebuild step reintroduces the
                // wipe, these read 0 and every citizen silently loses its job a few
                // hundred frames later. One line per load.
                {
                    const uint8_t* tiles =
                        *reinterpret_cast<const uint8_t* const*>((const uint8_t*)game + 0x80);
                    fprintf(stderr, "[GridProbe] tiles=%p regionIds:", (const void*)tiles);
                    if (tiles)
                        for (int c = 32000; c <= 35000; c += 500)
                            fprintf(stderr, " [%d]=%u/kind%u", c,
                                    *reinterpret_cast<const uint16_t*>(tiles + (size_t)c*0x30 + 0x26),
                                    tiles[(size_t)c*0x30 + 0x29]);
                    fprintf(stderr, "\n"); fflush(stderr);
                }
            }
        }
    }

    // Step 32 — AchievementInstance (retail 0x140097645-0x1400976dd).
    //
    // THIS IS WHAT STARTS A SCENARIO OR TUTORIAL. +0x15C is the index of the chosen
    // Scenario in the global resource ExternalList; ScenarioDialog::LoadSelected puts
    // it there (via WorldSetup_LoadFromSave) and passes a NULL instance IBP, so the
    // instance is built here and nowhere else. Scenario derives from
    // Achievement::Achievement, and SetAchievement -> Achievement::GoalList::Initialize
    // (Runtime 0x18009f0e0) walks the achievement's goal array, asks each goal for its
    // instance type id (Goal vtable +0x48) and constructs one through Object::Factory.
    // For a tutorial that goal is FinishTutorialGoal, whose FinishTutorialInstance
    // allocates the TutorialInstance in its own vtable +0x20 (exe 0x1400d7a90) and is
    // then stepped every tick through vtable +0x28 (-> 0x1400e1db0).
    //
    // Until this ran the port loaded the tutorial WORLD and played it as a sandbox:
    // no goal list, no popup, no steps. The old one-line `*(p+0x150) = nullptr` carried
    // the comment "SKIPPED on new game (achievementId == -1)" — true of a new game, but
    // it skipped unconditionally, so the scenario/tutorial path got no instance either.
    //
    // The block is inert on a new game (id == -1), which is why it is default-on;
    // BANISHED_LIVE_SCENARIO_GOALS=0 is the kill switch for an A/B run.
    *(void**)(p + 0x150) = nullptr;
    if (achievementId >= 0) {
        EnsureAchievementFuncs();
        if (!s_fn_AchievementInstance_Ctor || !s_fn_AchievementInstance_SetAchievement ||
            !fn_MemAlloc || !g_pMemAllocSingleton || !fn_InternalBasePtr_Ctor ||
            !g_pKeyMapSingleton) {
            fprintf(stderr, "[GUL::Load] step32 SKIPPED - unresolved (ctor=%p set=%p "
                            "alloc=%p ibp=%p reslist=%p)\n",
                    (void*)s_fn_AchievementInstance_Ctor,
                    (void*)s_fn_AchievementInstance_SetAchievement,
                    (void*)fn_MemAlloc, (void*)fn_InternalBasePtr_Ctor,
                    (void*)g_pKeyMapSingleton);
            fflush(stderr);
        } else {
            void* instance = nullptr;
            void* mem = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
            if (mem) {
                s_fn_AchievementInstance_Ctor(mem);
                // Retail installs the EXE's AchievementInstance vtable (0x14015e350)
                // over the one the Runtime ctor just wrote. Slot 0 is the exe's scalar
                // dtor (0x140019d90); slots 1-3 are JMP thunks straight back into the
                // Runtime base. If that address is unregistered we keep the Runtime
                // vtable, which differs only in the dtor — deliberately better than
                // installing a null.
                uintptr_t exeVt = ResolveAddr(Module::Exe, 0x14015e350);
                if (exeVt) *(void**)mem = (void*)exeVt;
                instance = mem;
            }
            uint64_t ibp = 0;
            new (&ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(instance));   // B.3.3 bridge
            *(uint64_t*)(p + 0x150) = ibp;

            // scenario = resourceList->data[achievementId]
            //   MOV RAX,[0x1401c3de8] / MOV RCX,[RAX+8] / MOV RAX,[RCX+0x20] / [RAX+idx*8]
            void* scenario  = nullptr;
            void* container = *g_pKeyMapSingleton;
            void* resList   = container ? *(void**)((uintptr_t)container + 0x08) : nullptr;
            void** data     = resList ? *(void***)((uintptr_t)resList + 0x20) : nullptr;
            if (data) scenario = data[achievementId];

            // IsDerivedFrom(scenario, Achievement::_typeInfo.typeId). Retail reads the id
            // through the .rdata slot at 0x140151f68, which points at the Runtime's
            // exported BaseTypeInfo for Achievement; its first dword IS the type id.
            void* checked = scenario;
            if (scenario && fn_IsDerivedFrom && s_AchievementTypeId)
                checked = fn_IsDerivedFrom(scenario, *s_AchievementTypeId) ? scenario : nullptr;

            void* inst = *(void**)(p + 0x150);
            fprintf(stderr, "[GUL::Load] step32 achievementId=%d instance=%p scenario=%p "
                            "derived=%p achTid=%u\n",
                    achievementId, inst, scenario, checked,
                    s_AchievementTypeId ? *s_AchievementTypeId : 0u);
            fflush(stderr);
            if (inst) s_fn_AchievementInstance_SetAchievement(inst, checked);
            *(int32_t*)(p + 0x15C) = achievementId;
        }
    }

    // Step 33 — finalize camera + add scene globals + unpause audio channels.
    if (*(void**)(p + 0xE0))
        Reimpl_FUN_140098660(*(void**)(p + 0xE0), 40.0f, 2.0f, 512.0f);
    {
        void* gScene = *(void**)(p + 0x60);   // g_pGameWorld+0x60
        if (gScene && fn_Graphics_Scene_AddGlobalObject) {
            if (*(void**)(p + 0x110)) fn_Graphics_Scene_AddGlobalObject(gScene, *(void**)(p + 0x110)); // WorldGeom
            // M5 5b lighting: add the SunLight so the directional light + ambient
            // light the terrain in the main render pass (Renderer::Draw dispatches the
            // light's vtable+0x50 SunLight::Draw → Context::SetLight + DrawItemList).
            // The crashing shadow pass (SunLight::DrawShadowMap, vtable+0x48) is gated by
            // renderer+0x3c, which GameUpdateLoop::Draw step 6 forces to 0 in standalone
            // ("Shadows: Off") — the shadow draw never runs, so the terrain renders lit
            // without the unwired shadow shader/render-target. Verified against the
            // proven menu path (which adds its SunLight the same way) + the Runtime
            // decompiles (Renderer::Draw 0x180104bb0, SunLight::Draw 0x1801067b0,
            // DrawShadowMap 0x180106bc0, AddScene 0x180105700).
            static const bool kAddSunLight = true;
            if (kAddSunLight && *(void**)(p + 0x100))
                fn_Graphics_Scene_AddGlobalObject(gScene, *(void**)(p + 0x100)); // SunLight
            // SkyMesh: add as a Scene global object so the reflection + main passes
            // both draw the `_clear_` sky dome (the dark-water fix; see BuildSkyMesh_Step28).
            static const bool kAddSkyMesh = true;
            if (kAddSkyMesh && *(void**)(p + 0x108))
                fn_Graphics_Scene_AddGlobalObject(gScene, *(void**)(p + 0x108)); // SkyMesh
        }
        void* aScene = *(void**)(p + 0x78);   // g_pGameWorld+0x78
        if (aScene && fn_Audio_Scene_Pause) {
            fn_Audio_Scene_Pause(aScene, 4, false);
            fn_Audio_Scene_Pause(aScene, 3, false);
            fn_Audio_Scene_Pause(aScene, 2, false);
        }
    }

    // Publish the completion signal LAST, after every subsystem + the world are
    // fully built, so GameUpdateLoop_IsWorldBuilt only returns true once the loop
    // is safe to transition into and draw. release-store pairs with the acquire
    // load in the poller.
    s_worldBuiltLoop.store(loopVoid, std::memory_order_release);

    fprintf(stderr, "[GUL::Load] EXIT — world built: game=%p ui=%p entityList=%p scene=%p sun=%p\n",
            *(void**)(p + 0xF8), *(void**)(p + 0x98), *(void**)(p + 0xE8),
            *(void**)(p + 0x60), *(void**)(p + 0x100));
    fflush(stderr);
}

// ============================================================
// STANDALONE_EXE: register the Load-owned exe addresses so gameupdateloop.cpp's
// EnsureInit (which resolves these via ResolveAddr) finds real bodies/values.
// ============================================================
#include "address_registry.h"

// .rdata float constants the in-game loop dereferences (*g_fXxx). Verified via
// read_memory: 1.0 / 6.0 (audio time scale) / 40.0 / 2.0 / 512.0 (camera params).
static float s_fConst_1_0      = 1.0f;    // 0x14017214c
static float s_fAudioTimeScale = 6.0f;    // 0x14017219c
static float s_fCamParam1      = 40.0f;   // 0x1401721dc
static float s_fCamParam2      = 2.0f;    // 0x140172174
static float s_fCamParam3      = 512.0f;  // 0x140172240

extern "C" void RegisterGameUpdateLoopLoadAddrs() {
    RegisterExeAddr(0x1400983f0, reinterpret_cast<uintptr_t>(&Reimpl_FUN_1400983f0)); // CalcTransform
    RegisterExeAddr(0x140098660, reinterpret_cast<uintptr_t>(&Reimpl_FUN_140098660)); // UpdateCamera
    RegisterExeAddr(0x14017214c, reinterpret_cast<uintptr_t>(&s_fConst_1_0));
    RegisterExeAddr(0x14017219c, reinterpret_cast<uintptr_t>(&s_fAudioTimeScale));
    RegisterExeAddr(0x1401721dc, reinterpret_cast<uintptr_t>(&s_fCamParam1));
    RegisterExeAddr(0x140172174, reinterpret_cast<uintptr_t>(&s_fCamParam2));
    RegisterExeAddr(0x140172240, reinterpret_cast<uintptr_t>(&s_fCamParam3));
}
