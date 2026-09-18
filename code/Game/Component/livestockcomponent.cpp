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
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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
    if (self->data28 == 0) {
        return;
    }

    // Verify fn_IsDerivedFrom and globals are available
    if (!fn_IsDerivedFrom || !s_pLivestockConfigTypeId || !s_pTimeMultiplier || !g_Timer_secondsPerTick) {
        return;
    }

    // Get the related component from _unk18 and check its type
    void* relatedComponent = self->_unk18;
    bool isDerived = fn_IsDerivedFrom(relatedComponent, *s_pLivestockConfigTypeId);

    void* configComponent = isDerived ? relatedComponent : nullptr;

    // Check if config component has valid data at +0x20
    if (configComponent == nullptr) {
        return;
    }
    int64_t configDataPtr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(configComponent) + 0x20);
    if (configDataPtr == 0) {
        return;
    }

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

// ============================================================
// LivestockComponent::OnAdd (vtable slot [6], 0x140123c70)
// ============================================================
// A livestock with no pasture (+0x28 == 0) files itself into the world's
// FREE-ROAMING livestock list at gameState+0x80 (count +0x88). This is what makes
// the start-condition livestock (Template\LivestockCow/Sheep/Chicken.rsc, spawned
// loose by the start-config simple pool) findable by a pasture's adopt pass.
//
// FUN_140003110 is DynArray8_ResizeFill; called with count+1 it appends.

void LivestockComponent_OnAdd(LivestockComponent* self) {
    if (!self || self->data28 != 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uintptr_t gs = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!gs) return;
    uint64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    else ibp = reinterpret_cast<uint64_t>(self);
    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(gs + 0x80))
        .SetSizeFill(*reinterpret_cast<int*>(gs + 0x88) + 1, &ibp);
    std::fprintf(stderr, "[Pasture] livestock %p joined the free-roaming pool (now %d)\n",
                 (void*)self, *reinterpret_cast<int*>(gs + 0x88));
    std::fflush(stderr);
}

// ============================================================
// LivestockComponent::OnRemove (vtable slot [7], 0x140123c00)
// ============================================================
// Erase from whichever list owns it: the world free-roaming list when it has no
// pasture, otherwise its pasture's animal list (pasture+0x30 / count +0x38).
// FUN_140002cc0 is DynArray8_EraseByValue.

void LivestockComponent_OnRemove(LivestockComponent* self) {
    if (!self) return;
    typedef void (*fn_Erase_t)(void* arrayField, void* valueIBP);
    static fn_Erase_t s_erase = nullptr;
    static bool s_res = false;
    if (!s_res) { s_res = true;
        s_erase = reinterpret_cast<fn_Erase_t>(ResolveAddr(Module::Exe, 0x140002cc0)); }
    if (!s_erase) return;

    uint64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
    else ibp = reinterpret_cast<uint64_t>(self);

    if (self->data28 == 0) {
        if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
        uintptr_t gs = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
        if (gs) s_erase(reinterpret_cast<void*>(gs + 0x80), &ibp);
        return;
    }
    s_erase(reinterpret_cast<void*>(static_cast<uintptr_t>(self->data28) + 0x30), &ibp);
}

// ===== Standalone factory =====
// Inert by default; BANISHED_LIVE_PASTURE graduates three slots (indices from the
// retail vtable at 0x1401639b0): [6] OnAdd, [7] OnRemove, [9] Update. Size 0x38 and
// flags 0x8b come from LivestockComponent_Ctor (0x140123ee0) — Component_Ctor's 0x80
// OR'd with the leaf's 0x0b.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

static uint32_t LivestockComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("LivestockComponent");
    return cached;
}
static void* LivestockComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

extern "C" void LivestockComponent_Serialize(void* self, void* stream);

static void LivestockComponent_OnAddVSlot(void* s) {
    LivestockComponent_OnAdd(reinterpret_cast<LivestockComponent*>(s));
}
static void LivestockComponent_OnRemoveVSlot(void* s) {
    LivestockComponent_OnRemove(reinterpret_cast<LivestockComponent*>(s));
}
static void LivestockComponent_UpdateVSlot(void* s, void* t) {
    LivestockComponent_Update(reinterpret_cast<LivestockComponent*>(s),
                              t ? *reinterpret_cast<int64_t*>(t) : 0);
}
static void LivestockComponent_SerializeVSlot(void* self, void* stream) {
    LivestockComponent_Serialize(self, stream);
}
// Slot 3 (Serialize, retail 0x140123e10, body in animalwave_serialize.cpp) wired
// UNCONDITIONALLY (save/load wave 2026-08-28): a loaded save's LivestockComponent
// otherwise kept the base-Component serialize, so the assigned-pasture backref
// (+0x28) and production timer (+0x30) reset to null/0 on load.
static void LivestockComponent_InstallLive() {
    LivestockComponent_LiveVtable[3] = reinterpret_cast<void*>(&LivestockComponent_SerializeVSlot);
    LivestockComponent_LiveVtable[6] = reinterpret_cast<void*>(&LivestockComponent_OnAddVSlot);
    LivestockComponent_LiveVtable[7] = reinterpret_cast<void*>(&LivestockComponent_OnRemoveVSlot);
    LivestockComponent_LiveVtable[9] = reinterpret_cast<void*>(&LivestockComponent_UpdateVSlot);
}
extern "C" void* LivestockComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x38, 0x8b,
        LivestockComponent_LiveVtable,
        reinterpret_cast<void*>(&LivestockComponent_LiveGetTypeId));
    LivestockComponent_InstallLive();
    return p;
}
static void* LivestockComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x38, 0x8b,
        LivestockComponent_LiveVtable,
        reinterpret_cast<void*>(&LivestockComponent_LiveGetTypeId));
    LivestockComponent_InstallLive();
    return mem;
}

REGISTER_TYPE(LivestockComponent, "Component",
              &LivestockComponent_Factory, &LivestockComponent_CreateInPlace);
