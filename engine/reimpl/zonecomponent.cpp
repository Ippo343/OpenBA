// engine/reimpl/zonecomponent.cpp
// Reimplementation of ZoneComponent lifecycle methods from Application-x64.exe.
//
// ZoneComponent — simplest component (0x28 bytes, same size as Component base).
// Manages zone rendering in the world map. No additional fields beyond Component.
//
// Vtable at 0x140164e30:
//   [0] Dtor            = 0x14003A870
//   [1] GetDescription  = 0x14003A690
//   [2] Startup         = 0x14003A680
//   [3] Shutdown        = 0x1401302F0  <-- HOOKED (serialize + zone registration)
//   [6] Draw            = 0x1401301F0  <-- PASSTHROUGH (complex zone rendering)
//   [7] OnCreate        = 0x140130080  <-- PASSTHROUGH (complex zone creation + tree walk)
//   Other slots: NO-OP

#include "ZoneComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// Component_Serialize(this, stream) — direct call at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer — resolved through .rdata dispatch slots
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

typedef int (*fn_Serializer_GetVersion_t)(void* self);
static fn_Serializer_GetVersion_t fn_Serializer_GetVersion = nullptr;

// FUN_140088e20 — zone map registration (add entity to zone map)
typedef void (*fn_ZoneMapAdd_t)(void* zoneMap, void* entityPtr);
static fn_ZoneMapAdd_t fn_ZoneMapAdd = nullptr;

// Game state singleton (DAT_1401c3dd8)
static void** g_pGameState = nullptr;

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Shutdown (0x1401302F0)
// Serializes component, then conditionally adds entity to zone map.
// If serializer version==0 and stream is reading (mode==0),
// registers the owning entity into the zone map at GameState+0xF0.
// ============================================================
static void Reimpl_Shutdown(ZoneComponent* self, void* stream) {
    fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    fn_Serializer_Ctor(serializer, stream, 1);

    int version = fn_Serializer_GetVersion(serializer);
    if (version == 0 && *(int*)((uintptr_t)stream + 8) == 0) {
        void* entityPtr = self->owner;
        void* zoneMap = *(void**)((uintptr_t)*g_pGameState + 0xF0);
        fn_ZoneMapAdd(zoneMap, &entityPtr);
    }

    fn_Serializer_Dtor(serializer);
}

// ============================================================
// Draw (0x1401301F0) — PASSTHROUGH
// Complex zone rendering with IsDerivedFrom checks, coordinate
// extraction from entity data, and calls to zone rendering system.
// ============================================================
static void Passthrough_Draw(ZoneComponent* self) {
    typedef void (*orig_t)(ZoneComponent*);
    ((orig_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// OnCreate (0x140130080) — PASSTHROUGH
// Complex zone creation: extracts entity coords, does IsDerivedFrom
// check, calls zone rendering setup, and walks a tree structure.
// ============================================================
static void Passthrough_OnCreate(ZoneComponent* self) {
    typedef void (*orig_t)(ZoneComponent*);
    ((orig_t)g_hookOnCreate.trampoline)(self);
}

// ============================================================
// Resolve helpers
// ============================================================
static bool ResolveHelpers() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    fn_ZoneMapAdd = reinterpret_cast<fn_ZoneMapAdd_t>(
        ResolveAddr(Module::Exe, 0x140088e20));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    // IO::Serializer dispatched through .rdata slots
    uintptr_t pCtorSlot = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t pDtorSlot = ResolveAddr(Module::Exe, 0x140152ad0);
    uintptr_t pGetVersionSlot = ResolveAddr(Module::Exe, 0x140152a48);
    fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(pCtorSlot));
    fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(pDtorSlot));
    fn_Serializer_GetVersion = reinterpret_cast<fn_Serializer_GetVersion_t>(
        *reinterpret_cast<uintptr_t*>(pGetVersionSlot));

    return fn_Component_Serialize && fn_Serializer_Ctor && fn_Serializer_Dtor
        && fn_Serializer_GetVersion && fn_ZoneMapAdd;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallZoneComponentShutdown() {
    if (!ResolveHelpers()) {
#ifdef _DEBUG
        printf("[ZoneComponent] ERROR: Failed to resolve helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x1401302F0);
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 13);
#ifdef _DEBUG
    printf("[ZoneComponent] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveZoneComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallZoneComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401301F0);
    InstallHook(target, (void*)Passthrough_Draw, &g_hookDraw, 12);
#ifdef _DEBUG
    printf("[ZoneComponent] Draw hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveZoneComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallZoneComponentOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140130080);
    InstallHook(target, (void*)Passthrough_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[ZoneComponent] OnCreate hooked at %p (passthrough)\n", (void*)target);
#endif
}
void RemoveZoneComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
