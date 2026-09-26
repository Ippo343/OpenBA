// code/Game/Tutorial/tutorialsetup.cpp
// TutorialSetup (abstract base) — all tutorial setup actions inherit from this.
// ENDGAME B.3.8 block GOAL: a REAL abstract class (Tutorial/TutorialSetup.h)
// with a compiler-generated vtable; the retail evidence is in the header.
//
// TutorialSetup itself has no fields beyond the External base (0x20 bytes).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/TutorialSetup.h"
#include "object/Internal.h"      // Object::detail::InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// [0] 0x140028390: stamp, ~External, MemFree on &1. Only a half-built or
// half-destroyed object dispatches here (the class is abstract).
Object::Base* TutorialSetup::DeletingDtor(unsigned flags) {
    this->~TutorialSetup();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1]/[2] over TutorialSetup's BaseTypeInfo; the abstract registration stays
// in factory_registrations_bulk.cpp (B38_RULES D4).
const char* TutorialSetup::TypeName() const { return "TutorialSetup"; }     // 0x140028380
uint32_t    TutorialSetup::Type() const {                                   // 0x140028370
    return ::Object::GetTypeIdByName("TutorialSetup");
}

// ============================================================
// TutorialSetup::Serialize (vtable +0x28, 0x1400e2000)
// ============================================================
// No fields: a Serializer(stream, 0) scope, created and destroyed; returns 1.
bool TutorialSetup::Serialize(void* stream) {
    IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    return true;
}
