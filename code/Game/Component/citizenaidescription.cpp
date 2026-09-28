// code/Game/Component/citizenaidescription.cpp
// Clean reimplementation of CitizenAIDescription (an Object::External "Description").
//
// CitizenAIDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable at 0x1401624f0.
//
// This description carries NO serialized fields — the CitizenAI behaviour is
// entirely code-driven. Factory (0x14014e750 area) allocates 0x20 bytes, runs the
// External base ctor, sets the vtable. Serialize (0x14010fc40) just constructs +
// destructs an IO::Serializer (reads nothing). GetVersion (0x14010fc80) = 1.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).
//
// ENDGAME B.3.8 K1: a REAL class (CitizenAIDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401624f0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/CitizenAIDescription.h"
#include "Component/CitizenAIComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x14002fb80 / createInPlace 0x14002fbe0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
CitizenAIDescription::CitizenAIDescription()
    : Description() {}

// [5] 0x14010fc40: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool CitizenAIDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x14002fc30: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* CitizenAIDescription::Reconstruct() {
    this->~CitizenAIDescription();
    return new (static_cast<void*>(this)) CitizenAIDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void CitizenAIDescription::Destruct() { this->~CitizenAIDescription(); }

// [8] 0x14002fc80: CitizenAIComponent's type id through the hidden return slot.
Object::Type* CitizenAIDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = CitizenAIComponent::TypeId();
    return ret;
}

// [9] 0x14002fbe0: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* CitizenAIDescription::Construct() {
    return new (static_cast<void*>(this)) CitizenAIDescription();
}

// create 0x14002fb80: MemAlloc(0x20, 8), then the (inlined) ctor.
void* CitizenAIDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CitizenAIDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CitizenAIDescription());
}
// createInPlace 0x14002fbe0.
void* CitizenAIDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CitizenAIDescription());
}

DEFINE_EXE_TYPE(CitizenAIDescription, "Description")
