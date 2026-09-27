// code/Game/Component/clearresourcedescription.cpp
// ClearResourceDescription (entity description block) — clear-resource marker.
// Extends Object::External (0x20 base). Vtable 0x1401628d0. Parent "Description".
// Size 0x20 (NO fields beyond the External base). GetVersion=1.
//
// Factory 0x14014e950-area (MemAlloc 0x20 + vtable only, no defaults).
// Serialize (vtable +0x28, 0x140118730): Serializer ctor/dtor only, no payload.
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (ClearResourceDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401628d0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ClearResourceDescription.h"
#include "Component/ClearResourceComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x140031710: ~ClearResourceDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* ClearResourceDescription::DeletingDtor(unsigned flags) {
    this->~ClearResourceDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140118730: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool ClearResourceDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x1400316b0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* ClearResourceDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) ClearResourceDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void ClearResourceDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140031700: ClearResourceComponent's type id through the hidden return slot.
Object::Type* ClearResourceDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ClearResourceComponent::TypeId();
    return ret;
}

// [9] 0x140031660: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* ClearResourceDescription::Construct() {
    return new (static_cast<void*>(this)) ClearResourceDescription();
}

// create 0x140031600: MemAlloc(0x20, 8), then the (inlined) ctor.
void* ClearResourceDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ClearResourceDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ClearResourceDescription());
}
// createInPlace 0x140031660.
void* ClearResourceDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ClearResourceDescription());
}

DEFINE_EXE_VTABLE(ClearResourceDescription, "_ZTV24ClearResourceDescription")
DEFINE_EXE_TYPE(ClearResourceDescription, "Description")
