// engine/reimpl/firecomponent.cpp
// Reimplementation of FireComponent::Update from Application-x64.exe.
//
// Two-state fire system:
//   Active (field30 != 0): countdown + spread + extinguish.
//   Inactive (field30 == 0): accumulate time + random fire-start probability.
//
// Original: 0x140048fb0 (53 instructions)

#include "FireComponent.h"
#include "Component/firecomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Update (0x140048fb0)
// ============================================================
static void Reimpl_Update(FireComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "FireComponent::Update";
    FireComponent_Update(self, pDeltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFireComponentUpdate() {
    FireComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140048fb0);
    // Prologue: MOV [RSP+0x10],RSI(5) + PUSH RDI(1) + SUB RSP,0x20(4)
    //         + MOV RAX,[RCX+0x30](4) = 14 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 14);
#ifdef _DEBUG
    printf("[FireComponent] Update hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveFireComponentUpdate() { RemoveHook(&g_hookUpdate); }
