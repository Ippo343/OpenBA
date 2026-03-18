// engine/reimpl/fishingwork.cpp
// Reimplementation of FishingWork vtable methods from Application-x64.exe.
//
// FishingWork is a Work subclass (0x70 bytes) for fishing dock operations.
// Has an InternalBasePtr array at +0x48, int array at +0x58, and
// a retry counter at +0x68.
//
// Vtable at 0x140161320, 12 slots:
//   +0x00: ScalarDtor (0x14002a2c0)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a270)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a260)   — tiny, not hooked
//   +0x18: Serialize  (0x1400ea910)   — HOOKED (deep reimpl)
//   +0x20: Slot4      (0x1400e9f20)   — HOOKED (passthrough, complex fishing logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400e9eb0)   — HOOKED (deep reimpl, OnComplete)
//   +0x38: Slot7      (0x1400e9e80)   — HOOKED (deep reimpl, OnCancel)
//   +0x40: Slot8      (0x1400ea830)   — HOOKED (deep reimpl, Cleanup)
//   +0x48: Slot9      (0x1400e9c30)   — HOOKED (passthrough, Validate)
//   +0x50: Slot10     (0x1400ea8b0)   — HOOKED (deep reimpl, GetTarget)
//   +0x58: Slot11     (0x140029860)   — 9 bytes, unhookable

#include "FishingWork.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/fishingwork_funcs.h"
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
// Serialize (0x1400ea910) — deeply reimplemented
// Work_Serialize + Serializer(v0) + IBP array + int array + int
// ============================================================
static void Reimpl_Serialize(FishingWork* self, void* stream) {
    g_lastHook = "FishingWork::Serialize";
    FishingWork_Serialize_Impl(self, stream);
}

// ============================================================
// Slot4 / Execute (0x1400e9f20) — passthrough (too complex, many unresolved deps)
// Main fishing work assignment logic with map grid, random spot picking, etc.
// ============================================================
typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    g_lastHook = "FishingWork::Slot4";
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

// ============================================================
// Slot6 / OnComplete (0x1400e9eb0) — deeply reimplemented
// Clears citizen ref, optionally marks spot as done.
// ============================================================
static int Reimpl_Slot6(FishingWork* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "FishingWork::OnComplete";
    return FishingWork_OnComplete_Impl(self, param2, param3, param4);
}

// ============================================================
// Slot7 / OnCancel (0x1400e9e80) — deeply reimplemented
// Delegates to OnComplete via vtable.
// NOTE: Must declare 4 params to avoid clobbering R9 (bug class 6).
// ============================================================
static void Reimpl_Slot7(FishingWork* self, void* param2, void* param3, uint32_t param4) {
    g_lastHook = "FishingWork::OnCancel";
    FishingWork_OnCancel_Impl(self, param2, param3, param4);
}

// ============================================================
// Slot8 / Cleanup (0x1400ea830) — deeply reimplemented
// Cleans up citizen refs' task lists.
// ============================================================
static void Reimpl_Slot8(FishingWork* self) {
    g_lastHook = "FishingWork::Cleanup";
    FishingWork_Cleanup_Impl(self);
}

// ============================================================
// Slot9 / Validate (0x1400e9c30) — passthrough (complex validation logic)
// ============================================================
typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "FishingWork::Validate";
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

// ============================================================
// Slot10 / GetTarget (0x1400ea8b0) — deeply reimplemented
// Returns workplace entity's target via hidden return pointer.
// ============================================================
static void* Reimpl_Slot10(FishingWork* self, void* outPtr) {
    g_lastHook = "FishingWork::GetTarget";
    return FishingWork_GetTarget_Impl(self, outPtr);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFishingWorkSerialize() {
    FishingWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea910);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[FishingWork] Serialize hooked at %p (deep reimpl)\n", (void*)target);
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
    FishingWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9eb0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[FishingWork] Slot6/OnComplete hooked at %p (deep reimpl)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFishingWorkSlot7() {
    FishingWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9e80);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 12);
#ifdef _DEBUG
    printf("[FishingWork] Slot7/OnCancel hooked at %p (deep reimpl)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot7() { RemoveHook(&g_hookSlot7); }

void InstallFishingWorkSlot8() {
    FishingWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea830);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[FishingWork] Slot8/Cleanup hooked at %p (deep reimpl)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallFishingWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9c30);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 17);
#ifdef _DEBUG
    printf("[FishingWork] Slot9/Validate hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot9() { RemoveHook(&g_hookSlot9); }

void InstallFishingWorkSlot10() {
    FishingWork_ResolveFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ea8b0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 17);
#ifdef _DEBUG
    printf("[FishingWork] Slot10/GetTarget hooked at %p (deep reimpl)\n", (void*)target);
#endif
}

void RemoveFishingWorkSlot10() { RemoveHook(&g_hookSlot10); }
