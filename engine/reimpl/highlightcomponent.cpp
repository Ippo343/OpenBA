// engine/reimpl/highlightcomponent.cpp
// Reimplementation of HighlightComponent lifecycle methods from Application-x64.exe.
//
// HighlightComponent (0x128 bytes) manages selection/highlight outlines around entities.
// Vtable at 0x140159990. Only non-trivial override: Shutdown (0x14004c880).

#include "HighlightComponent.h"
#include "Component/highlightcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x14004c880)
// ============================================================
static void Reimpl_Shutdown(HighlightComponent* self, void* stream) {
    g_lastHook = "HighlightComponent::Shutdown";
    HighlightComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHighlightComponentShutdown() {
    HighlightComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004c880);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[HighlightComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveHighlightComponentShutdown() { RemoveHook(&g_hookShutdown); }
