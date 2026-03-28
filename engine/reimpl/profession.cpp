// engine/reimpl/profession.cpp
// Reimplementation of Profession vtable methods from Application-x64.exe.
//
// Profession (0x60 bytes), vtable at 0x14015efa0.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x14001e6f0 — Dtor(self, flags)              — REIMPLEMENTED
//   +0x08: 0x14001e520 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x14001e510 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400a9020 — Slot3/Serialize? (6b, unhookable: mov eax,3; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400a8f10 — Slot5/Serialize                 — REIMPLEMENTED
//   +0x30: 0x14001e580 — Slot6/Cleanup                   — REIMPLEMENTED
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x14001e570 — Slot8/Reset                     — REIMPLEMENTED

#include "Profession.h"
#include "Game/profession_funcs.h"
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
// Original function typedefs (kept for reference / fallback)
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
typedef void  (*fn_Slot5_t)(void* self, void* stream);
typedef void  (*fn_Slot6_t)(void* self);
typedef void  (*fn_Slot8_t)(void* self);

// ============================================================
// Reimplemented wrappers — delegate to clean code
// ============================================================

// Dtor (0x14001e6f0) — REIMPLEMENTED
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "Profession::Dtor";
    return Profession_Dtor((Profession*)self, flags);
}

// Slot5/Serialize (0x1400a8f10) — REIMPLEMENTED
static void Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "Profession::Slot5";
    Profession_Serialize((Profession*)self, stream);
}

// Slot6/Cleanup (0x14001e580) — REIMPLEMENTED
static void Reimpl_Slot6(void* self) {
    g_lastHook = "Profession::Slot6";
    Profession_Cleanup((Profession*)self);
}

// Slot8/Reset (0x14001e570) — REIMPLEMENTED
static void Reimpl_Slot8(void* self) {
    g_lastHook = "Profession::Slot8";
    Profession_Slot8((Profession*)self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallProfessionDtor() {
    Profession_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001e6f0);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[Profession] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionDtor() { RemoveHook(&g_hookDtor); }

void InstallProfessionSlot5() {
    Profession_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400a8f10);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[Profession] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionSlot5() { RemoveHook(&g_hookSlot5); }

void InstallProfessionSlot6() {
    Profession_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001e580);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[Profession] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionSlot6() { RemoveHook(&g_hookSlot6); }

void InstallProfessionSlot8() {
    Profession_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001e570);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[Profession] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveProfessionSlot8() { RemoveHook(&g_hookSlot8); }
