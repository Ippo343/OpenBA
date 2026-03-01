// engine/reimpl/clearareacomponent.cpp
// Reimplementation of ClearAreaComponent lifecycle methods from Application-x64.exe.
//
// ClearAreaComponent — ControllerComponent subclass (0x50 bytes).
// Has one extra field: clearState (uint32) at +0x48.
// Handles clearing terrain areas during construction/placement.
//
// Vtable at 0x1401626d0:
//   [3] Shutdown        = 0x140117EC0  <-- HOOKED (serialize with clearState)
//   All others: NO-OP

#include "ClearAreaComponent.h"
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

typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t fn_Serializer_SerializeInt = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;

// ============================================================
// Shutdown (0x140117EC0)
// Calls Component_Serialize, then:
//   1. Empty serializer block (version 0) — format compat stub
//   2. Serializer block with clearState at +0x48
// ============================================================
static void Reimpl_Shutdown(ClearAreaComponent* self, void* stream) {
    g_lastHook = "ClearAreaComponent::Shutdown";
    fn_Component_Serialize(self, stream);

    // Empty serializer block (format compatibility stub)
    uint8_t serializer1[16];
    fn_Serializer_Ctor(serializer1, stream, 0);
    fn_Serializer_Dtor(serializer1);

    // Serialize clearState
    uint8_t serializer2[16];
    fn_Serializer_Ctor(serializer2, stream, 0);
    fn_Serializer_SerializeInt(serializer2, (int*)&self->clearState, nullptr);
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
    uintptr_t pSerIntSlot = ResolveAddr(Module::Exe, 0x140152ac8);
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(pCtorSlot));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(pDtorSlot));
    fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(
        *reinterpret_cast<uintptr_t*>(pSerIntSlot));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor
        && fn_Serializer_SerializeInt;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallClearAreaComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[ClearAreaComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140117EC0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[ClearAreaComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveClearAreaComponentShutdown() { RemoveHook(&g_hookShutdown); }
