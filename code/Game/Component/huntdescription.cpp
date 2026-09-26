// code/Game/Component/huntdescription.cpp
// HuntDescription (entity description block) — hunting lodge parameters.
// Extends Object::External (0x20 base). Vtable 0x140163820. Parent "Description".
// Size 0x28. GetVersion=3 (slot +0x18 @ 0x1401229e0: mov eax,3; ret).
//
// Ctor defaults (HuntDescription_Factory):
//   float    @ +0x20 = 0   (not written by factory; left zero from base)
//   int/enum @ +0x24 = 5
// Serialize (vtable +0x28, 0x140122960) — Serializer ctor with version=1, stream order:
//   float @ +0x20
//   if (serializer.GetVersion() > 0): enum @ +0x24 (encoding vcall +0x98)
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (HuntDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163820) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/HuntDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140034f90 / createInPlace 0x140034ff0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
HuntDescription::HuntDescription()
    : Description(),
      _f20(0.0f),
      _type(5) {}

// [0] 0x1400350a0: ~HuntDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* HuntDescription::DeletingDtor(unsigned flags) {
    this->~HuntDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140122960: stream order as retail.
bool HuntDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_f20, nullptr);
    if (ser.GetVersion() > 0) ser.Serialize(_type, nullptr);   // version-gated, as retail
    return true;
}

// [6] 0x140035040: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* HuntDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) HuntDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void HuntDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140035090: HuntComponent's type id through the hidden return slot.
Object::Type* HuntDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("HuntComponent");
    return ret;
}

// [9] 0x140034ff0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* HuntDescription::Construct() {
    return new (static_cast<void*>(this)) HuntDescription();
}

// create 0x140034f90: MemAlloc(0x28, 8), then the (inlined) ctor.
void* HuntDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HuntDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HuntDescription());
}
// createInPlace 0x140034ff0.
void* HuntDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HuntDescription());
}

DEFINE_EXE_VTABLE(HuntDescription, "_ZTV15HuntDescription")
DEFINE_EXE_TYPE(HuntDescription, "Description")
