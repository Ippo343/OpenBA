// engine/reimpl/buildcomponent.cpp
// Hook wrappers for BuildComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/buildcomponent.cpp.
//
// BuildComponent — ControllerComponent subclass (0x78 bytes).
// Manages building construction: resource tracking, worker assignments, progress.
//
// Vtable at 0x140162260:
//   [3] Shutdown        = 0x140108D80  <-- REIMPL (serialize with entity arrays)
//   [4] Serialize       = 0x140108D60  <-- 3-byte RET 0 (unhookable no-op)
//   [6] Draw            = 0x140108D40  <-- REIMPL (delegates to original — complex)
//   [7] OnCreate        = 0x140108CB0  <-- REIMPL (remove from removal queue)
//  [11] OnUpdate2       = 0x140108D70  <-- 3-byte RET 0 (unhookable no-op)
//   Other slots: NO-OP

#include "BuildComponent.h"
#include "Component/buildcomponent_funcs.h"
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

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(BuildComponent* self, void* stream) {
    g_lastHook = "BuildComponent::Shutdown";
    BuildComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(BuildComponent* self) {
    g_lastHook = "BuildComponent::Draw";
    BuildComponent_Draw(self);
}

static void Reimpl_OnCreate(BuildComponent* self) {
    g_lastHook = "BuildComponent::OnCreate";
    BuildComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallBuildComponentShutdown() {
    BuildComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140108D80);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[BuildComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallBuildComponentDraw() {
    BuildComponent_InitFuncs();
    // VTable hook: function is only 4 instructions (21 bytes) with RIP-relative MOV
    // and relative JZ. Prologue hooking corrupts the tiny function body.
    // vtable at 0x140162260, Draw = slot 6 (+0x30)
    InstallHookVTable(0x140162260, 6, (void*)Reimpl_Draw, &g_hookDraw);
    RegisterHookInfo("BuildComponent::Draw", &g_hookDraw);
#ifdef _DEBUG
    printf("[BuildComponent] Draw vtable-hooked (original=%p)\n", g_hookDraw.trampoline);
#endif
}
void RemoveBuildComponentDraw() { RemoveHookVTable(&g_hookDraw); }

void InstallBuildComponentOnCreate() {
    BuildComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140108CB0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[BuildComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
