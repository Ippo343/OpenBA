// engine/reimpl/achievementdialog.cpp
// Reimplementation of AchievementDialog vtable methods from Application-x64.exe.
//
// AchievementDialog (0xD0 bytes), vtable at 0x14015b020.
// Extends DialogInstance (0xB8 base).
//
// Vtable overrides (exe-range functions):
//   +0x00 (Slot0):  0x1400103F0 — Dtor (scalar deleting destructor)
//   +0x20 (Slot4):  0x14005D490 — Setup/Init (372 instr, populates achievement table)
//   +0x30 (Slot6):  0x14005D240 — Draw/Refresh (105 instr, updates achievement display)
//   +0x40 (Slot8):  0x14005D440 — OnCommand (msg=0x12, id=100 -> close dialog)
//   +0x50 (Slot10): 0x140010560 — DialogController Dtor (secondary vtable)
//   +0x70 (Slot14): 0x14005DD40 — Update (65 instr, checks achievement element state)
//   +0x88 (Slot17): 0x14005DC50 — Draw2 (50 instr, gets entity name string)
//
// Unhookable (too small, 6-8 bytes):
//   +0x08 (Slot1):  0x140010310 — 8 bytes, DtorVec (mov rax,[rip+xx]; ret)
//   +0x10 (Slot2):  0x140010300 — 7 bytes, DtorBase (mov eax,[rip+xx]; ret)
//   +0x38 (Slot7):  0x14005D480 — 3 bytes, ret 0 (no-op handler)
//   +0x58 (Slot11): 0x1400104A0 — 8 bytes, getter (mov rax,[rip+xx]; ret)
//   +0x60 (Slot12): 0x140010490 — 7 bytes, getter (mov eax,[rip+xx]; ret)
//
// Thunks to Runtime (6-byte jmp [rip+xx], not hooked):
//   +0x18 (Slot3), +0x28 (Slot5), +0x48 (Slot9), +0x68 (Slot13),
//   +0x78 (Slot15), +0x80 (Slot16), +0x90 (Slot18), +0x98 (Slot19)

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
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot10;
static HookInfo g_hookSlot14;
static HookInfo g_hookSlot17;

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot0 Dtor (0x1400103F0) — scalar deleting destructor
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

// Slot4 (0x14005D490) — Setup: populates achievement table UI
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot6 (0x14005D240) — Draw/Refresh: updates achievement display
typedef void (*fn_Slot6_t)(void* self, void* p2, void* p3, void* p4);
static void Reimpl_Slot6(void* self, void* p2, void* p3, void* p4) {
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self, p2, p3, p4);
}

// Slot8 (0x14005D440) — OnCommand: msg 0x12/id 100 = close
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self, p2, msg, id);
}

// Slot10 (0x140010560) — DialogController dtor (secondary vtable)
typedef void* (*fn_Slot10_t)(void* self, uint32_t flags);
static void* Reimpl_Slot10(void* self, uint32_t flags) {
    return ((fn_Slot10_t)g_hookSlot10.trampoline)(self, flags);
}

// Slot14 (0x14005DD40) — Update: checks achievement element state
typedef void (*fn_Slot14_t)(void* self);
static void Reimpl_Slot14(void* self) {
    ((fn_Slot14_t)g_hookSlot14.trampoline)(self);
}

// Slot17 (0x14005DC50) — Draw2: gets entity name for display
typedef void (*fn_Slot17_t)(void* self);
static void Reimpl_Slot17(void* self) {
    ((fn_Slot17_t)g_hookSlot17.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallAchievementDialogDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400103F0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[AchievementDialog] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogDtor() { RemoveHook(&g_hookDtor); }

void InstallAchievementDialogSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005D490);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[AchievementDialog] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogSlot4() { RemoveHook(&g_hookSlot4); }

void InstallAchievementDialogSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005D240);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[AchievementDialog] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogSlot6() { RemoveHook(&g_hookSlot6); }

void InstallAchievementDialogSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005D440);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[AchievementDialog] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogSlot8() { RemoveHook(&g_hookSlot8); }

void InstallAchievementDialogSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010560);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 19);
#ifdef _DEBUG
    printf("[AchievementDialog] Slot10 hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogSlot10() { RemoveHook(&g_hookSlot10); }

void InstallAchievementDialogSlot14() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005DD40);
    InstallHook(target, (void*)Reimpl_Slot14, &g_hookSlot14, 13);
#ifdef _DEBUG
    printf("[AchievementDialog] Slot14 hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogSlot14() { RemoveHook(&g_hookSlot14); }

void InstallAchievementDialogSlot17() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005DC50);
    InstallHook(target, (void*)Reimpl_Slot17, &g_hookSlot17, 15);
#ifdef _DEBUG
    printf("[AchievementDialog] Slot17 hooked at %p\n", (void*)target);
#endif
}
void RemoveAchievementDialogSlot17() { RemoveHook(&g_hookSlot17); }
