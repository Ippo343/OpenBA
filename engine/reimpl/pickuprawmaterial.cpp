// engine/reimpl/pickuprawmaterial.cpp
// Hook wrappers for PickupRawMaterial action from Application-x64.exe.
//
// PickupRawMaterial -- 0x30 bytes. Picks up a raw material from a source entity.
// Inherits: Object::Internal -> Action (0x20) -> PickupRawMaterial
// Vtable at 0x14015ff10 (10 slots).
//
// Overrides vs Action base:
//   Serialize    (0x1400c99e0) -- serializes status, entity refs
//   AbstractImpl (0x1400c98d0) -- picks up raw material, transfers to storage
//   GetTarget    (0x1400c9860) -- checks +0x28 and +0x20->parent(+0x10)
//   Execute/Enter/Exit/CheckEntity use base.

#include "PickupRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/pickuprawmaterial_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool PickupRawMaterial_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookPurecall;
static HookInfo g_hookGetTarget;

// ============================================================
// PickupRawMaterial::Serialize (0x1400c99e0) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PickupRawMaterial::Serialize";
    PickupRawMaterial_Serialize_Impl((PickupRawMaterial*)self, stream);
}

// ============================================================
// PickupRawMaterial::AbstractImpl (0x1400c98d0) -- reimplemented
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_Purecall(void* self, void* actionStack) {
    g_lastHook = "PickupRawMaterial::Purecall";
    return PickupRawMaterial_AbstractImpl_Impl((PickupRawMaterial*)self, actionStack);
}

// ============================================================
// PickupRawMaterial::GetTarget (0x1400c9860) -- reimplemented
// ============================================================
static bool Reimpl_GetTarget(void* self, void* param) {
    g_lastHook = "PickupRawMaterial::GetTarget";
    return PickupRawMaterial_GetTarget_Impl((PickupRawMaterial*)self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPickupRawMaterialSerialize() {
    if (!PickupRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PickupRawMaterial] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c99e0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[PickupRawMaterial] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePickupRawMaterialSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPickupRawMaterialPurecall() {
    if (!PickupRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PickupRawMaterial] ERROR: Failed to resolve helpers for Purecall\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c98d0);
    InstallHook(target, (void*)Reimpl_Purecall, &g_hookPurecall, 13);
#ifdef _DEBUG
    printf("[PickupRawMaterial] Purecall hooked at %p\n", (void*)target);
#endif
}
void RemovePickupRawMaterialPurecall() { RemoveHook(&g_hookPurecall); }

void InstallPickupRawMaterialGetTarget() {
    if (!PickupRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PickupRawMaterial] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9860);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 13);
#ifdef _DEBUG
    printf("[PickupRawMaterial] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemovePickupRawMaterialGetTarget() { RemoveHook(&g_hookGetTarget); }
