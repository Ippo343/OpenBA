// code/Game/Component/zonedescription.cpp
// Clean reimplementation of ZoneDescription (entity description block).
//
// ZoneDescription (0x20 bytes) — marker description for a zone-type building.
// Extends Object::External (0x20 base). Vtable at 0x140164eb0.
// Parent type "Description".
//
// Factory  : 0x14003a700  (MemAlloc 0x20 + ctor = ZoneComponent_vslot16 init)
// CreateInPlace: 0x14003a760
// Ctor body: just External::External + vtable (no extra fields).
// Serialize (vtable +0x28, 0x140130430): EMPTY — only constructs/destroys the
//   stack Serializer. Zero stream bytes (pure type marker).
// GetVersion (vtable +0x18, 0x140130470): mov eax,1; ret  -> file version 1.
// Dtor (vtable +0x00, 0x14003a810): vtable reset + External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (ZoneDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140164eb0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ZoneDescription.h"
#include "Component/ZoneComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x14003a810: ~ZoneDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* ZoneDescription::DeletingDtor(unsigned flags) {
    this->~ZoneDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140130430: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool ZoneDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x14003a7b0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* ZoneDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) ZoneDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void ZoneDescription::Destruct() { DeletingDtor(0); }

// [8] 0x14003a800: ZoneComponent's type id through the hidden return slot.
Object::Type* ZoneDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ZoneComponent::TypeId();
    return ret;
}

// [9] 0x14003a760: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* ZoneDescription::Construct() {
    return new (static_cast<void*>(this)) ZoneDescription();
}

// create 0x14003a700: MemAlloc(0x20, 8), then the (inlined) ctor.
void* ZoneDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ZoneDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ZoneDescription());
}
// createInPlace 0x14003a760.
void* ZoneDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ZoneDescription());
}

DEFINE_EXE_TYPE(ZoneDescription, "Description")
