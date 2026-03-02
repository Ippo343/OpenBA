// engine/reimpl/loaderdialog.cpp
// Reimplementation of LoaderDialog vtable methods from Application-x64.exe.
//
// LoaderDialog (0xC8 bytes), vtable at 0x14015f300.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140020C70 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x1400B2D90 — Setup: finds "loadingImage" element, stores ptr at +0xB8
//   +0x30 (Slot6):  0x1400B2D00 — Draw: rotates loading image via SetRotation
//   +0x38 (Slot7):  0x1400B2D50 — Update: accumulates time-based rotation angle
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x140020BA0 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x140020B90 — 7 bytes, DtorBase
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x28 (Slot5), +0x40 (Slot8), +0x48 (Slot9)

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
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140020C70)
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "LoaderDialog::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, LoaderDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x1400B2D90) — Setup: finds "loadingImage" element, stores at +0xB8
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "LoaderDialog::Slot4";
    uintptr_t s = (uintptr_t)self;
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, "loadingImage");
    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(retBuf, self, nameStr);
    *(void**)(s + 0xB8) = *(void**)retBuf;
    fn_String_Dtor(nameStr);
}

// Slot6 (0x1400B2D00) — Draw: applies rotation to loading spinner
typedef void (*fn_Slot6_t)(void* self);
static void Reimpl_Slot6(void* self) {
    g_lastHook = "LoaderDialog::Slot6";
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

// Slot7 (0x1400B2D50) — Update: accumulates rotation angle from elapsed time
typedef void (*fn_Slot7_t)(void* self, void* time);
static void Reimpl_Slot7(void* self, void* time) {
    g_lastHook = "LoaderDialog::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self, time);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallLoaderDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020C70);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[LoaderDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveLoaderDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallLoaderDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B2D90);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[LoaderDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveLoaderDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallLoaderDialogSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B2D00);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 18);
#ifdef _DEBUG
    printf("[LoaderDialog] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveLoaderDialogSlot6() { RemoveHook(&g_hookSlot6); }

void InstallLoaderDialogSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B2D50);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 12);
#ifdef _DEBUG
    printf("[LoaderDialog] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveLoaderDialogSlot7() { RemoveHook(&g_hookSlot7); }
