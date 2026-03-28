// engine/reimpl/statemachinecomponent.cpp
// Hook wrappers for StateMachineComponent vtable methods.
//
// Delegates to clean code in code/Game/Component/statemachinecomponent.cpp.
//
// Vtable: 0x140164520 (12 slots, 8 unique to SMC)
//
// Hookable slots:
//   [0] ScalarDtor  (0x1400384a0) - vtable hook (SEH + shared across 3 vtables)
//   [3] Shutdown    (0x14012b940) - vtable hook (SEH cookie)
//   [4] Serialize   (0x14012b6a0) - prologue hook (283 bytes)
//   [6] Draw        (0x14012b660) - prologue hook (49 bytes)
//   [8] OnRemove    (0x14012b640) - prologue hook (16 bytes, has rel JMP)
//   [9] Update      (0x14012b620) - prologue hook (16 bytes, has rel JMP) EXISTING
//   [11] OnUpdate2  (0x14012b7d0) - prologue hook (351 bytes)
//
// Unhookable:
//   [2] Startup/GetType (0x1400382b0) - 7 bytes, too small
//
// Shared base Component slots (not hooked here):
//   [1] GetTypeInfo (0x1400382c0), [5] 0x140118a60, [7] 0x140118830, [10] 0x140118a90

#include "StateMachineComponent.h"
#include "Component/statemachinecomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;
static HookInfo g_hookDraw;
static HookInfo g_hookOnRemove;
static HookInfo g_hookUpdate;
static HookInfo g_hookOnUpdate2;

// ============================================================
// ScalarDtor (vtable slot [0], 0x1400384a0)
// VTable hook: SEH cookie + shared across 3 vtables
// ============================================================
typedef void* (*OrigDtor_t)(StateMachineComponent*, uint32_t);

static void* Reimpl_ScalarDtor(StateMachineComponent* self, uint32_t flags) {
    g_lastHook = "StateMachineComponent::ScalarDtor";
    return StateMachineComponent_ScalarDtor(self, flags);
}

// ============================================================
// Shutdown (vtable slot [3], 0x14012b940)
// VTable hook: SEH cookie (MOV [RSP+0x20], -2)
// ============================================================
static void Reimpl_Shutdown(StateMachineComponent* self, void* stream) {
    g_lastHook = "StateMachineComponent::Shutdown";
    StateMachineComponent_Shutdown(self, stream);
}

// ============================================================
// Serialize (vtable slot [4], 0x14012b6a0)
// Prologue hook (283 bytes, no SEH)
// ============================================================
static void Reimpl_Serialize(StateMachineComponent* self) {
    g_lastHook = "StateMachineComponent::Serialize";
    StateMachineComponent_Serialize(self);
}

// ============================================================
// Draw (vtable slot [6], 0x14012b660)
// Prologue hook (49 bytes)
// ============================================================
static void Reimpl_Draw(StateMachineComponent* self) {
    g_lastHook = "StateMachineComponent::Draw";
    StateMachineComponent_Draw(self);
}

// ============================================================
// OnRemove (vtable slot [8], 0x14012b640)
// Prologue hook (16 bytes, has relative JMP -- full function replacement)
// NOTE: RDX (deltaTime) is forwarded from caller as implicit register.
// ============================================================
static void Reimpl_OnRemove(StateMachineComponent* self, int64_t deltaTime) {
    g_lastHook = "StateMachineComponent::OnRemove";
    StateMachineComponent_OnRemove(self, deltaTime);
}

// ============================================================
// Update (vtable slot [9], 0x14012b620)
// Prologue hook (16 bytes, has relative JMP -- full function replacement)
// ============================================================
static void Reimpl_Update(StateMachineComponent* self, int64_t deltaTime) {
    g_lastHook = "StateMachineComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[StateMachineComponent::Update] self=%p dt=%lld stateMachine=%p\n",
               (void*)self, deltaTime, (void*)&self->controllerSelf);
        callCount++;
    }
#endif
    StateMachineComponent_Update(self, deltaTime);
}

// ============================================================
// OnUpdate2 (vtable slot [11], 0x14012b7d0)
// Prologue hook (351 bytes)
// ============================================================
static void Reimpl_OnUpdate2(StateMachineComponent* self) {
    g_lastHook = "StateMachineComponent::OnUpdate2";
    StateMachineComponent_OnUpdate2(self);
}

// ============================================================
// Install / Remove functions
// ============================================================

// --- ScalarDtor ---
void InstallStateMachineComponentDtor() {
    StateMachineComponent_ResolveHelpers();
    // VTable hook: slot [0] of StateMachineComponent vtable
    InstallHookVTable(StateMachineComponentAddr::Vtable, 0, (void*)Reimpl_ScalarDtor, &g_hookDtor);
    RegisterHookInfo("StateMachineComponent::ScalarDtor", &g_hookDtor);
#ifdef _DEBUG
    printf("[SMC] ScalarDtor vtable-hooked (original=%p)\n", g_hookDtor.trampoline);
#endif
}
void RemoveStateMachineComponentDtor() { RemoveHookVTable(&g_hookDtor); }

// --- Shutdown ---
void InstallStateMachineComponentShutdown() {
    StateMachineComponent_ResolveHelpers();
    // VTable hook: slot [3] -- SEH cookie prevents prologue hook
    InstallHookVTable(StateMachineComponentAddr::Vtable, 3, (void*)Reimpl_Shutdown, &g_hookShutdown);
    RegisterHookInfo("StateMachineComponent::Shutdown", &g_hookShutdown);
#ifdef _DEBUG
    printf("[SMC] Shutdown vtable-hooked (original=%p)\n", g_hookShutdown.trampoline);
#endif
}
void RemoveStateMachineComponentShutdown() { RemoveHookVTable(&g_hookShutdown); }

// --- Serialize ---
void InstallStateMachineComponentSerialize() {
    StateMachineComponent_ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, StateMachineComponentAddr::Serialize);
    // 283-byte function, prologue: 3x PUSH (1B) + 3x PUSH R (2B) + SUB RSP,0x28 (4B) + MOV R13,RCX (3B) = 17 bytes
    // Boundary at 14 (SUB RSP end) is under 14-byte minimum; use 17 (includes MOV R13,RCX)
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 17);
#ifdef _DEBUG
    printf("[SMC] Serialize hooked at %p (prologue=16)\n", (void*)target);
#endif
}
void RemoveStateMachineComponentSerialize() { RemoveHook(&g_hookSerialize); }

// --- Draw ---
void InstallStateMachineComponentDraw() {
    StateMachineComponent_ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, StateMachineComponentAddr::Draw);
    // 49-byte function: PUSH RBX (1) + SUB RSP,0x20 (4) + MOV RBX,RCX (3) = 8 bytes safe
    // Then XORPS XMM1,XMM1 (3) + LEA RCX,[RSP+0x30] (5) = 16 total safe
    // Contains CALL rel32 at offset 0x11 -- use 16 bytes and relocating hook
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 16);
#ifdef _DEBUG
    printf("[SMC] Draw hooked at %p (prologue=16)\n", (void*)target);
#endif
}
void RemoveStateMachineComponentDraw() { RemoveHook(&g_hookDraw); }

// --- OnRemove ---
void InstallStateMachineComponentOnRemove() {
    StateMachineComponent_ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, StateMachineComponentAddr::OnRemove);
    // 16-byte function (full replacement, same as Update): ADD RCX,0x48; MOV R9B,1; XOR R8D,R8D; JMP rel32
    // Contains relative JMP at offset 10 -- trampoline not usable, but we don't call it
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 16);
#ifdef _DEBUG
    printf("[SMC] OnRemove hooked at %p (prologue=16, full function)\n", (void*)target);
#endif
}
void RemoveStateMachineComponentOnRemove() { RemoveHook(&g_hookOnRemove); }

// --- Update (existing, updated to use clean code) ---
void InstallStateMachineComponentUpdate() {
    StateMachineComponent_ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, StateMachineComponentAddr::Update);
    // 16-byte function (full replacement): MOV R9B,1; ADD RCX,0x48; MOVZX R8D,R9B; JMP rel32
    // Contains relative JMP at offset 11 -- trampoline not usable, but we don't call it
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[SMC] Update hooked at %p (prologue=16, full function)\n", (void*)target);
#endif
}
void RemoveStateMachineComponentUpdate() { RemoveHook(&g_hookUpdate); }

// --- OnUpdate2 ---
void InstallStateMachineComponentOnUpdate2() {
    StateMachineComponent_ResolveHelpers();
    uintptr_t target = ResolveAddr(Module::Exe, StateMachineComponentAddr::OnUpdate2);
    // 351-byte function: PUSH RBX (1) + SUB RSP,0x40 (4) + MOV [RSP+0x60],RBP (5) = 10 bytes
    // + MOV [RSP+0x68],RSI (5) = 15 bytes, + MOV [RSP+0x30],R12 (5) = 20 bytes safe
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 20);
#ifdef _DEBUG
    printf("[SMC] OnUpdate2 hooked at %p (prologue=20)\n", (void*)target);
#endif
}
void RemoveStateMachineComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
