// code/Game/Goal/finishtutorialgoal.cpp
// Clean reimplementation of FinishTutorialGoal vtable methods.
//
// FinishTutorialGoal (0x38 bytes) — achievement goal for completing tutorial.
// Vtable: 0x140160420
//
// Fields (from Ghidra decompilation):
//   +0x00: void*    vtable
//   +0x08: uint8_t  _goalBase[0x28]  — Goal base fields
//   +0x30: void*    tutorialRef      — External* to Tutorial object
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Tutorial/finishtutorialgoal_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// Vtable pointer for FinishTutorialGoal (resolved at runtime)
static void* s_vtable_FinishTutorialGoal = nullptr;
// Vtable pointer for FinishTutorialInstance (resolved at runtime)
static void* s_vtable_FinishTutorialInstance = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_vtable_FinishTutorialGoal = (void*)ResolveAddr(Module::Exe, 0x140160420);
    s_vtable_FinishTutorialInstance = (void*)ResolveAddr(Module::Exe, 0x140160478);
    s_resolved = true;
}

// ============================================================
// FinishTutorialGoal::Dtor (vtable +0x00, 0x140026260)
// ============================================================
// Scalar destructor: sets vtable, calls Goal::~Goal, optionally frees.
void* FinishTutorialGoal_Dtor(FinishTutorialGoal* self, uint32_t flags) {
    EnsureResolved();

    // Set vtable to FinishTutorialGoal (for correct virtual dispatch during destruction)
    *(void**)self = s_vtable_FinishTutorialGoal;

    // Call Goal base destructor
    if (fn_Goal_BaseDtor) {
        fn_Goal_BaseDtor(self);
    }

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// FinishTutorialGoal::Slot5 (vtable +0x28, 0x1400d7970)
// ============================================================
// Serialize/Load: calls Goal::Serialize base, then serializes
// the tutorial External* at +0x30 using the tutorial-specific
// External serializer.
int FinishTutorialGoal_Slot5(FinishTutorialGoal* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Call Goal base serialize
    if (fn_Goal_BaseSerialize) {
        fn_Goal_BaseSerialize(self, stream);
    }

    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize the tutorial External* at +0x30
    if (fn_ExternalPtr_SerializeTutorial) {
        fn_ExternalPtr_SerializeTutorial((void*)(s + 0x30), serializer);
    }

    // Destroy Serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializer);
    }

    return 1;
}

// ============================================================
// FinishTutorialGoal::Slot6 (vtable +0x30, 0x1400261a0)
// ============================================================
// Factory constructor: calls Goal::Goal() base ctor, sets vtable,
// zeroes the tutorial reference at +0x30.
// Returns null if input is null.
FinishTutorialGoal* FinishTutorialGoal_Slot6(FinishTutorialGoal* self) {
    EnsureResolved();

    if (self == nullptr) {
        return nullptr;
    }

    // Call Goal base default constructor
    if (fn_Goal_BaseCtor) {
        fn_Goal_BaseCtor(self);
    }

    // Set vtable to FinishTutorialGoal
    *(void**)self = s_vtable_FinishTutorialGoal;

    // Zero the tutorial reference
    *(uint64_t*)((uintptr_t)self + 0x30) = 0;

    return self;
}

// ============================================================
// FinishTutorialGoal::CreateInstance (vtable +0x50, 0x140026150)
// ============================================================
// Same as Slot6 — constructs a new FinishTutorialGoal in-place.
FinishTutorialGoal* FinishTutorialGoal_CreateInstance(FinishTutorialGoal* self) {
    return FinishTutorialGoal_Slot6(self);
}

// ============================================================
// FinishTutorialGoal::Slot11 / FinishTutorialInstance::Dtor
// (vtable +0x58, 0x140026200)
// ============================================================
// Scalar destructor for FinishTutorialInstance:
//   1. Sets vtable to FinishTutorialInstance
//   2. Deletes InternalBasePtr at +0x20
//   3. Calls GoalInstance::~GoalInstance
//   4. Optionally frees memory
void* FinishTutorialGoal_Slot11(void* self, uint32_t flags) {
    EnsureResolved();
    uintptr_t s = (uintptr_t)self;

    // Set vtable to FinishTutorialInstance
    *(void**)s = s_vtable_FinishTutorialInstance;

    // Delete InternalBasePtr at +0x20
    if (fn_InternalBasePtr_DeletePtr) {
        fn_InternalBasePtr_DeletePtr((void*)(s + 0x20));
    }

    // Call GoalInstance base destructor
    if (fn_GoalInstance_BaseDtor) {
        fn_GoalInstance_BaseDtor(self);
    }

    // If bit 0 set, free memory
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}
