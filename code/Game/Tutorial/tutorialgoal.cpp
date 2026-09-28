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
#include "Object/exe_type.h"   // DEFINE_EXE_ABSTRACT_TYPE

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(TutorialGoal, "Object::External")

// ============================================================
// TutorialGoal::Serialize (vtable +0x28, 0x1400dec30)
// ============================================================
// No fields: a Serializer(stream, 0) scope, created and destroyed; returns 1.
bool TutorialGoal::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}
