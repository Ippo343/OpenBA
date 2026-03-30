// engine/reimpl/particlecomponent.cpp
// Hook wrappers for ParticleComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/particlecomponent.cpp.

#include "ParticleComponent.h"
#include "Component/particlecomponent_funcs.h"
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
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;
static HookInfo g_hookOnRemove;
static HookInfo g_hookOnUpdate2;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(ParticleComponent* self, void* stream) {
    g_lastHook = "ParticleComponent::Shutdown";
    ParticleComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(ParticleComponent* self) {
    g_lastHook = "ParticleComponent::Serialize";
    ParticleComponent_Serialize(self);
}

static void Reimpl_Draw(ParticleComponent* self) {
    g_lastHook = "ParticleComponent::Draw";
    ParticleComponent_Draw(self);
}

static void Reimpl_OnCreate(ParticleComponent* self) {
    g_lastHook = "ParticleComponent::OnCreate";
    ParticleComponent_OnCreate(self);
}

static void Reimpl_OnRemove(ParticleComponent* self, void* time) {
    g_lastHook = "ParticleComponent::OnRemove";
    // OnRemove uses vtable hook — the original function ptr is stored
    // in g_hookOnRemove.trampoline by InstallHookVTable.
    // For the clean code path, we pass through to the original since
    // OnRemove is too complex for clean reimpl.
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookOnRemove.trampoline)(self, time);
}

static void Reimpl_OnUpdate2(ParticleComponent* self) {
    g_lastHook = "ParticleComponent::OnUpdate2";
    ParticleComponent_OnUpdate2(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallParticleComponentShutdown() {
    ParticleComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140053270);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ParticleComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallParticleComponentSerialize() {
    ParticleComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140053230);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[ParticleComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallParticleComponentDraw() {
    ParticleComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140052f70);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[ParticleComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallParticleComponentOnCreate() {
    ParticleComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x140052e80);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 16);
#ifdef _DEBUG
    printf("[ParticleComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallParticleComponentOnRemove() {
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    InstallHookVTable(0x14015a290, 8, (void*)Reimpl_OnRemove, &g_hookOnRemove);
    RegisterHookInfo("ParticleComponent::OnRemove", &g_hookOnRemove);
#ifdef _DEBUG
    printf("[ParticleComponent] OnRemove vtable-hooked (original=%p)\n", g_hookOnRemove.trampoline);
#endif
}
void RemoveParticleComponentOnRemove() { RemoveHookVTable(&g_hookOnRemove); }

void InstallParticleComponentOnUpdate2() {
    ParticleComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400531b0);
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 15);
#ifdef _DEBUG
    printf("[ParticleComponent] OnUpdate2 hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
