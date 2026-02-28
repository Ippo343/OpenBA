// engine/reimpl/locationwork.cpp
// Reimplementation of LocationWork vtable methods from Application-x64.exe.
//
// LocationWork is a Work subclass (0x50 bytes) with one extra field at +0x48
// (an InternalBasePtr to a target entity).
//
// Vtable at 0x1401616a0, 12 slots:
//   +0x00: ScalarDtor (0x14002a850)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a800)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a7f0)   — tiny, not hooked
//   +0x18: Serialize  (0x1400efba0)   — HOOKED (full reimpl)
//   +0x20: Slot4      (0x1400ef880)   — HOOKED (passthrough, complex pathfinding logic)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes (XOR AL,AL; RET), unhookable
//   +0x30: Slot6      (0x1400ef850)   — HOOKED (passthrough, clears +0x48)
//   +0x38: Slot7      (0x1400ef820)   — HOOKED (passthrough, clears +0x48)
//   +0x40: Slot8      (0x1400efaf0)   — HOOKED (passthrough, cleanup via subsystem)
//   +0x48: Slot9      (0x1400ef7c0)   — HOOKED (passthrough, validates target)
//   +0x50: Slot10     (0x1400efb40)   — HOOKED (passthrough, gets entity sub-object)
//   +0x58: Slot11     (0x1400295a0)   — 10 bytes, unhookable (checks +0x48 != 0)

#include "LocationWork.h"
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

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// Object::InternalBasePtr::Serialize
typedef void (*fn_InternalBasePtr_Serialize_t)(void* basePtr, void* serializer);
static fn_InternalBasePtr_Serialize_t fn_InternalBasePtr_Serialize = nullptr;

// TypeId for validation (DAT_1401c7158 — uint32 value at that address)
static uint32_t g_typeId = 0;

// ============================================================
// Serialize (0x1400efba0)
// Pattern: Work_Serialize + Serializer(v0) + InternalBasePtr::Serialize(+0x48)
//   + validate IsDerivedFrom, clear if invalid
// ============================================================
static void Reimpl_Serialize(LocationWork* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[LocationWork::Serialize] self=%p stream=%p field48=%p\n",
               (void*)self, stream, self->_field48);
        callCount++;
    }
#endif

    fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 0);

    fn_InternalBasePtr_Serialize(&self->_field48, serializerBuf);

    if (self->_field48 != nullptr) {
        if (!fn_IsDerivedFrom(self->_field48, g_typeId)) {
            self->_field48 = nullptr;
        }
    }
    // Note: fn_IsDerivedFrom is from engine_funcs.h (extern)

    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Passthrough reimplementations
// ============================================================

// Slot4 (0x1400ef880) — complex pathfinding/pickup logic
typedef uint64_t (*fn_Slot4_t)(void* self, void* param2, void* param3);
static uint64_t Reimpl_Slot4(void* self, void* param2, void* param3) {
    return ((fn_Slot4_t)g_hookSlot4.trampoline)(self, param2, param3);
}

// Slot6 (0x1400ef850) — clears +0x48 to null InternalBasePtr
typedef uint64_t (*fn_Slot6_t)(void* self);
static uint64_t Reimpl_Slot6(void* self) {
    return ((fn_Slot6_t)g_hookSlot6.trampoline)(self);
}

// Slot7 (0x1400ef820) — clears +0x48 to null InternalBasePtr
typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

// Slot8 (0x1400efaf0) — cleanup via subsystem call
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// Slot9 (0x1400ef7c0) — validates target, calls helper
typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

// Slot10 (0x1400efb40) — gets entity sub-object
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

    // InternalBasePtr::Serialize via IAT at [0x140152aa0]
    uintptr_t iatBPSer = ResolveAddr(Module::Exe, 0x140152aa0);
    if (iatBPSer) fn_InternalBasePtr_Serialize = *reinterpret_cast<fn_InternalBasePtr_Serialize_t*>(iatBPSer);

    // TypeId global — read uint32 value at 0x1401c7158
    g_typeId = *reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
}

void InstallLocationWorkSerialize() {
    ResolveHelpers();
#ifdef _DEBUG
    printf("[LocationWork] Work_Serialize=%p Serializer_Ctor=%p Dtor=%p\n",
           (void*)fn_Work_Serialize, (void*)fn_Serializer_Ctor, (void*)fn_Serializer_Dtor);
    printf("[LocationWork] InternalBasePtr_Serialize=%p IsDerivedFrom=%p typeId=0x%x\n",
           (void*)fn_InternalBasePtr_Serialize, (void*)fn_IsDerivedFrom, g_typeId);
#endif
    if (!fn_Work_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_InternalBasePtr_Serialize) {
#ifdef _DEBUG
        printf("[LocationWork] ERROR: failed to resolve helpers, skipping Serialize hook\n");
#endif
        return;
    }
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efba0);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[LocationWork] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveLocationWorkSerialize() { RemoveHook(&g_hookSerialize); }

void InstallLocationWorkSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef880);
    // Prologue: PUSH RBP(2) + PUSH R12(2) + PUSH R14(2) + MOV RBP,RSP(3) + SUB RSP,0x70(4) = 13
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[LocationWork] Slot4 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot4() { RemoveHook(&g_hookSlot4); }

void InstallLocationWorkSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef850);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + LEA RCX,[RSP+0x30](5) = 14
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 14);
#ifdef _DEBUG
    printf("[LocationWork] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot6() { RemoveHook(&g_hookSlot6); }

void InstallLocationWorkSlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef820);
    // Same prologue as Slot6: 14 bytes
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[LocationWork] Slot7 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot7() { RemoveHook(&g_hookSlot7); }

void InstallLocationWorkSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efaf0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + CMP [RCX+0x48],0(5) + LEA RBX,[RCX+0x48](4) = 15
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 15);
#ifdef _DEBUG
    printf("[LocationWork] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot8() { RemoveHook(&g_hookSlot8); }

void InstallLocationWorkSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400ef7c0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RAX,[RCX+0x48](4) + MOV RBX,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[LocationWork] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot9() { RemoveHook(&g_hookSlot9); }

void InstallLocationWorkSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400efb40);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RDI(2) + SUB RSP,0x20(4) + LEA RBX,[RCX+0x10](4) = 15
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 15);
#ifdef _DEBUG
    printf("[LocationWork] Slot10 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveLocationWorkSlot10() { RemoveHook(&g_hookSlot10); }
