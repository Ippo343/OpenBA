// code/Game/Goal/timegoal.cpp
// Clean reimplementation of TimeGoal vtable methods.
//
// TimeGoal (0x38 bytes) — achievement goal requiring elapsed time.
// Vtable: 0x140160540
//
// Fields:
//   +0x30: int32  requiredYears         — years to survive (multiplied by 0xe10=3600 for seconds)
//   +0x34: bool   requireOtherGoalsMet  — if set, time only counts when other goals are met
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Goal/timegoal_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Resolved helpers
// ============================================================

// Achievement::Goal::Serialize — dispatch slot at 0x140152840
typedef void (*fn_GoalSerialize_t)(void* self, void* stream);
static fn_GoalSerialize_t s_fn_GoalSerialize = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slotAddr = ResolveAddr(Module::Exe, 0x140152840);
    if (slotAddr) s_fn_GoalSerialize = *reinterpret_cast<fn_GoalSerialize_t*>(slotAddr);

    s_helpersResolved = true;
}

// ============================================================
// TimeGoal::Slot5/Load (vtable +0x28, 0x1400dab20)
// ============================================================
// Serializes TimeGoal fields. Calls base Goal::Serialize,
// then serializes requiredYears (int) and requireOtherGoalsMet (bool).

void TimeGoal_Slot5(TimeGoal* self, void* stream) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Call base class serialize
    if (s_fn_GoalSerialize) {
        s_fn_GoalSerialize(self, stream);
    }

    // Create serializer
    uint8_t serializerBuf[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializerBuf, stream, 0);
    }

    // Serialize requiredYears (+0x30)
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(serializerBuf, (void*)(s + 0x30), nullptr);
    }

    // Serialize requireOtherGoalsMet (+0x34)
    if (g_fn_Serialize_Bool) {
        g_fn_Serialize_Bool(serializerBuf, (void*)(s + 0x34), nullptr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// TimeGoal::Slot6/Cleanup (vtable +0x30, 0x1400267a0)
// ============================================================
// Resets the goal to its default constructed state.

void TimeGoal_Slot6(TimeGoal* self) {
    // Call scalar dtor via vtable +0x00 with flags=0
    typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
    void** vtable = *(void***)self;
    fn_ScalarDtor_t scalarDtor = (fn_ScalarDtor_t)vtable[0];
    if (scalarDtor) {
        scalarDtor(self, 0);
    }

    // Re-construct in place
    TimeGoal_Ctor_Impl(self);
}

// ============================================================
// TimeGoal::Slot10/CreateInstance (vtable +0x50, 0x140026750)
// ============================================================
// Construct a TimeGoal in pre-allocated memory.

void* TimeGoal_Slot10(void* memory) {
    if (!memory) return nullptr;
    return TimeGoal_Ctor_Impl(memory);
}

// ============================================================
// TimeGoal::Slot11/SharedDtor (vtable +0x58, 0x140026800)
// ============================================================
// This is the destructor shared between TimeGoal Slot11 and TimeGoalInstance Dtor.
// It sets the vtable to TimeGoalInstance, calls base GoalInstance::~GoalInstance,
// then optionally frees memory.

void* TimeGoal_Slot11(void* self, uint32_t flags) {
    // Set vtable to TimeGoalInstance
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x140160598);
    if (vtableAddr) {
        *(void**)self = (void*)vtableAddr;
    }

    // Call base Achievement::GoalInstance::~GoalInstance
    typedef void (*fn_GoalInstanceDtor_t)(void* self);
    static fn_GoalInstanceDtor_t s_fn_GoalInstanceDtor = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_fn_GoalInstanceDtor = (fn_GoalInstanceDtor_t)ResolveExport(
            "??1GoalInstance@Achievement@@QEAA@XZ");
        s_resolved = true;
    }
    if (s_fn_GoalInstanceDtor) {
        s_fn_GoalInstanceDtor(self);
    }

    // Free memory if requested
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TimeGoal::Dtor (vtable +0x00, 0x140026850)
// ============================================================
// TimeGoal destructor. Sets vtable, calls base Goal::~Goal, optionally frees.

void* TimeGoal_Dtor(TimeGoal* self, uint32_t flags) {
    // Set vtable to TimeGoal
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x140160540);
    if (vtableAddr) {
        *(void**)self = (void*)vtableAddr;
    }

    // Call base Achievement::Goal::~Goal
    typedef void (*fn_GoalDtor_t)(void* self);
    static fn_GoalDtor_t s_fn_GoalDtor = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_fn_GoalDtor = (fn_GoalDtor_t)ResolveExport(
            "??1Goal@Achievement@@QEAA@XZ");
        s_resolved = true;
    }
    if (s_fn_GoalDtor) {
        s_fn_GoalDtor(self);
    }

    // Free memory if requested
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// TimeGoal_Ctor_Impl
// ============================================================
// Construct TimeGoal in place. From decompiled TimeGoal_Factory (0x1400266f0).

void* TimeGoal_Ctor_Impl(void* memory) {
    // Call base constructor Achievement::Goal::Goal
    typedef void* (*fn_GoalCtor_t)(void* self);
    static fn_GoalCtor_t s_fn_GoalCtor = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_fn_GoalCtor = (fn_GoalCtor_t)ResolveExport(
            "??0Goal@Achievement@@QEAA@XZ");
        s_resolved = true;
    }
    if (s_fn_GoalCtor) {
        s_fn_GoalCtor(memory);
    }

    uintptr_t s = (uintptr_t)memory;

    // Set vtable to TimeGoal
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x140160540);
    if (vtableAddr) {
        *(void**)memory = (void*)vtableAddr;
    }

    // Initialize fields
    *(int32_t*)(s + 0x30) = 1;    // requiredYears (default 1)
    *(uint8_t*)(s + 0x34) = 0;    // requireOtherGoalsMet (default false)

    return memory;
}
