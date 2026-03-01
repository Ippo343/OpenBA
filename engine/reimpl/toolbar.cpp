// engine/reimpl/toolbar.cpp
// Reimplementation of Toolbar vtable methods from Application-x64.exe.
//
// Toolbar (0x68 bytes), vtable at 0x140160120.
// Extends Object::External.
// Factory at 0x140024ba0, Ctor at 0x1400d2130.
//
// Vtable layout (9 slots):
//   +0x00: 0x140024c10 — Slot0 Dtor (hooked)
//   +0x08: 0x140024b90 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x140024b80 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x1400d2120 — Slot3 GetSlotCount? (6 bytes, unhookable — returns 4)
//   +0x20: 0x14003a912 — Slot4 JMP thunk (6 bytes, unhookable)
//   +0x28: 0x1400d2010 — Slot5 Serialize (hooked)
//   +0x30: 0x140024bf0 — Slot6 Cleanup (hooked)
//   +0x38: 0x140001640 — Slot7 dispatch thunk (7 bytes, unhookable)
//   +0x40: 0x140024be0 — Slot8 (12 bytes, RIP-relative JNZ — unsafe for trampoline)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookToolbarDtor;
typedef void* (*fn_ToolbarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ToolbarDtor(void* self, uint32_t flags) {
    return ((fn_ToolbarDtor_t)g_hookToolbarDtor.trampoline)(self, flags);
}

// --- Slot5: Serialize ---
static HookInfo g_hookToolbarSerialize;
typedef void (*fn_ToolbarSerialize_t)(void* self, void* stream);
static void Reimpl_ToolbarSerialize(void* self, void* stream) {
    ((fn_ToolbarSerialize_t)g_hookToolbarSerialize.trampoline)(self, stream);
}

// --- Slot6: Cleanup ---
static HookInfo g_hookToolbarCleanup;
typedef void (*fn_ToolbarCleanup_t)(void* self);
static void Reimpl_ToolbarCleanup(void* self) {
    ((fn_ToolbarCleanup_t)g_hookToolbarCleanup.trampoline)(self);
}

void InstallToolbarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140024c10);
    InstallHook(target, (void*)Reimpl_ToolbarDtor, &g_hookToolbarDtor, 19);
#ifdef _DEBUG
    printf("[Toolbar] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarDtor() { RemoveHook(&g_hookToolbarDtor); }

void InstallToolbarSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d2010);
    InstallHook(target, (void*)Reimpl_ToolbarSerialize, &g_hookToolbarSerialize, 14);
#ifdef _DEBUG
    printf("[Toolbar] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarSerialize() { RemoveHook(&g_hookToolbarSerialize); }

void InstallToolbarCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140024bf0);
    InstallHook(target, (void*)Reimpl_ToolbarCleanup, &g_hookToolbarCleanup, 14);
#ifdef _DEBUG
    printf("[Toolbar] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveToolbarCleanup() { RemoveHook(&g_hookToolbarCleanup); }
