// engine/reimpl/tendarea.cpp
// Reimplementation of TendArea vtable methods from Application-x64.exe.
//
// TendArea is a Work subclass (0x60 bytes) for area tending work (crops, orchards, etc).
// Has a BST at +0x48, a count at +0x50, and a serialized int at +0x58.
//
// Vtable at 0x140161990, 12 slots:
//   +0x00: ScalarDtor (0x14002c5d0)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002c580)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002c570)   — tiny, not hooked
//   +0x18: Serialize  (0x1400f5900)   — HOOKED (full reimpl)
//   +0x20: Slot4      (0x1400f5490)   — HOOKED (passthrough, complex tending logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400f5470)   — HOOKED (passthrough)
//   +0x38: Slot7      (0x1400f5460)   — needs check
//   +0x40: Slot8      (0x1400f5830)   — HOOKED (passthrough, cleanup)
//   +0x48: Slot9      (0x1400f5230)   — HOOKED (passthrough, validates)
//   +0x50: Slot10     (0x1400f58a0)   — HOOKED (passthrough)
//   +0x58: Slot11     (0x140029860)   — 9 bytes, unhookable

#include "TendArea.h"
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
static HookInfo g_hookSlot10;

// ============================================================
// Local helper function pointers
// ============================================================

// Work base Serialize at 0x1400f7400
typedef void (*fn_Work_Serialize_t)(void* self, void* stream);
static fn_Work_Serialize_t fn_Work_Serialize = nullptr;

// IO::Serializer ctor/dtor/Serialize via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serializer_Serialize_t)(void* self, int* value, const char* name);
static fn_Serializer_Serialize_t fn_Serializer_Serialize = nullptr;

// BST serialization helper at 0x1400296b0
typedef void (*fn_BST_Serialize_t)(void* bst, void* serializer);
static fn_BST_Serialize_t fn_BST_Serialize = nullptr;

// ============================================================
// Serialize (0x1400f5900)
// Pattern: Work_Serialize + Serializer(v0) + BST_Serialize(+0x48) +
//          Serializer::Serialize(+0x58)
// ============================================================
static void Reimpl_Serialize(TendArea* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[TendArea::Serialize] self=%p stream=%p field58=%d\n",
               (void*)self, stream, self->_field58);
        callCount++;
    }
#endif

    fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Serialize BST at +0x48
    fn_BST_Serialize(&self->_field48, serializerBuf);

    // Serialize int at +0x58
    fn_Serializer_Serialize(serializerBuf, reinterpret_cast<int*>(&self->_field58), nullptr);

    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Passthrough reimplementations
// ============================================================

typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

typedef uint8_t (*fn_Slot6_t)(void* self, void* param2);
static uint8_t Reimpl_Slot6(void* self, void* param2) {
    return ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
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

void InstallTendAreaSerialize() {
    fn_Work_Serialize = reinterpret_cast<fn_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    uintptr_t iatSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSer) fn_Serializer_Serialize = *reinterpret_cast<fn_Serializer_Serialize_t*>(iatSer);

    fn_BST_Serialize = reinterpret_cast<fn_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));

#ifdef _DEBUG
    printf("[TendArea] Work_Serialize=%p BST_Serialize=%p Serializer_Serialize=%p\n",
           (void*)fn_Work_Serialize, (void*)fn_BST_Serialize, (void*)fn_Serializer_Serialize);
#endif

    if (!fn_Work_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_BST_Serialize || !fn_Serializer_Serialize) {
#ifdef _DEBUG
        printf("[TendArea] ERROR: failed to resolve helpers, skipping Serialize hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5900);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[TendArea] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveTendAreaSerialize() { RemoveHook(&g_hookSerialize); }

void InstallTendAreaSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5490);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[TendArea] Slot4 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveTendAreaSlot4() { RemoveHook(&g_hookSlot4); }

void InstallTendAreaSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5470);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 12);
#ifdef _DEBUG
    printf("[TendArea] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveTendAreaSlot6() { RemoveHook(&g_hookSlot6); }

void InstallTendAreaSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5830);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[TendArea] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveTendAreaSlot8() { RemoveHook(&g_hookSlot8); }

void InstallTendAreaSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f5230);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[TendArea] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveTendAreaSlot9() { RemoveHook(&g_hookSlot9); }

void InstallTendAreaSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f58a0);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[TendArea] Slot10 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveTendAreaSlot10() { RemoveHook(&g_hookSlot10); }
