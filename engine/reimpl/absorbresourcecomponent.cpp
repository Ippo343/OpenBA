// engine/reimpl/absorbresourcecomponent.cpp
// Reimplementation of AbsorbResourceComponent lifecycle methods from Application-x64.exe.
//
// AbsorbResourceComponent is a ControllerComponent subclass (0x48 bytes) with no
// additional fields. It has only one non-trivial vtable override:
//
// Methods reimplemented:
//   - Shutdown (0x140105c00) — serialize via Component_Serialize + two empty Serializer blocks
//
// All other vtable slots are no-ops (no Draw, no Update, no OnCreate, etc.)

#include "AbsorbResourceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/absorbresourcecomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x140105c00) -- delegates to clean code
// ============================================================
static void Reimpl_Shutdown(AbsorbResourceComponent* self, void* stream) {
    g_lastHook = "AbsorbResourceComponent::Shutdown";
    AbsorbResourceComponent_Shutdown(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAbsorbResourceComponentShutdown() {
    AbsorbResourceComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140105c00);
    // Prologue: PUSH RBX(2) + SUB RSP,0x40(4) + MOV RBX,RDX(3) = 9, + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[AbsorbResourceComponent] Shutdown hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveAbsorbResourceComponentShutdown() { RemoveHook(&g_hookShutdown); }
