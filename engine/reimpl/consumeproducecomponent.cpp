// engine/reimpl/consumeproducecomponent.cpp
// Hook wrappers for ConsumeProduceComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/consumeproducecomponent.cpp.
//
// ConsumeProduceComponent extends ResourceProduceComponent (0x70 bytes).
// Two non-trivial vtable overrides:
//   Shutdown  (0x140118e00) — parent Shutdown + versioned serialization of consume fields
//   Serialize (0x140118ef0) — init consume data from building template

#include "ConsumeProduceComponent.h"
#include "Component/consumeproducecomponent_funcs.h"
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

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(ConsumeProduceComponent* self, void* stream) {
    g_lastHook = "ConsumeProduceComponent::Shutdown";
    ConsumeProduceComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(ConsumeProduceComponent* self) {
    g_lastHook = "ConsumeProduceComponent::Serialize";
    ConsumeProduceComponent_Serialize(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallConsumeProduceComponentShutdown() {
    ConsumeProduceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140118e00);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveConsumeProduceComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallConsumeProduceComponentSerialize() {
    ConsumeProduceComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140118ef0);
    // Prologue: PUSH RSI(1)+PUSH RDI(1)+PUSH R15(2)+SUB RSP,0x20(4)+MOV R15,RCX(3)+MOV [RSP+0x40],RBX(5) = 16
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 17);
#ifdef _DEBUG
    printf("[ConsumeProduceComponent] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveConsumeProduceComponentSerialize() { RemoveHook(&g_hookSerialize); }
