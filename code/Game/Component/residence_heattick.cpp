// code/Game/Component/residence_heattick.cpp
// ResidenceComponent state-2 (built) per-frame tick — faithful port of
// 0x140129970 (Ghidra label "ResidenceDescription_helper_a350_sub_9970"; it is
// the residence HEAT tick). Registered as the state-2 trans38Handler in the
// ResidenceComponent controller state table (residencecomponent.cpp) and
// dispatched every frame by StateMachine::Tick through the node vtable slot 7
// (see controller_state_table.h Trans38 — the exe slot-7 dispatcher 0x140037a00
// passes RDX=sm and R8=Time* dt through its tail-JMP).
//
// Retail logic (decompile-verified):
//   1. heatRemaining(+0x48, System::Time int64 countdown) -= *dt, every tick.
//   2. [audio] occupied-house chimney emitter upkeep (FUN_140002280/42960/42c60).
//   3. [mesh]  sub-part 6 when edible count > 0 (0x14012a9d0), sub-part 7 when
//              0x14012ab00 count > 0 — via FUN_14012c3f0 on the MeshInstance.
//   4. [abandon] occupancy==0 && storage non-empty && no pending action ->
//              push a ClearStorage action (0x14012ead0/ec60/eb50).
//   5. BURN: if worldTemp < desc->heatThreshold(+0x28) AND heatRemaining < 0
//      AND residentSlotCount(+0x58) > 0: scan the house StorageComponent
//      (finder 0x140006270) slots for a stack whose RawMaterialDescription
//      (tid DWORD_1401c7cb8) has FUEL flag (+0x5c bit 6, 0x40) and available
//      units (slot.count(+0x8) - slot.reserved(+0xC) > 0); then refill
//      heatRemaining = Time(desc->fuelBurnDuration(+0x2C)), subtract 1 unit
//      (0x14012d150), bump the town per-category consumed counter
//      (townMgr(+0xf0)+0x170 row, cat via 0x140088f90, +0xC field), break.
//
// STANDALONE: branch 2 (chimney audio) and 4 (abandon/ClearStorage, needs the
// un-ported 0x140003050 finder chain) are skipped. Branch 3 is LIVE: it is NOT
// a mesh op — 0x14012c3f0 is StatusIcon_Toggle (statusiconcomponent.cpp) and
// 0x140002f90 is the StatusIconComponent finder (Ghidra misnames both as
// MeshInstance ops). It CLEARS icon bit 6 (no-food) while the house has edible
// stock and bit 7 (no-firewood / cold) while it has fuel — the SET side lives
// in the Gather/Gather2 no-target branches (citizenai_gather.cpp). Branches
// 1 + 3 + 5 together close the retail warm/cold loop the building status icon
// displays.

#include "Component/ResidenceDescription.h"
#include "Component/RawMaterialDescription.h"
#include "system/Value.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "Component/exe_bodies.h"   // CitizenComp_Storage_GetResourceCount, Storage_GetResourceCount2,
                                    // CitizenComp_FUN_14012c3f0
#include "Action/exe_bodies.h"      // ActRm_StorageSubtract_d150, ActRm_GetResourceCategory_88f90
#include "Work/exe_bodies.h"        // WorkBase_FindComponent_6270, WorkBase_FindComponent_2f90

// ---- exe helpers, by retail address ----------------------------------------
//   0x140006270 WorkBase_FindComponent_6270  Entity_FindComponent_Storage (the REAL storage
//               finder; NOT the misnamed 0x140008090 residence finder)
//   0x14012d150 ActRm_StorageSubtract_d150   StorageComponent slot subtract (storage, stack, n)
//   0x140088f90 ActRm_GetResourceCategory_88f90  townMgr+0x170 counter row index
//   0x14012a9d0 CitizenComp_Storage_GetResourceCount  edible count
//   0x14012ab00 Storage_GetResourceCount2    fuel count
//   0x140002f90 WorkBase_FindComponent_2f90  StatusIconComponent finder
//   0x14012c3f0 CitizenComp_FUN_14012c3f0    status-icon bit toggle

// Signature matches the Trans38 dispatcher tail-call: (component, sm, Time* dt).
extern "C" char ResidenceComponent_HeatTick(void* selfV, void* /*sm*/, void* dtP) {
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);
    if (!self) return 1;

    // 1. Heat countdown -= frame dt (unconditional, every tick).
    if (dtP) {
        *reinterpret_cast<int64_t*>(self + 0x48) -= *reinterpret_cast<int64_t*>(dtP);
    }

    // [2 (audio) and 4 (abandon) skipped — see file header.]

    // 3. Status-icon upkeep: while the house HAS food, clear icon bit 6
    //    (no-food); while it HAS firewood, clear bit 7 (no-firewood / cold).
    //    Faithful to the decompile: FUN_14012c3f0(*finder2f90(owner), bit, 0).
    {
        if (CitizenComp_Storage_GetResourceCount(static_cast<int64_t>(self)) > 0) {
            int64_t iconIBP = 0;
            WorkBase_FindComponent_2f90(*reinterpret_cast<uintptr_t*>(self + 0x10), &iconIBP);
            if (iconIBP) CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(iconIBP), 6, 0);
        }
        if (Storage_GetResourceCount2(static_cast<int64_t>(self)) > 0) {
            int64_t iconIBP = 0;
            WorkBase_FindComponent_2f90(*reinterpret_cast<uintptr_t*>(self + 0x10), &iconIBP);
            if (iconIBP) CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(iconIBP), 7, 0);
        }
    }

    // 5. Burn branch.
    if (GameUpdateLoop::_singleton == nullptr) return 1;
    uintptr_t gw = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    uintptr_t wsim = *reinterpret_cast<uintptr_t*>(gw + 0x130);   // weather sim ptr
    if (!wsim) return 1;
    float worldTemp = *reinterpret_cast<float*>(wsim + 0x18);

    void* descObj = *reinterpret_cast<void**>(self + 0x18);
    if (!descObj || !Object::IsDerivedFromNullSafe(descObj, ResidenceDescription::TypeId())) return 1;
    uintptr_t desc = reinterpret_cast<uintptr_t>(descObj);
    float heatThreshold = *reinterpret_cast<float*>(desc + 0x28);


    if (worldTemp >= heatThreshold) return 1;
    if (*reinterpret_cast<int64_t*>(self + 0x48) >= 0) return 1;
    if (*reinterpret_cast<int*>(self + 0x58) <= 0) return 1;

    // Find the house StorageComponent.
    int64_t storeIBP = 0;
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(self + 0x10), &storeIBP);
    uintptr_t store = static_cast<uintptr_t>(storeIBP);
    if (!store) return 1;

    int slotCount = *reinterpret_cast<int*>(store + 0x30);
    uintptr_t slots = *reinterpret_cast<uintptr_t*>(store + 0x28);
    bool burned = false;
    for (int i = 0; i < slotCount; ++i) {
        uintptr_t slot  = slots + static_cast<uintptr_t>(i) * 0x10;
        uintptr_t stack = *reinterpret_cast<uintptr_t*>(slot);
        if (!stack) continue;   // guard (retail assumes every slot has a stack)

        // Find the stack's RawMaterialDescription (GetTypeId via vtable+0x10,
        // then IsDerivedFrom — same idiom as fn_StorageComponent_QueryCount).
        void* mat = nullptr;
        {
            int compCount = *reinterpret_cast<int*>(stack + 0x30);
            uintptr_t comps = *reinterpret_cast<uintptr_t*>(stack + 0x28);
            for (int j = 0; j < compCount; ++j) {
                void* comp = *reinterpret_cast<void**>(comps + static_cast<uintptr_t>(j) * 8);
                if (!comp) continue;
                uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
                if (tid == RawMaterialDescription::TypeId()) {
                    if (Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId())) mat = comp;
                    break;
                }
            }
        }
        if (!mat) continue;     // guard (retail would deref null +0x5c)

        uint32_t matFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(mat) + 0x5c);
        int avail = *reinterpret_cast<int*>(slot + 8) - *reinterpret_cast<int*>(slot + 0xc);
        if (((matFlags >> 6) & 1) != 0 && avail > 0) {
            // Refill the countdown to the description burn duration, then burn 1.
            int64_t t = 0;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), *reinterpret_cast<float*>(desc + 0x2c));
            *reinterpret_cast<int64_t*>(self + 0x48) = t;

            ActRm_StorageSubtract_d150(store, static_cast<int64_t>(stack), 1);

            int64_t townMgr = *reinterpret_cast<int64_t*>(gw + 0xf0);
            if (townMgr) {
                int cat = ActRm_GetResourceCategory_88f90(reinterpret_cast<void*>(store), stack);
                if (cat >= 0) {
                    // +0x170 holds a POINTER to the counter rows (0x20 stride,
                    // consumed count at +0xC) — same deref as the eat loop.
                    int64_t rows = *reinterpret_cast<int64_t*>(townMgr + 0x170);
                    if (rows)
                        *reinterpret_cast<int*>(rows + static_cast<int64_t>(cat) * 0x20 + 0xc) += 1;
                }
            }

            // One line per burn (one per house per burn-duration — low volume);
            // uncapped so long soak runs show the full consumption sawtooth.
            // (PERF 2026-09-05: gated -- 800 houses in winter made this unbuffered stderr
            // line 1.7% of the EntityList phase in the perf build.)
            burned = true;
            break;
        }
    }
    if (!burned) {
        // House is COLD with no fuel available: heatRemaining stays negative, the
        // warm-check 0x14012ac30 reads cold, and Gather2's no-target branch will
        // raise status-icon bit 7. This branch re-runs every frame while cold, so
        // throttle to ~1 line per 600 occurrences.
        static int n = 0;
        if ((n++ % 600) == 0) {
            fprintf(stderr, "[HeatTick] house=%p COLD no fuel available "
                    "(temp=%.2f < %.2f, heatLeft=%lld, storageSlots=%d)\n",
                    selfV, worldTemp, heatThreshold,
                    (long long)*reinterpret_cast<int64_t*>(self + 0x48), slotCount);
            fflush(stderr);
        }
    }
    return 1;
}
