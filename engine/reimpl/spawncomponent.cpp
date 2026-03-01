// engine/reimpl/spawncomponent.cpp
// Reimplementation of SpawnComponent::Update from Application-x64.exe.
//
// Periodic entity spawning system. For each spawn entry in the resource data:
//   1. Check if current growth rate (at linked object +0x28) falls within
//      the entry's [minGrowth, maxGrowth] range.
//   2. If in range, accumulate delta time into per-entry timer.
//   3. Look up spawn interval from linked template data.
//   4. When accumulated time exceeds interval, subtract interval and call
//      the spawn function. Repeat while timer still exceeds interval.
//
// Original: 0x140055ad0 (114 instructions)

#include "SpawnComponent.h"
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
// Constants from exe .rdata
// ============================================================
// DAT_140172234 = 300.0f (seconds per season, used for interval conversion)
static constexpr float SECS_PER_SEASON = 300.0f;

// ============================================================
// Helper function pointers & globals (resolved at install time)
// ============================================================

// FUN_1400052c0 — gets the linked object from entity owner.
// Signature: InternalBasePtr* (this_entity, out_ptr)
typedef void* (*fn_GetLinkedObject_t)(void* entity, void* outPtr);
static fn_GetLinkedObject_t fn_GetLinkedObject = nullptr;

// FUN_140055780 — performs the actual spawn operation.
// Signature: void (SpawnComponent*, resourceData, entryIndex, flag)
typedef void (*fn_DoSpawn_t)(void* self, void* resourceData, int entryIndex, char flag);
static fn_DoSpawn_t fn_DoSpawn = nullptr;

// Object::Base::IsDerivedFrom — already in engine_funcs.h (fn_IsDerivedFrom)

// Type ID for spawn resource data (DAT_1401c4918)
static uint32_t* g_pSpawnResourceTypeId = nullptr;

// Type ID for spawn template component (DAT_1401c77e8)
static uint32_t* g_pSpawnTemplateTypeId = nullptr;

// Time::AsSeconds — inlined: (float)((double)ticks * *g_Timer_secondsPerTick)

// Time::Time(float) — inlined: ticks = (int64)(seconds / *g_Timer_secondsPerTick)

// ============================================================
// Update (0x140055ad0)
// Iterates spawn entries, accumulates time, spawns when interval exceeded.
//
// Parameters: RCX = this (SpawnComponent*), RDX = Time* (int64*)
// ============================================================
static void Reimpl_Update(SpawnComponent* self, int64_t* pDeltaTime) {
    g_lastHook = "SpawnComponent::Update";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[SpawnComponent::Update] self=%p dt=%lld\n",
               (void*)self, *pDeltaTime);
        callCount++;
    }
#endif

    // Get the linked object from owner entity (via FUN_1400052c0)
    // CheckPtr on owner (self+0x10) is a no-op, skip
    uintptr_t linkedObjPtr = 0;
    if (fn_GetLinkedObject) {
        fn_GetLinkedObject(*(void**)((uintptr_t)self + 0x10), &linkedObjPtr);
    }

    // Get resource data via IsDerivedFrom check on _unk18
    void* baseObj = self->_unk18;
    void* resourceData = nullptr;
    if (baseObj && fn_IsDerivedFrom && g_pSpawnResourceTypeId) {
        if (fn_IsDerivedFrom(baseObj, *g_pSpawnResourceTypeId)) {
            resourceData = baseObj;
        }
    }

    // CheckPtr on linkedObjPtr is a no-op, skip
    // Get current growth rate from linked object
    float currentGrowth = *(float*)(linkedObjPtr + 0x28);

    // Get spawn entry count from resourceData+0x28
    int entryCount = *(int*)((uintptr_t)resourceData + 0x28);
    if (entryCount <= 0) return;

    // Iterate spawn entries
    // Resource data layout at +0x20: pointer to array of 0x10-byte entries
    //   Each entry: +0x00 = pointer to template data
    //               +0x08 = float minGrowth
    //               +0x0C = float maxGrowth
    // Timer array at self->field28 (self+0x28): array of int64 timers, one per entry

    uintptr_t entryArrayBase = *(uintptr_t*)((uintptr_t)resourceData + 0x20);
    int64_t* timerArray = *(int64_t**)((uintptr_t)self + 0x28);

    for (int i = 0; i < entryCount; i++) {
        uintptr_t entry = entryArrayBase + (uintptr_t)i * 0x10;

        float minGrowth = *(float*)(entry + 0x08);
        float maxGrowth = *(float*)(entry + 0x0C);

        // Check if current growth is within [minGrowth, maxGrowth]
        if (currentGrowth < minGrowth || currentGrowth > maxGrowth) {
            continue;
        }

        // Accumulate delta time for this entry
        timerArray[i] += *pDeltaTime;

        // Get spawn template data from the entry
        uintptr_t templateData = *(uintptr_t*)entry;

        // Search template's component list for the spawn interval component
        // templateData+0x30 = count of components, templateData+0x28 = array of component ptrs
        int componentCount = *(int*)(templateData + 0x30);
        void* spawnIntervalComp = nullptr;

        for (int j = 0; j < componentCount; j++) {
            void* comp = *(void**)(*(uintptr_t*)(templateData + 0x28) + (uintptr_t)j * 8);
            // Call virtual GetDescription (vtable slot 2, offset +0x10) to get type ID
            typedef int (*fn_GetDescription_t)(void*);
            int typeId = ((fn_GetDescription_t)(*(void***)(comp))[2])(comp);

            if (g_pSpawnTemplateTypeId && typeId == (int)*g_pSpawnTemplateTypeId) {
                // Found the matching component, verify with IsDerivedFrom
                if (fn_IsDerivedFrom(comp, *g_pSpawnTemplateTypeId)) {
                    spawnIntervalComp = comp;
                }
                break;
            }
        }

        // Convert the spawn interval (seconds) to ticks
        // spawnIntervalComp+0x28 is float interval in game-time units
        float intervalSecs = *(float*)((uintptr_t)spawnIntervalComp + 0x28) * SECS_PER_SEASON;
        int64_t intervalTicks = (int64_t)((double)intervalSecs / *g_Timer_secondsPerTick);

        // While accumulated time exceeds interval, spawn and subtract
        while (timerArray[i] >= intervalTicks) {
            timerArray[i] -= intervalTicks;
            if (fn_DoSpawn) {
                fn_DoSpawn(self, resourceData, i, 0);
            }
        }
    }
}

// ============================================================
// Install / Remove
// ============================================================
void InstallSpawnComponentUpdate() {
    // Resolve helpers
    fn_GetLinkedObject = reinterpret_cast<fn_GetLinkedObject_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    fn_DoSpawn = reinterpret_cast<fn_DoSpawn_t>(
        ResolveAddr(Module::Exe, 0x140055780));

    // Spawn resource type ID (at 0x1401c4918)
    g_pSpawnResourceTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c4918));

    // Spawn template type ID (at 0x1401c77e8)
    g_pSpawnTemplateTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c77e8));

#ifdef _DEBUG
    printf("[SpawnComponent] GetLinkedObject = %p\n", (void*)fn_GetLinkedObject);
    printf("[SpawnComponent] DoSpawn = %p\n", (void*)fn_DoSpawn);
    printf("[SpawnComponent] SpawnResourceTypeId = %p\n", (void*)g_pSpawnResourceTypeId);
    printf("[SpawnComponent] SpawnTemplateTypeId = %p\n", (void*)g_pSpawnTemplateTypeId);
    printf("[SpawnComponent] IsDerivedFrom = %p\n", (void*)fn_IsDerivedFrom);
    printf("[SpawnComponent] secondsPerTick = %p\n", (void*)g_Timer_secondsPerTick);
#endif

    if (!fn_GetLinkedObject || !fn_DoSpawn || !fn_IsDerivedFrom || !g_Timer_secondsPerTick) {
#ifdef _DEBUG
        printf("[SpawnComponent] ERROR: missing critical deps, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x140055ad0);
    // Prologue bytes:
    //   0: MOV [RSP+0x10],RDX (5)
    //   5: PUSH RBX (1)
    //   6: PUSH RSI (1)
    //   7: PUSH RDI (1)
    //   8: PUSH R13 (2)
    //  10: PUSH R15 (2)
    // = 12 bytes, exactly at instruction boundary
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[SpawnComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveSpawnComponentUpdate() { RemoveHook(&g_hookUpdate); }
