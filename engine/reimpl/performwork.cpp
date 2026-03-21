// engine/reimpl/performwork.cpp
// Hook wrappers for PerformWork action from Application-x64.exe.
//
// PerformWork is a generic work-performing action that animates a citizen
// performing work at a building. It tracks elapsed time vs required time,
// handles facing direction via yaw, and optionally decrements tool durability.
//
// Struct size: 0x48, vtable at 0x14015fea0
// Inherits: Object::Internal -> Action -> PerformWork
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c9550 -- REIMPLEMENTED
//   Slot 4 (AbstractImpl): 0x1400c9420 -- REIMPLEMENTED
//   Slot 5 (Execute):      0x1400c93c0 -- REIMPLEMENTED
//   Slot 6 (Enter):        BASE
//   Slot 7 (Exit):         0x1400c9370 -- REIMPLEMENTED
//   Slot 8 (CheckEntity):  0x1400c9320 -- REIMPLEMENTED
//   Slot 9 (GetTarget):    0x1400c92d0 -- not hooked (too small prologue)

#include "PerformWork.h"
#include "hooks.h"
#include "resolve.h"
#include "Action/performwork_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool PerformWork_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;
static HookInfo g_hookExecute;
static HookInfo g_hookExit;
static HookInfo g_hookCheckEntity;

// ============================================================
// PerformWork::Serialize -- reimplemented
// ============================================================

static void Reimpl_Serialize(PerformWork* self, void* stream) {
    g_lastHook = "PerformWork::Serialize";
    PerformWork_Serialize_Impl(self, stream);
}

// ============================================================
// PerformWork::AbstractImpl -- reimplemented
// Sets animation, facing direction, and optionally decrements tool durability
// ============================================================

static int Reimpl_AbstractImpl(PerformWork* self, void* actionStack) {
    g_lastHook = "PerformWork::AbstractImpl";
    return PerformWork_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// PerformWork::Execute -- reimplemented
// Accumulates elapsed time (+0x38) vs required time (+0x30),
// returns 0 when done, 1 when still working, 2 on break needed
// ============================================================

static int Reimpl_Execute(PerformWork* self, void* actionStack, void* deltaTime) {
    g_lastHook = "PerformWork::Execute";
    return PerformWork_Execute_Impl(self, actionStack, deltaTime);
}

// ============================================================
// PerformWork::Exit -- reimplemented
// Resets character animation to idle
// ============================================================

static void Reimpl_Exit(PerformWork* self, void* actionStack) {
    g_lastHook = "PerformWork::Exit";
    PerformWork_Exit_Impl(self, actionStack);
}

// ============================================================
// PerformWork::CheckEntity -- reimplemented
// Resets character animation (same as Exit)
// ============================================================

static void Reimpl_CheckEntity(PerformWork* self) {
    g_lastHook = "PerformWork::CheckEntity";
    PerformWork_CheckEntity_Impl(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPerformWorkSerialize() {
    PerformWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9550);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[PerformWork] Serialize hooked at %p (prologue=20, reimplemented)\n", (void*)target);
#endif
}

void RemovePerformWorkSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallPerformWorkAbstractImpl() {
    PerformWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9420);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(1)+SUB(4)+MOV(3) = 15 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 15);
#ifdef _DEBUG
    printf("[PerformWork] AbstractImpl hooked at %p (prologue=15, reimplemented)\n", (void*)target);
#endif
}

void RemovePerformWorkAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallPerformWorkExecute() {
    PerformWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c93c0);
    // Prologue: SUB(4)+MOV(3)+MOV(4)+CMP(4) = 15 bytes
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 15);
#ifdef _DEBUG
    printf("[PerformWork] Execute hooked at %p (prologue=15, reimplemented)\n", (void*)target);
#endif
}

void RemovePerformWorkExecute() {
    RemoveHook(&g_hookExecute);
}

void InstallPerformWorkExit() {
    PerformWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9370);
    // Prologue: PUSH(2)+SUB(4)+LEA(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_Exit, &g_hookExit, 13);
#ifdef _DEBUG
    printf("[PerformWork] Exit hooked at %p (prologue=13, reimplemented)\n", (void*)target);
#endif
}

void RemovePerformWorkExit() {
    RemoveHook(&g_hookExit);
}

void InstallPerformWorkCheckEntity() {
    PerformWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9320);
    // Prologue: PUSH(2)+SUB(4)+LEA(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[PerformWork] CheckEntity hooked at %p (prologue=13, reimplemented)\n", (void*)target);
#endif
}

void RemovePerformWorkCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
