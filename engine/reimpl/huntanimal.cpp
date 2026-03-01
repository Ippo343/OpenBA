// engine/reimpl/huntanimal.cpp
// Reimplementation of HuntAnimal action from Application-x64.exe.
//
// HuntAnimal handles the hunting behavior: finding a nearby animal,
// spawning a corpse entity at the kill location, and starting herd migration.
// The Enter method (0x1400c7de0) is extremely complex (250 instructions) with
// grid math, entity queries, and corpse placement -- passthrough only.
//
// Struct size: 0x40, vtable at 0x14015fde0
// Inherits: Object::Internal -> Action -> HuntAnimal
//
// Vtable overrides vs Action base:
//   Slot 3 (Serialize):    0x1400c8260 (override) -- PASSTHROUGH
//   Slot 4 (AbstractImpl): 0x1400c8250 (tiny, possibly 1-2 bytes)
//   Slot 5 (Execute):      0x1400c8240 (tiny)
//   Slot 6 (Enter):        0x1400c7de0 (override) -- PASSTHROUGH (250 instr, grid/entity logic)
//   Slot 7 (Exit):         BASE
//   Slot 8 (CheckEntity):  BASE
//   Slot 9 (GetTarget):    0x1400c7d60 (override) -- checks herd->entity and animal->entity

#include "HuntAnimal.h"
#include "hooks.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookSerialize;
static HookInfo g_hookEnter;

// ============================================================
// Original function typedefs
// ============================================================

typedef void (*HuntAnimal_Serialize_t)(void* self, void* stream);
typedef void (*HuntAnimal_Enter_t)(void* self);

// ============================================================
// HuntAnimal::Serialize -- passthrough wrapper
// Serializes status, animal entity (+0x20), herd entity (+0x28),
// hunt started flag (+0x38)
// ============================================================

static void Reimpl_Serialize(void* self, void* stream) {
    g_lastHook = "HuntAnimal::Serialize";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[HuntAnimal::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((HuntAnimal_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

// ============================================================
// HuntAnimal::Enter -- passthrough wrapper
// Complex hunting logic: finds nearby animal via grid query, validates
// terrain, spawns corpse, starts herd migration. 250 instructions.
// ============================================================

static void Reimpl_Enter(void* self) {
    g_lastHook = "HuntAnimal::Enter";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        uint8_t* act = (uint8_t*)self;
        uint8_t started = *(uint8_t*)(act + 0x38);
        printf("[HuntAnimal::Enter] self=%p huntStarted=%d\n", self, started);
        callCount++;
    }
#endif
    ((HuntAnimal_Enter_t)g_hookEnter.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallHuntAnimalSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c8260);
    // Prologue: PUSH(2)+SUB(4)+MOV(9)+MOV(5) = 20 bytes
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 20);
#ifdef _DEBUG
    printf("[HuntAnimal] Serialize hooked at %p (prologue=20, passthrough)\n", (void*)target);
#endif
}

void RemoveHuntAnimalSerialize() {
    RemoveHook(&g_hookSerialize);
}

void InstallHuntAnimalEnter() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c7de0);
    // Prologue: PUSH(2)+PUSH(2)+LEA(5)+SUB(7) = 16 bytes
    InstallHook(target, (void*)Reimpl_Enter, &g_hookEnter, 16);
#ifdef _DEBUG
    printf("[HuntAnimal] Enter hooked at %p (prologue=16, passthrough)\n", (void*)target);
#endif
}

void RemoveHuntAnimalEnter() {
    RemoveHook(&g_hookEnter);
}
