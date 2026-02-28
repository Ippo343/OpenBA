// engine/reimpl/statusiconcomponent.cpp
// Reimplementation of StatusIconComponent lifecycle methods from Application-x64.exe.
//
// StatusIconComponent (0x108 bytes) displays floating status icons above entities
// (e.g., hunger, cold, disease). Embeds a Graphics::Geometry sub-object at +0x28
// (0xC0 bytes, with overridden vtable at 0x14015f0c0).
//
// Vtable at 0x140164620. Non-trivial overrides:
//   Shutdown  (0x14012bff0) — Serializes iconId field (+0xF0 as uint, then fieldF0)
//   Serialize (0x14012bed0) — Reads sprite data from building, calls SpriteSheet::GetSpriteCount
//   OnCreate  (0x14012beb0) — Not a defined function in Ghidra, likely tiny (skip)
//   OnRemove  (0x14012bbd0) — Massive (160 instr): manages Scene add/remove, positions icon
//   OnUpdate2 (0x14012bf40) — Scene::AddObject + flags + vtable dispatches
//
// Strategy: Passthrough wrappers. All methods are deeply coupled to Graphics::Scene,
// Geometry, SpriteSheet, and entity positioning subsystems.
//
// Prologue sizes:
//   Shutdown:  15 (PUSH RDI + SUB RSP,0x40 + MOV [RSP+0x20],-0x2)
//   Serialize: 12 (MOV [RSP+8],RBX + MOV [RSP+0x10],RSI + PUSH RDI)
//   OnRemove:  15 (PUSH RBP + PUSH RDI + LEA RBP,[RSP-0x4F] + SUB RSP,0x98)
//   OnUpdate2: 14 (MOV [RSP+0x10],RBX + PUSH RDI + SUB RSP,0x20 + MOV RAX,[RCX])

#include "StatusIconComponent.h"
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
static HookInfo g_hookOnRemove;
static HookInfo g_hookOnUpdate2;

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

static void Reimpl_OnRemove(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnRemove.trampoline)(self);
}

static void Reimpl_OnUpdate2(void* self) {
    typedef void (*Orig_t)(void*);
    ((Orig_t)g_hookOnUpdate2.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStatusIconComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bff0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[StatusIconComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStatusIconComponentSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bed0);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 12);
#ifdef _DEBUG
    printf("[StatusIconComponent] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentSerialize() { RemoveHook(&g_hookSerialize); }

void InstallStatusIconComponentOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bbd0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 15);
#ifdef _DEBUG
    printf("[StatusIconComponent] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentOnRemove() { RemoveHook(&g_hookOnRemove); }

void InstallStatusIconComponentOnUpdate2() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012bf40);
    InstallHook(target, (void*)Reimpl_OnUpdate2, &g_hookOnUpdate2, 14);
#ifdef _DEBUG
    printf("[StatusIconComponent] OnUpdate2 hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusIconComponentOnUpdate2() { RemoveHook(&g_hookOnUpdate2); }
