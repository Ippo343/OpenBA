// code/Game/Goal/tutorialgoal.cpp
// Clean reimplementation of TutorialGoal vtable methods.
//
// TutorialGoal (abstract base) — all tutorial goals inherit from this.
// Vtable: 0x1401607a0
// Extends Object::External.
//
// TutorialGoal itself has no fields beyond the External base.
// Dtor sets vtable, calls External::~External, optionally frees memory.
// Serialize creates a Serializer from the stream then immediately destroys it
// (no fields to serialize in the base class).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorialgoal_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// TutorialGoal::Dtor (vtable +0x00, 0x140027220)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to TutorialGoal vtable
// 2. Call External::~External(self)
// 3. If (flags & 1), free memory
// 4. Return self
void* TutorialGoal_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoal
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Call External::~External
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TutorialGoal::Serialize (vtable +0x28, 0x1400dec30)
// ============================================================
// Base class serialize — no fields to serialize.
// Creates a Serializer(stream, version=0), immediately destroys it.
// Returns 1 (true).
void TutorialGoal_Serialize(void* self, void* stream) {
    (void)self; // unused — base has no fields

    // Create and immediately destroy a serializer
    // This establishes the version for derived classes that call base first
    alignas(8) uint8_t serializer[32]; // Serializer is ~24-32 bytes
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
