// code/Game/Component/healerdescription.cpp
// HealerDescription (entity description block) — healer/herbalist building parameters.
// Extends Object::External (0x20 base). Vtable 0x1401597c0. Parent "Description".
// Size 0x20 (no own fields). GetVersion=1.
//
// Factory 0x14014ae00-area (MemAlloc 0x20 + base init + vtable; no field inits).
// Serialize (vtable +0x28, 0x14004b090): Serializer ctor/dtor only — no fields.
// GetVersion (vtable +0x18, 0x14004b0d0): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (HealerDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401597c0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/HealerDescription.h"
#include "Component/HealerComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// [0] 0x140006600: ~HealerDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* HealerDescription::DeletingDtor(unsigned flags) {
    this->~HealerDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14004b090: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool HealerDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x1400065a0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* HealerDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) HealerDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void HealerDescription::Destruct() { DeletingDtor(0); }

// [8] 0x1400065f0: HealerComponent's type id through the hidden return slot.
Object::Type* HealerDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = HealerComponent::TypeId();
    return ret;
}

// [9] 0x140006550: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* HealerDescription::Construct() {
    return new (static_cast<void*>(this)) HealerDescription();
}

// create 0x1400064f0: MemAlloc(0x20, 8), then the (inlined) ctor.
void* HealerDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HealerDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HealerDescription());
}
// createInPlace 0x140006550.
void* HealerDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HealerDescription());
}

DEFINE_EXE_TYPE(HealerDescription, "Description")
