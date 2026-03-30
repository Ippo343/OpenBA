// engine/reimpl/tornadocomponent.cpp
// Hook wrappers for TornadoComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/tornadocomponent.cpp.

#include "TornadoComponent.h"
#include "Component/tornadocomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;
static HookInfo g_hookOnRemove;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(TornadoComponent* self, void* stream) {
    g_lastHook = "TornadoComponent::Shutdown";
    TornadoComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(TornadoComponent* self) {
    g_lastHook = "TornadoComponent::Serialize";
    TornadoComponent_Serialize(self);
}

static void Reimpl_OnRemove(TornadoComponent* self, void* time) {
    g_lastHook = "TornadoComponent::OnRemove";
    TornadoComponent_OnRemove(self, time);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTornadoComponentShutdown() {
    TornadoComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140058b80);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TornadoComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTornadoComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTornadoComponentSerialize() {
    TornadoComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400585a0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 13);
#ifdef _DEBUG
    printf("[TornadoComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTornadoComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallTornadoComponentOnRemove() {
    TornadoComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140057cd0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 15);
#ifdef _DEBUG
    printf("[TornadoComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveTornadoComponentOnRemove() { RemoveHook(&g_hookOnRemove); }
