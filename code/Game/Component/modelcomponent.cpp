// code/Game/Component/modelcomponent.cpp
// Clean reimplementation of ModelComponent lifecycle methods from Application-x64.exe.
//
// ModelComponent (0x170 bytes) is the largest component. It embeds a
// Graphics::MeshInstance sub-object at +0x28 (0x120 bytes) and a
// Math::Transform at +0x148 (0x1C bytes).
//
// Vtable at 0x140163ba0. Non-trivial overrides:
//   Shutdown  (0x140124b70) — Serializes mesh state, transform, model variant data
//   Serialize (0x140124ac0) — Reads building data, sets mesh from resource arrays
//   Draw      (0x140124a30) — Adds mesh to Graphics::Scene if valid variant
//   OnCreate  (0x1401249d0) — Removes mesh from Scene, resets modelId
//   OnRemove  (0x140124960) — Calls MeshInstance update, checks animating, clears flag
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/modelcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// IO::Serializer::Serialize(short*) — dispatch slot at 0x140152468
typedef void (*fn_Serialize_Short_t)(void* serializer, int16_t* field, const char* name);
static fn_Serialize_Short_t s_fn_Serialize_Short = nullptr;

// IO::Serializer::Serialize(float*) — dispatch slot at 0x140152a60
typedef void (*fn_Serialize_Float_t)(void* serializer, float* field, const char* name);
static fn_Serialize_Float_t s_fn_Serialize_Float = nullptr;

// Math::Transform::Serialize — dispatch slot at 0x1401522e0
// Note: param order is (transform, serializer) per disassembly (RCX=transform, RDX=serializer)
typedef void (*fn_Transform_Serialize_t)(void* transform, void* serializer);
static fn_Transform_Serialize_t s_fn_Transform_Serialize = nullptr;

// Graphics::MeshInstance::Serialize — dispatch slot at 0x140152990
typedef void (*fn_MeshInstance_Serialize_t)(void* meshInstance, void* serializer);
static fn_MeshInstance_Serialize_t s_fn_MeshInstance_Serialize = nullptr;

// Graphics::MeshInstance::SetMesh — dispatch slot at 0x140151498
typedef void (*fn_MeshInstance_SetMesh_t)(void* meshInstance, void* meshResource);
static fn_MeshInstance_SetMesh_t s_fn_MeshInstance_SetMesh = nullptr;

// Graphics::MeshInstance::IsAnimating — dispatch slot at 0x1401514b0
typedef bool (*fn_MeshInstance_IsAnimating_t)(void* meshInstance);
static fn_MeshInstance_IsAnimating_t s_fn_MeshInstance_IsAnimating = nullptr;

// Graphics::Geometry::SetTransform — dispatch slot at 0x1401512e0
typedef void (*fn_Geometry_SetTransform_t)(void* geometry, void* transform);
static fn_Geometry_SetTransform_t s_fn_Geometry_SetTransform = nullptr;

// Graphics::Scene::AddObject — dispatch slot at 0x140151370
typedef uint32_t (*fn_Scene_AddObject_t)(void* scene, void* geometry);
static fn_Scene_AddObject_t s_fn_Scene_AddObject = nullptr;

// Graphics::Scene::RemoveObject — dispatch slot at 0x140151360
typedef void (*fn_Scene_RemoveObject_t)(void* scene, uint32_t objectId);
static fn_Scene_RemoveObject_t s_fn_Scene_RemoveObject = nullptr;

// System::Random::Rand — dispatch slot at 0x1401512e8
typedef uint32_t (*fn_Random_Rand_t)(void* random, uint32_t range);
static fn_Random_Rand_t s_fn_Random_Rand = nullptr;

// System::Time::Time(Time*, float) — dispatch slot at 0x140152b00
typedef void (*fn_Time_Ctor_t)(void* self, float seconds);
static fn_Time_Ctor_t s_fn_Time_Ctor = nullptr;

// ModelComponent_helper_2b70 (0x140122b70) — notifies entity of model change
typedef void (*fn_ModelHelper_2b70_t)(void* entity, int param);
static fn_ModelHelper_2b70_t s_fn_ModelHelper_2b70 = nullptr;

// FUN_1400c3470 — entity notification callback
typedef void (*fn_EntityNotify_t)(void* entity);
static fn_EntityNotify_t s_fn_EntityNotify = nullptr;

// DAT_1401c7a88 — MultiObjectDescription type ID
static uint32_t* s_TypeId_MultiObjDesc = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    // Dispatch slots — read function pointer from .rdata slot at runtime
    slot = ResolveAddr(Module::Exe, 0x140152468);
    if (slot) s_fn_Serialize_Short = *reinterpret_cast<fn_Serialize_Short_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_Serialize_Float = *reinterpret_cast<fn_Serialize_Float_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401522e0);
    if (slot) s_fn_Transform_Serialize = *reinterpret_cast<fn_Transform_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152990);
    if (slot) s_fn_MeshInstance_Serialize = *reinterpret_cast<fn_MeshInstance_Serialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151498);
    if (slot) s_fn_MeshInstance_SetMesh = *reinterpret_cast<fn_MeshInstance_SetMesh_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401514b0);
    if (slot) s_fn_MeshInstance_IsAnimating = *reinterpret_cast<fn_MeshInstance_IsAnimating_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401512e0);
    if (slot) s_fn_Geometry_SetTransform = *reinterpret_cast<fn_Geometry_SetTransform_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151370);
    if (slot) s_fn_Scene_AddObject = *reinterpret_cast<fn_Scene_AddObject_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151360);
    if (slot) s_fn_Scene_RemoveObject = *reinterpret_cast<fn_Scene_RemoveObject_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401512e8);
    if (slot) s_fn_Random_Rand = *reinterpret_cast<fn_Random_Rand_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_Time_Ctor = *reinterpret_cast<fn_Time_Ctor_t*>(slot);

    // Direct exe addresses
    s_fn_ModelHelper_2b70 = reinterpret_cast<fn_ModelHelper_2b70_t>(
        ResolveAddr(Module::Exe, 0x140122b70));

    s_fn_EntityNotify = reinterpret_cast<fn_EntityNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));

    // Type ID pointer
    s_TypeId_MultiObjDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a88));

    s_helpersResolved = true;
}

// ============================================================
// Helper: Get MultiObjectDescription sibling component
// ============================================================
static void* GetMultiObjDescription(uintptr_t self) {
    EnsureHelpers();
    if (!fn_IsDerivedFrom || !s_TypeId_MultiObjDesc) return nullptr;

    void* obj = *(void**)(self + 0x18);
    if (!obj) return nullptr;

    bool derived = fn_IsDerivedFrom(obj, *s_TypeId_MultiObjDesc);
    return derived ? obj : nullptr;
}

// ============================================================
// Helper: Get mesh resource from description arrays
// ============================================================
// Description +0x20 = array of variant groups (each 0x18 bytes, sub-array ptr at +8)
// variant group[variantIdx] has sub-array at +8, indexed by modelVariant * 8
static void* GetMeshFromDescription(void* desc, int16_t variantIdx, int16_t modelVariant) {
    if (!desc || modelVariant < 0) return nullptr;
    uintptr_t d = (uintptr_t)desc;
    // desc+0x20 points to array of variant groups
    uintptr_t groupArray = *(uintptr_t*)(d + 0x20);
    // Each group is 0x18 bytes; get the one at variantIdx
    uintptr_t group = *(uintptr_t*)(groupArray + 8 + (int64_t)variantIdx * 0x18);
    // Index into sub-array by modelVariant
    void* meshResource = *(void**)(group + (int64_t)modelVariant * 8);
    return meshResource;
}

// ============================================================
// ModelComponent::Shutdown (vtable slot [3], 0x140124b70)
// ============================================================
// Serializes mesh state: variant indices, transform, scale, mesh instance data,
// and a boolean flag for whether modelId is valid.
//
// On load (serializer direction == 0): if variant >= 0, calls SetMesh to
// restore the mesh from the description resource arrays.
// Also conditionally calls vtable Draw (+0x30) to re-add to scene.

void ModelComponent_Shutdown(ModelComponent* self, void* stream) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Base class serialize
    fn_ComponentBase_Serialize(self, stream);

    // Create IO::Serializer (version 0)
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize variant index (+0x164 as short) and model variant (+0x166 as short)
    if (s_fn_Serialize_Short) {
        s_fn_Serialize_Short(serializer, (int16_t*)(s + 0x164), nullptr);
        s_fn_Serialize_Short(serializer, (int16_t*)(s + 0x166), nullptr);
    }

    // Serialize Math::Transform at +0x148
    if (s_fn_Transform_Serialize) {
        s_fn_Transform_Serialize((void*)(s + 0x148), serializer);
    }

    // Serialize scale float at +0x168
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, (float*)(s + 0x168), nullptr);
    }

    // On load: if model variant >= 0, restore mesh from description
    int64_t serFirst = *(int64_t*)serializer;
    if (*(int32_t*)(serFirst + 8) == 0) {
        int16_t modelVariant = *(int16_t*)(s + 0x166);
        if (modelVariant >= 0) {
            void* desc = GetMultiObjDescription(s);
            void* meshResource = GetMeshFromDescription(
                desc, *(int16_t*)(s + 0x164), modelVariant);
            if (meshResource && s_fn_MeshInstance_SetMesh) {
                s_fn_MeshInstance_SetMesh((void*)(s + 0x28), meshResource);
            }
        }
    }

    // Serialize MeshInstance sub-object
    if (s_fn_MeshInstance_Serialize) {
        s_fn_MeshInstance_Serialize((void*)(s + 0x28), serializer);
    }

    // Serialize bool flag: modelId >= 0
    bool hasModelId = (self->modelId >= 0);
    g_fn_Serialize_Bool(serializer, &hasModelId, nullptr);

    // On load: if flag was set, call vtable Draw (+0x30) to re-register in scene
    serFirst = *(int64_t*)serializer;
    if (*(int32_t*)(serFirst + 8) == 0 && hasModelId) {
        // Call vtable slot +0x30 (Draw) on self
        typedef void (*fn_VtableDraw_t)(void*);
        fn_VtableDraw_t drawFn = *(fn_VtableDraw_t*)(*(uintptr_t*)s + 0x30);
        if (drawFn) {
            drawFn(self);
        }
    }

    // Destroy serializer
    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// ModelComponent::Serialize (vtable slot [4], 0x140124ac0)
// ============================================================
// Reads building data from MultiObjectDescription, sets variant indices,
// optionally randomizes variant, calls SetMesh if variant >= 0.

void ModelComponent_Serialize(ModelComponent* self) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    void* desc = GetMultiObjDescription(s);
    if (!desc) return;
    uintptr_t d = (uintptr_t)desc;

    // Read variant index from description +0x30 (as short -> stored at +0x164)
    *(int16_t*)(s + 0x164) = *(int16_t*)(d + 0x30);

    // If description +0x38 (byte flag) is set, randomize the variant index
    if (*(uint8_t*)(d + 0x38) != 0) {
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton && s_fn_Random_Rand) {
            void* randomPtr = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
            uint32_t range = *(uint32_t*)(d + 0x28);
            uint32_t randVal = s_fn_Random_Rand(randomPtr, range);
            *(int16_t*)(s + 0x164) = (int16_t)randVal;
        }
    }

    // Read model variant from description +0x34
    int16_t modelVariant = *(int16_t*)(d + 0x34);
    *(int16_t*)(s + 0x166) = modelVariant;

    // If variant >= 0, set the mesh
    if (modelVariant >= 0 && s_fn_MeshInstance_SetMesh) {
        void* meshResource = GetMeshFromDescription(
            desc, *(int16_t*)(s + 0x164), modelVariant);
        if (meshResource) {
            s_fn_MeshInstance_SetMesh((void*)(s + 0x28), meshResource);
        }
    }
}

// ============================================================
// ModelComponent::Draw (vtable slot [6], 0x140124a30)
// ============================================================
// Adds mesh geometry to the Graphics::Scene for rendering.
// Only acts if variant >= 0 and not yet in scene (modelId < 0).

void ModelComponent_Draw(ModelComponent* self) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    int16_t modelVariant = *(int16_t*)(s + 0x166);
    int32_t modelId = *(int32_t*)(s + 0x16c);

    if (modelVariant >= 0 && modelId < 0) {
        // IsDerivedFrom check (result unused in original — just side effect?)
        if (fn_IsDerivedFrom && s_TypeId_MultiObjDesc) {
            fn_IsDerivedFrom(*(void**)(s + 0x18), *s_TypeId_MultiObjDesc);
        }

        // Set transform on the geometry sub-object
        if (s_fn_Geometry_SetTransform) {
            s_fn_Geometry_SetTransform((void*)(s + 0x28), (void*)(s + 0x148));
        }

        // Call MeshInstance::Update with Time(0) via vtable +0x8
        if (s_fn_Time_Ctor) {
            uint8_t timeObj[8];
            s_fn_Time_Ctor(timeObj, 0.0f);
            typedef void (*fn_MeshVtable8_t)(void*, void*);
            fn_MeshVtable8_t updateFn = *(fn_MeshVtable8_t*)(*(uintptr_t*)(s + 0x28) + 0x8);
            if (updateFn) {
                updateFn((void*)(s + 0x28), timeObj);
            }
        }

        // Add to scene
        if (s_fn_Scene_AddObject && g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene) {
                uint32_t id = s_fn_Scene_AddObject(scene, (void*)(s + 0x28));
                *(uint32_t*)(s + 0x16c) = id;
            }
        }
    }
}

// ============================================================
// ModelComponent::OnCreate (vtable slot [7], 0x1401249d0)
// ============================================================
// Removes mesh from Scene and resets modelId. Notifies owner entity.

void ModelComponent_OnCreate(ModelComponent* self) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    int16_t modelVariant = *(int16_t*)(s + 0x166);
    int32_t modelId = *(int32_t*)(s + 0x16c);

    if (modelVariant >= 0 && modelId >= 0) {
        // Remove from scene
        if (s_fn_Scene_RemoveObject && g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene) {
                s_fn_Scene_RemoveObject(scene, (uint32_t)modelId);
            }
        }

        // Reset modelId to -1
        *(int32_t*)(s + 0x16c) = -1;

        // Check owner and notify
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
        }
        void* ownerEntity = *(void**)(s + 0x10);
        if (ownerEntity && s_fn_ModelHelper_2b70) {
            s_fn_ModelHelper_2b70(ownerEntity, 0);
        }
    }
}

// ============================================================
// ModelComponent::OnRemove (vtable slot [8], 0x140124960)
// ============================================================
// Calls MeshInstance vtable +0x8 (Update), checks IsAnimating.
// If not animating, clears draw flag bit 2 from +0x20, notifies entity.
// NOTE: RDX (time) is forwarded implicitly by the caller.

void ModelComponent_OnRemove(ModelComponent* self, void* time) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Call MeshInstance vtable +0x8 (Update) — RDX contains time pointer
    typedef void (*fn_MeshVtable8_t)(void*, void*);
    fn_MeshVtable8_t updateFn = *(fn_MeshVtable8_t*)(*(uintptr_t*)(s + 0x28) + 0x8);
    if (updateFn) {
        updateFn((void*)(s + 0x28), time);
    }

    // Check if still animating
    if (s_fn_MeshInstance_IsAnimating) {
        bool animating = s_fn_MeshInstance_IsAnimating((void*)(s + 0x28));
        if (!animating) {
            // Clear flag bit 2 (0x04) from flags at +0x20
            uint32_t oldFlags = *(uint32_t*)(s + 0x20);
            uint32_t newFlags = oldFlags & ~0x04u;
            *(uint32_t*)(s + 0x20) = newFlags;

            // If flags actually changed (low 16 bits) and owner is non-null, notify
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
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ModelComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[ModelComponent] Funcs resolved: SetMesh=%p IsAnim=%p SetTransform=%p "
           "AddObj=%p RemoveObj=%p Helper2b70=%p EntityNotify=%p\n",
           (void*)s_fn_MeshInstance_SetMesh, (void*)s_fn_MeshInstance_IsAnimating,
           (void*)s_fn_Geometry_SetTransform,
           (void*)s_fn_Scene_AddObject, (void*)s_fn_Scene_RemoveObject,
           (void*)s_fn_ModelHelper_2b70, (void*)s_fn_EntityNotify);
#endif
}
