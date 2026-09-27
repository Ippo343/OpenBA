// code/Game/Component/autopickupdescription.cpp
// AutoPickupDescription (entity description block) — auto-pickup parameters.
// Extends Object::External (0x20 base). Vtable 0x140158e70. Parent "Description".
// Size 0x28. GetVersion=1.
//
// Factory 0x14014a840-area (MemAlloc 0x28 + vtable + defaults). Defaults:
//   bool @ +0x20 = 1
// Serialize (vtable +0x28, 0x140044b60), stream order:
//   bool @ +0x20
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (AutoPickupDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140158e70) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/AutoPickupDescription.h"
#include "Component/AutoPickupComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140003420 / createInPlace 0x140003480):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
AutoPickupDescription::AutoPickupDescription()
    : Description(),
      _enabled(true) {}

// [0] 0x140003530: ~AutoPickupDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* AutoPickupDescription::DeletingDtor(unsigned flags) {
    this->~AutoPickupDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140044b60: stream order as retail.
bool AutoPickupDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_enabled, nullptr);
    return true;
}

// [6] 0x1400034d0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* AutoPickupDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) AutoPickupDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void AutoPickupDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140003520: AutoPickupComponent's type id through the hidden return slot.
Object::Type* AutoPickupDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = AutoPickupComponent::TypeId();
    return ret;
}

// [9] 0x140003480: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* AutoPickupDescription::Construct() {
    return new (static_cast<void*>(this)) AutoPickupDescription();
}

// create 0x140003420: MemAlloc(0x28, 8), then the (inlined) ctor.
void* AutoPickupDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AutoPickupDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AutoPickupDescription());
}
// createInPlace 0x140003480.
void* AutoPickupDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AutoPickupDescription());
}

DEFINE_EXE_VTABLE(AutoPickupDescription, "_ZTV21AutoPickupDescription")
DEFINE_EXE_TYPE(AutoPickupDescription, "Description")
