// engine/reimpl/animal.cpp
// Reimplementation of AnimalComponent::Update from Application-x64.exe.
//
// AnimalComponent manages individual wild/domesticated animals:
//   - Aging: accumulate age at +0x44, check death threshold at +0x48
//   - Death handling: remove entity and add to corpse list
//   - Wander timer: randomized movement interval based on config
//   - Behavior update: calls UpdateBehavior helper
//   - Path finding: random destination within bounds (short[4] at +0x38),
//     terrain validation (type 0x02), pathfinding object creation (0x480 bytes),
//     max 100 attempts to find valid destination, re-finds habitat every 10 failures
//
// Update address: 0x140106420  (452 instructions — PASSTHROUGH WRAPPER)
// Struct size: 0x58, vtable at 0x140162110
//
// This function is extremely complex (pathfinding, grid math, entity management).
// Implemented as a passthrough wrapper with debug logging for observability.
// Full reimplementation deferred until all helper functions are better understood.

#include "AnimalComponent.h"
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
// Original function typedef
// ============================================================

typedef void (*AnimalComponent_Update_t)(void* self, int64_t* deltaTime);

// ============================================================
// AnimalComponent::Update — passthrough wrapper
// ============================================================

static void Reimpl_Update(void* self, int64_t* deltaTime) {
    g_lastHook = "AnimalComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        uint8_t* comp = (uint8_t*)self;
        float age = *(float*)(comp + 0x44);
        float deathAge = *(float*)(comp + 0x48);
        uint8_t active = *(uint8_t*)(comp + 0x40);
        printf("[AnimalComponent::Update] self=%p age=%.2f/%.2f active=%d dt=%lld\n",
               self, age, deathAge, active, *deltaTime);
        callCount++;
    }
#endif

    // Call original via trampoline
    ((AnimalComponent_Update_t)g_hookUpdate.trampoline)(self, deltaTime);
}

// ============================================================
// Install / Remove
// ============================================================

void InstallAnimalComponentUpdate() {
    // VTable hook: MOV RAX,RSP in prologue breaks with RSP compensation.
    // vtable at 0x140162110, Update = slot 9 (+0x48)
    InstallHookVTable(0x140162110, 9, (void*)Reimpl_Update, &g_hookUpdate);
    RegisterHookInfo("AnimalComponent::Update", &g_hookUpdate);
#ifdef _DEBUG
    printf("[AnimalComponent] Update vtable-hooked (original=%p)\n", g_hookUpdate.trampoline);
#endif
}

void RemoveAnimalComponentUpdate() {
    RemoveHookVTable(&g_hookUpdate);
}
