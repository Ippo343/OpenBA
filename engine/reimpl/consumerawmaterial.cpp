// engine/reimpl/consumerawmaterial.cpp
// Hook wrappers for ConsumeRawMaterial action from Application-x64.exe.
//
// ConsumeRawMaterial -- 0x40 bytes. Consumes raw materials from entity's inventory.
// Inherits: Object::Internal -> Action (0x20) -> ConsumeRawMaterial
// Vtable at 0x14015fc20 (10 slots).
//
// Overrides vs Action base:
//   Serialize    (0x1400c6120) -- versioned serialization with entity ref + material list
//   AbstractImpl (0x1400c5f50) -- consume materials or check availability
//   CheckEntity  (0x1400c5e30) -- undo consumption if consume mode
//   GetTarget    (0x1400c5f10) -- simple compare with +0x20 (not hooked)
//   Execute/Enter/Exit use base.

#include "ConsumeRawMaterial.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/consumerawmaterial_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool ConsumeRawMaterial_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;
static HookInfo g_hookCheckEntity;

// ============================================================
// ConsumeRawMaterial::Serialize (0x1400c6120) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "ConsumeRawMaterial::Serialize";
    ConsumeRawMaterial_Serialize_Impl((ConsumeRawMaterial*)self, stream);
}

// ============================================================
// ConsumeRawMaterial::AbstractImpl (0x1400c5f50) -- reimplemented
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_AbstractImpl(void* self, void* actionStack) {
    g_lastHook = "ConsumeRawMaterial::AbstractImpl";
    return ConsumeRawMaterial_AbstractImpl_Impl((ConsumeRawMaterial*)self, actionStack);
}

// ============================================================
// ConsumeRawMaterial::CheckEntity (0x1400c5e30) -- reimplemented
// ============================================================
static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "ConsumeRawMaterial::CheckEntity";
    ConsumeRawMaterial_CheckEntity_Impl((ConsumeRawMaterial*)self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallConsumeRawMaterialSerialize() {
    if (!ConsumeRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ConsumeRawMaterial] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c6120);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[ConsumeRawMaterial] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveConsumeRawMaterialSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallConsumeRawMaterialAbstractImpl() {
    if (!ConsumeRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ConsumeRawMaterial] ERROR: Failed to resolve helpers for AbstractImpl\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5f50);
    // Prologue: PUSH(2)+SUB(4)+CMP(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 13);
#ifdef _DEBUG
    printf("[ConsumeRawMaterial] AbstractImpl hooked at %p\n", (void*)target);
#endif
}

void RemoveConsumeRawMaterialAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallConsumeRawMaterialCheckEntity() {
    if (!ConsumeRawMaterial_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[ConsumeRawMaterial] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c5e30);
    // Prologue: PUSH(2)+SUB(4)+CMP(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[ConsumeRawMaterial] CheckEntity hooked at %p\n", (void*)target);
#endif
}

void RemoveConsumeRawMaterialCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
