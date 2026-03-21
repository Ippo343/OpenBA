// engine/reimpl/huntanimal.cpp
// Hook wrappers for HuntAnimal action from Application-x64.exe.
//
// HuntAnimal handles the hunting behavior: finding a nearby animal,
// spawning a corpse entity at the kill location, and starting herd migration.
//
// Struct size: 0x40, vtable at 0x14015fde0
// Inherits: Object::Internal -> Action -> HuntAnimal
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c8260 -- ENABLED hook, reimplemented
//   Slot 4 (AbstractImpl): 0x1400c8250 -- unhookable (9 bytes)
//   Slot 5 (Execute):      0x1400c8240 -- unhookable (9 bytes)
//   Slot 6 (Enter):        0x1400c7de0 -- ENABLED hook, reimplemented
//   Slot 7 (Exit):         BASE (0x1400c4ba0)
//   Slot 8 (CheckEntity):  BASE (0x1400c4a30)
//   Slot 9 (GetTarget):    0x1400c7d60 -- no hook (never created), reimplemented in clean code

#include "HuntAnimal.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Action/huntanimal_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool HuntAnimal_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookEnter;

// ============================================================
// HuntAnimal::Serialize (0x1400c8260) -- reimplemented
// ============================================================
static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "HuntAnimal::Serialize";
    HuntAnimal_Serialize_Impl((HuntAnimal*)self, stream);
}

// ============================================================
// HuntAnimal::Enter (0x1400c7de0) -- reimplemented
// Complex hunting logic: finds nearby animal via grid query, validates
// terrain, spawns corpse, starts herd migration. ~250 instructions.
// ============================================================
static void Reimpl_Enter(void* self) {
    g_lastHook = "HuntAnimal::Enter";
    HuntAnimal_Enter_Impl((HuntAnimal*)self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallHuntAnimalSerialize() {
    if (!HuntAnimal_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[HuntAnimal] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8260);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[HuntAnimal] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveHuntAnimalSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallHuntAnimalEnter() {
    if (!HuntAnimal_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[HuntAnimal] ERROR: Failed to resolve helpers for Enter\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c7de0);
    // Prologue: PUSH(2)+PUSH(2)+LEA(5)+SUB(7) = 16 bytes
    InstallHook(target, (void*)Reimpl_Enter, &g_hookEnter, 16);
#ifdef _DEBUG
    printf("[HuntAnimal] Enter hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveHuntAnimalEnter() {
    RemoveHook(&g_hookEnter);
}
