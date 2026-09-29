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
// FindComponent (+0x28/+0x30, NO CheckPtr), typeId @0x1401c4870.
// Returns the raw component pointer directly via *retval (NOT an IBP Ctor).
// ABI: undefined8* fn(longlong entity, undefined8* retval).
void* WorkLeaf_FindComp_63d0(uintptr_t entity, void** retval) {
    const uint32_t tid = RadiusDescription::TypeId(); // 0x1401c4870
    void* found = nullptr;
    if (*reinterpret_cast<int*>(entity + 0x30) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x28);
        int n = *reinterpret_cast<int*>(entity + 0x30);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            // No CheckPtr in this variant.
            void* comp = *reinterpret_cast<void**>(off + *reinterpret_cast<uintptr_t*>(entity + 0x28));
            uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
            if (t == tid) {
                void* c = *reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (Object::IsDerivedFromNullSafe(c, tid)) found = c;
                break;
            }
            ++i;
            off += 8;
            if (i >= n) break;
        }
    }
    *retval = found;
    return retval;
}

// ============================================================
// 0x1401193a0  ConsumeProduceAction_helper_93a0
// ============================================================
// Resolves the entity behind the action's IBP at self+0x10, finds a component of type
// @0x1401c7388 in that entity's list (+0x28/+0x30, NO CheckPtr), then returns a bool:
//   if (foundComp+0x48) < 0 -> return true
//   else                    -> return (self+0x68) < (foundComp+0x48)
// (matches FUN_1401193a0 disasm: tail compares dword[R15+0x68] with dword[found+0x48])
// ABI: bool fn(longlong self).
bool WorkLeaf_ConsumeProduce_93a0(uintptr_t self) {
    const uint32_t tid = ConsumeProduceDescription::TypeId(); // 0x1401c7388

    void* found = nullptr;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(self + 0x10) + 0x28);
    if (*reinterpret_cast<int*>(entity + 0x30) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x28);
        int n = *reinterpret_cast<int*>(entity + 0x30);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            void* comp = *reinterpret_cast<void**>(off + *reinterpret_cast<uintptr_t*>(entity + 0x28));
            uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
            if (t == tid) {
                void* c = *reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (Object::IsDerivedFromNullSafe(c, tid)) found = c;
                break;
            }
            ++i;
            off += 8;
            if (i >= n) break;
        }
    }
    int capacity = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(found) + 0x48);
    if (capacity < 0) return true;
    return *reinterpret_cast<int*>(self + 0x68) < capacity;
}

// ============================================================
// 0x140128350  PastureWork_helper_8350
// ============================================================
// Scans the work entity's component list (self+0x30/+0x38). For each component it
// dereferences a nested entity IBP at component+0x10, finds a sub-component of type
// @0x1401c6f98 in THAT entity (list +0x30/+0x38, WITH CheckPtr per slot), builds an
// IBP for it, and tracks the index whose sub-component+0x44 (a float "score") is the
// strict maximum among those that (a) have not got bit 20 of +0x20 set and
// (b) exceed the derived (@0x1401c6fd0) object's +0x30 threshold.
// Returns the best outer index, or -1 if none.
// ABI: int fn(longlong self).
int WorkLeaf_Pasture_8350(uintptr_t self) {
    const uint32_t tidInner = AnimalComponent::TypeId();   // 0x1401c6f98
    const uint32_t tidThresh = AnimalDescription::TypeId(); // 0x1401c6fd0

    int bestIndex = -1;
    float bestScore = 0.0f;
    int n = *reinterpret_cast<int*>(self + 0x38);
    if (n <= 0) return -1;

    int64_t outerOff = 0;
    for (int outer = 0; outer < n; ++outer, outerOff += 8) {
        uintptr_t* compSlot = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(self + 0x30) + outerOff);

        // Nested entity IBP at component+0x10.
        uintptr_t innerEnt = *reinterpret_cast<uintptr_t*>(*compSlot + 0x10);

        // Find sub-component of type tidInner in innerEnt's list (+0x30/+0x38).
        void* sub = nullptr;
        int innerCount = *reinterpret_cast<int*>(innerEnt + 0x38);
        if (innerCount > 0) {
            int64_t innerOff = 0;
            for (int j = 0; j < innerCount; ++j, innerOff += 8) {
                uintptr_t* slot = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(innerEnt + 0x30) + innerOff);
                void* comp = reinterpret_cast<void*>(*slot);
                uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
                if (t == tidInner) {
                    uintptr_t* slot2 = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(innerEnt + 0x30) + innerOff);
                    void* c = reinterpret_cast<void*>(*slot2);
                    sub = (Object::IsDerivedFromNullSafe(c, tidInner)) ? c : nullptr;
                    break;
                }
            }
        }

        // Wrap the sub-component in an IBP buffer (matches the exe's local_res8).
        uintptr_t subIBP = 0;
        new (&subIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(sub));   // B.3.3 bridge

        // Candidate score = subIBP+0x44. Keep only if strictly greater than current best.
        if (bestScore < *reinterpret_cast<float*>(subIBP + 0x44)) {
            // Skip if bit 20 of +0x20 is set.
            if ((*reinterpret_cast<uint32_t*>(subIBP + 0x20) >> 0x14 & 1) == 0) {
                float score = *reinterpret_cast<float*>(subIBP + 0x44);
                void* obj = *reinterpret_cast<void**>(subIBP + 0x18);
                void* derived = nullptr;
                if (Object::IsDerivedFromNullSafe(obj, tidThresh)) derived = obj;
                // Keep only if score strictly exceeds derived+0x30 (threshold).
                float threshold = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(derived) + 0x30);
                if (threshold <= score && score != threshold) {
                    bestScore = *reinterpret_cast<float*>(subIBP + 0x44);
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
// Sets self+0x48 = 1, resolves an entity (FUN_140003ea0) behind self+0x10, then if its
// "consume" state byte (+0x52, or +0x50 if +0x52 has high bit set) is not 0x0e, runs a
// state-machine query: optionally tick the SM (StateMachine_Tick @0x1400a9660 with a
// zero Time when +0x51 has high bit set), then call the SM vtable +0x8 query passing the
// current event id (+0x51) and an out int (= 0x0e default). On success (>= 0) stores the
// result event (+0x53) and the queried state (+0x52).
// Closure: FUN_140003ea0 (registered), StateMachine_Tick 0x1400a9660 (registered),
//          System::Time::Time float-ctor + InternalBasePtr CheckPtr (engine).
// ABI: void fn(longlong self).
// FUN_140003ea0 = PlayerPath_FindComponent_3ea0 (Player/exe_bodies.h), 0x1400a9660 =
// StateMachine_TickReimpl (Component/controller_state_table.h): called by name (B3R).
void WorkLeaf_ConsumeProduce_b310(uintptr_t self) {
    *reinterpret_cast<uint8_t*>(self + 0x48) = 1;

    uintptr_t ent = 0;
    PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(self + 0x10), &ent);

    char state = *reinterpret_cast<char*>(ent + 0x52);
    if (state < 0) state = *reinterpret_cast<char*>(ent + 0x50);
    if (state == 0x0e) return;

    int outVal = 0x0e; // local_res8[0]

    // If event byte +0x51 has its high bit set, tick the SM with a zero Time first.
    if (*reinterpret_cast<char*>(ent + 0x51) < 0) {
        // Build a zero Time (System::Time::Time(&t, 0.0f)) and tick.
        uint8_t timeBuf[16] = {0};
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), 0.0f);
        {
            // StateMachine_Tick(&(ent+0x48), Time, 0, 0). The SM object is at ent+0x48.
            int64_t t = *reinterpret_cast<int64_t*>(timeBuf);
            StateMachine_TickReimpl(reinterpret_cast<void*>(ent + 0x48), t, 0, 0);
        }
    }

    // SM vtable +0x8 query: int (*)(sm, int eventId, int* outVal)
    void* sm = *reinterpret_cast<void**>(ent + 0x48);
    typedef int (*SMQuery_t)(void* sm, int eventId, int* outVal);
    SMQuery_t query = *reinterpret_cast<SMQuery_t*>(*reinterpret_cast<uintptr_t*>(sm) + 0x8);
    int result = query(sm, static_cast<int>(*reinterpret_cast<char*>(ent + 0x51)), &outVal);
    if (result >= 0) {
        *reinterpret_cast<char*>(ent + 0x53) = static_cast<char>(result);
        *reinterpret_cast<uint8_t*>(ent + 0x52) = static_cast<uint8_t>(outVal);
    }
}
