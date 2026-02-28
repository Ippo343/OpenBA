// engine/reimpl/clearresourcecomponent.cpp
// Reimplementation of ClearResourceComponent lifecycle methods from Application-x64.exe.
//
// ClearResourceComponent — ControllerComponent subclass (0x48 bytes, no extra fields).
// Handles clearing resources from tiles during construction.
//
// Vtable at 0x140162840:
//   [3] Shutdown        = 0x1401185E0  <-- HOOKED (serialize with 2 empty serializer blocks)
//   All others: NO-OP (Draw=base, OnCreate=base, OnRemove, Update, Slot10, OnUpdate2)

#include "ClearResourceComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers
// ============================================================
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x1401185E0)
// Calls Component_Serialize, then creates and immediately destroys two
// empty Serializer blocks (version 0). This pattern preserves save
// format compatibility for components that once had serialized fields.
// ============================================================
static void Reimpl_Shutdown(ClearResourceComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    uint8_t serializer1[16];
    fn_Serializer_Ctor(serializer1, stream, 0);
    fn_Serializer_Dtor(serializer1);

    uint8_t serializer2[16];
    fn_Serializer_Ctor(serializer2, stream, 0);
    fn_Serializer_Dtor(serializer2);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t pCtorSlot = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t pDtorSlot = ResolveAddr(Module::Exe, 0x140152ad0);
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(pCtorSlot));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(pDtorSlot));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallClearResourceComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[ClearResourceComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401185E0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[ClearResourceComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveClearResourceComponentShutdown() { RemoveHook(&g_hookShutdown); }
