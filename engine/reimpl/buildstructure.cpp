// engine/reimpl/buildstructure.cpp
// Reimplementation of BuildStructure Work action from Application-x64.exe.
//
// BuildStructure — structure construction work action. Size = 0x70.
// Inherits: Object::Internal -> Work (0x48) -> BuildStructure
// Ctor: 0x1400e5a90, Vtable: 0x1401610a0
//
// Vtable (12 slots):
//   [0]  ScalarDtor    = 0x140029680   <-- REIMPLEMENTED (clean C++)
//   [1]  VectorDtor    = 0x140029620
//   [2]  BaseDtor      = 0x140029610
//   [3]  Serialize     = 0x1400e5850   <-- REIMPLEMENTED (clean C++)
//   [4]  Execute       = 0x1400e4fc0   <-- REIMPLEMENTED (clean C++)
//   [5]  Shared        = 0x1400f7540
//   [6]  OnComplete    = 0x1400e4f80   <-- passthrough
//   [7]  OnCancel      = 0x1400e4f40   <-- passthrough
//   [8]  Cleanup       = 0x1400e57a0   <-- passthrough (tree traversal)
//   [9]  Validate      = 0x1400e4be0   <-- passthrough (very complex)
//   [10] GetTarget     = 0x1400e5830
//   [11] GetPriority   = 0x140029670
//
// Constructor reimplemented. Serialize and Execute call clean code in
// code/Game/Work/buildstructure.cpp.

#include "BuildActions.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Work/buildstructure_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================
typedef void* (*fn_ObjectInternal_Ctor_t)(void* self);
static fn_ObjectInternal_Ctor_t fn_ObjectInternal_Ctor = nullptr;

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

// ============================================================
// Constructor (0x1400e5a90)
// ============================================================
static void* Reimpl_Ctor(BuildStructure* self) {
    g_lastHook = "BuildStructure::Ctor";
    fn_ObjectInternal_Ctor(self);

    // Work base fields
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    self->field_10 = nullptr;
    self->field_18 = nullptr;
    self->field_20 = -1;
    self->field_24 = 0;
    self->field_28 = -1;
    self->field_2c = 0;
    self->field_30 = nullptr;
    self->field_38 = nullptr;
    self->field_40 = 0;

    // Set BuildStructure vtable
    self->vtable = (void*)ResolveAddr(Module::Exe, BuildStructureAddr::Vtable);

    // BuildStructure fields (ctor zeroes +0x50..+0x68)
    self->field_50 = nullptr;
    self->field_58 = 0;
    self->field_60 = nullptr;
    self->field_68 = 0;

    return self;
}

// ============================================================
// ScalarDtor (0x140029680) — reimplemented (clean C++)
// ============================================================
static void* Reimpl_ScalarDtor(BuildStructure* self, uint32_t flags) {
    g_lastHook = "BuildStructure::ScalarDtor";
    return BuildStructure_ScalarDtor(self, flags);
}

// ============================================================
// Serialize (0x1400e5850) — calls clean reimplementation
// ============================================================
static void Reimpl_Serialize(BuildStructure* self, void* stream) {
    g_lastHook = "BuildStructure::Serialize";
    BuildStructure_Serialize(self, stream);
}

// ============================================================
// Execute (0x1400e4fc0) — calls clean reimplementation
// Returns int (NOT bool) per bug class 7.
// ============================================================
static int Reimpl_Execute(BuildStructure* self, void* citizenPtr) {
    g_lastHook = "BuildStructure::Execute";
    return BuildStructure_Execute(self, citizenPtr);
}

// ============================================================
// OnComplete (0x1400e4f80) — reimplemented
// ============================================================
static int Reimpl_OnComplete(BuildStructure* self, void* citizenIBP,
                               void* unused, int slotIndex) {
    g_lastHook = "BuildStructure::OnComplete";
    BuildStructure_OnComplete(self, citizenIBP, unused, slotIndex);
    return 0; // original returns false (XOR AL,AL)
}

// ============================================================
// OnCancel (0x1400e4f40) — reimplemented
// ============================================================
static int Reimpl_OnCancel(BuildStructure* self, void* citizenIBP,
                             void* unused, int slotIndex) {
    g_lastHook = "BuildStructure::OnCancel";
    BuildStructure_OnCancel(self, citizenIBP, unused, slotIndex);
    return 0; // original returns false (XOR AL,AL)
}

// ============================================================
// Cleanup (0x1400e57a0) — reimplemented
// ============================================================
static void Reimpl_Cleanup(BuildStructure* self) {
    g_lastHook = "BuildStructure::Cleanup";
    BuildStructure_Cleanup(self);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_ObjectInternal_Ctor = reinterpret_cast<fn_ObjectInternal_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152768)));
    return fn_ObjectInternal_Ctor != nullptr;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallBuildStructureCtor() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[BuildStructure] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, BuildStructureAddr::Ctor);
    InstallHook(target, (void*)Reimpl_Ctor, &g_hookCtor, 19);
#ifdef _DEBUG
    printf("[BuildStructure] Ctor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureCtor() { RemoveHook(&g_hookCtor); }

void InstallBuildStructureScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildStructureAddr::ScalarDtor);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 12);
#ifdef _DEBUG
    printf("[BuildStructure] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallBuildStructureSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, BuildStructureAddr::Serialize);
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[BuildStructure] Serialize hooked at %p -> clean reimpl\n", (void*)target);
#endif
}
void RemoveBuildStructureSerialize() { RemoveHook(&g_hookSerialize); }

void InstallBuildStructureExecute() {
    // Prologue: MOV [RSP+0x10],RDX (5) + PUSH RBP/RBX/RSI/RDI (4) + PUSH R12/R13 (4) = 13 bytes
    // The MOV [RSP+0x10],RDX is bug class 4 (RSP-relative store) but we never call
    // the trampoline, so it's safe — we fully reimplement in clean C++.
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e4fc0);
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 13);
#ifdef _DEBUG
    printf("[BuildStructure] Execute hooked at %p -> clean reimpl\n", (void*)target);
#endif
}
void RemoveBuildStructureExecute() { RemoveHook(&g_hookExecute); }

void InstallBuildStructureOnComplete() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e4f80);
    InstallHook(target, (void*)Reimpl_OnComplete, &g_hookOnComplete, 13);
#ifdef _DEBUG
    printf("[BuildStructure] OnComplete hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureOnComplete() { RemoveHook(&g_hookOnComplete); }

void InstallBuildStructureOnCancel() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e4f40);
    InstallHook(target, (void*)Reimpl_OnCancel, &g_hookOnCancel, 13);
#ifdef _DEBUG
    printf("[BuildStructure] OnCancel hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureOnCancel() { RemoveHook(&g_hookOnCancel); }

void InstallBuildStructureCleanup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e57a0);
    InstallHook(target, (void*)Reimpl_Cleanup, &g_hookCleanup, 13);
#ifdef _DEBUG
    printf("[BuildStructure] Cleanup hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildStructureCleanup() { RemoveHook(&g_hookCleanup); }
