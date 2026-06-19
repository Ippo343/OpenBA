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
// This file must NOT depend on hooks.h — portable for standalone exe.

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

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(StatusIconComponent, 0x108, 0xC2)
REGISTER_TYPE(StatusIconComponent, "Component",
              &StatusIconComponent_Factory, &StatusIconComponent_CreateInPlace);
#endif // STANDALONE_EXE
