// engine/reimpl/showtool.cpp
// Hook wrappers for ShowTool action from Application-x64.exe.
//
// ShowTool -- 0x30 bytes. Shows a tool UI for a citizen.
// Inherits: Object::Internal -> Action (0x20) -> ShowTool
// Vtable at 0x1401589b0 (10 slots).
//
// Vtable (10 slots):
//   [0]  +0x00  ScalarDtor    = 0x1400c49f0   (base Action, not hooked)
//   [1]  +0x08  VectorDtor    = 0x1400c49a0   (base Action, not hooked)
//   [2]  +0x10  BaseDtor      = 0x1400c4990   (base Action, not hooked)
//   [3]  +0x18  Serialize     = 0x140042780   <-- reimplemented
//   [4]  +0x20  AbstractImpl  = 0x140042710   <-- reimplemented (_purecall)
//   [5]  +0x28  Execute       = 0x1400c4a90   (base Action, not hooked)
//   [6]  +0x30  Enter         = 0x1400c4b90   (base Action, not hooked)
//   [7]  +0x38  Exit          = 0x140042700   (3 bytes, unhookable -- in clean code only)
//   [8]  +0x40  CheckEntity   = 0x1400426d0   <-- reimplemented
//   [9]  +0x48  GetTarget     = 0x140042680   <-- reimplemented

#include "ShowTool.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/showtool_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool ShowTool_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// ShowTool::Serialize (0x140042780) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "ShowTool::Serialize";
    ShowTool_Serialize_Impl(self, stream);
}

// ============================================================
// ShowTool::AbstractImpl / _purecall (0x140042710) -- reimplemented
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "ShowTool::Purecall";
    return ShowTool_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// ShowTool::CheckEntity (0x1400426d0) -- reimplemented
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "ShowTool::CheckEntity";
    ShowTool_CheckEntity_Impl(self);
}

// ============================================================
// ShowTool::GetTarget (0x140042680) -- reimplemented
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "ShowTool::GetTarget";
    return ShowTool_GetTarget_Impl(self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallShowToolSerialize() {
    if (!ShowTool_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ShowTool] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042780);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ShowTool] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolSerialize() { RemoveHook(&g_hookSerialize); }

void InstallShowToolPurecall() {
    if (!ShowTool_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ShowTool] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042710);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[ShowTool] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolPurecall() { RemoveHook(&g_hookPurecall); }

void InstallShowToolCheckEntity() {
    if (!ShowTool_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ShowTool] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400426d0);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[ShowTool] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallShowToolGetTarget() {
    if (!ShowTool_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ShowTool] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042680);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 14);
#ifdef _DEBUG
    printf("[ShowTool] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveShowToolGetTarget() { RemoveHook(&g_hookGetTarget); }
