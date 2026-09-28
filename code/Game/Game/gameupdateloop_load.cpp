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

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::{Camera,Scene,Geometry,
// MeshInstance,SunLight} are real classes. These go AHEAD of "resolve.h" and the
// explicit <windows.h> below — <wingdi.h> declares a global `Rectangle` FUNCTION
// that hides `Volume::Rectangle`'s class name inside Volume.h's own
// static_asserts (Camera.h and Geometry.h include Volume.h).
#include "graphics/Camera.h"
#include "graphics/Scene.h"
#include "graphics/Mesh.h"        // MeshInstance
#include "graphics/SunLight.h"
#include "graphics/Material.h"   // Graphics::Material::TypeId() (ENDGAME B.3.9 TR)
#include "GameUpdateLoop.h"
#include "WorldGeometry.h"      // caller batch CB6: m_worldGeometry's class
#include <new>
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "system/Filename.h"
#include "system/String.h"
#include "resolve.h"
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
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K2)
#include "audio/Audio.h"     // ENDGAME B.3.7 callers (K4)
#include "ui/StringTable.h"  // UI::StringTable::TypeId() (D5, ENDGAME B.3.9 CG3)
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Device::_singleton
#include "Game/exe_bodies.h"
#include "graphics/Camera.h"            // RtGraphics_Camera_Ctor / Renderer_Ctor
#include "graphics/Animation.h"         // RtGraphics_AnimationManager_Ctor
#include "picking/Scene.h"              // RtPicking_Scene_Ctor
#include "ui/UserInterface.h"           // RtUi_UserInterface_Ctor
#include "save/SaveLoad.h"              // RtSave_SaveLoad_Ctor / RtSave_SavedState_Dtor
#include "utility/PerlinNoise.h"        // RtUtility_PerlinNoise_Ctor

// ============================================================
// Externs from sibling reimpl TUs (already registered/defined)
// ============================================================
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
// (The instance is the real class and SetAchievement a member — ENDGAME B.3.8 /
// B.3.7.) The Achievement type id is Achievement::Achievement::TypeId()
// (ENDGAME B.3.9 TR: was a by-name resolve of the Runtime record, cached in a
// namespace-scope atomic).

// B3R: the Runtime ctor/dtor doors Load used to resolve by name are called by name:
// RtSystem_Random_CtorSeed (system/Value.h), RtPicking_Scene_Ctor (picking/Scene.h),
// RtUi_UserInterface_Ctor (ui/UserInterface.h), RtGraphics_AnimationManager_Ctor
// (graphics/Animation.h), RtSave_SaveLoad_Ctor / RtSave_SavedState_Dtor
// (save/SaveLoad.h). The "[GUL::Load] resolves:" stderr line went with the cache.

// code/Game/Game/gameupdateloop_serialize.cpp — the serializer roundtrip check;
// the LOAD-FROM-SAVE branch below calls SaveRestoreOut (slot 7's body) directly.
extern "C" void GameUpdateLoop_SaveRoundtripCheck(GameUpdateLoop* self, void* loadedState);

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

static void LoadProfessionList_Step2(GameUpdateLoop* self) {

    if (!::Object::ExternalFactory::_singleton) {
        self->m_professionList = nullptr;
        fprintf(stderr, "[GUL::ProfList] SKIP — factory unavailable (factory=%p)\n",
                ::Object::ExternalFactory::_singleton);
        fflush(stderr);
        return;
    }

    void* factory = ::Object::ExternalFactory::_singleton;
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    // The singleton is still read through the EXISTING door, so the guard
    // above still gates the same cell and the same registered body still runs.
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    System::Filename fname("Game/Profession/Profession.rsc");
    void* profList = const_cast<::Object::External*>(
        efactory->FindOrLoad(fname));                                       // B.3.3 bridge
    self->m_professionList = profList;

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
static void LoadUIMaterial_Step3(GameUpdateLoop* self) {
    self->m_uiMaterial = nullptr;

    if (!::Object::ExternalFactory::_singleton)
        return;

    System::Filename fname("Material/UI/UIMaterialNoBlend.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* mat = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton
            ->FindOrLoad(fname));                                          // B.3.3 bridge

    // Retail's type gate: Graphics::Material's type id (ENDGAME B.3.9 TR: the class
    // accessor; was an RT_DATA_SLOT read of its ?_typeInfo@ record).
    bool isMat = false;
    if (mat)
        isMat = ::Object::IsDerivedFromNullSafe(mat, Graphics::Material::TypeId());
    if (isMat) self->m_uiMaterial = mat;

    fprintf(stderr, "[GUL::UIMat] gw+0xb8=%p (loaded=%p isMaterial=%d)\n",
            self->m_uiMaterial, mat, (int)isMat);
    fflush(stderr);
}

static void LoadHelpTopics_Step4(GameUpdateLoop* self) {
    self->m_helpTopics = nullptr;

    if (!::Object::ExternalFactory::_singleton) {
        fprintf(stderr, "[GUL::Help] SKIP — factory unavailable (factory=%p)\n",
                ::Object::ExternalFactory::_singleton);
        fflush(stderr);
        return;
    }

    System::Filename fname("Game/Help/HelpTopics.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* list = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton
            ->FindOrLoad(fname));                                          // B.3.3 bridge

    // Retail's type gate: Object::ExternalList's type id (retail reads it through the
    // exe import slot 0x140151ef0; ENDGAME B.3.9 TR: the class accessor).
    bool isList = false;
    if (list)
        isList = ::Object::IsDerivedFromNullSafe(list, ::Object::ExternalList::TypeId());
    if (isList) self->m_helpTopics = list;

    fprintf(stderr, "[GUL::Help] gw+0xa0=%p (loaded=%p isExternalList=%d topics=%d)\n",
            self->m_helpTopics, list, (int)isList,
            isList ? reinterpret_cast<const ::Object::ExternalList*>(list)->GetResourceCount()
                   : -1);
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

static void LoadDiseaseList_Step1(GameUpdateLoop* self) {
    self->m_diseaseList = nullptr;

    if (!::Object::ExternalFactory::_singleton) {
        fprintf(stderr, "[GUL::DiseaseList] SKIP — factory unavailable (factory=%p)\n",
                ::Object::ExternalFactory::_singleton);
        fflush(stderr);
        return;
    }

    System::Filename fname("Game/Disease/Diseases.rsc");
    // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
    void* list = const_cast<::Object::External*>(
        ::Object::ExternalFactory::_singleton
            ->FindOrLoad(fname));                                          // B.3.3 bridge

    // Retail's type gate (same ExternalList type id as the HelpTopics list).
    bool isList = false;
    if (list)
        isList = ::Object::IsDerivedFromNullSafe(list, ::Object::ExternalList::TypeId());
    if (isList) self->m_diseaseList = list;

    // B3R: the `fn_ExternalList_GetResourceCount` readiness test went with the door.
    int count = isList
                    ? reinterpret_cast<const ::Object::ExternalList*>(list)  // B.3.3 bridge
                          ->GetResourceCount()
                    : -1;
    fprintf(stderr, "[GUL::DiseaseList] gw+0xa8=%p (loaded=%p isExternalList=%d diseases=%d)\n",
            self->m_diseaseList, list, (int)isList, count);

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
    if (old) System::MemoryAllocator::MemFree(old);
    *(int32_t*)(d + 0x08) = 0;
    *(void**)(d + 0x00) = 0;
    int32_t cnt = *(int32_t*)(s + 0x08);
    *(int32_t*)(d + 0x08) = cnt;
    if (cnt > 0) {
        *(int32_t*)(d + 0x0C) = cnt;
        *(void**)(d + 0x00) = System::MemoryAllocator::MemAlloc((uint64_t)cnt << 2, 4);
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
        System::MemoryAllocator::MemFree(*reinterpret_cast<void**>(e + 0x00));
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
    RtGraphics_Camera_Ctor(reinterpret_cast<RtGraphics::Camera*>(p + 0x34),
                           reinterpret_cast<const ::Frustum*>(frustum), &t3, true);
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
    void* sub = System::MemoryAllocator::MemAlloc(0x28, 4);
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
    RtPicking_Scene_Ctor((void*)(p + 8));
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
    RtSystem_Random_Ctor(reinterpret_cast<System::Random*>(rng1));
    seed1 = reinterpret_cast<System::Random*>(rng1)->Rand();   // B.3.4b bridge
    RtSystem_Random_Ctor(reinterpret_cast<System::Random*>(rng2));
    seed2 = reinterpret_cast<System::Random*>(rng2)->Rand();   // B.3.4b bridge
    RtUtility_PerlinNoise_Ctor(reinterpret_cast<RtUtility::PerlinNoise*>(p + 0x34),
                               0.25f, 4.0f, (int)seed2, (int)seed1);
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
    {
        uint8_t* vp = reinterpret_cast<uint8_t*>(::Video::Device::_singleton);
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
    void* fr = RtVolume_Frustum_Ctor_Perspective(reinterpret_cast<::Frustum*>(frustum),
        reinterpret_cast<const Math::Scalar*>(&nearZ), reinterpret_cast<const Math::Scalar*>(&farZ),
        reinterpret_cast<const Math::Scalar*>(&fovX), reinterpret_cast<const Math::Scalar*>(&fovY));
    reinterpret_cast<Graphics::Camera*>((uint8_t*)camera + 0x34)->SetFrustum(
        *reinterpret_cast<const ::Frustum*>(fr ? fr : frustum));   // B.3.5 bridge
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
    if (!System::MemoryAllocator::_singleton) return nullptr;
    return System::MemoryAllocator::MemAlloc(size, align);
}

// ============================================================
// Step 28 helper — SkyMesh MeshInstance (+0x108): the sky `_clear_` dome.
// ============================================================
//
// Faithful port of the exe Load's sky-build (0x140097356..0x140097420):
// MemAlloc a Graphics::MeshInstance (0x120 bytes) and construct the REAL
// Runtime class on it (ENDGAME B.3.8 LOOP, B38_RULES D8: retail runs the
// imported ctor, then stamps the exe's 9-slot COPY of the table, 0x14015e240 —
// slot 0 the exe scalar deleting dtor 0x140019e80, slots 1-8 IAT thunks into
// the Runtime bodies — an artefact of the inlined ctor, not an exe class; the
// class's own DeletingDtor is that exe body since block RB), FindOrLoad
// "Models/Sky/SkyMesh.rsc", SetMesh
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
// LOADER THREAD: no function-local resolve cache (B6-5) — the ctor is the
// class's, and Time(float) is engine_funcs' boot-resolved fn_Time_Ctor_Float
// (the same export, `??0Time@System@@QEAA@M@Z`).
static void BuildSkyMesh_Step28(GameUpdateLoop* self) {
    void* mem = AllocSub(0x120, 8);
    self->m_skyMesh = mem;
    if (!mem) {
        fprintf(stderr, "[GUL::SkyMesh] SKIP — mem=%p\n", mem);
        fflush(stderr);
        return;
    }
    ::new (mem) Graphics::MeshInstance();

    // Load the sky dome resource (the `_clear_`-material mesh) via the global factory.
    void* meshRes = nullptr;
    if (::Object::ExternalFactory::_singleton) {
        void* factory = ::Object::ExternalFactory::_singleton;
        // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
        ::Object::ExternalFactory* efactory =
            reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge
        System::Filename fname("Models/Sky/SkyMesh.rsc");
        meshRes = const_cast<::Object::External*>(efactory->FindOrLoad(fname));  // B.3.3 bridge
    }

    // B.3.5 bridge — ?SetMesh@MeshInstance@Graphics@@…. The `s_SetMesh` cache, its
    // typedef, its resolve and the `SetMesh=%p` it fed the [GUL::SkyMesh] fprintf
    // all went in the closing pass: a diagnostic that prints the address of a door
    // nobody calls any more reports nothing. `meshRes` is an object test and stays.
    if (meshRes)
        reinterpret_cast<Graphics::MeshInstance*>(mem)->SetMesh(
            Graphics::ExternalPtrHandle{meshRes});   // B.3.5 bridge

    // Startup: vtable[+8](skyMesh, Time(0)) — matches the original Load's
    // `(*(code**)(*skyMesh + 8))(skyMesh, &Time0)`.
    {
        uint64_t timeObj = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeObj), 0.0f);
        // B.3.5 bridge — vtable+0x08 is slot 1 of `Graphics::Geometry`, i.e.
        // `Update(const System::Time&)`. The receiver is a PROVED MeshInstance:
        // 0x120 bytes from AllocSub, run through `??0MeshInstance@Graphics@@` above.
        reinterpret_cast<Graphics::Geometry*>(mem)->Update(
            *reinterpret_cast<const System::Time*>(&timeObj));   // B.3.5 bridge
    }
    reinterpret_cast<Graphics::MeshInstance*>(mem)->_detailId = 0;   // +0x28  B.3.5 bridge

    fprintf(stderr, "[GUL::SkyMesh] built skyMesh=%p meshRes=%p\n",
            mem, meshRes);
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

// ENDGAME B.3.8 LOOP: slot 6 of the real class (was the hand-built table's
// GameUpdateLoop_Load). Caller batch CB6: the fields are the class's named
// members; `loopVoid` is only the build guard's / log's pointer value.
void GameUpdateLoop::Load() {
    void* const loopVoid = this;
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

    fprintf(stderr, "[GUL::Load] ENTER self=%p seed=%d mapSize=%d climateGen=%p terrainGen=%p saved=%p\n",
            loopVoid, m_seed, m_mapSize,
            m_climateConfig, m_terrainConfig, m_savedState);
    fflush(stderr);

    // GameUpdateLoop::_singleton = this loop (0x140096b35; read by AddGlobalObject /
    // Audio::Pause at the tail).
    _singleton = this;

    // Steps 1-4: resource Finds (Diseases/Profession/Material/Help). Only step 3
    // (UI material) is still DEFERRED (left null; consumers guarded).
    //   step 1 Diseases    → +0xA8, opt-in BANISHED_LIVE_DISEASELIST (unblocks citizen illness)
    //   step 2 Professions  → +0xB0, the citizen-economy keystone (BANISHED_LIVE_PROFESSIONLIST)
    //   step 4 HelpTopics   → +0xA0, consumed only by the help dialog
    LoadDiseaseList_Step1(this);       // step 1 → +0xA8 (diseases); env-gated, else null
    LoadProfessionList_Step2(this);    // step 2 → +0xB0 (professions); env-gated, else null
    LoadUIMaterial_Step3(this);        // step 3 → +0xB8 (UI material — SaveLoad::Draw overlay)
    LoadHelpTopics_Step4(this);        // step 4 → +0xA0 (help topics)

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
        if (mem) RtSystem_Random_CtorSeed(static_cast<System::Random*>(mem), static_cast<uint32_t>(m_seed));
        m_random = mem;
    }

    // Step 6 — PlayerController/Camera (+0xE0, 0x10c). Initial frustum (overwritten by step 33).
    {
        int width = 1920, height = 1080;
        {
            uint8_t* vp = reinterpret_cast<uint8_t*>(::Video::Device::_singleton);
            if (vp) { width = *(int32_t*)(vp + 8); height = *(int32_t*)(vp + 4); }  // step6 reads +8/+4
        }
        float aspect = (height != 0) ? (float)width / (float)height : 1.7777778f;
        void* mem = AllocSub(0x10c, 4);
        if (mem) {
            float nearZ = 2.0f, farZ = 70.0f, fovX = 60.0f, fovY = aspect * 60.0f;
            uint8_t frustum[0xC0] = {};
            RtVolume_Frustum_Ctor_Perspective(reinterpret_cast<::Frustum*>(frustum),
                reinterpret_cast<const Math::Scalar*>(&nearZ), reinterpret_cast<const Math::Scalar*>(&farZ),
                reinterpret_cast<const Math::Scalar*>(&fovX), reinterpret_cast<const Math::Scalar*>(&fovY));
            Reimpl_FUN_1400985b0(mem, frustum, 0.0f, -40.0f, 25.0f);
        }
        m_playerController = mem;
    }

    // Step 7 — Renderer (+0x90, 0x50).
    {
        void* mem = AllocSub(0x50, 4);
        if (mem) RtGraphics_Renderer_Ctor(static_cast<RtGraphics::Renderer*>(mem));
        m_renderer = mem;
    }

    // Step 8 — EntityList (+0xE8, 0xe8).
    {
        void* mem = AllocSub(0xe8, 4);
        if (mem) Reimpl_FUN_1400c3f90(mem);
        m_entityList = mem;
    }

    // Step 9 — UserInterface (+0x98, 0x128).
    {
        void* mem = AllocSub(0x128, 4);
        if (mem) RtUi_UserInterface_Ctor(static_cast<UI::UserInterface*>(mem));
        m_userInterface = mem;
    }

    // Step 10 — town/trade manager (+0xF0, 0x1c0) — exe ctor 0x140090a30.
    {
        void* mem = AllocSub(0x1c0, 4);
        if (mem) Reimpl_TradeComponent_impl_0a30(mem);
        m_citizenList = mem;
    }

    // Step 11 — Game (+0xF8, 0xf8) — CRITICAL: clears the m_game==null Stage-C guards.
    {
        void* mem = AllocSub(0xf8, 8);
        if (mem) Reimpl_FUN_14009ee70(mem);
        m_game = mem;
    }

    // Step 12 — Tutorial-ish (+0x118, 0x138).
    {
        void* mem = AllocSub(0x138, 4);
        if (mem) Reimpl_FUN_1400f9180(mem);
        m_tutorial = mem;
    }

    // Step 13 — EntityUI (+0x120, 0xb0).
    {
        void* mem = AllocSub(0xb0, 4);
        if (mem) Reimpl_FUN_1401025c0(mem);
        m_entityUI = mem;
    }

    // Step 14 — DayCounter Time struct (+0x128, 0x18): flag=1, Time(+8,0.0), Time(+0x10,3600.0).
    {
        void* mem = AllocSub(0x18, 8);
        if (mem) {
            uint8_t* d = (uint8_t*)mem;
            *(uint32_t*)d = 1;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(d + 8), 0.0f);
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(d + 0x10), 3600.0f);
        }
        m_dayCounter = mem;
    }

    // Step 15 — Weather sim (+0x130, 0x78).
    {
        void* mem = AllocSub(0x78, 8);
        if (mem) Reimpl_FUN_140093f60(mem);
        m_weather = mem;
    }

    // Step 16 — env config: bind (envRes, climateGen) into the weather sim.
    //
    // envRes = EnvironmentDescription loaded from Environment/Environment.rsc —
    // it holds the 12 lighting keyframes AND the rain/snow Weather descriptor
    // array (envRes+0x28) that the particle engine builds effects from. Retail's
    // GameUpdateLoop::Load FindOrLoads it and passes it to FUN_140093e30; we do
    // the same. A partial/failed load degrades gracefully to null (the weather
    // sim still runs its temperature/snow half).
    if (m_weather) {
        void* envRes = nullptr;
        if (::Object::ExternalFactory::_singleton) {
            System::Filename fname("Environment/Environment.rsc");
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            envRes = const_cast<::Object::External*>(
                ::Object::ExternalFactory::_singleton
                    ->FindOrLoad(fname));                                    // B.3.3 bridge
            int wxCount = envRes ? *(int*)((uint8_t*)envRes + 0x30) : 0;
            fprintf(stderr, "[GUL::Weather] Environment.rsc envRes=%p weatherDescCount=%d\n",
                    envRes, wxCount);
            fflush(stderr);
        }
        Reimpl_FUN_140093e30(m_weather, envRes, /*climateGen*/m_climateConfig);
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
        m_eventNotifier = mem;
    }

    // Step 18 — Graphics::Scene (+0x60, 0x78).
    {
        void* mem = AllocSub(0x78, 8);
        if (mem) RtGraphics_Scene_Ctor(static_cast<RtGraphics::Scene*>(mem));
        m_graphicsScene = mem;
    }

    // Step 19 — Audio::Scene (+0x78, 0x38).
    {
        void* mem = AllocSub(0x38, 8);
        if (mem) RtAudio_Scene_Ctor(mem);
        m_audioScene = mem;
    }

    // Step 20 — Picking::Scene (+0x70, 0x60).
    {
        void* mem = AllocSub(0x60, 8);
        if (mem) RtPicking_Scene_Ctor(mem);
        m_pickingScene = mem;
    }

    // Step 21 - CharacterGrid: loose Morton quadtree over characters (+0x88, 0x60).
    {
        void* mem = AllocSub(0x60, 8);
        if (mem) Reimpl_FUN_1400182b0(mem, 2048.0f);
        m_characterGrid = mem;
    }

    // Step 22 — AnimationManager (+0x68, 0x9d8).
    {
        void* mem = AllocSub(0x9d8, 8);
        if (mem) RtGraphics_AnimationManager_Ctor(static_cast<RtGraphics::AnimationManager*>(mem));
        m_animationManager = mem;
    }

    // Step 23 — WorldGeometry (+0x110, 0x230): the real ctor (0x1400b00e0).
    // Caller batch CB6: was the Reimpl_Geometry_Init door (kept for its
    // address-registry row), which placement-news the same class.
    {
        void* mem = AllocSub(0x230, 8);
        if (mem) ::new (mem) WorldGeometry();
        m_worldGeometry = mem;
    }

    // Step 24 — SunLight (+0x100, 0x108): ctor + Set(dir,col1,col2). No exe vtable override
    // (matches the proven menu path, which uses the Runtime default SunLight vtable).
    {
        void* mem = AllocSub(0x108, 8);
        if (mem) {
            // ENDGAME B.3.8 LOOP (B38_RULES D8): the REAL Runtime class. Retail
            // runs the imported ctor and stamps the exe's 11-slot copy of its
            // table (0x14015e290, slot 0 the exe scalar deleting dtor
            // 0x140019e40); SunLight's own DeletingDtor is that body since RB.
            ::new (mem) Graphics::SunLight();
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
            // B.3.5 bridge — ?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z.
            reinterpret_cast<Graphics::SunLight*>(mem)->Set(
                *reinterpret_cast<const Math::Vector3*>(dir),
                *reinterpret_cast<const Math::Vector4*>(col1),
                *reinterpret_cast<const Math::Vector4*>(col2));   // B.3.5 bridge
            reinterpret_cast<Graphics::SunLight*>(mem)->_detailId = 0;   // +0x28  B.3.5 bridge
        }
        m_sunLight = mem;
    }
    if (m_worldGeometry)   // worldGeom +0x28 = 1
        static_cast<WorldGeometry*>(m_worldGeometry)->_detailId = 1;   // B.3.8 bridge

    // Step 25 — SaveLoad (+0x140, 0x90).
    {
        void* mem = AllocSub(0x90, 4);
        if (mem) RtSave_SaveLoad_Ctor(static_cast<Save::SaveLoad*>(mem));
        m_saveLoad = mem;
    }

    // Step 26 — KeyState/map-input struct (+0x148, 0x38) zero-init.
    {
        void* mem = AllocSub(0x38, 4);
        if (mem) memset(mem, 0, 0x38);
        m_imageCapture = mem;
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
        m_musicPlayer = mem;
    }

    // Step 28 — SkyMesh MeshInstance (+0x108): the sky `_clear_` dome (helper above).
    BuildSkyMesh_Step28(this);

    // Step 29 — Seasons/Events StringTables. Fill the +0x138 event-log struct's
    // StringTable slots: +0x00 = events (message templates, indexed by eventType),
    // +0x08 = seasons. AddEvent (FUN_140095580) formats every event's text from
    // these tables, so leaving them null yields empty event text AND risks a
    // null-deref in the season/date formatter (FUN_140095230 dereferences +0x08).
    // Load via ExternalFactory::FindOrLoad (m_resourceList is empty in standalone),
    // mirroring the Environment/SkyMesh finds above. The ":events"/":seasons"
    // suffix selects the named sub-resource within Dialog/StringTable.rsc.
    {
        void* evStruct = m_eventNotifier;
        if (evStruct && ::Object::ExternalFactory::_singleton) {
            void* factory = ::Object::ExternalFactory::_singleton;
            // ENDGAME B.3.3 step 6A: member call on the real Object::ExternalFactory.
            ::Object::ExternalFactory* efactory =
                reinterpret_cast<::Object::ExternalFactory*>(factory);   // B.3.3 bridge

            // StringTable type-id, read directly (D5): the IsDerivedFrom validation is
            // what makes FindOrLoad return the actual :events/:seasons sub-table
            // instead of the wrong level.
            uint32_t stTid = UI::StringTable::TypeId();

            // Load a StringTable sub-resource. AdjustDelimiters normalizes the
            // "rsc:subname" delimiter so FindOrLoad resolves the named sub-table.
            auto loadStringTable = [&](const char* name) -> void* {
                // Retail: String(name) then Filename::AdjustDelimiters on it —
                // Filename(const char*) is exactly Set + AdjustDelimiters.
                System::Filename fname(name);
                void* ext = const_cast<::Object::External*>(
                    efactory->FindOrLoad(fname));                            // B.3.3 bridge
                if (!ext) return nullptr;
                if (stTid != 0 && !::Object::IsDerivedFromNullSafe(ext, stTid)) return nullptr;
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
        void* achMgr = ::Achievement::AchievementManager::_singleton;
        if (achMgr) {
            reinterpret_cast<Achievement::AchievementManager*>(achMgr)->SetEventHandler(&EventLog_OnAchievementAwarded);   // B.3.7 bridge
            fprintf(stderr, "[GUL::Load] step30 AchievementManager::SetEventHandler "
                            "(mgr=%p handler=EventLog_OnAchievementAwarded)\n", achMgr);
        } else {
            fprintf(stderr, "[GUL::Load] step30 SetEventHandler SKIPPED (mgr=%p)\n",
                    achMgr);
        }
        fflush(stderr);
    }

    // Retail reads +0x15C into a register HERE, before either branch, and writes it
    // back after step 32 (0x1400976d7). That ordering is load-bearing on the
    // restore-from-save path: GameUpdateLoop's own serializer round-trips +0x15C
    // (gameupdateloop_serialize.cpp), so the SaveRestore below overwrites the field
    // with whatever the .sav held — the id the SCENARIO DIALOG chose has to survive.
    const int achievementId = m_scenarioIndex;

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
    if (m_savedState == nullptr /*new game*/) {
        void* mgr = ::Achievement::AchievementManager::_singleton;
        if (mgr) {
            reinterpret_cast<Achievement::AchievementManager*>(mgr)->Reset();   // B.3.7 bridge
            fprintf(stderr, "[GUL::Load] step30b AchievementManager::Reset(mgr=%p) -> "
                            "instances=%d\n", mgr, *(int32_t*)((uintptr_t)mgr + 0x20));
        } else {
            fprintf(stderr, "[GUL::Load] step30b Reset SKIPPED (mgr=%p) — "
                            "no achievement can ever be awarded\n",
                    mgr);
        }
        fflush(stderr);
    }

    static const bool kEnableWorldgen = true;
    if (kEnableWorldgen && m_savedState == nullptr /*new game*/) {
        void* worldGeom  = m_worldGeometry;
        void* terrainGen = m_terrainConfig;
        int   sizeIdx    = m_mapSize;
        uint32_t seed    = static_cast<uint32_t>(m_seed);
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
                uint8_t* townMgr = static_cast<uint8_t*>(m_citizenList);
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
            void* mapStruct = m_imageCapture;
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
            void* game = m_game;
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
                void* startCfg   = m_mapConfig;   // mapGen
                void* playerCtrl = m_playerController;
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
                    StartTown_SpawnEntities(startCfg, this);
                }
            }

            // [M5 5c DIAG] Start-town (FUN_1400a3300) prerequisite probe. Dumps the
            // state of startConfig (mapGen = loop+0x40), the Game tile grid (Game+0x80),
            // and the camera target, WITHOUT calling FUN_1400a3300 yet. Remove once
            // start-town is implemented. See project_newgame_path_roadmap / task #6/#7.
            {
                uint8_t* mapGen = static_cast<uint8_t*>(m_mapConfig);   // startConfig (MapGenerator rsc)
                uint8_t* game   = static_cast<uint8_t*>(m_game);
                uint8_t* wgeom  = static_cast<uint8_t*>(m_worldGeometry);
                uint8_t* pctrl  = static_cast<uint8_t*>(m_playerController);
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
                // Keystone sanity: sample terrain height via Reimpl_MapGetTerrainPos
                // (FUN_14009f3e0) at map centre + a few offsets. Banished valley => centre
                // should be low/buildable, edges higher.
                {
                    const float pts[4][2] = {{0,0},{-100,0},{100,100},{-120,-120}};
                    for (int i = 0; i < 4; ++i) {
                        float in[3] = { pts[i][0], pts[i][1], 0.0f };
                        float out[3] = {0,0,0};
                        Reimpl_MapGetTerrainPos(game, out, in, 1, nullptr);
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
    if (m_savedState != nullptr) {
        void* savedState = m_savedState;
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
        SaveRestoreOut(false, static_cast<Save::SavedState*>(savedState), &loadOut);
        // BANISHED_SAVE_TEST=roundtrip: serialize the just-restored world again
        // and compare the raw bytes (gameupdateloop_serialize.cpp). No-op otherwise.
        GameUpdateLoop_SaveRoundtripCheck(this, savedState);
        RtSave_SavedState_Dtor(static_cast<Save::SavedState*>(savedState));
        System::MemoryAllocator::MemFree(savedState);
        m_savedState = nullptr;
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
            void* game = m_game;
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
    m_scenarioInstance = nullptr;
    if (achievementId >= 0) {
        const uint32_t achTypeId = Achievement::Achievement::TypeId();
        if (!System::MemoryAllocator::_singleton || !g_gameContainer) {
            fprintf(stderr, "[GUL::Load] step32 SKIPPED - unresolved (reslist=%p)\n",
                    (void*)g_gameContainer);
            fflush(stderr);
        } else {
            void* instance = nullptr;
            void* mem = System::MemoryAllocator::MemAlloc(0x28, 8);
            if (mem) {
                // ENDGAME B.3.8 LOOP (B38_RULES D8): retail runs the imported
                // `??0AchievementInstance@Achievement@@` and then stamps the EXE's
                // copy of the class's table (0x14015e350: slot 0 the exe scalar
                // deleting dtor 0x140019d90 — ~AchievementInstance + MemFree on &1 —
                // slots 1-3 import thunks into the Runtime bodies). That copy is
                // an MSVC artefact of the inlined ctor, not an exe class: the real
                // Runtime class (concrete since block RB, its DeletingDtor = that
                // exe body) is built here and wears its compiler table. The old
                // path ran the ctor door and read 0x14015e350 by address, which
                // was never registered, so the object kept the minted table.
                // `::new`: the class declares (and does not define) its own
                // placement operator new.
                instance = ::new (mem) Achievement::AchievementInstance();
            }
            uint64_t ibp = 0;
            new (&ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(instance));   // B.3.3 bridge
            m_scenarioInstance = reinterpret_cast<void*>(ibp);

            // scenario = resourceList->data[achievementId]
            //   MOV RAX,[0x1401c3de8] / MOV RCX,[RAX+8] / MOV RAX,[RCX+0x20] / [RAX+idx*8]
            void* scenario  = nullptr;
            void* container = g_gameContainer;
            void* resList   = container ? *(void**)((uintptr_t)container + 0x08) : nullptr;
            void** data     = resList ? *(void***)((uintptr_t)resList + 0x20) : nullptr;
            if (data) scenario = data[achievementId];

            // IsDerivedFrom(scenario, Achievement::_typeInfo.typeId). Retail reads the id
            // through the .rdata slot at 0x140151f68, which points at the Runtime's
            // exported BaseTypeInfo for Achievement; its first dword IS the type id.
            void* checked = scenario;
            if (scenario && achTypeId)
                checked = ::Object::IsDerivedFromNullSafe(scenario, achTypeId) ? scenario : nullptr;

            void* inst = m_scenarioInstance;
            fprintf(stderr, "[GUL::Load] step32 achievementId=%d instance=%p scenario=%p "
                            "derived=%p achTid=%u\n",
                    achievementId, inst, scenario, checked,
                    achTypeId);
            fflush(stderr);
            // ExternalPtr<Achievement> arrives as a bare register value (B7-5).
            if (inst) reinterpret_cast<Achievement::AchievementInstance*>(inst)->SetAchievement(   // B.3.7 bridge
                Object::ExternalPtr<Achievement::Achievement>(reinterpret_cast<Achievement::Achievement*>(checked)));
            m_scenarioIndex = achievementId;
        }
    }

    // Step 33 — finalize camera + add scene globals + unpause audio channels.
    if (m_playerController)
        Reimpl_FUN_140098660(m_playerController, 40.0f, 2.0f, 512.0f);
    {
        void* gScene = m_graphicsScene;   // g_pGameWorld+0x60
        // B.3.5: the `fn_Graphics_Scene_AddGlobalObject` readiness term went with
        // the door; `gScene` is an object test and stays.
        if (gScene) {
            Graphics::Scene* gs = reinterpret_cast<Graphics::Scene*>(gScene);   // B.3.5 bridge
            if (m_worldGeometry)
                gs->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(m_worldGeometry)); // WorldGeom  B.3.5 bridge
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
            if (kAddSunLight && m_sunLight)
                gs->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(m_sunLight)); // SunLight  B.3.5 bridge
            // SkyMesh: add as a Scene global object so the reflection + main passes
            // both draw the `_clear_` sky dome (the dark-water fix; see BuildSkyMesh_Step28).
            static const bool kAddSkyMesh = true;
            if (kAddSkyMesh && m_skyMesh)
                gs->AddGlobalObject(reinterpret_cast<Graphics::Geometry*>(m_skyMesh)); // SkyMesh  B.3.5 bridge
        }
        void* aScene = m_audioScene;   // g_pGameWorld+0x78
        if (aScene) {
            reinterpret_cast<Audio::Scene*>(aScene)->Pause(4, false);   // B.3.7 bridge
            reinterpret_cast<Audio::Scene*>(aScene)->Pause(3, false);   // B.3.7 bridge
            reinterpret_cast<Audio::Scene*>(aScene)->Pause(2, false);   // B.3.7 bridge
        }
    }

    // Publish the completion signal LAST, after every subsystem + the world are
    // fully built, so GameUpdateLoop_IsWorldBuilt only returns true once the loop
    // is safe to transition into and draw. release-store pairs with the acquire
    // load in the poller.
    s_worldBuiltLoop.store(loopVoid, std::memory_order_release);

    fprintf(stderr, "[GUL::Load] EXIT — world built: game=%p ui=%p entityList=%p scene=%p sun=%p\n",
            m_game, m_userInterface, m_entityList,
            m_graphicsScene, m_sunLight);
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
