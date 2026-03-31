// engine/reimpl/storagecomponent.cpp
// Hook wrappers for StorageComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/storagecomponent.cpp.
//
// StorageComponent (0x40 bytes) manages storage slots for buildings. PERF HOTSPOT.
// Vtable at 0x140164720. Non-trivial overrides:
//   Shutdown    (0x14012c730) — versioned serialization of storage slot data
//   Draw        (0x14012c640) — rebuild storage slots from building template
//   QueryCount  (0x14012cf10) — count items across all slots (44 xrefs, O(n*m))
//   Helper_c560 (0x14012c560) — check material flag intersection

#include "StorageComponent.h"
#include "Component/storagecomponent_funcs.h"
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
static HookInfo g_hookQueryCount;
static HookInfo g_hookHelper_c560;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(StorageComponent* self, void* stream) {
    g_lastHook = "StorageComponent::Shutdown";
    StorageComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(StorageComponent* self) {
    g_lastHook = "StorageComponent::Draw";
    StorageComponent_Draw(self);
}

static int Reimpl_QueryCount(StorageComponent* self) {
    g_lastHook = "StorageComponent::QueryCount";
    return StorageComponent_QueryCount((void*)self);
}

static uint32_t Reimpl_Helper_c560(void* param_1, void* param_2) {
    g_lastHook = "StorageComponent::Helper_c560";
    return StorageComponent_Helper_c560(param_1, param_2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallStorageComponentHelper_c560() {
    StorageComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012c560);
    // Prologue: MOV [RSP+8],RBX(5) + MOV [RSP+10],RBP(5) + MOV [RSP+18],RSI(5) = 15
    InstallHook(target, (void*)Reimpl_Helper_c560, &g_hookHelper_c560, 15);
#ifdef _DEBUG
    printf("[StorageComponent] Helper_c560 hooked at %p (reimpl, null-safe)\n", (void*)target);
#endif
}

void RemoveStorageComponentHelper_c560() { RemoveHook(&g_hookHelper_c560); }

void InstallStorageComponentQueryCount() {
    StorageComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012cf10);
    // Prologue: MOV R11,RSP(3) + MOV [R11+8],RCX(4) + PUSH R12(2) + PUSH R14(2) + SUB RSP,0x38(4) = 15
    InstallHook(target, (void*)Reimpl_QueryCount, &g_hookQueryCount, 15);
#ifdef _DEBUG
    printf("[StorageComponent] QueryCount hooked at %p (reimpl, null-safe)\n", (void*)target);
#endif
}

void RemoveStorageComponentQueryCount() { RemoveHook(&g_hookQueryCount); }

void InstallStorageComponentShutdown() {
    StorageComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012c730);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[StorageComponent] Shutdown hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStorageComponentDraw() {
    StorageComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012c640);
    // Prologue: MOV [RSP+0x18],RBX(5) + PUSH RBP(1) + PUSH RDI(1) + PUSH R14(2) + SUB RSP,0x20(4) = 13
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[StorageComponent] Draw hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveStorageComponentDraw() { RemoveHook(&g_hookDraw); }
