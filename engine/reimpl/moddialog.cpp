// engine/reimpl/moddialog.cpp
// Reimplementation of ModDialog vtable methods from Application-x64.exe.
//
// ModDialog (0xD8 bytes), vtable at 0x14015c870.
// Extends DialogInstance (0xB8 base).
// Has a secondary vtable at Slot10-19 (DialogController-like interface).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x140012A30 — Dtor (calls FUN_14006ff00 cleanup then MemFree)
//   +0x20 (Slot4):  0x14006F150 — Setup: populates mod list table UI (758 instr, huge)
//   +0x30 (Slot6):  0x14006E480 — 7 bytes, JMP [rip+xx] thunk (unhookable)
//   +0x40 (Slot8):  0x14006ED40 — OnCommand: mod enable/disable/details/refresh (87 instr)
//   +0x50 (Slot10): 0x1400129D0 — Secondary dtor (ImageBuffer cleanup)
//
// Unhookable (too small):
//   +0x08 (Slot1):  0x1400128F0 — 8 bytes, DtorVec
//   +0x10 (Slot2):  0x1400128E0 — 7 bytes, DtorBase
//   +0x30 (Slot6):  0x14006E480 — 7 bytes, JMP [rip+xx] (indirect thunk to Runtime)
//
// Thunks to Runtime (6-byte jmp, not hooked):
//   +0x18 (Slot3), +0x28 (Slot5), +0x38 (Slot7), +0x48 (Slot9)
//   Secondary vtable: +0x58-0x98 (Slot11-19) all thunks

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
static HookInfo g_hookSlot10;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x140012A30) — calls cleanup, then conditional MemFree
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

// Slot4 (0x14006F150) — Setup: populates mod list, thumbnails, descriptions
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot8 (0x14006ED40) — OnCommand: handles mod list interactions
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id,
                           uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id,
                         uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self, p2, msg, id, p5, p6, p7, p8);
}

// Slot10 (0x1400129D0) — Secondary dtor (ImageBuffer cleanup)
typedef void* (*fn_Slot10_t)(void* self, uint32_t flags);
static void* Reimpl_Slot10(void* self, uint32_t flags) {
    return ((fn_Slot10_t)g_hookSlot10.trampoline)(self, flags);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallModDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012A30);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 12);
#ifdef _DEBUG
    printf("[ModDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveModDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallModDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006F150);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ModDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveModDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallModDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006ED40);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[ModDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveModDialogSlot8() { RemoveHook(&g_hookSlot8); }

void InstallModDialogSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400129D0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 12);
#ifdef _DEBUG
    printf("[ModDialog] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveModDialogSlot10() { RemoveHook(&g_hookSlot10); }
