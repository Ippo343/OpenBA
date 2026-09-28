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

// graphics/Scene.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Scene.h"
#include "Component/HighlightComponent.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4a closing: the stream-scope ctor/dtor pair
#include <new>
#include <cstdint>
#include <cstdio>

#include <cstdlib>

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
void HighlightComponent::OnRemove() {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    // A null +0x28 vtable is the "fresh/inert" tell (same test Impl_c4a0 uses):
    // the embedded EdgeMeshInstance was never constructed, so nothing of ours is
    // in the Scene and +0x120 is not a real object id. Must precede the sceneId
    // read — an inert component's +0x120 is 0, not -1.
    if (*reinterpret_cast<void**>(s + 0x28) == nullptr) return;
    int32_t sceneId = *reinterpret_cast<int32_t*>(s + 0x120);
    if (sceneId < 0) return;
    if (GameUpdateLoop::_singleton == nullptr) return;
    void* scene = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0x60);
    if (!scene) return;
    reinterpret_cast<Graphics::Scene*>(scene)->RemoveObject(static_cast<uint32_t>(sceneId));   // B.3.5 bridge
    *reinterpret_cast<int32_t*>(s + 0x120) = -1;
}

// ============================================================================
// ENDGAME B.3.8 VIS — HighlightComponent as a REAL class (HighlightComponent.h).
//
// Before: a 48-slot HighlightComponent_InertVtable filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal), +0x120 = -1 patched in after the
// zero-fill. After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/HighlightComponent.h"

// 0x14004c930: Component() is the CALL 0x1401188b0; the 0x140159990 stamp is
// the compiler's. TREE DIVERGENCE: the EdgeMeshInstance / Transform are left
// zero (see the header) and flags get bit 1 (0x82). +0x120 = -1 ("not in
// scene") is retail's and load-bearing: slot 7 runs on EVERY entity teardown,
// and a 0 would make it Scene::RemoveObject(scene, 0) on whichever object
// genuinely owns id 0 (live freeze 2026-07-25).
HighlightComponent::HighlightComponent()
    : edgeMeshInstance{}, fieldF8(nullptr), transform{}, highlightFlag(0),
      _pad11D{}, highlightId(-1), _pad124{} {
    OrFlagsInCtor(0x2);
}

// [3] 0x14004c880 — Component::Persist (a direct CALL 0x1401189b0) PLUS its own
// (empty) Serializer scope. The scope is NOT decoration: the save encoding
// brackets every scope with sentinels, so an "empty" scope still occupies 16
// bytes (AAAAAAAA open, two dwords, BBBBBBBB close). Measured on the packaged
// tutorial save: retail's HighlightComponent record is 66 bytes, of which
// Component::Persist is 50 and this scope the remaining 16. Skipping it desyncs
// the stream for every object after this one.
void HighlightComponent::Persist(void* stream) {
    Component::Persist(stream);
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)
}

// [8] 0x14004c850 / [12] 0x14004c870. PORT GAP — B38 D9: never ported; the
// tree's table left them inert; kept inert here.
void HighlightComponent::Tick(void*) {}
void HighlightComponent::OnEvent(int) {}

// create 0x140006e40: MemAlloc(0x128, 8), then the ctor.
void* HighlightComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HighlightComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HighlightComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* HighlightComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HighlightComponent());
}

DEFINE_EXE_TYPE(HighlightComponent, "Component")
