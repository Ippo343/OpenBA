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
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/FireDescription.h"
#include "Action/exe_bodies.h"

// Leaf bodies (firecomponent_ignite.cpp).
extern "C" void FireComponent_Ignite_610(void* self);
extern "C" void FireComponent_Spread_130(void* self);
extern "C" void FireComponent_Extinguish_4c0(void* self);

// FUN_1400a9290 — {rate, weight} probability roll (ActMvDep_TimeCheck_a9290,
// called directly). Reads BOTH floats of the pair and CLEARS *time on every call,
// both arms.

// ============================================================
// ENDGAME B.3.8 CIT — FireComponent as a REAL class (FireComponent.h).
// ============================================================
// Before: a 48-slot hand-built g_FireComponent_LiveVtable filled at every create
// (slots 3 Serialize, 9 Update, 15 water applied), Inert_ScalarDtor in slot 0,
// and registered at the retail table address 0x140159380. After: the compiler's
// 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2). The retail
// table address had no by-address reader in the tree (every SendComponentMessage
// +0x78 dispatch goes through the object's own vptr), so its registration went
// with the hand-built table (B38 D5).
#include "Object/factory_registration.h"
#include "Component/FireComponent.h"

// 0x140049b80. Component() is the CALL 0x1401188b0; the 0x140159380 stamp is
// the compiler's; +0x28..+0x40 = 0; flags |= 0x08 (0x88).
FireComponent::FireComponent()
    : bucketsApplied(0), bucketsAtSpread(0), fireTimer(0), spreadTimer(0), igniteAccum(0) {
    OrFlagsInCtor(0x08);
}

// [3] 0x140049a90.
// ============================================================
// FireComponent::Shutdown / Serialize (vtable slot 3, 0x140049a90)
// ============================================================
// Component_Serialize, then a version-1 Serializer block:
//   Time  +0x30, Time +0x38, int +0x28,
//   and, only when the stream version > 0, int +0x2c.
// This is the third independent body proving +0x28 / +0x2c are two int32s.
void FireComponent::Persist(void* stream) {
    if (!stream) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    Component::Persist(stream);   // retail's direct CALL 0x1401189b0 (was the engine_funcs door)

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(reinterpret_cast<void*>(s + 0x30))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<System::Time*>(reinterpret_cast<void*>(s + 0x38))->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 0)   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// [9] 0x140048fb0. RDX is a pointer to the elapsed System::Time (EntityList
// bucket tick 0x1400c42af).
// ============================================================
// FireComponent::Update (vtable slot 9, 0x140048fb0)
// ============================================================
void FireComponent::Update(void* time) {
    int64_t* pDeltaTime = static_cast<int64_t*>(time);
    if (!pDeltaTime) return;

    const int64_t dt = *pDeltaTime;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    int64_t* pFireTimer   = reinterpret_cast<int64_t*>(s + 0x30);
    int64_t* pSpreadTimer = reinterpret_cast<int64_t*>(s + 0x38);
    int64_t* pAccum       = reinterpret_cast<int64_t*>(s + 0x40);

    if (*pFireTimer == 0) {
        // ---- idle: roll for a new fire ----
        const uintptr_t gw = (GameUpdateLoop::_singleton != nullptr)
                           ? reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) : 0;
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
        void* desc = (obj && ::Object::IsDerivedFromOrTrust(obj, FireDescription::TypeId()))
                   ? obj : nullptr;
        void* rng = *reinterpret_cast<void**>(gw + 0xD8);
        float* pair = reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(desc) + FireDescriptionField::IgniteChance0);

        const bool fired = ActMvDep_TimeCheck_a9290(pAccum, pair, rng);
        if (fired) {
            FireComponent_Ignite_610(this);
            return;
        }
    } else {
        // ---- burning: count down ----
        *pFireTimer   -= dt;
        *pSpreadTimer -= dt;

        if (*pFireTimer > 0 && *pSpreadTimer < 1)
            FireComponent_Spread_130(this);

        if (*pFireTimer < 1)
            FireComponent_Extinguish_4c0(this);
    }
}


// create 0x1400055e0: MemAlloc(0x48, 8), then the ctor.
void* FireComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FireComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FireComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* FireComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FireComponent());
}

DEFINE_EXE_TYPE(FireComponent, "Component")
