// engine/reimpl/particlecomponent.cpp
// Reimplementation of ParticleComponent lifecycle methods from Application-x64.exe.
//
// ParticleComponent (0x48 bytes) manages particle effects attached to entities.
// Fields: +0x28=emitter array ptr, +0x30=emitter data, +0x38=emitter array2, +0x40=count
//
// Vtable at 0x14015a290. Non-trivial overrides:
//   Shutdown  (0x140053270) — Serializes particle state, version-gated
//   Serialize (0x140053230) — Calls particle setup helper (0x140052480)
//   Draw      (0x140052f70) — Complex: iterates emitters, positions via MeshInstance::GetNodeToWorld
//   OnCreate  (0x140052e80) — Iterates emitters, calls Scene::RemoveObject on each
//   OnRemove  (0x140052b30) — Massive (200 instructions): positions particles relative to model
//   OnUpdate2 (0x1400531b0) — Updates emitter opacity based on time
//
// Strategy: All passthrough. These methods are deeply coupled to Graphics::Scene,
// MeshInstance, and particle emitter subsystems.
//
// Prologue sizes:
//   Shutdown:  15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Serialize: 13 (PUSH RBX + SUB RSP,0x20 + MOV RBX,RCX + CALL)
//   Draw:      13 (MOV [RSP+0x20],RBX + PUSH RBP + PUSH RDI + PUSH R15 + SUB RSP,0x40)
//   OnCreate:  16 (MOV [RSP+0x18],RBX + MOV [RSP+0x20],RBP + PUSH R14 + SUB RSP,0x20)
//   OnRemove:  14 (MOV RAX,RSP + MOV [RAX+0x10],RDX + PUSH RBP + PUSH RDI + PUSH R12)
//   OnUpdate2: 15 (MOV [RSP+8],RBX + MOV [RSP+0x10],RBP + MOV [RSP+0x18],RSI + PUSH RDI)

#include "ParticleComponent.h"
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
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "ParticleComponent::Shutdown";
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Serialize(void* self) {
    g_lastHook = "ParticleComponent::Serialize";
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookSerialize.trampoline)(self);
}

static void Reimpl_Draw(void* self) {
    g_lastHook = "ParticleComponent::Draw";
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookDraw.trampoline)(self);
}

static void Reimpl_OnCreate(void* self) {
    g_lastHook = "ParticleComponent::OnCreate";
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnCreate.trampoline)(self);
}

static void Reimpl_OnRemove(void* self, void* time) {
    g_lastHook = "ParticleComponent::OnRemove";
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookOnRemove.trampoline)(self, time);
}

static void Reimpl_OnUpdate2(void* self) {
    g_lastHook = "ParticleComponent::OnUpdate2";
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnUpdate2.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallParticleComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140053270);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ParticleComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallParticleComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140053230);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[ParticleComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallParticleComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140052f70);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[ParticleComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallParticleComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140052e80);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 16);
#ifdef _DEBUG
    printf("[ParticleComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallParticleComponentOnRemove() {
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x14015a290, OnRemove = slot 8 (+0x40)
    InstallHookVTable(0x14015a290, 8, (void*)Reimpl_OnRemove, &g_hookOnRemove);
    RegisterHookInfo("ParticleComponent::OnRemove", &g_hookOnRemove);
#ifdef _DEBUG
    printf("[ParticleComponent] OnRemove vtable-hooked (original=%p)\n", g_hookOnRemove.trampoline);
#endif
}
void RemoveParticleComponentOnRemove() { RemoveHookVTable(&g_hookOnRemove); }

void InstallParticleComponentOnUpdate2() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400531b0);
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 15);
#ifdef _DEBUG
    printf("[ParticleComponent] OnUpdate2 hooked at %p\n", (void*)target);
#endif
}
void RemoveParticleComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
