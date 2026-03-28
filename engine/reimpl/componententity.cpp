// engine/reimpl/componententity.cpp
// Reimplementation of ComponentEntity vtable methods from Application-x64.exe.
//
// ComponentEntity (0x48 bytes), vtable at 0x14015fa78.
// Extends Object::Internal.
//
// Vtable layout (12 slots, Object::Internal base):
//   +0x00: 0x140021a00 -- Dtor(self, flags)           -> DEEP REIMPL
//   +0x08: 0x1400219b0 -- GetTypeInfo (8b, unhookable)
//   +0x10: 0x1400219a0 -- GetTypeId (7b, unhookable)
//   +0x18: 0x1400c3140 -- Slot3/Serialize              -> DEEP REIMPL (hook DISABLED)
//   +0x20: NULL         -- Slot4 (null, skip)
//   +0x28: 0x140021c90 -- Slot5                        -> DEEP REIMPL
//   +0x30: 0x140021c80 -- Slot6/GetTypeInfo2 (8b, unhookable)
//   +0x38: 0x140021c70 -- Slot7/GetTypeId2 (7b, unhookable)
//   +0x40: 0x1400c4b40 -- Slot8/Load                   -> DEEP REIMPL
//   +0x48: 0x14003addc -- Slot9 (_purecall, passthrough OK)
//   +0x50: 0x1400c4a90 -- Slot10 (3b, unhookable: xor eax,eax; ret)
//   +0x58: 0x1400c4b90 -- Slot11 (3b, unhookable: ret 0x0)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Entity/componententity_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot3;
static HookInfo g_hookSlot5;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;

// ============================================================
// Original function typedefs (for Slot9 passthrough)
// ============================================================
typedef void (*fn_Slot9_t)(void* self);

// ============================================================
// Reimpl wrappers -- delegate to clean code in code/Game/Entity/
// ============================================================
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "ComponentEntity::Dtor";
    return ComponentEntity_Dtor((ComponentEntity*)self, flags);
}

static void Reimpl_Slot3(void* self, void* stream) {
    g_lastHook = "ComponentEntity::Slot3";
    ComponentEntity_Serialize((ComponentEntity*)self, stream);
}

static void* Reimpl_Slot5(void* self, uint32_t flags) {
    g_lastHook = "ComponentEntity::Slot5";
    return ComponentEntity_Slot5((ComponentEntity*)self, flags);
}

static void Reimpl_Slot8(void* self, void* stream) {
    g_lastHook = "ComponentEntity::Slot8";
    ComponentEntity_Slot8((ComponentEntity*)self, stream);
}

static void Reimpl_Slot9(void* self) {
    g_lastHook = "ComponentEntity::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallComponentEntityDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140021a00);
    // Prologue: mov [rsp+8],rbx(5) + push rdi(2) + sub rsp,0x20(4) + mov ebx,edx(2) + mov rdi,rcx(3) = 13
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[ComponentEntity] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveComponentEntityDtor() { RemoveHook(&g_hookDtor); }

void InstallComponentEntitySlot3() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c3140);
    // Prologue: push rbp(2)+push rsi(1)+push rdi(1)+push r12(2)+push r13(2)+push r14(2)+push r15(2)+sub rsp,0x40(4) = 16
    InstallHook(target, (void*)Reimpl_Slot3, &g_hookSlot3, 16);
#ifdef _DEBUG
    printf("[ComponentEntity] Slot3 hooked at %p\n", (void*)target);
#endif
}
void RemoveComponentEntitySlot3() { RemoveHook(&g_hookSlot3); }

void InstallComponentEntitySlot5() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140021c90);
    // Prologue: mov [rsp+8],rcx(5) + push rdi(2) + sub rsp,0x30(4) + mov qword,-2(9) = 19
    InstallHook(target, (void*)Reimpl_Slot5, &g_hookSlot5, 19);
#ifdef _DEBUG
    printf("[ComponentEntity] Slot5 hooked at %p\n", (void*)target);
#endif
}
void RemoveComponentEntitySlot5() { RemoveHook(&g_hookSlot5); }

void InstallComponentEntitySlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400c4b40);
    // Prologue: push rbx(2) + sub rsp,0x40(4) + mov qword,-2(9) = 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[ComponentEntity] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveComponentEntitySlot8() { RemoveHook(&g_hookSlot8); }

void InstallComponentEntitySlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14003addc);
    // Prologue: sub rsp,0x28(4) + mov rcx,[rip+xx](7) = boundary at 4, need more. Next is call(6) = 10. Next test(3) = 13
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 17);
#ifdef _DEBUG
    printf("[ComponentEntity] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveComponentEntitySlot9() { RemoveHook(&g_hookSlot9); }
