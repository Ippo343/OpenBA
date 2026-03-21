// code/Game/Goal/timegoalinstance.cpp
// Clean reimplementation of TimeGoalInstance vtable methods.
//
// TimeGoalInstance (0x28 bytes) — tracks elapsed time for TimeGoal.
// Vtable: 0x140160598
//
// Fields:
//   +0x08..+0x1F: GoalInstance base (0x18 bytes)
//     +0x18: void*  goal — pointer to parent TimeGoal (from GoalInstance base)
//   +0x20: int64   elapsedTime — accumulated time in engine ticks (System::Time)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Goal/timegoalinstance_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Resolved helpers
// ============================================================

// Achievement::GoalInstance::Persist — dispatch slot at 0x140152868
typedef void (*fn_GoalInstancePersist_t)(void* self, void* stream);
static fn_GoalInstancePersist_t s_fn_GoalInstancePersist = nullptr;

// System::Time::Serialize — dispatch slot at 0x140152aa8
typedef void (*fn_TimeSerialize_t)(void* time, void* serializer);
static fn_TimeSerialize_t s_fn_TimeSerialize = nullptr;

// fn_InternalBasePtr_Ctor — dispatch slot at 0x140152a90
typedef void* (*fn_IBPCtor_t)(void* self, void* internal);
static fn_IBPCtor_t s_fn_IBPCtor = nullptr;

// Achievement::GoalList::AreGoalsMetExcept — dispatch slot at 0x140152878
typedef bool (*fn_AreGoalsMetExcept_t)(void* goalList, void* exceptIBP);
static fn_AreGoalsMetExcept_t s_fn_AreGoalsMetExcept = nullptr;

// Achievement::GoalInstance::Update — dispatch slot at 0x140152880
typedef void (*fn_GoalInstanceUpdate_t)(void* self, void* time, void* goalList);
static fn_GoalInstanceUpdate_t s_fn_GoalInstanceUpdate = nullptr;

// fn_IsDerivedFrom — dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// System::Time::AsSeconds — dispatch slot at 0x140151318
typedef float (*fn_TimeAsSeconds_t)(void* time);
static fn_TimeAsSeconds_t s_fn_TimeAsSeconds = nullptr;

// TypeInfo_TimeGoal — type ID at 0x1401c62e8
static uint32_t* s_TypeInfo_TimeGoal = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    // Resolve dispatch slots (read function pointer from .rdata slot)
    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152868);
    if (slot) s_fn_GoalInstancePersist = *reinterpret_cast<fn_GoalInstancePersist_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_TimeSerialize = *reinterpret_cast<fn_TimeSerialize_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a90);
    if (slot) s_fn_IBPCtor = *reinterpret_cast<fn_IBPCtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152878);
    if (slot) s_fn_AreGoalsMetExcept = *reinterpret_cast<fn_AreGoalsMetExcept_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152880);
    if (slot) s_fn_GoalInstanceUpdate = *reinterpret_cast<fn_GoalInstanceUpdate_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140151318);
    if (slot) s_fn_TimeAsSeconds = *reinterpret_cast<fn_TimeAsSeconds_t*>(slot);

    s_TypeInfo_TimeGoal = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c62e8);

    s_helpersResolved = true;
}

// ============================================================
// TimeGoalInstance::Slot3/Serialize (vtable +0x18, 0x1400dad30)
// ============================================================
// Serializes instance state. Calls GoalInstance::Persist first,
// then serializes the elapsed time field via Time::Serialize.

void TimeGoalInstance_Slot3(TimeGoalInstance* self, void* stream) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Call base GoalInstance::Persist
    if (s_fn_GoalInstancePersist) {
        s_fn_GoalInstancePersist(self, stream);
    }

    // Create serializer
    uint8_t serializerBuf[24];
    if (g_fn_Serializer_Ctor) {
        g_fn_Serializer_Ctor(serializerBuf, stream, 0);
    }

    // Serialize elapsed time (+0x20) via System::Time::Serialize
    if (s_fn_TimeSerialize) {
        s_fn_TimeSerialize((void*)(s + 0x20), serializerBuf);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) {
        g_fn_Serializer_Dtor(serializerBuf);
    }
}

// ============================================================
// TimeGoalInstance::Slot5/Startup (vtable +0x28, 0x1400dac30)
// ============================================================
// Accumulates elapsed time. Checks if other goals are met via
// AreGoalsMetExcept; if not, resets elapsed time to zero.
// Then calls base GoalInstance::Update.
//
// From disassembly:
//   self->elapsedTime += *time;
//   InternalBasePtr ibp; IBPCtor(&ibp, self);
//   if (!GoalList::AreGoalsMetExcept(goalList, &ibp)) self->elapsedTime = 0;
//   GoalInstance::Update(self, time, goalList);

void TimeGoalInstance_Slot5(TimeGoalInstance* self, void* time, void* goalList) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Accumulate time: self->elapsedTime += *(int64_t*)time
    int64_t deltaTime = *(int64_t*)time;
    *(int64_t*)(s + 0x20) += deltaTime;

    // Check if other goals are met (except this instance)
    uint8_t ibpBuf[8];
    if (s_fn_IBPCtor) {
        s_fn_IBPCtor(ibpBuf, self);
    }

    if (s_fn_AreGoalsMetExcept) {
        bool othersMet = s_fn_AreGoalsMetExcept(goalList, ibpBuf);
        if (!othersMet) {
            // Reset elapsed time to zero — other goals not met
            *(int64_t*)(s + 0x20) = 0;
        }
    }

    // Call base GoalInstance::Update
    if (s_fn_GoalInstanceUpdate) {
        s_fn_GoalInstanceUpdate(self, time, goalList);
    }
}

// ============================================================
// TimeGoalInstance::Slot6/Shutdown (vtable +0x30, 0x1400dacb0)
// ============================================================
// Checks if the time goal is complete.
//
// From disassembly:
//   goal = self->+0x18 (parent goal pointer)
//   IsDerivedFrom(goal, TypeInfo_TimeGoal) -> timeGoal (or null)
//   seconds = Time::AsSeconds(&self->elapsedTime)
//   return (timeGoal->requiredYears * 0xe10) <= seconds
//
// 0xe10 = 3600 = seconds per year in game time (1 year = 1 hour real time)

int TimeGoalInstance_Slot6(TimeGoalInstance* self) {
    EnsureHelpers();
    uintptr_t s = (uintptr_t)self;

    // Get parent goal pointer
    void* goal = *(void**)(s + 0x18);

    // Cast to TimeGoal via IsDerivedFrom
    void* timeGoal = nullptr;
    if (s_fn_IsDerivedFrom_dispatch && goal && s_TypeInfo_TimeGoal) {
        bool isDerived = s_fn_IsDerivedFrom_dispatch(goal, *s_TypeInfo_TimeGoal);
        if (isDerived) {
            timeGoal = goal;
        }
    }

    // Convert elapsed time to seconds
    float elapsedSeconds = 0.0f;
    if (s_fn_TimeAsSeconds) {
        elapsedSeconds = s_fn_TimeAsSeconds((void*)(s + 0x20));
    }

    // Compare against required time
    if (timeGoal) {
        int requiredYears = *(int32_t*)((uintptr_t)timeGoal + 0x30);
        float requiredSeconds = (float)(requiredYears * 0xe10);
        return (requiredSeconds <= elapsedSeconds) ? 1 : 0;
    }

    return 0;
}
