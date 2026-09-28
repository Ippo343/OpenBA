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

#include "graphics/Scene.h"
#include "graphics/Mesh.h"
#include "Component/modelcomponent_funcs.h"
#include "scene_watch.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "Component/ModelDescription.h"   // ENDGAME B.3.9 CB: 0x1401c7a88 cell class
#include "perf_experimental.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <atomic>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
// graphics/*.h are included at the TOP of this file (before resolve.h) because
// they pull runtime/types/volume/Volume.h, whose global-scope `struct Rectangle`
// is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has been seen.

// ============================================================
// Resolved helpers
// ============================================================

// System::Time::Time(Time*, float) — dispatch slot at 0x140152b00
typedef void (*fn_Time_Ctor_t)(void* self, float seconds);
static fn_Time_Ctor_t s_fn_Time_Ctor = nullptr;

// ENDGAME B.3.9 GFX (coordinator ruling 4, B9-13 foreign site): the
// embedded Graphics::MeshInstance at +0x28 is built and destroyed by the real
// class (`::new` / `->~MeshInstance()`), not through import slots 0x140151490
// / 0x1401514a8, so those two resolutions are gone from this file.

// ModelComponent_helper_2b70 (0x1400c2b70) — notifies entity of model change
typedef void (*fn_ModelHelper_2b70_t)(void* entity, int param);
static fn_ModelHelper_2b70_t s_fn_ModelHelper_2b70 = nullptr;

// FUN_1400c3470 — entity notification callback
typedef void (*fn_EntityNotify_t)(void* entity);
static fn_EntityNotify_t s_fn_EntityNotify = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    // Dispatch slots — read function pointer from .rdata slot at runtime

    slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_Time_Ctor = *reinterpret_cast<fn_Time_Ctor_t*>(slot);

    // Direct exe addresses
    s_fn_ModelHelper_2b70 = reinterpret_cast<fn_ModelHelper_2b70_t>(
        ResolveAddr(Module::Exe, 0x1400c2b70));

    s_fn_EntityNotify = reinterpret_cast<fn_EntityNotify_t>(
        ResolveAddr(Module::Exe, 0x1400c3470));

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
        reinterpret_cast<Component*>(obj)->OnEvent(param);   // B.3.8 bridge (slot 12, +0x60; an entity component)
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
    if (!modelComp) return -1;
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
        variantIdx = (int16_t)reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(count));   // B.3.4b bridge
    }
    *(int16_t*)(s + 0x164) = variantIdx;

    // Keep the model variant Serialize already chose (+0x166); skip if it has no
    // renderable mesh ( < 0 ).
    int16_t modelVariant = *(int16_t*)(s + 0x166);
    if (modelVariant < 0) return -1;

    void* meshResource = GetMeshFromDescription(desc, variantIdx, modelVariant);
    if (!meshResource) return -1;
    reinterpret_cast<Graphics::MeshInstance*>(s + 0x28)
        ->SetMesh(Graphics::ExternalPtrHandle{meshResource});   // B.3.5 bridge
    return variantIdx;
}






// ============================================================
// Init: resolve function pointers
// ============================================================
void ModelComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    // The six Graphics doors this used to print are members now (B.3.5 caller
    // batch I2); only the two exe helpers are still resolved pointers.
    printf("[ModelComponent] Funcs resolved: Helper2b70=%p EntityNotify=%p\n",
           (void*)s_fn_ModelHelper_2b70, (void*)s_fn_EntityNotify);
#endif
}

// ============================================================================
// ENDGAME B.3.8 VIS — ModelComponent as a REAL class (ModelComponent.h).
//
// Before: a 24-slot g_ModelComponent_RealVtable filled on first use, every
// unset slot a file-local stub, ModelComponent_InitReal as the ctor. After: the
// compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2). The
// spawn chain still drives rendering: ComponentEntity::CtorFromDescription
// calls slot 4 (Startup = ModelComponent_Serialize, sets the mesh from the
// description) and OnAddedPass calls slot 6 (OnAdded = ModelComponent_Draw,
// registers the geometry in Graphics::Scene).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/ModelComponent.h"

// 0x140124d20: Component() is the CALL 0x1401188b0 (the Object::Internal base
// registers the object — the dtor's ~Internal must match it); the 0x140163ba0
// stamp is the compiler's. The body is zeroed, the MeshInstance door builds
// +0x28, Transform() builds +0x148, variant/scale = 0, modelId = -1,
// flags |= 0x83.
ModelComponent::ModelComponent()
    : meshInstance{}, transform{}, _pad164{}, modelId(-1) {
    EnsureHelpers();
    ::new (static_cast<void*>(meshInstance)) Graphics::MeshInstance();   // +0x28, retail CALL [??0MeshInstance] (B.3.9 GFX)
    ::new (static_cast<void*>(transform)) Math::Transform();   // +0x148 Math::Transform (slot 0x140151328)   // B.3.1 bridge
    OrFlagsInCtor(0x83);
}

// 0x140124cd0: ~MeshInstance(+0x28); the compiler then chains ~Component and
// ~Internal (the old slot 0 guarded ~Internal with ObjAudit::IsRegistered for
// never-registered instances; the real ctor always registers).
ModelComponent::~ModelComponent() {
    EnsureHelpers();
    // retail CALL [??1MeshInstance] on +0x28 -- a destructor call, no delete (B.3.9 GFX).
    reinterpret_cast<Graphics::MeshInstance*>(meshInstance)->~MeshInstance();
}

// ============================================================
// ModelComponent::Persist (vtable slot [3], 0x140124b70)
// ============================================================
// Serializes mesh state: variant indices, transform, scale, mesh instance data,
// and a boolean flag for whether modelId is valid.
//
// On load (serializer direction == 0): if variant >= 0, calls SetMesh to
// restore the mesh from the description resource arrays.
// Also conditionally calls vtable Draw (+0x30) to re-add to scene.

void ModelComponent::Persist(void* stream) {
    EnsureHelpers();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Base class serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Create IO::Serializer (version 0)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize variant index (+0x164 as short) and model variant (+0x166 as short)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<short*>(s + 0x164), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<short*>(s + 0x166), nullptr);   // B.3.4 bridge

    // Serialize Math::Transform at +0x148 (slot 0x1401522e0)
    reinterpret_cast<Math::Transform*>(s + 0x148)->Serialize(serializer);   // B.3.1 bridge

    // Serialize scale float at +0x168
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x168), nullptr);   // B.3.4 bridge

    // On load: if model variant >= 0, restore mesh from description
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        int16_t modelVariant = *(int16_t*)(s + 0x166);
        if (modelVariant >= 0) {
            void* desc = GetMultiObjDescription(s);
            void* meshResource = GetMeshFromDescription(
                desc, *(int16_t*)(s + 0x164), modelVariant);
            if (meshResource) {
                reinterpret_cast<Graphics::MeshInstance*>(s + 0x28)
                    ->SetMesh(Graphics::ExternalPtrHandle{meshResource});   // B.3.5 bridge
            }
        }
    }

    // Serialize MeshInstance sub-object
    reinterpret_cast<Graphics::MeshInstance*>(s + 0x28)
        ->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.5 bridge

    // Serialize bool flag: modelId >= 0
    bool hasModelId = (modelId >= 0);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(hasModelId, nullptr);   // B.3.4 bridge

    // On load: if flag was set, call vtable Draw (+0x30) to re-register in scene
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0 && hasModelId) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        // Slot 6 (+0x30, OnAdded) on this, a virtual dispatch in retail.
        OnAdded();
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
// ============================================================
// ModelComponent::Startup (vtable slot [4], 0x140124ac0)
// ============================================================
// Reads building data from MultiObjectDescription, sets variant indices,
// optionally randomizes variant, calls SetMesh if variant >= 0.

void ModelComponent::Startup() {
    EnsureHelpers();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    void* desc = GetMultiObjDescription(s);
    if (!desc) return;
    uintptr_t d = (uintptr_t)desc;

    // Read variant index from description +0x30 (as short -> stored at +0x164)
    *(int16_t*)(s + 0x164) = *(int16_t*)(d + 0x30);

    // If description +0x38 (byte flag) is set, randomize the variant index
    if (*(uint8_t*)(d + 0x38) != 0) {
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            void* randomPtr = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0xd8);
            uint32_t range = *(uint32_t*)(d + 0x28);
            uint32_t randVal = reinterpret_cast<System::Random*>(randomPtr)->Rand(static_cast<uint32_t>(range));   // B.3.4b bridge
            *(int16_t*)(s + 0x164) = (int16_t)randVal;
        }
    }

    // Read model variant from description +0x34
    int16_t modelVariant = *(int16_t*)(d + 0x34);
    *(int16_t*)(s + 0x166) = modelVariant;

    // If variant >= 0, set the mesh
    void* meshResource = nullptr;
    if (modelVariant >= 0) {
        meshResource = GetMeshFromDescription(
            desc, *(int16_t*)(s + 0x164), modelVariant);
        if (meshResource) {
            reinterpret_cast<Graphics::MeshInstance*>(s + 0x28)
                ->SetMesh(Graphics::ExternalPtrHandle{meshResource});   // B.3.5 bridge
        }
    }

}
// ============================================================
// ModelComponent::OnAdded (vtable slot [6], 0x140124a30)
// ============================================================
// Adds mesh geometry to the Graphics::Scene for rendering.
// Only acts if variant >= 0 and not yet in scene (modelId < 0).

#if defined(_DEBUG)
// [ModelComponent::OnAdded] trace budget (B6-5: namespace-scope atomic).
static std::atomic<int> s_modelDrawLog{0};
#endif

void ModelComponent::OnAdded() {
    EnsureHelpers();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    int16_t modelVariant = *(int16_t*)(s + 0x166);
    int32_t modelId = *(int32_t*)(s + 0x16c);

    if (modelVariant >= 0 && modelId < 0) {
        // IsDerivedFrom check (result unused in original — just side effect?)
        // cell 0x1401c7a88 = ModelDescription (the old "MultiObjDesc" name lied)
        if (fn_IsDerivedFrom) {
            fn_IsDerivedFrom(*(void**)(s + 0x18), ModelDescription::TypeId());
        }

        // Set transform on the geometry sub-object
        reinterpret_cast<Graphics::Geometry*>(s + 0x28)
            ->SetTransform(*reinterpret_cast<const Math::Transform*>(s + 0x148));   // B.3.5 bridge

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
        if (g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene) {
                uint32_t id = reinterpret_cast<Graphics::Scene*>(scene)
                    ->AddObject(reinterpret_cast<Graphics::Geometry*>(s + 0x28));   // B.3.5 bridge
                *(uint32_t*)(s + 0x16c) = id;
                SceneWatch_Note(id, (void*)(s + 0x28), "ModelComponent::Draw");
#if defined(_DEBUG)
                if (s_modelDrawLog.fetch_add(1, std::memory_order_relaxed) < 10) {
                    fprintf(stderr, "[ModelComponent::OnAdded] self=%p scene=%p "
                            "modelVariant=%d sceneObjId=%u\n",
                            (void*)this, scene, (int)modelVariant, id);
                    fflush(stderr);
                }
#endif
            }
        }
    }
}
// ============================================================
// ModelComponent::OnRemove (vtable slot [7], 0x1401249d0)
// ============================================================
// Removes mesh from Scene and resets modelId. Notifies owner entity.

void ModelComponent::OnRemove() {
    EnsureHelpers();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    int16_t modelVariant = *(int16_t*)(s + 0x166);
    int32_t modelId = *(int32_t*)(s + 0x16c);

    if (modelVariant >= 0 && modelId >= 0) {
        // Remove from scene
        if (g_pGameWorldSingleton) {
            void* scene = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x60);
            if (scene) {
                reinterpret_cast<Graphics::Scene*>(scene)
                    ->RemoveObject((uint32_t)modelId);   // B.3.5 bridge
            }
        }

        // Reset modelId to -1
        *(int32_t*)(s + 0x16c) = -1;

        // Check owner and notify
        void* ownerEntity = *(void**)(s + 0x10);
        if (ownerEntity && s_fn_ModelHelper_2b70) {
            s_fn_ModelHelper_2b70(ownerEntity, 0);
        }
    }
}
// ============================================================
// ModelComponent::Tick (vtable slot [8], 0x140124960)
// ============================================================
// Calls MeshInstance vtable +0x8 (Update), checks IsAnimating.
// If not animating, clears draw flag bit 2 from +0x20, notifies entity.
// NOTE: RDX (time) is forwarded implicitly by the caller.

void ModelComponent::Tick(void* time) {
    EnsureHelpers();
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

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
        if (PerfX::AnimBatchDecide(this, time, &batchedTime)) return;
        void* timeArg = &batchedTime;
        if (PerfX::AnimCullSkip(this)) {
            uint8_t* flag = PerfX::RootOnlyFlag();
            *flag = 1;
            updateFn((void*)(s + 0x28), timeArg);
            *flag = 0;
        } else {
            updateFn((void*)(s + 0x28), timeArg);
        }
    }

    // Check if still animating
    {
        bool animating = reinterpret_cast<const Graphics::MeshInstance*>(s + 0x28)
                             ->IsAnimating();   // B.3.5 bridge
        if (!animating) {
            // Clear flag bit 2 (0x04) from flags at +0x20
            uint32_t oldFlags = *(uint32_t*)(s + 0x20);
            uint32_t newFlags = oldFlags & ~0x04u;
            *(uint32_t*)(s + 0x20) = newFlags;

            // If flags actually changed (low 16 bits) and owner is non-null, notify
            if (((newFlags ^ oldFlags) & 0xFFFF) != 0) {
                void* ownerEntity = *(void**)(s + 0x10);
                if (ownerEntity != nullptr) {
                    if (s_fn_EntityNotify) {
                        s_fn_EntityNotify(*(void**)(s + 0x10));
                    }
                }
            }
        }
    }
}

// [0] 0x140035ad0: ~ModelComponent (0x140124cd0: ~MeshInstance(+0x28) + ~Internal), MemFree on &1.
Object::Base* ModelComponent::DeletingDtor(unsigned flags) {
    this->~ModelComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400358d0: MemAlloc(0x170, 8), then the ctor.
void* ModelComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ModelComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ModelComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ModelComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ModelComponent());
}

DEFINE_EXE_TYPE(ModelComponent, "Component")
