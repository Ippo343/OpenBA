// code/Game/Tutorial/tutorialgoal.cpp
// TutorialGoal (abstract base) — all tutorial goals inherit from this.
// ENDGAME B.3.8 block GOAL: a REAL abstract class (Tutorial/TutorialGoal.h)
// with a compiler-generated vtable; the retail evidence is in the header.
//
// TutorialGoal itself has no fields beyond the External base.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialGoal.h"
#include "object/Internal.h"      // Object::detail::InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// [0] 0x140027220: stamp, ~External, MemFree on &1. Only a half-built or
// half-destroyed object dispatches here (the class is abstract).
Object::Base* TutorialGoal::DeletingDtor(unsigned flags) {
    this->~TutorialGoal();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] over TutorialGoal's BaseTypeInfo; the abstract registration stays
// in factory_registrations_bulk.cpp (B38_RULES D4).
const char* TutorialGoal::TypeName() const { return "TutorialGoal"; }       // 0x140027210
uint32_t    TutorialGoal::Type() const {                                    // 0x140027200
    return ::Object::GetTypeIdByName("TutorialGoal");
}

// ============================================================
// TutorialGoal::Serialize (vtable +0x28, 0x1400dec30)
// ============================================================
// No fields: a Serializer(stream, 0) scope, created and destroyed; returns 1.
bool TutorialGoal::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}
