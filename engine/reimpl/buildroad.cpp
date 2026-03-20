// engine/reimpl/buildroad.cpp
// Hook wrappers for BuildRoad Work action from Application-x64.exe.
//
// BuildRoad — road construction work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> BuildRoad
// Ctor: 0x1400e4ad0, Vtable: 0x140161020
//
// Vtable (12 slots):
//   [0]  +0x00  ScalarDtor    = 0x1400295b0   <-- reimplemented
//   [1]  +0x08  VectorDtor    = 0x140029550   (not hooked)
//   [2]  +0x10  BaseDtor      = 0x140029540   (returns TypeInfo, not hooked)
//   [3]  +0x18  Serialize     = 0x1400e4980   <-- reimplemented
//   [4]  +0x20  Execute       = 0x1400e4190   <-- REIMPLEMENTED (clean C++)
//   [5]  +0x28  Shared        = 0x1400f7540   (3 bytes, trivial return false, unhookable)
//   [6]  +0x30  OnComplete    = 0x1400e4150   <-- reimplemented
//   [7]  +0x38  OnCancel      = 0x1400e4120   <-- reimplemented
//   [8]  +0x40  Cleanup       = 0x1400e4910   <-- reimplemented
//   [9]  +0x48  Validate      = 0x1400e40c0   <-- reimplemented
//   [10] +0x50  GetTarget     = 0x1400e4960   (RIP-relative prologue, bug class 2, unhookable)
//   [11] +0x58  GetPriority   = 0x1400295a0   (10 bytes total, too small to hook)

#include "BuildActions.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/buildroad_funcs.h"
#include <cstdio>
#include <cstdint>

// Forward declaration for resolution
extern bool BuildRoad_ResolveFuncs();

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookCtor;
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookExecute;
static HookInfo g_hookOnComplete;
static HookInfo g_hookOnCancel;
static HookInfo g_hookCleanup;
static HookInfo g_hookValidate;

// ============================================================
// Constructor (0x1400e4ad0) — reimplemented
// ============================================================
static void* Reimpl_Ctor(BuildRoad* self) {
    g_lastHook = "BuildRoad::Ctor";
    return BuildRoad_Ctor_Impl(self);
}

// ============================================================
// ScalarDtor (0x1400295b0) — reimplemented
// ============================================================
static void* Reimpl_ScalarDtor(BuildRoad* self, uint32_t flags) {
    g_lastHook = "BuildRoad::ScalarDtor";
    return BuildRoad_ScalarDtor_Impl(self, flags);
}

// ============================================================
// Serialize (0x1400e4980) — reimplemented
// ============================================================
static void Reimpl_Serialize(BuildRoad* self, void* stream) {
    g_lastHook = "BuildRoad::Serialize";
    BuildRoad_Serialize_Impl(self, stream);
}

// ============================================================
// Execute (0x1400e4190) — REIMPLEMENTED (clean C++)
// Uses int return (bug class 7).
// ============================================================
static int Reimpl_Execute(BuildRoad* self, void* entityIBP) {
    g_lastHook = "BuildRoad::Execute";
    return BuildRoad_Execute(self, entityIBP);
}

// ============================================================
// OnComplete (0x1400e4150) — reimplemented
// ============================================================
static int Reimpl_OnComplete(BuildRoad* self, void* param2, void* param3, int param4) {
    g_lastHook = "BuildRoad::OnComplete";
    return BuildRoad_OnComplete_Impl(self, param2, param3, param4);
}

// ============================================================
// OnCancel (0x1400e4120) — reimplemented
// ============================================================
static int Reimpl_OnCancel(BuildRoad* self) {
    g_lastHook = "BuildRoad::OnCancel";
    return BuildRoad_OnCancel_Impl(self);
}

// ============================================================
// Cleanup (0x1400e4910) — reimplemented
// ============================================================
static int Reimpl_Cleanup(BuildRoad* self) {
    g_lastHook = "BuildRoad::Cleanup";
    return BuildRoad_Cleanup_Impl(self);
}

// ============================================================
// Validate (0x1400e40c0) — reimplemented
// ============================================================
static int Reimpl_Validate(BuildRoad* self, void* param2) {
    g_lastHook = "BuildRoad::Validate";
    return BuildRoad_Validate_Impl(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallBuildRoadCtor() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for Ctor\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[BuildRoad] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadCtor() { RemoveHook(&g_hookCtor); }

void InstallBuildRoadScalarDtor() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for ScalarDtor\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[BuildRoad] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallBuildRoadSerialize() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for Serialize\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[BuildRoad] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadSerialize() { RemoveHook(&g_hookSerialize); }

void InstallBuildRoadExecute() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Execute);
    // Prologue: PUSH RBP(1) + PUSH RSI(1) + PUSH R14(2) + LEA(4) + SUB(7) = 15
    // Worktree used 16, Stage 1 used 15. Use 16 to match worktree (safe boundary).
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 16);
#ifdef _DEBUG
    printf("[BuildRoad] Execute hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveBuildRoadExecute() { RemoveHook(&g_hookExecute); }

void InstallBuildRoadOnComplete() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for OnComplete\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::OnComplete);
    InstallHook(target, (void*)Reimpl_OnComplete, &g_hookOnComplete, 13);
#ifdef _DEBUG
    printf("[BuildRoad] OnComplete hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadOnComplete() { RemoveHook(&g_hookOnComplete); }

void InstallBuildRoadOnCancel() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for OnCancel\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::OnCancel);
    InstallHook(target, (void*)Reimpl_OnCancel, &g_hookOnCancel, 12);
#ifdef _DEBUG
    printf("[BuildRoad] OnCancel hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadOnCancel() { RemoveHook(&g_hookOnCancel); }

void InstallBuildRoadCleanup() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for Cleanup\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Cleanup);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[BuildRoad] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadCleanup() { RemoveHook(&g_hookCleanup); }

void InstallBuildRoadValidate() {
    if (!BuildRoad_ResolveFuncs()) {
#ifdef _DEBUG
        printf("[BuildRoad] ERROR: Failed to resolve helpers for Validate\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildRoadAddr::Validate);
    InstallHook(target, (void*)Reimpl_Validate, &g_hookValidate, 12);
#ifdef _DEBUG
    printf("[BuildRoad] Validate hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildRoadValidate() { RemoveHook(&g_hookValidate); }
