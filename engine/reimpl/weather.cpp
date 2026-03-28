// engine/reimpl/weather.cpp
// Reimplementation of Weather vtable methods from Application-x64.exe.
//
// Weather (0x80 bytes), vtable at 0x140160290.
// Extends Object::External.
//
// Vtable layout (9 slots):
//   +0x00: 0x140025020 — Dtor(self, flags)              — REIMPLEMENTED
//   +0x08: 0x140024fa0 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140024f90 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400d3e40 — Slot3/Serialize? (6b, unhookable: mov eax,5; ret)
//   +0x20: 0x14003a912 — Slot4 base (6b JMP thunk, unhookable)
//   +0x28: 0x1400d3c90 — Slot5/Serialize                — REIMPLEMENTED
//   +0x30: 0x140025000 — Slot6/Cleanup                  — REIMPLEMENTED
//   +0x38: 0x140001640 — Slot7 base (7b, unhookable)
//   +0x40: 0x140024ff0 — Slot8/Reset (DISABLED: near JCC in prologue)

#include "Weather.h"
#include "Game/weather_funcs.h"
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
// Reimplemented wrappers
// ============================================================

// Dtor (0x140025020) — REIMPLEMENTED
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "Weather::Dtor";
    return Weather_Dtor((Weather*)self, flags);
}

// Slot5/Serialize (0x1400d3c90) — REIMPLEMENTED
static void Reimpl_Slot5(void* self, void* stream) {
    g_lastHook = "Weather::Slot5";
    Weather_Serialize((Weather*)self, stream);
}

// Slot6/Cleanup (0x140025000) — REIMPLEMENTED
static void Reimpl_Slot6(void* self) {
    g_lastHook = "Weather::Slot6";
    Weather_Cleanup((Weather*)self);
}

// Slot8/Reset (0x140024ff0) — hook DISABLED (near JCC in prologue)
static void Reimpl_Slot8(void* self) {
    g_lastHook = "Weather::Slot8";
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWeatherDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140025020);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[Weather] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveWeatherDtor() { RemoveHook(&g_hookDtor); }

void InstallWeatherSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d3c90);
    // Prologue: push rbx(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 15);
#ifdef _DEBUG
    printf("[Weather] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveWeatherSlot5() { RemoveHook(&g_hookSlot5); }

void InstallWeatherSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140025000);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + mov rax,[rcx](3) + xor edx,edx(2) + mov rbx,rcx(3) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[Weather] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveWeatherSlot6() { RemoveHook(&g_hookSlot6); }

void InstallWeatherSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140024ff0);
    // Prologue: test rcx,rcx(3) + jnz(6) + xor eax,eax(2) + ret(1) = 12
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 12);
#ifdef _DEBUG
    printf("[Weather] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveWeatherSlot8() { RemoveHook(&g_hookSlot8); }
