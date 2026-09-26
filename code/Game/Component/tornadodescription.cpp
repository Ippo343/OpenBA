// code/Game/Component/tornadodescription.cpp
// TornadoDescription (entity description block) — tornado disaster parameters.
// Extends Object::External (0x20 base). Vtable 0x14015ab00. Parent "Description".
// Size 0x38. GetVersion=1.
//
// Factory 0x14014b900-area (MemAlloc 0x38 + ctor sub_8e80). Ctor defaults:
//   float @ +0x20 = 1.0 (0x3f800000),  int @ +0x24 = 10
//   Time  @ +0x28 = 0   (8 bytes)
//   int   @ +0x30 = 1,  int @ +0x34 = 2
// Serialize (vtable +0x28, 0x140058d90), stream order (note: Time @ +0x28 is
// written AFTER the +0x30 int):
//   float @ +0x20
//   int   @ +0x24
//   int   @ +0x30
//   Time  @ +0x28   (System::Time::Serialize, 8 bytes)
//   int   @ +0x34
// GetVersion (vtable +0x18, 0x140058e30): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (TornadoDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015ab00) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/TornadoDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x14000d560 / createInPlace 0x14000d5a0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
TornadoDescription::TornadoDescription()
    : Description(),
      _f20(1.0f),
      _i24(10),
      _i30(1),
      _i34(2) {
    _time28._ticks = 0;
}

// [0] 0x14000d5e0: ~TornadoDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* TornadoDescription::DeletingDtor(unsigned flags) {
    this->~TornadoDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140058d90: stream order as retail.
bool TornadoDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_f20, nullptr);
    ser.Serialize(_i24, nullptr);
    ser.Serialize(_i30, nullptr);
    _time28.Serialize(ser);
    ser.Serialize(_i34, nullptr);
    return true;
}

// [6] 0x14000d5b0 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* TornadoDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void TornadoDescription::Destruct() {}

// [8] 0x14000d5d0: TornadoComponent's type id through the hidden return slot.
Object::Type* TornadoDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("TornadoComponent");
    return ret;
}

// [9] 0x14000d5a0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* TornadoDescription::Construct() {
    return new (static_cast<void*>(this)) TornadoDescription();
}

// create 0x14000d560: MemAlloc(0x38, 8), then the (inlined) ctor.
void* TornadoDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TornadoDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TornadoDescription());
}
// createInPlace 0x14000d5a0.
void* TornadoDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TornadoDescription());
}

DEFINE_EXE_VTABLE(TornadoDescription, "_ZTV18TornadoDescription")
DEFINE_EXE_TYPE(TornadoDescription, "Description")
