// engine/reimpl/storagecomponent.cpp
// Reimplementation of StorageComponent lifecycle methods from Application-x64.exe.
//
// StorageComponent is a direct Component subclass (0x40 bytes). PERF HOTSPOT.
// Three non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x14012c730) — complex versioned serialization of storage slots
//   - Draw (0x14012c640) — rebuild storage slot data from building template
//   - Serialize at vtable[4] is just RET 0x0 (no-op, not hooked)
//
// Strategy: Shutdown and Draw are complex functions with many sub-calls.
// Using passthrough via trampoline for safety — these are critical for
// save/load and could corrupt saves if reimplemented incorrectly.

#include "StorageComponent.h"
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

// ============================================================
// Original function typedefs for passthrough
// ============================================================
typedef void (*fn_StorageComponent_Shutdown_t)(StorageComponent* self, void* stream);
typedef void (*fn_StorageComponent_Draw_t)(StorageComponent* self);

// ============================================================
// Shutdown (0x14012c730) — PASSTHROUGH
// Complex versioned serialization: serializes slot count, iterates
// each slot serializing type info and item references, plus
// IsDerivedFrom checks and version-0 migration logic.
// Too critical for save/load to fully reimpl without extensive testing.
// ============================================================
static void Reimpl_Shutdown(StorageComponent* self, void* stream) {
    g_lastHook = "StorageComponent::Shutdown";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageComponent::Shutdown] self=%p stream=%p slotCount=%d (passthrough)\n",
               (void*)self, stream, self->slotCount);
        callCount++;
    }
#endif

    ((fn_StorageComponent_Shutdown_t)g_hookShutdown.trampoline)(self, stream);
}

// ============================================================
// Draw (0x14012c640) — PASSTHROUGH
// Rebuilds storage slot descriptors from building template data.
// Uses IsDerivedFrom check, entity component lookups, and
// iterates template storage entries to populate internal arrays.
// ============================================================
static void Reimpl_Draw(StorageComponent* self) {
    g_lastHook = "StorageComponent::Draw";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[StorageComponent::Draw] self=%p slotCount=%d (passthrough)\n",
               (void*)self, self->slotCount);
        callCount++;
    }
#endif

    ((fn_StorageComponent_Draw_t)g_hookDraw.trampoline)(self);
}

// ============================================================
// QueryCount (0x14012cf10) — REIMPLEMENTED (null-safe)
// Original exe function crashes (NULL+0x48) when a material type
// entity lacks RawMaterialDescription component. 44 xrefs in exe.
// Our inline fn_StorageComponent_QueryCount has a null guard.
// ============================================================
static HookInfo g_hookQueryCount;

static int Reimpl_QueryCount(StorageComponent* self) {
    g_lastHook = "StorageComponent::QueryCount";
    return fn_StorageComponent_QueryCount((void*)self);
}

// ============================================================
// Helper_c560 (0x14012c560) — REIMPLEMENTED (null-safe)
// Checks if (param_1[0x3C] & RawMaterialDescription[0x5C]) != 0.
// Same crash pattern: searches material type for RawMaterialDescription
// component, dereferences NULL+0x5C when not found. Returns 0 if missing.
// ============================================================
static HookInfo g_hookHelper_c560;

static uint32_t Reimpl_Helper_c560(void* param_1, void* param_2) {
    g_lastHook = "StorageComponent::Helper_c560";
    uintptr_t p2 = (uintptr_t)param_2;
    int compCount = *(int*)(p2 + 0x30);
    uintptr_t compArray = *(uintptr_t*)(p2 + 0x28);
    void* rawMatDesc = nullptr;
    for (int i = 0; i < compCount; i++) {
        void* comp = *(void**)(compArray + (uintptr_t)i * 8);
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (typeId == *g_TypeId_RawMaterialDescription) {
            if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription))
                rawMatDesc = comp;
            break;
        }
    }
    if (!rawMatDesc) return 0;
    uint32_t flags1 = *(uint32_t*)((uintptr_t)param_1 + 0x3C);
    uint32_t flags2 = *(uint32_t*)((uintptr_t)rawMatDesc + 0x5C);
    return (flags1 & flags2) != 0 ? 1 : 0;
}

// ============================================================
// Install / Remove
// ============================================================

void InstallStorageComponentHelper_c560() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012c560);
    // Prologue: MOV [RSP+8],RBX(5) + MOV [RSP+10],RBP(5) + MOV [RSP+18],RSI(5) = 15
    InstallHook(target, (void*)Reimpl_Helper_c560, &g_hookHelper_c560, 15);
#ifdef _DEBUG
    printf("[StorageComponent] Helper_c560 hooked at %p (reimpl, null-safe)\n", (void*)target);
#endif
}

void RemoveStorageComponentHelper_c560() { RemoveHook(&g_hookHelper_c560); }

void InstallStorageComponentQueryCount() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012cf10);
    // Prologue: MOV R11,RSP(3) + MOV [R11+8],RCX(4) + PUSH R12(2) + PUSH R14(2) + SUB RSP,0x38(4) = 15
    InstallHook(target, (void*)Reimpl_QueryCount, &g_hookQueryCount, 15);
#ifdef _DEBUG
    printf("[StorageComponent] QueryCount hooked at %p (reimpl, null-safe)\n", (void*)target);
#endif
}

void RemoveStorageComponentQueryCount() { RemoveHook(&g_hookQueryCount); }

void InstallStorageComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012c730);
    // Prologue: PUSH RDI(2) + SUB RSP,0x40(4) + MOV qword,-0x2(9) = 15
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 15);
#ifdef _DEBUG
    printf("[StorageComponent] Shutdown hooked at %p (prologue=15, passthrough)\n", (void*)target);
#endif
}

void RemoveStorageComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallStorageComponentDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14012c640);
    // Prologue: MOV [RSP+0x18],RBX(5) + PUSH RBP(1) + PUSH RDI(1) + PUSH R14(2) + SUB RSP,0x20(4) = 13
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[StorageComponent] Draw hooked at %p (prologue=13, passthrough)\n", (void*)target);
#endif
}

void RemoveStorageComponentDraw() { RemoveHook(&g_hookDraw); }
