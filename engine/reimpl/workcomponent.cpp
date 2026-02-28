// engine/reimpl/workcomponent.cpp
// Reimplementation of WorkComponent lifecycle methods from Application-x64.exe.
//
// WorkComponent is a direct Component subclass (0x38 bytes) with work ID, flags,
// and state fields. Three non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x14012e710) — versioned serialization of workId, workState, workFlags
//   - Draw (0x14012e6f0) — compute work capacity and store at +0x30
//   - OnCreate (0x14012e6c0) — check if work slot available, register if so
//
// All other vtable slots are no-ops (no Update, no Serialize, etc.)

#include "WorkComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Local helper function pointers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_Serialize_t)(void* self, int* value, const char* name);
static fn_Serializer_Serialize_t fn_Serializer_Serialize = nullptr;

// IO::Serializer::GetVersion(this) -> int
typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// FUN_14012e5c0 — compute work capacity for this component
typedef int (*fn_ComputeWorkCapacity_t)(void* self);
static fn_ComputeWorkCapacity_t fn_ComputeWorkCapacity = nullptr;

// FUN_14012ead0 — check if work slot is available (BST lookup)
typedef bool (*fn_CheckWorkSlot_t)(void* self);
static fn_CheckWorkSlot_t fn_CheckWorkSlot = nullptr;

// FUN_14012eb50 — register work slot
typedef void (*fn_RegisterWorkSlot_t)(void* self);
static fn_RegisterWorkSlot_t fn_RegisterWorkSlot = nullptr;

// ============================================================
// Shutdown (0x14012e710)
// Versioned serialization:
//   - Always: serialize workId (+0x28)
//   - Version < 3: serialize a legacy bool (discarded)
//   - Version >= 1: serialize workState (+0x30)
//   - Version >= 2: serialize workFlags (+0x2c)
// ============================================================
static void Reimpl_Shutdown(WorkComponent* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[WorkComponent::Shutdown] self=%p workId=%d workFlags=0x%x workState=%d\n",
               (void*)self, self->workId, self->workFlags, self->workState);
        callCount++;
    }
#endif

    fn_Component_Serialize(self, stream);

    uint8_t serializerBuf[24];
    fn_Serializer_Ctor(serializerBuf, stream, 3);

    // Always serialize workId
    fn_Serializer_Serialize(serializerBuf, &self->workId, nullptr);

    // Legacy: version < 3 had a bool field (no longer used)
    int version = fn_Serializer_GetVersion(serializerBuf);
    if (version < 3) {
        bool legacyBool = false;
        fn_Serializer_Serialize(serializerBuf, (int*)&legacyBool, nullptr);
    }

    // Version >= 1: serialize workState
    version = fn_Serializer_GetVersion(serializerBuf);
    if (version >= 1) {
        fn_Serializer_Serialize(serializerBuf, &self->workState, nullptr);
    }

    // Version >= 2: serialize workFlags
    version = fn_Serializer_GetVersion(serializerBuf);
    if (version >= 2) {
        fn_Serializer_Serialize(serializerBuf, (int*)&self->workFlags, nullptr);
    }

    fn_Serializer_Dtor(serializerBuf);
}

// ============================================================
// Draw (0x14012e6f0)
// Compute the effective work capacity and store it at +0x30.
// Calls FUN_14012e5c0 which factors in building size and data.
// ============================================================
static void Reimpl_Draw(WorkComponent* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[WorkComponent::Draw] self=%p workId=%d\n",
               (void*)self, self->workId);
        callCount++;
    }
#endif

    int capacity = fn_ComputeWorkCapacity(self);
    self->workState = capacity;
}

// ============================================================
// OnCreate (0x14012e6c0)
// Check if work slot is available (via BST lookup using workId).
// If available, register it with the work slot manager.
// ============================================================
static void Reimpl_OnCreate(WorkComponent* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[WorkComponent::OnCreate] self=%p workId=%d\n",
               (void*)self, self->workId);
        callCount++;
    }
#endif

    bool available = fn_CheckWorkSlot(self);
    if (available) {
        fn_RegisterWorkSlot(self);
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallWorkComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    // IO::Serializer::Serialize(int) via IAT at [0x140152ac8]
    uintptr_t iatSerialize = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerialize) fn_Serializer_Serialize = *reinterpret_cast<fn_Serializer_Serialize_t*>(iatSerialize);

    // IO::Serializer::GetVersion via IAT at [0x140152a48]
    uintptr_t iatGetVer = ResolveAddr(Module::Exe, 0x140152a48);
    if (iatGetVer) fn_Serializer_GetVersion = *reinterpret_cast<fn_Serializer_GetVersion_t*>(iatGetVer);

#ifdef _DEBUG
    printf("[WorkComponent] Component_Serialize = %p\n", (void*)fn_Component_Serialize);
    printf("[WorkComponent] Serializer_Ctor = %p\n", (void*)fn_Serializer_Ctor);
    printf("[WorkComponent] Serializer_Serialize = %p\n", (void*)fn_Serializer_Serialize);
    printf("[WorkComponent] Serializer_GetVersion = %p\n", (void*)fn_Serializer_GetVersion);
#endif

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor ||
        !fn_Serializer_Serialize || !fn_Serializer_GetVersion) {
#ifdef _DEBUG
        printf("[WorkComponent] ERROR: failed to resolve Shutdown helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e710);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[WorkComponent] Shutdown hooked at %p (prologue=15)\n", (void*)target);
#endif
}

void RemoveWorkComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallWorkComponentDraw() {
    fn_ComputeWorkCapacity = reinterpret_cast<fn_ComputeWorkCapacity_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    if (!fn_ComputeWorkCapacity) {
#ifdef _DEBUG
        printf("[WorkComponent] ERROR: failed to resolve Draw helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e6f0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 14);
#ifdef _DEBUG
    printf("[WorkComponent] Draw hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveWorkComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallWorkComponentOnCreate() {
    fn_CheckWorkSlot = reinterpret_cast<fn_CheckWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012ead0));
    fn_RegisterWorkSlot = reinterpret_cast<fn_RegisterWorkSlot_t>(
        ResolveAddr(Module::Exe, 0x14012eb50));

    if (!fn_CheckWorkSlot || !fn_RegisterWorkSlot) {
#ifdef _DEBUG
        printf("[WorkComponent] ERROR: failed to resolve OnCreate helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14012e6c0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x20(4) + MOV RBX,RCX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 14);
#ifdef _DEBUG
    printf("[WorkComponent] OnCreate hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveWorkComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
