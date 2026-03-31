// engine/reimpl/pasturecomponent.cpp
// Hook wrappers for PastureComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/pasturecomponent.cpp.
//
// PastureComponent (0x68 bytes) has SHIFTED controller sub-object at +0x48.
// Vtable at 0x140163f60. Non-trivial overrides:
//   Shutdown  (0x140127f40) — entity ref array serialization
//   Draw      (0x140127a30) — compute pasture capacity
//   OnCreate  (0x1401277e0) — complex entity ref setup + registration

#include "PastureComponent.h"
#include "Component/pasturecomponent_funcs.h"
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
static void Reimpl_Shutdown(PastureComponent* self, void* stream) {
    g_lastHook = "PastureComponent::Shutdown";
    PastureComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(PastureComponent* self) {
    g_lastHook = "PastureComponent::Draw";
    PastureComponent_Draw(self);
}

static void Reimpl_OnCreate(PastureComponent* self) {
    g_lastHook = "PastureComponent::OnCreate";
    PastureComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPastureComponentShutdown() {
    PastureComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140127f40);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[PastureComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemovePastureComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallPastureComponentDraw() {
    PastureComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140127a30);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[PastureComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemovePastureComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallPastureComponentOnCreate() {
    PastureComponent_InitFuncs();
    // Prologue: PUSH R13(2) + SUB RSP,0x60(4) + CMP [RCX+0x38],0(4) + MOV R13,RCX(3) = 13
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401277e0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[PastureComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemovePastureComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
