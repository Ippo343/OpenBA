// engine/reimpl/performwork.cpp
// Reimplementation of PerformWork action from Application-x64.exe.
//
// PerformWork is a generic work-performing action that animates a citizen
// performing work at a building. It tracks elapsed time vs required time,
// handles facing direction via yaw, and optionally decrements tool durability.
//
// Struct size: 0x48, vtable at 0x14015fea0
// Inherits: Object::Internal -> Action -> PerformWork
//
// Constructor at 0x1400c96b0 inits:
//   +0x20: zeroed (InternalBasePtr - work target)
//   +0x28: zeroed (float - facing yaw)
//   +0x2C: zeroed (int - animation ID)
//   +0x30: zeroed (Time - required duration)
//   +0x38: zeroed (Time - elapsed time)
//   +0x40: zeroed (bool - use tool flag)
//   +0x44: -1 sentinel (int - ?)
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c9550 -- PASSTHROUGH
//   Slot 4 (AbstractImpl): 0x1400c9420 -- PASSTHROUGH (animation + tool logic)
//   Slot 5 (Execute):      0x1400c93c0 -- PASSTHROUGH (time accumulation + completion check)
//   Slot 6 (Enter):        BASE
//   Slot 7 (Exit):         0x1400c9370 -- PASSTHROUGH (reset animation)
//   Slot 8 (CheckEntity):  0x1400c9320 -- PASSTHROUGH (reset animation)
//   Slot 9 (GetTarget):    0x1400c92d0 -- simple: compares entity->+0x10 with param

#include "PerformWork.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;
static HookInfo g_hookExecute;
static HookInfo g_hookExit;
static HookInfo g_hookCheckEntity;

// ============================================================
// Original function typedefs
// ============================================================

typedef void (*PerformWork_Serialize_t)(void* self, void* stream);
typedef uint64_t (*PerformWork_AbstractImpl_t)(void* self, void* actionStack);
typedef uint8_t (*PerformWork_Execute_t)(void* self, void* actionStack, int64_t* deltaTime);
typedef void (*PerformWork_Exit_t)(void* self, void* actionStack);
typedef void (*PerformWork_CheckEntity_t)(void* self);

// ============================================================
// PerformWork::Serialize -- passthrough wrapper
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "PerformWork::Serialize";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[PerformWork::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((PerformWork_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// PerformWork::AbstractImpl -- passthrough wrapper
// Sets animation, facing direction, and optionally decrements tool durability
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self, void* actionStack) {
    g_lastHook = "PerformWork::AbstractImpl";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        float yaw = *(float*)(act + 0x28);
        int32_t animId = *(int32_t*)(act + 0x2C);
        uint8_t useTool = *(uint8_t*)(act + 0x40);
        printf("[PerformWork::AbstractImpl] self=%p yaw=%.2f anim=%d useTool=%d\n",
               self, yaw, animId, useTool);
        callCount++;
    }
#endif
    return ((PerformWork_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self, actionStack);
}

// ============================================================
// PerformWork::Execute -- passthrough wrapper
// Accumulates elapsed time (+0x38) vs required time (+0x30),
// returns 0 when done, 1 when still working, 2 on failure
// ============================================================

static uint8_t Reimpl_Execute(void* self, void* actionStack, int64_t* deltaTime) {
    g_lastHook = "PerformWork::Execute";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[PerformWork::Execute] self=%p dt=%lld\n", self, *deltaTime);
        callCount++;
    }
#endif
    return ((PerformWork_Execute_t)g_hookExecute.trampoline)(self, actionStack, deltaTime);
}

// ============================================================
// PerformWork::Exit -- passthrough wrapper
// Resets character animation to idle
// ============================================================

static void Reimpl_Exit(void* self, void* actionStack) {
    g_lastHook = "PerformWork::Exit";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[PerformWork::Exit] self=%p\n", self);
        callCount++;
    }
#endif
    ((PerformWork_Exit_t)g_hookExit.trampoline)(self, actionStack);
}

// ============================================================
// PerformWork::CheckEntity -- passthrough wrapper
// Resets character animation (same as Exit)
// ============================================================

static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "PerformWork::CheckEntity";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[PerformWork::CheckEntity] self=%p\n", self);
        callCount++;
    }
#endif
    ((PerformWork_CheckEntity_t)g_hookCheckEntity.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallPerformWorkSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9550);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[PerformWork] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemovePerformWorkSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallPerformWorkAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9420);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(1)+SUB(4)+MOV(3) = 15 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 15);
#ifdef _DEBUG
    printf("[PerformWork] AbstractImpl hooked at %p (prologue=15, passthrough)\n", (void*)target);
#endif
}

void RemovePerformWorkAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallPerformWorkExecute() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c93c0);
    // Prologue: SUB(4)+MOV(3)+MOV(4)+CMP(4) = 15 bytes
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 15);
#ifdef _DEBUG
    printf("[PerformWork] Execute hooked at %p (prologue=15, passthrough)\n", (void*)target);
#endif
}

void RemovePerformWorkExecute() {
    RemoveHook(&g_hookExecute);
}

void InstallPerformWorkExit() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9370);
    // Prologue: PUSH(2)+SUB(4)+LEA(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_Exit, &g_hookExit, 13);
#ifdef _DEBUG
    printf("[PerformWork] Exit hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemovePerformWorkExit() {
    RemoveHook(&g_hookExit);
}

void InstallPerformWorkCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c9320);
    // Prologue: PUSH(2)+SUB(4)+LEA(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[PerformWork] CheckEntity hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemovePerformWorkCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
