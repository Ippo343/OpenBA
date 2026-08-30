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

#include "Goal/timegoalinstance_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
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

    // Call base GoalInstance::Persist.
    //
    // ⚠ This guard is not a safety net, it is a STREAM HAZARD. Retail calls the
    // base unconditionally, and the base reads an ExternalPtr + a uint off the
    // stream, so skipping it consumes fewer bytes than the save wrote and
    // desyncs everything after this object. Slot 0x140152868 was unregistered
    // for exactly this reason and the skip was silent: our own saves skipped the
    // same bytes on the SAVE leg, so the round trip stayed self-consistent and
    // only a RETAIL-authored save (the packaged tutorial) exposed it -- as a
    // crash three objects later in InternalFactory::PersistObject. Say so loudly
    // rather than quietly dropping a read.
    if (s_fn_GoalInstancePersist) {
        s_fn_GoalInstancePersist(self, stream);
    } else {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            fprintf(stderr, "[TimeGoalInstance] *** GoalInstance::Persist is NULL "
                            "(dispatch slot 0x140152868 unregistered) — the base "
                            "fields are NOT being serialized and the stream is now "
                            "DESYNCED. See standalone/reg_goal.cpp.\n");
            fflush(stderr);
        }
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

// ============================================================
// In-place constructor used by both Factory and CreateInPlace.
// Original TimeGoalInstance is allocated by TimeGoal_Slot10 in the running
// game; the loader path needs an explicit factory because the .rsc loader
// calls Factory::Construct(typeId) for instances that are stored standalone.
//
// Layout: vtable + GoalInstance(0x18) + elapsedTime int64 (+0x20) = 0x28 bytes.
// ============================================================
static void* TimeGoalInstance_Ctor_Impl(void* memory) {
    if (!memory) return nullptr;

    // Call base Achievement::GoalInstance::GoalInstance
    typedef void* (*fn_GoalInstanceCtor_t)(void* self);
    static fn_GoalInstanceCtor_t s_fn_GoalInstanceCtor = nullptr;
    static bool s_resolved = false;
    if (!s_resolved) {
        s_fn_GoalInstanceCtor = (fn_GoalInstanceCtor_t)ResolveExport(
            "??0GoalInstance@Achievement@@QEAA@XZ");
        s_resolved = true;
    }
    if (s_fn_GoalInstanceCtor) {
        s_fn_GoalInstanceCtor(memory);
    }

    uintptr_t s = (uintptr_t)memory;

    // Set vtable to TimeGoalInstance
    uintptr_t vtableAddr = ResolveAddr(Module::Exe, 0x140160598);
    if (vtableAddr) {
        *(void**)memory = (void*)vtableAddr;
    }

    // Initialize elapsedTime to 0
    *(int64_t*)(s + 0x20) = 0;

    return memory;
}


#include "Object/factory_registration.h"
#include "address_registry.h"
#include "Goal/timegoal_funcs.h"  // for TimeGoal_Slot11 (shared dtor)

// ============================================================
// TimeGoalInstance vtable (exe addr 0x140160598, 7 slots)
// ============================================================
// Slot +0x00 Dtor: SHARED with TimeGoal_Slot11 — see timegoal.cpp.
//   The shared dtor sets vtable to TimeGoalInstance, calls
//   GoalInstance::~GoalInstance, optionally frees. Declared in timegoal_funcs.h.

// Slot +0x10 GetTypeId: returns CRC32 typeId from REGISTER_TYPE storage.
static uint32_t TimeGoalInstance_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TimeGoalInstance");
    return cached;
}

// Trampoline for inline-defined TimeGoalInstance_Slot4.
static int TimeGoalInstance_Slot4_Trampoline(TimeGoalInstance* self) {
    return TimeGoalInstance_Slot4(self);
}

static void* g_TimeGoalInstance_Vtable[7] = {
    reinterpret_cast<void*>(&TimeGoal_Slot11),                        // +0x00 dtor (SHARED)
    nullptr,                                                          // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&TimeGoalInstance_GetTypeId),             // +0x10 GetTypeId
    reinterpret_cast<void*>(&TimeGoalInstance_Slot3),                 // +0x18 Slot3 / Serialize
    reinterpret_cast<void*>(&TimeGoalInstance_Slot4_Trampoline),      // +0x20 Slot4 (returns 0)
    reinterpret_cast<void*>(&TimeGoalInstance_Slot5),                 // +0x28 Slot5 / Update
    reinterpret_cast<void*>(&TimeGoalInstance_Slot6),                 // +0x30 Slot6 / IsComplete
};

extern "C" void RegisterTimeGoalInstanceVtable() {
    RegisterExeAddr(0x140160598, reinterpret_cast<uintptr_t>(g_TimeGoalInstance_Vtable));
}

extern "C" void* TimeGoalInstance_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);  // TimeGoalInstance size = 0x28
    if (!p) return nullptr;
    return TimeGoalInstance_Ctor_Impl(p);
}

extern "C" void* TimeGoalInstance_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return TimeGoalInstance_Ctor_Impl(memory);
}

REGISTER_TYPE(TimeGoalInstance, "Achievement::GoalInstance",
              &TimeGoalInstance_Factory, &TimeGoalInstance_CreateInPlace);
