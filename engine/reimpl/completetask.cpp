// engine/reimpl/completetask.cpp
// Hook wrappers for CompleteTask action from Application-x64.exe.
//
// CompleteTask — 0x38 bytes. Completes a task/work order on a building.
// Inherits: Object::Internal -> Action (0x20) -> CompleteTask
// Vtable at 0x14015fbd0 (10 slots).
//
// Vtable:
//   [0] +0x00  ScalarDtor    = 0x140022030   (RIP-relative, bug class 2, unhookable)
//   [1] +0x08  VectorDtor    = 0x140021fb0   (not hooked)
//   [2] +0x10  BaseDtor      = 0x140021fa0   (not hooked)
//   [3] +0x18  Serialize     = 0x1400c5be0   <-- reimplemented
//   [4] +0x20  AbstractImpl  = 0x1400c5b40   <-- reimplemented
//   [5] +0x28  Execute       = 0x1400c4a90   (base Action, returns 0)
//   [6] +0x30  Enter         = 0x1400c4b90   (base Action, no-op)
//   [7] +0x38  Exit          = 0x1400c4ba0   (base Action, no-op)
//   [8] +0x40  CheckEntity   = 0x1400c5ac0   <-- reimplemented
//   [9] +0x48  GetTarget     = 0x1400c5a50   <-- reimplemented

#include "CompleteTask.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/completetask_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool CompleteTask_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// CompleteTask::Serialize (0x1400c5be0) — reimplemented
// ============================================================
static void Reimpl_Serialize(CompleteTask* self, void* stream) {
    g_lastHook = "CompleteTask::Serialize";
    CompleteTask_Serialize_Impl(self, stream);
}

// ============================================================
// CompleteTask::AbstractImpl (0x1400c5b40) — reimplemented
// Returns int (bug class 7).
// ============================================================
static int Reimpl_Purecall(CompleteTask* self, void* actionStack) {
    g_lastHook = "CompleteTask::Purecall";
    return CompleteTask_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// CompleteTask::CheckEntity (0x1400c5ac0) — reimplemented
// ============================================================
static void Reimpl_CheckEntity(CompleteTask* self) {
    g_lastHook = "CompleteTask::CheckEntity";
    CompleteTask_CheckEntity_Impl(self);
}

// ============================================================
// CompleteTask::GetTarget (0x1400c5a50) — reimplemented
// ============================================================
static bool Reimpl_GetTarget(CompleteTask* self, void* param) {
    g_lastHook = "CompleteTask::GetTarget";
    return CompleteTask_GetTarget_Impl(self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallCompleteTaskSerialize() {
    if (!CompleteTask_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CompleteTask] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5be0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CompleteTask] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCompleteTaskPurecall() {
    if (!CompleteTask_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CompleteTask] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5b40);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[CompleteTask] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskPurecall() { RemoveHook(&g_hookPurecall); }

void InstallCompleteTaskCheckEntity() {
    if (!CompleteTask_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CompleteTask] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5ac0);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[CompleteTask] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallCompleteTaskGetTarget() {
    if (!CompleteTask_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CompleteTask] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5a50);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[CompleteTask] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveCompleteTaskGetTarget() { RemoveHook(&g_hookGetTarget); }
