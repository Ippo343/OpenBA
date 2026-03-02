// engine/reimpl/eventpopupdialog.cpp
// Reimplementation of EventPopupDialog vtable methods from Application-x64.exe.
//
// EventPopupDialog (0xC0 bytes), vtable at 0x14015bd80.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x1400119B0 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x140068C20 — Setup: populates event popup UI elements
//   +0x40 (Slot8):  0x140068BD0 — OnCommand: msg=0x12, id=200 -> close dialog
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140011910 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140011900 — 7 bytes, DtorBase
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x28 (Slot5), +0x30 (Slot6), +0x38 (Slot7), +0x48 (Slot9)

#include "ui/Dialogs.h"
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
static HookInfo g_hookSlot8;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x1400119B0)
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "EventPopupDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, EventPopupDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x140068C20) — Setup: populates event popup text/image elements
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "EventPopupDialog::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot8 (0x140068BD0) — OnCommand: msg=0x12, id=200 closes dialog
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id) {
    g_lastHook = "EventPopupDialog::Slot8";
    if (msg == 0x12 && id == 200) {
        uintptr_t s = (uintptr_t)self;
        uint8_t basePtr[16];
        fn_InternalBasePtr_Ctor(basePtr, self);
        fn_UI_CloseDialog(*(void**)(s + 0x10), basePtr);
    }
}

// ============================================================
// Install / Remove
// ============================================================

void InstallEventPopupDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400119B0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[EventPopupDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEventPopupDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallEventPopupDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140068C20);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 12);
#ifdef _DEBUG
    printf("[EventPopupDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveEventPopupDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallEventPopupDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140068BD0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[EventPopupDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveEventPopupDialogSlot8() { RemoveHook(&g_hookSlot8); }
