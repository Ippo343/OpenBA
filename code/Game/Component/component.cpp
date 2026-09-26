// code/Game/Component/component.cpp — ENDGAME B.3.8 pilot
// The members of `Component` and `ControllerComponent` (Component.h,
// ControllerComponent.h). Retail addresses on every body.

#include "Component/Component.h"
#include "Component/ControllerComponent.h"
#include "Object/factory_registration.h"
#include "io/Serializer.h"

// ============================================================================
// Component
// ============================================================================

// 0x1401188b0. `Object::Internal()` is retail's `CALL [0x140152768]`
// (`??0Internal@Object@@QEAA@XZ`); the 0x140162920 stamp is the compiler's.
// The trailing `if (+0x10 != 0) { … 0x1400c3470 }` is dead (the field was
// zeroed three instructions earlier) and is not reproduced.
Component::Component()
    : Object::Internal(),
      _entity(nullptr),
      _description(nullptr),
      _flags(0x80),
      _pad24(0) {}

// [0] 0x140031cc0: stamp 0x140162920, `~Internal`, `if (flags & 1) MemFree`.
Object::Base* Component::DeletingDtor(unsigned flags) {
    this->~Component();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] 0x140031c40 / 0x140031c30 over Component's BaseTypeInfo. The abstract
// registration stays in code/Game/Object/factory_registrations_bulk.cpp (B38
// D4: moving it would move its record in the registration order), so the base
// reads its id by name. Only a half-built or half-destroyed object ever
// dispatches here; every leaf overrides both.
const char* Component::TypeName() const { return "Component"; }
uint32_t    Component::Type() const { return ::Object::GetTypeIdByName("Component"); }

// [3] 0x1401189b0 — the ported body (absorbresourcecomponent.cpp) is still
// registered at that address for its 52 by-address callers (B38 D5 (b) row);
// the member delegates to it, one body.
void Component::Persist(void* stream) { Component_SerializeReimpl(this, stream); }

// [4]-[15]: retail's shared no-op bodies (`RET`; [13] `XOR EAX,EAX; RET`).
void     Component::Startup() {}                                   // 0x140118a50
void     Component::Slot5() {}                                     // 0x140118a60
void     Component::OnAdded() {}                                   // 0x140118820
void     Component::OnRemove() {}                                  // 0x140118830
void     Component::Tick(void*) {}                                 // 0x140118a70
void     Component::Update(void*) {}                               // 0x140118a80
void     Component::PostUpdate() {}                                // 0x140118a90
void     Component::Slot11() {}                                    // 0x140118aa0
void     Component::OnEvent(int) {}                                // 0x140118ab0
uint32_t Component::CommandMask(int) { return 0; }                 // 0x140118ac0
void     Component::Command(int) {}                                // 0x140118ad0
void     Component::OnMessage(int, void*, uint32_t, uint32_t) {}   // 0x140118ae0

// ============================================================================
// ControllerComponent
// ============================================================================

// [0] 0x140032800: stamp 0x140162b80, stamp 0x140162920, `~Internal`, MemFree on &1.
Object::Base* ControllerComponent::DeletingDtor(unsigned flags) {
    this->~ControllerComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

const char* ControllerComponent::TypeName() const { return "ControllerComponent"; }       // 0x1400327f0
uint32_t    ControllerComponent::Type() const {                                           // 0x1400327e0
    return ::Object::GetTypeIdByName("ControllerComponent");
}

// [3] 0x1401195a0: `CALL 0x1401189b0` (Component::Persist, a direct call — a
// qualified base call), then one EMPTY Serializer(stream, 0) scope. An empty
// scope still occupies bytes in the save encoding, so it is not optional.
void ControllerComponent::Persist(void* stream) {
    Component::Persist(stream);
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)
}
