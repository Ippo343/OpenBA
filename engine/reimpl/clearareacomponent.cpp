// engine/reimpl/clearareacomponent.cpp
// Reimplementation of ClearAreaComponent lifecycle methods from Application-x64.exe.
//
// ClearAreaComponent — ControllerComponent subclass (0x50 bytes).
// Has one extra field: clearState (uint32) at +0x48.
// Handles clearing terrain areas during construction/placement.
//
// Vtable at 0x1401626d0:
//   [3] Shutdown        = 0x140117EC0  <-- HOOKED (serialize with clearState)
//   All others: NO-OP

#include "ClearAreaComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/clearareacomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x140117EC0) -- delegates to clean code
// ============================================================
static void Reimpl_Shutdown(ClearAreaComponent* self, void* stream) {
    g_lastHook = "ClearAreaComponent::Shutdown";
    ClearAreaComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallClearAreaComponentShutdown() {
    ClearAreaComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140117EC0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ClearAreaComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaComponentShutdown() { RemoveHook(&g_hookShutdown); }
