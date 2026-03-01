// engine/reimpl/tutorialimage.cpp
// Reimplementation of TutorialImage vtable methods from Application-x64.exe.
//
// TutorialImage (0x30 bytes), vtable at 0x140160a40.
// Extends Object::External.
//
// Vtable layout (10 slots):
//   +0x00: 0x140027a80 — Dtor (19b prologue)
//   +0x08: 0x140027970 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027960 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400e03a0 — GetDescription (6b, unhookable, returns 0x1000)
//   +0x20: 0x14003a912 — Serialize thunk (unhookable)
//   +0x28: 0x1400e0330 — Serialize (15b prologue)
//   +0x30: 0x140027a30 — Cleanup (15b prologue)
//   +0x38: 0x140001640 — Base dispatch (8b, unhookable)
//   +0x40: 0x1400279e0 — Ctor/Init (15b prologue)
//   +0x48: 0x1400e0300 — Slot9 (13b prologue)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookDtor;
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

void InstallTutorialImageDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027a80);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[TutorialImage] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageDtor() { RemoveHook(&g_hookDtor); }

// --- Slot5: Serialize ---
static HookInfo g_hookSerialize;
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

void InstallTutorialImageSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0330);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TutorialImage] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageSerialize() { RemoveHook(&g_hookSerialize); }

// --- Slot6: Cleanup ---
static HookInfo g_hookCleanup;
typedef void (*fn_Cleanup_t)(void* self);
static void Reimpl_Cleanup(void* self) {
    ((fn_Cleanup_t)g_hookCleanup.trampoline)(self);
}

void InstallTutorialImageCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027a30);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 15);
#ifdef _DEBUG
    printf("[TutorialImage] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageCleanup() { RemoveHook(&g_hookCleanup); }

// --- Slot8: Ctor/Init ---
static HookInfo g_hookCtor;
typedef void* (*fn_Ctor_t)(void* self);
static void* Reimpl_Ctor(void* self) {
    return ((fn_Ctor_t)g_hookCtor.trampoline)(self);
}

void InstallTutorialImageCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400279e0);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 15);
#ifdef _DEBUG
    printf("[TutorialImage] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageCtor() { RemoveHook(&g_hookCtor); }

// --- Slot9: Slot9 ---
static HookInfo g_hookSlot9;
typedef void (*fn_Slot9_t)(void* self, void* param1, void* param2);
static void Reimpl_Slot9(void* self, void* param1, void* param2) {
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param1, param2);
}

void InstallTutorialImageSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e0300);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[TutorialImage] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialImageSlot9() { RemoveHook(&g_hookSlot9); }
