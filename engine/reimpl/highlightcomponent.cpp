// engine/reimpl/highlightcomponent.cpp
// Reimplementation of HighlightComponent lifecycle methods from Application-x64.exe.
//
// HighlightComponent (0x128 bytes) manages selection/highlight outlines around entities.
// Embeds Graphics::EdgeMeshInstance at +0x28 (0xD0 bytes) and Math::Transform at +0x100.
//
// Vtable at 0x140159990. Non-trivial overrides:
//   Shutdown  (0x14004c880) — Minimal: Component_Serialize + empty Serializer (version 0)
//   OnRemove  (0x14004c850) — Tiny function (not defined in Ghidra as function, inline thunk)
//
// Note: Serialize, Draw, OnCreate are all no-ops (base addresses).
// HighlightComponent is mostly a data container that the rendering system reads directly.
//
// Strategy: Shutdown is a passthrough (calls Component_Serialize + creates empty Serializer).
// OnRemove at 0x14004c850 is not recognized as a function by Ghidra (inline code), skip it.
//
// Prologue size:
//   Shutdown: 14 (PUSH RBX + SUB RSP,0x30 + MOV RBX,RDX + CALL)

#include "HighlightComponent.h"
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
    g_lastHook = "HighlightComponent::Shutdown";
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHighlightComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14004c880);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[HighlightComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveHighlightComponentShutdown() { RemoveHook(&g_hookShutdown); }
