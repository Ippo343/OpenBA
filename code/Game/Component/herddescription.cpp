// code/Game/Component/herddescription.cpp
// HerdDescription (entity description block) — wild-animal herd parameters.
// Extends Object::External (0x20 base). Vtable 0x140163700. Parent "Description".
// Size 0x40. GetVersion=2.
//
// Factory 0x14014f190-area (MemAlloc 0x40 + vtable + zero). Serialize (vtable
// +0x28, 0x1401221e0), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
//   int   @ +0x28,  int   @ +0x2c
//   float @ +0x30,  float @ +0x34
//   int   @ +0x38,  float @ +0x3c
// Decompiled READ-ONLY from Application-x64.exe.
//
// Part of the PopulationDeer vertical slice (Foundation C keystone).
//
// ENDGAME B.3.8 K1: a REAL class (HerdDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163700) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/HerdDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140034d30 / createInPlace 0x140034d90):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
HerdDescription::HerdDescription()
    : Description(),
      _ref20(nullptr),
      _i28(0),
      _i2c(0),
      _f30(0.0f),
      _f34(0.0f),
      _i38(0),
      _f3c(0.0f) {}

// [0] 0x140034e40: ~HerdDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* HerdDescription::DeletingDtor(unsigned flags) {
    this->~HerdDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x1401221e0: stream order as retail.
bool HerdDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_ref20, &ser);
    ser.Serialize(_i28, nullptr);
    ser.Serialize(_i2c, nullptr);
    ser.Serialize(_f30, nullptr);
    ser.Serialize(_f34, nullptr);
    ser.Serialize(_i38, nullptr);
    ser.Serialize(_f3c, nullptr);
    return true;
}

// [6] 0x140034de0 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* HerdDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void HerdDescription::Destruct() {}

// [8] 0x140034e30: HerdComponent's type id through the hidden return slot.
Object::Type* HerdDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("HerdComponent");
    return ret;
}

// [9] 0x140034d90: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* HerdDescription::Construct() {
    return new (static_cast<void*>(this)) HerdDescription();
}

// create 0x140034d30: MemAlloc(0x40, 8), then the (inlined) ctor.
void* HerdDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HerdDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HerdDescription());
}
// createInPlace 0x140034d90.
void* HerdDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HerdDescription());
}

DEFINE_EXE_VTABLE(HerdDescription, "_ZTV15HerdDescription")
DEFINE_EXE_TYPE(HerdDescription, "Description")
