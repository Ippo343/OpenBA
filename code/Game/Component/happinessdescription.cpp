// code/Game/Component/happinessdescription.cpp
// HappinessDescription (entity description block) — happiness-effect parameters
// (ToolKit: type / radius / idle range). Extends Object::External (0x20 base).
// Vtable 0x140159660. Parent "Description". Size 0x30. GetVersion=3.
//
// Factory 0x14014ad00-area (MemAlloc 0x30). Ctor defaults:
//   int   @ +0x20 = 5     (HappinessType enum)
//   short @ +0x24 = 0     (two bools @ +0x24/+0x25)
//   ptr   @ +0x28 = 0     (+0x28 enum, +0x2c int)
// Serialize (vtable +0x28, 0x14004acb0), stream order:
//   enum @ +0x20  (serializer encoding vtable +0x98)
//   bool @ +0x24
//   bool @ +0x25
//   enum @ +0x28  (serializer encoding vtable +0x98)
//   int  @ +0x2c
// GetVersion (vtable +0x18, 0x14004ad50): mov eax,3; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (HappinessDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159660) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/HappinessDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140005c10 / createInPlace 0x140005c80):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
HappinessDescription::HappinessDescription()
    : Description(),
      _happinessType(5),
      _b24(false),
      _b25(false),
      _u28(0),
      _i2c(0) {}

// [0] 0x140005d50: ~HappinessDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* HappinessDescription::DeletingDtor(unsigned flags) {
    this->~HappinessDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14004acb0: stream order as retail.
bool HappinessDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_happinessType, nullptr);
    ser.Serialize(_b24, nullptr);
    ser.Serialize(_b25, nullptr);
    ser.Serialize(_u28, nullptr);
    ser.Serialize(_i2c, nullptr);
    return true;
}

// [6] 0x140005ce0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* HappinessDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) HappinessDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void HappinessDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140005d40: HappinessComponent's type id through the hidden return slot.
Object::Type* HappinessDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("HappinessComponent");
    return ret;
}

// [9] 0x140005c80: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* HappinessDescription::Construct() {
    return new (static_cast<void*>(this)) HappinessDescription();
}

// create 0x140005c10: MemAlloc(0x30, 8), then the (inlined) ctor.
void* HappinessDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HappinessDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HappinessDescription());
}
// createInPlace 0x140005c80.
void* HappinessDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HappinessDescription());
}

DEFINE_EXE_VTABLE(HappinessDescription, "_ZTV20HappinessDescription")
DEFINE_EXE_TYPE(HappinessDescription, "Description")
