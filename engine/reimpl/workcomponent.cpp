// engine/reimpl/workcomponent.cpp
// Hook wrappers for WorkComponent lifecycle methods.
// Delegates to clean reimplementation in code/Game/Component/workcomponent.cpp.
//
// WorkComponent (0x38 bytes) manages work ID, flags, and state.
// Vtable at 0x140164be0. Non-trivial overrides:
//   Shutdown  (0x14012e710) — versioned serialization of workId, workState, workFlags
//   Draw      (0x14012e6f0) — compute work capacity
//   OnCreate  (0x14012e6c0) — check/register work slot

#include "WorkComponent.h"
#include "Component/workcomponent_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Reimplementation wrappers (delegate to clean code)
// ============================================================
static void Reimpl_Shutdown(WorkComponent* self, void* stream) {
    g_lastHook = "WorkComponent::Shutdown";
    WorkComponent_Shutdown(self, stream);
}

static void Reimpl_Draw(WorkComponent* self) {
    g_lastHook = "WorkComponent::Draw";
    WorkComponent_Draw(self);
}

static void Reimpl_OnCreate(WorkComponent* self) {
    g_lastHook = "WorkComponent::OnCreate";
    WorkComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWorkComponentShutdown() {
    WorkComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e710);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[WorkComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveWorkComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallWorkComponentDraw() {
    WorkComponent_InitFuncs();
    // Use vtable hook instead of prologue hook to avoid CALL rel32 relocation issues.
    // WorkComponent vtable = 0x140164be0, Draw = slot 6 (+0x30)
    InstallHookVTable(0x140164be0, 6, (void*)Reimpl_Draw, &g_hookDraw);
#ifdef _DEBUG
    printf("[WorkComponent] Draw vtable-hooked (slot 6)\n");
#endif
}
void RemoveWorkComponentDraw() { RemoveHookVTable(&g_hookDraw); }

void InstallWorkComponentOnCreate() {
    WorkComponent_InitFuncs();
    // Use vtable hook instead of prologue hook to avoid CALL rel32 relocation issues.
    // WorkComponent vtable = 0x140164be0, OnCreate = slot 7 (+0x38)
    InstallHookVTable(0x140164be0, 7, (void*)Reimpl_OnCreate, &g_hookOnCreate);
#ifdef _DEBUG
    printf("[WorkComponent] OnCreate vtable-hooked (slot 7)\n");
#endif
}
void RemoveWorkComponentOnCreate() { RemoveHookVTable(&g_hookOnCreate); }
