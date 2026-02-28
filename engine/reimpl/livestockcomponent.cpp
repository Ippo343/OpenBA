// engine/reimpl/livestockcomponent.cpp
// Reimplementation of LivestockComponent::Update from Application-x64.exe.
//
// Simple reproduction timer. Accumulates elapsed time into +0x30.
// When the accumulator exceeds a threshold (configComponent+0x2C * 300.0f),
// increments the birth counter at herdComponent(+0x28)+0x44 and wraps.
//
// Original: 0x140123b50

#include "LivestockComponent.h"
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

// Object::Base::IsDerivedFrom(this, typeId) -> bool
// Already in engine_funcs.h as fn_IsDerivedFrom

// Object::InternalBasePtr::CheckPtr — no-op in release, skip

// System::Time::Time(float) — constructs Time from seconds:
//   ticks = (int64_t)((double)seconds / _secondsPerTick)
// We inline this rather than calling the Runtime function.

// ============================================================
// Globals (in Application-x64.exe .data/.bss)
// ============================================================

// Type ID for the config component checked via IsDerivedFrom.
// Located at 0x1401c79a8 — runtime-filled by RegisterType.
static uint32_t* g_pLivestockConfigTypeId = nullptr;

// Time multiplier constant: 300.0f at 0x140172234 (.rdata)
static float* g_pTimeMultiplier = nullptr;

// ============================================================
// Update (0x140123b50)
// Reproduction timer: accumulates delta time, fires birth event
// when threshold exceeded (configInterval * 300.0f seconds -> ticks).
//
// Parameters: RCX = this, RDX = Time (int64)
// ============================================================
static void Reimpl_Update(LivestockComponent* self, int64_t deltaTime) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 5) {
        printf("[LivestockComponent::Update] self=%p dt=%lld herd=%p\n",
               (void*)self, deltaTime, (void*)self->data28);
        callCount++;
    }
#endif

    // Early out: no herd component pointer
    if (self->data28 == 0)
        return;

    // Get the related component from _unk18 and check its type
    void* relatedComponent = self->_unk18;
    bool isDerived = fn_IsDerivedFrom(
        reinterpret_cast<void*>(relatedComponent),
        *g_pLivestockConfigTypeId);

    void* configComponent = nullptr;
    if (isDerived) {
        configComponent = relatedComponent;
    }

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
    float thresholdSeconds = intervalSeconds * (*g_pTimeMultiplier);
    int64_t thresholdTicks = static_cast<int64_t>(
        static_cast<double>(thresholdSeconds) / (*g_Timer_secondsPerTick));

    // Fire birth events while accumulator exceeds threshold
    if (thresholdTicks < static_cast<int64_t>(self->data30)) {
        do {
            // CheckPtr is a no-op, skip it
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
// Install / Remove
// ============================================================
void InstallLivestockComponentUpdate() {
    // Resolve globals in the exe
    g_pLivestockConfigTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c79a8));
    g_pTimeMultiplier = reinterpret_cast<float*>(
        ResolveAddr(Module::Exe, 0x140172234));

#ifdef _DEBUG
    printf("[LivestockComponent] ConfigTypeId @ %p = 0x%x\n",
           (void*)g_pLivestockConfigTypeId,
           g_pLivestockConfigTypeId ? *g_pLivestockConfigTypeId : 0);
    printf("[LivestockComponent] TimeMultiplier @ %p = %f\n",
           (void*)g_pTimeMultiplier,
           g_pTimeMultiplier ? *g_pTimeMultiplier : 0.0f);
#endif

    if (!g_pLivestockConfigTypeId || !g_pTimeMultiplier) {
#ifdef _DEBUG
        printf("[LivestockComponent] ERROR: failed to resolve globals, skipping hook\n");
#endif
        return;
    }

    // fn_IsDerivedFrom is resolved in engine_funcs — verify it's available
    if (!fn_IsDerivedFrom) {
#ifdef _DEBUG
        printf("[LivestockComponent] ERROR: fn_IsDerivedFrom not resolved, skipping hook\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, LivestockComponentAddr::Update);
    // Prologue: MOV [RSP+0x18],RSI(5) + MOV [RSP+0x20],RDI(5) + PUSH R14(2) = 12 bytes
    // Instruction boundary exactly at 12 bytes (before SUB RSP,0x20).
    InstallHook(target, (void*)Reimpl_Update, &g_hookUpdate, 12);
#ifdef _DEBUG
    printf("[LivestockComponent] Update hooked at %p (prologue=12)\n", (void*)target);
#endif
}

void RemoveLivestockComponentUpdate() { RemoveHook(&g_hookUpdate); }
