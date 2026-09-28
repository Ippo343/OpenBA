// code/Game/Component/statusicondescription.cpp
// Clean reimplementation of StatusIconDescription (entity sub-description).
//
// StatusIconDescription (0x38 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x1401646a0.  GetVersion = 1.
//
// Field layout (from Factory + Serialize FUN_14012c180 + dtor FUN_140038a60):
//   +0x20: void*  _descRef       ExternalPtr (Description) field (default 0)
//   +0x28: float  _scale         (default 1.0)
//   +0x2c: float  _scale2        (default 1.0, not serialized — runtime/cached)
//   +0x30: bool   _flag          (default 0; serialized)
//
// Serialize (FUN_14012c180) stream order:
//   1. ExternalPtr serialize (+0x20)   FUN_14000bd30
//   2. float (+0x28)
//   3. bool  (+0x30)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (StatusIconDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401646a0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/StatusIconDescription.h"
#include "Component/StatusIconComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140038920 / createInPlace 0x140038990):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
StatusIconDescription::StatusIconDescription()
    : Description(),
      _descRef(nullptr),
      _scale(1.0f),
      _scale2(1.0f),
      _flag(false),
      _pad31() {}

// [5] 0x14012c180: stream order as retail.
bool StatusIconDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    Reimpl_ExternalPtr_Serialize_StringTable(&_descRef, &ser);
    ser.Serialize(_scale, nullptr);
    ser.Serialize(_flag, nullptr);
    return true;
}

// [6] 0x1400389f0: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* StatusIconDescription::Reconstruct() {
    this->~StatusIconDescription();
    return new (static_cast<void*>(this)) StatusIconDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void StatusIconDescription::Destruct() { this->~StatusIconDescription(); }

// [8] 0x140038a50: StatusIconComponent's type id through the hidden return slot.
Object::Type* StatusIconDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = StatusIconComponent::TypeId();
    return ret;
}

// [9] 0x140038990: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* StatusIconDescription::Construct() {
    return new (static_cast<void*>(this)) StatusIconDescription();
}

// create 0x140038920: MemAlloc(0x38, 8), then the (inlined) ctor.
void* StatusIconDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StatusIconDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StatusIconDescription());
}
// createInPlace 0x140038990.
void* StatusIconDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StatusIconDescription());
}

DEFINE_EXE_TYPE(StatusIconDescription, "Description")
