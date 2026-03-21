// engine/reimpl/createrawmaterial.cpp
// Hook wrappers for CreateRawMaterial action from Application-x64.exe.
//
// CreateRawMaterial -- 0x58 bytes. Spawns raw material entities from a resource building.
// Inherits: Object::Internal -> Action (0x20) -> CreateRawMaterial
// Vtable at 0x14015fd10 (10 slots).
//
// Overrides vs Action base:
//   Serialize    (0x1400c6f30) -- REIMPLEMENTED
//   AbstractImpl (0x1400c6b40) -- REIMPLEMENTED
//   GetTarget    (0x1400c6aa0) -- REIMPLEMENTED
//   Slots 5-8 (Execute, Enter, Exit, CheckEntity): BASE

#include "CreateRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/createrawmaterial_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool CreateRawMaterial_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;

// ============================================================
// CreateRawMaterial::Serialize (0x1400c6f30) -- reimplemented
// ============================================================
static void Reimpl_Serialize(CreateRawMaterial* self, void* stream) {
    g_lastHook = "CreateRawMaterial::Serialize";
    CreateRawMaterial_Serialize_Impl(self, stream);
}

// ============================================================
// CreateRawMaterial::AbstractImpl (0x1400c6b40) -- reimplemented
// ============================================================
static int Reimpl_AbstractImpl(CreateRawMaterial* self, void* actionStack) {
    g_lastHook = "CreateRawMaterial::AbstractImpl";
    return CreateRawMaterial_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallCreateRawMaterialSerialize() {
    if (!CreateRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CreateRawMaterial] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6f30);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[CreateRawMaterial] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveCreateRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallCreateRawMaterialAbstractImpl() {
    if (!CreateRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[CreateRawMaterial] ERROR: Failed to resolve helpers for AbstractImpl\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6b40);
    // Prologue: PUSH(1)+PUSH(1)+PUSH(1)+PUSH(2)+PUSH(2)+PUSH(2)+MOV(3)+SUB(7) = 19 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 22);
#ifdef _DEBUG
    printf("[CreateRawMaterial] AbstractImpl hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveCreateRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}
