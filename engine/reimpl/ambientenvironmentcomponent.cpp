// engine/reimpl/ambientenvironmentcomponent.cpp
// Reimplementation of AmbientEnvironmentComponent::Update from Application-x64.exe.
//
// This component samples the terrain grid around the entity to determine the
// dominant biome, then blends day/night ambient sound sets based on time-of-day.
//
// Update logic (0x140043310, 222 instructions):
//   1. Get world grid singleton (DAT_1401c3dd8+0xf8)
//   2. IsDerivedFrom check on _unk18 (config component at DAT_1401c3f08)
//   3. Read sampling dimensions from config (+0x28 = count)
//   4. Allocate temp int array for biome vote counts
//   5. Double-loop over grid cells at (self+0x28..+0x34 offsets):
//      - For each cell, read terrain type byte from grid data
//      - Match against config biome table (+0x20), accumulate vote counts
//   6. Find dominant biome (max vote count)
//   7. Compute day/night blend factor from time-of-day:
//      factor = clamp((currentTime - config+0x30) / (config+0x34 - config+0x30), 0, 1)
//   8. Look up audio component on entity (type DAT_1401c3e60)
//   9. Apply two sound sets via FUN_140042a10:
//      - Slot 0: day sound from biome table, volume = factor
//      - Slot 1: night sound from biome table, volume = 1.0 - factor
//   10. Free temp array
//
// Strategy: Passthrough wrapper. The function deeply interacts with:
//   - World grid data structures (0x30-byte cell entries)
//   - Biome config tables (0x28-byte entries with bitmask matching)
//   - Audio scene system (AddEmitter, SetEmitterVolume, StopEffect3D)
//   - Entity component queries (InternalBasePtr, IsDerivedFrom)
// Full reimplementation requires reimplementing the audio scene + grid subsystems.
//
// Original: 0x140043310

#include "AmbientEnvironmentComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Component/ambientenvironmentcomponent_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookUpdate;

// ============================================================
// Update (0x140043310) -- delegates to clean code
// ============================================================
static void Reimpl_Update(AmbientEnvironmentComponent* self) {
    g_lastHook = "AmbientEnvironmentComponent::Update";
    // Must call trampoline directly — AmbientEnvironmentComponent_Update is a
    // passthrough that resolves the hooked address via ResolveAddr (infinite recursion).
    typedef void (*OrigUpdate_t)(AmbientEnvironmentComponent*);
    ((OrigUpdate_t)g_hookUpdate.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallAmbientEnvironmentComponentUpdate() {
    AmbientEnvironmentComponent_InitFuncs();
    uintptr_t target = ResolveAddr(Module::Exe, 0x140043310);
    // Prologue: PUSH RBX(2) + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) +
    //   PUSH R12(2) + PUSH R13(2) + PUSH R14(2) + PUSH R15(2) +
    //   SUB RSP,0x68(4) = 17 bytes
    // All register saves, no RIP-relative. Instruction boundary at 17.
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 17);
#ifdef _DEBUG
    printf("[AmbientEnvironmentComponent] Update hooked at %p (prologue=17)\n", (void*)target);
#endif
}

void RemoveAmbientEnvironmentComponentUpdate() { RemoveHook(&g_hookUpdate); }
