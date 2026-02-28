// engine/reimpl/statemachinecomponent.cpp
// Reimplementation of StateMachineComponent::Update from Application-x64.exe.
//
// The Update method is a 1-line delegate: it tail-calls StateMachine_Tick
// on the state machine pointer at this+0x48 with (deltaTime, true, true).
//
// Original: 0x14012b620 (16 bytes total, tiny tail-call function)

#include "StateMachineComponent.h"
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
// Helper function pointers (resolved at install time)
// ============================================================

// StateMachine_Tick(stateMachine*, Time, bool processTransitions, bool processActions)
// Located at 0x1400a9660 in Application-x64.exe
typedef void (*fn_StateMachine_Tick_t)(void* stateMachine, int64_t deltaTime, char param3, char param4);
static fn_StateMachine_Tick_t fn_StateMachine_Tick = nullptr;

// ============================================================
// Update (0x14012b620)
// Delegates to StateMachine_Tick(this->controllerSelf, dt, true, true).
// The state machine pointer is at offset +0x48.
//
// Parameters: RCX = this, RDX = Time (int64)
// ============================================================
static void Reimpl_Update(StateMachineComponent* self, int64_t deltaTime) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[StateMachineComponent::Update] self=%p dt=%lld stateMachine=%p\n",
               (void*)self, deltaTime, self->controllerSelf);
        callCount++;
    }
#endif

    if (fn_StateMachine_Tick) {
        fn_StateMachine_Tick(self->controllerSelf, deltaTime, 1, 1);
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStateMachineComponentUpdate() {
    // Resolve StateMachine_Tick in the exe
    fn_StateMachine_Tick = reinterpret_cast<fn_StateMachine_Tick_t>(
        ResolveAddr(Module::Exe, 0x1400a9660));

#ifdef _DEBUG
    printf("[StateMachineComponent] StateMachine_Tick = %p\n", (void*)fn_StateMachine_Tick);
#endif

    if (!fn_StateMachine_Tick) {
#ifdef _DEBUG
        printf("[StateMachineComponent] ERROR: failed to resolve StateMachine_Tick, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, StateMachineComponentAddr::Update);
    // Entire function is 16 bytes (4 instructions: MOV R9B + ADD RCX + MOVZX R8D + JMP).
    // Contains a relative JMP at offset 11 — trampoline is NOT usable, but we don't call it.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[StateMachineComponent] Update hooked at %p (prologue=16, full function)\n", (void*)target);
#endif
}

void RemoveStateMachineComponentUpdate() { RemoveHook(&g_hookUpdate); }
