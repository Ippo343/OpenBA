// engine/reimpl/decalcomponent.cpp
// Reimplementation of DecalComponent lifecycle methods from Application-x64.exe.
//
// DecalComponent (0x38 bytes) manages ground decals for buildings (shadows, footprints).
//
// Vtable at 0x140162fd0. Non-trivial overrides:
//   Shutdown  (0x14011b820) — Serializes decalScale (+0x30) and decalState (+0x28)
//   Serialize (0x14011b7d0) — Reads building data, copies scale from data+0x20
//   Draw      (0x14011b560) — Complex: terrain decal rendering (111 instructions)
//   OnCreate  (0x14011b510) — Removes decal from terrain if decalId >= 0
//   OnUpdate2 (0x14011b810) — Not a defined function in Ghidra; likely tiny thunk
//
// Strategy: All passthrough. Draw is a massive renderer function (111 instructions),
// Serialize/OnCreate interact with terrain rendering subsystem.
//
// Prologue sizes:
//   Shutdown: 15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Serialize: 14 (MOV [RSP+8],RBX + PUSH RDI + SUB RSP,0x20 + MOV RBX,[RCX+0x18])
//   Draw: 13 (MOV RAX,RSP + MOV [RAX+0x20],RBX + PUSH RBP + PUSH RSI + PUSH RDI)
//   OnCreate: 13 (PUSH RBX + SUB RSP,0x20 + MOV EDX,[RCX+0x2C] + MOV RBX,RCX)

#include "DecalComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookSerialize;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Passthrough wrappers
// ============================================================
static void Reimpl_Shutdown(void* self, void* stream) {
    typedef void (*Orig_t)(void*, void*);
    ((Orig_t)g_hookShutdown.trampoline)(self, stream);
}

static void Reimpl_Serialize(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookSerialize.trampoline)(self);
}

static void Reimpl_Draw(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookDraw.trampoline)(self);
}

static void Reimpl_OnCreate(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnCreate.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDecalComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011b820);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[DecalComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveDecalComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallDecalComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011b7d0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 14);
#ifdef _DEBUG
    printf("[DecalComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveDecalComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallDecalComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011b560);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[DecalComponent] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveDecalComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallDecalComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14011b510);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[DecalComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveDecalComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
