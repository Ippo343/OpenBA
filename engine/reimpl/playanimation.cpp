// engine/reimpl/playanimation.cpp
// Hook wrappers for PlayAnimation action from Application-x64.exe.
//
// PlayAnimation -- 0x40 bytes. Plays an animation on a citizen entity.
// Inherits: Object::Internal -> Action (0x20) -> PlayAnimation
// Vtable at 0x140158950 (10 slots).
//
// Vtable (10 slots):
//   [0]  +0x00  ScalarDtor    = 0x1400c49f0   (base Action, not hooked)
//   [1]  +0x08  VectorDtor    = 0x1400c49a0   (base Action, not hooked)
//   [2]  +0x10  BaseDtor      = 0x1400c4990   (base Action, not hooked)
//   [3]  +0x18  Serialize     = 0x140042480   <-- reimplemented
//   [4]  +0x20  AbstractImpl  = 0x140042420   <-- reimplemented (_purecall)
//   [5]  +0x28  Execute       = 0x140042400   <-- reimplemented
//   [6]  +0x30  Enter         = 0x1400c4b90   (base Action, not hooked)
//   [7]  +0x38  Exit          = 0x1400c4a90   (base Action, not hooked)
//   [8]  +0x40  CheckEntity   = 0x1400423f0   (3 bytes, unhookable -- in clean code only)
//   [9]  +0x48  GetTarget     = 0x1400423b0   <-- reimplemented

#include "PlayAnimation.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/playanimation_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool PlayAnimation_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookExecute;
static HookInfo g_hookGetTarget;

// ============================================================
// PlayAnimation::Serialize (0x140042480) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PlayAnimation::Serialize";
    PlayAnimation_Serialize_Impl(self, stream);
}

// ============================================================
// PlayAnimation::AbstractImpl / _purecall (0x140042420) -- reimplemented
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "PlayAnimation::Purecall";
    return PlayAnimation_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// PlayAnimation::Execute (0x140042400) -- reimplemented
// ============================================================
static int Reimpl_Execute(void* self, void* actionStack, void* time_ptr) {
    g_lastHook = "PlayAnimation::Execute";
    return PlayAnimation_Execute_Impl(self, actionStack, time_ptr);
}

// ============================================================
// PlayAnimation::GetTarget (0x1400423b0) -- reimplemented
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "PlayAnimation::GetTarget";
    return PlayAnimation_GetTarget_Impl(self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPlayAnimationSerialize() {
    if (!PlayAnimation_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PlayAnimation] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042480);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PlayAnimation] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPlayAnimationPurecall() {
    if (!PlayAnimation_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PlayAnimation] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042420);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[PlayAnimation] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationPurecall() { RemoveHook(&g_hookPurecall); }

void InstallPlayAnimationExecute() {
    if (!PlayAnimation_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PlayAnimation] ERROR: Failed to resolve helpers for Execute\n");
#endif
        return;
    }
    // 0x140042400 is 21 bytes total. First instruction boundary >= 12 is at offset 13:
    //   49 8B 00 (3) + 48 01 41 38 (4) + 48 8B 51 38 (4) + 33 C0 (2) = 13
    uintptr_t target = ResolveAddr(Module::Exe, 0x140042400);
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 13);
#ifdef _DEBUG
    printf("[PlayAnimation] Execute hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationExecute() { RemoveHook(&g_hookExecute); }

void InstallPlayAnimationGetTarget() {
    if (!PlayAnimation_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PlayAnimation] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400423b0);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[PlayAnimation] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemovePlayAnimationGetTarget() { RemoveHook(&g_hookGetTarget); }
