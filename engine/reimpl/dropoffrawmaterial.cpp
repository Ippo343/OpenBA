// engine/reimpl/dropoffrawmaterial.cpp
// Hook wrappers for DropOffRawMaterial action from Application-x64.exe.
//
// DropOffRawMaterial — 0x40 bytes. Drops off raw material to storage.
// Inherits: Object::Internal -> Action (0x20) -> DropOffRawMaterial
// Vtable at 0x14015fd80 (10 slots).
//
// Overrides vs Action base:
//   Serialize    (0x1400c7b10) — serializes status, entity refs, resource, flag, count
//   AbstractImpl (0x1400c7300) — complex resource transfer logic
//   GetTarget    (0x1400c7270) — checks +0x38 and +0x30->+0x10
//   CheckEntity  (0x1400c72f0) — JMP thunk to 0x1400c7570 (drops all inventory, tiny/unhookable)
//   Execute/Enter/Exit use base.

#include "DropOffRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/dropoffrawmaterial_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool DropOffRawMaterial_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;

// ============================================================
// DropOffRawMaterial::Serialize (0x1400c7b10) — reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "DropOffRawMaterial::Serialize";
    DropOffRawMaterial_Serialize_Impl((DropOffRawMaterial*)self, stream);
}

// ============================================================
// DropOffRawMaterial::AbstractImpl (0x1400c7300) — reimplemented
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_AbstractImpl(void* self, void* actionStack) {
    g_lastHook = "DropOffRawMaterial::AbstractImpl";
    return DropOffRawMaterial_AbstractImpl_Impl((DropOffRawMaterial*)self, actionStack);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallDropOffRawMaterialSerialize() {
    if (!DropOffRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[DropOffRawMaterial] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c7b10);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[DropOffRawMaterial] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveDropOffRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallDropOffRawMaterialAbstractImpl() {
    if (!DropOffRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[DropOffRawMaterial] ERROR: Failed to resolve helpers for AbstractImpl\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c7300);
    // Prologue: MOV(5)+MOV(5)+PUSH(1)+PUSH(1)+PUSH(1) = 13 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 13);
#ifdef _DEBUG
    printf("[DropOffRawMaterial] AbstractImpl hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveDropOffRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}
