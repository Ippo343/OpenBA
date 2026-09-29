// code/Game/Component/statusiconcomponent.cpp
// Clean reimplementation of StatusIconComponent lifecycle methods from Application-x64.exe.
//
// StatusIconComponent (0x108 bytes) displays floating status icons above entities
// (e.g., hunger, cold, disease). Embeds a Graphics::Geometry sub-object at +0x28
// (0xC0 bytes, with overridden vtable at 0x14015f0c0).
//
// Vtable at 0x140164620. Non-trivial overrides:
//   Persist  (0x14012bff0) — Serializes the billboard's activeMask (+0xF0 as uint)
//   Startup  (0x14012bed0) — Reads sprite data from building description
//   Tick     (0x14012bbd0) — Manages Scene add/remove, positions icon (very complex)
//   Slot11   (0x14012bf40) — Updates icon visibility, adds to Scene (ShowAfterLoad; PORT GAP)
// and the non-virtual SetFlag (0x14012c3f0).
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
#include "Component/MapComponent.h"
#include "Entity/ComponentEntity.h"
#include "scene_watch.h"
#include "math/Math.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/exe_bodies.h"   // B3R STORAGE: g_gameSettings
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/Map.h"              // Map::size
#include "perf_experimental.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K4)
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"   // ActHunt_NotifyEntityChanged_c3470

// Exe helpers, by retail address: 0x1400aa440 StatusIconGeometry::Setup (below),
// 0x1400c3470 ActHunt_NotifyEntityChanged_c3470 (entity notification), 0x140152b00
// ??0Time@System@@QEAA@M@Z (RtSystem_Time_Ctor), 0x140004680 FindComponent<MapComponent>,
// 0x14009f3e0 Reimpl_MapGetTerrainPos (grid cell -> world position).

// A StatusIconComponent that never ran StatusIconComponent_StartupInit still carries the
// inert factory's zero-fill: the embedded geometry's vtable (+0x28) is 0 AND the scene
// object id (+0x100) is 0. That 0 is "never initialised", NOT scene object 0 � feeding it
// to Graphics::Scene::RemoveObject spins forever (in-world freeze, see
// reference_inert_component_sentinel_fields), and AddObject-ing a geometry whose vtable is
// null faults in the renderer's draw pass. The geometry vtable is the in-band marker: the
// real ctor and StartupInit both always write it, and nothing ever clears it.
// [STANDALONE GUARD D4] (a guard of the tree's; retail has none).
// LEFT: the test reads the billboard's vptr (a guard the real ctor makes always-true;
// removing it is NEXT_SESSION �0.2's).
static inline bool SI_Initialised(const StatusIconComponent* c) {
    return *reinterpret_cast<void* const*>(&c->_geometry) != nullptr;
}

// The graphics scene the icons register with: GameUpdateLoop +0x60.
static Graphics::Scene* StatusIcon_Scene() {
    return GameUpdateLoop::_singleton->m_graphicsScene;
}

// ============================================================
// StatusIconGeometry::Setup (0x1400aa440): the geometry-bounds setup
// ============================================================
// Stores the sprite parameters into the billboard and computes its axis-aligned
// bounds. half = size * 0.5; bounds box is min(-half,-half,0) .. max(half,half,
// count*size). 1:1 port; the only callee is Graphics::Geometry::SetBounds.
void StatusIconGeometry::Setup(float size, int count, uint8_t constant, UI::SpriteSheet* sheet) {
    float half = size * 0.5f;

    iconSize = size;
    _materialTypeFlags = 2;
    spriteCount = count;
    spriteSheet = sheet;
    activeMask = 0;
    constantSize = constant;   // the original stores the low byte of its fourth argument

    ::Box box;
    box.min.x = -half;
    box.min.y = -half;
    box.min.z = 0.0f;
    box.max.x = half;
    box.max.y = half;
    box.max.z = (float)count * size;

    SetBounds(box);
}

// ============================================================
// Helper: the StatusIconDescription (the component's +0x18, IsDerivedFrom checked)
// ============================================================
static StatusIconDescription* GetStatusIconDescription(const StatusIconComponent* self) {
    Description* obj = self->_description;
    if (!obj) return nullptr;

    bool derived = Object::IsDerivedFromNullSafe(obj, StatusIconDescription::TypeId());
    return derived ? static_cast<StatusIconDescription*>(obj) : nullptr;
}


// ============================================================
// StatusIcon_EnsureSprite — retail's lazy sprite setup (component vtable slot 4)
// ============================================================
// Slot 4 (0x14012bed0, StatusIconComponent::Startup) is the ONLY thing that
// ever writes the embedded Geometry's sprite/material from the StatusIconDescription
// — StatusIconGeometry::Setup has exactly one caller in the whole exe.
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
// ⚠ CALL THE BODY NON-VIRTUALLY, NEVER DISPATCH VTABLE SLOT 4 HERE.
// Retail's slot 4 is exactly the sprite setup and nothing else. Until ENDGAME B.3.8
// VIS OUR slot 4 ALSO re-ran the construction half (the Geometry ctor door on +0x28
// and the scene-object id at +0x100 = -1), so dispatching it mid-life reset a live
// component. The real class's slot 4 no longer does, but the rule stands: the call
// is qualified.
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
static void StatusIcon_EnsureSprite(StatusIconComponent* c) {
    uint32_t saved = c->_geometry.activeMask;
    c->StatusIconComponent::Startup();
    c->_geometry.activeMask = saved;
}

// ============================================================
// StatusIconComponent::ShowAfterLoad — the body of vtable slot [11] (0x14012bf40)
// ============================================================
// Updates icon visibility. Re-runs the sprite setup (slot 4), restoring the +0xF0
// mask it clobbers. If the icon is not in the scene and bits are set: adds the
// geometry to the Scene, sets flag bit 2, notifies the owner entity, then calls
// vtable +0x40 (Tick) with Time(0) to position it.
// PORT GAP (retail 0x14012bf40): Slot11 does not call this yet (see the header).

void StatusIconComponent::ShowAfterLoad() {
    // Save the mask (+0xF0)
    uint32_t savedMask = _geometry.activeMask;

    // Retail is
    //     lVar2 = param_1[0x1e];                    // +0xF0
    //     (**(code **)(*param_1 + 0x20))();         // THIS component's vtable, slot 4
    //     *(int *)(param_1 + 0x1e) = (int)lVar2;    // slot 4 clobbers +0xF0 -> restore
    // `*param_1` is the COMPONENT's own vtable pointer (offset 0), not the embedded
    // Geometry's at +0x28 — this copy read +0x28 and called the geometry's slot 4
    // instead, which is why nothing ever re-ran the sprite setup.
    StatusIcon_EnsureSprite(this);

    // Restore the mask to the saved value (original: *(int*)(param_1 + 0x1e) cast)
    _geometry.activeMask = savedMask;

    // If iconId < 0 and the mask != 0: add to scene and notify
    if (iconId < 0 && savedMask != 0) {
        // Add geometry to scene
        if (GameUpdateLoop::_singleton) {
            Graphics::Scene* scene = StatusIcon_Scene();
            if (scene) {
                uint32_t newId = scene->AddObject(&_geometry);
                SceneWatch_Note(newId, &_geometry, "StatusIconComponent::OnAdded");

                // Set flags: OR bit 2 (0x04) into flags at +0x20
                uint32_t oldFlags = _flags;
                iconId = static_cast<int32_t>(newId);
                uint32_t newFlags = oldFlags | 0x04u;
                _flags = newFlags;

                // If flags changed (low 16 bits) and owner non-null, notify entity
                if (((newFlags ^ oldFlags) & 0xFFFF) != 0) {
                    if (_entity != nullptr) {
                        ActHunt_NotifyEntityChanged_c3470(_entity);
                    }
                }

                // Call vtable +0x40 (Tick) with Time(0) to position the icon
                {
                    System::Time timeObj;
                    RtSystem_Time_Ctor(&timeObj, 0.0f);
                    Tick(&timeObj);   // vtable +0x40 (slot 8), a virtual dispatch in retail
                }
            }
        }
    }
}

// ============================================================
// StatusIconComponent::SetFlag (0x14012c3f0; the tree's CitizenComp_FUN_14012c3f0,
// moved from citizencomponent.cpp)
// ============================================================
// Set/clear bit `flagIndex` in the status mask (+0xF0); when the mask becomes
// empty/non-empty, add/remove the geometry from the scene and toggle the +0x100
// scene id and the +0x20 flag bit 4, then notify the owning entity.
void StatusIconComponent::SetFlag(uint8_t flagIndex, char value) {
    if ((((_entity->_entityFlags >> 10) & 1) == 0) || (value == '\0')) {
        uint32_t bit = 1u << (flagIndex & 0x1f);
        if (value == '\0') {
            _geometry.activeMask &= ~bit;
        } else {
            _geometry.activeMask |= bit;
        }
        uint32_t changed;
        // [STANDALONE GUARD D4] geometry (+0x28) vtable installed?
        // See the long note in the clear-branch below: both scene calls depend on it.
        const bool inited = SI_Initialised(this);

        // [diag] BANISHED_SI_CENSUS - report BOTH arms. "No bad scene nodes" is only a
        // fix if the icons still REGISTER; if `inited` were false the guard would suppress
        // every AddObject and the SceneWatch silence would mean the feature is off, not
        // healthy. So count uninitialised components separately from suppressed adds.
        // See [[reference_diagnostics_must_report_outcomes]].
        if (static_cast<int32_t>(_geometry.activeMask) == 0) {
            // [STANDALONE GUARD] A StatusIconComponent that never ran StartupInit still
            // carries the inert factory's zero-fill: the embedded geometry's vtable
            // (+0x28) is 0 and the scene id (+0x100) is 0. That 0 means "never
            // registered", NOT scene object 0 — and Scene::RemoveObject(scene, 0)
            // removes whichever object genuinely owns id 0, after which the engine's list
            // walk spins: an in-world FREEZE, not a crash (the failure mode fixed at source
            // for HighlightComponent in 0f97d16). Repair the sentinel instead of removing.
            // Reachable on the default path since the pause port graduated: SetAccepting's
            // accepting=false branch CLEARS icon flags 2/3/4, and clearing the last flag on
            // an uninitialised icon lands exactly here.
            //
            // WARNING: the old note here claimed "the AddObject side below needs no guard -
            // an uninitialised +0x100 == 0 fails its `< 0` test". That was FALSE, and it was
            // falsified BY THE LINE BELOW: repairing the sentinel to -1 is exactly what makes
            // `+0x100 < 0` true, so the next time a flag is SET this function registered a
            // geometry whose vtable is still 0 into the Graphics::Scene.
            // `[SceneWatch] BAD=1 ... vtable=0000000000000000` on a 21-minute load soak
            // (2026-08-29) is that node, and it is the `EXEC of <heap>` in Scene::Clean.
            // The upstream fix is the real ctor (StatusIconComponent::StatusIconComponent),
            // which builds the geometry at CONSTRUCTION where retail's ctor 0x14012c0c0
            // has it, so `inited` below is true for loaded icons too. The gate stays as the
            // same [STANDALONE GUARD D4] its twin StatusIcon_Toggle carries.
            //
            // WARNING: THIS FUNCTION IS PORTED TWICE - see
            // [[reference_one_retail_fn_ported_twice_diverges]]. The other copy is
            // StatusIcon_Toggle (below), which HAD the guard; THIS copy is the one the
            // building-side callers reach for 0x14012c3f0. Change both or neither.
            if (!inited) {
                if (iconId == 0)
                    iconId = -1;
            } else if (-1 < iconId) {
                StatusIcon_Scene()->RemoveObject(static_cast<uint32_t>(iconId));
                iconId = -1;
            }
            uint32_t before = _flags;
            uint32_t after = before & 0xfffffffb;
            _flags = after;
            changed = after ^ before;
        } else {
            GameSettings* settings = g_gameSettings;
            if (inited && settings && settings->statusIconEnable != '\0' && iconId < 0) {
                uint32_t id = StatusIcon_Scene()->AddObject(&_geometry);
                iconId = static_cast<int32_t>(id);
                SceneWatch_Note(id, &_geometry, "StatusIcon(12c3f0)::Add");
            }
            System::Time timeVal{0};
            RtSystem_Time_Ctor(&timeVal, 0.0f);
            Tick(&timeVal);   // vtable +0x40 (slot 8), a virtual dispatch in retail
            uint32_t before = _flags;
            uint32_t after = before | 4;
            _flags = after;
            changed = after ^ before;
        }
        if ((changed & 0xffff) != 0 && _entity != nullptr) {
            ActHunt_NotifyEntityChanged_c3470(_entity);
        }
    }
}

// B4 FORWARD: outside callers in 16 files (Component: attendancecomponent*, buildcomponent_state8,
// citizenai_gather, citizen_needsupdate, destroycomponent_states, workcomponent,
// workplacecomponent; Work: consumeproduce, fishingwork, foresterwork, harvestarea,
// harvestorchard, herbalistwork, huntwork, pasturework)
void CitizenComp_FUN_14012c3f0(int64_t* param_1, uint8_t flagIndex, char value) {
    reinterpret_cast<StatusIconComponent*>(param_1)->SetFlag(flagIndex, value);
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

// Graphics::Geometry::SetTransform (Runtime export) — used by Tick positioning.

// Entity component finders: 0x140003f60 ModelComponent (type 0x1401c7a50) and
// 0x140004740 MultiObjectComponent (type 0x1401c7ac0) are replaced by the by-name
// lookup below; 0x140004680 -> MapComponent (type 0x1401c79e0).
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
    uintptr_t v = reinterpret_cast<uintptr_t>(p);  // CAST-OK: ptr_int
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
static bool SI_ComponentArray(const ComponentEntity* entity, int* outCount,
                              Object::InternalPtr<Component>** outArr) {
    if (!SI_Plausible(entity)) return false;
    int count = entity->_componentCount;
    Object::InternalPtr<Component>* arr = entity->_components;
    if (count < 0 || count > 4096) return false;
    if (count > 0 && !SI_Plausible(arr)) return false;
    if (outCount) *outCount = count;
    if (outArr) *outArr = arr;
    return true;
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
    if (GameUpdateLoop::_singleton == nullptr) return;
    Graphics::Scene* scene = StatusIcon_Scene();
    int32_t sceneId = iconId;
    // [STANDALONE GUARD D4] a component that never ran StartupInit still has the inert
    // zero-fill, so sceneId 0 means "never registered" rather than scene object 0 —
    // RemoveObject on it spins forever (in-world freeze). Repair the sentinel instead.
    if (!SI_Initialised(this)) {
        if (sceneId == 0) iconId = -1;
        return;
    }
    if (sceneId >= 0 && scene) {
        scene->RemoveObject((uint32_t)sceneId);
        iconId = -1;
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
static Component* StatusIcon_FindCompByName(const ComponentEntity* entity, uint32_t want, const char* typeName) {
    if (!entity) return nullptr;
    if (want == 0 || want == 0xFFFFFFFFu) return nullptr;

    int count = 0;
    Object::InternalPtr<Component>* arr = nullptr;
    if (!SI_ComponentArray(entity, &count, &arr)) return nullptr;

    for (int i = 0; i < count; i++) {
        Component* comp = arr[i].Get();
        if (!comp) continue;
        // [STANDALONE GUARD D2] the entry must look like a heap pointer before it is
        // dereferenced. This is the read that faulted on 0xa. A line here now means a
        // genuinely bad array entry — see the header note: it must NOT be extended to
        // test the vtable's function pointers.
        if (!SI_Plausible(comp)) {
            if (SI_LogOnce(&s_siBadEntryBudget)) {
                fprintf(stderr, "[StatusIcon] GUARD: entity=%p component[%d/%d]=%p is not a "
                        "plausible pointer (arr=%p) — skipping it during the %s lookup\n",
                        static_cast<const void*>(entity), i, count, static_cast<void*>(comp),
                        static_cast<void*>(arr), typeName);
                fflush(stderr);
            }
            continue;
        }
        uint32_t tid = comp->Type();
        if (tid == want)
            return Object::IsDerivedFromNullSafe(comp, want) ? comp : nullptr;
    }
    return nullptr;
}


// ============================================================
// StatusIcon_Toggle — the second port of FUN_14012c3f0 (0x14012c3f0)
// ============================================================
// The authentic status-icon flag SETTER: OR/AND (1<<bit) into the icon active-bits word
// (+0xF0). If the word becomes 0 → RemoveObject (hide). Otherwise, if the global icon-
// visibility setting (settings+0xc) is on and the icon is not yet in the scene, AddObject
// the embedded geometry (+0x28), then call component vtable+0x40 (Tick = positioning),
// and OR the +0x20 dirty bit; on a low-16 change, notify the owner entity (FUN_1400c3470).
// Verified line-by-line against the disassembly. The citizen's own icons
// (CitizenComponent's CitizenStatusIcon_SetFlag) reach this copy; the building-side
// callers reach StatusIconComponent::SetFlag. It DIFFERS from SetFlag (B3): it re-runs
// the sprite setup before the scene test (StatusIcon_EnsureSprite) and skips the scene
// calls on an uninitialised icon. LEFT free: unifying the two is a `behaviour:` commit.
void StatusIcon_Toggle(StatusIconComponent* selfC, int bit, bool setFlag) {
    if (GameUpdateLoop::_singleton == nullptr) return;
    Graphics::Scene* scene = StatusIcon_Scene();

    // [STANDALONE GUARD D4] see SI_Initialised: with no StartupInit the geometry has no
    // vtable and +0x100 == 0 is "never registered", so neither scene call below may run.
    const bool inited = SI_Initialised(selfC);

    // Culled-entity guard: if the owner entity is culled (entity+0x40 bit 10) the original
    // skips the SET path (only clears are honoured while culled).
    ComponentEntity* entity = selfC->_entity;
    if (entity && ((entity->_entityFlags >> 10) & 1) != 0 && setFlag) return;

    uint32_t mask = 1u << (bit & 0x1f);
    uint32_t* pFlags = &selfC->_geometry.activeMask;
    if (!setFlag) *pFlags &= ~mask; else *pFlags |= mask;

    uint32_t old20 = selfC->_flags;
    uint32_t new20;
    if ((int32_t)*pFlags == 0) {
        // All status bits cleared → remove the icon from the scene.
        int32_t sid = selfC->iconId;
        if (!inited) {
            if (sid == 0) selfC->iconId = -1;   // repair the zero-fill sentinel
        } else if (sid >= 0 && scene) {
            scene->RemoveObject((uint32_t)sid);
            selfC->iconId = -1;
        }
        new20 = old20 & 0xfffffffbu;
    } else {
        // At least one status bit set → ensure the icon is in the scene (visibility on)
        // and position it via component vtable+0x40 (Tick).
        GameSettings* settings = g_gameSettings;
        uint8_t vis = settings ? settings->statusIconEnable : 0;
        // Retail guarantees the geometry has its sprite before it can be added to the
        // Scene, because slot 11 (0x14012bf40) re-runs slot 4 on every call and slot 11
        // is retail's only scene-add path. Standalone drives show/hide from THIS function
        // instead (slot 11 has no dispatcher here), so the guarantee has to be restored
        // at the same point: a stream-restored component has never run slot 4, and adding
        // its sprite-less geometry is a silent no-draw. See StatusIcon_EnsureSprite.
        if (inited) StatusIcon_EnsureSprite(selfC);
        if (inited && vis != 0 && selfC->iconId < 0 && scene) {
            uint32_t id = scene->AddObject(&selfC->_geometry);
            selfC->iconId = (int32_t)id;
            SceneWatch_Note(id, &selfC->_geometry, "StatusIconComponent::Show");
        }
        {
            System::Time timeObj;
            RtSystem_Time_Ctor(&timeObj, 0.0f);
            selfC->Tick(&timeObj);   // vtable +0x40 (slot 8), a virtual dispatch in retail
        }
        new20 = old20 | 4u;
    }
    selfC->_flags = new20;
    if (((new20 ^ old20) & 0xffff) != 0 && selfC->_entity != nullptr) {
        ActHunt_NotifyEntityChanged_c3470(selfC->_entity);
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
// Serializes the activeMask icon state. Simple: Component_Serialize base,
// then one uint via IO::Serializer(version 0). On load, writes back to +0xF0.

void StatusIconComponent::Persist(void* stream) {
    // Base class serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (Persist takes void*)

    // Serialize the mask as uint (copy to local, serialize, write back on load)
    uint32_t iconVal = _geometry.activeMask;
    ser.Serialize(iconVal, nullptr);

    // On load (direction 0): write the value back to the field
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        _geometry.activeMask = iconVal;
    }
}

// ============================================================
// StatusIconComponent::Startup (vtable slot [4], 0x14012bed0; the tree's
// StatusIconComponent_Serialize)
// ============================================================
// Reads sprite data from the StatusIconDescription, initializes the geometry:
// the SpriteSheet's sprite count, then Setup (0x1400aa440) with the icon size,
// the count, the constant-size flag and the sheet.
// ⚠ A LOAD never dispatches this slot — anything that must hold for a stream-restored
// component belongs in the ctor (StatusIconComponent::StatusIconComponent), not here.
// The icon's warning bits are raised/cleared by genuine events
// (WorkPlaceComponent_SetAccepting / _Attach for building warnings; CitizenComponent
// Update for citizen health icons) — no per-frame occupancy poll.
void StatusIconComponent::Startup() {
    StatusIconDescription* desc = GetStatusIconDescription(this);
    if (!desc) return;

    // The sprite count of the SpriteSheet at description +0x20
    UI::SpriteSheet* spriteSheet = desc->_spriteSheet;
    if (!spriteSheet) return;

    int spriteCount = spriteSheet->GetSpriteCount();

    // Setup(size = desc +0x28, count, constant = desc +0x30's low byte, sheet = desc +0x20)
    _geometry.Setup(desc->_scale, spriteCount, static_cast<uint8_t>(desc->_flag), spriteSheet);
}
// [7] TREE DIVERGENCE (retail 0x14012beb0, 15 B): the removed-from-world handler
// above (StatusIconComponent::OnRemove) — MarkForRemoval (0x1400c2ea0) dispatches
// it for capability bit 1 (0xc2 has it); it removes the icon's scene object so
// the renderer never holds freed geometry.
// ============================================================
// Standalone faithful port of StatusIconComponent::Tick (0x14012bbd0)
// ============================================================
// Manages Scene add/remove on the game-settings icon-visibility byte (settings+0xc),
// then positions the icon. Buildings take the MODEL-BOUNDS path (icon centred above
// the model's bounding box); the map-cell fallback mirrors the original for entities
// without a model. Verified line-by-line against the disassembly.
void StatusIconComponent::Tick(void* /*time*/) {
    // [STANDALONE GUARD D4] never-initialised component: the inert factory's zero-fill left
    // the geometry vtable (+0x28) null, so +0x100 == 0 is "not registered", not scene
    // object 0. Touching the scene from here would either add a null-vtable geometry to
    // the draw list or RemoveObject an id that was never handed out (which spins forever).
    if (!SI_Initialised(this)) {
        if (SI_LogOnce(&s_siUninitBudget)) {
            fprintf(stderr, "[StatusIcon] GUARD: comp=%p never ran StartupInit "
                    "(geom vtable +0x28 == 0, sceneId=%d) — icon refresh skipped\n",
                    static_cast<void*>(this), iconId);
            fflush(stderr);
        }
        return;
    }

    GameSettings* settings = g_gameSettings;
    if (!settings || GameUpdateLoop::_singleton == nullptr) return;
    Graphics::Scene* scene = StatusIcon_Scene();
    uint8_t iconVisible = settings->statusIconEnable;

    int32_t sceneId = iconId;
    if (iconVisible == 0) {
        if (sceneId >= 0 && scene) {
            scene->RemoveObject((uint32_t)sceneId);
            iconId = -1;
        }
    } else if (sceneId < 0) {
        if (scene) {
            uint32_t id = scene->AddObject(&_geometry);
            iconId = (int32_t)id;
            SceneWatch_Note(id, &_geometry, "StatusIconComponent::Update");
        }
    }
    if (iconId < 0) return;

    // Icon opacity (settings+8 / 100) cached in the geometry's _opacity/_opacityPrev
    // (+0xa4/+0xa8); dirty bit 2 on change.
    float scaleVal = (float)(settings->statusIconOpacity) / 100.0f;
    if (scaleVal != _geometry._opacity) {
        _geometry._dirtyFlags |= 4u;
        _geometry._opacity = scaleVal;
        _geometry._opacityPrev = scaleVal;
    }

    StatusIconDescription* desc = GetStatusIconDescription(this);

    float pos[3];
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = desc ? desc->_scale2 : 0.0f;   // base height offset

    ComponentEntity* entity = _entity;

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
    Component* model = StatusIcon_FindCompByName(entity, ModelComponent::TypeId(), "ModelComponent");
    // [PerfX ICON_CULL -- NON-RETAIL] an icon whose owner model is off-screen is not
    // repositioned; the tick is replayed (root-only flag clear) if the sim pauses.
    if (model && PerfX::IconCull() && PerfX::ModelCulled(model)) {
        PerfX::Count(PerfX::kCntIconCulled);
        PerfX::NoteDeferred(this, [](void* o) {
            reinterpret_cast<StatusIconComponent*>(o)->StatusIconComponent::Tick(nullptr);   // B.3.8 bridge
        });
        return;
    }
    Component* multi = StatusIcon_FindCompByName(entity, MultiObjectComponent::TypeId(), "MultiObjectComponent");

    if (multi == nullptr && model != nullptr) {
        // MODEL-BOUNDS path: the mesh's local bounds (model+0x5c = meshInstance+0x34);
        // size.z = max.z - min.z, on top of the model's translation.
        const ModelComponent* mdl = static_cast<const ModelComponent*>(model);
        const ::Box& bounds = mdl->meshInstance._localBounds;
        float sizeZ = bounds.max.z - bounds.min.z;
        pos[0] = mdl->transform.translation.x;
        pos[1] = mdl->transform.translation.y;
        pos[2] = mdl->transform.translation.z + sizeZ;
    } else {
        // MAP-CELL fallback.
        MapComponent* mc = entity->FindComponent<MapComponent>();   // 0x140004680
        if (mc) {
            Map* widthPtr = GameUpdateLoop::_singleton->m_game;
            int w = widthPtr->size;
            // The footprint is read as int16 (the tree's width; MapComponent.h declares
            // uint16 -- COMP-d's header, LISTED).
            int idx = ((int)static_cast<int16_t>(mc->tileY0) + ((int)static_cast<int16_t>(mc->tileY1) >> 1)) * w
                      + ((int)static_cast<int16_t>(mc->tileX1) >> 1) + (int)static_cast<int16_t>(mc->tileX0);
            float f = (float)(w >> 1) - 0.5f;
            float in[3]  = { (float)(idx % w) - f, (float)(idx / w) - f, 0.0f };
            float out[3] = { 0.0f, 0.0f, 0.0f };
            Reimpl_MapGetTerrainPos(widthPtr, out, in, 0, nullptr);   // 0x14009f3e0
            pos[0] += out[0];
            pos[1] += out[1];
            pos[2] += out[2];
        }
    }

    // Transform: {rotation (0,0,0,1), translation pos} — pure translation.
    Math::Transform xf;
    xf.translation = Math::Vector3(pos[0], pos[1], pos[2]);
    _geometry.SetTransform(xf);
}
// [11] 0x14012bf40. PORT GAP (retail 0x14012bf40) — B38 D9: the body is ported
// above (ShowAfterLoad) but the tree's table never installed it; the inert stub
// is kept here.
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
