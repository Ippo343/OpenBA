// engine/reimpl/modbrowseui.cpp
// Reimplementation of ModBrowseUI vtable methods from Application-x64.exe.
//
// ModBrowseUI (0x30 bytes), vtable at 0x14015c500.
// Extends DialogController (0x30 base). No subclass fields.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140012720 — Dtor (scalar deleting destructor)
//
// Unhookable (too small):
//   +0x08: 0x1400126C0 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x1400126B0 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14006D390 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x20: Slot4, +0x28: Slot5, +0x30: Slot6, +0x38: Slot7, +0x40: Slot8, +0x48: Slot9, +0x50: Slot10

#include "ui/Panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;

// ============================================================
// Reimplemented wrappers
// ============================================================

// Dtor (scalar deleting destructor) — REIMPLEMENTED
// Original: 0x140012720. Sets vtable, calls base dtor, conditional free.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ModBrowseUI::Dtor";

    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ModBrowseUIAddr::Vtable);

    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallModBrowseUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012720);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ModBrowseUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveModBrowseUIDtor() { RemoveHook(&g_hookDtor); }
