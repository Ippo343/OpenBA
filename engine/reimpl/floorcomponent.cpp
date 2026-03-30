// engine/reimpl/floorcomponent.cpp
// Reimplementation of FloorComponent lifecycle methods from Application-x64.exe.
//
// FloorComponent — Component subclass (0x30 bytes).
// Fields: floorFlag (byte +0x28), floorId (int32 +0x2C, init -1).
// Manages floor/ground plane rendering for buildings.
//
// Vtable at 0x140163360:
//   [3] Shutdown        = 0x14011F560  <-- HOOKED (serialize floorFlag as bool)
//   [6] Draw            = 0x14011F340  <-- HOOKED (IsDerivedFrom check + render)
//   [7] OnCreate        = 0x14011F2A0  <-- HOOKED (remove existing floor mesh)
//  [11] OnUpdate2       = 0x14011F540  <-- unhookable (12-byte stub)

#include "FloorComponent.h"
#include "Component/floorcomponent_funcs.h"
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
// Shutdown (0x14011F560)
// ============================================================
static void Reimpl_Shutdown(FloorComponent* self, void* stream) {
    g_lastHook = "FloorComponent::Shutdown";
    FloorComponent_Shutdown(self, stream);
}

// ============================================================
// Draw (0x14011F340)
// ============================================================
static void Reimpl_Draw(FloorComponent* self) {
    g_lastHook = "FloorComponent::Draw";
    FloorComponent_Draw(self);
}

// ============================================================
// OnCreate (0x14011F2A0)
// ============================================================
static void Reimpl_OnCreate(FloorComponent* self) {
    g_lastHook = "FloorComponent::OnCreate";
    FloorComponent_OnCreate(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFloorComponentShutdown() {
    FloorComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011F560);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[FloorComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveFloorComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallFloorComponentDraw() {
    FloorComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011F340);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[FloorComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveFloorComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallFloorComponentOnCreate() {
    FloorComponent_InitFuncs();

    uintptr_t target = ResolveAddr(Module::Exe, 0x14011F2A0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[FloorComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveFloorComponentOnCreate() { RemoveHook(&g_hookOnCreate); }

// NOTE: OnUpdate2 (0x14011F540) is a 12-byte stub function. The entire function
// body is exactly 12 bytes (CMP+JNZ+RET), making it unhookable — the trampoline
// would save the entire function but have nowhere to jump back to. This method
// is a conditional call into FUN_14011F390 (the complex floor mesh placement),
// which would need its own reimpl. Skipped for now.
