// engine/reimpl/clearstorage.cpp
// Reimplementation of ClearStorage vtable methods from Application-x64.exe.
//
// ClearStorage is a Work subclass (0x68 bytes) for clearing storage buildings.
// Has ptrs at +0x48/+0x50, a BST root at +0x58, and a count at +0x60.
//
// Vtable at 0x1401611a0, 12 slots:
//   +0x00: ScalarDtor (0x14002a100)   — tiny, not hooked
//   +0x08: VectorDtor (0x14002a0a0)   — tiny, not hooked
//   +0x10: BaseDtor   (0x14002a090)   — tiny, not hooked
//   +0x18: Serialize  (0x1400e7350)   — HOOKED (full reimpl)
//   +0x20: Slot4      (0x1400e6c50)   — HOOKED (full reimpl → ClearStorage_Execute)
//   +0x28: Execute    (0x1400f7540)   — 3 bytes, unhookable
//   +0x30: Slot6      (0x1400e6c30)   — HOOKED (passthrough)
//   +0x38: Slot7      (0x1400e6c20)   — needs check
//   +0x40: Slot8      (0x1400e72a0)   — HOOKED (passthrough)
//   +0x48: Slot9      (0x1400e69f0)   — HOOKED (passthrough, complex)
//   +0x50: Slot10     (0x1400e7330)   — HOOKED (passthrough)
//   +0x58: Slot11     (0x14002a0f0)   — 9 bytes, unhookable (checks +0x60 > 0)

#include "ClearStorage.h"
#include "Work/clearstorage_funcs.h"
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

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// BST serialization helper at 0x1400296b0
typedef void (*fn_BST_Serialize_t)(void* bst, void* serializer);
static fn_BST_Serialize_t fn_BST_Serialize = nullptr;

// ============================================================
// Serialize (0x1400e7350)
// Pattern: Work_Serialize + Serializer(v0) + BST_Serialize(+0x58)
// ============================================================
static void Reimpl_Serialize(ClearStorage* self, void* stream) {
    g_lastHook = "ClearStorage::Serialize";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[ClearStorage::Serialize] self=%p stream=%p field60=%d\n",
               (void*)self, stream, self->_field60);
        callCount++;
    }
#endif

    fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 0);
    fn_BST_Serialize(&self->_field58, serializerBuf);
    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Passthrough reimplementations
// ============================================================

// Slot4/Execute — delegates to clean reimplementation in code/Game/Work/clearstorage.cpp
static int Reimpl_Slot4(ClearStorage* self, void* citizenAI, void* time) {
    g_lastHook = "ClearStorage::Slot4";
    return ClearStorage_Execute(self, citizenAI, time);
}

typedef uint8_t (*fn_Slot6_t)(void* self, void* param2);
static uint8_t Reimpl_Slot6(void* self, void* param2) {
    g_lastHook = "ClearStorage::Slot6";
    return ((fn_Slot6_t)g_hookSlot6.trampoline)(self, param2);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "ClearStorage::Slot8";
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

// Slot9/Validate — hook DISABLED (bug class 3: MOV RAX,RSP in prologue).
// Clean reimpl exists in code/Game/Work/clearstorage.cpp (ClearStorage_Validate)
// but we keep the trampoline here since the hook can't be safely installed.
typedef uint64_t (*fn_Slot9_t)(void* self, void* param2);
static uint64_t Reimpl_Slot9(void* self, void* param2) {
    g_lastHook = "ClearStorage::Slot9";
    return ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2);
}

typedef void* (*fn_Slot10_t)(void* self, void* param2);
static void* Reimpl_Slot10(void* self, void* param2) {
    g_lastHook = "ClearStorage::Slot10";
    return ((fn_Slot10_t)g_hookSlot10.trampoline)(self, param2);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallClearStorageSerialize() {
    fn_Work_Serialize = reinterpret_cast<fn_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    fn_BST_Serialize = reinterpret_cast<fn_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));

#ifdef _DEBUG
    printf("[ClearStorage] Work_Serialize=%p BST_Serialize=%p\n",
           (void*)fn_Work_Serialize, (void*)fn_BST_Serialize);
#endif

    if (!fn_Work_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor || !fn_BST_Serialize) {
#ifdef _DEBUG
        printf("[ClearStorage] ERROR: failed to resolve helpers, skipping Serialize hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e7350);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV [RSP+0x20],-2(9) = 15
    InstallHook(target, (void*)Reimpl_Serialize, &g_hookSerialize, 15);
#ifdef _DEBUG
    printf("[ClearStorage] Serialize hooked at %p\n", (void*)target);
#endif
}

void RemoveClearStorageSerialize() { RemoveHook(&g_hookSerialize); }

void InstallClearStorageSlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e6c50);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[ClearStorage] Slot4 hooked at %p (reimpl)\n", (void*)target);
#endif
}

void RemoveClearStorageSlot4() { RemoveHook(&g_hookSlot4); }

void InstallClearStorageSlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e6c30);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[ClearStorage] Slot6 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveClearStorageSlot6() { RemoveHook(&g_hookSlot6); }

void InstallClearStorageSlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e72a0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 16);
#ifdef _DEBUG
    printf("[ClearStorage] Slot8 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveClearStorageSlot8() { RemoveHook(&g_hookSlot8); }

void InstallClearStorageSlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e69f0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 18);
#ifdef _DEBUG
    printf("[ClearStorage] Slot9 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveClearStorageSlot9() { RemoveHook(&g_hookSlot9); }

void InstallClearStorageSlot10() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400e7330);
    InstallHook(target, (void*)Reimpl_Slot10, &g_hookSlot10, 18);
#ifdef _DEBUG
    printf("[ClearStorage] Slot10 hooked at %p (passthrough)\n", (void*)target);
#endif
}

void RemoveClearStorageSlot10() { RemoveHook(&g_hookSlot10); }
