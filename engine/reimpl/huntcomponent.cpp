// engine/reimpl/huntcomponent.cpp
// Hook wrappers for HuntComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/huntcomponent.cpp.
//
// HuntComponent (0x58 bytes) extends ResourceProduceComponent.
// Vtable at 0x140163770. Non-trivial override:
//   Update (0x140122740) — hunt progress timer with terrain speed scaling

#include "HuntComponent.h"
#include "Component/huntcomponent_funcs.h"
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
// Reimplementation wrapper (delegates to clean code)
// ============================================================
static void Reimpl_Update(HuntComponent* self, int64_t deltaTime) {
    g_lastHook = "HuntComponent::Update";
    HuntComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHuntComponentUpdate() {
    HuntComponent_InitFuncs();

    if (!g_Timer_secondsPerTick) {
#ifdef _DEBUG
        printf("[HuntComponent] ERROR: g_Timer_secondsPerTick not resolved, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140122740);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           SUB RSP,0x20(4) = 12 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[HuntComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveHuntComponentUpdate() { RemoveHook(&g_hookUpdate); }
