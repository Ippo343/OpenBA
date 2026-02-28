// engine/reimpl/herbalistcomponent.cpp
// Reimplementation of HerbalistComponent lifecycle methods from Application-x64.exe.
//
// HerbalistComponent (0x50 bytes) is a ResourceProduceComponent subclass that
// manages herbalist buildings. Factory at 0x140006b40 does ResourceProduceComponent_Ctor
// then sets vtable to 0x140159860. No additional fields beyond ResourceProduceComponent.
//
// Vtable at 0x140159860. Non-trivial overrides:
//   Shutdown (0x14004b200) — Only non-trivial method.
//
// Note: Serialize, Draw, OnCreate, OnRemove, Update, Slot10, OnUpdate2 are all base no-ops.
//
// Shutdown decompilation (0x14004b200, 13 instructions):
//   PUSH RBX; SUB RSP,0x30; MOV RBX,RDX
//   CALL 0x14012b4a0  // ResourceProduceComponent parent Shutdown
//   Create Serializer(stream, version=0)
//   ~Serializer()
//   Return
//
// This is a minimal Shutdown that just calls the parent ResourceProduceComponent::Shutdown
// and creates an empty Serializer (no additional data to serialize).
//
// Strategy: Passthrough wrapper. The parent class Shutdown does the real work.
//
// Prologue size:
//   Shutdown: 14 (PUSH RBX + SUB RSP,0x30 + MOV RBX,RDX + CALL)

#include "HerbalistComponent.h"
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
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHerbalistComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004b200);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[HerbalistComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveHerbalistComponentShutdown() { RemoveHook(&g_hookShutdown); }
