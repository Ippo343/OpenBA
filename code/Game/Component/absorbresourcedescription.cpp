// code/Game/Component/absorbresourcedescription.cpp
// Clean reimplementation of AbsorbResourceDescription (Object::External "Description").
//
// AbsorbResourceDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable at 0x140162090.
//
// Carries NO serialized fields. Factory (0x14014e450 area) allocates 0x20 bytes,
// runs External base ctor, sets vtable. Serialize (0x140105d50) just ctor+dtor a
// Serializer (reads nothing). GetVersion (0x140105d90) = 1.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).
//
// ENDGAME B.3.8 K1: a REAL class (AbsorbResourceDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140162090) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/AbsorbResourceDescription.h"
#include "Component/AbsorbResourceComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x14002ea80 / createInPlace 0x14002eae0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
AbsorbResourceDescription::AbsorbResourceDescription()
    : Description() {}

// [5] 0x140105d50: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool AbsorbResourceDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x14002eb30: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* AbsorbResourceDescription::Reconstruct() {
    this->~AbsorbResourceDescription();
    return new (static_cast<void*>(this)) AbsorbResourceDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void AbsorbResourceDescription::Destruct() { this->~AbsorbResourceDescription(); }

// [8] 0x14002eb80: AbsorbResourceComponent's type id through the hidden return slot.
Object::Type* AbsorbResourceDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = AbsorbResourceComponent::TypeId();
    return ret;
}

// [9] 0x14002eae0: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* AbsorbResourceDescription::Construct() {
    return new (static_cast<void*>(this)) AbsorbResourceDescription();
}

// create 0x14002ea80: MemAlloc(0x20, 8), then the (inlined) ctor.
void* AbsorbResourceDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AbsorbResourceDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AbsorbResourceDescription());
}
// createInPlace 0x14002eae0.
void* AbsorbResourceDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AbsorbResourceDescription());
}

DEFINE_EXE_TYPE(AbsorbResourceDescription, "Description")
