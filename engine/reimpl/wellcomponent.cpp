// engine/reimpl/wellcomponent.cpp
// Reimplementation of WellComponent lifecycle methods.
//
// WellComponent (0x48 bytes) extends ControllerComponent with no additional fields.
// Constructor sets flags |= 0x2.
//
// Vtable at 0x14015ae90 (12 slots):
//   +0x00 Dtor           = 0x14000fe50 (too small to hook)
//   +0x08 GetDescription = 0x14000fc70 (7 bytes, unhookable)
//   +0x10 Startup        = 0x14000fc60 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x14005cfa0 (HOOKED — two empty serializer blocks)
//   +0x20 Serialize      = 0x140118a50 (no-op)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140118820 (no-op)
//   +0x38 OnCreate       = 0x14005cf60 (HOOKED — registers with population manager)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)

#include "WellComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================

static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Local function pointers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// Object::InternalBasePtr::InternalBasePtr(this, Internal*) — construct a smart pointer
typedef void* (*fn_InternalBasePtr_Ctor_t)(void* self, void* internal);
static fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor = nullptr;

// FUN_140091ba0 — adds entity to a dynamic array (used by OnCreate to register with population)
typedef void (*fn_ArrayPushEntity_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayPushEntity_t fn_ArrayPushEntity = nullptr;

// Global: DAT_1401c3dd8 — pointer to game world/population manager
static void** g_pWorldManager = nullptr;

// ============================================================
// Shutdown (0x14005cfa0)
// Serializes component state. WellComponent has no own fields,
// but creates TWO empty serializer blocks (version 0).
// This likely represents two serializer "sections" for
// ControllerComponent compatibility (one for component, one for controller data).
//
// Prologue: PUSH RBX(1) + SUB RSP,0x40(4) + MOV RBX,RDX(3) + CALL(5) = 13
// ============================================================

static void Reimpl_Shutdown(WellComponent* self, void* stream) {
    g_lastHook = "WellComponent::Shutdown";
    fn_Component_Serialize(self, stream);

    // First empty serializer block (component section)
    uint8_t serializer1[16];
    fn_Serializer_Ctor(serializer1, stream, 0);
    fn_Serializer_Dtor(serializer1);

    // Second empty serializer block (controller section)
    uint8_t serializer2[16];
    fn_Serializer_Ctor(serializer2, stream, 0);
    fn_Serializer_Dtor(serializer2);
}

// ============================================================
// OnCreate (0x14005cf60)
// Registers the owning entity with the world/population manager.
// Creates an InternalBasePtr from the owner (this+0x10), then
// pushes it to manager+0xF0's array.
//
// Prologue: SUB RSP,0x28(4) + MOV RDX,RCX(3) + LEA RCX,[RSP+0x38](5) = 12
// ============================================================

static void Reimpl_OnCreate(WellComponent* self) {
    g_lastHook = "WellComponent::OnCreate";
    // Create InternalBasePtr on stack from owner entity (self+0x10 = owner)
    uint8_t basePtr[24];
    fn_InternalBasePtr_Ctor(basePtr, self->owner);

    // Push to world manager's water source array at +0xF0
    void* manager = *g_pWorldManager;
    void* waterArray = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(manager) + 0xF0);
    fn_ArrayPushEntity(waterArray, basePtr);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallWellComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[WellComponent] ERROR: Failed to resolve serializer functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14005cfa0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[WellComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveWellComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}

void InstallWellComponentOnCreate() {
    fn_InternalBasePtr_Ctor = reinterpret_cast<fn_InternalBasePtr_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a90)));
    fn_ArrayPushEntity = reinterpret_cast<fn_ArrayPushEntity_t>(
        ResolveAddr(Module::Exe, 0x140091ba0));
    g_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    if (!fn_InternalBasePtr_Ctor || !fn_ArrayPushEntity || !g_pWorldManager) {
#ifdef _DEBUG
        printf("[WellComponent] ERROR: Failed to resolve OnCreate functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14005cf60);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[WellComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}

void RemoveWellComponentOnCreate() {
    RemoveHook(&g_hookOnCreate);
}
