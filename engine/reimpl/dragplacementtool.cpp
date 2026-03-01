// engine/reimpl/dragplacementtool.cpp
// Reimplementation of DragPlacementTool vtable methods from Application-x64.exe.
//
// DragPlacementTool (0x48 bytes), vtable at 0x140161b90.
// Extends DragTool (0x38 bytes).
// Factory at 0x14002d8b0, Ctor at 0x1400fc530.
//
// Vtable layout (15 slots):
//   +0x00: 0x14002d900 — Slot0 Dtor (hooked)
//   +0x08: 0x14002d8a0 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002d890 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400fc3e0 — Slot4 Startup (hooked)
//   +0x28: 0x1400fc3c0 — Slot5 (3 bytes RET, unhookable)
//   +0x30: 0x1400fc6b0 — Slot6 OnKey (shared with DragTool — hooked there)
//   +0x38: 0x1400fc3c0 — Slot7 (3 bytes RET, unhookable)
//   +0x40: 0x1400fbe40 — Slot8 Draw (hooked)
//   +0x48: 0x1400fc640 — Slot9 Shutdown (JZ rel8 in prologue — unsafe for trampoline)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x1400fb900 — Slot13 CalcPath (hooked)
//   +0x70: 0x1400fbae0 — Slot14 CalcCost (hooked)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDragPlacementToolDtor;
typedef void* (*fn_DragPlacementToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_DragPlacementToolDtor(void* self, uint32_t flags) {
    return ((fn_DragPlacementToolDtor_t)g_hookDragPlacementToolDtor.trampoline)(self, flags);
}

// --- Slot4: Startup ---
static HookInfo g_hookDragPlacementToolStartup;
typedef void (*fn_DragPlacementToolStartup_t)(void* self);
static void Reimpl_DragPlacementToolStartup(void* self) {
    ((fn_DragPlacementToolStartup_t)g_hookDragPlacementToolStartup.trampoline)(self);
}

// --- Slot8: Draw ---
static HookInfo g_hookDragPlacementToolDraw;
typedef void (*fn_DragPlacementToolDraw_t)(void* self, void* context, char param);
static void Reimpl_DragPlacementToolDraw(void* self, void* context, char param) {
    ((fn_DragPlacementToolDraw_t)g_hookDragPlacementToolDraw.trampoline)(self, context, param);
}

// --- Slot13: CalcPath ---
static HookInfo g_hookDragPlacementToolCalcPath;
typedef uint64_t (*fn_DragPlacementToolCalcPath_t)(void* self, int x, int z);
static uint64_t Reimpl_DragPlacementToolCalcPath(void* self, int x, int z) {
    return ((fn_DragPlacementToolCalcPath_t)g_hookDragPlacementToolCalcPath.trampoline)(self, x, z);
}

// --- Slot14: CalcCost ---
static HookInfo g_hookDragPlacementToolCalcCost;
typedef uint64_t (*fn_DragPlacementToolCalcCost_t)(void* self, int x, int z, uint32_t* a, int* b, int* c, uint32_t* d);
static uint64_t Reimpl_DragPlacementToolCalcCost(void* self, int x, int z, uint32_t* a, int* b, int* c, uint32_t* d) {
    return ((fn_DragPlacementToolCalcCost_t)g_hookDragPlacementToolCalcCost.trampoline)(self, x, z, a, b, c, d);
}

void InstallDragPlacementToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002d900);
    InstallHook(target, (void*)Reimpl_DragPlacementToolDtor, &g_hookDragPlacementToolDtor, 19);
#ifdef _DEBUG
    printf("[DragPlacementTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveDragPlacementToolDtor() { RemoveHook(&g_hookDragPlacementToolDtor); }

void InstallDragPlacementToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fc3e0);
    InstallHook(target, (void*)Reimpl_DragPlacementToolStartup, &g_hookDragPlacementToolStartup, 15);
#ifdef _DEBUG
    printf("[DragPlacementTool] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveDragPlacementToolStartup() { RemoveHook(&g_hookDragPlacementToolStartup); }

void InstallDragPlacementToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fbe40);
    InstallHook(target, (void*)Reimpl_DragPlacementToolDraw, &g_hookDragPlacementToolDraw, 15);
#ifdef _DEBUG
    printf("[DragPlacementTool] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveDragPlacementToolDraw() { RemoveHook(&g_hookDragPlacementToolDraw); }

void InstallDragPlacementToolCalcPath() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fb900);
    InstallHook(target, (void*)Reimpl_DragPlacementToolCalcPath, &g_hookDragPlacementToolCalcPath, 19);
#ifdef _DEBUG
    printf("[DragPlacementTool] CalcPath hooked at %p\n", (void*)target);
#endif
}
void RemoveDragPlacementToolCalcPath() { RemoveHook(&g_hookDragPlacementToolCalcPath); }

void InstallDragPlacementToolCalcCost() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fbae0);
    InstallHook(target, (void*)Reimpl_DragPlacementToolCalcCost, &g_hookDragPlacementToolCalcCost, 13);
#ifdef _DEBUG
    printf("[DragPlacementTool] CalcCost hooked at %p\n", (void*)target);
#endif
}
void RemoveDragPlacementToolCalcCost() { RemoveHook(&g_hookDragPlacementToolCalcCost); }
