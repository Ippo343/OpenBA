// code/Game/Component/component.cpp — ENDGAME B.3.8 pilot
// The members of `Component` and `ControllerComponent` (Component.h,
// ControllerComponent.h). Retail addresses on every body.

#include "Component/Component.h"
#include "Component/ControllerComponent.h"
#include "Component/Description.h"       // Description::TypeId() (ENDGAME B.3.9 RF)
#include "Entity/ComponentEntity.h"
#include "Game/serialize_externals.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "io/Serializer.h"
#include "resolve.h"
#include "Object/exe_type.h"   // DEFINE_EXE_ABSTRACT_TYPE

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

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(Component, "Object::Internal")

// [3] 0x1401189b0 — Component::Persist, the shared base serialize every
// Component subclass chains to. ENDGAME B.3.8 K1 folded the ported body here
// from absorbresourcecomponent.cpp. Retail: `Internal::Persist` (IAT
// 0x1401515e0), Serializer(stream, 0); the owner InternalBasePtr (+0x10)
// through InternalBasePtr::Serialize (IAT 0x140152aa0), nulled when it is not
// a ComponentEntity (IsDerivedFrom IAT 0x140152a98 against the ti cell
// 0x1401c5d70); the capability word (+0x20) through the uint door; the
// description ExternalPtr (+0x18) through 0x140031d10.
void Component::Persist(void* stream) {
    Object::Internal::Persist(stream);

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)

    reinterpret_cast<Object::InternalBasePtr*>(&_entity)->Serialize(ser);   // B.3.8 bridge (_entity is the retail InternalPtr)
    if (_entity && !::Object::IsDerivedFromNullSafe(_entity, ComponentEntity::TypeId())) {
        _entity = nullptr;
    }

    ser.Serialize(_flags, nullptr);
    Component_Helper1d10_Reimpl(&_description, &ser);
}

// 0x1401189b0 door: reached by address from 52 sites (fn tables, the inert
// component factories, the *_serialize.cpp bodies), registered in
// standalone/reg_component_economy.cpp. A qualified (non-virtual) call, so a
// hand-built receiver runs the same body.
void Component_SerializeReimpl(void* self, void* stream) {
    reinterpret_cast<Component*>(self)->Component::Persist(stream);   // B.3.8 bridge
}

// 0x140031d10 — Object::ExternalPtr<Description>::Serialize (the Component's
// +0x18 description reference; registered in standalone/reg_component_gap_B.cpp).
// Byte-identical to the other ExternalPtr<T> serialize bodies, so it delegates
// to the shared ExternalPtrSerializeGeneric with Description's ti cell
// 0x1401c72e0, read BY CELL as Description::TypeId() (ENDGAME B.3.9 RF: the
// abstract base has its record).
void Component_Helper1d10_Reimpl(void* extRefField, void* serializer) {
    const uint32_t typeId = Description::TypeId();
    ExternalPtrSerializeGeneric(reinterpret_cast<void**>(extRefField), serializer,
                                &typeId, "Description");
}

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

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(ControllerComponent, "Component")

// [3] 0x1401195a0: `CALL 0x1401189b0` (Component::Persist, a direct call — a
// qualified base call), then one EMPTY Serializer(stream, 0) scope. An empty
// scope still occupies bytes in the save encoding, so it is not optional.
void ControllerComponent::Persist(void* stream) {
    Component::Persist(stream);
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (stream is void* in Internal::Persist)
}
