// engine/reimpl/tutorialimagehotkey.cpp
// Reimplementation of TutorialImageHotKey vtable methods from Application-x64.exe.
//
// TutorialImageHotKey (0x48 bytes), vtable at 0x140160ab0.
// Extends TutorialImage. Ctor at 0x1400e0680.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027d70 — Dtor (19b prologue)
//   +0x08: 0x140027cf0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027ce0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e0620 — GetDescription (6b, unhookable, returns 0x1001)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e0540 — Serialize (15b prologue)
//   +0x30: 0x140027d50 — Cleanup (14b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x140027d40 — Ctor/Init (12b prologue)
//   +0x48: 0x1400e0420 — Slot9 (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Tutorial/tutorialimagehotkey_funcs.h"
#include "TutorialImages.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialImageHotKey::Dtor";
    return TutorialImageHotKey_Dtor(self, flags);
}

void InstallTutorialImageHotKeyDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027d70);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialImageHotKey] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageHotKeyDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialImageHotKey::Serialize";
    TutorialImageHotKey_Serialize(self, stream);
}

void InstallTutorialImageHotKeySerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0540);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialImageHotKey] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageHotKeySerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "TutorialImageHotKey::Cleanup";
    TutorialImageHotKey_Cleanup(self);
}

void InstallTutorialImageHotKeyCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027d50);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[TutorialImageHotKey] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageHotKeyCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Ctor/Init ---
// RE-ENABLED: near Jcc wrapper at 0x140027d40. Use vtable patch on slot 8.
static void* g_savedCtor = nullptr;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialImageHotKey::Ctor";
    return TutorialImageHotKey_Ctor(self);
}

static void PatchVtableSlot_HotKey(size_t slotIndex, void* newFunc, void** outOld) {
    uintptr_t* entry = (uintptr_t*)(ResolveAddr(Module::Exe, TutorialImageHotKeyAddr::Vtable) + slotIndex * 8);
    DWORD oldProt;
    VirtualProtect(entry, 8, PAGE_READWRITE, &oldProt);
    if (outOld) *outOld = (void*)*entry;
    *entry = (uintptr_t)newFunc;
    VirtualProtect(entry, 8, oldProt, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), entry, 8);
}

void InstallTutorialImageHotKeyCtor() {
    PatchVtableSlot_HotKey(8, (void*)Reimpl_Ctor, &g_savedCtor);
#ifdef _DEBUG
    printf("[TutorialImageHotKey] Ctor vtable-patched (slot 8)\n");
#endif
}
void RemoveTutorialImageHotKeyCtor() {
    if (g_savedCtor) {
        PatchVtableSlot_HotKey(8, g_savedCtor, nullptr);
        g_savedCtor = nullptr;
    }
}

// --- Slot9: Slot9 ---
static HookInfo g_hookSlot9;
typedef void* (*fn_Slot9_t)(void* self, void* param1, void* param2);
static void* Reimpl_Slot9(void* self, void* param1, void* param2) {
    g_lastHook = "TutorialImageHotKey::Slot9";
    return TutorialImageHotKey_Slot9(self, param1, param2);
}

void InstallTutorialImageHotKeySlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0420);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[TutorialImageHotKey] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageHotKeySlot9() { RemoveHook(&g_hookSlot9); }
