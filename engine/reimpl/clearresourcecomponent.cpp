// engine/reimpl/clearresourcecomponent.cpp
// Reimplementation of ClearResourceComponent lifecycle methods from Application-x64.exe.
//
// ClearResourceComponent — ControllerComponent subclass (0x48 bytes, no extra fields).
// Handles clearing resources from tiles during construction.
//
// Vtable at 0x140162840:
//   [3] Shutdown        = 0x1401185E0  <-- HOOKED (serialize with 2 empty serializer blocks)
//   All others: NO-OP (Draw=base, OnCreate=base, OnRemove, Update, Slot10, OnUpdate2)

#include "ClearResourceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/clearresourcecomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x1401185E0) -- delegates to clean code
// ============================================================
static void Reimpl_Shutdown(ClearResourceComponent* self, void* stream) {
    g_lastHook = "ClearResourceComponent::Shutdown";
    ClearResourceComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallClearResourceComponentShutdown() {
    ClearResourceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401185E0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[ClearResourceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveClearResourceComponentShutdown() { RemoveHook(&g_hookShutdown); }
