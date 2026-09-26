// code/Game/Component/multiobjectdescription.cpp
// Clean reimplementation of MultiObjectDescription (entity description block).
//
// MultiObjectDescription (0x20 bytes) — groups multiple sub-objects. No extra fields.
// Extends Object::External (0x20 base). Vtable at 0x140163d80. Parent "Description".
//
// Factory (MultiObjectDescription_Factory): MemAlloc 0x20 + External ctor + vtable.
// Serialize (vtable +0x28, 0x140125b50): Serializer ctor/dtor only — returns 1.
// GetVersion (vtable +0x18, 0x140125b90): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x140036060): External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (MultiObjectDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163d80) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/MultiObjectDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x140036060: ~MultiObjectDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* MultiObjectDescription::DeletingDtor(unsigned flags) {
    this->~MultiObjectDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140125b50: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool MultiObjectDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x140036000 — PORT GAP (retail: DeletingDtor(0), then the ctor again on
// `this`, B38_RULES §2.2). The hand-built table had a `return 0` stub; kept so
// this commit changes no behaviour. D9 #10 transcribes it for every description.
Object::External* MultiObjectDescription::Reconstruct() { return nullptr; }

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void MultiObjectDescription::Destruct() {}

// [8] 0x140036050: MultiObjectComponent's type id through the hidden return slot.
Object::Type* MultiObjectDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("MultiObjectComponent");
    return ret;
}

// [9] 0x140035fb0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* MultiObjectDescription::Construct() {
    return new (static_cast<void*>(this)) MultiObjectDescription();
}

// create 0x140035f50: MemAlloc(0x20, 8), then the (inlined) ctor.
void* MultiObjectDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MultiObjectDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MultiObjectDescription());
}
// createInPlace 0x140035fb0.
void* MultiObjectDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MultiObjectDescription());
}

DEFINE_EXE_VTABLE(MultiObjectDescription, "_ZTV22MultiObjectDescription")
DEFINE_EXE_TYPE(MultiObjectDescription, "Description")
