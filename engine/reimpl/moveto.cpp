// engine/reimpl/moveto.cpp
// Reimplementation of MoveTo action from Application-x64.exe.
//
// MoveTo is the LARGEST state machine action (0x480 = 1152 bytes). It handles
// full A* pathfinding, citizen movement along waypoints, speed calculation
// based on terrain type, and bridge detection. The Execute method alone is
// 176+ lines with grid math, path traversal, and animation updates.
//
// ALL override methods use passthrough wrappers due to extreme complexity and
// deep coupling to the pathfinding subsystem, terrain grid, and entity system.
//
// Struct size: 0x480, vtable at 0x14015fe40
// Inherits: Object::Internal -> Action -> MoveTo
// Constructor at 0x1400c90e0
//
// Key fields:
//   +0x20: targetId (-1 = invalid)
//   +0x24: path version
//   +0x28: target entity (InternalBasePtr)
//   +0x30: path data pointer
//   +0x38: destination position (Vector3)
//   +0x48: current path index
//   +0x60: secondary ID (-1 sentinel)
//   +0x64: path node count
//   +0x68: path node array (up to 0x100 entries)
//   +0x468: idle animation ID
//   +0x46C: walk animation ID
//   +0x470: bridge flag
//   +0x474: road flag
//   +0x478: speed multiplier (default 1.0)
//
// Vtable overrides (ALL override base):
//   Slot 3 (Serialize):    0x1400c8ec0 -- PASSTHROUGH (versioned, path data serialization)
//   Slot 4 (AbstractImpl): 0x1400c8ab0 -- PASSTHROUGH (distance check + path setup)
//   Slot 5 (Execute):      0x1400c8570 -- PASSTHROUGH (176 lines, A* path traversal)
//   Slot 6 (Enter):        0x1400c8970 -- PASSTHROUGH (bridge detection, road check)
//   Slot 7 (Exit):         0x1400c8520 -- PASSTHROUGH (reset animation)
//   Slot 8 (CheckEntity):  0x1400c84d0 -- PASSTHROUGH (reset animation, same as Exit)
//   Slot 9 (GetTarget):    0x1400c8480 -- simple: compares entity->+0x10 with param

#include "MoveTo.h"
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
static HookInfo g_hookEnter;
static HookInfo g_hookExit;
static HookInfo g_hookCheckEntity;

// ============================================================
// Original function typedefs
// ============================================================

typedef void (*MoveTo_Serialize_t)(void* self, void* stream);
typedef uint64_t (*MoveTo_AbstractImpl_t)(void* self, void* p2, void* p3, void* p4);
typedef uint64_t (*MoveTo_Execute_t)(void* self, void* param2);
typedef void (*MoveTo_Enter_t)(void* self);
typedef void (*MoveTo_Exit_t)(void* self);
typedef void (*MoveTo_CheckEntity_t)(void* self);

// ============================================================
// MoveTo::Serialize -- passthrough wrapper
// Versioned serialization of path data, waypoints, and destination
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        int32_t targetId = *(int32_t*)(act + 0x20);
        float speed = *(float*)(act + 0x478);
        printf("[MoveTo::Serialize] self=%p targetId=%d speed=%.2f\n",
               self, targetId, speed);
        callCount++;
    }
#endif
    ((MoveTo_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// MoveTo::AbstractImpl -- passthrough wrapper
// Calculates distance to target, sets up path via A* if needed
// ============================================================

static uint64_t Reimpl_AbstractImpl(void* self, void* p2, void* p3, void* p4) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        int32_t targetId = *(int32_t*)(act + 0x20);
        printf("[MoveTo::AbstractImpl] self=%p targetId=%d\n", self, targetId);
        callCount++;
    }
#endif
    return ((MoveTo_AbstractImpl_t)g_hookAbstractImpl.trampoline)(self, p2, p3, p4);
}

// ============================================================
// MoveTo::Execute -- passthrough wrapper
// A* path traversal: follows waypoints, calculates speed based on
// terrain type (road/bridge/normal), updates character facing/position
// ============================================================

static uint64_t Reimpl_Execute(void* self, void* param2) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        uint8_t* act = (uint8_t*)self;
        int32_t pathIdx = *(int32_t*)(act + 0x48);
        int32_t pathCount = *(int32_t*)(act + 0x64);
        printf("[MoveTo::Execute] self=%p pathIdx=%d/%d\n", self, pathIdx, pathCount);
        callCount++;
    }
#endif
    return ((MoveTo_Execute_t)g_hookExecute.trampoline)(self, param2);
}

// ============================================================
// MoveTo::Enter -- passthrough wrapper
// Bridge detection via nearby entity query, sets road flag
// ============================================================

static void Reimpl_Enter(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        uint8_t onRoad = *(uint8_t*)(act + 0x474);
        printf("[MoveTo::Enter] self=%p onRoad=%d\n", self, onRoad);
        callCount++;
    }
#endif
    ((MoveTo_Enter_t)g_hookEnter.trampoline)(self);
}

// ============================================================
// MoveTo::Exit -- passthrough wrapper
// Resets character animation to idle
// ============================================================

static void Reimpl_Exit(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[MoveTo::Exit] self=%p\n", self);
        callCount++;
    }
#endif
    ((MoveTo_Exit_t)g_hookExit.trampoline)(self);
}

// ============================================================
// MoveTo::CheckEntity -- passthrough wrapper
// Same as Exit: resets animation
// ============================================================

static void Reimpl_CheckEntity(void* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[MoveTo::CheckEntity] self=%p\n", self);
        callCount++;
    }
#endif
    ((MoveTo_CheckEntity_t)g_hookCheckEntity.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallMoveToSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8ec0);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[MoveTo] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemoveMoveToSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallMoveToAbstractImpl() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8ab0);
    // Prologue: MOV(5)+MOV(5)+PUSH(1)+SUB(4) = 15 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 15);
#ifdef _DEBUG
    printf("[MoveTo] AbstractImpl hooked at %p (prologue=15, passthrough)\n", (void*)target);
#endif
}

void RemoveMoveToAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallMoveToExecute() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8570);
    // Prologue: MOV(3)+MOV(4)+PUSH(1)+PUSH(1)+PUSH(1)+PUSH(2)+PUSH(2) = 14 bytes
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 14);
#ifdef _DEBUG
    printf("[MoveTo] Execute hooked at %p (prologue=14, passthrough)\n", (void*)target);
#endif
}

void RemoveMoveToExecute() {
    RemoveHook(&g_hookExecute);
}

void InstallMoveToEnter() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8970);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(1)+MOV(3) = 11, next SUB RSP starts at 11
    // SUB RSP,0x80 = 7 bytes, ends at 18
    InstallHook(target, (void*)Reimpl_Enter, &g_hookEnter, 18);
#ifdef _DEBUG
    printf("[MoveTo] Enter hooked at %p (prologue=18, passthrough)\n", (void*)target);
#endif
}

void RemoveMoveToEnter() {
    RemoveHook(&g_hookEnter);
}

void InstallMoveToExit() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8520);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_Exit, &g_hookExit, 13);
#ifdef _DEBUG
    printf("[MoveTo] Exit hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveMoveToExit() {
    RemoveHook(&g_hookExit);
}

void InstallMoveToCheckEntity() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c84d0);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[MoveTo] CheckEntity hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveMoveToCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
