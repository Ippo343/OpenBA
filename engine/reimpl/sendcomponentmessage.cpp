// engine/reimpl/sendcomponentmessage.cpp
// Hook wrappers for SendComponentMessage action from Application-x64.exe.
//
// SendComponentMessage — 0x40 bytes. Sends a message to a specific component
// type on the target entity.
// Inherits: Object::Internal -> Action (0x20) -> SendComponentMessage
// Vtable at 0x1401600c0 (10 slots).
//
// Vtable:
//   [0] +0x00  ScalarDtor    = 0x1400230f0   (RIP-relative, bug class 2, unhookable)
//   [1] +0x08  VectorDtor    = 0x140023060   (not hooked)
//   [2] +0x10  BaseDtor      = 0x140023050   (not hooked)
//   [3] +0x18  Serialize     = 0x1400cb000   <-- reimplemented
//   [4] +0x20  AbstractImpl  = 0x1400caef0   <-- reimplemented
//   [5] +0x28  Execute       = 0x1400c4a90   (base Action, returns 0)
//   [6] +0x30  Enter         = 0x1400c4b90   (base Action, no-op)
//   [7] +0x38  Exit          = 0x1400c4ba0   (base Action, no-op)
//   [8] +0x40  CheckEntity   = 0x1400c4a30   (base Action, no-op, returns void)
//   [9] +0x48  GetTarget     = 0x1400cae80   <-- reimplemented

#include "SendComponentMessage.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/sendcomponentmessage_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool SendComponentMessage_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookAbstractImpl;
static HookInfo g_hookGetTarget;

// ============================================================
// SendComponentMessage::Serialize (0x1400cb000) — reimplemented
// ============================================================
static void Reimpl_Serialize(SendComponentMessage* self, void* stream) {
    g_lastHook = "SendComponentMessage::Serialize";
    SendComponentMessage_Serialize_Impl(self, stream);
}

// ============================================================
// SendComponentMessage::AbstractImpl (0x1400caef0) — reimplemented
// Returns int (bug class 7).
// ============================================================
static int Reimpl_AbstractImpl(SendComponentMessage* self, void* actionStack) {
    g_lastHook = "SendComponentMessage::AbstractImpl";
    return SendComponentMessage_AbstractImpl_Impl(self, actionStack);
}

// ============================================================
// SendComponentMessage::GetTarget (0x1400cae80) — reimplemented
// ============================================================
static bool Reimpl_GetTarget(SendComponentMessage* self, void* param) {
    g_lastHook = "SendComponentMessage::GetTarget";
    return SendComponentMessage_GetTarget_Impl(self, param);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallSendComponentMessageSerialize() {
    if (!SendComponentMessage_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[SendComponentMessage] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cb000);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[SendComponentMessage] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveSendComponentMessageSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallSendComponentMessageAbstractImpl() {
    if (!SendComponentMessage_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[SendComponentMessage] ERROR: Failed to resolve helpers for AbstractImpl\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400caef0);
    // Prologue: PUSH(2)+SUB(4)+MOV(5)+MOV(5) = 16 bytes
    InstallHook(target, (void*)Reimpl_AbstractImpl, &g_hookAbstractImpl, 16);
#ifdef _DEBUG
    printf("[SendComponentMessage] AbstractImpl hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveSendComponentMessageAbstractImpl() {
    RemoveHook(&g_hookAbstractImpl);
}

void InstallSendComponentMessageGetTarget() {
    if (!SendComponentMessage_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[SendComponentMessage] ERROR: Failed to resolve helpers for GetTarget\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400cae80);
    // Prologue: MOV(5)+MOV(5)+PUSH(1)+SUB(4) = 15 bytes
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 15);
#ifdef _DEBUG
    printf("[SendComponentMessage] GetTarget hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveSendComponentMessageGetTarget() {
    RemoveHook(&g_hookGetTarget);
}
