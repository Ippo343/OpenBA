// engine/reimpl/menuscene.cpp
// Reimplementation of MenuScene vtable methods from Application-x64.exe.
//
// MenuScene (0x208 bytes), vtable at 0x14015ede0.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x14001b830 — Dtor(self, flags)
//   +0x08: 0x14001b7b0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x14001b7a0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400a3960 — Slot3/Serialize? (6b, unhookable: mov eax,1; ret)
//   +0x20: 0x1400a3620 — Slot4 (6b, unhookable: mov eax,1; ret)
//   +0x28: 0x1400a3630 — Slot5/Load
//   +0x30: 0x14001b810 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x14001b800 — Slot8/IsDerivedFrom

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
void InstallMenuSceneDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001b830);
    // Prologue: mov [rsp+8],rbx(5) + push rdi(2) + sub rsp,0x20(4) + mov ebx,edx(2) + mov rdi,rcx(3) = 13
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 13);
#ifdef _DEBUG
    printf("[MenuScene] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveMenuSceneDtor() { RemoveHook(&g_hookDtor); }

void InstallMenuSceneSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400a3630);
    // Prologue: push rbp(2) + mov rbp,rsp(3) + sub rsp,0x40(4) + mov [rbp-0x20],-2(8) = 17
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 17);
#ifdef _DEBUG
    printf("[MenuScene] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveMenuSceneSlot5() { RemoveHook(&g_hookSlot5); }

void InstallMenuSceneSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001b810);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[MenuScene] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveMenuSceneSlot6() { RemoveHook(&g_hookSlot6); }

void InstallMenuSceneSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001b800);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[MenuScene] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveMenuSceneSlot8() { RemoveHook(&g_hookSlot8); }
