// engine/reimpl/forestercomponent.cpp
// Hook wrappers for ForesterComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/forestercomponent.cpp.
//
// ForesterComponent (0x58 bytes) extends ResourceProduceComponent.
// Vtable at 0x140163460. One non-trivial hookable override:
//   Shutdown (0x14011f960) — base + 2 bool fields serialization

#include "ForesterComponent.h"
#include "Component/forestercomponent_funcs.h"
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
// Reimplementation wrapper (delegates to clean code)
// ============================================================
static void Reimpl_Shutdown(ForesterComponent* self, void* stream) {
    g_lastHook = "ForesterComponent::Shutdown";
    ForesterComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallForesterComponentShutdown() {
    ForesterComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011f960);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ForesterComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveForesterComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}
