// engine/reimpl/cemeterycomponent.cpp
// Reimplementation of CemeteryComponent lifecycle methods from Application-x64.exe.
//
// CemeteryComponent — ControllerComponent subclass (0x58 bytes).
// Fields: field48 (void* / array ptr, zeroed), field50 (void* / count, zeroed).
// Manages cemetery plot grid for tombstone placement.
//
// Vtable at 0x1401590a0:
//   [3] Shutdown        = 0x1400475C0  <-- HOOKED (serialize array of tile indices)
//   [4] Serialize       = 0x1400475B0  <-- 3-byte RET 0 (unhookable no-op)
//   [6] Draw            = 0x140047200  <-- PASSTHROUGH (complex grid rendering, 233 instrs)
//   [7] OnCreate        = 0x1400471C0  <-- HOOKED (simple: register entity in cemetery list)
//   Other slots: NO-OP

#include "CemeteryComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/cemeterycomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimpl wrappers -- delegate to clean code
// ============================================================
static void Reimpl_Shutdown(CemeteryComponent* self, void* stream) {
    g_lastHook = "CemeteryComponent::Shutdown";
    CemeteryComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(CemeteryComponent* self) {
    g_lastHook = "CemeteryComponent::Draw";
    // Must call trampoline directly — CemeteryComponent_Draw is a passthrough
    // that resolves the hooked address via ResolveAddr (infinite recursion).
    typedef void (*OrigDraw_t)(CemeteryComponent*);
    ((OrigDraw_t)g_hookDraw.trampoline)(self);
}

static void Reimpl_OnCreate(CemeteryComponent* self) {
    g_lastHook = "CemeteryComponent::OnCreate";
    CemeteryComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCemeteryComponentShutdown() {
    CemeteryComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400475C0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[CemeteryComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveCemeteryComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallCemeteryComponentDraw() {
    CemeteryComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140047200);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[CemeteryComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveCemeteryComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallCemeteryComponentOnCreate() {
    CemeteryComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400471C0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[CemeteryComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveCemeteryComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
