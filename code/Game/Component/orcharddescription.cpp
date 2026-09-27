// code/Game/Component/orcharddescription.cpp
// OrchardDescription (entity description block) — orchard farming parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a210. Parent "Description".
// Size 0x30. GetVersion=3.
//
// Factory 0x14014b440-area (MemAlloc 0x30 + vtable + defaults). Defaults:
//   int  @ +0x20 = 5, int @ +0x24 = 2, int @ +0x28 = 2 (8-byte store -> +0x2c=0)
// Serialize (vtable +0x28, 0x140052300), stream order:
//   int   @ +0x24
//   int   @ +0x28
//   float @ +0x2c
//   enum  @ +0x20   (serializer-encoding vtable +0x98 dispatch, version-gated)
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (OrchardDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015a210) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/OrchardDescription.h"
#include "Component/OrchardComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x1400098d0 / createInPlace 0x140009940):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
OrchardDescription::OrchardDescription()
    : Description(),
      _type(5),
      _i24(2),
      _i28(2),
      _f2c(0.0f) {}

// [0] 0x140009a10: ~OrchardDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* OrchardDescription::DeletingDtor(unsigned flags) {
    this->~OrchardDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140052300: stream order as retail.
bool OrchardDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_i24, nullptr);
    ser.Serialize(_i28, nullptr);
    ser.Serialize(_f2c, nullptr);
    if (ser.GetVersion() > 0) ser.Serialize(_type, nullptr);   // version-gated, as retail
    return true;
}

// [6] 0x1400099a0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* OrchardDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) OrchardDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void OrchardDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140009a00: OrchardComponent's type id through the hidden return slot.
Object::Type* OrchardDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = OrchardComponent::TypeId();
    return ret;
}

// [9] 0x140009940: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* OrchardDescription::Construct() {
    return new (static_cast<void*>(this)) OrchardDescription();
}

// create 0x1400098d0: MemAlloc(0x30, 8), then the (inlined) ctor.
void* OrchardDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(OrchardDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) OrchardDescription());
}
// createInPlace 0x140009940.
void* OrchardDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) OrchardDescription());
}

DEFINE_EXE_VTABLE(OrchardDescription, "_ZTV18OrchardDescription")
DEFINE_EXE_TYPE(OrchardDescription, "Description")
