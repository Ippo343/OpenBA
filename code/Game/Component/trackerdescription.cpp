// code/Game/Component/trackerdescription.cpp
// Clean reimplementation of TrackerDescription (entity description block).
//
// TrackerDescription (0x20 bytes) — trade/route tracker. No extra fields.
// Extends Object::External (0x20 base). Vtable at 0x14015ac00. Parent "Description".
//
// Factory (TrackerDescription_Factory): MemAlloc 0x20 + External ctor + vtable.
// Serialize (vtable +0x28, 0x1400593c0): Serializer ctor/dtor only — returns 1.
// GetVersion (vtable +0x18, 0x140059400): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x14000d940): External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (TrackerDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015ac00) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/TrackerDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x14000d940: ~TrackerDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* TrackerDescription::DeletingDtor(unsigned flags) {
    this->~TrackerDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x1400593c0: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool TrackerDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x14000d8e0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* TrackerDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) TrackerDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void TrackerDescription::Destruct() { DeletingDtor(0); }

// [8] 0x14000d930: TrackerComponent's type id through the hidden return slot.
Object::Type* TrackerDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("TrackerComponent");
    return ret;
}

// [9] 0x14000d890: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* TrackerDescription::Construct() {
    return new (static_cast<void*>(this)) TrackerDescription();
}

// create 0x14000d830: MemAlloc(0x20, 8), then the (inlined) ctor.
void* TrackerDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TrackerDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TrackerDescription());
}
// createInPlace 0x14000d890.
void* TrackerDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TrackerDescription());
}

DEFINE_EXE_VTABLE(TrackerDescription, "_ZTV18TrackerDescription")
DEFINE_EXE_TYPE(TrackerDescription, "Description")
