// code/Game/Component/happinesscomponent.cpp
// Clean reimplementation of HappinessComponent lifecycle methods from Application-x64.exe.
//
// HappinessComponent (0x50 bytes) -- ControllerComponent subclass with shifted
// controller sub-object at +0x30 (own data byte at +0x28).
// Tracks citizen happiness state.
//
// Vtable at 0x1401595d0:
//   [3] Shutdown  = 0x14004aa80 -- serialize base + happinessFlag
//   [7] OnCreate  = 0x14004aa20 -- register with happiness list
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/happinesscomponent_funcs.h"
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

// IO::Serializer::Serialize(bool*) -- dispatch slot at 0x140152a58
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// FUN_140002cc0 -- pushes an InternalBasePtr into a dynamic array
typedef void (*fn_ArrayPushBasePtr_t)(void* arrayBase, void* internalBasePtr);
static fn_ArrayPushBasePtr_t s_fn_ArrayPushBasePtr = nullptr;

// FishingComponent_impl_a300 -- sets index on entity to given value
// Complex helper that looks up building type, finds linked component, sets map index.
typedef void (*fn_SetEntityIndex_t)(void* self, int16_t index);
static fn_SetEntityIndex_t s_fn_SetEntityIndex = nullptr;

// Global: DAT_1401c3dd8 -- pointer to game world/population manager
static void** s_pGameWorld = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    s_fn_ArrayPushBasePtr = reinterpret_cast<fn_ArrayPushBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    s_fn_SetEntityIndex = reinterpret_cast<fn_SetEntityIndex_t>(
        ResolveAddr(Module::Exe, 0x14004a300));

    s_pGameWorld = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// HappinessComponent::Shutdown (vtable slot [3], 0x14004aa80)
// ============================================================
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local_20, param_2, 0); ~Serializer(local_20)  // empty controller block
//   Serializer(local_30, param_2, 0)
//   Serialize(local_30, param_1+0x28, nullptr)  // happinessFlag as bool
//   ~Serializer(local_30)

void HappinessComponent_Shutdown(HappinessComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller compat)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Serialize happinessFlag (bool at +0x28)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    }
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer2, (bool*)&self->happinessFlag, nullptr);
    }
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// HappinessComponent::OnCreate (vtable slot [7], 0x14004aa20)
// ============================================================
// Ghidra decompilation:
//   if (param_1[0x28] != 0) {
//     InternalBasePtr::Ctor(local, param_1)
//     FUN_140002cc0( *(gameWorld + 0xF0) + 0x50, local )
//     FishingComponent_impl_a300(param_1, -1)
//     param_1[0x28] = 0
//   }

void HappinessComponent_OnCreate(HappinessComponent* self) {
    EnsureHelpers();

    if (self->happinessFlag != 0) {
        // Create InternalBasePtr from self (the component, passed as entity ref)
        uint8_t basePtr[24];
        if (fn_InternalBasePtr_Ctor) {
            fn_InternalBasePtr_Ctor(basePtr, self);
        }

        // Push to happiness list at (gameWorld->+0xF0)->+0x50
        if (s_fn_ArrayPushBasePtr && s_pGameWorld && *s_pGameWorld) {
            void* manager = *s_pGameWorld;
            void* happinessArray = reinterpret_cast<void*>(
                *reinterpret_cast<uintptr_t*>(
                    reinterpret_cast<uintptr_t>(manager) + 0xF0) + 0x50);
            s_fn_ArrayPushBasePtr(happinessArray, basePtr);
        }

        // Set entity index to -1
        if (s_fn_SetEntityIndex) {
            s_fn_SetEntityIndex(self, -1);
        }

        // Clear the flag
        self->happinessFlag = 0;
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HappinessComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HappinessComponent] Funcs resolved: Serialize=%p ArrayPush=%p SetIdx=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ArrayPushBasePtr,
           (void*)s_fn_SetEntityIndex);
#endif
}
