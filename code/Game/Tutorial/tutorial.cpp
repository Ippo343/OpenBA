// code/Game/Tutorial/tutorial.cpp
// Clean reimplementation of Tutorial vtable methods.
//
// Tutorial (0x40 bytes), vtable at 0x140160740.
// Extends Object::External (0x20 bytes base).
//
// Fields:
//   +0x20: void* setup       — ExternalPtr to TutorialSetup
//   +0x28: void* steps       — array of ExternalPtr to TutorialStep
//   +0x30: int64  stepCount  — number of steps (count + capacity packed)
//   +0x38: int32  currentStep — current step index (-1 = none)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Tutorial/tutorial_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Tutorial::Dtor (vtable +0x00, 0x140026f50)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to Tutorial
// 2. Free steps array at +0x28
// 3. Zero +0x28 and +0x30
// 4. Call External::~External(self)
// 5. If (flags & 1), free self
// 6. Return self
void* Tutorial_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to Tutorial
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Free steps array
    void* stepsArray = *(void**)(s + 0x28);
    if (fn_MemFreeStatic && stepsArray) {
        fn_MemFreeStatic(stepsArray);
    }

    // Zero steps pointer and count
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;

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
// Tutorial::Cleanup (vtable +0x30, 0x140026f00)
// ============================================================
// Cleanup and reinitialize.
// 1. Call self->vtable[0](self, 0) — Dtor with flags=0 (cleanup only, no free)
// 2. Call External::External(self) — reinitialize base
// 3. Set vtable to Tutorial
// 4. Zero all Tutorial fields
void* Tutorial_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call Dtor(self, 0) through vtable — this cleans up derived class fields too
    typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
    uintptr_t vtablePtr = *(uintptr_t*)s;
    fn_ScalarDtor_t dtor = *(fn_ScalarDtor_t*)vtablePtr;
    dtor(self, 0);

    // Reinitialize External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to Tutorial
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Zero Tutorial fields
    *(uint64_t*)(s + 0x20) = 0;  // setup
    *(uint64_t*)(s + 0x28) = 0;  // steps array
    *(uint64_t*)(s + 0x30) = 0;  // step count
    *(int32_t*)(s + 0x38) = -1;  // current step index

    return self;
}

// ============================================================
// Tutorial::Ctor (vtable +0x40, 0x140026eb0)
// ============================================================
// Constructor.
// 1. Null check self
// 2. Call External::External(self) — initialize base
// 3. Set vtable to Tutorial
// 4. Zero all Tutorial fields
void* Tutorial_Ctor(void* self) {
    if (!self) return nullptr;

    uintptr_t s = (uintptr_t)self;

    // Initialize External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to Tutorial
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Zero Tutorial fields
    *(uint64_t*)(s + 0x20) = 0;  // setup
    *(uint64_t*)(s + 0x28) = 0;  // steps array
    *(uint64_t*)(s + 0x30) = 0;  // step count
    *(int32_t*)(s + 0x38) = -1;  // current step index

    return self;
}
