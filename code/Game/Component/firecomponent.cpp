// code/Game/Component/firecomponent.cpp
// FireComponent — the per-frame tick (vtable slot 9), the Serialize slot (3), and
// the standalone factory that installs a REAL vtable for slots 3, 9 and 15.
//
// Two-state fire:
//   idle    (fireTimer == 0) — accumulate dt into +0x40 and roll for ignition,
//                              unless GameWorld+0xD0 bit 6 ("no fire") is set.
//   burning (fireTimer != 0) — count both timers down; spread when the spread
//                              timer expires; extinguish when the fire timer does.
//
// Original: 0x140048fb0 (53 instructions).
//
// ⚠ WHY THIS FILE USED TO DO NOTHING AT ALL — three stacked reasons, all fixed:
//   1. The factory was COMPONENT_INERT_FACTORY, so vtable slot 9 was Inert_Stub and
//      FireComponent_Update had no caller anywhere in the tree.
//   2. Ignite / Spread / Extinguish were unported, so ResolveAddr returned 0 and
//      every call site here was skipped by a null guard.
//   3. The FireDescription type-id global (0x1401c4218) was unregistered, so
//      IsDerivedFrom could never match and an added `if (buildingData)` guard
//      deleted the ignition roll outright. That guard is GONE — retail calls
//      FUN_1400a9290 unconditionally and so do we; see FireDesc() in
//      firecomponent_ignite.cpp for how the description is now obtained.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/firecomponent_funcs.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// Leaf bodies (firecomponent_ignite.cpp).
extern "C" void FireComponent_Ignite_610(void* self);
extern "C" void FireComponent_Spread_130(void* self);
extern "C" void FireComponent_Extinguish_4c0(void* self);
extern "C" void FireComponent_WaterApplied_090(void* self, int msg);
extern uint32_t g_tid_FireDescription;

namespace {

// FUN_1400a9290 — {rate, weight} probability roll (registered: reg_action_move.cpp).
// Reads BOTH floats of the pair and CLEARS *time on every call, both arms.
typedef bool (*fn_ProbCheck_t)(void* timeAccum, float* pair, void* rng);
fn_ProbCheck_t s_probCheck = nullptr;
bool s_init = false;

void EnsureInit() {
    if (s_init) return;
    s_probCheck = reinterpret_cast<fn_ProbCheck_t>(ResolveAddr(Module::Exe, 0x1400a9290));
    s_init = true;
}

}  // namespace

// ============================================================
// FireComponent::Update (vtable slot 9, 0x140048fb0)
// ============================================================
void FireComponent_Update(FireComponent* self, int64_t* pDeltaTime) {
    EnsureInit();
    if (!self || !pDeltaTime) return;

    const int64_t dt = *pDeltaTime;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    int64_t* pFireTimer   = reinterpret_cast<int64_t*>(s + 0x30);
    int64_t* pSpreadTimer = reinterpret_cast<int64_t*>(s + 0x38);
    int64_t* pAccum       = reinterpret_cast<int64_t*>(s + 0x40);

    if (*pFireTimer == 0) {
        // ---- idle: roll for a new fire ----
        const uintptr_t gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton)
                           ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
        if (!gw) return;

        // GameWorld+0xD0 is the debug / new-game flag word: bit0 zeroCost, bit1 noHunger,
        // bit2 noCold, bit3 noHealth, bit4 noSick, bit5 noCropDisease, BIT6 noFire,
        // bit7 noTornado. NewGameDialog::StartGame writes the whole word explicitly
        // (newgamedialog.cpp:740): disaster combo index 0 => 0 (fire ON), otherwise
        // 0xF0 (bits 4-7 => fire OFF). It is never left uninitialised.
        const uint32_t worldFlags = *reinterpret_cast<uint32_t*>(gw + 0xD0);
        if ((worldFlags >> 6) & 1) {
            return;
        }

        *pAccum += dt;

        // Retail: FUN_1400a9290(&accum, (float*)(desc + 0x44), gw->rng) — called
        // UNCONDITIONALLY with the description pointer, which retail always has.
        // No `if (desc)` here: that guard is what used to delete this roll.
        void* obj = *reinterpret_cast<void**>(s + 0x18);
        void* desc = (obj && ::Object::IsDerivedFromOrTrust(obj, g_tid_FireDescription))
                   ? obj : nullptr;
        if (!s_probCheck) return;
        void* rng = *reinterpret_cast<void**>(gw + 0xD8);
        float* pair = reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(desc) + FireDescriptionField::IgniteChance0);

        const bool fired = s_probCheck(pAccum, pair, rng);
        if (fired) {
            FireComponent_Ignite_610(self);
            return;
        }
    } else {
        // ---- burning: count down ----
        *pFireTimer   -= dt;
        *pSpreadTimer -= dt;

        if (*pFireTimer > 0 && *pSpreadTimer < 1)
            FireComponent_Spread_130(self);

        if (*pFireTimer < 1)
            FireComponent_Extinguish_4c0(self);
    }
}

// ============================================================
// FireComponent::Shutdown / Serialize (vtable slot 3, 0x140049a90)
// ============================================================
// Component_Serialize, then a version-1 Serializer block:
//   Time  +0x30, Time +0x38, int +0x28,
//   and, only when the stream version > 0, int +0x2c.
// This is the third independent body proving +0x28 / +0x2c are two int32s.
void FireComponent_Shutdown(FireComponent* self, void* stream) {
    if (!self || !stream) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    if (fn_ComponentBase_Serialize) fn_ComponentBase_Serialize(self, stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(reinterpret_cast<void*>(s + 0x30))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(reinterpret_cast<void*>(s + 0x38))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 0)   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FireComponent_InitFuncs() {
    EnsureInit();
}

// ============================================================
// Standalone factory — a REAL vtable, not the all-stubs inert one.
// ============================================================
// FireComponent overrides six slots in retail (0, 1, 2, 3, 9, 15 — read from the
// vtable BYTES at 0x140159380, which ends at 0x1401593ff where FireDescription's
// begins). The inert base gives us 0 (ScalarDtor) and 2 (GetTypeId); this fills
// the three that carry the mechanic:
//   3  Serialize      FireComponent_Shutdown
//   9  Update         FireComponent_Update       <- the per-frame tick
//   15 water applied  FireComponent_WaterApplied <- SendComponentMessage +0x78
//
// ⚠ The slot writes MUST happen after InitInertTyped/MakeInertTyped, never at
// static-init time: FillTypedVtable re-stubs the table on first construction and
// would erase them (reference_live_vtable_slot_must_be_wired_in_installlive).
//
// GATE: BANISHED_LIVE_FIRE — GRADUATED 2026-08-22, now in the default-on table. The
// "brand-new lane with no positive soak yet" that used to be written here is no
// longer true: see the graduation note in component_factory_base.cpp for the two
// arms that settled it. The var survives as a kill-switch (=0) for A/B runs.
#include "Component/component_factory_base.h"
#include "address_registry.h"

static uint32_t FireComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FireComponent");
    return cached;
}

static void* g_FireComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void FireComponent_UpdateVSlot(void* self, void* dt) {
    FireComponent_Update(reinterpret_cast<FireComponent*>(self),
                         reinterpret_cast<int64_t*>(dt));
}
static void FireComponent_SerializeVSlot(void* self, void* stream) {
    FireComponent_Shutdown(reinterpret_cast<FireComponent*>(self), stream);
}
static void FireComponent_WaterVSlot(void* self, int msg) {
    FireComponent_WaterApplied_090(self, msg);
}

static void FireComponent_FinishLiveVtable() {
    // Slot 3 (Serialize) is wired UNCONDITIONALLY, above the live gate.
    // Serializer wiring is not a feature flag: PersistObject dispatches slot 3
    // on every load, and a component whose slot 3 is absent falls back to the
    // base Component serialize and consumes FEWER bytes than the save leg wrote
    // -- a silent stream desync that surfaces as wild objIds on a LATER record.
    // It also made every BANISHED_LIVE_*=0 A/B run change the save-stream shape,
    // so such an A/B could "prove" something that was really a desync.
    // See reference_slot3_wrapper_must_resolve_first.
    g_FireComponent_LiveVtable[3]  = reinterpret_cast<void*>(&FireComponent_SerializeVSlot);
    g_FireComponent_LiveVtable[9]  = reinterpret_cast<void*>(&FireComponent_UpdateVSlot);
    g_FireComponent_LiveVtable[15] = reinterpret_cast<void*>(&FireComponent_WaterVSlot);
}

extern "C" void* FireComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(
        0x48, 0x88, g_FireComponent_LiveVtable,
        reinterpret_cast<void*>(&FireComponent_LiveGetTypeId));
    FireComponent_FinishLiveVtable();
    return p;
}

static void* FireComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(
        mem, 0x48, 0x88, g_FireComponent_LiveVtable,
        reinterpret_cast<void*>(&FireComponent_LiveGetTypeId));
    FireComponent_FinishLiveVtable();
    return mem;
}

// Register the vtable at its exe address so anything resolving 0x140159380 (and
// SendComponentMessage's +0x78 dispatch in particular) lands on our table.
extern "C" void RegisterFireComponentVtable() {
    RegisterExeAddr(FireComponentAddr::Vtable,
                    reinterpret_cast<uintptr_t>(g_FireComponent_LiveVtable));
}

REGISTER_TYPE(FireComponent, "Component",
              &FireComponent_Factory, &FireComponent_CreateInPlace);
