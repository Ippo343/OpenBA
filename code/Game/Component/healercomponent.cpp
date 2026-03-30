// code/Game/Component/healercomponent.cpp
// Clean reimplementation of HealerComponent lifecycle methods from Application-x64.exe.
//
// HealerComponent (0x48 bytes) extends ControllerComponent with no additional fields.
// Constructor sets flags |= 0x2.
//
// Vtable at 0x140159730:
//   [3] Shutdown  = 0x14004aeb0 -- two empty serializer blocks
//   [7] OnCreate  = 0x14004ae70 -- registers with healer list
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/healercomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// FUN_140002cc0 -- pushes an InternalBasePtr into a dynamic array
typedef void (*fn_ArrayPushBasePtr_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayPushBasePtr_t s_fn_ArrayPushBasePtr = nullptr;

// Global: DAT_1401c3dd8 -- pointer to game world/population manager
static void** s_pWorldManager = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_fn_ArrayPushBasePtr = reinterpret_cast<fn_ArrayPushBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_pWorldManager = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// HealerComponent::Shutdown (vtable slot [3], 0x14004aeb0)
// ============================================================
// Same pattern as WellComponent: Component_Serialize + two empty serializer blocks.

void HealerComponent_Shutdown(HealerComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Two empty serializer blocks (component section + controller section)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// HealerComponent::OnCreate (vtable slot [7], 0x14004ae70)
// ============================================================
// Creates InternalBasePtr from owner, pushes to healer list at
// (manager+0xF0)+0x70.

void HealerComponent_OnCreate(HealerComponent* self) {
    EnsureHelpers();

    uint8_t basePtr[24];
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(basePtr, self->owner);
    }

    // Push to healer list at (manager + 0xF0) + 0x70
    if (s_fn_ArrayPushBasePtr && s_pWorldManager && *s_pWorldManager) {
        void* manager = *s_pWorldManager;
        void* healerArray = reinterpret_cast<void*>(
            *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(manager) + 0xF0) + 0x70);
        s_fn_ArrayPushBasePtr(healerArray, basePtr);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HealerComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HealerComponent] Funcs resolved: Serialize=%p ArrayPush=%p WorldMgr=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ArrayPushBasePtr,
           (void*)s_pWorldManager);
#endif
}
