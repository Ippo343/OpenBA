// engine/reimpl/tutorialgoaldialog.cpp
// Reimplementation of TutorialGoalDialog vtable methods from Application-x64.exe.
//
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
// Object::External base ~0x20 bytes, subclass fields start at +0x20.
//
// Vtable layout (10 slots — Object::External base):
//   +0x00: 0x140027590 — ScalarDtor (hookable, 14b prologue)
//   +0x08: 0x140027430 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140027420 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400df080 — Slot3: MOV EAX,1; RET (6b, unhookable)
//   +0x20: 0x14003a912 — Runtime thunk (6b, unhookable)
//   +0x28: 0x1400defd0 — Shutdown: serialize/cleanup (hookable, 14b)
//   +0x30: 0x140027520 — OnEvent: handle goal dialog events (hookable, 14b)
//   +0x38: 0x140001640 — Delegator: MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX] (8b, unhookable)
//   +0x40: 0x1400deee0 — OnInit: initialize tutorial goal state (hookable, 13b)
//   +0x48: 0x1400274b0 — OnDestroy: cleanup tutorial goal dialog (hookable, 14b)

#include "ui/Dialogs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookShutdown;
static HookInfo g_hookOnEvent;
static HookInfo g_hookOnInit;
static HookInfo g_hookOnDestroy;

// ============================================================
// Reimplemented / Passthrough wrappers
// ============================================================

// Slot 0: ScalarDtor(this, flags)
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "TutorialGoalDialog::ScalarDtor";
    return ((fn_ScalarDtor_t)g_hookScalarDtor.trampoline)(self, flags);
}

// Slot 5: Shutdown(this, stream) — serialize/cleanup
typedef uint64_t (*fn_Shutdown_t)(void* self, void* stream);
static uint64_t Reimpl_Shutdown(void* self, void* stream) {
    g_lastHook = "TutorialGoalDialog::Shutdown";
    return ((fn_Shutdown_t)g_hookShutdown.trampoline)(self, stream);
}

// Slot 6: OnEvent(this, p2, p3, p4) — handle goal dialog events
typedef void* (*fn_OnEvent_t)(void* self, uint64_t p2, uint64_t p3, uint64_t p4);
static void* Reimpl_OnEvent(void* self, uint64_t p2, uint64_t p3, uint64_t p4) {
    g_lastHook = "TutorialGoalDialog::OnEvent";
    return ((fn_OnEvent_t)g_hookOnEvent.trampoline)(self, p2, p3, p4);
}

// Slot 8: OnInit(this) — REIMPLEMENTED
// Finds a dialog by string name from +0x20, checks its user data against
// the target description. Returns true(1) if the dialog's description
// matches self->data38, or if data38 is null.
//
// Original: 0x1400DEEE0.
static uint64_t Reimpl_OnInit(void* self) {
    g_lastHook = "TutorialGoalDialog::OnInit";
    uintptr_t s = (uintptr_t)self;

    // Find dialog by string name at +0x20
    uint64_t dialogPtr = 0;
    fn_FindDialogByName(&dialogPtr, (void*)(s + 0x20));

    if (dialogPtr == 0) {
        return 0;  // dialog not found — return false
    }

    // Get dialog's user data at dialogInstance+0x30
    void* userData = *(void**)(dialogPtr + 0x30);
    void* description = nullptr;

    if (userData != nullptr) {
        // Check if it's a Description type (DAT_1401c7318 = g_TypeId_BuildComponent)
        if (fn_IsDerivedFrom(userData, *g_TypeId_BuildComponent)) {
            // Direct Description — use as-is
            if (fn_IsDerivedFrom(userData, *g_TypeId_BuildComponent)) {
                description = userData;
            }
        } else {
            // Check if it's a ComponentEntity
            if (fn_IsDerivedFrom(userData, *g_TypeId_ComponentEntity)) {
                void* entity = nullptr;
                if (fn_IsDerivedFrom(userData, *g_TypeId_ComponentEntity)) {
                    entity = userData;
                }
                // Get Description from ComponentEntity at +0x28
                description = *(void**)((uintptr_t)entity + 0x28);
            }
        }
    }

    // Resolve the target External name at +0x28
    fn_ResolveExternalByName((void*)(s + 0x28));

    // If self->data38 is set, compare with found description
    void* target = *(void**)(s + 0x38);
    if (target != nullptr) {
        fn_ResolveExternalByName((void*)(s + 0x28));
        if (target != description) {
            return 0;  // mismatch — return false
        }
    }

    return 1;  // match — return true
}

// Slot 9: OnDestroy(this) — REIMPLEMENTED
// Reinitializes the TutorialGoalDialog to its default state by
// calling the Object::External base constructor, then setting the
// final vtable and zeroing all subclass fields.
//
// Original: 0x1400274B0.
static void* Reimpl_OnDestroy(void* self) {
    g_lastHook = "TutorialGoalDialog::OnDestroy";

    if (self != nullptr) {
        // Call Object::External::External(self) to reinit base
        fn_External_External(self);

        // Set final vtable (PTR_FUN_140160870)
        // The disassembly shows two writes — intermediate then final vtable.
        // We only need the final one since the intermediate is immediately overwritten.
        static void* s_vtable = nullptr;
        if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, 0x140160870);
        *(void**)self = s_vtable;

        uintptr_t s = (uintptr_t)self;

        // Zero data20 (dialog name string — 8 bytes)
        *(uint64_t*)(s + 0x20) = 0;

        // Zero data28 (target entity string — 8 bytes)
        *(uint64_t*)(s + 0x28) = 0;

        // Set data30 = 0xFFFFFFFF (target index = -1)
        *(uint32_t*)(s + 0x30) = 0xFFFFFFFF;

        // Zero data38 (target pointer)
        *(uint64_t*)(s + 0x38) = 0;
    }

    return self;
}

// ============================================================
// Install / Remove
// ============================================================

void InstallTutorialGoalDialogScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027590);
    // Prologue: MOV [rsp+8],rcx(5) + PUSH RDI(1) + SUB RSP,0x30(4) + MOV qword,-2(9) = 19
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 19);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallTutorialGoalDialogShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400defd0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x50(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTutorialGoalDialogOnEvent() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140027520);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_OnEvent, &g_hookOnEvent, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnEvent hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnEvent() { RemoveHook(&g_hookOnEvent); }

void InstallTutorialGoalDialogOnInit() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400deee0);
    InstallHook(target, (void*)Reimpl_OnInit, &g_hookOnInit, 13);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnInit hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnInit() { RemoveHook(&g_hookOnInit); }

void InstallTutorialGoalDialogOnDestroy() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400274b0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x30(4) + MOV qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_OnDestroy, &g_hookOnDestroy, 15);
#ifdef _DEBUG
    printf("[TutorialGoalDialog] OnDestroy hooked at %p\n", (void*)target);
#endif
}
void RemoveTutorialGoalDialogOnDestroy() { RemoveHook(&g_hookOnDestroy); }
