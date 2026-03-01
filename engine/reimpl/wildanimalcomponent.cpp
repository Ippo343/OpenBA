// engine/reimpl/wildanimalcomponent.cpp
// Reimplementation of WildAnimalComponent lifecycle methods.
//
// WildAnimalComponent (0x30 bytes) — simple Component subclass.
// Holds a single entity reference at +0x28 (the wild animal entity).
//
// Vtable at 0x140164af0:
//   [3] Shutdown = 0x14012e300 (passthrough — serialization with InternalBasePtr + IsDerivedFrom)
//   [7] OnCreate = 0x14012e2b0 (passthrough — registers self with animal's component list)
//   All other slots are base no-ops.

#include "WildAnimalComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Shutdown (0x14012e300) — passthrough
// Serializes Component base + InternalBasePtr at +0x28.
// After deserialization, validates with IsDerivedFrom(DAT_1401c7820)
// and nulls out if type check fails (save compatibility).
// ============================================================
typedef void (*WildAnimalShutdown_t)(void* self, void* stream);
static HookInfo g_hookShutdown;

static void Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "WildAnimalComponent::Shutdown";
    ((WildAnimalShutdown_t)g_hookShutdown.trampoline)(self, stream);
}

void InstallWildAnimalComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e300);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[WildAnimalComponent] Shutdown hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveWildAnimalComponentShutdown() { RemoveHook(&g_hookShutdown); }

// ============================================================
// OnCreate (0x14012e2b0) — passthrough
// If data28 (entity ref at +0x28) is non-null:
//   1. Creates InternalBasePtr from owner (self)
//   2. CheckPtr on +0x28 entity
//   3. Adds owner to entity's component list at +0x38 via FUN_140002cc0
// Registers this component's owner with the referenced wild animal entity.
// ============================================================
typedef void (*WildAnimalOnCreate_t)(void* self);
static HookInfo g_hookOnCreate;

static void Reimpl_OnCreate(void* self) {
    g_lastHook = "WildAnimalComponent::OnCreate";
    ((WildAnimalOnCreate_t)g_hookOnCreate.trampoline)(self);
}

void InstallWildAnimalComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e2b0);
    // Prologue: PUSH RBX(1) + SUB RSP,0x20(4) + CMP [RCX+0x28],0(5) + LEA RBX,[RCX+0x28](4) = 14
    // 0x14012e2bf - 0x14012e2b0 = 0xF = 15
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[WildAnimalComponent] OnCreate hooked at %p (passthrough, prologue=15)\n", (void*)target);
#endif
}
void RemoveWildAnimalComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
