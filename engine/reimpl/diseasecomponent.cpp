// engine/reimpl/diseasecomponent.cpp
// Reimplementation of DiseaseComponent::Update from Application-x64.exe.
//
// The Update vtable entry at 0x140048b90 is a small 16-byte dispatch thunk:
//   if (field30 > 0) goto ActiveDisease (0x140048840)
//   else goto InfectionTick (0x140048610)
//
// field30 = disease duration remaining (ticks). When > 0, the citizen is sick.
// When <= 0, the citizen is healthy and rolls for seasonal infection.
//
// The two branches are complex (200+ instructions each) with deep calls into:
//   - InfectionTick: seasonal probability roll, grid neighbor checks
//   - ActiveDisease: countdown timer, cure checks (healer buildings),
//     periodic health damage, neighbor-spread via world grid sampling
//   - Helper functions: CheckCure (0x1400481b0), Cure (0x140048320),
//     Infect (0x1400483a0), grid queries, audio/UI notifications
//
// Strategy: Full reimplementation of the dispatch thunk + passthrough to
// the two branch bodies via direct function pointers (not trampolines).
// This lets us log and understand the disease state machine without needing
// to reimplement the entire entity query / world grid / audio subsystems.
//
// Original thunk: 0x140048b90 (16 bytes, CMP+JG+JMP)

#include "DiseaseComponent.h"
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
// Branch function pointers (resolved at install time)
// These are the two bodies that the original thunk dispatches to.
// ============================================================

// DiseaseComponent_InfectionTick(this, deltaTime) — healthy citizen path
// Seasonal infection probability roll, checks month range, grid neighbor query.
typedef void (*fn_InfectionTick_t)(DiseaseComponent* self, int64_t* deltaTime);
static fn_InfectionTick_t fn_InfectionTick = nullptr;

// DiseaseComponent_ActiveDisease(this, deltaTime) — sick citizen path
// Countdown timer, cure checks, periodic damage + spread.
typedef void (*fn_ActiveDisease_t)(DiseaseComponent* self, int64_t* deltaTime);
static fn_ActiveDisease_t fn_ActiveDisease = nullptr;

// ============================================================
// Update (0x140048b90) — dispatch thunk
//
// Parameters: RCX = this (DiseaseComponent*), RDX = deltaTime (int64*)
//
// Original logic (16 bytes):
//   CMP qword ptr [RCX+0x30], 0x0
//   JG  ActiveDisease
//   JMP InfectionTick
// ============================================================
static void Reimpl_Update(DiseaseComponent* self, int64_t* deltaTime) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[DiseaseComponent::Update] self=%p duration=%lld dt=%lld\n",
               (void*)self,
               *reinterpret_cast<int64_t*>(&self->field30),
               deltaTime ? *deltaTime : -1);
        callCount++;
    }
#endif

    // Dispatch: field30 is the disease duration remaining (ticks).
    // Positive = currently sick (active disease path).
    // Zero or negative = healthy (infection tick / seasonal roll path).
    int64_t duration = *reinterpret_cast<int64_t*>(&self->field30);

    if (duration > 0) {
        // Citizen is sick — run active disease logic
        fn_ActiveDisease(self, deltaTime);
    } else {
        // Citizen is healthy — check for new infection
        fn_InfectionTick(self, deltaTime);
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDiseaseComponentUpdate() {
    // Resolve the two branch function bodies
    fn_InfectionTick = reinterpret_cast<fn_InfectionTick_t>(
        ResolveAddr(Module::Exe, 0x140048610));
    fn_ActiveDisease = reinterpret_cast<fn_ActiveDisease_t>(
        ResolveAddr(Module::Exe, 0x140048840));

    if (!fn_InfectionTick || !fn_ActiveDisease) {
#ifdef _DEBUG
        printf("[DiseaseComponent] ERROR: failed to resolve branch functions, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140048b90);
    // The thunk is exactly 16 bytes (CMP[5] + JG[6] + JMP[5]).
    // Both jumps are relative so the trampoline won't work, but we never call it.
    // 16 bytes > 12 minimum, ends on instruction boundary (followed by CC padding).
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 16);
#ifdef _DEBUG
    printf("[DiseaseComponent] Update hooked at %p (prologue=16)\n", (void*)target);
    printf("[DiseaseComponent]   InfectionTick = %p\n", (void*)fn_InfectionTick);
    printf("[DiseaseComponent]   ActiveDisease = %p\n", (void*)fn_ActiveDisease);
#endif
}

void RemoveDiseaseComponentUpdate() { RemoveHook(&g_hookUpdate); }
