// code/Game/Component/wildanimaldescription.cpp
// WildAnimalDescription (entity description block) — wild-animal marker.
// Extends Object::External (0x20 base). Vtable 0x140164b70. Parent "Description".
// Size 0x20 (NO fields beyond the External base). GetVersion=1.
//
// Factory 0x14014fdd0-area (MemAlloc 0x20 + vtable only, no defaults).
// Serialize (vtable +0x28, 0x14012e440): constructs/destructs a Serializer but
//   serializes nothing (no payload). Returns 1.
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (WildAnimalDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140164b70) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/WildAnimalDescription.h"
#include "Component/WildAnimalComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x140039d90: ~WildAnimalDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* WildAnimalDescription::DeletingDtor(unsigned flags) {
    this->~WildAnimalDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14012e440: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool WildAnimalDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x140039d30: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* WildAnimalDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) WildAnimalDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void WildAnimalDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140039d80: WildAnimalComponent's type id through the hidden return slot.
Object::Type* WildAnimalDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = WildAnimalComponent::TypeId();
    return ret;
}

// [9] 0x140039ce0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* WildAnimalDescription::Construct() {
    return new (static_cast<void*>(this)) WildAnimalDescription();
}

// create 0x140039c80: MemAlloc(0x20, 8), then the (inlined) ctor.
void* WildAnimalDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(WildAnimalDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) WildAnimalDescription());
}
// createInPlace 0x140039ce0.
void* WildAnimalDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) WildAnimalDescription());
}

DEFINE_EXE_VTABLE(WildAnimalDescription, "_ZTV21WildAnimalDescription")
DEFINE_EXE_TYPE(WildAnimalDescription, "Description")
