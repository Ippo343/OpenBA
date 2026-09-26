// code/Game/Component/tombstonedescription.cpp
// TombstoneDescription (entity description block) — cemetery marker parameters.
// Extends Object::External (0x20 base). Vtable 0x14015aa00. Parent "Description".
// Size 0x28. GetVersion=1.
//
// Factory 0x14014b8c0-area (MemAlloc 0x28). Ctor defaults:
//   float @ +0x20 = 360.0 (0x43b40000),  float @ +0x24 = 24.0 (0x41c00000)
// Serialize (vtable +0x28, 0x140057a20), stream order:
//   float @ +0x20,  float @ +0x24
// GetVersion (vtable +0x18, 0x140057a80): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (TombstoneDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015aa00) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/TombstoneDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x14000d2e0 / createInPlace 0x14000d350):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
TombstoneDescription::TombstoneDescription()
    : Description(),
      _f20(360.0f),
      _f24(24.0f) {}

// [0] 0x14000d420: ~TombstoneDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* TombstoneDescription::DeletingDtor(unsigned flags) {
    this->~TombstoneDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140057a20: stream order as retail.
bool TombstoneDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_f20, nullptr);
    ser.Serialize(_f24, nullptr);
    return true;
}

// [6] 0x14000d3b0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* TombstoneDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) TombstoneDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void TombstoneDescription::Destruct() { DeletingDtor(0); }

// [8] 0x14000d410: TombstoneComponent's type id through the hidden return slot.
Object::Type* TombstoneDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("TombstoneComponent");
    return ret;
}

// [9] 0x14000d350: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* TombstoneDescription::Construct() {
    return new (static_cast<void*>(this)) TombstoneDescription();
}

// create 0x14000d2e0: MemAlloc(0x28, 8), then the (inlined) ctor.
void* TombstoneDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TombstoneDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TombstoneDescription());
}
// createInPlace 0x14000d350.
void* TombstoneDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TombstoneDescription());
}

DEFINE_EXE_VTABLE(TombstoneDescription, "_ZTV20TombstoneDescription")
DEFINE_EXE_TYPE(TombstoneDescription, "Description")
