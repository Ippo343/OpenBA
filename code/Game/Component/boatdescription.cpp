// code/Game/Component/boatdescription.cpp
// BoatDescription (entity description block) — trader-boat / vessel parameters.
// Extends Object::External (0x20 base). Vtable 0x140158fd0. Parent "Description".
// Size 0x20 (no own fields). GetVersion=2.
//
// Factory 0x14014a8c0-area (MemAlloc 0x20 + base init + vtable; no field inits).
// Serialize (vtable +0x28, 0x1400469c0): Serializer ctor/dtor only — no fields.
// GetVersion (vtable +0x18, 0x140046a00): mov eax,2; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (BoatDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140158fd0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/BoatDescription.h"
#include "Component/BoatComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [5] 0x1400469c0: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool BoatDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x140003810: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* BoatDescription::Reconstruct() {
    this->~BoatDescription();
    return new (static_cast<void*>(this)) BoatDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void BoatDescription::Destruct() { this->~BoatDescription(); }

// [8] 0x140003860: BoatComponent's type id through the hidden return slot.
Object::Type* BoatDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = BoatComponent::TypeId();
    return ret;
}

// [9] 0x1400037c0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* BoatDescription::Construct() {
    return new (static_cast<void*>(this)) BoatDescription();
}

// create 0x140003760: MemAlloc(0x20, 8), then the (inlined) ctor.
void* BoatDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BoatDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BoatDescription());
}
// createInPlace 0x1400037c0.
void* BoatDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BoatDescription());
}

DEFINE_EXE_TYPE(BoatDescription, "Description")
