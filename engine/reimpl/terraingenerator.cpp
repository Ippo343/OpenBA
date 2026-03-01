// engine/reimpl/terraingenerator.cpp
// Reimplementation of TerrainGenerator vtable methods from Application-x64.exe.
//
// TerrainGenerator (0x90 bytes), vtable at 0x14015f170.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x140020600 — Dtor(self, flags)
//   +0x08: 0x140020580 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140020570 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400b1800 — Slot3/Serialize? (6b, unhookable: mov eax,4; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400b1690 — Slot5/Load
//   +0x30: 0x1400205e0 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x1400205d0 — Slot8/IsDerivedFrom

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
void InstallTerrainGeneratorDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020600);
    // Prologue: mov [rsp+8],rbx(5) + push rdi(2) + sub rsp,0x20(4) + mov ebx,edx(2) + mov rdi,rcx(3) = 13
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 13);
#ifdef _DEBUG
    printf("[TerrainGenerator] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTerrainGeneratorDtor() { RemoveHook(&g_hookDtor); }

void InstallTerrainGeneratorSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400b1690);
    // Prologue: push rbp(2) + mov rbp,rsp(3) + sub rsp,0x40(4) + mov [rbp-0x20],-2(8) = 17
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 17);
#ifdef _DEBUG
    printf("[TerrainGenerator] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveTerrainGeneratorSlot5() { RemoveHook(&g_hookSlot5); }

void InstallTerrainGeneratorSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400205e0);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[TerrainGenerator] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveTerrainGeneratorSlot6() { RemoveHook(&g_hookSlot6); }

void InstallTerrainGeneratorSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400205d0);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[TerrainGenerator] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveTerrainGeneratorSlot8() { RemoveHook(&g_hookSlot8); }
