// code/Game/Component/marketlocationdescription.cpp
// MarketLocationDescription (entity description block) — market location marker.
// Extends Object::External (0x20 base). Vtable 0x140159e60. Parent "Description".
// Size 0x20 (no fields beyond the External base).
// GetVersion=1 (slot +0x18 @ 0x14004f1b0: mov eax,1; ret).
//
// Factory @ 0x14014b180-area: MemAlloc 0x20 + External base + vtable, no extra fields.
// Serialize (vtable +0x28, 0x14004f170): Serializer ctor + dtor only (no stream fields).
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (MarketLocationDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159e60) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/MarketLocationDescription.h"
#include "Component/MarketLocationComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x1400088a0: ~MarketLocationDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* MarketLocationDescription::DeletingDtor(unsigned flags) {
    this->~MarketLocationDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14004f170: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool MarketLocationDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x1400087d0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* MarketLocationDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) MarketLocationDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void MarketLocationDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140008820: MarketLocationComponent's type id through the hidden return slot.
Object::Type* MarketLocationDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = MarketLocationComponent::TypeId();
    return ret;
}

// [9] 0x140008780: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* MarketLocationDescription::Construct() {
    return new (static_cast<void*>(this)) MarketLocationDescription();
}

// create 0x140008720: MemAlloc(0x20, 8), then the (inlined) ctor.
void* MarketLocationDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MarketLocationDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MarketLocationDescription());
}
// createInPlace 0x140008780.
void* MarketLocationDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MarketLocationDescription());
}

DEFINE_EXE_TYPE(MarketLocationDescription, "Description")
