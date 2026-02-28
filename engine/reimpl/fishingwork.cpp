// engine/reimpl/fishingwork.cpp
// Reimplementation of FishingWork vtable methods from Application-x64.exe.
//
// FishingWork is a Work subclass (0x70 bytes) for fishing dock operations.
// Has an InternalBasePtr array at +0x48, ptrs at +0x50/+0x58/+0x60, and
// a count at +0x68. More complex serialization than other Work types.
//
// Vtable at 0x140161320, 12 slots:
//   +0x00: ScalarDtor (0x14002a2c0)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a270)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a260)   — tiny, not hooked
//   +0x18: Serialize  (0x1400ea910)   — HOOKED (passthrough, complex array serialization)
//   +0x20: Slot4      (0x1400e9f20)   — HOOKED (passthrough, complex fishing logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400e9eb0)   — HOOKED (passthrough)
//   +0x38: Slot7      (0x1400e9e80)   — HOOKED (passthrough)
//   +0x40: Slot8      (0x1400ea830)   — HOOKED (passthrough, cleanup)
//   +0x48: Slot9      (0x1400e9c30)   — HOOKED (passthrough, validates)
//   +0x50: Slot10     (0x1400ea8b0)   — HOOKED (passthrough)
//   +0x58: Slot11     (0x140029860)   — 9 bytes, unhookable

#include "FishingWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookSerialize;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// Passthrough reimplementations
// All methods are complex; Serialize involves array/loop serialization
// with IsDerivedFrom validation per element. Full reimpl deferred.
// ============================================================

// Serialize (0x1400ea910) — complex: Work_Serialize + Serializer(v0) +
//   serialize count at +0x50, resize array at +0x48, loop serialize each
//   InternalBasePtr with IsDerivedFrom check, then serialize array at +0x58
//   with count at +0x60, then serialize int at +0x68
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(void* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[FishingWork::Serialize] self=%p stream=%p\n", self, stream);
        callCount++;
    }
#endif
    ((fn_Serialize_t)g_hookSerialize.trampoline)(self, stream);
}

typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

typedef uint64_t (*fn_Slot6_t)(void* self, void* param2);
static uint64_t Reimpl_Slot6(void* self, void* param2) {
    return ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

typedef void* (*fn_Slot10_t)(void* self, void* param2);
static void* Reimpl_Slot10(void* self, void* param2) {
    return ((fn_Slot10_t)g_hookSlot10.trampoline)(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFishingWorkSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea910);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[FishingWork] Serialize hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallFishingWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9f20);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[FishingWork] Slot4 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallFishingWorkSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9eb0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 12);
#ifdef _DEBUG
    printf("[FishingWork] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFishingWorkSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9e80);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 12);
#ifdef _DEBUG
    printf("[FishingWork] Slot7 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot7() { RemoveHook(&g_hookSlot7); }

void InstallFishingWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea830);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[FishingWork] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallFishingWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9c30);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[FishingWork] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot9() { RemoveHook(&g_hookSlot9); }

void InstallFishingWorkSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea8b0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[FishingWork] Slot10 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot10() { RemoveHook(&g_hookSlot10); }
