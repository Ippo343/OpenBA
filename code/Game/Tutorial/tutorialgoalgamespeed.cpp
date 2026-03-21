// code/Game/Goal/tutorialgoalgamespeed.cpp
// Clean reimplementation of TutorialGoalGameSpeed vtable methods.
//
// TutorialGoalGameSpeed (0x28 bytes) — tutorial goal waiting for game speed change.
// Vtable: 0x1401608e0
// Extends TutorialGoal -> Object::External
//
// Fields:
//   +0x20: int32_t targetSpeed — target game speed (default 1)
//
// The Check function is trivial: compare game's current speed against targetSpeed.
// Reads from: game(singleton) -> +0x120 (time/speed controller) -> +0x34 (current speed)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Tutorial/tutorialgoalgamespeed_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// TutorialGoalGameSpeed::Check (vtable +0x40, 0x1400df150)
// ============================================================
// Compares game's current speed against targetSpeed.
// Original: MOV RAX,[g_pGameWorldSingleton]; MOV ECX,[RCX+0x20];
//           MOV RDX,[RAX+0x120]; CMP [RDX+0x34],ECX; SETZ AL; RET
int TutorialGoalGameSpeed_Check(TutorialGoalGameSpeed* self) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    uintptr_t game = (uintptr_t)*g_pGameWorldSingleton;

    // game+0x120 = time/speed controller
    uintptr_t timeCtrl = *(uintptr_t*)(game + 0x120);
    if (timeCtrl == 0) return 0;

    // time controller +0x34 = current game speed
    int32_t currentSpeed = *(int32_t*)(timeCtrl + 0x34);

    return (currentSpeed == self->targetSpeed) ? 1 : 0;
}

// ============================================================
// TutorialGoalGameSpeed::Dtor (vtable +0x00, 0x140027730)
// ============================================================
// 1. Set vtable to TutorialGoalGameSpeed (no-op destruction step)
// 2. Set vtable to TutorialGoal base
// 3. Call External::~External
// 4. Optionally free memory
void* TutorialGoalGameSpeed_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to TutorialGoalGameSpeed (for correct dtor dispatch if nested)
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Set vtable to TutorialGoal base
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;

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
// TutorialGoalGameSpeed::Serialize (vtable +0x28, 0x1400df170)
// ============================================================
// 1. Call base TutorialGoal::Serialize (empty serializer create/destroy)
// 2. Create new serializer, serialize int at +0x20 (targetSpeed)
void TutorialGoalGameSpeed_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Base class serialize
    uint8_t ser0[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser0, stream, 0);
    }
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser0);
    }

    // Serialize our fields
    uint8_t ser[32];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(ser, stream, 0);
    }

    // Serialize targetSpeed (+0x20) — int
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser, (void*)(s + 0x20), nullptr);
    }

    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(ser);
    }
}

// ============================================================
// TutorialGoalGameSpeed::Cleanup (vtable +0x30, 0x1400276e0)
// ============================================================
// Calls ScalarDtor(self, 0) via vtable, then re-initializes.
void TutorialGoalGameSpeed_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call ScalarDtor(self, 0) via vtable
    uintptr_t* vtable = *(uintptr_t**)s;
    typedef void* (*DtorFn)(void*, uint32_t);
    DtorFn dtor = (DtorFn)vtable[0];
    dtor(self, 0);

    // Re-initialize via External::External
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Reset targetSpeed to default
    *(int32_t*)(s + 0x20) = 1;
}

// ============================================================
// TutorialGoalGameSpeed::Init (vtable +0x48, 0x140027690)
// ============================================================
// Constructor/init called by factory after allocation.
void* TutorialGoalGameSpeed_Init(void* self) {
    if (self == nullptr) {
        return nullptr;
    }

    uintptr_t s = (uintptr_t)self;

    // Call External::External base init
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable chain: TutorialGoal -> TutorialGoalGameSpeed
    uintptr_t baseVtable = ResolveAddr(Module::Exe, TutorialGoalAddr::Vtable);
    *(uintptr_t*)s = baseVtable;
    uintptr_t myVtable = ResolveAddr(Module::Exe, TutorialGoalGameSpeedAddr::Vtable);
    *(uintptr_t*)s = myVtable;

    // Set default target speed
    *(int32_t*)(s + 0x20) = 1;

    return self;
}
