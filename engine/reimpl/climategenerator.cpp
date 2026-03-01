// engine/reimpl/climategenerator.cpp
// Reimplementation of ClimateGenerator vtable methods from Application-x64.exe.
//
// ClimateGenerator (0x110 bytes), vtable at 0x14015e140.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x140018f60 — Dtor(self, flags)
//   +0x08: 0x140018ee0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140018ed0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400930b0 — Slot3/Serialize? (6b, unhookable: mov eax,3; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x140092fa0 — Slot5/Load
//   +0x30: 0x140018f40 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x140018f30 — Slot8/IsDerivedFrom

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
typedef void  (*fn_Slot5_t)(void* self, void* stream);
typedef void  (*fn_Slot6_t)(void* self);
typedef void  (*fn_Slot8_t)(void* self);

// ============================================================
// Passthrough wrappers
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

static void Reimpl_Slot5(void* self, void* stream) {
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self, stream);
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
void InstallClimateGeneratorDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140018f60);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[ClimateGenerator] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveClimateGeneratorDtor() { RemoveHook(&g_hookDtor); }

void InstallClimateGeneratorSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140092fa0);
    // Prologue: push rbx(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[ClimateGenerator] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveClimateGeneratorSlot5() { RemoveHook(&g_hookSlot5); }

void InstallClimateGeneratorSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140018f40);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[ClimateGenerator] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveClimateGeneratorSlot6() { RemoveHook(&g_hookSlot6); }

void InstallClimateGeneratorSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140018f30);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[ClimateGenerator] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveClimateGeneratorSlot8() { RemoveHook(&g_hookSlot8); }
