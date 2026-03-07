// engine/reimpl/productionui.cpp
// Reimplementation of ProductionUI vtable methods from Application-x64.exe.
//
// ProductionUI (0x50 bytes), vtable at 0x14015d070.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400132B0 — Dtor (scalar deleting destructor)
//   +0x20: 0x140075DA0 — Slot4 (shutdown)
//   +0x38: 0x140075A20 — Slot7 (update handler)
//
// Unhookable (too small):
//   +0x08: 0x140013220 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140013210 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x140076300 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x48: Slot9, +0x50: Slot10

#include "ui/Panels.h"
#include "Dialog/ui_panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot7;

// ============================================================
// Passthrough wrappers
// ============================================================
// Dtor (0x1400132B0) — REIMPLEMENTED
// Ghidra: FUN_1400132b0
// 1. Set vtable
// 2. Destroy 2 x 0x10-byte vector items at +0x30 (FUN_140001740: free [0], zero [0]+[1])
// 3. Call DialogController::~DialogController
// 4. Conditional free
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ProductionUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ProductionUIAddr::Vtable);
    *(void**)self = s_vtable;

    uintptr_t s = (uintptr_t)self;

    // _eh_vector_destructor_iterator_ equivalent:
    // Destroy 2 items of 0x10 bytes at +0x30, in reverse order
    // Each item: free pointer at [0], zero [0] and [1]
    for (int i = 1; i >= 0; i--) {
        uintptr_t item = s + 0x30 + (i * 0x10);
        fn_MemFreeStatic(*(void**)item);
        *(uint64_t*)item = 0;
        *(uint64_t*)(item + 8) = 0;
    }

    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ProductionUI::Slot4";
    ProductionUI_Slot4((ProductionUI*)self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "ProductionUI::Slot7";
    ProductionUI_Slot7((ProductionUI*)self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallProductionUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400132B0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ProductionUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveProductionUIDtor() { RemoveHook(&g_hookDtor); }

void InstallProductionUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075DA0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 16);
#ifdef _DEBUG
    printf("[ProductionUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveProductionUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallProductionUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075A20);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 16);
#ifdef _DEBUG
    printf("[ProductionUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveProductionUISlot7() { RemoveHook(&g_hookSlot7); }
