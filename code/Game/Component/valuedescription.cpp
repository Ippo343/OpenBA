// code/Game/Component/valuedescription.cpp
// ValueDescription (entity description block) — resource value/trade parameters.
// Extends Object::External (0x20 base). Vtable 0x14015add0. Parent "Description".
// Size 0x50. GetVersion=3 (slot +0x18 @ 0x14005ce10: mov eax,3; ret).
//
// Ctor defaults (ValueDescription_Factory @ 0x14000faa0):
//   ExternalPtr @ +0x20 = 0   (sprite/icon ref)
//   String      @ +0x28 = 0   (name)
//   ExternalPtr @ +0x30 = 0
//   String      @ +0x38 = 0
//   int         @ +0x40 = 0
//   int         @ +0x44 = 100 (baseValue)
//   int         @ +0x48 = 0   (weight) [factory writes 8 bytes @+0x44 = 100]
// Serialize (vtable +0x28, 0x14005cd60), stream order:
//   ExternalPtr @ +0x20  (FUN_14000bd30 -> fn_ExternalPtr_BuildSerialize)
//   String      @ +0x28
//   ExternalPtr @ +0x30  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   String      @ +0x38
//   int         @ +0x40
//   int         @ +0x44
//   int         @ +0x48
// ToolKit fields: sprite / name / baseValue / weight.
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (ValueDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015add0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ValueDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x14000faa0 / createInPlace 0x14000fb10):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
ValueDescription::ValueDescription()
    : Description(),
      _sprite(nullptr),
      _ref30(nullptr),
      _i40(0),
      _baseValue(100),
      _weight(0) {}

// [0] 0x14000fbf0: ~ValueDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* ValueDescription::DeletingDtor(unsigned flags) {
    this->~ValueDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14005cd60: stream order as retail.
bool ValueDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_sprite, &ser);
    _name.Serialize(&ser);
    fn_ExternalPtr_BuildSerialize(&_ref30, &ser);
    _s38.Serialize(&ser);
    ser.Serialize(_i40, nullptr);
    ser.Serialize(_baseValue, nullptr);
    ser.Serialize(_weight, nullptr);
    return true;
}

// [6] 0x14000fb80 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* ValueDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void ValueDescription::Destruct() {}

// [8] 0x140004950 (shared by 19 tables): `MOV dword [RDX],0; MOV RAX,RDX; RET` —
// a data-only description builds no component.
Object::Type* ValueDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = 0;
    return ret;
}

// [9] 0x14000fb10: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* ValueDescription::Construct() {
    return new (static_cast<void*>(this)) ValueDescription();
}

// create 0x14000faa0: MemAlloc(0x50, 8), then the (inlined) ctor.
void* ValueDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ValueDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ValueDescription());
}
// createInPlace 0x14000fb10.
void* ValueDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ValueDescription());
}

DEFINE_EXE_VTABLE(ValueDescription, "_ZTV16ValueDescription")
DEFINE_EXE_TYPE(ValueDescription, "Description")
