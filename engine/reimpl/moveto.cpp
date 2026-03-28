// engine/reimpl/moveto.cpp
// Hook wrappers for MoveTo action from Application-x64.exe.
//
// MoveTo is the LARGEST state machine action (0x480 = 1152 bytes). It handles
// full A* pathfinding, citizen movement along waypoints, speed calculation
// based on terrain type, and bridge detection. The Execute method alone is
// 176+ lines with grid math, path traversal, and animation updates.
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
//   +0x60: current path index (-1 = no active segment)
//   +0x64: path node count
//   +0x68: path node array (up to 0x100 entries)
//   +0x468: idle animation ID
//   +0x46C: walk animation ID
//   +0x470: bridge flag
//   +0x474: road flag
//   +0x478: speed multiplier (default 1.0)
//
// Vtable overrides:
//   Slot 3 (Serialize):    0x1400c8ec0 -- REIMPLEMENTED
//   Slot 4 (AbstractImpl): 0x1400c8ab0 -- REIMPLEMENTED
//   Slot 5 (Execute):      0x1400c8570 -- REIMPLEMENTED (vtable hook, bug class 3 prologue)
//   Slot 6 (Enter):        0x1400c8970 -- REIMPLEMENTED
//   Slot 7 (Exit):         0x1400c8520 -- REIMPLEMENTED
//   Slot 8 (CheckEntity):  0x1400c84d0 -- REIMPLEMENTED
//   Slot 9 (GetTarget):    0x1400c8480 -- not hooked (too small), standalone exe only

#include "MoveTo.h"
#include "hooks.h"
#include "resolve.h"
#include "Action/moveto_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool MoveTo_ResolveFuncs();

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
// MoveTo::Serialize -- reimplemented
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "MoveTo::Serialize";
    MoveTo_Serialize_Impl(self, stream);
}

// ============================================================
// MoveTo::AbstractImpl -- reimplemented
// Uses int return (bug class 7). Forwards RDX (actionStack).
// ============================================================

static int Reimpl_AbstractImpl(void* self, void* actionStack, void* /*p3*/, void* /*p4*/) {
    g_lastHook = "MoveTo::AbstractImpl";
    return MoveTo_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// MoveTo::Execute -- vtable-hooked (bug class 3: MOV RAX,RSP)
// Reimplemented — delegates to clean code in code/Game/Action/moveto.cpp.
// ============================================================

static uint64_t Reimpl_Execute(void* self, void* actionStack, void* time) {
    g_lastHook = "MoveTo::Execute";
    return (uint64_t)MoveTo_Execute_Impl(self, actionStack, time);
}

// ============================================================
// MoveTo::Enter -- reimplemented
// ============================================================

static void Reimpl_Enter(void* self) {
    g_lastHook = "MoveTo::Enter";
    MoveTo_Enter_Impl(self);
}

// ============================================================
// MoveTo::Exit -- reimplemented
// Forwards RDX (actionStack) per bug class 6.
// ============================================================

static void Reimpl_Exit(void* self, void* actionStack) {
    g_lastHook = "MoveTo::Exit";
    MoveTo_Exit_Impl(self, actionStack);
}

// ============================================================
// MoveTo::CheckEntity -- reimplemented
// ============================================================

static void Reimpl_CheckEntity(void* self) {
    g_lastHook = "MoveTo::CheckEntity";
    MoveTo_CheckEntity_Impl(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallMoveToSerialize() {
    if (!MoveTo_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[MoveTo] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8ec0);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[MoveTo] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveMoveToSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallMoveToAbstractImpl() {
    if (!MoveTo_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[MoveTo] ERROR: Failed to resolve helpers for AbstractImpl\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8ab0);
    // Prologue: MOV(5)+MOV(5)+PUSH(1)+SUB(4) = 15 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 15);
#ifdef _DEBUG
    printf("[MoveTo] AbstractImpl hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveMoveToAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallMoveToExecute() {
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x14015fe40, Execute = slot 5 (+0x28)
    InstallHookVTable(0x14015fe40, 5, (void*)Reimpl_Execute, &g_hookExecute);
    RegisterHookInfo("MoveTo::Execute", &g_hookExecute);
#ifdef _DEBUG
    printf("[MoveTo] Execute vtable-hooked (original=%p)\n", g_hookExecute.trampoline);
#endif
}

void RemoveMoveToExecute() {
    RemoveHookVTable(&g_hookExecute);
}

void InstallMoveToEnter() {
    if (!MoveTo_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[MoveTo] ERROR: Failed to resolve helpers for Enter\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8970);
    // Prologue: MOV(5)+PUSH(1)+PUSH(1)+PUSH(1)+MOV(3) = 11, next SUB RSP starts at 11
    // SUB RSP,0x80 = 7 bytes, ends at 18
    InstallHook(target, (void*)Reimpl_Enter, &g_hookEnter, 18);
#ifdef _DEBUG
    printf("[MoveTo] Enter hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveMoveToEnter() {
    RemoveHook(&g_hookEnter);
}

void InstallMoveToExit() {
    if (!MoveTo_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[MoveTo] ERROR: Failed to resolve helpers for Exit\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8520);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_Exit, &g_hookExit, 13);
#ifdef _DEBUG
    printf("[MoveTo] Exit hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveMoveToExit() {
    RemoveHook(&g_hookExit);
}

void InstallMoveToCheckEntity() {
    if (!MoveTo_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[MoveTo] ERROR: Failed to resolve helpers for CheckEntity\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c84d0);
    // Prologue: MOV(5)+PUSH(1)+SUB(4)+MOV(3) = 13 bytes
    InstallHook(target, (void*)Reimpl_CheckEntity, &g_hookCheckEntity, 13);
#ifdef _DEBUG
    printf("[MoveTo] CheckEntity hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveMoveToCheckEntity() {
    RemoveHook(&g_hookCheckEntity);
}
