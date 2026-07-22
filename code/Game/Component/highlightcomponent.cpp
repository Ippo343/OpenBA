// code/Game/Component/highlightcomponent.cpp
// Clean reimplementation of HighlightComponent::Shutdown from Application-x64.exe.
//
// HighlightComponent (0x128 bytes) manages selection/highlight outlines around entities.
// Embeds Graphics::EdgeMeshInstance at +0x28 (0xD0 bytes) and Math::Transform at +0x100.
//
// Vtable at 0x140159990. Only non-trivial override:
//   Shutdown (0x14004c880) -- Component_Serialize + empty serializer (version 0)
//
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local, param_2, 0); ~Serializer(local)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/highlightcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Graphics::Scene::RemoveObject(scene, objectId) -- dispatch slot at 0x140151360
typedef void (*fn_Scene_RemoveObject_t)(void* scene, uint32_t objectId);
static fn_Scene_RemoveObject_t s_fn_Scene_RemoveObject = nullptr;

#include <cstdlib>
static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140151360);
    if (slot) s_fn_Scene_RemoveObject = reinterpret_cast<fn_Scene_RemoveObject_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// HighlightComponent::Shutdown (vtable slot [3], 0x14004c880)
// ============================================================
// Minimal: Component_Serialize + empty Serializer (version 0).

void HighlightComponent_Shutdown(HighlightComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HighlightComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HighlightComponent] Funcs resolved: Serialize=%p\n",
           (void*)s_fn_Component_Serialize);
#endif
}

// ============================================================
// HighlightComponent removed-from-world (vtable slot 7, +0x38)
// ============================================================
// The selection outline is an EdgeMeshInstance embedded at +0x28; when it is
// live in the Graphics::Scene its object id is at +0x120 (set by the tool-side
// Entity_impl_c4a0 / 0x14004c4a0 in tool_base_render.cpp). Retail's own +0x38 is
// a no-op (0x140118830) — retail removes the outline through the EditTool
// DESELECT path (cb40 -> c4a0 -> Scene::RemoveObject) which runs BEFORE the
// selected entity is freed.
//
// STANDALONE DEVIATION (required): our EditTool detects a dead selection only
// AFTER the EntityList drain has freed the entity block, so its cb40 reads an
// already-dead self+0x30 and never reaches this (freed) component to remove the
// outline. The dangling EdgeMeshInstance then stays in the Scene draw list and
// the next GameUpdateLoop::Draw faults in EdgeMeshInstance::Draw ->
// MeshInstance::GetNodeToWorld -> Transform::Lerp on freed node memory (live
// crash 2026-07-20, RDI inside the demolished yard-entity block).
//
// Fix: participate in the MarkForRemoval (0x1400c2ea0) +0x38 broadcast, which
// runs at mark time — while the component is still valid and BEFORE the drain
// frees it (and before that frame's Draw). This requires capability bit 1 (0x2)
// in the +0x20 flag word (the broadcast's per-component gate); HighlightComponent
// was the ONLY scene-object-owning component missing it (Model 0x83 / Storage
// Display 0xCB / StatusIcon 0xC2 all have it). No-ops when no outline is
// registered (+0x120 < 0), so it is safe on every non-selected teardown.
static void HighlightComponent_OnRemovedFromWorld(void* self) {
    EnsureHelpers();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    int32_t sceneId = *reinterpret_cast<int32_t*>(s + 0x120);
    if (sceneId < 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton || !s_fn_Scene_RemoveObject) return;
    void* scene = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x60);
    if (!scene) return;
    s_fn_Scene_RemoveObject(scene, static_cast<uint32_t>(sceneId));
    *reinterpret_cast<int32_t*>(s + 0x120) = -1;
    if (std::getenv("BANISHED_DESTROY_TRACE")) {
        std::fprintf(stderr, "[HighlightComp] removed-from-world: RemoveObject "
                     "outline sceneId=%d comp=%p\n", sceneId, self);
        std::fflush(stderr);
    }
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Manual expansion of COMPONENT_INERT_FACTORY with two deltas: capability flags
// 0x80 -> 0x82 (add removed-from-world broadcast bit 1) and slot 7 (+0x38) wired
// to the outline-remover above. Slot 7 is installed AFTER InitInertTyped, which
// re-stubs the shared per-type vtable on first fill (idempotent thereafter).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

static uint32_t HighlightComponent_InertGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HighlightComponent");
    return cached;
}
static void* HighlightComponent_InertVtable[::ComponentFactory::kInertVtableSlots];
static void HighlightComponent_InstallSlots() {
    HighlightComponent_InertVtable[7] =
        reinterpret_cast<void*>(&HighlightComponent_OnRemovedFromWorld);
}
extern "C" void* HighlightComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x128, 0x82,
        HighlightComponent_InertVtable,
        reinterpret_cast<void*>(&HighlightComponent_InertGetTypeId));
    HighlightComponent_InstallSlots();
    return p;
}
static void* HighlightComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x128, 0x82,
        HighlightComponent_InertVtable,
        reinterpret_cast<void*>(&HighlightComponent_InertGetTypeId));
    HighlightComponent_InstallSlots();
    return mem;
}
REGISTER_TYPE(HighlightComponent, "Component",
              &HighlightComponent_Factory, &HighlightComponent_CreateInPlace);
