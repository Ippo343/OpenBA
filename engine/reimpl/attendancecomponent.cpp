// engine/reimpl/attendancecomponent.cpp
// Reimplementation of AttendanceComponent::Update from Application-x64.exe.
//
// AttendanceComponent manages a queue of citizens waiting to attend a building
// (e.g., church, tavern, school). When the building is idle (not busy), the
// component processes the attendance queue.
//
// Update logic (0x140044370, 22 instructions):
//   1. Check if queue count (field30 as int at +0x30) is nonzero; early-out if empty
//   2. Get the entity's building component via helper FUN_140002e10
//   3. Check if building is idle (building+0x60 == 0)
//   4. If idle, call FUN_140043f60 to process the attendance queue:
//      - Iterates queued citizens (field28 = queue array ptr, field30 = count)
//      - For each citizen, checks building assignments at +0x208, +0x228, +0x230
//      - Marks attended via helper calls
//
// Strategy: Full reimplementation of the top-level dispatch. The queue
// processing helper (FUN_140043f60) is called via resolved function pointer
// since it interacts with building assignment slots.
//
// Original: 0x140044370

#include "AttendanceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// Object::InternalBasePtr::CheckPtr — validates smart pointer (no-op in release)
// Resolved via import table indirect call at [0x140152ab0].
typedef void (*fn_CheckPtr_t)(void* internalBasePtrAddr);
static fn_CheckPtr_t fn_CheckPtr = nullptr;

// FUN_140002e10(entityPtr, outBuildingPtr) — get building component from entity
// Looks up a component by type on the entity.
// RCX = entity (Internal*), RDX = out InternalBasePtr*
typedef void (*fn_GetBuildingComponent_t)(void* entity, void* outPtr);
static fn_GetBuildingComponent_t fn_GetBuildingComponent = nullptr;

// FUN_140043f60(self) — process attendance queue
// Iterates queued citizens, checks building assignment slots, marks attended.
typedef void (*fn_ProcessAttendanceQueue_t)(void* self);
static fn_ProcessAttendanceQueue_t fn_ProcessAttendanceQueue = nullptr;

// ============================================================
// Update (0x140044370)
//
// Parameters: RCX = this (AttendanceComponent*)
// Note: This Update does NOT take a deltaTime parameter (single-arg).
// ============================================================
static void Reimpl_Update(AttendanceComponent* self) {
    g_lastHook = "AttendanceComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[AttendanceComponent::Update] self=%p queueCount=%d\n",
               (void*)self,
               *reinterpret_cast<int32_t*>(
                   reinterpret_cast<uintptr_t>(self) + 0x30));
        callCount++;
    }
#endif

    // Check if queue count at +0x30 (as int32) is nonzero
    int32_t queueCount = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x30);
    if (queueCount == 0)
        return;

    // CheckPtr on owner smart pointer at +0x10
    fn_CheckPtr(reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(self) + 0x10));

    // Get entity pointer
    void* entity = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    // Get building component from entity
    int64_t buildingPtr = 0;
    fn_GetBuildingComponent(entity, &buildingPtr);

    // CheckPtr on returned building pointer
    fn_CheckPtr(&buildingPtr);

    // Check if building is idle (building+0x60 == 0)
    int32_t busyState = *reinterpret_cast<int32_t*>(buildingPtr + 0x60);

#ifdef _DEBUG
    if (callCount <= 5) {
        printf("[AttendanceComponent::Update] building=%p busyState=%d\n",
               (void*)buildingPtr, busyState);
    }
#endif

    if (busyState == 0) {
        // Building is idle — process the attendance queue
        fn_ProcessAttendanceQueue(self);
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAttendanceComponentUpdate() {
    // Resolve CheckPtr — it's an indirect call through import table at [0x140152ab0]
    // Read the function pointer from the IAT entry
    uintptr_t iatEntry = ResolveAddr(Module::Exe, 0x140152ab0);
    if (iatEntry) {
        fn_CheckPtr = *reinterpret_cast<fn_CheckPtr_t*>(iatEntry);
    }

    fn_GetBuildingComponent = reinterpret_cast<fn_GetBuildingComponent_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    fn_ProcessAttendanceQueue = reinterpret_cast<fn_ProcessAttendanceQueue_t>(
        ResolveAddr(Module::Exe, 0x140043f60));

    if (!fn_CheckPtr || !fn_GetBuildingComponent || !fn_ProcessAttendanceQueue) {
#ifdef _DEBUG
        printf("[AttendanceComponent] ERROR: failed to resolve helpers, skipping hook\n");
        printf("[AttendanceComponent]   CheckPtr = %p\n", (void*)fn_CheckPtr);
        printf("[AttendanceComponent]   GetBuildingComponent = %p\n", (void*)fn_GetBuildingComponent);
        printf("[AttendanceComponent]   ProcessAttendanceQueue = %p\n", (void*)fn_ProcessAttendanceQueue);
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140044370);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x30],0(4) + MOV RDI,RCX(3) = 13 bytes
    // Instruction boundary at 13 (before JZ).
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[AttendanceComponent] Update hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveAttendanceComponentUpdate() { RemoveHook(&g_hookUpdate); }
