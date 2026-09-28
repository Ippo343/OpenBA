// code/Game/Component/tradecomponent_livestock.cpp
// Trade wave — the last unported function in the trade path.
//
//   0x14005ace0  TradeComponent_SpawnPurchasedLivestock
//                (engine_funcs' fn_Building_AddBuyToStorage)
//
// This is the LIVESTOCK arm of the trade checkout. When a player buys animals at a
// trading post, the two other arms of 0x140081420 only register the purchase as
// available to the town; this one is what actually puts the animals in the world.
// Until now it was the single null dependency that kept the whole checkout opt-in
// (see the header of code/Game/Dialog/tradeui_checkout.cpp).
//
// Ghidra's name for the fn-ptr, "Building_AddBuyToStorage", is a mislabel of the same
// family as the two in tradeui_checkout.cpp: nothing here touches a StorageComponent.
// It clones `qty` entities from the purchased template and scatters them across the
// trading post's own footprint.
//
// ── Shape ────────────────────────────────────────────────────────────────────────
//   param_1 = the TradeComponent (its entity IBP is at +0x10)
//   param_2 = the purchased entity template/description
//   param_3 = how many to spawn (unsigned; the loop is guarded by a SIGNED test)
//
//   1. Find the post's component at +0x10 via 0x140002ed0 and read TWO footprint
//      corners out of it with 0x140122e20(comp, &out, 2, 0|1, NULL).
//   2. Convert both corners to map cells and normalise them into a min/max rect.
//   3. Seed an alternating sex from ONE Rand(2) before the loop.
//   4. Per animal: clone the template, find its AnimalComponent, place it at a random
//      point inside the corner-to-corner box, stamp the roam rect, randomise its
//      behaviour phase, and register the entity with the world.
//
// ── Notes taken from the DISASSEMBLY, not the decompile ──────────────────────────
// This cluster's decompiles render hidden out-params and float constants as noise
// (see reference_unresolved_rdata_const_deletes_guarded_test), so every float step
// below was read off the instruction stream at 0x14005af6c-0x14005b0d7:
//
//  * The two RandFloat draws are applied CROSSED — the FIRST scales the delta's Y,
//    the SECOND scales its X (14005afc8 MULSS XMM2,XMM7 / 14005afd2 MULSS XMM1,XMM6).
//    This is the same crossing already documented in pasturecomponent_spawn.cpp. The
//    world RNG at gw+0xd8 is shared, so drawing these in the wrong order desynchronises
//    every later roll in the game.
//  * The delta's Z is multiplied by XMM9, and XMM9 is set by `XORPS XMM9,XMM9`
//    (14005ae8e) — a hard 0.0. Every animal therefore lands at exactly corner-0's Z.
//  * +0x44 (behaviour phase) is RandFloat * animal[+0x48] * *0x140172114 — NOT the bare
//    RandFloat its pasture sibling writes.
//  * The cell round-trip (`idx = size*y + x` then `x = idx % size, y = idx / size`)
//    is retail's, and is a no-op by construction. Kept as-is rather than "simplified",
//    so the port diffs cleanly against the disassembly.
//
// ⚠ THE 0x1400c3470 CALL IS DEAD CODE IN RETAIL, and is ported that way deliberately.
// The guard is:
//        MOV ECX,[RDI+0x20] / MOV EAX,ECX / BTR EAX,0x16 / MOV [RDI+0x20],EAX
//        XOR ECX,EAX / TEST CX,CX / JZ ...
// BTR clears bit 22, so ECX^EAX is either 0 or 0x00400000 — and `TEST CX,CX` looks at
// the LOW 16 BITS ONLY, which are always zero either way. The branch can never be
// taken. This is the same species of retail dead code as 0x140045190 in the boat state
// machine. The flag clear itself is real and load-bearing; only the call is unreachable.
// "Repairing" it to TEST ECX,ECX would fire a visual-refresh call retail never makes.

#include "Component/AnimalComponent.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Component/animalcomponent_funcs.h"   // AnimalComponent_UpdateBehavior (0x140107320)
#include "Component/herdcomponent_funcs.h"     // HerdComponent_Helper7400 (0x140107400)
#include "Entity/componententity_funcs.h"      // 0x1400c3720 / 0x1400c48a0
#include "Action/exe_bodies.h"                 // Pf_EntityResolvePos_2e20, ActHunt_NotifyEntityChanged_c3470
#include "Work/exe_bodies.h"                   // WorkBase_FindComponent_2ed0
#include "Game/exe_constants.h"                // kRdata_140172220 / kRdata_140172114
#include "system/MemoryAllocator.h"

namespace {

// Leaves, by retail address:
//   0x140002ed0 WorkBase_FindComponent_2ed0 (footprint)   0x140122e20 Pf_EntityResolvePos_2e20
//   0x1400c3720 ComponentEntity_CtorFromDescription       0x1400c48a0 EntityList_RegisterEntity_Impl
//   0x140107400 HerdComponent_Helper7400 (the registered row is reg_herd_live.cpp's
//               one-line thunk to it)                     0x140107320 AnimalComponent_UpdateBehavior
//   0x1400c3470 ActHunt_NotifyEntityChanged_c3470 (model refresh)
//   .rdata 0x140172220 (yaw spread) / 0x140172114 (phase scale) -> kRdata_ constants.

inline uint64_t MakeIBP(void* obj) {
    uint64_t r = 0;
    new (&r) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(obj));   // B.3.3 bridge
    return r;
}

inline uintptr_t World() {
    return (GameUpdateLoop::_singleton != nullptr)
             ? reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) : 0;
}

// Retail's world-coordinate -> cell clamp, repeated verbatim for both corners.
inline int ToCell(float world, float half, int mapSize) {
    int c = static_cast<int>(world + half);      // CVTTSS2SI: truncate toward zero
    if (c < 0) c = 0;
    if (c >= mapSize) c = mapSize - 1;
    return c;
}

}  // namespace

// ============================================================
// 0x14005ace0 — spawn purchased livestock across the post's footprint
// ============================================================
extern "C" void TradeComponent_SpawnPurchasedLivestock(void* selfV, void* tmpl,
                                                       unsigned int qty) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    // ── the post's footprint corners ─────────────────────────────────────────────
    uint64_t fpIBP[2] = {0, 0};
    WorkBase_FindComponent_2ed0(*reinterpret_cast<uintptr_t*>(comp + 0x10), fpIBP);

    // Corner 0 and corner 1 of the footprint. Three floats each: 0x140122e20 writes a
    // full Vector3 through its out-param, and a 2-float array here would let it scribble
    // over whatever local sits next on the stack (reference_out_param_buffer_too_small).
    float a[3] = {0.0f, 0.0f, 0.0f};
    float b[3] = {0.0f, 0.0f, 0.0f};
    Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(fpIBP), a, 2, 0, nullptr);
    Pf_EntityResolvePos_2e20(*reinterpret_cast<void**>(fpIBP), b, 2, 1, nullptr);

    const uintptr_t w = World();
    if (!w) return;
    const int mapSize = **reinterpret_cast<int**>(w + 0xf8);
    const float half  = static_cast<float>(mapSize >> 1);

    // Retail derives each cell, packs it into a linear index, then immediately unpacks
    // it again. The round-trip is an identity; it is kept so this reads against the
    // disassembly one instruction at a time.
    int minX = ToCell(a[0], half, mapSize);
    int minY = ToCell(a[1], half, mapSize);
    {
        const int idx = mapSize * minY + minX;
        minX = idx % mapSize;
        minY = idx / mapSize;
    }
    int maxX = ToCell(b[0], half, mapSize);
    int maxY = ToCell(b[1], half, mapSize);
    {
        const int idx = mapSize * maxY + maxX;
        maxX = idx % mapSize;
        maxY = idx / mapSize;
    }
    if (maxX < minX) { const int t = minX; minX = maxX; maxX = t; }
    if (maxY < minY) { const int t = minY; minY = maxY; maxY = t; }

    void* rng = *reinterpret_cast<void**>(w + 0xd8);

    // ONE Rand(2) before the loop, flipped every iteration, so a purchase of 8 comes
    // out 4/4 rather than 8 independent coin flips.
    bool female = reinterpret_cast<System::Random*>(rng)->Rand(2u) == 0u;   // B.3.4b bridge

    if (static_cast<int>(qty) <= 0) return;

    const int rectW = maxX - minX + 1;
    const int rectH = maxY - minY + 1;
    const float yawSpread  = kRdata_140172220;
    const float phaseScale = kRdata_140172114;

    int spawned = 0;
    for (unsigned int i = 0; i < qty; ++i) {
        void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
        void* ent = mem ? ComponentEntity_CtorFromDescription(
                              reinterpret_cast<ComponentEntity*>(mem), tmpl) : nullptr;
        uint64_t entIBP = MakeIBP(ent);
        if (!entIBP) continue;

        // Inline AnimalComponent scan. Retail compares GetTypeId (vtable +0x10) against
        // DAT_1401c6f98 itself rather than calling the 0x140005500 finder; reproduced
        // inline for the same reason the pasture port does — this project has a
        // documented trap where a finder's Ghidra name and its actual type id disagree.
        void* animal = nullptr;
        {
            const uintptr_t e = static_cast<uintptr_t>(entIBP);
            const int n = *reinterpret_cast<int*>(e + 0x38);
            const uintptr_t list = *reinterpret_cast<uintptr_t*>(e + 0x30);
            for (int c = 0; c < n && list; ++c) {
                void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(c) * 8);
                void* cc = *slot;
                if (!cc) continue;
                const uint32_t tid = reinterpret_cast<Object::Base*>(cc)->Type();   // B.3.8 bridge
                if (tid != AnimalComponent::TypeId()) continue;
                if (Object::IsDerivedFromNullSafe(cc, AnimalComponent::TypeId())) animal = cc;
                break;
            }
        }
        uint64_t animalIBP = MakeIBP(animal);

        // CROSSED, per the disassembly: r1 scales Y, r2 scales X. Both are drawn even
        // when there is no AnimalComponent to place — the shared world RNG must advance
        // exactly as often as retail advances it.
        const float r1 = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
        const float r2 = reinterpret_cast<System::Random*>(rng)->RandFloat();   // B.3.4b bridge
        const float pos[3] = {
            a[0] + (b[0] - a[0]) * r2,
            a[1] + (b[1] - a[1]) * r1,
            a[2] + (b[2] - a[2]) * 0.0f,     // XORPS XMM9,XMM9 — always corner-0's Z
        };

        const float yaw = fn_Random_RandFloatTol(rng) * yawSpread;
        if (animalIBP)
            HerdComponent_Helper7400(static_cast<int64_t>(animalIBP),
                                     const_cast<float*>(pos), yaw, (char)female);
        female = !female;   // alternate AFTER placing, as retail does

        if (animalIBP) {
            const uintptr_t an = static_cast<uintptr_t>(animalIBP);

            // Clear bit 22. The refresh call behind it is unreachable in retail — see
            // the DEAD CODE note in this file's header — but the flag clear is real.
            const uint32_t before = *reinterpret_cast<uint32_t*>(an + 0x20);
            const uint32_t after  = before & ~0x00400000u;
            *reinterpret_cast<uint32_t*>(an + 0x20) = after;
            if (static_cast<uint16_t>(before) != static_cast<uint16_t>(after) &&
                *reinterpret_cast<int64_t*>(an + 0x10) != 0) {
                ActHunt_NotifyEntityChanged_c3470(*reinterpret_cast<void**>(an + 0x10));
            }

            // The roam rect: where this animal is allowed to wander.
            *reinterpret_cast<int16_t*>(an + 0x38) = static_cast<int16_t>(minX);
            *reinterpret_cast<int16_t*>(an + 0x3a) = static_cast<int16_t>(minY);
            *reinterpret_cast<int16_t*>(an + 0x3c) = static_cast<int16_t>(rectW);
            *reinterpret_cast<int16_t*>(an + 0x3e) = static_cast<int16_t>(rectH);

            const float span = *reinterpret_cast<float*>(an + 0x48);
            *reinterpret_cast<float*>(an + 0x44) =
                reinterpret_cast<System::Random*>(rng)->RandFloat() * span * phaseScale;   // B.3.4b bridge
            AnimalComponent_UpdateBehavior(reinterpret_cast<AnimalComponent*>(an));
        }

        uint64_t regIBP = entIBP;
        EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(w + 0xe8), &regIBP,
                                       *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(tmpl) + 0x20));
        ++spawned;
    }

    // OUTCOME, not intent: "the spawn ran" and "animals exist" are separate claims.
    std::fprintf(stderr, "[TradeLivestock] spawned %d/%u animal(s) from template %p "
                 "into rect cell(%d,%d) %dx%d\n",
                 spawned, qty, tmpl, minX, minY, rectW, rectH);
    std::fflush(stderr);
}
