// engine/reimpl/noiseui.cpp
// Reimplementation of NoiseUI vtable methods from Application-x64.exe.
//
// NoiseUI (0x70 bytes), vtable at 0x14015ca80.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140012DA0 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400714C0 — Slot4 (shutdown)
//   +0x48: 0x1400714A0 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x140012D20 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140012D10 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x140071BB0 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x38: Slot7, +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
// Virtual dtor call helper: vtable[0](obj, 1)
typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
static inline void VDtor_Delete(void* obj) {
    if (obj) {
        void** vtbl = *(void***)obj;
        ((fn_VDtor_t)vtbl[0])(obj, 1);
    }
}

// Dtor (0x140012DA0) — cleanup sub-objects, then DialogController dtor
// Reimplements FUN_140071bc0 (cleanup) + outer scalar deleting destructor
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "NoiseUI::Dtor";
    uintptr_t s = (uintptr_t)self;

    // Set vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, NoiseUIAddr::Vtable);
    *(void**)self = s_vtable;

    // Destroy sub-objects (order matches original: +0x40, +0x38, +0x30)
    VDtor_Delete(*(void**)(s + 0x40));
    VDtor_Delete(*(void**)(s + 0x38));
    VDtor_Delete(*(void**)(s + 0x30));

    // Base class dtor
    fn_DialogController_Dtor(self);

    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

static void Reimpl_Slot4(void* self) {
    g_lastHook = "NoiseUI::Slot4";
    NoiseUI_Slot4((NoiseUI*)self);
}

static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "NoiseUI::Slot9";
    NoiseUI_Slot9((NoiseUI*)self, param2, param3, param4, param5, param6);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallNoiseUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012DA0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[NoiseUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveNoiseUIDtor() { RemoveHook(&g_hookDtor); }

void InstallNoiseUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400714C0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 14);
#ifdef _DEBUG
    printf("[NoiseUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveNoiseUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallNoiseUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400714A0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 19);
#ifdef _DEBUG
    printf("[NoiseUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveNoiseUISlot9() { RemoveHook(&g_hookSlot9); }
