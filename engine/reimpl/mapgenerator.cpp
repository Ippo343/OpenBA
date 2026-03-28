// engine/reimpl/mapgenerator.cpp
// Reimplementation of MapGenerator vtable methods from Application-x64.exe.
//
// MapGenerator (0x178 bytes), vtable at 0x14015ed80.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x14001ae70 — Dtor(self, flags)
//   +0x08: 0x14001add0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x14001adc0 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400a3020 — Slot3/Serialize? (6b, unhookable: mov eax,8; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400a2980 — Slot5/Load
//   +0x30: 0x14001ae30 — Slot6/Cleanup
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x14001ae20 — Slot8/IsDerivedFrom

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "MapGenerator.h"
#include "Game/mapgenerator_funcs.h"
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
// Reimplementation wrappers — delegate to clean code
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "MapGenerator::Dtor";
    return MapGenerator_Dtor((MapGenerator*)self, flags);
}

static void Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "MapGenerator::Slot5";
    MapGenerator_Serialize((MapGenerator*)self, stream);
}

static void Reimpl_Slot6(void* self) {
    g_lastHook = "MapGenerator::Slot6";
    MapGenerator_Cleanup((MapGenerator*)self);
}

static void* Reimpl_Slot8(void* self) {
    g_lastHook = "MapGenerator::Slot8";
    return MapGenerator_Slot8((MapGenerator*)self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMapGeneratorDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001ae70);
    // Prologue: mov [rsp+8],rbx(5) + push rdi(2) + sub rsp,0x20(4) + mov ebx,edx(2) + mov rdi,rcx(3) = 13 (boundary at 13: e8)
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[MapGenerator] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveMapGeneratorDtor() { RemoveHook(&g_hookDtor); }

void InstallMapGeneratorSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400a2980);
    // Prologue: push rbp(2) + push rsi(1) + push rdi(1) + mov rbp,rsp(3) + sub rsp,0x40(4) + mov [rbp-0x20],-2(8) = 19 -> 15
    // Actually: 40 55(2) + 56(1) + 57(1) + 48 8b ec(3) + 48 83 ec 40(4) + 48 c7 45 e0 fe ff ff ff(8) = 19 -> boundary at 15 (after sub)
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 19);
#ifdef _DEBUG
    printf("[MapGenerator] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveMapGeneratorSlot5() { RemoveHook(&g_hookSlot5); }

void InstallMapGeneratorSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001ae30);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[MapGenerator] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveMapGeneratorSlot6() { RemoveHook(&g_hookSlot6); }

void InstallMapGeneratorSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14001ae20);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[MapGenerator] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveMapGeneratorSlot8() { RemoveHook(&g_hookSlot8); }
