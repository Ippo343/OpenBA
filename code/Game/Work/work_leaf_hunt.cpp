// code/Game/Work/work_leaf_hunt.cpp
// Wave (Game-gap / Work-leaf) — faithful 1:1 ports of small exe helper functions
// referenced via ResolveAddr(Module::Exe, 0xADDR) from the Hunt / HarvestOrchard /
// Pasture / ConsumeProduce work classes. Each ResolveAddr returns 0 (silently
// skipped) unless the address is registered. This file provides the clean reimplementations so the
#include "Work/exe_bodies.h"
// standalone build fires real logic.
//
// Each function below is a byte-faithful port of the decompiled + disassembled exe
// helper (offsets verified against Application-x64.exe in Ghidra). Do NOT "improve"
// behaviour — these are 1:1 reproductions.
//
// SAFE subset only: every function here has its ENTIRE exe sub-callee closure already
// registered (or is engine-resolved). The intricate action-chain builders
// (0x1400bbbc0, 0x1400bb7b0, 0x1400bef70, 0x1400f2370) are DEFERRED — see the batch
// report — because their sub-callee closures are incomplete and/or the 11-arg
// XMM2/mixed-stack CreateRawMaterial call is too fidelity-risky to register blind on a
// path autostart does not exercise.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include <new>
#include "object/TypeInfo.h"
#include "Player/exe_bodies.h"                 // B3R: PlayerPath_FindComponent_3ea0
#include "Component/controller_state_table.h"   // B3R: StateMachine_TickReimpl
#include "system/Value.h"                      // B3R: RtSystem_Time_Ctor
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/HuntComponent.h"                // ENDGAME B.3.9 D5
#include "Component/FruitComponent.h"                // ENDGAME B.3.9 D5
#include "Component/PastureComponent.h"              // ENDGAME B.3.9 D5
#include "Component/RadiusDescription.h"             // ENDGAME B.3.9 D5
#include "Component/ConsumeProduceDescription.h"     // ENDGAME B.3.9 D5
#include "Component/AnimalComponent.h"               // ENDGAME B.3.9 D5
#include "Component/AnimalDescription.h"             // ENDGAME B.3.9 D5
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Component/ComponentDescription.h"
#include "Component/Description.h"
#include "Component/ConsumeProduceComponent.h"
#include "Component/ResourceProduceComponent.h"
#include "Component/StateMachineComponent.h"
#include "Component/LivestockComponent.h"

// ============================================================
// FindComponent-by-typeId family
// ============================================================
// Standard pattern: scan the entity's component pointer array, call each component's
// GetTypeId (vtable +0x10), and on the first type match run an IsDerivedFrom recheck
// before returning the component through an out InternalBasePtr.
//
// Two layout variants appear:
//   * "+0x30/+0x38" : list ptr at entity+0x30, count at entity+0x38, WITH CheckPtr on
//                     each slot (the canonical FindComponentByTypeId shape).
//   * "+0x28/+0x30" : list ptr at entity+0x28, count at entity+0x30, NO CheckPtr.

// ---- 0x14002a730  HuntWork_Serialize_sub_a730 ----------------------------------
// FindComponent (+0x30/+0x38, WITH CheckPtr), typeId @0x1401c7890.
// ABI: InternalBasePtr* fn(longlong entity, InternalBasePtr* retIBP)  (member value-return).
void* WorkLeaf_FindComp_a730(uintptr_t entity, void* retIBP) {
    return new (retIBP) ::Object::InternalPtr<HuntComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<HuntComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
}

// ---- 0x14000a0b0  FindComponent (+0x30/+0x38, WITH CheckPtr) -------------------
// typeId @0x1401c4250. (HarvestOrchard "DescComp" finder.)
void* WorkLeaf_FindComp_a0b0(uintptr_t entity, void* retIBP) {
    return new (retIBP) ::Object::InternalPtr<FruitComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<FruitComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
}

// ---- 0x140004fe0  FindComponent (+0x30/+0x38, WITH CheckPtr) -------------------
// typeId @0x1401c7ba0. (PastureComponent finder.)
void* WorkLeaf_FindComp_4fe0(uintptr_t entity, void* retIBP) {
    return new (retIBP) ::Object::InternalPtr<PastureComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<PastureComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
}


// ---- 0x1400063d0  HuntWork_helper_63d0  ----------------------------------------
// The "+0x28/+0x30" variant: RCX is a ComponentDescription (the list is its
// sub-descriptions, NO CheckPtr), typeId @0x1401c4870 (RadiusDescription).
// Returns the raw pointer directly via *retval (NOT an IBP Ctor).
// ABI: undefined8* fn(longlong desc, undefined8* retval).
// LEFT: the caller (HuntWork, WORK-b) hands the description as an integer.
void* WorkLeaf_FindComp_63d0(uintptr_t entity, void** retval) {
    ComponentDescription* desc = reinterpret_cast<ComponentDescription*>(entity);
    const uint32_t tid = RadiusDescription::TypeId(); // 0x1401c4870
    void* found = nullptr;
    if (desc->_subDescCount > 0) {
        Description** list = desc->_subDescs;
        int n = desc->_subDescCount;
        int i = 0;
        for (;;) {
            // No CheckPtr in this variant.
            Description* comp = desc->_subDescs[i];
            uint32_t t = comp->Type();
            if (t == tid) {
                Description* c = list[i];
                if (Object::IsDerivedFromNullSafe(c, tid)) found = c;
                break;
            }
            ++i;
            if (i >= n) break;
        }
    }
    *retval = found;
    return retval;
}

// ============================================================
// 0x1401193a0  ConsumeProduceAction_helper_93a0
// ============================================================
// RCX is a ConsumeProduceComponent (the address sits among its bodies; its header
// is COMP-e's, so this stays free). Finds the ConsumeProduceDescription among the
// sub-descriptions of the entity's description (+0x28/+0x30, NO CheckPtr), then:
//   if (found->+0x48) < 0 -> return true
//   else                  -> return consumeState (+0x68) < (found->+0x48)
// (matches FUN_1401193a0 disasm: tail compares dword[R15+0x68] with dword[found+0x48])
// LEFT: the caller (ConsumeProduce, WORK-a) hands the component as an integer, and
// ConsumeProduceDescription +0x48 is read raw (not declared in its header).
bool WorkLeaf_ConsumeProduce_93a0(uintptr_t self) {
    ConsumeProduceComponent* comp = reinterpret_cast<ConsumeProduceComponent*>(self);
    const uint32_t tid = ConsumeProduceDescription::TypeId(); // 0x1401c7388

    void* found = nullptr;
    ComponentDescription* desc = comp->_entity->_description;
    if (desc->_subDescCount > 0) {
        Description** list = desc->_subDescs;
        int n = desc->_subDescCount;
        int i = 0;
        for (;;) {
            Description* sub = desc->_subDescs[i];
            uint32_t t = sub->Type();
            if (t == tid) {
                Description* c = list[i];
                if (Object::IsDerivedFromNullSafe(c, tid)) found = c;
                break;
            }
            ++i;
            if (i >= n) break;
        }
    }
    int capacity = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(found) + 0x48);
    if (capacity < 0) return true;
    return comp->consumeState < capacity;
}

// ============================================================
// 0x140128350  PastureWork_helper_8350
// ============================================================
// RCX is a PastureComponent (a member of it; its header is COMP-d's). For each of
// its livestock, find the AnimalComponent on the animal's entity, and track the
// index whose age (+0x44) is the strict maximum among those that (a) do not have
// bit 20 of their flags (+0x20) set and (b) exceed their AnimalDescription's
// +0x30 threshold. Returns the best index, or -1 if none.
// ABI: int fn(longlong self).
// LEFT: the caller (PastureWork, WORK-b) hands the component as an integer.
int WorkLeaf_Pasture_8350(uintptr_t self) {
    PastureComponent* pasture = reinterpret_cast<PastureComponent*>(self);
    const uint32_t tidThresh = AnimalDescription::TypeId(); // 0x1401c6fd0

    int bestIndex = -1;
    float bestScore = 0.0f;
    int n = pasture->animalCount;
    if (n <= 0) return -1;

    for (int outer = 0; outer < n; ++outer) {
        LivestockComponent* livestock = pasture->animals[outer];

        // The AnimalComponent (0x1401c6f98) of the animal's entity.
        AnimalComponent* sub = livestock->_entity->FindComponent<AnimalComponent>();

        // Candidate score = the animal's age. Keep only if strictly greater than current best.
        if (bestScore < sub->age) {
            // Skip if bit 20 of the flags is set.
            if ((sub->_flags >> 0x14 & 1) == 0) {
                float score = sub->age;
                Description* obj = sub->_description;
                AnimalDescription* derived = nullptr;
                if (Object::IsDerivedFromNullSafe(obj, tidThresh)) derived = static_cast<AnimalDescription*>(obj);
                // Keep only if score strictly exceeds the description's threshold.
                float threshold = derived->_f30;
                if (threshold <= score && score != threshold) {
                    bestScore = sub->age;
                    bestIndex = outer;
                }
            }
        }
    }
    return bestIndex;
}

// ============================================================
// 0x14012b310  ConsumeProduceAction_helper_b310
// ============================================================
// RCX is a ResourceProduceComponent (+0x48 produceFlag; the address sits among its
// bodies; its header is COMP-e's, so this stays free). Sets produceFlag = 1, finds the
// entity's StateMachineComponent (0x140003ea0), then if its state (the pending state,
// or the input when none is pending) is not 0x0e: when the active state is negative,
// tick the machine with a zero Time (StateMachine::Tick 0x1400a9660, useAlt 0, run 0),
// then ask the current controller node for the transition from the active state
// (Controller slot 1 FindTransition, 0x14012b3a4 `CALL [RAX+0x8]`, out int = 0x0e by
// default). On success (>= 0) store the transition (+0x53) and the queried state (+0x52).
// LEFT: the caller (ConsumeProduce, WORK-a) hands the component as an integer.
void WorkLeaf_ConsumeProduce_b310(uintptr_t self) {
    ResourceProduceComponent* producer = reinterpret_cast<ResourceProduceComponent*>(self);
    producer->produceFlag = 1;

    StateMachineComponent* machine = producer->_entity->FindComponent<StateMachineComponent>();   // 0x140003ea0
    StateMachine& sm = machine->stateMachine;

    char state = sm.pendingState;
    if (state < 0) state = sm.currentInput;
    if (state == 0x0e) return;

    int outVal = 0x0e; // local_res8[0]

    // If the active state is negative, tick the SM with a zero Time first.
    if (sm.activeState < 0) {
        // System::Time::Time(&t, 0.0f), then StateMachine::Tick(&t, 0, 0): retail
        // passes the Time's ADDRESS (0x14012b380 `LEA RDX,[RSP+0x40]`); with run = 0
        // Tick never reads it.
        System::Time timeVal = {0};
        RtSystem_Time_Ctor(&timeVal, 0.0f);
        sm.Tick(&timeVal, 0, 0);
    }

    // Controller slot 1: int FindTransition(int stateIndex, int* desiredStateId)
    int result = sm.current->FindTransition(static_cast<int>(sm.activeState), &outVal);
    if (result >= 0) {
        sm.transitionState = static_cast<int8_t>(result);
        sm.pendingState = static_cast<int8_t>(outVal);
    }
}
