// code/Game/Component/minecomponent.cpp
// Clean reimplementation of MineComponent lifecycle methods from Application-x64.exe.
//
// MineComponent (0x28 bytes) — simplest possible component, no additional fields.
// Vtable at 0x140159ed0. Only Shutdown has real logic.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/minecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

static bool s_initialized = false;

// ============================================================
// MineComponent::Shutdown (vtable slot [3], 0x14004f260)
// ============================================================
// Base component serialization + empty serializer block.

void MineComponent_Shutdown(MineComponent* self, void* stream) {
    // Slot-3 serializers are dispatched straight off the vtable by the entity
    // stream, so nothing else ever reaches this file. Resolving here is what
    // keeps the pointers below non-null -- when they were null the whole body
    // read ZERO bytes and silently desynced every object after it.
    MineComponent_InitFuncs();
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MineComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_initialized = true;
}

// ============================================================================
// ENDGAME B.3.8 VIS — MineComponent as a REAL class (MineComponent.h).
//
// Before: COMPONENT_INERT_FACTORY_SER — a 48-slot table filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal). After: the compiler's 16-slot
// vtable and a real ctor/dtor chain (B38_RULES D1, D2).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/MineComponent.h"

// 0x14004f2e0: Component() is the CALL 0x1401188b0; the 0x140159ed0 stamp is
// the compiler's; no own store.
MineComponent::MineComponent() {}

// [0] 0x140008fa0: ~MineComponent (0x14004f2a0: restamp + ~Internal), MemFree on &1.
Object::Base* MineComponent::DeletingDtor(unsigned flags) {
    this->~MineComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x14004f260 — Component::Persist + one empty Serializer scope.
void MineComponent::Persist(void* stream) { MineComponent_Shutdown(this, stream); }
// [6] 0x14004f250 / [7] 0x14004f240 — the class's own `RET` bodies.
void MineComponent::OnAdded() {}
void MineComponent::OnRemove() {}

// create 0x140008dd0: MemAlloc(0x28, 8), then the ctor.
void* MineComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MineComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MineComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* MineComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MineComponent());
}

DEFINE_EXE_VTABLE(MineComponent, "_ZTV13MineComponent")
DEFINE_EXE_TYPE(MineComponent, "Component")
