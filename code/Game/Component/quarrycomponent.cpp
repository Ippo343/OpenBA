// code/Game/Component/quarrycomponent.cpp
// Clean reimplementation of QuarryComponent lifecycle methods from Application-x64.exe.
//
// QuarryComponent (0x28 bytes) — minimal component, same as MineComponent.
// Vtable at 0x14015a490. Only Shutdown has real logic.
//

#include "Component/quarrycomponent_funcs.h"
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
// QuarryComponent::Shutdown (vtable slot [3], 0x140055230)
// ============================================================

void QuarryComponent_Shutdown(QuarryComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void QuarryComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_initialized = true;
}

// ============================================================================
// ENDGAME B.3.8 VIS — QuarryComponent as a REAL class (QuarryComponent.h).
//
// Before: a 48-slot QuarryComponent_LiveVtable re-filled at every create
// (InitInertTyped + InstallLive), Inert_ScalarDtor in slot 0. After: the
// compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2).
// The pit a quarry digs is a heightmap carve driven from slots 12 and 15 —
// bodies in quarrycomponent_deform.cpp.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/QuarryComponent.h"

extern "C" void QuarryComponent_Slot12(void* self, int msgId);
extern "C" void QuarryComponent_Slot15(void* self);

// 0x1400552b0: Component() is the CALL 0x1401188b0; the 0x14015a490 stamp is
// the compiler's; flags |= 3 (0x83 with Component's 0x80).
QuarryComponent::QuarryComponent() {
    OrFlagsInCtor(0x3);
}

// [0] 0x14000b5d0: ~QuarryComponent (0x140055270: restamp + ~Internal), MemFree on &1.
Object::Base* QuarryComponent::DeletingDtor(unsigned flags) {
    this->~QuarryComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [3] 0x140055230 — the body resolves its own helper first (slot 3 is the first
// call into the class on a load-from-save).
void QuarryComponent::Persist(void* stream) {
    QuarryComponent_InitFuncs();
    QuarryComponent_Shutdown(this, stream);
}
// [6] 0x140055220 / [7] 0x140055210 — the class's own `RET` bodies.
void QuarryComponent::OnAdded() {}
void QuarryComponent::OnRemove() {}
// [12] 0x1400550f0 — initial carve at _initialDepth on msg 4 (build done).
void QuarryComponent::OnEvent(int eventId) { QuarryComponent_Slot12(this, eventId); }
// [15] 0x140054b80 — progressive carve, sent by ConsumeProduce per finished cycle.
void QuarryComponent::OnMessage(int, void*, uint32_t, uint32_t) { QuarryComponent_Slot15(this); }

// create 0x14000b3d0: MemAlloc(0x28, 8), then the ctor.
void* QuarryComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(QuarryComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) QuarryComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* QuarryComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) QuarryComponent());
}

DEFINE_EXE_VTABLE(QuarryComponent, "_ZTV15QuarryComponent")
DEFINE_EXE_TYPE(QuarryComponent, "Component")
