// code/Game/Component/diseasecomponent.cpp
// Clean reimplementation of DiseaseComponent::Update from Application-x64.exe.
//
// The Update vtable entry at 0x140048b90 is a small 16-byte dispatch thunk:
//   if (field30 > 0) goto ActiveDisease (0x140048840)
//   else goto InfectionTick (0x140048610)
//
// field30 = disease duration remaining (ticks). When > 0, the citizen is sick.
// When <= 0, the citizen is healthy and rolls for seasonal infection.
//
// The two branches are complex (200+ instructions each) with deep calls into
// disease subsystems. Strategy: dispatch thunk reimplemented, branch bodies
// called via direct function pointers.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/diseasecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// DiseaseComponent_InfectionTick(this, deltaTime) -- healthy citizen path
// Seasonal infection probability roll, checks month range, grid neighbor query.
typedef void (*fn_InfectionTick_t)(DiseaseComponent* self, int64_t* deltaTime);
static fn_InfectionTick_t s_fn_InfectionTick = nullptr;

// DiseaseComponent_ActiveDisease(this, deltaTime) -- sick citizen path
// Countdown timer, cure checks, periodic damage + spread.
typedef void (*fn_ActiveDisease_t)(DiseaseComponent* self, int64_t* deltaTime);
static fn_ActiveDisease_t s_fn_ActiveDisease = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_InfectionTick = reinterpret_cast<fn_InfectionTick_t>(
        ResolveAddr(Module::Exe, 0x140048610));
    s_fn_ActiveDisease = reinterpret_cast<fn_ActiveDisease_t>(
        ResolveAddr(Module::Exe, 0x140048840));

    s_initialized = true;
}

// ============================================================
// DiseaseComponent::Update (vtable slot [9], 0x140048b90)
// ============================================================
// Dispatch thunk: field30 is the disease duration remaining (ticks).
// Positive = currently sick (active disease path).
// Zero or negative = healthy (infection tick / seasonal roll path).
//
// Original logic (16 bytes):
//   CMP qword ptr [RCX+0x30], 0x0
//   JG  ActiveDisease
//   JMP InfectionTick

void DiseaseComponent_Update(DiseaseComponent* self, int64_t* deltaTime) {
    EnsureHelpers();

    int64_t duration = *reinterpret_cast<int64_t*>(&self->field30);

    if (duration > 0) {
        // Citizen is sick -- run active disease logic
        if (s_fn_ActiveDisease) {
            s_fn_ActiveDisease(self, deltaTime);
        }
    } else {
        // Citizen is healthy -- check for new infection
        if (s_fn_InfectionTick) {
            s_fn_InfectionTick(self, deltaTime);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void DiseaseComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[DiseaseComponent] Funcs resolved: InfectionTick=%p ActiveDisease=%p\n",
           (void*)s_fn_InfectionTick, (void*)s_fn_ActiveDisease);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(DiseaseComponent, 0x58, 0x8B)
REGISTER_TYPE(DiseaseComponent, "Component",
              &DiseaseComponent_Factory, &DiseaseComponent_CreateInPlace);
#endif // STANDALONE_EXE
