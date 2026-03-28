// engine/reimpl/cropui.cpp
// Reimplementation of CropUI vtable methods from Application-x64.exe.
//
// CropUI (0x60 bytes), vtable at 0x14015d330 (SelectResourceUI base vtable).
// Extends SelectResourceUI (abstract) which extends DialogController (0x30 base).
// Note: This vtable is shared as the SelectResourceUI base. OrchardUI and PastureUI
// have their own override vtables but may share some method pointers.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140013CA0 — Dtor (scalar deleting destructor)
//   +0x20: 0x140078BE0 — Slot4 (setup/refresh)
//   +0x38: 0x140078CB0 — Slot7 (update handler)
//   +0x40: 0x140078D30 — Slot8 (additional handler)
//   +0x48: 0x140078D80 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140013C90 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140013C80 — 7 bytes, Slot2 (Cleanup getter)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x30: Slot6, +0x50: Slot10

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
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;

// ============================================================
// Reimplemented wrappers
// ============================================================

// Dtor (scalar deleting destructor) — REIMPLEMENTED
// Original: 0x140013CA0. Sets vtable, calls base dtor, conditional free.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CropUI::Dtor";

    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, CropUIAddr::Vtable);

    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// Slot4 (setup/refresh) — REIMPLEMENTED (finds selectResource element, sets ID, populates combo)
#include "Dialog/ui_panels.h"
static void Reimpl_Slot4(void* self) {
    g_lastHook = "CropUI::Slot4";
    CropUI_Slot4((CropUI*)self);
}

// Slot7 (update handler) — REIMPLEMENTED (enable/disable combo based on item count)
static void Reimpl_Slot7(void* self) {
    g_lastHook = "CropUI::Slot7";
    CropUI_Slot7((CropUI*)self);
}

// Slot8 (additional handler) — REIMPLEMENTED (repopulate combo if count changed)
static void Reimpl_Slot8(void* self) {
    g_lastHook = "CropUI::Slot8";
    CropUI_Slot8((CropUI*)self);
}

// Slot9 (notification handler) — REIMPLEMENTED (combo selection → set resource type)
typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "CropUI::Slot9";
    SelectResourceUI_Slot9(self, (void*)param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCropUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140013CA0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CropUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCropUIDtor() { RemoveHook(&g_hookDtor); }

void InstallCropUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078BE0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[CropUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveCropUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallCropUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078CB0);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 15);
#ifdef _DEBUG
    printf("[CropUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveCropUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallCropUISlot8() {
    // Bug class 9: 0x140078D30 is shared across CropUI, OrchardUI, PastureUI, BuildUI vtables.
    // Prologue hook would intercept all callers — use vtable patch for CropUI only.
    // CropUI vtable = 0x14015d330, Slot8 = +0x40 (index 8)
    InstallHookVTable(0x14015d330, 8, (void*)Reimpl_Slot8, &g_hookSlot8);
#ifdef _DEBUG
    printf("[CropUI] Slot8 vtable-hooked (slot 8)\n");
#endif
}
void RemoveCropUISlot8() { RemoveHookVTable(&g_hookSlot8); }

void InstallCropUISlot9() {
    // Bug class 9: 0x140078D80 is shared across CropUI, OrchardUI, BuildUI vtables.
    // Prologue hook would intercept all callers — use vtable patch for CropUI only.
    // CropUI vtable = 0x14015d330, Slot9 = +0x48 (index 9)
    InstallHookVTable(0x14015d330, 9, (void*)Reimpl_Slot9, &g_hookSlot9);
#ifdef _DEBUG
    printf("[CropUI] Slot9 vtable-hooked (slot 9)\n");
#endif
}
void RemoveCropUISlot9() { RemoveHookVTable(&g_hookSlot9); }
