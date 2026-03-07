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
#include "Dialog/ui_dialogs.h"
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

// Virtual dtor call helper: vtable[0](obj, 1)
typedef void* (*fn_VDtor_t)(void* self, uint32_t flags);
static inline void VDtor_Delete(void* obj) {
    if (obj) {
        void** vtbl = *(void***)obj;
        ((fn_VDtor_t)vtbl[0])(obj, 1);
    }
}

// Slot0 Dtor (0x140012A30) — cleanup sub-objects, then DialogInstance dtor
// Reimplements FUN_14006ff00 (cleanup) + the outer scalar deleting destructor
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ModDialog::Dtor";
    uintptr_t s = (uintptr_t)self;

    // Set vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ModDialogAddr::Vtable);
    *(void**)self = s_vtable;

    // Destroy array of mod entries at +0xB8 (count at +0xC0, stride 0x58 per entry)
    // Each entry has 3 virtual objects at offsets +0x50, +0x48, +0x40
    int32_t count = *(int32_t*)(s + 0xC0);
    uintptr_t arrayBase = *(uintptr_t*)(s + 0xB8);
    for (int32_t i = 0; i < count; i++) {
        uintptr_t entryBase = arrayBase + (uintptr_t)i * 0x58;
        VDtor_Delete(*(void**)(entryBase + 0x50));
        VDtor_Delete(*(void**)(entryBase + 0x48));
        VDtor_Delete(*(void**)(entryBase + 0x40));
    }

    // Free array, zero fields
    fn_MemFreeStatic(*(void**)(s + 0xB8));
    *(uint64_t*)(s + 0xB8) = 0;
    *(uint64_t*)(s + 0xC0) = 0;

    // Set +0xC8 and +0xD0 to null InternalBasePtr (via temp, matching original)
    uint64_t tmp = 0;
    fn_InternalBasePtr_Ctor(&tmp, nullptr);
    *(uint64_t*)(s + 0xC8) = tmp;
    fn_InternalBasePtr_Ctor(&tmp, nullptr);
    *(uint64_t*)(s + 0xD0) = tmp;

    // Free array again (matches original — redundant but binary-compatible)
    fn_MemFreeStatic(*(void**)(s + 0xB8));
    *(uint64_t*)(s + 0xB8) = 0;
    *(uint64_t*)(s + 0xC0) = 0;

    // Base class dtor
    fn_DialogInstance_Dtor(self);

    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 (0x14006F150) — Setup: populates mod list, thumbnails, descriptions
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ModDialog::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot8 (0x14006ED40) — OnCommand: handles mod list interactions
typedef void (*fn_Slot8_t)(void* self, void* p2, int msg, int id,
                           uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8);
static void Reimpl_Slot8(void* self, void* p2, int msg, int id,
                         uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8) {
    g_lastHook = "ModDialog::Slot8";
    ModDialog_Slot8((ModDialog*)self, p2, msg, id, p5, p6, p7, p8);
}

// Slot10 (0x1400129D0) — Secondary dtor (ImageBuffer cleanup)
// self = ImageBuffer sub-object pointer, NOT the ModDialog itself
typedef void* (*fn_Slot10_t)(void* self, uint32_t flags);
static void* Reimpl_Slot10(void* self, uint32_t flags) {
    g_lastHook = "ModDialog::Slot10";
    fn_ImageBuffer_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
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
