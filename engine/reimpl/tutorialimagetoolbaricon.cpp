// engine/reimpl/tutorialimagetoolbaricon.cpp
// Reimplementation of TutorialImageToolbarIcon vtable methods from Application-x64.exe.
//
// TutorialImageToolbarIcon (0x48 bytes), vtable at 0x140160b90.
// Extends TutorialImage. Ctor at 0x1400e0c30.
//
// Vtable layout (10 slots):
//   +0x00: 0x140028090 — Dtor (19b prologue)
//   +0x08: 0x140028010 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140028000 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e0bd0 — GetDescription (6b, unhookable, returns 0x1001)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e0af0 — Serialize (15b prologue)
//   +0x30: 0x140028070 — Cleanup (14b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x140028060 — Ctor/Init (12b prologue)
//   +0x48: 0x1400e0940 — Slot9 (13b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialImageToolbarIcon::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

void InstallTutorialImageToolbarIconDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028090);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialImageToolbarIcon] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageToolbarIconDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialImageToolbarIcon::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialImageToolbarIconSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0af0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialImageToolbarIcon] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageToolbarIconSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "TutorialImageToolbarIcon::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialImageToolbarIconCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028070);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[TutorialImageToolbarIcon] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageToolbarIconCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialImageToolbarIcon::Ctor";
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialImageToolbarIconCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140028060);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 12);
#ifdef _DEBUG
    printf("[TutorialImageToolbarIcon] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageToolbarIconCtor() { RemoveHook(&g_hookCtor); }

// --- Slot9: Slot9 ---
static HookInfo g_hookSlot9;
typedef void (*fn_Slot9_t)(void* self, void* param1, void* param2);
static void Reimpl_Slot9(void* self, void* param1, void* param2) {
    g_lastHook = "TutorialImageToolbarIcon::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param1, param2);
}

void InstallTutorialImageToolbarIconSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0940);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[TutorialImageToolbarIcon] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageToolbarIconSlot9() { RemoveHook(&g_hookSlot9); }
