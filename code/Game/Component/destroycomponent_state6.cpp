// code/Game/Component/destroycomponent_state6.cpp
// DestroyComponent state 6 — COLLAPSE / RUBBLE. The arm the fire and tornado
// lanes both take, and until now the one remaining "[DestroySM] NOT-IMPLEMENTED
// state-6" stub in the whole destruction pipeline.
//
// ── How the game gets here ──────────────────────────────────────────────────
// FireComponent/TornadoComponent call FUN_14011e7b0 (ported as
// FireComponent_BeginBurning_11e7b0, firecomponent_smstate.cpp), which
//   * refuses when the building is unfinished (BuildComponent+0x48 == 0),
//   * stamps DestroyComponent+0x50 = 1  (the COLLAPSE flag) and
//     DestroyComponent+0x48 = the burn Time  (the collapse COUNTDOWN),
//   * drives the entity's state machine to state 3.
// State-3 enter (0x14011d7b0) then branches on that +0x50 byte: 0 -> state 4
// (the ordinary player demolition / upgrade path), non-zero -> state 6. So the
// whole disaster lane lands here, and with the handlers stubbed the building just
// sat wearing its intact mesh until the fire's own state machine forced state 7.
//
// ── The three retail handlers ───────────────────────────────────────────────
// Registration site: DestroyDescription_helper_dd00 (0x14011dd00), which calls
// `sub_3e50(6, A=0x14011d4d0, B=0x14011d400, D=0x14011d490)`. Per
// controller_state_table.h the populator's A/B/D land in record slots +0x08 /
// +0x20 / +0x18 — so A = enter, B = update, D = trans38 (the real per-frame tick,
// the same column state 4/5 use for their ticks):
//
//   enter   0x14011d4d0  Destroy_State6Enter_d4d0
//              cancel every citizen action that targets this building, empty its
//              storage, then either take a rubble step (countdown already spent)
//              or re-point the materials to the burnt set.
//   update  0x14011d400  Destroy_State6Update_d400
//              one step while the countdown is still running, plus a coin-flip
//              extra step (Random::Rand(rng, 2) == 0).
//   trans38 0x14011d490  Destroy_State6Tick_d490
//              per-frame: comp+0x48 -= *time; on reaching 0, clamp to 0 and take a
//              rubble step. This is what makes a burning building visibly fall
//              apart over its burn duration.
//
// ── The rubble step (0x14011d5d0) ───────────────────────────────────────────
// Not a one-shot "swap in the rubble mesh" — it steps the ModelComponent's
// mesh-variant index (+0x166) DOWN by one, floored at DestroyDescription+0x30, and
// re-registers the geometry in Graphics::Scene. Buildings author their damage
// stages as descending model variants, so repeated steps walk intact -> damaged ->
// rubble. Each step also re-points the mesh materials (0x140125020: the burnt
// material set at DestroyDescription+0x28) and re-stamps the ground decal
// (DecalComponent+0x28 = DestroyDescription+0x2c — the scorch mark).
//
// ── DestroyDescription fields ───────────────────────────────────────────────
// The names are the AUTHORED ones (field-name strings in Tools-x64.exe, keys in
// Template/*.rsc); the roles were confirmed from these bodies:
//   +0x28 _damagedSkin     rubble MATERIAL-SET index  -> GraphicsMesh::GetMaterialList
//   +0x2c _damagedDecal    rubble DECAL state         -> DecalComponent+0x28
//   +0x30 _minDamageLevel  rubble MESH-VARIANT floor  -> ModelComponent+0x166 min
//
// ── STANDALONE deviations (each logged once at runtime) ─────────────────────
//   * The PickingComponent pick-mesh rebuild (0x140128f50) is skipped — it is not
//     in the address registry, so a collapsing building keeps its intact pick
//     shape. A click-target nicety; the visuals are unaffected.
//   * ActionStack_AbortForEntity_c52c0 skips any action whose vtable slot 8/9 is
//     null. Our action vtables are partially-filled arrays, not the exe's, so an
//     unported slot must not be called — retail's are all real functions.
//
// DEFAULT-ON via the default-on table (graduated 2026-08-23 on a fire soak: 3 buildings
// collapsed 2 -> 1 -> 0 over 8 rubble steps, empty crash log). BANISHED_LIVE_DESTROY_COLLAPSE
// is now a kill-switch (=0) for A/B runs.

#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// ============================================================
// Tracing
// ============================================================
// Milestone lines are deliberately NOT behind the feature flag or a trace env: a
// gate that only speaks when the feature is enabled goes silent exactly when the
// feature starts working. Budgeted so a long burn cannot flood the log.
static int  s_milestones = 0;
static bool Milestone() { return s_milestones++ < 60; }

// ============================================================
// Resolved helpers
// ============================================================
typedef void* (*fn_FindIBP_t)(uintptr_t entity, void* retIBP);
typedef void  (*fn_ModelNotify_t)(void* ownerEntity, int mode);        // 0x1400c2b70
// 0x14000f090 was Collection::Array<Bytes<0x10,8>>::SetSize monomorphised
// into the exe (ENDGAME B.3.2 step 2: stride 0x10, align 8, zero tail) —
// converted at its call site below.
typedef void  (*fn_PickRebuild_t)(void* pickingComp);                  // 0x140128f50
typedef void  (*fn_SetMesh_t)(void* meshInstance, void* mesh);         // slot 0x140151498
typedef void* (*fn_GetMaterialList_t)(void* graphicsMesh, int setIdx); // slot 0x140152288
typedef void  (*fn_SetMaterialInst_t)(void* meshInst, int idx, void* extPtr); // slot 0x140151408
typedef uint32_t (*fn_Rand_t)(void* rng, uint32_t range);              // slot 0x1401512e8

static fn_FindIBP_t         s_findModel     = nullptr;  // 0x140003f60 (ModelComponent)
static fn_FindIBP_t         s_findStorage   = nullptr;  // 0x140006270 (StorageComponent)
static fn_FindIBP_t         s_findPicking   = nullptr;  // 0x140026a30 (PickingComponent)
static fn_ModelNotify_t     s_modelNotify   = nullptr;
static fn_PickRebuild_t     s_pickRebuild   = nullptr;
static fn_SetMesh_t         s_setMesh       = nullptr;
static fn_GetMaterialList_t s_getMatList    = nullptr;
static fn_SetMaterialInst_t s_setMatInst    = nullptr;
static fn_Rand_t            s_rand          = nullptr;
static uint32_t*            s_tidDestroyDesc = nullptr;  // 0x1401c7628
static uint32_t*            s_tidModelDesc   = nullptr;  // 0x1401c7a88
static uint32_t*            s_tidDecalComp   = nullptr;  // 0x1401c7580

static void* ReadSlot(uintptr_t ghidraSlot) {
    uintptr_t s = ResolveAddr(Module::Exe, ghidraSlot);
    return s ? *reinterpret_cast<void**>(s) : nullptr;
}

static bool s_resolved = false;
static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;
    s_findModel     = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_findStorage   = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_findPicking   = reinterpret_cast<fn_FindIBP_t>(ResolveAddr(Module::Exe, 0x140026a30));
    s_modelNotify   = reinterpret_cast<fn_ModelNotify_t>(ResolveAddr(Module::Exe, 0x1400c2b70));
    s_pickRebuild   = reinterpret_cast<fn_PickRebuild_t>(ResolveAddr(Module::Exe, 0x140128f50));
    s_setMesh       = reinterpret_cast<fn_SetMesh_t>(ReadSlot(0x140151498));
    s_getMatList    = reinterpret_cast<fn_GetMaterialList_t>(ReadSlot(0x140152288));
    s_setMatInst    = reinterpret_cast<fn_SetMaterialInst_t>(ReadSlot(0x140151408));
    s_rand          = reinterpret_cast<fn_Rand_t>(ReadSlot(0x1401512e8));
    s_tidDestroyDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7628));
    s_tidModelDesc   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a88));
    s_tidDecalComp   = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7580));
}

// A .rdata type-id global that never got seeded reads back 0 / 0xFFFFFFFF, and an
// IsDerivedFrom against it returns false — which silently DELETES the test it
// guards and hands the caller a null. Fall back to the name registry (the proven
// pattern from action_base_rawmat.cpp).
static uint32_t TypeIdCached(uint32_t* slot, const char* name, uint32_t* cache) {
    uint32_t t = slot ? *slot : 0;
    if (t != 0 && t != 0xFFFFFFFFu) return t;
    if (*cache == 0) *cache = ::Object::GetTypeIdByName(name);
    return *cache;
}
static uint32_t TidDestroyDesc() {
    static uint32_t c = 0; return TypeIdCached(s_tidDestroyDesc, "DestroyDescription", &c);
}
static uint32_t TidModelDesc() {
    static uint32_t c = 0; return TypeIdCached(s_tidModelDesc, "ModelDescription", &c);
}
static uint32_t TidDecalComp() {
    static uint32_t c = 0; return TypeIdCached(s_tidDecalComp, "DecalComponent", &c);
}

// Retail's `IsDerivedFrom(p, tid) ? p : nullptr` idiom.
static void* AsType(void* obj, uint32_t typeId) {
    if (!obj || !typeId || !fn_IsDerivedFrom) return nullptr;
    return fn_IsDerivedFrom(obj, typeId) ? obj : nullptr;
}


// Find a component through one of the member-return finders. `entitySlot` is the
// owning component's IBP field (comp+0x10); returns the component or null.
static void* FindOn(void* entitySlot, fn_FindIBP_t finder) {
    if (!finder || !entitySlot) return nullptr;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(entitySlot);
    if (!entity) return nullptr;
    void* out = nullptr;
    finder(entity, &out);
    if (!out) return nullptr;
    return out;
}

// ============================================================
// FUN_14000b630 — Entity::FindComponent<DecalComponent>
// ============================================================
// The shared member-return FindComponent shape: walk the entity's InternalBasePtr
// array (ptr @entity+0x30, count @+0x38), CheckPtr each slot, compare the object's
// GetTypeId (vtable +0x10) against DWORD_1401c7580, and on the first match
// re-CheckPtr + IsDerivedFrom before constructing retIBP from it. 0x1401c7580 is
// "DecalComponent" — the registration at 0x14014edd0 passes name="Decal",
// suffix="Component" (rdx/r8), the same pair-encoding ModelComponent uses.
extern "C" void* Destroy_FindDecalComponent_b630(uintptr_t entity, void* retIBP) {
    EnsureResolved();
    uint32_t typeId = TidDecalComp();
    void* found = nullptr;
    if (entity && typeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; ++i) {
            void** ibp = reinterpret_cast<void**>(base + static_cast<uintptr_t>(i) * 8);
            void* obj = *ibp;
            if (!obj) continue;
            uint32_t t = (*reinterpret_cast<uint32_t (**)(void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x10))(obj);
            if (t == typeId) {
                void* o = *ibp;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(o, typeId)) found = o;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    else *reinterpret_cast<void**>(retIBP) = found;
    return retIBP;
}

// ============================================================
// FUN_1400c52c0 — abort every action on one action stack that targets an entity
// ============================================================
// (stack, &entityIBP). Stack layout: +0x00 items (IBP array), +0x08 count,
// +0x10 the current action's IBP, +0x18 the current index.
// Pass 1 asks each live action `vtable+0x48(this, &entityCopy)` — "do you reference
// this entity?". If NONE does, the stack is left completely alone. If one does, pass
// 2 tears down the WHOLE stack: for every slot, walk the action and its child chain
// (child at action+0x10) calling `vtable+0x40` (abort) on each, release the slot's
// IBP and store a null IBP; finally reset the current index to -1 and release the
// current-action IBP. That is what stops citizens walking into a collapsing building.
extern "C" void ActionStack_AbortForEntity_c52c0(void* stack, void** pEntityIbp) {
    EnsureResolved();
    if (!stack || !pEntityIbp) return;
    uintptr_t st = reinterpret_cast<uintptr_t>(stack);
    int count = *reinterpret_cast<int*>(st + 8);
    if (count <= 0) return;

    // STANDALONE: our action vtables are partially-filled arrays. Calling an unported
    // slot 8/9 would jump through null; retail's are always real functions.
    auto slotOf = [](void* act, int idx) -> void* {
        void** vt = *reinterpret_cast<void***>(act);
        return vt ? vt[idx] : nullptr;
    };
    static bool s_warnedSlot = false;
    auto warnSlot = [&](int idx) {
        if (s_warnedSlot) return;
        s_warnedSlot = true;
        std::fprintf(stderr, "[Collapse] action-abort SKIPPED an action whose vtable "
                             "slot %d (+0x%x) is null — unported in standalone\n",
                     idx, idx * 8);
        std::fflush(stderr);
    };

    void** items = *reinterpret_cast<void***>(st);
    if (!items) return;

    bool hit = false;
    for (int i = 0; i < count && !hit; ++i) {
        void* act = items[i];
        if (!act) continue;
        act = items[i];
        if (!act) continue;
        void* fn = slotOf(act, 9);                 // +0x48 ReferencesEntity
        if (!fn) { warnSlot(9); continue; }
        void* entityCopy = *pEntityIbp;            // retail copies the raw ptr to a local
        hit = reinterpret_cast<char (*)(void*, void*)>(fn)(act, &entityCopy) != 0;
    }
    if (!hit) return;

    for (int i = 0; i < count; ++i) {
        void* cur = items[i];
        if (!cur) continue;
        for (void* p = cur; p != nullptr; ) {
            void* fn = slotOf(p, 8);               // +0x40 Abort
            if (!fn) { warnSlot(8); break; }
            reinterpret_cast<void (*)(void*)>(fn)(p);
            p = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(p) + 0x10);
        }
        if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(&items[i])->DeletePtr(
            );   // B.3.3 bridge
        void* nullIbp = nullptr;
        if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        items[i] = nullIbp;
    }

    *reinterpret_cast<int*>(st + 0x18) = -1;
    if (*reinterpret_cast<void**>(st + 0x10) != nullptr) {
        if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(st + 0x10))->DeletePtr(
            );   // B.3.3 bridge
        void* nullIbp = nullptr;
        if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<void**>(st + 0x10) = nullIbp;
    }

    if (Milestone()) {
        std::fprintf(stderr, "[Collapse] action stack %p aborted (%d slot(s)) — it targeted "
                             "the collapsing building\n", stack, count);
        std::fflush(stderr);
    }
}

// ============================================================
// FUN_140089e30 — TownManager: abort every citizen's actions for one entity
// ============================================================
// (townManager, &entityIBP). Walks the town's citizen roster (ptr @+0xa0,
// count @+0xa8) — the same array citizen_employ_cascade.cpp and citizen_onremove.cpp
// walk — and runs the abort above on each citizen's CitizenAIComponent action stack
// (citizen+0x278, list at +0x48).
extern "C" void TownMgr_AbortCitizenActionsFor_89e30(void* townMgr, void** pEntityIbp) {
    if (!townMgr || !pEntityIbp) return;
    uintptr_t tm = reinterpret_cast<uintptr_t>(townMgr);
    int n = *reinterpret_cast<int*>(tm + 0xa8);
    if (n <= 0) return;
    uintptr_t base = *reinterpret_cast<uintptr_t*>(tm + 0xa0);
    if (!base) return;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(base + static_cast<uintptr_t>(i) * 8);
        void* entityCopy = *pEntityIbp;
        uintptr_t citizen = reinterpret_cast<uintptr_t>(*slot);
        if (!citizen) continue;
        uintptr_t ai = *reinterpret_cast<uintptr_t*>(citizen + 0x278);
        if (!ai) continue;
        ActionStack_AbortForEntity_c52c0(reinterpret_cast<void*>(ai + 0x48), &entityCopy);
    }
}

// ============================================================
// ModelDescription mesh lookup
// ============================================================
// desc+0x20 is an array of 0x18-byte variant groups whose +0x08 holds the
// per-model-variant mesh array. Same math as modelcomponent.cpp's
// GetMeshFromDescription (there: `groupArray + 8 + variantIdx*0x18`; here the
// disassembly's `groups + (variantIdx*3)*8 + 8` — identical).
static void* MeshFromModelDesc(void* modelDesc, int16_t variantIdx, int16_t modelVariant) {
    if (!modelDesc || variantIdx < 0 || modelVariant < 0) return nullptr;
    uintptr_t groups = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(modelDesc) + 0x20);
    if (!groups) return nullptr;
    uintptr_t group = *reinterpret_cast<uintptr_t*>(groups + 8 + static_cast<int64_t>(variantIdx) * 0x18);
    if (!group) return nullptr;
    return *reinterpret_cast<void**>(group + static_cast<int64_t>(modelVariant) * 8);
}

// ============================================================
// FUN_140125020 — ModelComponent: re-point every mesh material to one material set
// ============================================================
// (modelComp, materialSetIdx). Bails when either variant index is negative. Reads
// the mesh for the CURRENT variant pair out of the ModelDescription, asks it for
// material set `materialSetIdx` (GraphicsMesh::GetMaterialList(int) -> const
// Collection::Array& {data @+0x00, count @+0x08}) and writes each entry into the
// MeshInstance at modelComp+0x28 (MeshInstance::SetMaterialInstance). This is what
// makes the collapsing building look CHARRED rather than merely lower-poly.
extern "C" void ModelComp_SetMaterialSet_125020(void* modelComp, int materialSetIdx) {
    EnsureResolved();
    if (!modelComp) return;
    uintptr_t m = reinterpret_cast<uintptr_t>(modelComp);

    int16_t variantIdx   = *reinterpret_cast<int16_t*>(m + 0x164);
    int16_t modelVariant = *reinterpret_cast<int16_t*>(m + 0x166);
    if (variantIdx < 0 || modelVariant < 0) return;

    void* modelDesc = AsType(*reinterpret_cast<void**>(m + 0x18), TidModelDesc());
    void* mesh = MeshFromModelDesc(modelDesc, variantIdx, modelVariant);
    if (!mesh) return;

    if (!s_getMatList || !s_setMatInst) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[Collapse] material re-point SKIPPED — dispatch slot %s "
                                 "unregistered; the rubble keeps its intact materials\n",
                         s_getMatList ? "0x140151408 (SetMaterialInstance)"
                                      : "0x140152288 (GetMaterialList)");
            std::fflush(stderr);
        }
        return;
    }

    void* list = s_getMatList(mesh, materialSetIdx);
    if (!list) return;
    void** data = *reinterpret_cast<void***>(list);
    int    n    = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(list) + 8);
    if (!data || n <= 0) return;
    for (int i = 0; i < n; ++i)
        s_setMatInst(reinterpret_cast<void*>(m + 0x28), i, data[i]);
}

// ============================================================
// FUN_14011d5d0 — ONE rubble step
// ============================================================
// Steps the model down one damage stage and refreshes everything hanging off the
// mesh. Called from all three state-6 handlers.
extern "C" void Destroy_RubbleStep_d5d0(void* destroyComp) {
    EnsureResolved();
    if (!destroyComp) return;
    uintptr_t comp = reinterpret_cast<uintptr_t>(destroyComp);
    void* entitySlot = reinterpret_cast<void*>(comp + 0x10);

    void* modelComp = FindOn(entitySlot, s_findModel);
    if (!modelComp) return;                   // retail: no ModelComponent, nothing to do
    uintptr_t mc = reinterpret_cast<uintptr_t>(modelComp);

    void* dd = AsType(*reinterpret_cast<void**>(comp + 0x18), TidDestroyDesc());
    if (!dd) {
        // STANDALONE ONLY. Retail derefs this unguarded because a DestroyComponent's
        // +0x18 is always a DestroyDescription; the way it comes back null HERE is an
        // unseeded type-id, which is our artifact rather than the game's.
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[Collapse] rubble step ABORTED — DestroyComponent+0x18 did "
                                 "not cast to DestroyDescription (type-id unseeded?)\n");
            std::fflush(stderr);
        }
        return;
    }
    uintptr_t d = reinterpret_cast<uintptr_t>(dd);
    const int rubbleMaterialSet = *reinterpret_cast<int*>(d + 0x28);
    const int rubbleDecalState  = *reinterpret_cast<int*>(d + 0x2c);
    const int rubbleMeshFloor   = *reinterpret_cast<int*>(d + 0x30);

    // _minDamageLevel is very often a hugely negative number (0xE1E1E1E1), and that is
    // CORRECT — not a deserialize bug. The resource compiler always emits the slot but
    // fills it with uninitialised memory when the .rsc omits the field, and only
    // Template/Market.rsc authors it (`int _minDamageLevel = 1`). See the long note in
    // destroydescription.cpp. An earlier revision of this function "refused" an
    // out-of-range value and substituted 0; that produced the right answer for the
    // wrong reason and printed a misleading warning, so it is gone. Retail's ordering
    // below already absorbs the garbage.

    // One step down the damage-stage ladder. Retail (0x14011d5d0) clamps to >= 0 FIRST
    // and only THEN raises to the description's floor — so a negative _minDamageLevel
    // never binds and the building collapses all the way to variant 0, while Market's
    // floor of 1 stops it at variant 1. The order of these two tests is load-bearing.
    const int cur = static_cast<int>(*reinterpret_cast<int16_t*>(mc + 0x166));
    int next = cur - 1;
    if (next < 0) next = 0;
    if (next < rubbleMeshFloor) next = rubbleMeshFloor;

    if (next != cur) {
        void** vt = *reinterpret_cast<void***>(mc);
        // vtable +0x38 (slot 7) removes the current geometry from Graphics::Scene and
        // +0x30 (slot 6) adds the new one back — modelcomponent.cpp implements both
        // (ModelComponent_OnCreate / ModelComponent_Draw).
        if (cur >= 0 && vt && vt[7]) reinterpret_cast<void (*)(void*)>(vt[7])(modelComp);
        *reinterpret_cast<int16_t*>(mc + 0x166) = static_cast<int16_t>(next);
        if (next >= 0) {
            void* modelDesc = AsType(*reinterpret_cast<void**>(mc + 0x18), TidModelDesc());
            void* mesh = MeshFromModelDesc(modelDesc,
                                           *reinterpret_cast<int16_t*>(mc + 0x164),
                                           static_cast<int16_t>(next));
            if (s_setMesh) s_setMesh(reinterpret_cast<void*>(mc + 0x28), mesh);
            if (vt && vt[6]) reinterpret_cast<void (*)(void*)>(vt[6])(modelComp);
        }
        if (Milestone()) {
            std::fprintf(stderr, "[Collapse] rubble step comp=%p mesh variant %d -> %d "
                                 "(floor %d, matSet %d, decal %d)\n",
                         destroyComp, cur, next, rubbleMeshFloor,
                         rubbleMaterialSet, rubbleDecalState);
            std::fflush(stderr);
        }
    }

    // Tell the owner entity its model changed (the MultiObjectComponent child forwarder).
    void* modelOwner = *reinterpret_cast<void**>(mc + 0x10);
    if (modelOwner && s_modelNotify) s_modelNotify(modelOwner, 0);

    // Burnt materials on whatever mesh is now showing.
    ModelComp_SetMaterialSet_125020(modelComp, rubbleMaterialSet);

    // Pick-mesh rebuild — unregistered in standalone (see the header note).
    void* picking = FindOn(entitySlot, s_findPicking);
    if (picking) {
        if (s_pickRebuild) {
            s_pickRebuild(picking);
        } else {
            static bool warned = false;
            if (!warned) {
                warned = true;
                std::fprintf(stderr, "[Collapse] pick-mesh rebuild SKIPPED — 0x140128f50 NOT "
                                     "REGISTERED; the rubble keeps its intact click shape\n");
                std::fflush(stderr);
            }
        }
    }

    // Scorch decal: swap the DecalComponent's state, and if a decal is currently
    // placed (+0x2c decalId >= 0) release and re-place it so the new state shows.
    void* decalIbp = nullptr;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(entitySlot);
    if (entity) Destroy_FindDecalComponent_b630(entity, &decalIbp);
    if (decalIbp) {
        uintptr_t dc = reinterpret_cast<uintptr_t>(decalIbp);
        if (rubbleDecalState != *reinterpret_cast<int*>(dc + 0x28)) {
            *reinterpret_cast<int*>(dc + 0x28) = rubbleDecalState;
            if (*reinterpret_cast<int*>(dc + 0x2c) >= 0) {
                void** vt = *reinterpret_cast<void***>(dc);
                if (vt && vt[7]) reinterpret_cast<void (*)(void*)>(vt[7])(decalIbp);
                if (vt && vt[6]) reinterpret_cast<void (*)(void*)>(vt[6])(decalIbp);
            }
        }
    }
}

// ============================================================
// FUN_14011d4d0 — state 6 ENTER
// ============================================================
// Note retail returns 0 (not 1) from both exits here; the dispatcher treats the
// enter return as "handled", and state 6 requests no transition of its own — the
// fire's own state machine forces state 7 when the burn ends.
extern "C" char Destroy_State6Enter_d4d0(void* data, void* /*sm*/) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    void* entitySlot = reinterpret_cast<void*>(comp + 0x10);

    // Building has storage -> cancel everything the town was doing with it, then
    // empty the storage outright (the goods burn; state 4's haul-away never runs).
    if (FindOn(entitySlot, s_findStorage) != nullptr) {
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            uintptr_t townMgr = *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
            void* entityCopy = *reinterpret_cast<void**>(entitySlot);
            if (townMgr) TownMgr_AbortCitizenActionsFor_89e30(
                reinterpret_cast<void*>(townMgr), &entityCopy);
        }
        void* storage = FindOn(entitySlot, s_findStorage);
        if (storage) {
            // B.3.2 bridge: exe 0x14000f090 IS Collection::Array<Bytes<0x10,8>>::SetSize(n).
            GameColl::AsArray<GameColl::Bytes<0x10, 8>>(
                reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(storage) + 0x28)).SetSize(0);
        }
        if (Milestone()) {
            std::fprintf(stderr, "[Collapse] state-6 enter comp=%p — storage emptied, "
                                 "citizen actions cancelled\n", data);
            std::fflush(stderr);
        }
    }

    void* dd = AsType(*reinterpret_cast<void**>(comp + 0x18), TidDestroyDesc());
    int64_t countdown = *reinterpret_cast<int64_t*>(comp + 0x48);

    if (countdown <= 0) {
        // Already spent (an instant destruction, e.g. a tornado strike) — go straight
        // to the rubble stage.
        if (Milestone()) {
            std::fprintf(stderr, "[Collapse] state-6 enter comp=%p countdown already 0 "
                                 "-> immediate rubble\n", data);
            std::fflush(stderr);
        }
        Destroy_RubbleStep_d5d0(data);
        return 0;
    }

    // Countdown still running: keep the current mesh but stamp the burnt material set
    // on it right away, so the building starts looking wrong the moment it catches.
    void* modelComp = FindOn(entitySlot, s_findModel);
    if (modelComp && dd)
        ModelComp_SetMaterialSet_125020(
            modelComp, *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dd) + 0x28));
    if (Milestone()) {
        std::fprintf(stderr, "[Collapse] state-6 enter comp=%p countdown=%lld — burning\n",
                     data, static_cast<long long>(countdown));
        std::fflush(stderr);
    }
    return 0;
}

// ============================================================
// FUN_14011d400 — state 6 UPDATE (record slot +0x20)
// ============================================================
// Runs only when the state machine has a transition pending. Retail takes one
// rubble step while the countdown is still running, and then a SECOND one on a
// coin flip — so both can fire in the same call. Kept as-is.
extern "C" char Destroy_State6Update_d400(void* data, void* /*sm*/) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);
    void* entitySlot = reinterpret_cast<void*>(comp + 0x10);

    if (FindOn(entitySlot, s_findModel) == nullptr) return 1;

    // Retail calls IsDerivedFrom(comp+0x18, DestroyDescription) here and DISCARDS the
    // result — no branch reads it. Not replicated: it has no side effect.

    if (*reinterpret_cast<int64_t*>(comp + 0x48) > 0)
        Destroy_RubbleStep_d5d0(data);

    if (s_rand && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        void* rng = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd8);
        if (rng && reinterpret_cast<System::Random*>(rng)->Rand(2u) == 0) Destroy_RubbleStep_d5d0(data);   // B.3.4b bridge
    }
    return 1;
}

// ============================================================
// FUN_14011d490 — state 6 TICK (record slot +0x18, the per-frame column)
// ============================================================
// comp+0x48 is the collapse countdown in raw System::Time units, seeded by
// FireComponent_BeginBurning_11e7b0. Subtract this frame's delta; when it reaches
// zero, clamp it and take the rubble step that puts the building in its final state.
extern "C" char Destroy_State6Tick_d490(void* data, void* /*sm*/, void* time) {
    EnsureResolved();
    uintptr_t comp = reinterpret_cast<uintptr_t>(data);

    int64_t remaining = *reinterpret_cast<int64_t*>(comp + 0x48);
    if (remaining == 0) return 1;
    remaining -= time ? *reinterpret_cast<int64_t*>(time) : 0;
    *reinterpret_cast<int64_t*>(comp + 0x48) = remaining;
    if (remaining > 0) return 1;

    *reinterpret_cast<int64_t*>(comp + 0x48) = 0;
    if (Milestone()) {
        std::fprintf(stderr, "[Collapse] state-6 countdown expired comp=%p -> rubble step\n", data);
        std::fflush(stderr);
    }
    Destroy_RubbleStep_d5d0(data);
    return 1;
}
