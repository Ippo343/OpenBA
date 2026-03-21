// engine/reimpl/animatenaturalresource.cpp
// Hook wrappers for AnimateNaturalResource action from Application-x64.exe.
//
// AnimateNaturalResource — 0x30 bytes. Triggers and waits for animation on a natural resource.
// Inherits: Object::Internal -> Action (0x20) -> AnimateNaturalResource
// Vtable at 0x14015fb10 (10 slots).
//
// Vtable:
//   [0] +0x00  ScalarDtor    = 0x140021e30   (RIP-relative, bug class 2, unhookable)
//   [1] +0x08  VectorDtor    = 0x140021db0   (not hooked)
//   [2] +0x10  BaseDtor      = 0x140021da0   (not hooked)
//   [3] +0x18  Serialize     = 0x1400c5660   <-- reimplemented
//   [4] +0x20  AbstractImpl  = 0x1400c5600   <-- reimplemented
//   [5] +0x28  Execute       = 0x1400c55b0   <-- reimplemented
//   [6] +0x30  Enter         = 0x1400c4b90   (base Action, no-op)
//   [7] +0x38  Exit          = 0x1400c4ba0   (base Action, no-op)
//   [8] +0x40  CheckEntity   = 0x1400c55a0   (3 bytes, ret 0, too small to hook)
//   [9] +0x48  GetTarget     = 0x1400c5560   <-- reimplemented

#include "AnimateNaturalResource.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/animatenaturalresource_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool AnimateNaturalResource_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookExecute;
static HookInfo g_hookGetTarget;

// ============================================================
// AnimateNaturalResource::Serialize (0x1400c5660) — reimplemented
// ============================================================
static void Reimpl_Serialize(AnimateNaturalResource* self, void* stream) {
    g_lastHook = "AnimateNaturalResource::Serialize";
    AnimateNaturalResource_Serialize_Impl(self, stream);
}

// ============================================================
// AnimateNaturalResource::AbstractImpl (0x1400c5600) — reimplemented
// Returns int (bug class 7).
// ============================================================
static int Reimpl_Purecall(AnimateNaturalResource* self, void* actionStack) {
    g_lastHook = "AnimateNaturalResource::Purecall";
    return AnimateNaturalResource_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// AnimateNaturalResource::Execute (0x1400c55b0) — reimplemented
// Returns int (bug class 7).
// ============================================================
static int Reimpl_Execute(AnimateNaturalResource* self, void* actionStack, void* time) {
    g_lastHook = "AnimateNaturalResource::Execute";
    return AnimateNaturalResource_Execute_Impl(self, actionStack, time);
}

// ============================================================
// AnimateNaturalResource::GetTarget (0x1400c5560) — reimplemented
// ============================================================
static bool Reimpl_GetTarget(AnimateNaturalResource* self, void* param) {
    g_lastHook = "AnimateNaturalResource::GetTarget";
    return AnimateNaturalResource_GetTarget_Impl(self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallAnimateNaturalResourceSerialize() {
    if (!AnimateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[AnimateNaturalResource] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5660);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourceSerialize() { RemoveHook(&g_hookSerialize); }

void InstallAnimateNaturalResourcePurecall() {
    if (!AnimateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[AnimateNaturalResource] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5600);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourcePurecall() { RemoveHook(&g_hookPurecall); }

void InstallAnimateNaturalResourceExecute() {
    if (!AnimateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[AnimateNaturalResource] ERROR: Failed to resolve helpers for Execute\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c55b0);
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 13);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] Execute hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourceExecute() { RemoveHook(&g_hookExecute); }

void InstallAnimateNaturalResourceGetTarget() {
    if (!AnimateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[AnimateNaturalResource] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5560);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[AnimateNaturalResource] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveAnimateNaturalResourceGetTarget() { RemoveHook(&g_hookGetTarget); }
