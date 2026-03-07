// engine/reimpl/tutorialhighlightdialog.cpp
// Reimplementation of TutorialHighlightDialog vtable methods from Application-x64.exe.
//
// TutorialHighlightDialog (0xF0 bytes), vtable at 0x1401609e0.
// Extends DialogInstance (0xB8 base).
// Has Ctor at 0x1400e0290.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027910 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x1400278b0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400278a0 — GetTypeId (7b, unhookable)
//   +0x18: 0x14003a948 — Runtime thunk (6b, unhookable)
//   +0x20: 0x1400e0020 — Startup: set up highlight overlay (hookable, 13b)
//   +0x28: 0x14003a94e — Runtime thunk (6b, unhookable)
//   +0x30: 0x1400dffe0 — Update: rel branch at byte 8, UNHOOKABLE via trampoline
//   +0x38: 0x1400e0000 — Load: 11b MOV+ADD+RET, too short for trampoline, UNHOOKABLE
//   +0x40: 0x14003aa1a — Runtime thunk (6b, unhookable)
//   +0x48: 0x14003a954 — Runtime thunk (6b, unhookable)
//
// Update and Load use vtable slot patching instead of function trampolining
// because they are too short or contain relative branches that would break
// when copied to a trampoline buffer.

#include "ui/Dialogs.h"
#include "Dialog/ui_dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookStartup;

// Vtable patching state for Update and Load (unhookable via trampoline)
static void* g_savedUpdate = nullptr;  // original vtable[6] (slot +0x30)
static void* g_savedLoad   = nullptr;  // original vtable[7] (slot +0x38)

// Helper: write one pointer into a (possibly read-only) vtable slot.
static void PatchVtableSlot(uintptr_t vtableBase, size_t slot, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(vtableBase + slot * sizeof(uintptr_t));
    DWORD oldProtect;
    VirtualProtect(entry, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, sizeof(uintptr_t), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(uintptr_t));
}

static void RestoreVtableSlot(uintptr_t vtableBase, size_t slot, void* savedFunc) {
    if (!savedFunc) return;
    uintptr_t* entry = (uintptr_t*)(vtableBase + slot * sizeof(uintptr_t));
    DWORD oldProtect;
    VirtualProtect(entry, sizeof(uintptr_t), PAGE_READWRITE, &oldProtect);
    *entry = (uintptr_t)savedFunc;
    VirtualProtect(entry, sizeof(uintptr_t), oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), entry, sizeof(uintptr_t));
}

// ============================================================
// Exe-internal helpers called by Update (resolved by exe address)
// ============================================================
//
// FUN_1400dfac0 — positions glowGroup (dataC0) and arrow (dataD0) based on
//   the target UI element (dataE0). Uses dataD8 as rotation angle (90/180/270
//   degree cases). Called when dataE8 == 0.
//
// FUN_1400dfda0 — positions arrow (dataD0) based on a Rectangle stored in
//   dataE8. Hides glowGroup (dataC0) when the rect doesn't intersect screen.
//   Called when dataE8 != 0.
typedef void (*fn_UpdateHelper_t)(void* self);
static fn_UpdateHelper_t s_UpdateHelper_NoRect  = nullptr; // 0x1400dfac0
static fn_UpdateHelper_t s_UpdateHelper_WithRect = nullptr; // 0x1400dfda0

// ============================================================
// Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialHighlightDialog::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, TutorialHighlightDialogAddr::Vtable);
    *(void**)self = s_vtable;
    fn_DialogInstance_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot 4: Startup(this) — set up tutorial highlight overlay — REIMPLEMENTED
typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "TutorialHighlightDialog::Startup";
    TutorialHighlightDialog_Startup((TutorialHighlightDialog*)self);
}

// Slot 6 (+0x30): Update(this) — REIMPLEMENTED via vtable patch
// Original 0x1400dffe0 is unhookable: CMP+JNZ at bytes 0-13, rel branch at byte 8.
//
// Logic:
//   if (self->dataE8 != 0):
//     FUN_1400dfda0(self)  — position arrow based on world-space rect in dataE8
//   else:
//     FUN_1400dfac0(self)  — position glow+arrow based on UI element in dataE0
static void Reimpl_Update(TutorialHighlightDialog* self) {
    g_lastHook = "TutorialHighlightDialog::Update";
    if (self->dataE8 != 0)
        s_UpdateHelper_WithRect(self);
    else
        s_UpdateHelper_NoRect(self);
}

// Slot 7 (+0x38): Load(this, param2) — REIMPLEMENTED via vtable patch
// Original 0x1400e0000 is only 11 bytes (MOV RAX,[RDX]; ADD [RCX+0xB8],RAX; RET).
// Too short for a 12-byte trampoline JMP.
//
// Accumulates delta-time ticks into self->dataB8 (a System::Time value).
// param2 is a pointer to a Time tick count to add.
static void Reimpl_Load(TutorialHighlightDialog* self, void* param2) {
    g_lastHook = "TutorialHighlightDialog::Load";
    TutorialHighlightDialog_Load(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialHighlightDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027910);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TutorialHighlightDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialHighlightDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialHighlightDialogStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0020);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 17);
#ifdef _DEBUG
    printf("[TutorialHighlightDialog] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialHighlightDialogStartup() { RemoveHook(&g_hookStartup); }

// Slot 6: Update — vtable patch (unhookable via trampoline, rel branch at byte 8)
void InstallTutorialHighlightDialogUpdate() {
    // Resolve exe helper functions at their Ghidra addresses.
    s_UpdateHelper_NoRect  = (fn_UpdateHelper_t)ResolveAddr(Module::Exe, 0x1400dfac0);
    s_UpdateHelper_WithRect = (fn_UpdateHelper_t)ResolveAddr(Module::Exe, 0x1400dfda0);

    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialHighlightDialogAddr::Vtable);
    PatchVtableSlot(vtable, 6, (void*)Reimpl_Update, &g_savedUpdate);
#ifdef _DEBUG
    printf("[TutorialHighlightDialog] Update vtable-patched (slot 6)\n");
#endif
}
void RemoveTutorialHighlightDialogUpdate() {
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialHighlightDialogAddr::Vtable);
    RestoreVtableSlot(vtable, 6, g_savedUpdate);
    g_savedUpdate = nullptr;
}

// Slot 7: Load — vtable patch (unhookable via trampoline, only 11 bytes)
void InstallTutorialHighlightDialogLoad() {
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialHighlightDialogAddr::Vtable);
    PatchVtableSlot(vtable, 7, (void*)Reimpl_Load, &g_savedLoad);
#ifdef _DEBUG
    printf("[TutorialHighlightDialog] Load vtable-patched (slot 7)\n");
#endif
}
void RemoveTutorialHighlightDialogLoad() {
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialHighlightDialogAddr::Vtable);
    RestoreVtableSlot(vtable, 7, g_savedLoad);
    g_savedLoad = nullptr;
}
