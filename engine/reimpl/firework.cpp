// engine/reimpl/firework.cpp
// Reimplementation of FireWork vtable methods from Application-x64.exe.
//
// FireWork is a Work subclass (0x60 bytes) for fire suppression work.
// Has a BST/map at +0x50 and a field at +0x58. Note: +0x48 is NOT initialized
// by the constructor (gap from Work base alignment).
//
// Vtable at 0x1401612a0, 12 slots:
//   +0x00: ScalarDtor (0x14002a230)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a1d0)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a1c0)   — tiny, not hooked
//   +0x18: Serialize  (0x1400e9a50)   — HOOKED (full reimpl)
//   +0x20: Slot4      (0x1400e9620)   — HOOKED (passthrough, complex fire logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400e9600)   — 20 bytes total, HOOKED (passthrough)
//   +0x38: Slot7      (0x1400e95f0)   — not found as function in Ghidra
//   +0x40: Slot8      (0x1400e99a0)   — HOOKED (passthrough, cleanup loop)
//   +0x48: Slot9      (0x1400e93c0)   — HOOKED (passthrough, complex fire AI)
//   +0x50: Slot10     (0x1400e9a30)   — not found as function in Ghidra
//   +0x58: Slot11     (0x14002a220)   — 9 bytes, unhookable (checks +0x58 > 0)

#include "FireWork.h"
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
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;

// ============================================================
// Local helper function pointers
// ============================================================

// Work base Serialize at 0x1400f7400
typedef void (*fn_Work_Serialize_t)(void* self, void* stream);
static fn_Work_Serialize_t fn_Work_Serialize = nullptr;

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_Serialize_t)(void* self, int* value, const char* name);
static fn_Serializer_Serialize_t fn_Serializer_Serialize = nullptr;

// BST serialization helper at 0x1400296b0 (FUN_1400296b0)
typedef void (*fn_BST_Serialize296b0_t)(void* bst, void* serializer);
static fn_BST_Serialize296b0_t fn_BST_Serialize = nullptr;

// ============================================================
// Serialize (0x1400e9a50)
// Pattern: Work_Serialize + Serializer(v0) + BST_Serialize(+0x50) +
//          Serializer::Serialize(+0x48)
// ============================================================
static void Reimpl_Serialize(FireWork* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[FireWork::Serialize] self=%p stream=%p field58=%d\n",
               (void*)self, stream, self->_field58);
        callCount++;
    }
#endif

    fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Serialize BST at +0x50
    fn_BST_Serialize(&self->_field50, serializerBuf);

    // Serialize int at +0x48 (Note: FireWork struct has this as padding area,
    // but the Serialize function accesses it. The decompilation shows param_1+0x48.)
    int* field48_ptr = reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(self) + 0x48);
    fn_Serializer_Serialize(serializerBuf, field48_ptr, nullptr);

    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Passthrough reimplementations
// ============================================================

// Slot4 (0x1400e9620) — complex fire suppression logic
typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

// Slot6 (0x1400e9600) — calls helper on +0x50, returns false
typedef void (*fn_Slot6_t)(void* self, void* param2);
static void Reimpl_Slot6(void* self, void* param2) {
    ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

// Slot8 (0x1400e99a0) — cleanup loop
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// Slot9 (0x1400e93c0) — complex fire AI validation
typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallFireWorkSerialize() {
    fn_Work_Serialize = reinterpret_cast<fn_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    // Serializer::Serialize via IAT at [0x140152ac8]
    uintptr_t iatSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSer) fn_Serializer_Serialize = *reinterpret_cast<fn_Serializer_Serialize_t*>(iatSer);

    fn_BST_Serialize = reinterpret_cast<fn_BST_Serialize296b0_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));

#ifdef _DEBUG
    printf("[FireWork] Work_Serialize=%p BST_Serialize=%p Serializer_Serialize=%p\n",
           (void*)fn_Work_Serialize, (void*)fn_BST_Serialize, (void*)fn_Serializer_Serialize);
#endif

    if (!fn_Work_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_BST_Serialize || !fn_Serializer_Serialize) {
#ifdef _DEBUG
        printf("[FireWork] ERROR: failed to resolve helpers, skipping Serialize hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9a50);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[FireWork] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveFireWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallFireWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9620);
    // Prologue: MOV [RSP+0x18],R8(5) + MOV [RSP+0x10],RDX(5) + PUSH RBP(2) + PUSH RBX(2) = 14
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 14);
#ifdef _DEBUG
    printf("[FireWork] Slot4 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallFireWorkSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e9600);
    // Prologue: SUB RSP,0x28(4) + ADD RCX,0x50(4) + CALL(5) = 13
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[FireWork] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallFireWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e99a0);
    // Prologue: MOV [RSP+0x8],RBX(5) + PUSH RDI(2) + SUB RSP,0x20(4) + MOV RDI,RCX(3) = 14
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[FireWork] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallFireWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e93c0);
    // Prologue: MOV RAX,RSP(3) + PUSH RBP(2) + PUSH RSI(2) + PUSH RDI(2) + PUSH R12(2) + PUSH R13(2) = 13
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[FireWork] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveFireWorkSlot9() { RemoveHook(&g_hookSlot9); }
