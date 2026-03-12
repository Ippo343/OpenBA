// engine/reimpl/finishtutorialinstance.cpp
// Reimplementation of FinishTutorialInstance vtable methods from Application-x64.exe.
//
// FinishTutorialInstance (0x28 bytes), vtable at 0x140160478.
// Extends Achievement::GoalInstance.
//
// Vtable layout (7 slots):
//   +0x00: 0x140026200 — Dtor(self, flags) (SHARED with FinishTutorialGoal Slot11)
//   +0x08: 0x140026060 — GetTypeInfo (8b, unhookable)
//   +0x10: 0x140026050 — GetTypeId (7b, unhookable)
//   +0x18: 0x1400d7bf0 — Slot3/Serialize
//   +0x20: 0x1400d7a90 — Slot4/Load
//   +0x28: 0x1400d7a50 — Slot5/Startup
//   +0x30: 0x1400d7bb0 — Slot6/Shutdown

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot3;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot6;

// ============================================================
// Original function typedefs
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
typedef void  (*fn_Slot3_t)(void* self, void* stream);
typedef void  (*fn_Slot4_t)(void* self, void* p2, void* p3);
typedef void  (*fn_Slot5_t)(void* self, void* p2, void* p3, void* p4);
typedef void  (*fn_Slot6_t)(void* self);

// ============================================================
// Passthrough wrappers
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "FinishTutorialInstance::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

static void Reimpl_Slot3(void* self, void* stream) {
    g_lastHook = "FinishTutorialInstance::Slot3";
    ((fn_Slot3_t)g_hookSlot3.trampoline)(self, stream);
}

static void Reimpl_Slot4(void* self, void* p2, void* p3) {
    g_lastHook = "FinishTutorialInstance::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self, p2, p3);
}

static void Reimpl_Slot5(void* self, void* p2, void* p3, void* p4) {
    g_lastHook = "FinishTutorialInstance::Slot5";
    ((fn_Slot5_t)g_hookSlot5.trampoline)(self, p2, p3, p4);
}

static void Reimpl_Slot6(void* self) {
    g_lastHook = "FinishTutorialInstance::Slot6";
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFinishTutorialInstanceDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026200);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[FinishTutorialInstance] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialInstanceDtor() { RemoveHook(&g_hookDtor); }

void InstallFinishTutorialInstanceSlot3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d7bf0);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot3, &g_hookSlot3, 15);
#ifdef _DEBUG
    printf("[FinishTutorialInstance] Slot3 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialInstanceSlot3() { RemoveHook(&g_hookSlot3); }

void InstallFinishTutorialInstanceSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d7a90);
    // Prologue: push rdi(2) + sub rsp,0x40(4) + mov qword[rsp+0x30],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[FinishTutorialInstance] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialInstanceSlot4() { RemoveHook(&g_hookSlot4); }

void InstallFinishTutorialInstanceSlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d7a50);
    // Prologue: mov [rsp+8],rbx(5) + push rdi(1) + sub rsp,0x20(4) + lea rbx,[rcx+0x20](4) + mov rdi,rdx(3) = 17
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 17);
#ifdef _DEBUG
    printf("[FinishTutorialInstance] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialInstanceSlot5() { RemoveHook(&g_hookSlot5); }

void InstallFinishTutorialInstanceSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400d7bb0);
    // Prologue: push rbx(2) + sub rsp,0x20(4) + lea rbx,[rcx+0x20](4) + mov rcx,rbx(3) = 13
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[FinishTutorialInstance] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveFinishTutorialInstanceSlot6() { RemoveHook(&g_hookSlot6); }
