// code/Game/Component/pasturedescription.cpp
// PastureDescription (entity description block) — pasture parameters.
// Extends Object::External (0x20 base). Vtable 0x140163ff0. Parent "Description".
// Size 0x28. GetVersion=2.
//
// Factory 0x14014f610-area (MemAlloc 0x28 + vtable + defaults). Defaults:
//   enum @ +0x20 = 5  (4-byte store)
// Serialize (vtable +0x28, 0x140128220), stream order:
//   enum @ +0x20   (serializer-encoding vtable +0x98 dispatch, version-gated)
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (PastureDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163ff0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/PastureDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x1400367a0 / createInPlace 0x140036800):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
PastureDescription::PastureDescription()
    : Description(),
      _type(5) {}

// [0] 0x1400368b0: ~PastureDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* PastureDescription::DeletingDtor(unsigned flags) {
    this->~PastureDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140128220: stream order as retail.
bool PastureDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    if (ser.GetVersion() > 0) ser.Serialize(_type, nullptr);   // version-gated, as retail
    return true;
}

// [6] 0x140036850 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* PastureDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void PastureDescription::Destruct() {}

// [8] 0x1400368a0: PastureComponent's type id through the hidden return slot.
Object::Type* PastureDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("PastureComponent");
    return ret;
}

// [9] 0x140036800: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* PastureDescription::Construct() {
    return new (static_cast<void*>(this)) PastureDescription();
}

// create 0x1400367a0: MemAlloc(0x28, 8), then the (inlined) ctor.
void* PastureDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PastureDescription());
}
// createInPlace 0x140036800.
void* PastureDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PastureDescription());
}

DEFINE_EXE_VTABLE(PastureDescription, "_ZTV18PastureDescription")
DEFINE_EXE_TYPE(PastureDescription, "Description")
