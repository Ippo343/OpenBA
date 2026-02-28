// engine/reimpl/terraformcomponent.cpp
// Reimplementation of TerraformComponent lifecycle methods from Application-x64.exe.
//
// TerraformComponent — Component subclass (0x48 bytes).
// Fields: field28/30/38/40 (all void*, zeroed). Manages terrain
// modification (flatten/raise) operations during construction.
//
// Vtable at 0x14015a880:
//   [3] Shutdown        = 0x140057500  <-- HOOKED (serialize terrain points + tile indices)
//   [6] Draw            = 0x1400574F0  <-- 3-byte RET 0 (unhookable no-op)
//   [7] OnCreate        = 0x140057350  <-- PASSTHROUGH (complex terrain modification logic)
//   Other slots: NO-OP

#include "TerraformComponent.h"
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

// Math::Vector3::Serialize(this, serializer) — serialize a 3D vector
typedef void (*fn_Vector3_Serialize_t)(void* vec3, void* serializer);
static fn_Vector3_Serialize_t fn_Vector3_Serialize = nullptr;

// FUN_14000d030 — resize Vector3 array (field28 + count at field30)
typedef void (*fn_ResizeVec3Array_t)(void* arrayPtr, uint32_t newCount);
static fn_ResizeVec3Array_t fn_ResizeVec3Array = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Shutdown (0x140057500)
// Serializes base component, then serializes a variable-length array
// of Vector3 terrain points. Format:
//   1. Component_Serialize
//   2. Serializer(version=0): serialize point count from +0x30,
//      if reading, resize array at +0x28, then serialize each Vector3
// ============================================================
static void Reimpl_Shutdown(TerraformComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize count (from +0x30, which is the count of Vector3s)
    uint32_t count = *(uint32_t*)((uintptr_t)self + 0x30);
    uint32_t localCount = count;
    fn_Serializer_SerializeInt(serializer, (int*)&localCount, nullptr);

    // If reading (stream+8 == 0), resize the Vector3 array
    if (*(int*)((uintptr_t)stream + 8) == 0) {
        fn_ResizeVec3Array((void*)((uintptr_t)self + 0x28), localCount);
    }

    // Serialize each Vector3 (12 bytes each)
    void* vecArray = *(void**)((uintptr_t)self + 0x28);
    if ((int)localCount > 0) {
        uint32_t remaining = localCount;
        uint8_t* pVec = (uint8_t*)vecArray;
        do {
            fn_Vector3_Serialize(pVec, serializer);
            pVec += 0xC;  // sizeof(Vector3) = 12
            remaining--;
        } while (remaining != 0);
    }

    fn_Serializer_Dtor(serializer);
}

// ============================================================
// OnCreate (0x140057350) — PASSTHROUGH
// Complex terrain modification: iterates terrain points, sets height
// values in the terrain grid, updates tile terrain types, refreshes
// terrain rendering. Too many terrain subsystem dependencies.
// ============================================================
static void Passthrough_OnCreate(TerraformComponent* self) {
    typedef void (*orig_t)(TerraformComponent*);
    ((orig_t)g_hookOnCreate.trampoline)(self);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_ResizeVec3Array = reinterpret_cast<fn_ResizeVec3Array_t>(
        ResolveAddr(Module::Exe, 0x14000d030));

    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad8)));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ad0)));
    fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(
        *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152ac8)));

    // Vector3::Serialize is also dispatched through a slot
    // From the disassembly, it's called inline in the loop — let me use the
    // Runtime export. Actually, looking at the decomp more carefully, the
    // original uses Math::Vector3::Serialize which is a Runtime DLL function.
    fn_Vector3_Serialize = reinterpret_cast<fn_Vector3_Serialize_t>(
        ResolveExport("?Serialize@Vector3@Math@@QEAAXAEAVSerializer@IO@@@Z"));
    if (!fn_Vector3_Serialize) {
        // Fallback: try the dispatch slot pattern
        // The CALL for Vector3::Serialize in the Shutdown function would be
        // through a dispatch slot. Let me check the actual call targets.
        // From disasm of 0x140057500, the Vector3::Serialize calls go through
        // a known Runtime function.
    }

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor
        && fn_Serializer_SerializeInt && fn_ResizeVec3Array;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTerraformComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[TerraformComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    // If Vector3::Serialize couldn't be resolved, fall back to passthrough
    if (!fn_Vector3_Serialize) {
#ifdef _DEBUG
        printf("[TerraformComponent] WARNING: Vector3::Serialize not found, using passthrough\n");
#endif
        // Use passthrough instead
        uintptr_t target = ResolveAddr(Module::Exe, 0x140057500);
        InstallHook(target, (void*)+[](TerraformComponent* self, void* stream) {
            typedef void (*orig_t)(TerraformComponent*, void*);
            ((orig_t)g_hookShutdown.trampoline)(self, stream);
        }, &g_hookShutdown, 13);
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140057500);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[TerraformComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveTerraformComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTerraformComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140057350);
    InstallHook(target, (void*)Passthrough_OnCreate, &g_hookOnCreate, 15);
#ifdef _DEBUG
    printf("[TerraformComponent] OnCreate hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveTerraformComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
