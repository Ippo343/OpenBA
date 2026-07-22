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

#include "Component/statusiconcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// IO::Serializer::Serialize(uint*) — dispatch slot at 0x140152a50
typedef void (*fn_Serialize_UInt_t)(void* serializer, uint32_t* field, const char* name);
static fn_Serialize_UInt_t s_fn_Serialize_UInt = nullptr;

// UI::SpriteSheet::GetSpriteCount — dispatch slot at 0x1401529b0
typedef int (*fn_SpriteSheet_GetSpriteCount_t)(void* spriteSheet);
static fn_SpriteSheet_GetSpriteCount_t s_fn_SpriteSheet_GetSpriteCount = nullptr;

// StatusIconComponent_helper_a440 (0x1400aa440) — geometry setup helper
typedef void (*fn_StatusIconHelper_a440_t)(void* geometry, float param2, int spriteCount,
                                           void* param4, void* spriteSheet);
static fn_StatusIconHelper_a440_t s_fn_StatusIconHelper_a440 = nullptr;

// Graphics::Geometry::SetBounds(this, Box const&) — Runtime export, IAT slot 0x1401525e8.
typedef void (*fn_Geometry_SetBounds_t)(void* geometry, const void* box);
static fn_Geometry_SetBounds_t s_fn_Geometry_SetBounds = nullptr;

// Graphics::Scene::AddObject — dispatch slot at 0x140151370
typedef uint32_t (*fn_Scene_AddObject_t)(void* scene, void* geometry);
static fn_Scene_AddObject_t s_fn_Scene_AddObject = nullptr;

// Graphics::Scene::RemoveObject — dispatch slot at 0x140151360
typedef void (*fn_Scene_RemoveObject_t)(void* scene, uint32_t objectId);
static fn_Scene_RemoveObject_t s_fn_Scene_RemoveObject = nullptr;

// System::Time::Time(Time*, float) — dispatch slot at 0x140152b00
typedef void (*fn_Time_Ctor_t)(void* self, float seconds);
static fn_Time_Ctor_t s_fn_Time_Ctor = nullptr;

// FUN_1400c3470 — entity notification callback
typedef void (*fn_EntityNotify_t)(void* entity);
static fn_EntityNotify_t s_fn_EntityNotify = nullptr;

// StatusIconDescription type ID at 0x1401c7e40
static uint32_t* s_TypeId_StatusIconDesc = nullptr;

// Original function pointers for complex delegation
typedef void (*fn_OnRemove_Original_t)(void*);
static fn_OnRemove_Original_t s_fn_OnRemove_Original = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    // Dispatch slots
    slot = ResolveAddr(Module::Exe, 0x140152a50);
    if (slot) s_fn_Serialize_UInt = *reinterpret_cast<fn_Serialize_UInt_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401529b0);
    if (slot) s_fn_SpriteSheet_GetSpriteCount = *reinterpret_cast<fn_SpriteSheet_GetSpriteCount_t*>(slot);
    // Standalone: the dispatch slot is unregistered → resolve the Runtime export
    // directly. UI::SpriteSheet::GetSpriteCount (const) — without it, Serialize
    // early-returns and the icon geometry is never set up.
    if (!s_fn_SpriteSheet_GetSpriteCount)
        s_fn_SpriteSheet_GetSpriteCount = reinterpret_cast<fn_SpriteSheet_GetSpriteCount_t>(
            ResolveExport("?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ"));

    slot = ResolveAddr(Module::Exe, 0x140151370);
    if (slot) s_fn_Scene_AddObject = *reinterpret_cast<fn_Scene_AddObject_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151360);
    if (slot) s_fn_Scene_RemoveObject = *reinterpret_cast<fn_Scene_RemoveObject_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_Time_Ctor = *reinterpret_cast<fn_Time_Ctor_t*>(slot);

    // Direct exe addresses
    s_fn_StatusIconHelper_a440 = reinterpret_cast<fn_StatusIconHelper_a440_t>(
        ResolveAddr(Module::Exe, 0x1400aa440));

    // Graphics::Geometry::SetBounds — Runtime export (the only callee of helper_a440).
    s_fn_Geometry_SetBounds = reinterpret_cast<fn_Geometry_SetBounds_t>(
        ResolveExport("?SetBounds@Geometry@Graphics@@QEAAXAEBVBox@Volume@@@Z"));

    s_fn_EntityNotify = reinterpret_cast<fn_EntityNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));

    // Original OnRemove — delegated because it's 160+ instructions with complex
    // entity positioning, bounding box calculations, map coordinate conversions,
    // and multiple subsystem interactions (too risky to fully reimplement)
    s_fn_OnRemove_Original = reinterpret_cast<fn_OnRemove_Original_t>(
        ResolveAddr(Module::Exe, 0x14012bbd0));

    // Type ID pointer
    s_TypeId_StatusIconDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7e40));

    s_helpersResolved = true;
}

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

    if (s_fn_Geometry_SetBounds)
        s_fn_Geometry_SetBounds(geometry, box);
}

// ============================================================
// Helper: Get StatusIconDescription sibling component
// ============================================================
static void* GetStatusIconDescription(uintptr_t self) {
    EnsureHelpers();
    if (!fn_IsDerivedFrom || !s_TypeId_StatusIconDesc) return nullptr;

    void* obj = *(void**)(self + 0x18);
    if (!obj) return nullptr;

    bool derived = fn_IsDerivedFrom(obj, *s_TypeId_StatusIconDesc);
    return derived ? obj : nullptr;
}

// ============================================================
// StatusIconComponent::Shutdown (vtable slot [3], 0x14012bff0)
// ============================================================
// Serializes the fieldF0 icon state. Simple: Component_Serialize base,
// then one uint via IO::Serializer(version 0). On load, writes back to +0xF0.

void StatusIconComponent_Shutdown(StatusIconComponent* self, void* stream) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Base class serialize
    fn_ComponentBase_Serialize(self, stream);

    // Create IO::Serializer (version 0)
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize fieldF0 as uint (copy to local, serialize, write back on load)
    uint32_t iconVal = *(uint32_t*)(s + 0xF0);
    if (s_fn_Serialize_UInt) {
        s_fn_Serialize_UInt(serializer, &iconVal, nullptr);
    }

    // On load: write value back to field
    // Check serializer direction: *(int*)(serializer[0] + 8) == 0 means loading
    int64_t serFirst = *(int64_t*)serializer;
    if (*(int32_t*)(serFirst + 8) == 0) {
        *(uint32_t*)(s + 0xF0) = iconVal;
    }

    // Destroy serializer
    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// StatusIconComponent::Serialize (vtable slot [4], 0x14012bed0)
// ============================================================
// Reads sprite data from StatusIconDescription, initializes geometry.
// Gets description, reads SpriteSheet::GetSpriteCount, calls helper
// to set up the geometry with sprite dimensions and sheet reference.

void StatusIconComponent_Serialize(StatusIconComponent* self) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    void* desc = GetStatusIconDescription(s);
    if (!desc) return;
    uintptr_t d = (uintptr_t)desc;

    // Get sprite count from the SpriteSheet at description +0x20
    void* spriteSheet = *(void**)(d + 0x20);
    if (!spriteSheet || !s_fn_SpriteSheet_GetSpriteCount) return;

    int spriteCount = s_fn_SpriteSheet_GetSpriteCount(spriteSheet);

    // Call geometry setup helper with:
    //   geometry (+0x28), float from desc +0x28, sprite count,
    //   value from desc +0x30 (Geometry data), SpriteSheet ptr from desc +0x20
    if (s_fn_StatusIconHelper_a440) {
        float iconSize = *(float*)(d + 0x28);
        // desc +0x30 is passed as a Geometry-like value (copied by value in original)
        void* geomData = *(void**)(d + 0x30);
        s_fn_StatusIconHelper_a440(
            (void*)(s + 0x28), iconSize, spriteCount, geomData, spriteSheet);
    }
}

// ============================================================
// StatusIconComponent::OnRemove (vtable slot [8], 0x14012bbd0)
// ============================================================
// Manages Scene add/remove based on game settings icon visibility flag,
// scales icon based on game settings, positions based on entity's model
// bounding box or map coordinates.
//
// This function is extremely complex (160+ instructions) with deep coupling to:
// - Game settings byte at +0xC (icon visibility toggle)
// - Graphics::Scene add/remove
// - Entity bounding box calculations (Volume::Box)
// - Map coordinate to world position conversion
// - Graphics::Geometry::SetTransform
//
// Delegated to original exe code to avoid incorrect reimplementation.

void StatusIconComponent_OnRemove(StatusIconComponent* self) {
    EnsureHelpers();
    if (s_fn_OnRemove_Original) {
        s_fn_OnRemove_Original(self);
    }
}

// ============================================================
// StatusIconComponent::OnUpdate2 (vtable slot [11], 0x14012bf40)
// ============================================================
// Updates icon visibility. Calls Geometry vtable +0x20, saves/restores
// fieldF0 value. If iconId < 0 and fieldF0 != 0: adds geometry to Scene,
// sets flag bit 2, notifies owner entity, then calls vtable +0x40 (OnRemove)
// with Time(0) to trigger positioning.

void StatusIconComponent_OnUpdate2(StatusIconComponent* self) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Save fieldF0 (fieldE8 in longlong* terms = offset 0xF0)
    int32_t savedFieldF0 = *(int32_t*)(s + 0xF0);

    // Call Geometry vtable +0x20 (Draw/Update)
    typedef void (*fn_GeomVtable20_t)(void*);
    fn_GeomVtable20_t drawFn = *(fn_GeomVtable20_t*)(*(uintptr_t*)(s + 0x28) + 0x20);
    if (drawFn) {
        drawFn((void*)(s + 0x28));
    }

    // Restore fieldF0 to saved value (original: *(int*)(param_1 + 0x1e) cast)
    *(int32_t*)(s + 0xF0) = savedFieldF0;

    // If iconId < 0 and fieldF0 != 0: add to scene and notify
    int32_t iconId = *(int32_t*)(s + 0x100);
    if (iconId < 0 && savedFieldF0 != 0) {
        // Add geometry to scene
        if (s_fn_Scene_AddObject && g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene) {
                uint32_t newId = s_fn_Scene_AddObject(scene, (void*)(s + 0x28));

                // Set flags: OR bit 2 (0x04) into flags at +0x20
                uint32_t oldFlags = *(uint32_t*)(s + 0x20);
                *(uint32_t*)(s + 0x100) = newId;
                uint32_t newFlags = oldFlags | 0x04u;
                *(uint32_t*)(s + 0x20) = newFlags;

                // If flags changed (low 16 bits) and owner non-null, notify entity
                if (((newFlags ^ oldFlags) & 0xFFFF) != 0) {
                    void* ownerEntity = *(void**)(s + 0x10);
                    if (ownerEntity != nullptr) {
                        if (fn_InternalBasePtr_CheckPtr) {
                            fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
                        }
                        if (s_fn_EntityNotify) {
                            s_fn_EntityNotify(*(void**)(s + 0x10));
                        }
                    }
                }

                // Call vtable +0x40 (OnRemove) with Time(0) to position the icon
                if (s_fn_Time_Ctor) {
                    uint8_t timeObj[8];
                    s_fn_Time_Ctor(timeObj, 0.0f);
                    typedef void (*fn_Vtable40_t)(void*, void*);
                    fn_Vtable40_t onRemoveFn = *(fn_Vtable40_t*)(*(uintptr_t*)s + 0x40);
                    if (onRemoveFn) {
                        onRemoveFn(self, timeObj);
                    }
                }
            }
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void StatusIconComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[StatusIconComponent] Funcs resolved: SerializeUInt=%p SpriteCount=%p "
           "Helper_a440=%p AddObj=%p RemoveObj=%p OnRemoveOrig=%p\n",
           (void*)s_fn_Serialize_UInt, (void*)s_fn_SpriteSheet_GetSpriteCount,
           (void*)s_fn_StatusIconHelper_a440,
           (void*)s_fn_Scene_AddObject, (void*)s_fn_Scene_RemoveObject,
           (void*)s_fn_OnRemove_Original);
#endif
}

// ============================================================
// Standalone bring-up (Phase A): faithful construction + diagnostics
// ============================================================
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include <cstdlib>

// The StatusIcon's embedded Graphics::Geometry sub-object (+0x28) normally uses the
// exe-resident billboard vtable PTR_FUN_14015f0c0, whose draw slots (AddDrawItems
// 0x1400a9b90 / Draw 0x1400a9c10) live in the unmapped exe. Until those are ported
// (Phase B), give the sub-object a SAFE standalone vtable: every slot is a no-op so
// that even when the occupancy toggle AddObject's the geometry, the scene's
// AddDrawItems call (vtable slot +0x18 here) is a no-op → nothing is drawn and the
// Draw slot is never reached → crash-free. Slot 0 (dtor) is also a no-op (the
// embedded sub-object is freed with the component; leaking its internals for a
// session is acceptable). 16 slots = comfortable headroom over the 12-slot original.
static void SIGeom_Noop() {}
static void* g_StatusIconGeometryVtable[16];
static bool  g_siGeomVtableInit = false;
static void EnsureGeometryVtable() {
    if (g_siGeomVtableInit) return;
    for (int i = 0; i < 16; ++i)
        g_StatusIconGeometryVtable[i] = reinterpret_cast<void*>(&SIGeom_Noop);
    g_siGeomVtableInit = true;
}

// ============================================================
// Phase B — real billboard draw + positioning (standalone)
// ============================================================
// The real billboard Draw/AddDrawItems vtable lives in statusicon_geometry.cpp.
void  StatusIconGeometry_BuildVtable(void** baseVtbl);
void** StatusIconGeometry_Vtable();
bool  StatusIconGeometry_VtableBuilt();

// Runtime base Graphics::Geometry ctor (??0Geometry@Graphics@@QEAA@XZ) — sets the base
// vtable + initializes InterpolatedState<Transform> / bounds fields the draw path needs.
typedef void* (*fn_Geometry_BaseCtor_t)(void* geometry);
static fn_Geometry_BaseCtor_t s_fn_Geometry_BaseCtor = nullptr;

// Graphics::Geometry::SetTransform (Runtime export) — used by OnRemove positioning.
typedef void (*fn_SI_GeomSetTransform_t)(void* geometry, const void* transform);
static fn_SI_GeomSetTransform_t s_fn_Geometry_SetTransform = nullptr;

// Entity component finders (registered exe ports; member ABI RCX=entity, RDX=retIBP).
typedef void* (*fn_Finder_t)(uintptr_t entity, void* outIBP);
static fn_Finder_t s_fn_FindModel = nullptr;   // 0x140003f60 -> ModelComponent (type 0x1401c7a50)
static fn_Finder_t s_fn_FindMulti = nullptr;   // 0x140004740 -> MultiObjectComponent (type 0x1401c7ac0)
static fn_Finder_t s_fn_FindMap   = nullptr;   // 0x140004680 -> MapComponent (type 0x1401c79e0)

// CharacterComponent_impl_f3e0 (0x14009f3e0) — grid cell -> world position.
typedef float* (*fn_GridToWorld_t)(int* grid, float* outWorld, float* inGrid, char flag, void* extra);
static fn_GridToWorld_t s_fn_GridToWorld = nullptr;

static bool s_drawHelpersResolved = false;
static void EnsureDrawHelpers() {
    if (s_drawHelpersResolved) return;
    s_fn_Geometry_BaseCtor = reinterpret_cast<fn_Geometry_BaseCtor_t>(
        ResolveExport("??0Geometry@Graphics@@QEAA@XZ"));
    s_fn_Geometry_SetTransform = reinterpret_cast<fn_SI_GeomSetTransform_t>(
        ResolveExport("?SetTransform@Geometry@Graphics@@QEAAXAEBVTransform@Math@@@Z"));
    s_fn_FindModel = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_FindMulti = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140004740));
    s_fn_FindMap   = reinterpret_cast<fn_Finder_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_GridToWorld = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_drawHelpersResolved = true;
}

// Component-vtable +0x40 (idx 8, OnRemove = positioning) routed for standalone. The
// occupancy toggle FUN_14012c3f0 (ported below as StatusIcon_Toggle) calls this slot
// directly to position the icon after AddObject. Original signature is (self, Time*);
// the Time arg is ignored by the body (0x14012bbd0). Forward-decl; defined further down.
void StatusIconComponent_OnRemove_Standalone(StatusIconComponent* self);
static void StatusIcon_OnRemove_VSlot(void* self, void* /*time*/) {
    StatusIconComponent_OnRemove_Standalone(reinterpret_cast<StatusIconComponent*>(self));
}


// Removed-from-world handler (component vtable +0x38, idx 7). Entity teardown
// dispatches this via MarkForRemoval (0x1400c2ea0: every component with
// capability bit 1 — ours has 0xC2 — gets vtable+0x38 before the 0x400 bit is
// set). Remove the icon's scene object so the renderer's draw list never holds
// freed geometry — same crash class as the StorageDisplay pile meshes fixed
// 2026-07-20 (demolition → Draw-phase Transform::Lerp on freed memory). Retail
// additionally strips icons at demolition start (state-3 enter → helper_c2a0);
// this teardown-time removal is the safety net that must always run.
static void StatusIcon_OnRemovedFromWorld_VSlot(void* self) {
    EnsureHelpers();
    uintptr_t c = (uintptr_t)self;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
    int32_t sceneId = *(int32_t*)(c + 0x100);
    if (getenv("BANISHED_DESTROY_TRACE")) {
        fprintf(stderr, "[StatusIcon] removed-from-world comp=%p sceneId=%d\n",
                self, sceneId);
        fflush(stderr);
    }
    if (sceneId >= 0 && scene && s_fn_Scene_RemoveObject) {
        s_fn_Scene_RemoveObject(scene, (uint32_t)sceneId);
        *(int32_t*)(c + 0x100) = -1;
    }
}

// StatusIconComponent_StartupInit — the spawn-time body init the inert factory skips.
// Routed to the component's Startup/Serialize slot (vtable +0x20 / index 4) so it runs
// during ComponentEntity::StartupComponents, exactly where the original dispatches the
// real Serialize (geometry setup). Replicates the parts of the real ctor (0x14012c0c0)
// the inert factory zero-fills instead of setting — critically sceneObjId(+0x100) = -1
// (the inert factory leaves it 0 → the occupancy toggle's RemoveObject spins on scene
// id 0). Then runs the real geometry-sprite Serialize. Env-gated: default-off keeps the
// run byte-identical.
void StatusIconComponent_StartupInit(StatusIconComponent* self) {
    EnsureHelpers();
    EnsureGeometryVtable();
    uintptr_t s = (uintptr_t)self;

    // Phase B: when BANISHED_LIVE_STATUSICON_DRAW is set, build the REAL embedded
    // Graphics::Geometry (Runtime base ctor → base vtable + InterpolatedState/bounds
    // fields) and install a standalone billboard vtable that overrides only the two
    // draw slots (Draw=2, AddDrawItems=3). Otherwise keep the Phase A no-op vtable.
    static bool drawMode = ::ComponentFactory::LiveEnabled("BANISHED_LIVE_STATUSICON_DRAW");
    if (drawMode) {
        EnsureDrawHelpers();
        if (s_fn_Geometry_BaseCtor) {
            s_fn_Geometry_BaseCtor((void*)(s + 0x28));        // base vtable + base fields
            void** baseV = *(void***)(s + 0x28);
            StatusIconGeometry_BuildVtable(baseV);            // copy base, override 2,3
        }
        *(void**)(s + 0x28) = StatusIconGeometry_VtableBuilt()
                                  ? StatusIconGeometry_Vtable()
                                  : g_StatusIconGeometryVtable;

        // Route the COMPONENT-vtable +0x40 (idx 8 = OnRemove = positioning) to the
        // standalone port. The inert factory leaves this an all-stub no-op; the occupancy
        // toggle FUN_14012c3f0 (StatusIcon_Toggle) calls component vtable+0x40 to position
        // the icon after AddObject, so it MUST point at OnRemove_Standalone or the icon
        // is added at the origin (un-positioned). The component vtable ptr at offset 0 is
        // the shared per-type LiveVtable (installed by InitInertTyped); writing slot 8 once
        // is idempotent across instances. (+0x58 Slot11 is intentionally left inert:
        // +0x48 Update is a bare `ret` so there is no per-tick path, and +0x58's
        // only dispatcher is the event-driven entity-notify FUN_1400c2bf0 which never sets
        // the occupancy flag — the warning bit is owned solely by the +0x40-driving toggle.
        // +0x38 removed-from-world IS wired below — teardown scene-object removal.)
        void** vt = *(void***)s;
        if (vt) vt[8] = reinterpret_cast<void*>(&StatusIcon_OnRemove_VSlot);
    } else {
        // Faithful ctor field-init delta (inert factory already did Object::Internal
        // base + flags +0x20|=0xC2 + zeroed body): geometry sub-object (+0x28) vtable
        // → safe standalone no-op billboard vtable.
        *(void**)(s + 0x28) = g_StatusIconGeometryVtable;
    }
    *(uint32_t*)(s + 0x100) = 0xFFFFFFFFu;   // sceneObjId = -1 (NOT scene-registered)

    // Teardown safety (both phases): route component vtable +0x38 (idx 7,
    // removed-from-world) to the scene-object removal above. The vtable ptr is
    // the shared per-type LiveVtable — writing once per instance is idempotent.
    {
        void** vtAll = *(void***)s;
        if (vtAll)
            vtAll[7] = reinterpret_cast<void*>(&StatusIcon_OnRemovedFromWorld_VSlot);
    }

    // Real geometry-sprite setup from the StatusIconDescription's SpriteSheet (guarded
    // — no-ops if desc/spriteSheet absent). The icon's warning bit is now raised/cleared
    // by genuine events (WorkPlaceComponent_SetAccepting / _Attach for building warnings;
    // CitizenComponent Update for citizen health icons) — no per-frame occupancy poll.
    StatusIconComponent_Serialize(self);

    // Diagnostic: settle whether the sprite-sheet RESOURCE is present in standalone
    // (the gating unknown for Phase B rendering). desc+0x20 = SpriteSheet,
    // spriteSheet+0x30 = MaterialInstance.
    static bool trace = [](){ const char* e = getenv("BANISHED_STATUSICON_TRACE");
                              return e && e[0] && e[0] != '0'; }();
    if (trace) {
        void* desc = GetStatusIconDescription(s);
        void* sheet = desc ? *(void**)((uintptr_t)desc + 0x20) : nullptr;
        void* mat   = sheet ? *(void**)((uintptr_t)sheet + 0x30) : nullptr;
        int   cnt   = (sheet && s_fn_SpriteSheet_GetSpriteCount)
                          ? s_fn_SpriteSheet_GetSpriteCount(sheet) : -1;
        printf("[StatusIcon::Startup] self=%p desc=%p sheet=%p material=%p spriteCount=%d "
               "geomBits(+0xc4)=%d\n",
               (void*)self, desc, sheet, mat, cnt, *(int*)(s + 0x28 + 0xC4));
        fflush(stdout);
    }
}

// Resolve a component on the owner entity BY NAME (Object::GetTypeIdByName), mirroring
// CharacterComponent's CharComp_FindSiblingByName. This is the standalone-safe equivalent
// of the exe finders 0x140003f60 (ModelComponent) / 0x140004740 (MultiObjectComponent):
// those compare the .rdata type-id globals DWORD_1401c7a50 / c7ac0, which are 0xFFFFFFFF
// (UNFILLED) in standalone, so they return null for EVERY entity. Resolving the type id by
// name at runtime matches the established brick pattern and has no shared-global dependency.
static void* StatusIcon_FindCompByName(uintptr_t entity, const char* typeName) {
    if (!entity || !fn_IsDerivedFrom) return nullptr;
    uint32_t want = ::Object::GetTypeIdByName(typeName);
    if (want == 0 || want == 0xFFFFFFFFu) return nullptr;
    int count = *(int*)(entity + 0x38);
    int64_t arr = *(int64_t*)(entity + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = (void**)(arr + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
        void* comp = *ibp;
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (tid == want)
            return fn_IsDerivedFrom(comp, want) ? comp : nullptr;
    }
    return nullptr;
}

// ============================================================
// Standalone faithful port of StatusIconComponent::OnRemove (0x14012bbd0)
// ============================================================
// Manages Scene add/remove on the game-settings icon-visibility byte (settings+0xc),
// then positions the icon. Buildings take the MODEL-BOUNDS path (icon centred above
// the model's bounding box); the map-cell fallback mirrors the original for entities
// without a model. Verified line-by-line against the disassembly.
void StatusIconComponent_OnRemove_Standalone(StatusIconComponent* self) {
    EnsureHelpers();
    EnsureDrawHelpers();
    uintptr_t c = (uintptr_t)self;

    void* settings = g_pGameSettingsPtr ? *g_pGameSettingsPtr : nullptr;
    if (!settings || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;
    void* scene = *(void**)(gw + 0x60);
    uint8_t iconVisible = reinterpret_cast<uint8_t*>(settings)[0xc];

    int32_t sceneId = *(int32_t*)(c + 0x100);
    if (iconVisible == 0) {
        if (sceneId >= 0 && s_fn_Scene_RemoveObject && scene) {
            s_fn_Scene_RemoveObject(scene, (uint32_t)sceneId);
            *(int32_t*)(c + 0x100) = -1;
        }
    } else if (sceneId < 0) {
        if (s_fn_Scene_AddObject && scene) {
            uint32_t id = s_fn_Scene_AddObject(scene, (void*)(c + 0x28));
            *(int32_t*)(c + 0x100) = (int32_t)id;
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
    void* desc = (fn_IsDerivedFrom && s_TypeId_StatusIconDesc &&
                  fn_IsDerivedFrom(descObj, *s_TypeId_StatusIconDesc)) ? descObj : nullptr;

    float pos[3];
    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = desc ? *(float*)((uintptr_t)desc + 0x2c) : 0.0f;   // base height offset

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(c + 0x10));
    uintptr_t entity = *(uintptr_t*)(c + 0x10);

    // Resolve Model + MultiObject by NAME (the exe finders key off unfilled .rdata globals
    // c7a50/c7ac0 → null for every entity in standalone; see StatusIcon_FindCompByName).
    // The MODEL-BOUNDS path then resolves for citizens (model = CharacterComponent's sibling
    // ModelComponent, +0x158 = live move-tick position) AND faithfully preserves the original's
    // branch for buildings that carry a MultiObjectComponent (→ map-cell). Map fallback below
    // still uses the exe finder (its c79e0 global IS filled, so it works).
    void* modelIBP[2] = { nullptr, nullptr };
    void* multiIBP[2] = { nullptr, nullptr };
    modelIBP[0] = StatusIcon_FindCompByName(entity, "ModelComponent");
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(c + 0x10));
    multiIBP[0] = StatusIcon_FindCompByName(entity, "MultiObjectComponent");

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
        if (s_fn_FindMap) s_fn_FindMap(entity, mapIBP);
        if (mapIBP[0] && s_fn_GridToWorld) {
            uintptr_t mc = (uintptr_t)mapIBP[0];
            int* widthPtr = *(int**)(gw + 0xf8);
            int w = *widthPtr;
            int idx = ((int)*(int16_t*)(mc + 0x2a) + ((int)*(int16_t*)(mc + 0x2e) >> 1)) * w
                      + ((int)*(int16_t*)(mc + 0x2c) >> 1) + (int)*(int16_t*)(mc + 0x28);
            float f = (float)(w >> 1) - 0.5f;
            float in[3]  = { (float)(idx % w) - f, (float)(idx / w) - f, 0.0f };
            float out[3] = { 0.0f, 0.0f, 0.0f };
            s_fn_GridToWorld(widthPtr, out, in, 0, nullptr);
            pos[0] += out[0];
            pos[1] += out[1];
            pos[2] += out[2];
        }
    }

    // Transform (0x1C bytes): {0,0,0, 1.0(cos), pos.x, pos.y, pos.z} — pure translation.
    float xf[7] = { 0.0f, 0.0f, 0.0f, 1.0f, pos[0], pos[1], pos[2] };
    if (s_fn_Geometry_SetTransform) s_fn_Geometry_SetTransform((void*)(c + 0x28), xf);
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
    EnsureHelpers();
    uintptr_t c = (uintptr_t)selfC;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gw = (uintptr_t)*g_pGameWorldSingleton;
    void* scene = *(void**)(gw + 0x60);

    // Culled-entity guard: if the owner entity is culled (entity+0x40 bit 10) the original
    // skips the SET path (only clears are honoured while culled).
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(c + 0x10));
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
        if (sid >= 0 && s_fn_Scene_RemoveObject && scene) {
            s_fn_Scene_RemoveObject(scene, (uint32_t)sid);
            *(int32_t*)(c + 0x100) = -1;
        }
        new20 = old20 & 0xfffffffbu;
    } else {
        // At least one status bit set → ensure the icon is in the scene (visibility on)
        // and position it via component vtable+0x40 (OnRemove).
        void* settings = g_pGameSettingsPtr ? *g_pGameSettingsPtr : nullptr;
        uint8_t vis = settings ? reinterpret_cast<uint8_t*>(settings)[0xc] : 0;
        if (vis != 0 && *(int32_t*)(c + 0x100) < 0 && s_fn_Scene_AddObject && scene) {
            uint32_t id = s_fn_Scene_AddObject(scene, (void*)(c + 0x28));
            *(int32_t*)(c + 0x100) = (int32_t)id;
        }
        if (s_fn_Time_Ctor) {
            uint8_t timeObj[8];
            s_fn_Time_Ctor(timeObj, 0.0f);
            typedef void (*fn_V40_t)(void*, void*);
            fn_V40_t v40 = *(fn_V40_t*)(*(uintptr_t*)c + 0x40);
            if (v40) v40(selfC, timeObj);
        }
        new20 = old20 | 4u;
    }
    *(uint32_t*)(c + 0x20) = new20;
    if (((new20 ^ old20) & 0xffff) != 0 && *(uintptr_t*)(c + 0x10) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr((void*)(c + 0x10));
        if (s_fn_EntityNotify) s_fn_EntityNotify(*(void**)(c + 0x10));
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

// COMPONENT_LIVE_FACTORY_STARTUP routes ONLY the Startup slot (index 4 / +0x20) to
// StatusIconComponent_StartupInit when BANISHED_LIVE_STATUSICON (or the global
// BANISHED_LIVE_COMPONENTS) is set; default-off => identical to the inert factory.
COMPONENT_LIVE_FACTORY_STARTUP(StatusIconComponent, 0x108, 0xC2,
                               "BANISHED_LIVE_STATUSICON", StatusIconComponent_StartupInit)
REGISTER_TYPE(StatusIconComponent, "Component",
              &StatusIconComponent_Factory, &StatusIconComponent_CreateInPlace);
