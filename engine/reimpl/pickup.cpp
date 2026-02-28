// engine/reimpl/pickup.cpp
// Reimplementation of Pickup vtable methods from Application-x64.exe.
//
// Pickup is a Work subclass (0x60 bytes) for item pickup work actions.
// Has uint32 at +0x48, InternalBasePtr at +0x50, and another ptr at +0x58.
//
// NOTE: Pickup has its OWN Execute override (0x1400f38f0), unlike other Work classes
// which share the base Execute (0x1400f7540 = XOR AL,AL; RET).
//
// Vtable at 0x140161810, 12 slots:
//   +0x00: ScalarDtor (0x14002c010)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002bfb0)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002bfa0)   — tiny, not hooked
//   +0x18: Serialize  (0x1400f3aa0)   — HOOKED (full reimpl)
//   +0x20: Slot4      (0x1400f2f00)   — HOOKED (passthrough, complex pickup logic)
//   +0x28: Execute    (0x1400f38f0)   — HOOKED (passthrough, unique pickup execute)
//   +0x30: Slot6      (0x1400f2ec0)   — HOOKED (passthrough)
//   +0x38: Slot7      (0x1400f2e80)   — HOOKED (passthrough)
//   +0x40: Slot8      (0x1400f39f0)   — HOOKED (passthrough, cleanup)
//   +0x48: Slot9      (0x1400f2e20)   — HOOKED (passthrough, validates)
//   +0x50: Slot10     (0x1400f3a50)   — HOOKED (passthrough)
//   +0x58: Slot11     (0x14002c000)   — 10 bytes, unhookable

#include "Pickup.h"
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
static HookInfo g_hookExecute;
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot10;

// ============================================================
// Local helper function pointers
// ============================================================

// Work base Serialize at 0x1400f7400
typedef void (*fn_Work_Serialize_t)(void* self, void* stream);
static fn_Work_Serialize_t fn_Work_Serialize = nullptr;

// IO::Serializer ctor/dtor/GetVersion/Serialize via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;
typedef void (*fn_Serializer_Serialize_t)(void* self, int* value, const char* name);
static fn_Serializer_Serialize_t fn_Serializer_Serialize = nullptr;
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_InternalBasePtr_Serialize_t)(void* basePtr, void* serializer);
static fn_InternalBasePtr_Serialize_t fn_InternalBasePtr_Serialize = nullptr;

// FUN_140018020 — array/list serialization helper for +0x58
typedef void (*fn_ArraySerialize_t)(void* arr, void* serializer);
static fn_ArraySerialize_t fn_ArraySerialize = nullptr;

// TypeId for validation (DAT_1401c7158 — uint32 value at that address)
static uint32_t g_typeId = 0;

// ============================================================
// Serialize (0x1400f3aa0)
// Pattern: Work_Serialize + Serializer(v2)
//   + Serializer::Serialize(+0x48)
//   + InternalBasePtr::Serialize(+0x50) + IsDerivedFrom validate
//   + if version > 0: ArraySerialize(+0x58)
// ============================================================
static void Reimpl_Serialize(Pickup* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[Pickup::Serialize] self=%p stream=%p field48=%d\n",
               (void*)self, stream, self->_field48);
        callCount++;
    }
#endif

    fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 2);

    // Serialize uint32 at +0x48
    fn_Serializer_Serialize(serializerBuf, reinterpret_cast<int*>(&self->_field48), nullptr);

    // Serialize InternalBasePtr at +0x50
    fn_InternalBasePtr_Serialize(&self->_field50, serializerBuf);

    // Validate the deserialized ptr
    if (self->_field50 != nullptr) {
        // fn_IsDerivedFrom is from engine_funcs.h (extern)
        if (!fn_IsDerivedFrom(self->_field50, g_typeId)) {
            self->_field50 = nullptr;
        }
    }

    // Version >= 1: serialize array at +0x58
    int version = fn_Serializer_GetVersion(serializerBuf);
    if (version > 0) {
        fn_ArraySerialize(&self->_field58, serializerBuf);
    }

    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Passthrough reimplementations
// ============================================================

typedef uint32_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint32_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

// Execute (0x1400f38f0) — unique to Pickup, handles re-targeting
typedef uint64_t (*fn_Execute_t)(void* self, void* param2, void* param3);
static uint64_t Reimpl_Execute(void* self, void* param2, void* param3) {
    return ((fn_Execute_t)g_hookExecute.trampoline)(self, param2, param3);
}

typedef uint64_t (*fn_Slot6_t)(void* self, void* param2);
static uint64_t Reimpl_Slot6(void* self, void* param2) {
    return ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
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

static void ResolveHelpers() {
    fn_Work_Serialize = reinterpret_cast<fn_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    uintptr_t iatSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSer) fn_Serializer_Serialize = *reinterpret_cast<fn_Serializer_Serialize_t*>(iatSer);

    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152ae0);
    if (iatGetVer) fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(iatGetVer);

    uintptr_t iatBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatBPSer) fn_InternalBasePtr_Serialize = *reinterpret_cast<fn_InternalBasePtr_Serialize_t*>(iatBPSer);

    fn_ArraySerialize = reinterpret_cast<fn_ArraySerialize_t>(
        ResolveAddr(Module::Exe, 0x140018020));

    // TypeId global — read uint32 value at 0x1401c7158
    g_typeId = *reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
}

void InstallPickupSerialize() {
    ResolveHelpers();
#ifdef _DEBUG
    printf("[Pickup] Work_Serialize=%p Serializer_Ctor=%p Serialize=%p GetVersion=%p\n",
           (void*)fn_Work_Serialize, (void*)fn_Serializer_Ctor,
           (void*)fn_Serializer_Serialize, (void*)fn_Serializer_GetVersion);
    printf("[Pickup] InternalBasePtr_Serialize=%p ArraySerialize=%p\n",
           (void*)fn_InternalBasePtr_Serialize, (void*)fn_ArraySerialize);
#endif
    if (!fn_Work_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_Serializer_Serialize || !fn_InternalBasePtr_Serialize ||
        !fn_ArraySerialize) {
#ifdef _DEBUG
        printf("[Pickup] ERROR: failed to resolve helpers, skipping Serialize hook\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3aa0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[Pickup] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemovePickupSerialize() { RemoveHook(&g_hookSerialize); }

void InstallPickupSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2f00);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[Pickup] Slot4 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupSlot4() { RemoveHook(&g_hookSlot4); }

void InstallPickupExecute() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f38f0);
    // Prologue: MOV [RSP+0x20],RBP(5) + PUSH RSI(2) + PUSH R14(2) + PUSH R15(2) + SUB RSP,0x20(4) = 15
    InstallHook(target, (void*)Reimpl_Execute, &g_hookExecute, 15);
#ifdef _DEBUG
    printf("[Pickup] Execute hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupExecute() { RemoveHook(&g_hookExecute); }

void InstallPickupSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2ec0);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 12);
#ifdef _DEBUG
    printf("[Pickup] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupSlot6() { RemoveHook(&g_hookSlot6); }

void InstallPickupSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2e80);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 12);
#ifdef _DEBUG
    printf("[Pickup] Slot7 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupSlot7() { RemoveHook(&g_hookSlot7); }

void InstallPickupSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f39f0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[Pickup] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupSlot8() { RemoveHook(&g_hookSlot8); }

void InstallPickupSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f2e20);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[Pickup] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupSlot9() { RemoveHook(&g_hookSlot9); }

void InstallPickupSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f3a50);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[Pickup] Slot10 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemovePickupSlot10() { RemoveHook(&g_hookSlot10); }
