// code/Game/Component/livestockcomponent.cpp
// Clean reimplementation of LivestockComponent::Update from Application-x64.exe.
//
// LivestockComponent (0x38 bytes) is a simple reproduction timer. Accumulates
// elapsed time into +0x30. When the accumulator exceeds a threshold
// (configComponent+0x2C * 300.0f), increments the birth counter at
// herdComponent(+0x28)+0x44 and wraps.
//
// Original: 0x140123b50
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/livestockcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Type ID for the config component checked via IsDerivedFrom.
// Located at 0x1401c79a8 — runtime-filled by RegisterType.
static uint32_t* s_pLivestockConfigTypeId = nullptr;

// Time multiplier constant: 300.0f at 0x140172234 (.rdata)
static float* s_pTimeMultiplier = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_pLivestockConfigTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c79a8));
    s_pTimeMultiplier = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));

    s_initialized = true;
}

// ============================================================
// LivestockComponent::Update (vtable slot [9], 0x140123b50)
// ============================================================
// Reproduction timer: accumulates delta time, fires birth event
// when threshold exceeded (configInterval * 300.0f seconds -> ticks).

void LivestockComponent_Update(LivestockComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    // Early out: no herd component pointer
    if (self->data28 == 0)
        return;

    // Verify fn_IsDerivedFrom and globals are available
    if (!fn_IsDerivedFrom || !s_pLivestockConfigTypeId || !s_pTimeMultiplier || !g_Timer_secondsPerTick)
        return;

    // Get the related component from _unk18 and check its type
    void* relatedComponent = self->_unk18;
    bool isDerived = fn_IsDerivedFrom(relatedComponent, *s_pLivestockConfigTypeId);

    void* configComponent = isDerived ? relatedComponent : nullptr;

    // Check if config component has valid data at +0x20
    if (configComponent == nullptr)
        return;
    int64_t configDataPtr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(configComponent) + 0x20);
    if (configDataPtr == 0)
        return;

    // Accumulate delta time into reproduction timer
    self->data30 += static_cast<uint64_t>(deltaTime);

    // Compute reproduction threshold in ticks:
    //   threshold = Time(config[+0x2C] * timeMultiplier)
    //   Time(float seconds) = (int64_t)((double)seconds / _secondsPerTick)
    float intervalSeconds = *reinterpret_cast<float*>(
        reinterpret_cast<uintptr_t>(configComponent) + 0x2C);
    float thresholdSeconds = intervalSeconds * (*s_pTimeMultiplier);
    int64_t thresholdTicks = static_cast<int64_t>(
        static_cast<double>(thresholdSeconds) / (*g_Timer_secondsPerTick));

    // Fire birth events while accumulator exceeds threshold
    if (thresholdTicks < static_cast<int64_t>(self->data30)) {
        do {
            // Increment birth counter at herdComponent+0x44
            int32_t* birthCounter = reinterpret_cast<int32_t*>(
                self->data28 + 0x44);
            *birthCounter = *birthCounter + 1;

            // Subtract threshold from accumulator
            self->data30 -= static_cast<uint64_t>(thresholdTicks);
        } while (thresholdTicks < static_cast<int64_t>(self->data30));
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void LivestockComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[LivestockComponent] Funcs resolved: ConfigTypeId=%p(%u) TimeMultiplier=%p(%f)\n",
           (void*)s_pLivestockConfigTypeId,
           s_pLivestockConfigTypeId ? *s_pLivestockConfigTypeId : 0u,
           (void*)s_pTimeMultiplier,
           s_pTimeMultiplier ? *s_pTimeMultiplier : 0.0f);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(LivestockComponent, 0x38, 0x8B)
REGISTER_TYPE(LivestockComponent, "Component",
              &LivestockComponent_Factory, &LivestockComponent_CreateInPlace);
