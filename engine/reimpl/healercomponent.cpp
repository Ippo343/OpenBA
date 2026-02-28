// engine/reimpl/healercomponent.cpp
// Reimplementation of HealerComponent lifecycle methods.
//
// HealerComponent (0x48 bytes) extends ControllerComponent with no additional fields.
// Constructor sets flags |= 0x2.
//
// Vtable at 0x140159730 (12 slots):
//   +0x00 Dtor           = 0x140006660 (too small to hook)
//   +0x08 GetDescription = 0x140006480 (7 bytes, unhookable)
//   +0x10 Startup        = 0x140006470 (7 bytes, unhookable)
//   +0x18 Shutdown       = 0x14004aeb0 (HOOKED — two empty serializer blocks)
//   +0x20 Serialize      = 0x140118a50 (no-op)
//   +0x28 Slot5          = 0x140118a60 (no-op)
//   +0x30 Draw           = 0x140118820 (no-op)
//   +0x38 OnCreate       = 0x14004ae70 (HOOKED — registers with healer list)
//   +0x40 OnRemove       = 0x140118a70 (no-op)
//   +0x48 Update         = 0x140118a80 (no-op)
//   +0x50 Slot10         = 0x140118a90 (no-op)
//   +0x58 OnUpdate2      = 0x140118aa0 (no-op)

#include "HealerComponent.h"
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

typedef void* (*fn_InternalBasePtr_Ctor_t)(void* self, void* internal);
static fn_InternalBasePtr_Ctor_t fn_InternalBasePtr_Ctor = nullptr;

// FUN_140002cc0 — pushes an InternalBasePtr into a dynamic array
typedef void (*fn_ArrayPushBasePtr_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayPushBasePtr_t fn_ArrayPushBasePtr = nullptr;

// Global: DAT_1401c3dd8 — pointer to game world/population manager
static void** g_pWorldManager = nullptr;

// ============================================================
// Shutdown (0x14004aeb0)
// Same pattern as WellComponent: Component_Serialize + two empty serializer blocks.
//
// Prologue: PUSH RBX(1) + SUB RSP,0x40(4) + MOV RBX,RDX(3) + CALL(5) = 13
// ============================================================

static void Reimpl_Shutdown(HealerComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    // Two empty serializer blocks (component section + controller section)
    uint8_t serializer1[16];
    fn_Serializer_Ctor(serializer1, stream, 0);
    fn_Serializer_Dtor(serializer1);

    uint8_t serializer2[16];
    fn_Serializer_Ctor(serializer2, stream, 0);
    fn_Serializer_Dtor(serializer2);
}

// ============================================================
// OnCreate (0x14004ae70)
// Registers the owning entity with the healer list in the population manager.
// Creates InternalBasePtr from owner, pushes to (manager+0xF0)+0x70.
//
// Decompiled:
//   InternalBasePtr::InternalBasePtr(local, param_1)
//   FUN_140002cc0( *(DAT_1401c3dd8 + 0xF0) + 0x70, local )
//
// Prologue: SUB RSP,0x28(4) + MOV RDX,RCX(3) + LEA RCX,[RSP+0x38](5) = 12
// ============================================================

static void Reimpl_OnCreate(HealerComponent* self) {
    uint8_t basePtr[24];
    fn_InternalBasePtr_Ctor(basePtr, self->owner);

    // Push to healer list at (manager + 0xF0) + 0x70
    void* manager = *g_pWorldManager;
    void* healerArray = reinterpret_cast<void*>(
        *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(manager) + 0xF0) + 0x70);
    fn_ArrayPushBasePtr(healerArray, basePtr);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallHealerComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[HealerComponent] ERROR: Failed to resolve serializer functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004aeb0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[HealerComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}

void RemoveHealerComponentShutdown() {
    RemoveHook(&g_hookShutdown);
}

void InstallHealerComponentOnCreate() {
    fn_InternalBasePtr_Ctor = reinterpret_cast<fn_InternalBasePtr_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152a90)));
    fn_ArrayPushBasePtr = reinterpret_cast<fn_ArrayPushBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    g_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    if (!fn_InternalBasePtr_Ctor || !fn_ArrayPushBasePtr || !g_pWorldManager) {
#ifdef _DEBUG
        printf("[HealerComponent] ERROR: Failed to resolve OnCreate functions\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004ae70);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[HealerComponent] OnCreate hooked at %p\n", (void*)target);
#endif
}

void RemoveHealerComponentOnCreate() {
    RemoveHook(&g_hookOnCreate);
}
