// engine/reimpl/interactcomponent.cpp
// Hook wrappers for InteractComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/interactcomponent.cpp.
//
// InteractComponent (0x38 bytes) -- direct Component subclass.
// Vtable at 0x1401638b0:
//   [3] Shutdown = 0x140123340
//   [6] Draw     = 0x140123110

#include "InteractComponent.h"
#include "Component/interactcomponent_funcs.h"
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

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(InteractComponent* self, void* stream) {
    g_lastHook = "InteractComponent::Shutdown";
    InteractComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(InteractComponent* self) {
    g_lastHook = "InteractComponent::Draw";
    InteractComponent_Draw(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallInteractComponentShutdown() {
    InteractComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140123340);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[InteractComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveInteractComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallInteractComponentDraw() {
    InteractComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140123110);
    // Prologue: 14 bytes
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[InteractComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveInteractComponentDraw() { RemoveHook(&g_hookDraw); }
