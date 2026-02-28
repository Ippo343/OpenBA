// engine/reimpl/modelcomponent.cpp
// Reimplementation of ModelComponent lifecycle methods from Application-x64.exe.
//
// ModelComponent is the LARGEST component (0x170 bytes). It embeds a
// Graphics::MeshInstance sub-object at +0x28 (0x120 bytes) and a
// Math::Transform at +0x148 (0x1C bytes).
//
// Vtable at 0x140163ba0. Non-trivial overrides:
//   Shutdown  (0x140124b70) — Serializes mesh state, transform, model variant data
//   Serialize (0x140124ac0) — Resolves building data, sets mesh from resource arrays
//   Draw      (0x140124a30) — Adds mesh to Graphics::Scene if valid variant
//   OnCreate  (0x1401249d0) — Removes mesh from Scene, resets modelId
//   OnRemove  (0x140124960) — Calls MeshInstance vtable, checks IsAnimating, clears draw flag
//
// Strategy: All methods are passthrough wrappers. They are deeply coupled to the
// Graphics::MeshInstance and Graphics::Scene subsystems.
//
// Prologue sizes (from disassembly):
//   Shutdown:  15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Serialize: 12 (MOV [RSP+8],RBX + MOV [RSP+0x10],RSI + PUSH RDI)
//   Draw:      14 (PUSH RDI + SUB RSP,0x20 + CMP [RCX+0x166],0)
//   OnCreate:  14 (PUSH RBX + SUB RSP,0x20 + CMP [RCX+0x166],0)
//   OnRemove:  14 (MOV [RSP+8],RBX + PUSH RDI + SUB RSP,0x20 + MOV RAX,[RCX+0x28])

#include "ModelComponent.h"
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

// ============================================================
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Serialize(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookSerialize.trampoline)(self);
}

static void Reimpl_Draw(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookDraw.trampoline)(self);
}

static void Reimpl_OnCreate(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnCreate.trampoline)(self);
}

static void Reimpl_OnRemove(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnRemove.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallModelComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124b70);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ModelComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallModelComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124ac0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 12);
#ifdef _DEBUG
    printf("[ModelComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallModelComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124a30);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[ModelComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallModelComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401249d0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[ModelComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallModelComponentOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124960);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 14);
#ifdef _DEBUG
    printf("[ModelComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentOnRemove() { RemoveHook(&g_hookOnRemove); }
