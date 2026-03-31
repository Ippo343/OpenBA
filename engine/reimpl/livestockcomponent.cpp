// engine/reimpl/livestockcomponent.cpp
// Hook wrappers for LivestockComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/livestockcomponent.cpp.
//
// LivestockComponent (0x38 bytes) is a reproduction timer.
// Vtable at 0x1401639b0. Non-trivial override:
//   Update (0x140123b50) — accumulates time, fires birth events

#include "LivestockComponent.h"
#include "Component/livestockcomponent_funcs.h"
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
static void Reimpl_Update(LivestockComponent* self, int64_t deltaTime) {
    g_lastHook = "LivestockComponent::Update";
    LivestockComponent_Update(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallLivestockComponentUpdate() {
    LivestockComponent_InitFuncs();

    // fn_IsDerivedFrom is resolved in engine_funcs — verify it's available
    if (!fn_IsDerivedFrom) {
#ifdef _DEBUG
        printf("[LivestockComponent] ERROR: fn_IsDerivedFrom not resolved, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, LivestockComponentAddr::Update);
    // Prologue: MOV [RSP+0x18],RSI(5) + MOV [RSP+0x20],RDI(5) + PUSH R14(2) = 12 bytes
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[LivestockComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveLivestockComponentUpdate() { RemoveHook(&g_hookUpdate); }
