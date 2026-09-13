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

#include "Component/modelcomponent_funcs.h"
#include "scene_watch.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "perf_experimental.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

// Graphics::MeshInstance::MeshInstance — default ctor, dispatch slot 0x140151490.
// Used by the real (Foundation F) ModelComponent factory to construct the
// embedded MeshInstance sub-object at +0x28.
typedef void* (*fn_MeshInstance_Ctor_t)(void* self);
static fn_MeshInstance_Ctor_t s_fn_MeshInstance_Ctor = nullptr;

// Graphics::MeshInstance::~MeshInstance — dispatch slot 0x1401514a8.
typedef void (*fn_MeshInstance_Dtor_t)(void* self);
static fn_MeshInstance_Dtor_t s_fn_MeshInstance_Dtor = nullptr;

// Math::Transform::Transform — default ctor (identity), dispatch slot 0x140151328.
typedef void* (*fn_Transform_Ctor_t)(void* self);
static fn_Transform_Ctor_t s_fn_Transform_Ctor = nullptr;

// ModelComponent_helper_2b70 (0x1400c2b70) — notifies entity of model change
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

    // Foundation F — sub-object ctors/dtor for the real ModelComponent factory.
    slot = ResolveAddr(Module::Exe, 0x140151490);
    if (slot) s_fn_MeshInstance_Ctor = *reinterpret_cast<fn_MeshInstance_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x1401514a8);
    if (slot) s_fn_MeshInstance_Dtor = *reinterpret_cast<fn_MeshInstance_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151328);
    if (slot) s_fn_Transform_Ctor = *reinterpret_cast<fn_Transform_Ctor_t*>(slot);

    // Direct exe addresses
    s_fn_ModelHelper_2b70 = reinterpret_cast<fn_ModelHelper_2b70_t>(
        ResolveAddr(Module::Exe, 0x1400c2b70));

    s_fn_EntityNotify = reinterpret_cast<fn_EntityNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));

    // Type ID pointer
    s_TypeId_MultiObjDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7a88));

    s_helpersResolved = true;
}

// ============================================================
// Reimpl of FUN_1400c2b70 (notify each child object of a model change)
// ============================================================
// Iterates the InternalBasePtr array {ptr@self+0x30, count@self+0x38}. For each
// entry: CheckPtr the IBP, then dispatch the object's vtable slot +0x60 with `param`.
// 1:1 port of the exe helper.
extern "C" void ModelComponentNotify_Reimpl(void* entity, int param) {
    uint8_t* self = reinterpret_cast<uint8_t*>(entity);
    int count = *(int*)(self + 0x38);
    int64_t* arr = *(int64_t**)(self + 0x30);
    for (int64_t i = 0; i < count; ++i) {
        // local copy of arr[i] (the child object pointer); CheckPtr resolves it.
        int64_t* obj = reinterpret_cast<int64_t*>(arr[i]);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&obj);
        void** vtbl = reinterpret_cast<void**>(*obj);
        typedef void (*slot60_t)(void* self, int param);
        reinterpret_cast<slot60_t>(vtbl[0x60 / 8])(obj, param);
    }
}

// ============================================================
// Helper: Get MultiObjectDescription sibling component
// ============================================================
static void* GetMultiObjDescription(uintptr_t self) {
    EnsureHelpers();

    void* obj = *(void**)(self + 0x18);
    if (!obj) return nullptr;

    // The standalone Runtime factory tree is incomplete, so fn_IsDerivedFrom
    // returns false for legit descriptions. ComponentEntity::CtorFromDescription
    // has already linked +0x18 to this component's ModelDescription template, so
    // trust the non-null pointer (matches the IsDerivedFromOrTrust policy used
    // throughout the spawn path).
    return obj;
}

// ============================================================
// Helper: Get mesh resource from description arrays
// ============================================================
// Description +0x20 = array of variant groups (each 0x18 bytes, sub-array ptr at +8)
// variant group[variantIdx] has sub-array at +8, indexed by modelVariant * 8
static void* GetMeshFromDescription(void* desc, int16_t variantIdx, int16_t modelVariant) {
    if (!desc || modelVariant < 0 || variantIdx < 0) return nullptr;
    uintptr_t d = (uintptr_t)desc;
    // desc+0x20 points to array of variant groups
    uintptr_t groupArray = *(uintptr_t*)(d + 0x20);
    if (!groupArray) return nullptr;            // description carries no meshes
    // Each group is 0x18 bytes; get the one at variantIdx
    uintptr_t group = *(uintptr_t*)(groupArray + 8 + (int64_t)variantIdx * 0x18);
    if (!group) return nullptr;
    // Index into sub-array by modelVariant
    void* meshResource = *(void**)(group + (int64_t)modelVariant * 8);
    return meshResource;
}

// ============================================================
// Per-entity model-variant pick (faithful core of FUN_1401253d0)
// ============================================================
// Citizens all render identically because ModelComponent::Serialize sets the
// variant index from a FIXED description field (+0x30) and the per-citizen pick
// that the original performs in the deferred citizen init (FUN_140117080 ->
// FUN_1401253d0) is guarded off in standalone. This helper reproduces the safe
// core of FUN_1401253d0: choose a random variant group within the description's
// valid range, then SetMesh the matching mesh into the MeshInstance (+0x28). It
// touches ONLY the same fields/SetMesh path ModelComponent::Serialize already
// uses for buildings — no risky vtable Draw/Remove calls.
//
// MUST be called AFTER the component's Serialize (so +0x166 is populated) and
// BEFORE the entity is registered (so the entity's OnAdded adds the new mesh to
// the scene). desc+0x28 is the variant-group count (the same field the building
// randomize path uses as its Rand range), so the pick is always in-bounds.
//
// Returns the chosen variant index, or -1 if no change was made. `forceIdx` picks
// a specific variant GROUP (e.g. gender: retail uses variantIdx=(gender==0)?1:0);
// pass -1 to keep the original random pick over the group count.
extern "C" int ModelComponent_RandomizeVariant(void* modelComp, int forceIdx) {
    EnsureHelpers();
    if (!modelComp || !s_fn_MeshInstance_SetMesh || !s_fn_Random_Rand) return -1;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return -1;
    uintptr_t s = (uintptr_t)modelComp;

    void* desc = GetMultiObjDescription(s);
    if (!desc) return -1;
    uintptr_t d = (uintptr_t)desc;

    // Variant-group count (desc+0x28). Nothing to randomize over <= 1 group.
    uint32_t count = *(uint32_t*)(d + 0x28);
    if (count <= 1) return -1;

    int16_t variantIdx;
    if (forceIdx >= 0) {
        variantIdx = (int16_t)((uint32_t)forceIdx % count);   // clamp into range
    } else {
        void* randomPtr = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
        if (!randomPtr) return -1;
        variantIdx = (int16_t)s_fn_Random_Rand(randomPtr, count);
    }
    *(int16_t*)(s + 0x164) = variantIdx;

    // Keep the model variant Serialize already chose (+0x166); skip if it has no
    // renderable mesh ( < 0 ).
    int16_t modelVariant = *(int16_t*)(s + 0x166);
    if (modelVariant < 0) return -1;

    void* meshResource = GetMeshFromDescription(desc, variantIdx, modelVariant);
    if (!meshResource) return -1;
    s_fn_MeshInstance_SetMesh((void*)(s + 0x28), meshResource);
    return variantIdx;
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
    void* meshResource = nullptr;
    if (modelVariant >= 0 && s_fn_MeshInstance_SetMesh) {
        meshResource = GetMeshFromDescription(
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
                SceneWatch_Note(id, (void*)(s + 0x28), "ModelComponent::Draw");
#if defined(_DEBUG)
                static int s_drawLog = 0;
                if (s_drawLog < 10) {
                    ++s_drawLog;
                    fprintf(stderr, "[ModelComponent::OnAdded] self=%p scene=%p "
                            "modelVariant=%d sceneObjId=%u\n",
                            (void*)self, scene, (int)modelVariant, id);
                    fflush(stderr);
                }
#endif
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
        // [PerfX ANIM_CULL -- NON-RETAIL] On the last speed-iteration retail poses every
        // animated mesh (AnimationState::UpdateNodes). A mesh outside the camera frustum
        // is never drawn, so its pose is left root-only (the animation clock and the
        // root-motion offset still advance exactly as retail's intermediate iterations
        // do); PerfX flushes the pose with a zero-time update if the sim pauses, and the
        // first visible tick recomputes it from the animation time. Off by default
        // outside the perf build (BANISHED_PERF_ANIM_CULL=0 kills it).
        // [PerfX ANIM_BATCH -- NON-RETAIL] at speed N retail runs N root-only updates per
        // frame per model only to sample the root-motion offset every tick. For a model
        // whose offset comes from the AnimationManager pool (instanced animation, ticked
        // every iteration anyway) or is zero (no root motion) the intermediate ones are
        // skipped and the last iteration advances the animation by the skipped ticks in
        // one call: the clock, the drawn pose and the walk speed are exact. A blending or
        // non-instanced root-motion model is ticked normally (PerfX::AnimBatchDecide).
        int64_t batchedTime = 0;
        if (PerfX::AnimBatchDecide(self, time, &batchedTime)) return;
        void* timeArg = &batchedTime;
        if (PerfX::AnimCullSkip(self)) {
            uint8_t* flag = PerfX::RootOnlyFlag();
            *flag = 1;
            updateFn((void*)(s + 0x28), timeArg);
            *flag = 0;
        } else {
            updateFn((void*)(s + 0x28), timeArg);
        }
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

// ===== Standalone factory (Foundation F — real renderable ModelComponent) =====
// Unlike the inert components (Foundation D/E), ModelComponent graduates to a
// REAL factory: it constructs the embedded Graphics::MeshInstance (+0x28) and
// Math::Transform (+0x148) sub-objects and installs a real vtable whose
// lifecycle slots route to the reimplemented methods above. The spawn chain
// then drives rendering for free — ComponentEntity::CtorFromDescription calls
// vtable+0x20 (Startup = ModelComponent_Serialize, sets the mesh from the
// description) during instantiation, and OnAddedPass calls vtable+0x30
// (OnAdded = ModelComponent_Draw, registers the geometry in Graphics::Scene).
// Mirrors the exe constructor at 0x140124d20.
#include "Object/factory_registration.h"

// GetTypeId (vtable +0x10): return the ModelComponent type id, cached.
static uint32_t ModelComponent_Real_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ModelComponent");
    return cached;
}

// ScalarDtor (vtable +0x00): destroy the embedded MeshInstance, unregister from the
// InternalFactory handle table, free if deleting.
//
// The ctor below runs the real Object::Internal::Internal, so this MUST run the matching
// ~Internal — otherwise the freed block stays in the handle table and the next unrelated
// ~Internal swap-removes it, writing `*(int*)(freedPtr+0x08) = idx` into a block the
// allocator has already reused. Caught live by ObjAudit: table slots 325/346/352 held
// stale ModelComponents whose memory had been recycled into SendComponentMessage actions.
// The IsRegistered guard keeps a never-registered instance from swap-removing through a
// garbage index. (See Object/internal_factory_audit.h.)
static void* ModelComponent_Real_ScalarDtor(void* self, uint32_t flags) {
    EnsureHelpers();
    if (self && s_fn_MeshInstance_Dtor)
        s_fn_MeshInstance_Dtor((void*)((uintptr_t)self + 0x28));
    if (self && fn_ObjectInternal_Dtor && ObjAudit::IsRegistered(self)) {
        fn_ObjectInternal_Dtor(self);
    } else {
        ObjAudit::NoteUnregisteredFree(self, "ModelComponent");
    }
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Safe no-op for every slot we don't override (base Component behaviour). Returns
// 0 in RAX, which reads correctly as void/int/pointer under the Win64 ABI.
static void* ModelComponent_Real_Stub(void*, ...) { return nullptr; }

// 24 slots — comfortable headroom over the original 19-slot vtable (0x140163ba0).
static void* g_ModelComponent_RealVtable[24];
static bool  g_mcVtableInit = false;

static void EnsureRealVtable() {
    if (g_mcVtableInit) return;
    for (int i = 0; i < 24; ++i)
        g_ModelComponent_RealVtable[i] = reinterpret_cast<void*>(&ModelComponent_Real_Stub);
    g_ModelComponent_RealVtable[0] = reinterpret_cast<void*>(&ModelComponent_Real_ScalarDtor); // +0x00
    g_ModelComponent_RealVtable[2] = reinterpret_cast<void*>(&ModelComponent_Real_GetTypeId);  // +0x10
    g_ModelComponent_RealVtable[3] = reinterpret_cast<void*>(&ModelComponent_Shutdown);        // +0x18 stream serialize
    g_ModelComponent_RealVtable[4] = reinterpret_cast<void*>(&ModelComponent_Serialize);       // +0x20 Startup (set mesh)
    g_ModelComponent_RealVtable[6] = reinterpret_cast<void*>(&ModelComponent_Draw);            // +0x30 OnAdded (scene add)
    g_ModelComponent_RealVtable[7] = reinterpret_cast<void*>(&ModelComponent_OnCreate);        // +0x38
    g_ModelComponent_RealVtable[8] = reinterpret_cast<void*>(&ModelComponent_OnRemove);        // +0x40
    g_mcVtableInit = true;
}

// In-place construct a real ModelComponent at `self` (0x170 bytes). Mirrors the
// exe ctor 0x140124d20: Internal base, our vtable, MeshInstance(+0x28),
// Transform(+0x148), variant/scale = 0, modelId = -1, flags |= 0x83.
static void* ModelComponent_InitReal(void* self) {
    EnsureHelpers();
    EnsureRealVtable();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);

    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    *(void**)s          = g_ModelComponent_RealVtable;  // +0x00 our vtable
    *(void**)(s + 0x10) = nullptr;                       // +0x10 owner IBP (set by CtorFromDescription)
    *(void**)(s + 0x18) = nullptr;                       // +0x18 description (set by CtorFromDescription)
    *(uint32_t*)(s + 0x20) = 0x83;                       // +0x20 capability flags

    // Zero the component body, then construct the embedded sub-objects.
    std::memset(s + 0x28, 0, 0x170 - 0x28);
    if (s_fn_MeshInstance_Ctor) s_fn_MeshInstance_Ctor(s + 0x28);   // +0x28 Graphics::MeshInstance
    if (s_fn_Transform_Ctor)    s_fn_Transform_Ctor(s + 0x148);     // +0x148 Math::Transform
    *(int64_t*)(s + 0x164) = 0;                          // variantIdx / modelVariant / scale
    *(int32_t*)(s + 0x16c) = -1;                         // modelId = not in scene
    return self;
}

extern "C" void* ModelComponent_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)0x170, 8);
    if (!p) return nullptr;
    return ModelComponent_InitReal(p);
}

static void* ModelComponent_CreateInPlace(void* mem) {
    return mem ? ModelComponent_InitReal(mem) : nullptr;
}

REGISTER_TYPE(ModelComponent, "Component",
              &ModelComponent_Factory, &ModelComponent_CreateInPlace);
