// engine/reimpl/modelcomponent.cpp
// Hook wrappers for ModelComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/modelcomponent.cpp.
//
// ModelComponent (0x170 bytes) is the largest component. It embeds a
// Graphics::MeshInstance sub-object at +0x28 (0x120 bytes) and a
// Math::Transform at +0x148 (0x1C bytes).
//
// Vtable at 0x140163ba0. Non-trivial overrides:
//   Shutdown  (0x140124b70) — Serializes mesh state, transform, model variant data
//   Serialize (0x140124ac0) — Reads building data, sets mesh from resource arrays
//   Draw      (0x140124a30) — Adds mesh to Graphics::Scene if valid variant
//   OnCreate  (0x1401249d0) — Removes mesh from Scene, resets modelId
//   OnRemove  (0x140124960) — Calls MeshInstance update, checks animating, clears flag

#include "ModelComponent.h"
#include "Component/modelcomponent_funcs.h"
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
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(ModelComponent* self, void* stream) {
    g_lastHook = "ModelComponent::Shutdown";
    ModelComponent_Shutdown(self, stream);
}

static void Reimpl_Serialize(ModelComponent* self) {
    g_lastHook = "ModelComponent::Serialize";
    ModelComponent_Serialize(self);
}

static void Reimpl_Draw(ModelComponent* self) {
    g_lastHook = "ModelComponent::Draw";
    ModelComponent_Draw(self);
}

static void Reimpl_OnCreate(ModelComponent* self) {
    g_lastHook = "ModelComponent::OnCreate";
    ModelComponent_OnCreate(self);
}

// Note: vtable slot +0x40 implicitly passes RDX (Time*) through to
// MeshInstance::Update(const Time&). Must forward it or RDX gets clobbered.
static void Reimpl_OnRemove(ModelComponent* self, void* time) {
    g_lastHook = "ModelComponent::OnRemove";
    ModelComponent_OnRemove(self, time);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallModelComponentShutdown() {
    ModelComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124b70);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ModelComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallModelComponentSerialize() {
    ModelComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124ac0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ModelComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallModelComponentDraw() {
    ModelComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124a30);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[ModelComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallModelComponentOnCreate() {
    ModelComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401249d0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[ModelComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallModelComponentOnRemove() {
    ModelComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140124960);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 14);
#ifdef _DEBUG
    printf("[ModelComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveModelComponentOnRemove() { RemoveHook(&g_hookOnRemove); }
