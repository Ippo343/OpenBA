// engine/reimpl/cleararea.cpp
// Hook wrappers for ClearArea Work action from Application-x64.exe.
//
// ClearArea — area clearing work action. Size = 0x78.
// Inherits: Object::Internal -> Work (0x48) -> ClearArea
// Ctor: 0x1400e68d0, Vtable: 0x140161110
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x140029870   <-- reimplemented
//   [1]  VectorDtor    = 0x140029810
//   [2]  BaseDtor      = 0x140029800
//   [3]  Serialize     = 0x1400e6740   <-- reimplemented
//   [4]  Execute       = 0x1400e5ee0   <-- REIMPLEMENTED (clean C++)
//   [5]  Shared        = 0x1400f7540   <-- trivial return false, not hooked
//   [6]  OnComplete    = 0x1400e5e30   <-- reimplemented
//   [7]  OnCancel      = 0x1400e5e00   <-- too small to hook, not hooked
//   [8]  Cleanup       = 0x1400e6650   <-- reimplemented
//   [9]  Validate      = 0x1400e5b10   <-- bug class 3 (RSP capture), not hookable
//   [10] GetTarget     = 0x1400e6720   <-- reimplemented
//   [11] GetPriority   = 0x140029860   <-- too small to hook (9 bytes total)

#include "BuildActions.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/cleararea_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookCtor;
static HookInfo g_hookScalarDtor;
static HookInfo g_hookSerialize;
static HookInfo g_hookExecute;
static HookInfo g_hookOnComplete;
static HookInfo g_hookCleanup;
static HookInfo g_hookGetTarget;

// ============================================================
// Constructor (0x1400e68d0) — delegates to clean code
// ============================================================
static void* Reimpl_Ctor(ClearArea* self) {
    g_lastHook = "ClearArea::Ctor";
    return ClearArea_Ctor(self);
}

// ============================================================
// ScalarDtor (0x140029870) — delegates to clean code
// ============================================================
static void* Reimpl_ScalarDtor(ClearArea* self, uint32_t flags) {
    g_lastHook = "ClearArea::ScalarDtor";
    return ClearArea_ScalarDtor(self, flags);
}

// ============================================================
// Serialize (0x1400e6740) — delegates to clean code
// ============================================================
static void Reimpl_Serialize(ClearArea* self, void* stream) {
    g_lastHook = "ClearArea::Serialize";
    ClearArea_Serialize(self, stream);
}

// ============================================================
// Execute (0x1400e5ee0) — REIMPLEMENTED (clean C++)
// Signature: int Execute(ClearArea* self, void* actionStack, void* time)
// Uses int return type (bug class 7).
// ============================================================
static int Reimpl_Execute(ClearArea* self, void* actionStack, void* time) {
    g_lastHook = "ClearArea::Execute";
    return ClearArea_Execute(self, actionStack, time);
}

// ============================================================
// OnComplete (0x1400e5e30) — delegates to clean code
// Params: self(RCX), citizen(RDX), treeEntry(R8), cellIndex(R9D)
// ============================================================
static int Reimpl_OnComplete(ClearArea* self, void* citizen, void* treeEntry, int cellIndex) {
    g_lastHook = "ClearArea::OnComplete";
    return ClearArea_OnComplete(self, citizen, treeEntry, cellIndex);
}

// ============================================================
// Cleanup (0x1400e6650) — delegates to clean code
// ============================================================
static int Reimpl_Cleanup(ClearArea* self) {
    g_lastHook = "ClearArea::Cleanup";
    return ClearArea_Cleanup(self);
}

// ============================================================
// GetTarget (0x1400e6720) — delegates to clean code
// Non-trivial return ABI: RCX=self, RDX=retbuf (member function)
// ============================================================
static void* Reimpl_GetTarget(ClearArea* self, void* retbuf) {
    g_lastHook = "ClearArea::GetTarget";
    return ClearArea_GetTarget(self, retbuf);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallClearAreaCtor() {
    uintptr_t target = ResolveAddr(Module::Exe, ClearAreaAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[ClearArea] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaCtor() { RemoveHook(&g_hookCtor); }

void InstallClearAreaScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, ClearAreaAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[ClearArea] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallClearAreaSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, ClearAreaAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ClearArea] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaSerialize() { RemoveHook(&g_hookSerialize); }

void InstallClearAreaExecute() {
    // Prologue: 5+5+1+1+1+1 = 14 bytes to PUSH R12 boundary
    // First two instructions are RSP-relative stores (bug class 4), safe for reimpl
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e5ee0);
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 14);
#ifdef _DEBUG
    printf("[ClearArea] Execute hooked at %p (reimplemented)\n", (void*)target);
#endif
}
void RemoveClearAreaExecute() { RemoveHook(&g_hookExecute); }

void InstallClearAreaOnComplete() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e5e30);
    InstallHook(target, (void*)Reimpl_OnComplete, &g_hookOnComplete, 14);
#ifdef _DEBUG
    printf("[ClearArea] OnComplete hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaOnComplete() { RemoveHook(&g_hookOnComplete); }

void InstallClearAreaCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e6650);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 14);
#ifdef _DEBUG
    printf("[ClearArea] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaCleanup() { RemoveHook(&g_hookCleanup); }

void InstallClearAreaGetTarget() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e6720);
    InstallHook(target, (void*)Reimpl_GetTarget, &g_hookGetTarget, 14);
#ifdef _DEBUG
    printf("[ClearArea] GetTarget hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaGetTarget() { RemoveHook(&g_hookGetTarget); }
