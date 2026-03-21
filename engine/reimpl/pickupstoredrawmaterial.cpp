// engine/reimpl/pickupstoredrawmaterial.cpp
// Hook wrappers for PickupStoredRawMaterial action from Application-x64.exe.
//
// PickupStoredRawMaterial -- 0x48 bytes. Picks up materials from a storage building.
// Inherits: Object::Internal -> Action (0x20) -> PickupStoredRawMaterial
// Vtable at 0x14015ff80 (10 slots).
//
// Overrides vs Action base:
//   Serialize    (0x1400ca0d0) -- serializes status, entity refs, material list, partial flag
//   AbstractImpl (0x1400c9d40) -- complex storage query/transfer
//   CheckEntity  (0x1400c9c70) -- undo pickup on entity remove
//   GetTarget    (0x1400c9bf0) -- checks +0x28 and +0x20->parent(+0x10) (not hooked)
//   Execute/Enter/Exit use base.

#include "PickupStoredRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/pickupstoredrawmaterial_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool PickupStoredRawMaterial_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;
static HookInfo g_hookCheckEntity;

// ============================================================
// PickupStoredRawMaterial::Serialize (0x1400ca0d0) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PickupStoredRawMaterial::Serialize";
    PickupStoredRawMaterial_Serialize_Impl((PickupStoredRawMaterial*)self, stream);
}

// ============================================================
// PickupStoredRawMaterial::AbstractImpl (0x1400c9d40) -- reimplemented
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_AbstractImpl(void* self, void* actionStack) {
    g_lastHook = "PickupStoredRawMaterial::AbstractImpl";
    return PickupStoredRawMaterial_AbstractImpl_Impl((PickupStoredRawMaterial*)self, actionStack);
}

// ============================================================
// PickupStoredRawMaterial::CheckEntity (0x1400c9c70) -- reimplemented
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "PickupStoredRawMaterial::CheckEntity";
    PickupStoredRawMaterial_CheckEntity_Impl((PickupStoredRawMaterial*)self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPickupStoredRawMaterialSerialize() {
    if (!PickupStoredRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PickupStoredRawMaterial] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ca0d0);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemovePickupStoredRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallPickupStoredRawMaterialAbstractImpl() {
    if (!PickupStoredRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PickupStoredRawMaterial] ERROR: Failed to resolve helpers for AbstractImpl\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9d40);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(1)+PUSH(2)+PUSH(2)+PUSH(2)+SUB(4) = 18 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 20);
#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] AbstractImpl hooked at %p\n", (void*)target);
#endif
}

void RemovePickupStoredRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallPickupStoredRawMaterialCheckEntity() {
    if (!PickupStoredRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[PickupStoredRawMaterial] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9c70);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(2)+SUB(4) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[PickupStoredRawMaterial] CheckEntity hooked at %p\n", (void*)target);
#endif
}

void RemovePickupStoredRawMaterialCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
