// code/Game/Component/firedescription.cpp
// FireDescription (entity description block) — fire disaster/spread parameters.
// Extends Object::External (0x20 base). Vtable 0x140159400. Parent "Description".
// Size 0x50. GetVersion=4.
//
// Factory 0x14014ac00-area (MemAlloc 0x50 + ctor sub_9d60). Ctor defaults:
//   float @ +0x20 = 1.0,  (+0x24 = 0)
//   int   @ +0x28 = 1,    (+0x2c = 0)
//   float @ +0x30 = 0.25, float @ +0x34 = 0.5
//   int   @ +0x38 = 8,    int   @ +0x3c = 100,  int @ +0x40 = 50 (0x32)
//   float @ +0x44 = 1.0,  float @ +0x48 = 1.0
// Serialize (vtable +0x28, 0x140049c00), stream order (note non-monotone):
//   float @ +0x20,  float @ +0x24
//   int   @ +0x28,  int   @ +0x2c
//   float @ +0x30,  float @ +0x34
//   int   @ +0x3c
//   float @ +0x44,  float @ +0x48
//   int   @ +0x38,  int   @ +0x40
// GetVersion (vtable +0x18, 0x140049d10): mov eax,4; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (FireDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159400) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/FireDescription.h"
#include "Component/FireComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140005640 / createInPlace 0x140005680):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
FireDescription::FireDescription()
    : Description(),
      _f20(1.0f),
      _f24(0.0f),
      _i28(1),
      _i2c(0),
      _f30(0.25f),
      _f34(0.5f),
      _i38(8),
      _i3c(100),
      _i40(50),
      _f44(1.0f),
      _f48(1.0f),
      _i4c(0) {}

// [0] 0x1400056c0: ~FireDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* FireDescription::DeletingDtor(unsigned flags) {
    this->~FireDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140049c00: stream order as retail.
bool FireDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_f20, nullptr);
    ser.Serialize(_f24, nullptr);
    ser.Serialize(_i28, nullptr);
    ser.Serialize(_i2c, nullptr);
    ser.Serialize(_f30, nullptr);
    ser.Serialize(_f34, nullptr);
    ser.Serialize(_i3c, nullptr);
    ser.Serialize(_f44, nullptr);
    ser.Serialize(_f48, nullptr);
    ser.Serialize(_i38, nullptr);
    ser.Serialize(_i40, nullptr);
    return true;
}

// [6] 0x140005690: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* FireDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) FireDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void FireDescription::Destruct() { DeletingDtor(0); }

// [8] 0x1400056b0: FireComponent's type id through the hidden return slot.
Object::Type* FireDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = FireComponent::TypeId();
    return ret;
}

// [9] 0x140005680: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* FireDescription::Construct() {
    return new (static_cast<void*>(this)) FireDescription();
}

// create 0x140005640: MemAlloc(0x50, 8), then the (inlined) ctor.
void* FireDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FireDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FireDescription());
}
// createInPlace 0x140005680.
void* FireDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FireDescription());
}

DEFINE_EXE_TYPE(FireDescription, "Description")
