// code/Game/Component/animalcomponent.cpp
// Clean reimplementation of AnimalComponent::Update from Application-x64.exe.
//
// AnimalComponent manages individual wild/domesticated animals:
//   - Aging: accumulate age at +0x44, check death threshold at +0x48
//   - Death handling: remove entity and add to corpse list
//   - Wander timer: randomized movement interval based on config
//   - Behavior update: calls UpdateBehavior helper (animation scaling)
//   - Path finding: random destination within bounds (short[4] at +0x38),
//     terrain validation (type 0x02), pathfinding object creation (0x480 bytes),
//     max 100 attempts to find valid destination, re-finds habitat every 10 failures
//
// Update address: 0x140106420 (452 instructions)
// Struct size: 0x58, vtable at 0x140162110
//
// Due to the extreme complexity of this function (pathfinding, grid math,
// entity management, 0x480-byte MoveTo_Action allocation, terrain validation),
// this delegates to the original function via ResolveAddr.
//
// Sub-functions documented for future full reimplementation:
//   - AnimalComponent_UpdateBehavior (animation scale based on age/config)
//   - AnimalComponent_FindHabitat (spiral search for valid terrain type 0x02)
//   - AnimalComponent_SetBounds (compute wander bounds from habitat)
//   - AnimalComponent_SetWanderTimer (randomize movement timer + facing direction)
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/animalcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Original function pointer (delegate for complex function)
// ============================================================
typedef void (*fn_AnimalUpdate_Original_t)(void* self, int64_t* deltaTime);
static fn_AnimalUpdate_Original_t s_fn_AnimalUpdate_Original = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    s_fn_AnimalUpdate_Original = reinterpret_cast<fn_AnimalUpdate_Original_t>(
        ResolveAddr(Module::Exe, 0x140106420));

    s_helpersResolved = true;
}

// ============================================================
// AnimalComponent::Update (vtable slot [9], 0x140106420)
// ============================================================
// Delegates to original function. The decompilation is documented
// in the Ghidra project and in this file's header comment.
//
// High-level flow:
//   1. age += dt * agingRate; if (age >= deathAge) { kill animal; return; }
//   2. Get config via IsDerivedFrom(self->_unk18, AnimalDescTypeId)
//   3. If active (data40 != 0) and flags match:
//      - Manage wander timer at +0x50 (countdown or randomize new target)
//   4. Call UpdateBehavior (animation scale)
//   5. Find family component, check if any family members alive
//   6. Decrement movement timer at +0x30; if expired:
//      a. Get current position from character component
//      b. If out of bounds or random chance: find new random destination
//         - Loop up to 100 attempts: random point in bounds, validate terrain==0x02
//         - Every 10 failures: re-find habitat via FindHabitat
//      c. If valid destination found: allocate 0x480-byte MoveTo action, push to queue
//      d. Else: set wander timer for retry

void AnimalComponent_Update(AnimalComponent* self, int64_t* deltaTime) {
    EnsureHelpers();

    if (s_fn_AnimalUpdate_Original) {
        s_fn_AnimalUpdate_Original(self, deltaTime);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AnimalComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[AnimalComponent] Update delegates to original at %p\n",
           (void*)s_fn_AnimalUpdate_Original);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(AnimalComponent, 0x58, 0xC9)
REGISTER_TYPE(AnimalComponent, "Component",
              &AnimalComponent_Factory, &AnimalComponent_CreateInPlace);
#endif // STANDALONE_EXE
