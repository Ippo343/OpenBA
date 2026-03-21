// engine/reimpl/removenaturalresource.cpp
// Hook wrappers for RemoveNaturalResource action from Application-x64.exe.
//
// RemoveNaturalResource — 0x38 bytes. Removes a natural resource entity.
// Inherits: Object::Internal -> Action (0x20) -> RemoveNaturalResource
// Vtable at 0x140160050 (10 slots).
//
// Overrides vs Action base:
//   Serialize   (0x1400cac50) — serializes status, entity(+0x20), entity(+0x28), bool(+0x30)
//   AbstractImpl(0x1400cab10) — checks resource ownership, sets harvest flags, calls removal
//   CheckEntity (0x1400caaa0) — clears "in use" bit on model component at +0x28
//   GetTarget   (0x1400caa20) — matches +0x28 or entity parent at +0x20
//   Execute/Enter/Exit use base.

#include "RemoveNaturalResource.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/removenaturalresource_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool RemoveNaturalResource_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookCheckEntity;
static HookInfo g_hookGetTarget;

// ============================================================
// RemoveNaturalResource::Serialize (0x1400cac50) — reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "RemoveNaturalResource::Serialize";
    RemoveNaturalResource_Serialize_Impl((RemoveNaturalResource*)self, stream);
}

// ============================================================
// RemoveNaturalResource::AbstractImpl (0x1400cab10) — reimplemented
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "RemoveNaturalResource::Purecall";
    return RemoveNaturalResource_AbstractImpl_Impl((RemoveNaturalResource*)self, actionStack);
}

// ============================================================
// RemoveNaturalResource::CheckEntity (0x1400caaa0) — reimplemented
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "RemoveNaturalResource::CheckEntity";
    RemoveNaturalResource_CheckEntity_Impl((RemoveNaturalResource*)self);
}

// ============================================================
// RemoveNaturalResource::GetTarget (0x1400caa20) — reimplemented
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "RemoveNaturalResource::GetTarget";
    return RemoveNaturalResource_GetTarget_Impl((RemoveNaturalResource*)self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallRemoveNaturalResourceSerialize() {
    if (!RemoveNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveNaturalResource] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cac50);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourceSerialize() { RemoveHook(&g_hookSerialize); }

void InstallRemoveNaturalResourcePurecall() {
    if (!RemoveNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveNaturalResource] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cab10);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 15);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] Purecall hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourcePurecall() { RemoveHook(&g_hookPurecall); }

void InstallRemoveNaturalResourceCheckEntity() {
    if (!RemoveNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveNaturalResource] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400caaa0);
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] CheckEntity hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourceCheckEntity() { RemoveHook(&g_hookCheckEntity); }

void InstallRemoveNaturalResourceGetTarget() {
    if (!RemoveNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[RemoveNaturalResource] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400caa20);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 15);
#ifdef _DEBUG
    printf("[RemoveNaturalResource] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveRemoveNaturalResourceGetTarget() { RemoveHook(&g_hookGetTarget); }
