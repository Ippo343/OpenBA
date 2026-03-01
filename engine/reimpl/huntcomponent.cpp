// engine/reimpl/huntcomponent.cpp
// Reimplementation of HuntComponent::Update from Application-x64.exe.
//
// Hunt progress timer: reads the terrain speed ratio from the entity's
// components, scales the delta time by that ratio, and accumulates
// the result into the hunt progress accumulator at +0x50.
//
// The terrain speed ratio = entityComponent(+0x60) / tileData(+0x30).
// If tileData(+0x30) is zero, it's lazily computed via FUN_14012e5c0.
//
// Original: 0x140122740

#include "HuntComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Helper function pointers (resolved at install time)
// ============================================================

// FUN_140002e10: Find component on entity by type (terrain/character component)
// Signature: InternalBasePtr* FindComponentByType1(entity, outPtr)
// RCX = entity (from owner->components), RDX = outPtr (InternalBasePtr*)
typedef void* (*fn_FindComponent1_t)(int64_t entity, void* outPtr);
static fn_FindComponent1_t fn_FindComponent1 = nullptr;

// FUN_140003050: Find component on entity by type (map tile component)
// Signature: InternalBasePtr* FindComponentByType2(entity, outPtr)
typedef void* (*fn_FindComponent2_t)(int64_t entity, void* outPtr);
static fn_FindComponent2_t fn_FindComponent2 = nullptr;

// FUN_14012e5c0: Compute terrain speed for a tile (cached at tile+0x30)
// Signature: int ComputeTerrainSpeed(tileData*)
typedef int (*fn_ComputeTerrainSpeed_t)(int64_t tileData);
static fn_ComputeTerrainSpeed_t fn_ComputeTerrainSpeed = nullptr;

// System::Time::AsSeconds — inline: (float)((double)ticks * _secondsPerTick)
// We inline this rather than resolving it.

// ============================================================
// Update (0x140122740)
// Computes hunt progress: accumulates delta time scaled by the
// terrain speed ratio (base speed / terrain speed).
//
// Parameters: RCX = this, RDX = Time (int64)
// ============================================================
static void Reimpl_Update(HuntComponent* self, int64_t deltaTime) {
    g_lastHook = "HuntComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[HuntComponent::Update] self=%p dt=%lld owner=%p accumulator=%lld\n",
               (void*)self, deltaTime, self->owner, (long long)self->huntData);
        callCount++;
    }
#endif

    // CheckPtr(this+0x10) — no-op in release, skip

    // Find the terrain/character component on our owner entity
    int64_t entityPtr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    int64_t terrainComponent = 0;
    fn_FindComponent1(entityPtr, &terrainComponent);

    // CheckPtr — no-op, skip
    // Read terrain base speed from component+0x60
    int terrainBaseSpeed = *reinterpret_cast<int*>(terrainComponent + 0x60);

    // CheckPtr — no-op, skip
    // CheckPtr(terrainComponent+0x10) — no-op, skip

    // Find the map tile component via terrainComponent's owner
    int64_t tileOwner = *reinterpret_cast<int64_t*>(terrainComponent + 0x10);
    int64_t tileComponentPtr = 0;
    void* result = fn_FindComponent2(tileOwner, &tileComponentPtr);

    // CheckPtr — no-op, skip
    // Dereference to get actual tile data
    int64_t tileData = *reinterpret_cast<int64_t*>(result);

    // Lazily compute terrain speed if not cached
    int terrainSpeed = *reinterpret_cast<int*>(tileData + 0x30);
    if (terrainSpeed == 0) {
        terrainSpeed = fn_ComputeTerrainSpeed(tileData);
        *reinterpret_cast<int*>(tileData + 0x30) = terrainSpeed;
    }

    // Convert delta time to seconds (inline Time::AsSeconds)
    float deltaSeconds = static_cast<float>(
        static_cast<double>(deltaTime) * (*g_Timer_secondsPerTick));

    // Scale by terrain speed ratio and convert back to ticks
    float scaledSeconds = deltaSeconds * (static_cast<float>(terrainBaseSpeed) /
                                           static_cast<float>(terrainSpeed));
    int64_t scaledTicks = static_cast<int64_t>(
        static_cast<double>(scaledSeconds) / (*g_Timer_secondsPerTick));

    // Accumulate into hunt progress at +0x50
    *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x50) += scaledTicks;
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHuntComponentUpdate() {
    // Resolve helper functions in the exe
    fn_FindComponent1 = reinterpret_cast<fn_FindComponent1_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    fn_FindComponent2 = reinterpret_cast<fn_FindComponent2_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    fn_ComputeTerrainSpeed = reinterpret_cast<fn_ComputeTerrainSpeed_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

#ifdef _DEBUG
    printf("[HuntComponent] FindComponent1      = %p\n", (void*)fn_FindComponent1);
    printf("[HuntComponent] FindComponent2      = %p\n", (void*)fn_FindComponent2);
    printf("[HuntComponent] ComputeTerrainSpeed = %p\n", (void*)fn_ComputeTerrainSpeed);
#endif

    if (!fn_FindComponent1 || !fn_FindComponent2 || !fn_ComputeTerrainSpeed) {
#ifdef _DEBUG
        printf("[HuntComponent] ERROR: failed to resolve helpers, skipping hook\n");
#endif
        return;
    }

    if (!g_Timer_secondsPerTick) {
#ifdef _DEBUG
        printf("[HuntComponent] ERROR: g_Timer_secondsPerTick not resolved, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140122740);
    // Prologue: MOV [RSP+0x10],RBX(5) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //           SUB RSP,0x20(4) = 12 bytes. Instruction boundary at 12.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[HuntComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveHuntComponentUpdate() { RemoveHook(&g_hookUpdate); }
