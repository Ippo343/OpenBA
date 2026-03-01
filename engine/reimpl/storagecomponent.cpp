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
// Install / Remove
// ============================================================
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
