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
#include "Component/attendancecomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Update (0x140044370) -- delegates to clean code
// ============================================================
static void Reimpl_Update(AttendanceComponent* self) {
    g_lastHook = "AttendanceComponent::Update";
    AttendanceComponent_Update(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAttendanceComponentUpdate() {
    AttendanceComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140044370);
    // Prologue: PUSH RDI(2) + SUB RSP,0x20(4) + CMP [RCX+0x30],0(4) + MOV RDI,RCX(3) = 13 bytes
    // Instruction boundary at 13 (before JZ).
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 13);
#ifdef _DEBUG
    printf("[AttendanceComponent] Update hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveAttendanceComponentUpdate() { RemoveHook(&g_hookUpdate); }
