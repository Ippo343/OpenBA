// code/Game/Component/spawncomponent.cpp
// Clean reimplementation of SpawnComponent::Update from Application-x64.exe.
//
// Periodic entity spawning system. For each spawn entry in the resource data:
//   1. Check if current growth rate falls within the entry's [minGrowth, maxGrowth] range.
//   2. If in range, accumulate delta time into per-entry timer.
//   3. Look up spawn interval from linked template data.
//   4. When accumulated time exceeds interval, subtract interval and call
//      the spawn function. Repeat while timer still exceeds interval.
//
// Original: 0x140055ad0 (114 instructions)
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/spawncomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

static constexpr float SECS_PER_SEASON = 300.0f;

// FUN_1400052c0 — gets linked object from entity owner
typedef void* (*fn_GetLinkedObject_t)(void* entity, void* outPtr);
static fn_GetLinkedObject_t s_fn_GetLinkedObject = nullptr;

// FUN_140055780 — performs actual spawn operation
typedef void (*fn_DoSpawn_t)(void* self, void* resourceData, int entryIndex, char flag);
static fn_DoSpawn_t s_fn_DoSpawn = nullptr;

// Type IDs
static uint32_t* s_pSpawnResourceTypeId = nullptr;   // DAT_1401c4918
static uint32_t* s_pSpawnTemplateTypeId = nullptr;    // DAT_1401c77e8

static bool s_initialized = false;

// ============================================================
// SpawnComponent::Update (0x140055ad0)
// ============================================================
void SpawnComponent_Update(SpawnComponent* self, int64_t* pDeltaTime) {
    // CheckPtr on owner IBP, then get linked object
    uintptr_t ownerIBP = (uintptr_t)self + 0x10;
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr((void*)ownerIBP);
    uintptr_t linkedObjPtr = 0;
    if (s_fn_GetLinkedObject)
        s_fn_GetLinkedObject(*(void**)ownerIBP, &linkedObjPtr);

    // Get resource data via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    if (!fn_IsDerivedFrom || !s_pSpawnResourceTypeId) return;
    bool isDerived = fn_IsDerivedFrom(baseObj, *s_pSpawnResourceTypeId);
    void* resourceData = isDerived ? baseObj : nullptr;
    if (!resourceData) return;

    // CheckPtr on linked object IBP before reading growth
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(&linkedObjPtr);
    float currentGrowth = *(float*)(linkedObjPtr + 0x28);

    // Get spawn entry count from resourceData+0x28
    int entryCount = *(int*)((uintptr_t)resourceData + 0x28);
    if (entryCount <= 0) return;

    uintptr_t entryArrayBase = *(uintptr_t*)((uintptr_t)resourceData + 0x20);

    for (int i = 0; i < entryCount; i++) {
        uintptr_t entry = entryArrayBase + (uintptr_t)i * 0x10;

        float minGrowth = *(float*)(entry + 0x08);
        float maxGrowth = *(float*)(entry + 0x0C);

        if (currentGrowth < minGrowth || currentGrowth > maxGrowth)
            continue;

        // Accumulate delta time
        int64_t byteOffset = (int64_t)i * 8;
        int64_t* pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)self + 0x28) + byteOffset);
        *pTimer += *pDeltaTime;

        // Get spawn template data from the entry
        uintptr_t templateData = *(uintptr_t*)entry;

        // Search template's component list for the spawn interval component
        int componentCount = *(int*)(templateData + 0x30);
        void* spawnIntervalComp = nullptr;

        for (int j = 0; j < componentCount; j++) {
            void* comp = *(void**)(*(uintptr_t*)(templateData + 0x28) + (uintptr_t)j * 8);
            typedef uint32_t (*fn_GetTypeId_t)(void*);
            uint32_t typeId = ((fn_GetTypeId_t)(*(void***)(comp))[2])(comp);

            if (typeId == *s_pSpawnTemplateTypeId) {
                if (fn_IsDerivedFrom(comp, *s_pSpawnTemplateTypeId)) {
                    spawnIntervalComp = comp;
                }
                break;
            }
        }

        if (!spawnIntervalComp) continue;

        // Convert spawn interval to ticks
        float intervalSecs = *(float*)((uintptr_t)spawnIntervalComp + 0x28) * SECS_PER_SEASON;
        int64_t intervalTicks = (int64_t)((double)intervalSecs / *g_Timer_secondsPerTick);

        // Re-read timer pointer (DoSpawn may realloc)
        pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)self + 0x28) + byteOffset);
        while (intervalTicks > 0 && *pTimer >= intervalTicks) {
            *pTimer -= intervalTicks;
            if (s_fn_DoSpawn)
                s_fn_DoSpawn(self, resourceData, i, 0);
            pTimer = (int64_t*)(*(uintptr_t*)((uintptr_t)self + 0x28) + byteOffset);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void SpawnComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_GetLinkedObject = reinterpret_cast<fn_GetLinkedObject_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_DoSpawn = reinterpret_cast<fn_DoSpawn_t>(
        ResolveAddr(Module::Exe, 0x140055780));
    s_pSpawnResourceTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4918));
    s_pSpawnTemplateTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c77e8));

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x38 (SpawnComponent_Factory MemAlloc), flags 0x88 (Ctor 0x140056030:
// Component base sets +0x20=0x80, leaf OR 0x88 => 0x88).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(SpawnComponent, 0x38, 0x88)
REGISTER_TYPE(SpawnComponent, "Component",
              &SpawnComponent_Factory, &SpawnComponent_CreateInPlace);
#endif // STANDALONE_EXE
