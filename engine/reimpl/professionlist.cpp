// engine/reimpl/professionlist.cpp
// Reimplementation of ProfessionList vtable methods from Application-x64.exe.
//
// ProfessionList (0x50 bytes), vtable at 0x14015ef50.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x14001e760 — Dtor(self, flags)
//   +0x08: 0x14001e5b0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x14001e5a0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400a8eb0 — Slot3/Serialize? (6b, unhookable: mov eax,2; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400a8e30 — Slot5/Load
//   +0x30: 0x14001e690 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x14001e630 — Slot8/Serialize2

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot8;

// ============================================================
// Original function typedefs
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
typedef void  (*fn_Slot5_t)(void* self);
typedef void  (*fn_Slot6_t)(void* self);
typedef void  (*fn_Slot8_t)(void* self);

// ============================================================
// Passthrough wrappers
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

static void Reimpl_Slot5(void* self) {
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self);
}

static void Reimpl_Slot6(void* self) {
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallProfessionListDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001e760);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[ProfessionList] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionListDtor() { RemoveHook(&g_hookDtor); }

void InstallProfessionListSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400a8e30);
    // Prologue: push rbx(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[ProfessionList] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionListSlot5() { RemoveHook(&g_hookSlot5); }

void InstallProfessionListSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001e690);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 15);
#ifdef _DEBUG
    printf("[ProfessionList] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionListSlot6() { RemoveHook(&g_hookSlot6); }

void InstallProfessionListSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001e630);
    // Prologue: push rbx(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[ProfessionList] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionListSlot8() { RemoveHook(&g_hookSlot8); }
