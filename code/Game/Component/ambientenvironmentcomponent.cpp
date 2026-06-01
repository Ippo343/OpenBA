// code/Game/Component/ambientenvironmentcomponent.cpp
// Clean reimplementation of AmbientEnvironmentComponent lifecycle methods.
//
// AmbientEnvironmentComponent (0x48 bytes) samples terrain grid around the entity
// to determine dominant biome, blends day/night ambient sounds based on time-of-day.
//
// Update (0x140043310, 222 instructions):
//   Very complex function deeply interacting with:
//     - World grid data structures (0x30-byte cell entries)
//     - Biome config tables (0x28-byte entries with bitmask matching)
//     - Audio scene system (AddEmitter, SetEmitterVolume, StopEffect3D)
//     - Entity component queries (InternalBasePtr, IsDerivedFrom)
//   Full reimplementation requires reimplementing audio scene + grid subsystems.
//   For now, delegates to the original function via resolved pointer.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/ambientenvironmentcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Original function pointer for Update (0x140043310)
// Too complex (222 instrs, grid+biome+audio subsystems) for clean reimpl at this stage.
typedef void (*fn_AmbientEnvUpdate_t)(void* self);
static fn_AmbientEnvUpdate_t s_fn_OriginalUpdate = nullptr;

static bool s_initialized = false;

// ============================================================
// AmbientEnvironmentComponent::Update (vtable slot [9], 0x140043310)
// ============================================================
void AmbientEnvironmentComponent_Update(AmbientEnvironmentComponent* self) {
    if (s_fn_OriginalUpdate)
        s_fn_OriginalUpdate(self);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AmbientEnvironmentComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_OriginalUpdate = reinterpret_cast<fn_AmbientEnvUpdate_t>(
        ResolveAddr(Module::Exe, 0x140043310));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AmbientEnvironmentComponent] Funcs resolved: OriginalUpdate=%p\n",
           (void*)s_fn_OriginalUpdate);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(AmbientEnvironmentComponent, 0x48, 0x88)
REGISTER_TYPE(AmbientEnvironmentComponent, "Component",
              &AmbientEnvironmentComponent_Factory, &AmbientEnvironmentComponent_CreateInPlace);
#endif // STANDALONE_EXE
