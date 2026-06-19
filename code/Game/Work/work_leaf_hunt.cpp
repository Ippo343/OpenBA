// code/Game/Work/work_leaf_hunt.cpp
// Wave (Game-gap / Work-leaf) — faithful 1:1 ports of small exe helper functions
// referenced via ResolveAddr(Module::Exe, 0xADDR) from the Hunt / HarvestOrchard /
// Pasture / ConsumeProduce work classes. In DLL mode each ResolveAddr returns the
// original exe code; in standalone mode it returns 0 (silently skipped) unless the
// address is registered. This file provides the clean reimplementations so the
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
// This file must NOT depend on hooks.h — it is portable for the standalone exe.
// Bodies are unconditional so it compiles in BOTH dll and standalone builds.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

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
static const uint32_t* s_tid_a730 = nullptr; // 0x1401c7890
void* WorkLeaf_FindComp_a730(uintptr_t entity, void* retIBP) {
    if (!s_tid_a730)
        s_tid_a730 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7890));
    void* found = nullptr;
    if (s_tid_a730 && *reinterpret_cast<int*>(entity + 0x38) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        int n = *reinterpret_cast<int*>(entity + 0x38);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            void** slot = reinterpret_cast<void**>(list + off);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_tid_a730) {
                void** slot2 = reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
                void* c = *slot2;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_tid_a730)) found = c;
                break;
            }
            ++i;
            off += 8;
            if (i >= n) { found = nullptr; break; }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// ---- 0x14000a0b0  FindComponent (+0x30/+0x38, WITH CheckPtr) -------------------
// typeId @0x1401c4250. (HarvestOrchard "DescComp" finder.)
static const uint32_t* s_tid_a0b0 = nullptr; // 0x1401c4250
void* WorkLeaf_FindComp_a0b0(uintptr_t entity, void* retIBP) {
    if (!s_tid_a0b0)
        s_tid_a0b0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4250));
    void* found = nullptr;
    if (s_tid_a0b0 && *reinterpret_cast<int*>(entity + 0x38) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        int n = *reinterpret_cast<int*>(entity + 0x38);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            void** slot = reinterpret_cast<void**>(list + off);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_tid_a0b0) {
                void** slot2 = reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
                void* c = *slot2;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_tid_a0b0)) found = c;
                break;
            }
            ++i;
            off += 8;
            if (i >= n) { found = nullptr; break; }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// ---- 0x140004fe0  FindComponent (+0x30/+0x38, WITH CheckPtr) -------------------
// typeId @0x1401c7ba0. (Pasture "RawMaterialDescription" finder.)
static const uint32_t* s_tid_4fe0 = nullptr; // 0x1401c7ba0
void* WorkLeaf_FindComp_4fe0(uintptr_t entity, void* retIBP) {
    if (!s_tid_4fe0)
        s_tid_4fe0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ba0));
    void* found = nullptr;
    if (s_tid_4fe0 && *reinterpret_cast<int*>(entity + 0x38) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        int n = *reinterpret_cast<int*>(entity + 0x38);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            void** slot = reinterpret_cast<void**>(list + off);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_tid_4fe0) {
                void** slot2 = reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
                void* c = *slot2;
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_tid_4fe0)) found = c;
                break;
            }
            ++i;
            off += 8;
            if (i >= n) { found = nullptr; break; }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// ---- 0x1400063d0  HuntWork_helper_63d0  ----------------------------------------
// FindComponent (+0x28/+0x30, NO CheckPtr), typeId @0x1401c4870.
// Returns the raw component pointer directly via *retval (NOT an IBP Ctor).
// ABI: undefined8* fn(longlong entity, undefined8* retval).
static const uint32_t* s_tid_63d0 = nullptr; // 0x1401c4870
void* WorkLeaf_FindComp_63d0(uintptr_t entity, void** retval) {
    if (!s_tid_63d0)
        s_tid_63d0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4870));
    void* found = nullptr;
    if (s_tid_63d0 && *reinterpret_cast<int*>(entity + 0x30) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x28);
        int n = *reinterpret_cast<int*>(entity + 0x30);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            // No CheckPtr in this variant.
            void* comp = *reinterpret_cast<void**>(off + *reinterpret_cast<uintptr_t*>(entity + 0x28));
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_tid_63d0) {
                void* c = *reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_tid_63d0)) found = c;
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
    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    static const uint32_t* s_tid = nullptr; // 0x1401c7388
    if (!s_tid)
        s_tid = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7388));

    void* found = nullptr;
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(self + 0x10) + 0x28);
    if (s_tid && *reinterpret_cast<int*>(entity + 0x30) > 0) {
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x28);
        int n = *reinterpret_cast<int*>(entity + 0x30);
        int i = 0;
        int64_t off = 0;
        for (;;) {
            void* comp = *reinterpret_cast<void**>(off + *reinterpret_cast<uintptr_t*>(entity + 0x28));
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_tid) {
                void* c = *reinterpret_cast<void**>(list + (int64_t)i * 8);
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_tid)) found = c;
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
    static const uint32_t* s_tidInner = nullptr; // 0x1401c6f98
    static const uint32_t* s_tidThresh = nullptr; // 0x1401c6fd0
    if (!s_tidInner)
        s_tidInner = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6f98));
    if (!s_tidThresh)
        s_tidThresh = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6fd0));

    int bestIndex = -1;
    float bestScore = 0.0f;
    int n = *reinterpret_cast<int*>(self + 0x38);
    if (n <= 0) return -1;

    int64_t outerOff = 0;
    for (int outer = 0; outer < n; ++outer, outerOff += 8) {
        uintptr_t* compSlot = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(self + 0x30) + outerOff);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(compSlot);

        // Nested entity IBP at component+0x10.
        uintptr_t innerEnt = *reinterpret_cast<uintptr_t*>(*compSlot + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&innerEnt);

        // Find sub-component of type s_tidInner in innerEnt's list (+0x30/+0x38).
        void* sub = nullptr;
        int innerCount = *reinterpret_cast<int*>(innerEnt + 0x38);
        if (s_tidInner && innerCount > 0) {
            int64_t innerOff = 0;
            for (int j = 0; j < innerCount; ++j, innerOff += 8) {
                uintptr_t* slot = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(innerEnt + 0x30) + innerOff);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* comp = reinterpret_cast<void*>(*slot);
                uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
                if (t == *s_tidInner) {
                    uintptr_t* slot2 = reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(innerEnt + 0x30) + innerOff);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
                    void* c = reinterpret_cast<void*>(*slot2);
                    sub = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_tidInner)) ? c : nullptr;
                    break;
                }
            }
        }

        // Wrap the sub-component in an IBP buffer (matches the exe's local_res8).
        uintptr_t subIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&subIBP, sub);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&subIBP);

        // Candidate score = subIBP+0x44. Keep only if strictly greater than current best.
        if (bestScore < *reinterpret_cast<float*>(subIBP + 0x44)) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&subIBP);
            // Skip if bit 20 of +0x20 is set.
            if ((*reinterpret_cast<uint32_t*>(subIBP + 0x20) >> 0x14 & 1) == 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&subIBP);
                float score = *reinterpret_cast<float*>(subIBP + 0x44);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&subIBP);
                void* obj = *reinterpret_cast<void**>(subIBP + 0x18);
                void* derived = nullptr;
                if (s_tidThresh && fn_IsDerivedFrom && fn_IsDerivedFrom(obj, *s_tidThresh)) derived = obj;
                // Keep only if score strictly exceeds derived+0x30 (threshold).
                float threshold = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(derived) + 0x30);
                if (threshold <= score && score != threshold) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&subIBP);
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
typedef void* (*WL_FindEntity_3ea0_t)(uintptr_t self, void* retIBP);
typedef void (*WL_StateMachine_Tick_t)(void* sm, int64_t deltaTime, char p3, char p4);
static WL_FindEntity_3ea0_t s_wl_findEntity_3ea0 = nullptr;
static WL_StateMachine_Tick_t s_wl_smTick = nullptr;
static bool s_wl_b310_init = false;
static void WorkLeaf_EnsureB310() {
    if (s_wl_b310_init) return;
    s_wl_b310_init = true;
    s_wl_findEntity_3ea0 = reinterpret_cast<WL_FindEntity_3ea0_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_wl_smTick = reinterpret_cast<WL_StateMachine_Tick_t>(ResolveAddr(Module::Exe, 0x1400a9660));
}
void WorkLeaf_ConsumeProduce_b310(uintptr_t self) {
    WorkLeaf_EnsureB310();
    *reinterpret_cast<uint8_t*>(self + 0x48) = 1;

    if (fn_InternalBasePtr_CheckPtr)
        fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
    uintptr_t ent = 0;
    if (s_wl_findEntity_3ea0)
        s_wl_findEntity_3ea0(*reinterpret_cast<uintptr_t*>(self + 0x10), &ent);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ent);

    char state = *reinterpret_cast<char*>(ent + 0x52);
    if (state < 0) state = *reinterpret_cast<char*>(ent + 0x50);
    if (state == 0x0e) return;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ent);
    int outVal = 0x0e; // local_res8[0]

    // If event byte +0x51 has its high bit set, tick the SM with a zero Time first.
    if (*reinterpret_cast<char*>(ent + 0x51) < 0) {
        // Build a zero Time (System::Time::Time(&t, 0.0f)) and tick.
        uint8_t timeBuf[16] = {0};
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeBuf, 0.0f);
        if (s_wl_smTick) {
            // StateMachine_Tick(&(ent+0x48), Time, 0, 0). The SM object is at ent+0x48.
            int64_t t = *reinterpret_cast<int64_t*>(timeBuf);
            s_wl_smTick(reinterpret_cast<void*>(ent + 0x48), t, 0, 0);
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
