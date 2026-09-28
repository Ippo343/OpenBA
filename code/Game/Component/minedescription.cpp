// code/Game/Component/minedescription.cpp
// MineDescription (entity description block) — mining building parameters.
// Extends Object::External (0x20 base). Vtable 0x140159f50. Parent "Description".
// Size 0x20 (no fields beyond the External base; products come from the parent
// ConsumeProduceDescription which Mine derives from at runtime, not stored here).
// GetVersion=1 (slot +0x18 @ 0x14004f390: mov eax,1; ret).
//
// Factory @ 0x14014b2c0-area: MemAlloc 0x20 + External base + vtable, no extra fields.
// Serialize (vtable +0x28, 0x14004f350): Serializer ctor + dtor only (no stream fields).
// ToolKit fields: products array (lives on the ConsumeProduce parent block, not here).
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (MineDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159f50) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/MineDescription.h"
#include "Component/MineComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x140008f40: ~MineDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* MineDescription::DeletingDtor(unsigned flags) {
    this->~MineDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14004f350: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool MineDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x140008ee0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* MineDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) MineDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void MineDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140008f30: MineComponent's type id through the hidden return slot.
Object::Type* MineDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = MineComponent::TypeId();
    return ret;
}

// [9] 0x140008e90: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* MineDescription::Construct() {
    return new (static_cast<void*>(this)) MineDescription();
}

// create 0x140008e30: MemAlloc(0x20, 8), then the (inlined) ctor.
void* MineDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MineDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MineDescription());
}
// createInPlace 0x140008e90.
void* MineDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MineDescription());
}

DEFINE_EXE_TYPE(MineDescription, "Description")
