// code/Game/Component/statemachinedescription.cpp
// Clean reimplementation of StateMachineDescription (Object::External "Description").
//
// StateMachineDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable at 0x1401645a0.
//
// Carries NO serialized fields — the state-machine behaviour is code-driven.
// Factory (0x14014f9d0 area) allocates 0x20 bytes, runs External base ctor, sets
// vtable. Serialize (0x14012bb00) just ctor+dtor a Serializer. GetVersion
// (0x14012bb40) = 1.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).
//
// ENDGAME B.3.8 K1: a REAL class (StateMachineDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401645a0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/StateMachineDescription.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140038330 / createInPlace 0x140038390):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
StateMachineDescription::StateMachineDescription()
    : Description() {}

// [0] 0x140038440: ~StateMachineDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* StateMachineDescription::DeletingDtor(unsigned flags) {
    this->~StateMachineDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14012bb00: one EMPTY Serializer(stream, 0) scope, then `MOV AL,1`.
bool StateMachineDescription::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}

// [6] 0x1400383e0: DeletingDtor(0) (destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* StateMachineDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) StateMachineDescription();
}

// [7] 0x140001640 — PORT GAP (retail: the shared `DeletingDtor(0)` body). The
// hand-built table had a `return 0` stub; kept until D9 #10's commit.
void StateMachineDescription::Destruct() {}

// [8] 0x140038430: StateMachineComponent's type id through the hidden return slot.
Object::Type* StateMachineDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ::Object::GetTypeIdByName("StateMachineComponent");
    return ret;
}

// [9] 0x140038390 — PORT GAP (retail: placement-new on `this`). The hand-built
// table had a `return nullptr` guard here; kept until D9 #10's commit.
Object::External* StateMachineDescription::Construct() { return nullptr; }

// create 0x140038330: MemAlloc(0x20, 8), then the (inlined) ctor.
void* StateMachineDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StateMachineDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StateMachineDescription());
}
// createInPlace 0x140038390.
void* StateMachineDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StateMachineDescription());
}

DEFINE_EXE_VTABLE(StateMachineDescription, "_ZTV23StateMachineDescription")
DEFINE_EXE_TYPE(StateMachineDescription, "Description")
