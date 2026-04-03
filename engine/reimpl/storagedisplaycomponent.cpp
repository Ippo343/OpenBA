// engine/reimpl/storagedisplaycomponent.cpp
// Hook wrappers for StorageDisplayComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/storagedisplaycomponent.cpp.
//
// StorageDisplayComponent (0x38 bytes) renders visual display objects for stored items.
// Vtable at 0x14015a780. Non-trivial overrides:
//   Shutdown  (0x140056970) — Component_Serialize + empty Serializer block
//   Draw      (0x140056730) — grid-based rendering of display objects
//   OnCreate  (0x1400566b0) — remove display objects from scene
//   Update    (0x1400566a0 -> 0x1400563e0) — proportional display allocation
//   OnUpdate2 (0x140056940) — Draw + Update combined

#include "StorageDisplayComponent.h"
#include "Component/storagedisplaycomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;
static HookInfo g_hookUpdate;
static HookInfo g_hookOnUpdate2;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(StorageDisplayComponent* self, void* stream) {
    g_lastHook = "StorageDisplayComponent::Shutdown";
    StorageDisplayComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::Draw";
    // Temporarily call trampoline to test if crash is from our reimpl
    typedef void (*OrigDraw_t)(StorageDisplayComponent*);
    ((OrigDraw_t)g_hookDraw.trampoline)(self);
}

static void Reimpl_OnCreate(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::OnCreate";
    StorageDisplayComponent_OnCreate(self);
}

static void Reimpl_Update(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::Update";
    StorageDisplayComponent_Update(self);
}

static void Reimpl_OnUpdate2(StorageDisplayComponent* self) {
    g_lastHook = "StorageDisplayComponent::OnUpdate2";
    StorageDisplayComponent_OnUpdate2(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStorageDisplayComponentShutdown() {
    StorageDisplayComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140056970);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Shutdown hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStorageDisplayComponentDraw() {
    StorageDisplayComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140056730);
    // Prologue: 7 PUSHes (1+1+1+2+2+2+2=11) + SUB RSP,0xb0(7) = 18
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 19);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Draw hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallStorageDisplayComponentOnCreate() {
    StorageDisplayComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400566b0);
    // Prologue: MOV [RSP+0x10],RSI(5) + PUSH RDI(1) + SUB RSP,0x20(4) + XOR EDI,EDI(2) = 12
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] OnCreate hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallStorageDisplayComponentUpdate() {
    StorageDisplayComponent_InitFuncs();
    // Update is a JMP thunk at 0x1400566a0 -> 0x1400563e0
    // We hook 0x1400563e0 directly (the actual function)
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400563e0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 14);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] Update hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentUpdate() { RemoveHook(&g_hookUpdate); }

void InstallStorageDisplayComponentOnUpdate2() {
    StorageDisplayComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140056940);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RAX,[RCX](3) + MOV RBX,RCX(3) = 12
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 12);
#ifdef _DEBUG
    printf("[StorageDisplayComponent] OnUpdate2 hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageDisplayComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
