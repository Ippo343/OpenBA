// code/Game/Component/storagelocationdescription.cpp
// Clean reimplementation of StorageLocationDescription (Foundation C).
//
// StorageLocationDescription (0x28 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x140164910 (exe symbol PTR_StorageLocationComponent_vslot18_140164910).
// BaseTypeInfo @ 0x1401c7f20, parent "Description", file version 2.
//
// Field layout (from StorageLocationDescription_Factory @ exe and Serialize @ 0x14012d470):
//   +0x20  int32  locationType  (enum; serialized via serializer vtable+0x98)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (StorageLocationDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140164910) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/StorageLocationDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140038e50 / createInPlace 0x140038eb0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
StorageLocationDescription::StorageLocationDescription()
    : Description(),
      _locationType(0) {}

// [0] 0x140038f60: ~StorageLocationDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* StorageLocationDescription::DeletingDtor(unsigned flags) {
    this->~StorageLocationDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14012d470: stream order as retail.
bool StorageLocationDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_locationType, nullptr);
    return true;
}

// [6] 0x140038f00: DeletingDtor(0) (destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* StorageLocationDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) StorageLocationDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void StorageLocationDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140038f50: StorageLocationComponent's type id through the hidden return slot.
Object::Type* StorageLocationDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("StorageLocationComponent");
    return ret;
}

// [9] 0x140038eb0: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* StorageLocationDescription::Construct() {
    return new (static_cast<void*>(this)) StorageLocationDescription();
}

// create 0x140038e50: MemAlloc(0x28, 8), then the (inlined) ctor.
void* StorageLocationDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageLocationDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageLocationDescription());
}
// createInPlace 0x140038eb0.
void* StorageLocationDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageLocationDescription());
}

DEFINE_EXE_VTABLE(StorageLocationDescription, "_ZTV26StorageLocationDescription")
DEFINE_EXE_TYPE(StorageLocationDescription, "Description")
