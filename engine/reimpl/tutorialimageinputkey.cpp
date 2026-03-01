// engine/reimpl/tutorialimageinputkey.cpp
// Reimplementation of TutorialImageInputKey vtable methods from Application-x64.exe.
//
// TutorialImageInputKey (0x38 bytes), vtable at 0x140160b20.
// Extends TutorialImage.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027fa0 — Dtor (19b prologue)
//   +0x08: 0x140027e90 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027e80 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e08a0 — GetDescription (6b, unhookable, returns 0x1001)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e07f0 — Serialize (15b prologue)
//   +0x30: 0x140027f50 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x140027f00 — Ctor/Init (15b prologue)
//   +0x48: 0x1400e06e0 — Slot9 (15b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialImageInputKey::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

void InstallTutorialImageInputKeyDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027fa0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialImageInputKey] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageInputKeyDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "TutorialImageInputKey::Serialize";
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialImageInputKeySerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e07f0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialImageInputKey] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageInputKeySerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    g_lastHook = "TutorialImageInputKey::Cleanup";
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialImageInputKeyCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027f50);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialImageInputKey] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageInputKeyCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    g_lastHook = "TutorialImageInputKey::Ctor";
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialImageInputKeyCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027f00);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialImageInputKey] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageInputKeyCtor() { RemoveHook(&g_hookCtor); }

// --- Slot9: Slot9 ---
static HookInfo g_hookSlot9;
typedef void (*fn_Slot9_t)(void* self, void* param1, void* param2);
static void Reimpl_Slot9(void* self, void* param1, void* param2) {
    g_lastHook = "TutorialImageInputKey::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param1, param2);
}

void InstallTutorialImageInputKeySlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e06e0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[TutorialImageInputKey] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageInputKeySlot9() { RemoveHook(&g_hookSlot9); }
