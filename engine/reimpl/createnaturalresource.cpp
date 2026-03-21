// engine/reimpl/createnaturalresource.cpp
// Hook wrappers for CreateNaturalResource action from Application-x64.exe.
//
// CreateNaturalResource -- 0x38 bytes. Creates a natural resource entity in the world.
// Inherits: Object::Internal -> Action (0x20) -> CreateNaturalResource
// Vtable at 0x14015fca0 (10 slots).
//
// Overrides vs Action base:
//   Serialize  (0x1400c6890) -- REIMPLEMENTED
//   _purecall  (0x1400c6650) -- REIMPLEMENTED (AbstractImpl)
//   GetTarget  (0x1400c6610) -- REIMPLEMENTED
//   Execute/Enter/Exit/CheckEntity use base.

#include "CreateNaturalResource.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/createnaturalresource_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool CreateNaturalResource_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookGetTarget;

// ============================================================
// CreateNaturalResource::Serialize (0x1400c6890) -- reimplemented
// ============================================================
static void Reimpl_Serialize(CreateNaturalResource* self, void* stream) {
    g_lastHook = "CreateNaturalResource::Serialize";
    CreateNaturalResource_Serialize_Impl(self, stream);
}

// ============================================================
// CreateNaturalResource::AbstractImpl (0x1400c6650) -- reimplemented
// ============================================================
static int Reimpl_Purecall(CreateNaturalResource* self, void* actionStack) {
    g_lastHook = "CreateNaturalResource::Purecall";
    return CreateNaturalResource_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// CreateNaturalResource::GetTarget (0x1400c6610) -- reimplemented
// ============================================================
static int Reimpl_GetTarget(CreateNaturalResource* self, void* param) {
    g_lastHook = "CreateNaturalResource::GetTarget";
    return CreateNaturalResource_GetTarget_Impl(self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallCreateNaturalResourceSerialize() {
    if (!CreateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CreateNaturalResource] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6890);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[CreateNaturalResource] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveCreateNaturalResourceSerialize() { RemoveHook(&g_hookSerialize); }

void InstallCreateNaturalResourcePurecall() {
    if (!CreateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CreateNaturalResource] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6650);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 20);
#ifdef _DEBUG
    printf("[CreateNaturalResource] Purecall hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveCreateNaturalResourcePurecall() { RemoveHook(&g_hookPurecall); }

void InstallCreateNaturalResourceGetTarget() {
    if (!CreateNaturalResource_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CreateNaturalResource] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6610);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 14);
#ifdef _DEBUG
    printf("[CreateNaturalResource] GetTarget hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveCreateNaturalResourceGetTarget() { RemoveHook(&g_hookGetTarget); }
