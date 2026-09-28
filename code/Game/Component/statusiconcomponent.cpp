// code/Game/Component/statusiconcomponent.cpp
// Clean reimplementation of StatusIconComponent lifecycle methods from Application-x64.exe.
//
// StatusIconComponent (0x108 bytes) displays floating status icons above entities
// (e.g., hunger, cold, disease). Embeds a Graphics::Geometry sub-object at +0x28
// (0xC0 bytes, with overridden vtable at 0x14015f0c0).
//
// Vtable at 0x140164620. Non-trivial overrides:
//   Shutdown  (0x14012bff0) — Serializes iconId field (+0xF0 as uint)
//   Serialize (0x14012bed0) — Reads sprite data from building description
//   OnRemove  (0x14012bbd0) — Manages Scene add/remove, positions icon (very complex)
//   OnUpdate2 (0x14012bf40) — Updates icon visibility, adds to Scene
//
// Decompiled from Application-x64.exe using Ghidra.

// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "graphics/Geometry.h"
#include "Component/statusiconcomponent_funcs.h"
#include "Component/StatusIconDescription.h"
#include "Component/ModelComponent.h"
#include "Component/MultiObjectComponent.h"
#include "scene_watch.h"
#include "engine_funcs.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include "perf_experimental.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K4)
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"   // ActHunt_NotifyEntityChanged_c3470

// Exe helpers, by retail address: 0x1400aa440 StatusIconComponent_GeometrySetup (below),
// 0x1400c3470 ActHunt_NotifyEntityChanged_c3470 (entity notification), 0x140152b00
// ??0Time@System@@QEAA@M@Z (RtSystem_Time_Ctor), 0x140004680 FloorComponent_FindResData
// (MapComponent), 0x14009f3e0 Reimpl_MapGetTerrainPos (grid cell -> world position).

// ============================================================
// Reimpl of FUN_1400aa440 (StatusIcon geometry-bounds setup)
// ============================================================
// Stores sprite parameters into the embedded Graphics::Geometry sub-object
// and computes its axis-aligned bounds. half = param2 * 0.5; bounds box is
// min(-half,-half,0) .. max(half,half, spriteCount*param2). 1:1 port; the
// only callee is Graphics::Geometry::SetBounds (Runtime export).
void StatusIconComponent_GeometrySetup(void* geometry, float param2, int spriteCount,
                                       void* param4, void* spriteSheet) {
    uint8_t* g = reinterpret_cast<uint8_t*>(geometry);
    float half = param2 * 0.5f;

    *(float*)(g + 0xC0) = param2;
    *(uint32_t*)(g + 0x30) = 2;
    *(int*)(g + 0xC4) = spriteCount;
    *(void**)(g + 0xD0) = spriteSheet;
    *(uint32_t*)(g + 0xC8) = 0;
    g[0xCC] = (uint8_t)(uintptr_t)param4;   // original stores low byte of param4

    // Box = { min.x, min.y, min.z, max.x, max.y, max.z } (6 contiguous floats)
    float box[6];
    uint32_t negHalf;
    { uint32_t bits; __builtin_memcpy(&bits, &half, 4); bits ^= 0x80000000u;
      __builtin_memcpy(&negHalf, &bits, 4); }
    __builtin_memcpy(&box[0], &negHalf, 4);   // min.x = -half
    __builtin_memcpy(&box[1], &negHalf, 4);   // min.y = -half
    box[2] = 0.0f;                            // min.z
    box[3] = half;                            // max.x
    box[4] = half;                            // max.y
    box[5] = (float)spriteCount * param2;     // max.z

    reinterpret_cast<Graphics::Geometry*>(geometry)
        ->SetBounds(*reinterpret_cast<const ::Box*>(box));   // B.3.5 bridge
}

// ============================================================
// Helper: Get StatusIconDescription sibling component
// ============================================================
static void* GetStatusIconDescription(uintptr_t self) {

    void* obj = *(void**)(self + 0x18);
    if (!obj) return nullptr;

    bool derived = Object::IsDerivedFromNullSafe(obj, StatusIconDescription::TypeId());
    return derived ? obj : nullptr;
}


// ============================================================
// StatusIconComponent::Serialize (vtable slot [4], 0x14012bed0)
// ============================================================
// Reads sprite data from StatusIconDescription, initializes geometry.
// Gets description, reads SpriteSheet::GetSpriteCount, calls helper
// to set up the geometry with sprite dimensions and sheet reference.

void StatusIconComponent_Serialize(StatusIconComponent* self) {
    uintptr_t s = (uintptr_t)self;

    void* desc = GetStatusIconDescription(s);
    if (!desc) return;
    uintptr_t d = (uintptr_t)desc;

    // Get sprite count from the SpriteSheet at description +0x20
    void* spriteSheet = *(void**)(d + 0x20);
    if (!spriteSheet) return;

    int spriteCount = reinterpret_cast<const UI::SpriteSheet*>(spriteSheet)->GetSpriteCount();  // B.3.6 bridge

    // Call geometry setup helper with:
    //   geometry (+0x28), float from desc +0x28, sprite count,
    //   value from desc +0x30 (Geometry data), SpriteSheet ptr from desc +0x20
    {
        float iconSize = *(float*)(d + 0x28);
        // desc +0x30 is passed as a Geometry-like value (copied by value in original)
        void* geomData = *(void**)(d + 0x30);
        StatusIconComponent_GeometrySetup(
            (void*)(s + 0x28), iconSize, spriteCount, geomData, spriteSheet);
    }
}


// ============================================================
// StatusIcon_EnsureSprite — retail's lazy sprite setup (component vtable slot 4)
// ============================================================
// Slot 4 (0x14012bed0, StatusIconComponent_Serialize below) is the ONLY thing that
// ever writes the embedded Geometry's sprite/material from the StatusIconDescription
// — `StatusIconComponent_helper_a440` has exactly one caller in the whole exe.
//
// Retail dispatches slot 4 from TWO places:
//   * ComponentEntity creation (FUN_1400c30a0, called only from CreateEntity), and
//   * slot 11 (0x14012bf40), on EVERY call, immediately before it adds the geometry
//     to the Scene.
// The second one is what makes icons work after a LOAD: a stream-restored component
// never goes through CreateEntity, so it never runs slot 4 at construction (see
// [[reference_ctor_scalars_in_startup_are_unset_after_load]]). Without the slot-11
// leg its geometry is added to the Scene with NO sprite and draws nothing — which is
// exactly what a loaded world (the tutorial, any .sav) showed: zero icons, no error.
//
// Retail brackets the call with a save/restore of +0xF0 because slot 4 clobbers it.
//
// ⚠ CALL THE PORT DIRECTLY, NEVER DISPATCH VTABLE SLOT 4 HERE.
// Retail's slot 4 is exactly StatusIconComponent_Serialize (0x14012bed0) — the sprite
// setup and nothing else. Until ENDGAME B.3.8 VIS OUR slot 4 ALSO re-ran the
// construction half (the Geometry ctor door on +0x28 and the scene-object id at
// +0x100 = -1), so dispatching it mid-life reset a live component. The real class's
// slot 4 no longer does, but the rule stands: call the port.
//
// This function is called from StatusIcon_Toggle — itself a standalone deviation,
// since retail's 0x14012c3f0 never touches slot 4 — immediately before Toggle's
// "not in the scene yet?" test. With +0x100 reset to -1 that test passed EVERY time,
// so Toggle re-Scene::AddObject'd the same geometry on every call and never removed
// the previous node. Measured 2026-09-04 on the tutorial: 16 icon geometries added
// 119,143 times, the Graphics::Scene growing a steady ~75 nodes/frame to 121,856
// nodes in a 5,355-entity world, StatusIconGeom_Draw called ~96,000 times per frame
// (1.9M draw calls/sec, climbing) and taking 67% of process time — the frame rate
// decayed from 60 fps to 8 over about two minutes.
//   [SceneAdd] StatusIconComponent::Show  calls=119143  distinct-geoms=16
//   [IconTrace] slot4 CLOBBERED sceneId comp=...  4849 -> -1
static void StatusIcon_EnsureSprite(uintptr_t c) {
    int32_t saved = *(int32_t*)(c + 0xF0);
    StatusIconComponent_Serialize(reinterpret_cast<StatusIconComponent*>(c));
    *(int32_t*)(c + 0xF0) = saved;
}

// ============================================================
// StatusIconComponent::OnUpdate2 (vtable slot [11], 0x14012bf40)
// ============================================================
// Updates icon visibility. Calls Geometry vtable +0x20, saves/restores
// fieldF0 value. If iconId < 0 and fieldF0 != 0: adds geometry to Scene,
// sets flag bit 2, notifies owner entity, then calls vtable +0x40 (OnRemove)
// with Time(0) to trigger positioning.

void StatusIconComponent_OnUpdate2(StatusIconComponent* self) {
    uintptr_t s = (uintptr_t)self;

    // Save fieldF0 (fieldE8 in longlong* terms = offset 0xF0)
    int32_t savedFieldF0 = *(int32_t*)(s + 0xF0);

    // Retail is
    //     lVar2 = param_1[0x1e];                    // +0xF0
    //     (**(code **)(*param_1 + 0x20))();         // THIS component's vtable, slot 4
    //     *(int *)(param_1 + 0x1e) = (int)lVar2;    // slot 4 clobbers +0xF0 -> restore
    // `*param_1` is the COMPONENT's own vtable pointer (offset 0), not the embedded
    // Geometry's at +0x28 — this copy read +0x28 and called the geometry's slot 4
    // instead, which is why nothing ever re-ran the sprite setup.
    StatusIcon_EnsureSprite(s);

    // Restore fieldF0 to saved value (original: *(int*)(param_1 + 0x1e) cast)
    *(int32_t*)(s + 0xF0) = savedFieldF0;

    // If iconId < 0 and fieldF0 != 0: add to scene and notify
    int32_t iconId = *(int32_t*)(s + 0x100);
    if (iconId < 0 && savedFieldF0 != 0) {
        // Add geometry to scene
        if (GameUpdateLoop::_singleton) {
            void* scene = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x60);
            if (scene) {
                uint32_t newId = reinterpret_cast<Graphics::Scene*>(scene)
                    ->AddObject(reinterpret_cast<Graphics::Geometry*>(s + 0x28));   // B.3.5 bridge
                SceneWatch_Note(newId, (void*)(s + 0x28), "StatusIconComponent::OnAdded");

                // Set flags: OR bit 2 (0x04) into flags at +0x20
                uint32_t oldFlags = *(uint32_t*)(s + 0x20);
                *(uint32_t*)(s + 0x100) = newId;
                uint32_t newFlags = oldFlags | 0x04u;
                *(uint32_t*)(s + 0x20) = newFlags;

                // If flags changed (low 16 bits) and owner non-null, notify entity
                if (((newFlags ^ oldFlags) & 0xFFFF) != 0) {
                    void* ownerEntity = *(void**)(s + 0x10);
                    if (ownerEntity != nullptr) {
                        ActHunt_NotifyEntityChanged_c3470(*(void**)(s + 0x10));
                    }
                }

                // Call vtable +0x40 (OnRemove) with Time(0) to position the icon
                {
                    uint8_t timeObj[8];
                    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeObj), 0.0f);
                    self->Tick(timeObj);   // vtable +0x40 (slot 8), a virtual dispatch in retail
                }
            }
        }
    }
}

// ============================================================
// Standalone bring-up (Phase A): faithful construction + diagnostics
// ============================================================
#include "Object/factory_registration.h"
#include "Component/StatusIconComponent.h"
#include <cstdlib>

// The embedded billboard (+0x28) is a real Graphics::Geometry subclass since
// ENDGAME B.3.8 VIS (StatusIconGeometry, StatusIconComponent.h; bodies in
// statusicon_geometry.cpp). The all-no-op fallback table and the minted-table
// copy it replaced are gone, and with them the Geometry ctor door this file
// resolved.

// Graphics::Geometry::SetTransform (Runtime export) — used by OnRemove positioning.

// Entity component finders: 0x140003f60 ModelComponent (type 0x1401c7a50) and
// 0x140004740 MultiObjectComponent (type 0x1401c7ac0) are replaced by the by-name
// lookup below; 0x140004680 FloorComponent_FindResData -> MapComponent (type 0x1401c79e0).
// CharacterComponent_impl_f3e0 (0x14009f3e0, Reimpl_MapGetTerrainPos) — grid cell ->
// world position.

// ============================================================
// [STANDALONE GUARD] component-array entry sanity (2 deviations, kept deliberately)
// ============================================================
// Retail's component walks (0x140003f60, 0x140004740, 0x140004680 ...) dereference every
// entry of the owner entity's component array with NO validation, because in retail the
// array is always consistent: an entity's removal clears the weak refs that point at it
// before anything else can walk them. Our removal lifecycle does not guarantee that yet
// (dangling weak refs after destroy/upgrade teardown are a known open issue), so a
// half-torn-down building can hand these walks freed memory. Live crash 2026-07-25:
// pausing a workplace whose upgrade was in flight faulted on READ of 0xa — a small
// non-pointer value read out of the array and dereferenced as a component pointer.
//
// SCOPE (thinned 2026-07-25 after the buildui wrong-finder fix landed and a live pause
// ran clean with none of these firing). Exactly two deviations from the decompile remain:
//   D2  the array ENTRY walked by StatusIcon_FindCompByName is range/alignment checked
//       before it is dereferenced — 0xa fails both (below 0x10000 and not 8-aligned),
//       as does every other small free-list-ish integer a reused block leaves behind.
//       This is the read that actually faulted, which is why it is the one kept.
//   D4  the "component never ran StartupInit" sentinel discipline (see SI_Initialised).
//
// D2 TESTS ONLY THE ENTRY, AND ONLY AS A DATA POINTER — do not extend it. It briefly also
// tested the component's vtable pointer and the GetTypeId function pointer at vtable+0x10;
// that second one was a live-fire bug (2026-07-25, defaults-only build, healthy world):
// a CODE address has no alignment guarantee, and measured on this build only 30 of 132
// component GetTypeId functions are 8-aligned — GCC packs these tiny statics with no
// padding. The check therefore rejected ~77% of all component types and silently skipped
// LIVE components during the ModelComponent / MultiObjectComponent lookups, which can
// select the wrong positioning branch. Every component pointer, by contrast, comes from
// Memory::Allocate(size, 8) and every vtable is an 8-aligned data symbol (66/66 measured),
// so the entry test cannot misfire the same way.
// Everything else that was here has been removed and those paths are byte-faithful to
// retail again: the owner-entity gate that suppressed the scene ops, the self/width/notify
// plausibility checks, and a whole-array pre-check in front of the exe map finder. That
// last one was dropped on principle rather than because it was harmless: the SAME array is
// walked unguarded by every WorkBase_FindComponent_* call site in the tree, so guarding one
// of them defends nothing systemically — if that corruption returns it will fault
// elsewhere first, and the fix belongs in the removal lifecycle, not here.
static inline bool SI_Plausible(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    return v >= 0x10000 && v < 0x00007fffffffffffull && (v & 7) == 0;
}

// Bounded "this should never happen" logging: the first few of each category go to
// standalone_error.log with the operands, then the category goes quiet so a per-tick
// caller cannot flood the log.
static bool SI_LogOnce(int* budget) {
    if (*budget <= 0) return false;
    --*budget;
    return true;
}

// PRECONDITION of D2, not a deviation in its own right: the per-entry check below reads
// THROUGH the array base, so `arr`/`count` have to be sane for it to mean anything. Retail
// reads both fields unguarded and so do we — this only rejects the values, it does not add
// a check the original lacks anywhere the original would have proceeded safely.
static bool SI_ComponentArray(uintptr_t entity, int* outCount, int64_t* outArr) {
    if (!SI_Plausible(reinterpret_cast<void*>(entity))) return false;
    int count = *(int*)(entity + 0x38);
    int64_t arr = *(int64_t*)(entity + 0x30);
    if (count < 0 || count > 4096) return false;
    if (count > 0 && !SI_Plausible(reinterpret_cast<void*>(arr))) return false;
    if (outCount) *outCount = count;
    if (outArr) *outArr = arr;
    return true;
}

// A StatusIconComponent that never ran StatusIconComponent_StartupInit still carries the
// inert factory's zero-fill: the embedded geometry's vtable (+0x28) is 0 AND the scene
// object id (+0x100) is 0. That 0 is "never initialised", NOT scene object 0 — feeding it
// to Graphics::Scene::RemoveObject spins forever (in-world freeze, see
// reference_inert_component_sentinel_fields), and AddObject-ing a geometry whose vtable is
// null faults in the renderer's draw pass. The geometry vtable is the in-band marker: the
// real ctor and StartupInit both always write it, and nothing ever clears it.
static inline bool SI_Initialised(uintptr_t c) {
    return *(void**)(c + 0x28) != nullptr;
}


// Removed-from-world handler (component vtable +0x38, idx 7). Entity teardown
// dispatches this via MarkForRemoval (0x1400c2ea0: every component with
// capability bit 1 — ours has 0xC2 — gets vtable+0x38 before the 0x400 bit is
// set). Remove the icon's scene object so the renderer's draw list never holds
// freed geometry — same crash class as the StorageDisplay pile meshes fixed
// 2026-07-20 (demolition → Draw-phase Transform::Lerp on freed memory). Retail
// additionally strips icons at demolition start (state-3 enter → helper_c2a0);
// this teardown-time removal is the safety net that must always run.
void StatusIconComponent::OnRemove() {
    uintptr_t c = (uintptr_t)this;
    if (GameUpdateLoop::_singleton == nullptr) return;
    void* scene = *(void**)((uintptr_t)GameUpdateLoop::_singleton + 0x60);
    int32_t sceneId = *(int32_t*)(c + 0x100);
    // [STANDALONE GUARD D4] a component that never ran StartupInit still has the inert
    // zero-fill, so sceneId 0 means "never registered" rather than scene object 0 —
    // RemoveObject on it spins forever (in-world freeze). Repair the sentinel instead.
    if (!SI_Initialised(c)) {
        if (sceneId == 0) *(int32_t*)(c + 0x100) = -1;
        return;
    }
    if (sceneId >= 0 && scene) {
        reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject((uint32_t)sceneId);   // B.3.5 bridge
        *(int32_t*)(c + 0x100) = -1;
    }
}

// The CTOR half of the spawn-time init (StatusIconComponent_CtorScalars until
// ENDGAME B.3.8 VIS) is the real ctor now — see StatusIconComponent::
// StatusIconComponent() at the end of this file. It MUST run at construction:
// retail's load path (ComponentEntity::Serialize 0x1400c3140) rebuilds a
// stream-restored component through the factory ctor and then calls ONLY
// RefreshEntityFlags — vtable +0x20 (Startup) is NEVER dispatched for it. See
// [[reference_ctor_scalars_in_startup_are_unset_after_load]].



// Resolve a component on the owner entity BY NAME (Object::GetTypeIdByName), mirroring
// CharacterComponent's CharComp_FindSiblingByName. This is the standalone-safe equivalent
// of the exe finders 0x140003f60 (ModelComponent) / 0x140004740 (MultiObjectComponent):
// those compare the .rdata type-id globals DWORD_1401c7a50 / c7ac0, which are 0xFFFFFFFF
// (UNFILLED) in standalone, so they return null for EVERY entity. Resolving the type id by
// name at runtime matches the established brick pattern and has no shared-global dependency.
//
// The scan itself is 1:1 with the exe finders (8-byte stride over entity+0x30, compare
// vtable+0x10 GetTypeId, IsDerivedFrom on the first match). The one deviation is
// [STANDALONE GUARD D2]: entries that cannot be a live component are SKIPPED instead of
// dereferenced, so a half-torn-down entity yields "component not found" rather than a
// fault. Retail would have had a fully-consistent array here.
// [StatusIcon] GUARD trace budgets (were function-local statics; B6-5).
static int s_siBadEntryBudget = 8;
static int s_siUninitBudget = 4;

// B.3.8 CB1b: the caller passes the class's TypeId() (a plain read), which replaces
// the per-name first-use cache this helper used to keep (B6-5); `typeName` is only
// for the guard trace.
static void* StatusIcon_FindCompByName(uintptr_t entity, uint32_t want, const char* typeName) {
    if (!entity) return nullptr;
    if (want == 0 || want == 0xFFFFFFFFu) return nullptr;

    int count = 0;
    int64_t arr = 0;
    if (!SI_ComponentArray(entity, &count, &arr)) return nullptr;

    for (int i = 0; i < count; i++) {
        void** ibp = (void**)(arr + (int64_t)i * 8);
        void* comp = *ibp;
        if (!comp) continue;
        // [STANDALONE GUARD D2] the entry must look like a heap pointer before it is
        // dereferenced. This is the read that faulted on 0xa. A line here now means a
        // genuinely bad array entry — see the header note: it must NOT be extended to
        // test the vtable's function pointers.
        if (!SI_Plausible(comp)) {
            if (SI_LogOnce(&s_siBadEntryBudget)) {
                fprintf(stderr, "[StatusIcon] GUARD: entity=%p component[%d/%d]=%p is not a "
                        "plausible pointer (arr=%p) — skipping it during the %s lookup\n",
                        (void*)entity, i, count, comp, (void*)arr, typeName);
                fflush(stderr);
            }
            continue;
        }
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
        if (tid == want)
            return Object::IsDerivedFromNullSafe(comp, want) ? comp : nullptr;
    }
    return nullptr;
}


// ============================================================
// StatusIcon_Toggle — faithful standalone port of FUN_14012c3f0 (0x14012c3f0)
// ============================================================
// The authentic status-icon flag SETTER: OR/AND (1<<bit) into the icon active-bits word
// (+0xF0). If the word becomes 0 → RemoveObject (hide). Otherwise, if the global icon-
// visibility setting (settings+0xc) is on and the icon is not yet in the scene, AddObject
// the embedded geometry (+0x28), then call component vtable+0x40 (OnRemove = positioning),
// and OR the +0x20 dirty bit; on a low-16 change, notify the owner entity (FUN_1400c3470).
// Verified line-by-line against the disassembly. This is the function WorkPlace attach/
// detach (0x14012fa20 / 0x14012f7e0) and SetAccepting (0x14012fc00) drive to show/clear the
// "no occupants / no workers" warning (bit 4 = 0x10). The real exe fn is unmapped in
// standalone, so the occupancy driver below calls this port instead.
void StatusIcon_Toggle(StatusIconComponent* selfC, int bit, bool setFlag) {
    uintptr_t c = (uintptr_t)selfC;
    if (GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gw = (uintptr_t)GameUpdateLoop::_singleton;
    void* scene = *(void**)(gw + 0x60);

    // [STANDALONE GUARD D4] see SI_Initialised: with no StartupInit the geometry has no
    // vtable and +0x100 == 0 is "never registered", so neither scene call below may run.
    const bool inited = SI_Initialised(c);

    // Culled-entity guard: if the owner entity is culled (entity+0x40 bit 10) the original
    // skips the SET path (only clears are honoured while culled).
    uintptr_t entity = *(uintptr_t*)(c + 0x10);
    if (entity && ((*(uint32_t*)(entity + 0x40) >> 10) & 1) != 0 && setFlag) return;

    uint32_t mask = 1u << (bit & 0x1f);
    uint32_t* pFlags = (uint32_t*)(c + 0xF0);
    if (!setFlag) *pFlags &= ~mask; else *pFlags |= mask;

    uint32_t old20 = *(uint32_t*)(c + 0x20);
    uint32_t new20;
    if ((int32_t)*pFlags == 0) {
        // All status bits cleared → remove the icon from the scene.
        int32_t sid = *(int32_t*)(c + 0x100);
        if (!inited) {
            if (sid == 0) *(int32_t*)(c + 0x100) = -1;   // repair the zero-fill sentinel
        } else if (sid >= 0 && scene) {
            reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject((uint32_t)sid);   // B.3.5 bridge
            *(int32_t*)(c + 0x100) = -1;
        }
        new20 = old20 & 0xfffffffbu;
    } else {
        // At least one status bit set → ensure the icon is in the scene (visibility on)
        // and position it via component vtable+0x40 (OnRemove).
        void* settings = g_gameSettings;
        uint8_t vis = settings ? reinterpret_cast<uint8_t*>(settings)[0xc] : 0;
        // Retail guarantees the geometry has its sprite before it can be added to the
        // Scene, because slot 11 (0x14012bf40) re-runs slot 4 on every call and slot 11
        // is retail's only scene-add path. Standalone drives show/hide from THIS function
        // instead (slot 11 has no dispatcher here), so the guarantee has to be restored
        // at the same point: a stream-restored component has never run slot 4, and adding
        // its sprite-less geometry is a silent no-draw. See StatusIcon_EnsureSprite.
        if (inited) StatusIcon_EnsureSprite(c);
        if (inited && vis != 0 && *(int32_t*)(c + 0x100) < 0 && scene) {
            uint32_t id = reinterpret_cast<Graphics::Scene*>(scene)
                ->AddObject(reinterpret_cast<Graphics::Geometry*>(c + 0x28));   // B.3.5 bridge
            *(int32_t*)(c + 0x100) = (int32_t)id;
            SceneWatch_Note(id, (void*)(c + 0x28), "StatusIconComponent::Show");
        }
        {
            uint8_t timeObj[8];
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeObj), 0.0f);
            selfC->Tick(timeObj);   // vtable +0x40 (slot 8), a virtual dispatch in retail
        }
        new20 = old20 | 4u;
    }
    *(uint32_t*)(c + 0x20) = new20;
    if (((new20 ^ old20) & 0xffff) != 0 && *(uintptr_t*)(c + 0x10) != 0) {
        ActHunt_NotifyEntityChanged_c3470(*(void**)(c + 0x10));
    }
}

// NOTE (2026-07-08): the per-frame occupancy poll `StatusIcon_DrainForceShow` and its
// deferred drive-list were removed. The building "no workers" warning (bit 4 / 0x10) is
// now raised by the genuine faithful event — WorkPlaceComponent_SetAccepting
// (workplacecomponent.cpp, port of Entity_impl_fc00) at the lodge-activation point — and
// cleared by WorkplaceComponent_Attach when a worker is seated. Citizen health icons are
// driven by CitizenComponent::Update. The status-icon user setting (settings byte +0xc)
// is already initialised to 1 at settings creation (game_main.cpp: settings[3]=1), so no
// per-frame poke is needed.

// ============================================================================
// ENDGAME B.3.8 VIS — StatusIconComponent as a REAL class (StatusIconComponent.h).
//
// Before: a 48-slot StatusIconComponent_LiveVtable filled per create (slots 7/8
// patched in by CtorScalars), Inert_ScalarDtor in slot 0, the billboard built by
// the Geometry ctor DOOR and re-stamped with a copy of the minted Geometry table.
// After: the compiler's 16-slot vtable, a real ctor/dtor chain (B38_RULES D1,
// D2) and the billboard as a real Graphics::Geometry member (D6).
// ============================================================================

// 0x14012c0c0: Component() is the CALL 0x1401188b0; the 0x140164620 stamp is the
// compiler's; the member's Geometry() is the `??0Geometry@Graphics@@` call and
// its 0x14015f0c0 stamp is the compiler's (StatusIconGeometry); +0x100 = -1
// (sceneObjId "not registered": a 0 there would make Scene::RemoveObject spin);
// flags |= 0xc2.
StatusIconComponent::StatusIconComponent() : _geometry(), iconId(-1), _pad104{} {
    OrFlagsInCtor(0xc2);
}

// ============================================================
// StatusIconComponent::Persist (vtable slot [3], 0x14012bff0)
// ============================================================
// Serializes the fieldF0 icon state. Simple: Component_Serialize base,
// then one uint via IO::Serializer(version 0). On load, writes back to +0xF0.

void StatusIconComponent::Persist(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Base class serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Create IO::Serializer (version 0)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize fieldF0 as uint (copy to local, serialize, write back on load)
    uint32_t iconVal = *(uint32_t*)(s + 0xF0);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(iconVal, nullptr);   // B.3.4 bridge

    // On load: write value back to field
    // Check serializer direction: *(int*)(serializer[0] + 8) == 0 means loading
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        *(uint32_t*)(s + 0xF0) = iconVal;
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// StatusIconComponent_StartupInit — the STARTUP half: the description-driven geometry
// sprite setup the inert factory skips. Routed to the component's Startup slot
// (vtable +0x20 / index 4) so it runs during ComponentEntity::StartupComponents, exactly
// where the original dispatches the real Serialize.
// ⚠ A LOAD never dispatches this slot — anything that must hold for a stream-restored
// component belongs in the ctor (StatusIconComponent::StatusIconComponent), not here.
void StatusIconComponent::Startup() {
    // The ctor half runs at CONSTRUCTION (the real ctor). It used to be re-run here
    // too (Geometry door + the table stamp + sceneObjId = -1 a second time, on a
    // component nothing had touched since construction); a C++ member cannot be
    // re-constructed in place, and retail's slot 4 is the sprite setup alone.

    // Real geometry-sprite setup from the StatusIconDescription's SpriteSheet (guarded
    // — no-ops if desc/spriteSheet absent). The icon's warning bit is now raised/cleared
    // by genuine events (WorkPlaceComponent_SetAccepting / _Attach for building warnings;
    // CitizenComponent Update for citizen health icons) — no per-frame occupancy poll.
    StatusIconComponent_Serialize(this);

}
// [7] TREE DIVERGENCE (retail 0x14012beb0, 15 B): the removed-from-world handler
// above (StatusIconComponent::OnRemove) — MarkForRemoval (0x1400c2ea0) dispatches
// it for capability bit 1 (0xc2 has it); it removes the icon's scene object so
// the renderer never holds freed geometry.
// ============================================================
// Standalone faithful port of StatusIconComponent::OnRemove (0x14012bbd0)
// ============================================================
// Manages Scene add/remove on the game-settings icon-visibility byte (settings+0xc),
// then positions the icon. Buildings take the MODEL-BOUNDS path (icon centred above
// the model's bounding box); the map-cell fallback mirrors the original for entities
// without a model. Verified line-by-line against the disassembly.
void StatusIconComponent::Tick(void* /*time*/) {
    uintptr_t c = reinterpret_cast<uintptr_t>(this);

    // [STANDALONE GUARD D4] never-initialised component: the inert factory's zero-fill left
    // the geometry vtable (+0x28) null, so +0x100 == 0 is "not registered", not scene
    // object 0. Touching the scene from here would either add a null-vtable geometry to
    // the draw list or RemoveObject an id that was never handed out (which spins forever).
    if (!SI_Initialised(c)) {
        if (SI_LogOnce(&s_siUninitBudget)) {
            fprintf(stderr, "[StatusIcon] GUARD: comp=%p never ran StartupInit "
                    "(geom vtable +0x28 == 0, sceneId=%d) — icon refresh skipped\n",
                    static_cast<void*>(this), *(int32_t*)(c + 0x100));
            fflush(stderr);
        }
        return;
    }

    void* settings = g_gameSettings;
    if (!settings || GameUpdateLoop::_singleton == nullptr) return;
    uintptr_t gw = (uintptr_t)GameUpdateLoop::_singleton;
    void* scene = *(void**)(gw + 0x60);
    uint8_t iconVisible = reinterpret_cast<uint8_t*>(settings)[0xc];

    int32_t sceneId = *(int32_t*)(c + 0x100);
    if (iconVisible == 0) {
        if (sceneId >= 0 && scene) {
            reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject((uint32_t)sceneId);   // B.3.5 bridge
            *(int32_t*)(c + 0x100) = -1;
        }
    } else if (sceneId < 0) {
        if (scene) {
            uint32_t id = reinterpret_cast<Graphics::Scene*>(scene)
                ->AddObject(reinterpret_cast<Graphics::Geometry*>(c + 0x28));   // B.3.5 bridge
            *(int32_t*)(c + 0x100) = (int32_t)id;
            SceneWatch_Note(id, (void*)(c + 0x28), "StatusIconComponent::Update");
        }
    }
    if (*(int32_t*)(c + 0x100) < 0) return;

    // Icon scale (settings+8 / 100) cached at +0xa4/+0xa8; flag bit 2 on change.
    float scaleVal = (float)(*(int32_t*)((uint8_t*)settings + 8)) / 100.0f;
    if (scaleVal != *(float*)(c + 0xa4)) {
        *(uint32_t*)(c + 0x54) |= 4u;
        *(float*)(c + 0xa4) = scaleVal;
        *(float*)(c + 0xa8) = scaleVal;
    }

    void* descObj = *(void**)(c + 0x18);
    void* desc = (Object::IsDerivedFromNullSafe(descObj, StatusIconDescription::TypeId())) ? descObj : nullptr;

    float pos[3];
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = desc ? *(float*)((uintptr_t)desc + 0x2c) : 0.0f;   // base height offset

    uintptr_t entity = *(uintptr_t*)(c + 0x10);

    // Resolve Model + MultiObject by NAME (the exe finders key off unfilled .rdata globals
    // c7a50/c7ac0 → null for every entity in standalone; see StatusIcon_FindCompByName).
    // The MODEL-BOUNDS path then resolves for citizens (model = CharacterComponent's sibling
    // ModelComponent, +0x158 = live move-tick position) AND faithfully preserves the original's
    // branch for buildings that carry a MultiObjectComponent (→ map-cell). Map fallback below
    // still uses the exe finder (its c79e0 global IS filled, so it works).
    // [PerfX ICON_LASTONLY -- NON-RETAIL] the positioning below is pure presentation
    // (Geometry::SetTransform on the icon sprite); on intermediate speed iterations the
    // result is overwritten before anything draws it, so only the last iteration runs it.
    if (PerfX::IconLastOnly() && PerfX::RootOnlyNow()) {
        PerfX::Count(PerfX::kCntIconSkipped);
        return;
    }
    void* modelIBP[2] = { nullptr, nullptr };
    void* multiIBP[2] = { nullptr, nullptr };
    modelIBP[0] = StatusIcon_FindCompByName(entity, ModelComponent::TypeId(), "ModelComponent");
    // [PerfX ICON_CULL -- NON-RETAIL] an icon whose owner model is off-screen is not
    // repositioned; the tick is replayed (root-only flag clear) if the sim pauses.
    if (modelIBP[0] && PerfX::IconCull() && PerfX::ModelCulled(modelIBP[0])) {
        PerfX::Count(PerfX::kCntIconCulled);
        PerfX::NoteDeferred(this, [](void* o) {
            reinterpret_cast<StatusIconComponent*>(o)->StatusIconComponent::Tick(nullptr);   // B.3.8 bridge
        });
        return;
    }
    multiIBP[0] = StatusIcon_FindCompByName(entity, MultiObjectComponent::TypeId(), "MultiObjectComponent");

    if (multiIBP[0] == nullptr && modelIBP[0] != nullptr) {
        // MODEL-BOUNDS path: Box at model+0x5c = {min(3), max(3)}; size.z = max.z-min.z.
        uintptr_t model = (uintptr_t)modelIBP[0];
        float sizeZ = *(float*)(model + 0x5c + 0x14) - *(float*)(model + 0x5c + 0x08);
        pos[0] = *(float*)(model + 0x158);
        pos[1] = *(float*)(model + 0x15c);
        pos[2] = *(float*)(model + 0x160) + sizeZ;
    } else {
        // MAP-CELL fallback.
        void* mapIBP[2] = { nullptr, nullptr };
        FloorComponent_FindResData(reinterpret_cast<void*>(entity), mapIBP);   // 0x140004680
        if (mapIBP[0]) {
            uintptr_t mc = (uintptr_t)mapIBP[0];
            int* widthPtr = *(int**)(gw + 0xf8);
            int w = *widthPtr;
            int idx = ((int)*(int16_t*)(mc + 0x2a) + ((int)*(int16_t*)(mc + 0x2e) >> 1)) * w
                      + ((int)*(int16_t*)(mc + 0x2c) >> 1) + (int)*(int16_t*)(mc + 0x28);
            float f = (float)(w >> 1) - 0.5f;
            float in[3]  = { (float)(idx % w) - f, (float)(idx / w) - f, 0.0f };
            float out[3] = { 0.0f, 0.0f, 0.0f };
            Reimpl_MapGetTerrainPos(widthPtr, out, in, 0, nullptr);   // 0x14009f3e0
            pos[0] += out[0];
            pos[1] += out[1];
            pos[2] += out[2];
        }
    }

    // Transform (0x1C bytes): {0,0,0, 1.0(cos), pos.x, pos.y, pos.z} — pure translation.
    float xf[7] = { 0.0f, 0.0f, 0.0f, 1.0f, pos[0], pos[1], pos[2] };
    reinterpret_cast<Graphics::Geometry*>(c + 0x28)
        ->SetTransform(*reinterpret_cast<const Math::Transform*>(xf));   // B.3.5 bridge
}
// [11] 0x14012bf40. PORT GAP (retail 0x14012bf40) — B38 D9: the body is ported
// above (StatusIconComponent_OnUpdate2) but the tree's table never installed it;
// the inert stub is kept here.
void StatusIconComponent::Slot11() {}

// create 0x1400388c0: MemAlloc(0x108, 8), then the ctor.
void* StatusIconComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StatusIconComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StatusIconComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* StatusIconComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StatusIconComponent());
}

DEFINE_EXE_TYPE(StatusIconComponent, "Component")
