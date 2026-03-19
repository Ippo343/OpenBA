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
#include "Work/fishingwork_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration
extern bool FishingWork_ResolveFuncs();

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

// Serialize (0x1400ea910) — Work_Serialize + Serializer(v0) +
//   serialize count at +0x50, resize array at +0x48, loop serialize each
//   InternalBasePtr with IsDerivedFrom check, then serialize array at +0x58
//   with count at +0x60, then serialize int at +0x68
typedef void (*fn_Serialize_t)(void* self, void* stream);
static void Reimpl_Serialize(FishingWork* self, void* stream) {
    g_lastHook = "FishingWork::Serialize";
    FishingWork_Serialize_Impl(self, stream);
}

typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(FishingWork* self, void* param2, void* param3) {
    g_lastHook = "FishingWork::Slot4";
    return FishingWork_Slot4_Impl(self, param2, param3);
}

typedef int (*fn_Slot6_t)(void* self, void* param2, void* param3, uint32_t param4);
static int Reimpl_Slot6(FishingWork* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "FishingWork::Slot6";
    return FishingWork_Slot6_Impl(self, param2, param3, param4);
}

typedef void (*fn_Slot7_t)(void* self, void* param2, void* param3);
static void Reimpl_Slot7(FishingWork* self, void* param2, void* param3) {
    g_lastHook = "FishingWork::Slot7";
    FishingWork_Slot7_Impl(self, param2, param3);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(FishingWork* self) {
    g_lastHook = "FishingWork::Slot8";
    FishingWork_Slot8_Impl(self);
}

typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(FishingWork* self, void* param2) {
    g_lastHook = "FishingWork::Slot9";
    return FishingWork_Slot9_Impl(self, param2);
}

typedef void* (*fn_Slot10_t)(void* self, void* param2);
static void* Reimpl_Slot10(FishingWork* self, void* param2) {
    g_lastHook = "FishingWork::Slot10";
    return FishingWork_Slot10_Impl(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFishingWorkSerialize() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Serialize: ResolveFuncs failed, using trampoline\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea910);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[FishingWork] Serialize hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallFishingWorkSlot4() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Slot4: ResolveFuncs failed, using trampoline\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9f20);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[FishingWork] Slot4 hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallFishingWorkSlot6() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Slot6: ResolveFuncs failed\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9eb0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[FishingWork] Slot6 hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFishingWorkSlot7() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Slot7: ResolveFuncs failed\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9e80);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 12);
#ifdef _DEBUG
    printf("[FishingWork] Slot7 hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot7() { RemoveHook(&g_hookSlot7); }

void InstallFishingWorkSlot8() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Slot8: ResolveFuncs failed\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea830);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[FishingWork] Slot8 hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallFishingWorkSlot9() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Slot9: ResolveFuncs failed, using trampoline\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9c30);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 17);
#ifdef _DEBUG
    printf("[FishingWork] Slot9 hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot9() { RemoveHook(&g_hookSlot9); }

void InstallFishingWorkSlot10() {
    if (!FishingWork_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[FishingWork] Slot10: ResolveFuncs failed\n");
#endif
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea8b0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 17);
#ifdef _DEBUG
    printf("[FishingWork] Slot10 hooked at %p (reimplemented)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot10() { RemoveHook(&g_hookSlot10); }
