// engine/reimpl/destroycomponent.cpp
// Reimplementation of DestroyComponent lifecycle methods from Application-x64.exe.
//
// DestroyComponent — ControllerComponent subclass (0x68 bytes).
// Fields: field48 (Time), destroyFlags (uint16 +0x50), field51 (bool),
//         field54 (uint32), field58 (ptr), field60 (entity ref / mesh ID).
// Manages building demolition/destruction process.
//
// Vtable at 0x140163120:
//   [3] Shutdown        = 0x14011DB50  <-- HOOKED (serialize all destroy state fields)
//   [4] Serialize       = 0x14011DB00  <-- HOOKED (OnCreate: copy mesh ref from building data)
//   Other slots: NO-OP (Draw=base, OnCreate=base, OnRemove, Update, Slot10, OnUpdate2)

#include "DestroyComponent.h"
#include "Component/destroycomponent_funcs.h"
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
// Shutdown (0x14011DB50)
// ============================================================
static void Reimpl_Shutdown(DestroyComponent* self, void* stream) {
    g_lastHook = "DestroyComponent::Shutdown";
    DestroyComponent_Shutdown(self, stream);
}

// ============================================================
// Serialize (0x14011DB00) — actually vtable slot [4]
// ============================================================
static void Reimpl_Serialize(DestroyComponent* self) {
    g_lastHook = "DestroyComponent::Serialize";
    DestroyComponent_Serialize(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDestroyComponentShutdown() {
    DestroyComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011DB50);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[DestroyComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallDestroyComponentSerialize() {
    DestroyComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011DB00);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[DestroyComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyComponentSerialize() { RemoveHook(&g_hookSerialize); }
