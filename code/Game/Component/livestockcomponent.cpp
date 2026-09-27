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


// ============================================================
// LivestockComponent::OnRemove (vtable slot [7], 0x140123c00)
// ============================================================
// Erase from whichever list owns it: the world free-roaming list when it has no
// pasture, otherwise its pasture's animal list (pasture+0x30 / count +0x38).
// FUN_140002cc0 is DynArray8_EraseByValue.


// ===== ENDGAME B.3.8 CIT — LivestockComponent as a REAL class (LivestockComponent.h) =====
// Before: a 48-slot hand-built LivestockComponent_LiveVtable filled at every
// create (slots 3 Serialize, 6 OnAdd, 7 OnRemove, 9 Update), Inert_ScalarDtor in
// slot 0. After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2).
#include "Object/factory_registration.h"
#include "Component/LivestockComponent.h"


// 0x140123ee0. Component() is the CALL 0x1401188b0; the 0x1401639b0 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 0x0b (0x8b).
LivestockComponent::LivestockComponent() : data28(0), data30(0) {
    OrFlagsInCtor(0x0b);
}

// [0] 0x140035680: restamp + ~Internal (no own work), MemFree on &1.
Object::Base* LivestockComponent::DeletingDtor(unsigned flags) {
    this->~LivestockComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

void LivestockComponent::OnAdded() {
    if (data28 != 0) return;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;

    uintptr_t gs = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!gs) return;
    uint64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
    else ibp = reinterpret_cast<uint64_t>(this);
    // B.3.2 bridge: FUN_140003110 is Collection::Array<void*>::SetSize(n, fill)
    GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(gs + 0x80))
        .SetSizeFill(*reinterpret_cast<int*>(gs + 0x88) + 1, &ibp);
    std::fprintf(stderr, "[Pasture] livestock %p joined the free-roaming pool (now %d)\n",
                 (void*)this, *reinterpret_cast<int*>(gs + 0x88));
    std::fflush(stderr);
}
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue);   // 0x140002cc0

void LivestockComponent::OnRemove() {
    // 0x140002cc0 is CemeteryListErase_Reimpl (cemeterycomponent.cpp): called
    // directly, no first-call cache (B.3.8, B6-5).
    const auto s_erase = &CemeteryListErase_Reimpl;

    uint64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(this));   // B.3.3 bridge
    else ibp = reinterpret_cast<uint64_t>(this);

    if (data28 == 0) {
        if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
        uintptr_t gs = *reinterpret_cast<uintptr_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
        if (gs) s_erase(reinterpret_cast<void*>(gs + 0x80), &ibp);
        return;
    }
    s_erase(reinterpret_cast<void*>(static_cast<uintptr_t>(data28) + 0x30), &ibp);
}

// [9] 0x140123b50. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af); the ported body takes the Time by value.
void LivestockComponent::Update(void* time) {
    const int64_t deltaTime = time ? *static_cast<int64_t*>(time) : 0;
    EnsureHelpers();

    // Early out: no herd component pointer
    if (data28 == 0) {
        return;
    }

    // Verify fn_IsDerivedFrom and globals are available
    if (!fn_IsDerivedFrom || !s_pLivestockConfigTypeId || !s_pTimeMultiplier || !g_Timer_secondsPerTick) {
        return;
    }

    // Get the related component from _unk18 and check its type
    void* relatedComponent = _description;
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
    data30 += static_cast<uint64_t>(deltaTime);

    // Compute reproduction threshold in ticks:
    //   threshold = Time(config[+0x2C] * timeMultiplier)
    //   Time(float seconds) = (int64_t)((double)seconds / _secondsPerTick)
    float intervalSeconds = *reinterpret_cast<float*>(
        reinterpret_cast<uintptr_t>(configComponent) + 0x2C);
    float thresholdSeconds = intervalSeconds * (*s_pTimeMultiplier);
    int64_t thresholdTicks = static_cast<int64_t>(
        static_cast<double>(thresholdSeconds) / (*g_Timer_secondsPerTick));

    // Fire birth events while accumulator exceeds threshold
    if (thresholdTicks < static_cast<int64_t>(data30)) {
        do {
            // Increment birth counter at herdComponent+0x44
            int32_t* birthCounter = reinterpret_cast<int32_t*>(
                data28 + 0x44);
            *birthCounter = *birthCounter + 1;

            // Subtract threshold from accumulator
            data30 -= static_cast<uint64_t>(thresholdTicks);
        } while (thresholdTicks < static_cast<int64_t>(data30));
    }
}

// create 0x140035480: MemAlloc(0x38, 8), then the ctor.
void* LivestockComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(LivestockComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) LivestockComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* LivestockComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) LivestockComponent());
}

DEFINE_EXE_VTABLE(LivestockComponent, "_ZTV18LivestockComponent")
DEFINE_EXE_TYPE(LivestockComponent, "Component")
