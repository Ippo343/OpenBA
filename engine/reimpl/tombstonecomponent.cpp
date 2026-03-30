// engine/reimpl/tombstonecomponent.cpp
// Hook wrappers for TombstoneComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/tombstonecomponent.cpp.

#include "TombstoneComponent.h"
#include "Component/tombstonecomponent_funcs.h"
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
static HookInfo g_hookUpdate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(TombstoneComponent* self, void* stream) {
    g_lastHook = "TombstoneComponent::Shutdown";
    TombstoneComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(TombstoneComponent* self) {
    g_lastHook = "TombstoneComponent::Serialize";
    TombstoneComponent_Serialize(self);
}

static void Reimpl_Update(TombstoneComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "TombstoneComponent::Update";
    TombstoneComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTombstoneComponentShutdown() {
    TombstoneComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400578E0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TombstoneComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTombstoneComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTombstoneComponentSerialize() {
    TombstoneComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140057860);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TombstoneComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTombstoneComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallTombstoneComponentUpdate() {
    TombstoneComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400577D0);
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[TombstoneComponent] Update hooked at %p\n", (void*)target);
#endif
}
void RemoveTombstoneComponentUpdate() { RemoveHook(&g_hookUpdate); }
