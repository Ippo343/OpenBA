// code/Game/Component/storagedisplaydescription.cpp
// Clean reimplementation of StorageDisplayDescription (Foundation C).
//
// StorageDisplayDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x14015a800 (exe symbol PTR_StorageDisplayComponent_vslot16_14015a800).
// BaseTypeInfo @ 0x1401c4988, parent "Description", file version 1.
//
// No own fields beyond the 0x20 External base. Serialize @ 0x140056a80 only
// constructs/destructs the Serializer (no field reads) — a header/marker block.
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (StorageDisplayDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015a800) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/StorageDisplayDescription.h"
#include "Component/StorageDisplayComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [5] 0x140056a80: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool StorageDisplayDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x14000c880: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* StorageDisplayDescription::Reconstruct() {
    this->~StorageDisplayDescription();
    return new (static_cast<void*>(this)) StorageDisplayDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void StorageDisplayDescription::Destruct() { this->~StorageDisplayDescription(); }

// [8] 0x14000c8d0: StorageDisplayComponent's type id through the hidden return slot.
Object::Type* StorageDisplayDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = StorageDisplayComponent::TypeId();
    return ret;
}

// [9] 0x14000c830: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* StorageDisplayDescription::Construct() {
    return new (static_cast<void*>(this)) StorageDisplayDescription();
}

// create 0x14000c7d0: MemAlloc(0x20, 8), then the (inlined) ctor.
void* StorageDisplayDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageDisplayDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageDisplayDescription());
}
// createInPlace 0x14000c830.
void* StorageDisplayDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageDisplayDescription());
}

DEFINE_EXE_TYPE(StorageDisplayDescription, "Description")
