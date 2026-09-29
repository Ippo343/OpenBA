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


#include "Component/ResidenceComponent.h"
#include "Component/StorageComponent.h"
#include "Component/StatusIconComponent.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"
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

// ---- exe helpers, by retail address ----------------------------------------
//   0x140006270 FindComponent<StorageComponent> (the REAL storage finder; NOT the
//               misnamed 0x140008090 residence finder)
//   0x14012d150 ActRm_StorageSubtract_d150   StorageComponent slot subtract (storage, stack, n)
//   0x140088f90 ActRm_GetResourceCategory_88f90  townMgr+0x170 counter row index
//   0x14012a9d0 CitizenComp_Storage_GetResourceCount  edible count
//   0x14012ab00 Storage_GetResourceCount2    fuel count
//   0x140002f90 FindComponent<StatusIconComponent>
//   0x14012c3f0 CitizenComp_FUN_14012c3f0    status-icon bit toggle

// The state-2 per-frame handler (the table's thunk in residencecomponent.cpp passes
// the Time* dt of the Trans38 dispatcher tail-call).
char ResidenceComponent::HeatTick(System::Time* dt) {
    // 1. Heat countdown -= frame dt (unconditional, every tick).
    if (dt) {   // a guard of the tree's
        heatRemaining._ticks -= dt->_ticks;
    }

    // [2 (audio) and 4 (abandon) skipped — see file header.]

    // 3. Status-icon upkeep: while the house HAS food, clear icon bit 6
    //    (no-food); while it HAS firewood, clear bit 7 (no-firewood / cold).
    //    Faithful to the decompile: FUN_14012c3f0(*finder2f90(owner), bit, 0).
    {
        // LEFT: the two count bodies take the residence as int64_t (COMP-b's declarations).
        if (CitizenComp_Storage_GetResourceCount(reinterpret_cast<int64_t>(this)) > 0) {
            StatusIconComponent* icon = _entity->FindComponent<StatusIconComponent>();
            if (icon) CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(icon), 6, 0);   // LEFT: B4 FORWARD's old signature
        }
        if (Storage_GetResourceCount2(reinterpret_cast<int64_t>(this)) > 0) {
            StatusIconComponent* icon = _entity->FindComponent<StatusIconComponent>();
            if (icon) CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(icon), 7, 0);   // LEFT: B4 FORWARD's old signature
        }
    }

    // 5. Burn branch.
    if (GameUpdateLoop::_singleton == nullptr) return 1;
    // LEFT: GameUpdateLoop::m_weather is void* (GAME-a): the temperature stays by offset.
    uintptr_t wsim = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton->m_weather);   // weather sim ptr
    if (!wsim) return 1;
    float worldTemp = *reinterpret_cast<float*>(wsim + 0x18);

    Description* descObj = _description;
    if (!descObj || !Object::IsDerivedFromNullSafe(descObj, ResidenceDescription::TypeId())) return 1;
    ResidenceDescription* desc = static_cast<ResidenceDescription*>(descObj);
    float heatThreshold = desc->_coldThreshold;


    if (worldTemp >= heatThreshold) return 1;
    if (heatRemaining._ticks >= 0) return 1;
    if (residentSlotCount <= 0) return 1;

    // Find the house StorageComponent.
    StorageComponent* store = _entity->FindComponent<StorageComponent>();   // 0x140006270
    if (!store) return 1;

    int slotCount = store->slotCount;
    StorageComponent::Item* slots = store->storageSlots;
    bool burned = false;
    for (int i = 0; i < slotCount; ++i) {
        StorageComponent::Item* slot = &slots[i];
        ComponentDescription* stack = slot->_resource;
        if (!stack) continue;   // guard (retail assumes every slot has a stack)

        // Find the stack's RawMaterialDescription (GetTypeId via vtable+0x10,
        // then IsDerivedFrom — same idiom as fn_StorageComponent_QueryCount).
        RawMaterialDescription* mat = nullptr;
        {
            int compCount = stack->_subDescCount;
            for (int j = 0; j < compCount; ++j) {
                void* comp = stack->_subDescs[j];
                if (!comp) continue;
                // LEFT: ComponentDescription::_subDescs is void** (COMP-c's header).
                uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
                if (tid == RawMaterialDescription::TypeId()) {
                    if (Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId()))
                        mat = reinterpret_cast<RawMaterialDescription*>(comp);   // LEFT: _subDescs is void** (COMP-c)
                    break;
                }
            }
        }
        if (!mat) continue;     // guard (retail would deref null +0x5c)

        uint32_t matFlags = mat->_flags;
        int avail = slot->_count - slot->_reserve;
        if (((matFlags >> 6) & 1) != 0 && avail > 0) {
            // Refill the countdown to the description burn duration, then burn 1.
            System::Time t{0};
            RtSystem_Time_Ctor(&t, desc->_heatInterval);
            heatRemaining = t;

            // LEFT: ActRm_StorageSubtract_d150 takes the storage and the item as integers (ACT-b's declaration).
            ActRm_StorageSubtract_d150(reinterpret_cast<uintptr_t>(store), reinterpret_cast<int64_t>(stack), 1);

            // LEFT: GameUpdateLoop::m_citizenList is void* (GAME-a): its +0x170 rows stay by offset.
            int64_t townMgr = reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_citizenList);
            if (townMgr) {
                int cat = ActRm_GetResourceCategory_88f90(store, reinterpret_cast<uintptr_t>(stack));   // LEFT: ACT-b's declaration takes the item as uintptr_t
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
                    static_cast<void*>(this), worldTemp, heatThreshold,
                    (long long)heatRemaining._ticks, slotCount);
            fflush(stderr);
        }
    }
    return 1;
}
