// code/Game/Component/citizenai_candidatelist.cpp
//
// Faithful C++ port of the CitizenAIComponent candidate-state / need-check
// subsystem from Application-x64.exe (the SEPARATE, self-contained citizen
// action-state machine — NOT the building-controller SM).
//
// Functions ported (all faithful to the Ghidra decompile + disassembly):
//   0x14002fe10  SortByPriority                 -> CitizenAI_SortByPriority
//   0x1401156e0  CitizenComponent_HasSufficientResources  -> CitizenComp_HasSufficientResources
//   0x1401157f0  CitizenComponent_HasSufficientResources2 -> CitizenComp_HasSufficientResources2
//   0x140115de0  CitizenComponent_GetHappiness  -> CitizenComp_GetHappiness
//   0x14010dca0  CitizenAIComponent_BuildCandidateStateList -> CitizenAI_BuildCandidateStateList
//   0x14010f1f0  CitizenAIComponent_ValidateAndEnterState   -> CitizenAI_ValidateAndEnterState
//
// Sub-callees reused via ResolveAddr (already registered reimpls):
//   0x140008090  FindComponent_Residence  (Ghidra: "Entity_FindComponent_Storage")
//   0x140006270  FindComponent_Storage    (Ghidra: same name -- see the typedefs)
//   0x14012a9d0  Storage_GetResourceCount
//   0x14012ab00  Storage_GetResourceCount2
//   0x14012cf10  StorageComponent_QueryCount
//   0x140115be0  CitizenComponent_CanFindHousing
//   the 14 CitizenAI_State_* handlers (ValidateAndEnterState dispatch)
//


#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_constants.h"
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

// ============================================================
// 1. SortByPriority (0x14002fe10)
// ------------------------------------------------------------
// Sorts an array of 12-byte (0xc) triples {int id, int priority, int unused}
// by the PRIORITY field (offset +4) ASCENDING, in place. The original is an
// introsort; n is tiny (<=14) so a behaviorally-equivalent insertion sort by
// priority ascending is used (final ascending order is what matters). The
// original is a STABLE-ish quicksort+heapsort hybrid; insertion sort is stable.
// Signature matches the original (base, count); the original's 3rd scratch arg
// (R8) is unused by the callee.
// ============================================================
void CitizenAI_SortByPriority(void* base, int count) {
    if (!base || count < 2) return;
    int32_t* a = reinterpret_cast<int32_t*>(base);  // triples: a[3*k+0]=id, +1=prio, +2=unused
    // DESCENDING by priority (highest at index 0). The original (0x14002fe10) is an
    // introsort whose small-array pass selection-sorts the MINIMUM to the END, i.e.
    // it sorts descending. TransitionState (0x14010db90) iterates the result
    // front-to-back and enters the first state whose handler pushes an action, so
    // the HIGHEST-priority satisfiable state must come first. (A prior port had this
    // ascending, which made citizens always pick the lowest-priority state — e.g.
    // Leisure/Wander over Work/needs.) Stable insertion sort, descending.
    for (int i = 1; i < count; i++) {
        int32_t id   = a[3 * i + 0];
        int32_t prio = a[3 * i + 1];
        int32_t unu  = a[3 * i + 2];
        int j = i - 1;
        while (j >= 0 && a[3 * j + 1] < prio) {
            a[3 * (j + 1) + 0] = a[3 * j + 0];
            a[3 * (j + 1) + 1] = a[3 * j + 1];
            a[3 * (j + 1) + 2] = a[3 * j + 2];
            j--;
        }
        a[3 * (j + 1) + 0] = id;
        a[3 * (j + 1) + 1] = prio;
        a[3 * (j + 1) + 2] = unu;
    }
}

// ============================================================
// Resolved sub-callees (all already registered reimpls in standalone).
// ============================================================

// ⚠️ TWO different component finders, both named "Entity_FindComponent_Storage" by
// Ghidra. They are NOT interchangeable — they search for different type ids, and
// Ghidra's DECOMPILER renders a call to either one under the same name, so the
// pseudocode cannot tell them apart. Always check the DISASSEMBLY's CALL target:
//   0x140008090 -> reads [0x1401c7cf0] = ResidenceComponent  (1400080d3: MOV EDI,...)
//   0x140006270 -> reads [0x1401c7e78] = StorageComponent    (1400062b3: MOV EDI,...)
// (ownership per standalone/factory_registrations.csv; ResidenceComponent derives
// from StorageComponent, which is why both feed StorageComponent::QueryCount and
// the confusion is easy to miss.)
//
// Called by name:
//   FindComponent_Residence(entity, outIBP) -> InternalBasePtr* (0x140008090)
//     CitizenComp_Entity_FindComponent_Storage
//   FindComponent_Storage(entity, outIBP) -> InternalBasePtr* (0x140006270)
//     WorkBase_FindComponent_6270
//   Storage_GetResourceCount(storageComp) -> int (0x14012a9d0)
//     CitizenComp_Storage_GetResourceCount
//   Storage_GetResourceCount2(storageComp) -> int (0x14012ab00)
//   StorageComponent_QueryCount(storageComp) -> int (0x14012cf10)
//     CitizenComp_StorageComponent_QueryCount
//   CitizenComponent_CanFindHousing(citizenComp) -> char (0x140115be0)
//     CitizenComp_CanFindHousing
// Wander-time .rdata float constants (read by BuildCandidateStateList case 0xc):
// kRdata_1401721fc (75.0), kRdata_140172234 (300.0), kRdata_140172254 (1800.0, bits).
static inline float RdataF(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

// ============================================================
// 2. CitizenComponent_HasSufficientResources (0x1401156e0)
// ------------------------------------------------------------
// self (CitizenComponent) at +0x238 = workplace IBP. Early-false if zero.
// Counts non-empty inventory rows of the workplace storage, multiplies by the
// per-slot requirement (storage[+0x50] row at index citizen[+0x240]) and the
// CitizenDescription[+0xc4] factor (truncated to int), then compares against the
// workplace's total GetResourceCount.
//   iVar2 = *(int*)( *(int64*)(storageA[+0x50]) + 8 + citizen[+0x240]*0x10 )
//   fVar1 = *(float*)( citizen[+0x288] + 0xc4 )   -> CVTTSS2SI (trunc to int)
//   iVar7 = count of rows in storageA where slot count (+8, stride 0x10) > 0
//   iVar4 = Storage_GetResourceCount(storageA)
//   return  iVar7 * iVar2 * (int)fVar1 <= iVar4
// (Entity_FindComponent_Storage is called twice on the same workplace; the
//  first result feeds GetResourceCount, the second feeds the row scan / iVar2.
//  Both resolve to the same storage component — ported faithfully.)
// ============================================================
int CitizenComp_HasSufficientResources(int64_t self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    if (*reinterpret_cast<int64_t*>(p + 0x238) == 0) return 0;

    // local_res8 = first find (feeds GetResourceCount + row scan)
    int64_t local_res8 = 0;
    CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(p + 0x238), &local_res8);

    int64_t lVar3desc = *reinterpret_cast<int64_t*>(p + 0x288);  // CitizenDescription

    // local_res10 = second find (feeds the per-slot requirement read)
    int64_t local_res10 = 0;
    void* self2 = CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(p + 0x238), &local_res10);

    // iVar2 = *(int*)( *(int64*)(*self2 + 0x50) + 8 + citizen[+0x240]*0x10 )
    // [STANDALONE GUARD] FindComponentStorage returns a non-null IBP wrapper but a
    // null resolved component when the StorageComponent type-id global is unfilled
    // (the recurring standalone limitation). Guard storageObj/local_res8 so the
    // workplace-storage scans can't deref null+0x50/+0x58.
    int iVar2 = 0;
    if (self2) {
        int64_t storageObj = *reinterpret_cast<int64_t*>(self2);
        if (storageObj) {
            int64_t rows = *reinterpret_cast<int64_t*>(storageObj + 0x50);
            int idx240 = *reinterpret_cast<int*>(p + 0x240);
            iVar2 = *reinterpret_cast<int*>(rows + 8 + (int64_t)idx240 * 0x10);
        }
    }
    float fVar1 = *reinterpret_cast<float*>(lVar3desc + 0xc4);   // CVTTSS2SI = trunc
    int factor = (int)fVar1;

    // Row scan over local_res8.
    int64_t lVar3 = local_res8;  // storage obj (for GetResourceCount)
    if (local_res8 == 0) return 0;   // no storage resolved -> "insufficient"
    int rowCount = *reinterpret_cast<int*>(local_res8 + 0x58);
    int iVar7 = 0;
    if (0 < rowCount) {
        int* piVar5 = reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(local_res8 + 0x50) + 8);
        int64_t lVar6 = rowCount;
        do {
            if (0 < *piVar5) iVar7++;
            piVar5 += 4;       // stride 0x10
            lVar6--;
        } while (lVar6 != 0);
    }

    int iVar4 = CitizenComp_Storage_GetResourceCount(lVar3);
    return (iVar7 * iVar2 * factor <= iVar4) ? 1 : 0;
}

// ============================================================
// 3. CitizenComponent_HasSufficientResources2 (0x1401157f0)
// ------------------------------------------------------------
// Like (2) but a single find, uses Storage_GetResourceCount2, a float threshold
// at CitizenDescription[+0xd4], and a FLOAT comparison:
//   iVar6 = count of rows where slot count (+8, stride 0x10) > 0
//   iVar3 = Storage_GetResourceCount2(storage)
//   return (float)iVar6 * desc[+0xd4] <= (float)iVar3       (COMISS / SETNC)
// ============================================================
int CitizenComp_HasSufficientResources2(int64_t self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    if (*reinterpret_cast<int64_t*>(p + 0x238) == 0) return 0;

    int64_t local_res8 = 0;
    CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(p + 0x238), &local_res8);

    int64_t lVar2 = local_res8;                                   // storage obj
    int64_t lVar1desc = *reinterpret_cast<int64_t*>(p + 0x288);   // CitizenDescription

    if (local_res8 == 0) return 0;   // [STANDALONE GUARD] no storage resolved
    int rowCount = *reinterpret_cast<int*>(local_res8 + 0x58);
    int iVar6 = 0;
    if (0 < rowCount) {
        int* piVar4 = reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(local_res8 + 0x50) + 8);
        int64_t lVar5 = rowCount;
        do {
            if (0 < *piVar4) iVar6++;
            piVar4 += 4;       // stride 0x10
            lVar5--;
        } while (lVar5 != 0);
    }

    int iVar3 = Storage_GetResourceCount2(lVar2);
    float threshold = *reinterpret_cast<float*>(lVar1desc + 0xd4);
    // Distribution diagnostic (firewood-evenness): per-house fuel sufficiency
    // inputs — occupied roster rows x CitizenDesc+0xd4 vs fuel units on hand.
    { static int n = 0;
      const bool early = (n < 12);
      if (early) { ++n;
        fprintf(stderr, "[HSR2] residence=%llx occupied=%d perOcc=%.1f have=%d -> %s\n",
                (unsigned long long)local_res8, iVar6, threshold, iVar3,
                ((float)iVar6 * threshold <= (float)iVar3) ? "sufficient" : "FETCH");
        fflush(stderr); } }
    // COMISS XMM1(=(float)iVar3), XMM0(=(float)iVar6*threshold); SETNC -> iVar3 >= product
    return ((float)iVar6 * threshold <= (float)iVar3) ? 1 : 0;
}

// ============================================================
// 4. CitizenComponent_GetHappiness (0x140115de0) — MISNAMED: this is CARRY COUNT
// ------------------------------------------------------------
// Finds the citizen's own-entity StorageComponent (self+0x10 = the owner entity)
// and tail-returns StorageComponent_QueryCount's result (Ghidra shows void; the
// function tail-calls QueryCount, whose EAX is the return). Nothing to do with
// happiness — it answers "how much am I carrying?", and it is the gate on state
// 0xd (LongTermPlan) = the haul-to-storage leg. Name kept to match Ghidra.
//
// 🐞 The finder here is 0x140006270 (StorageComponent), NOT the 0x140008090
// (ResidenceComponent) used by (2)/(3) above — see the note at the typedefs. This
// port originally called 0x140008090, which cannot match on a citizen entity:
// Template/Citizen.rsc declares `StorageDescription storage` and no residence, so
// the find always returned null, every citizen read as carrying nothing, state 0xd
// was never appended as a candidate, and NOTHING WAS EVER DELIVERED to a barn —
// silently, since the null was absorbed by the guard below. Verified against the
// disassembly, which the decompiler hides (both finders print under one name):
//   140115dfb: CALL 0x140006270
// ============================================================
int CitizenComp_GetHappiness(int64_t self) {
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void* local_res8[4] = {nullptr, nullptr, nullptr, nullptr};

    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(p + 0x10),
                                reinterpret_cast<void*>(local_res8));
    // Standalone: the citizen's own-entity StorageComponent find returns null when
    // its type-id global is unfilled (the recurring keystone-B pattern). The real
    // game always has a carry-inventory storage here; guard so QueryCount(null) -
    // which derefs +0x30 - can't fault. No storage found => count 0.
    //
    // This is NOT a cosmetic guard: despite the inherited Ghidra name, this function
    // is the citizen's CARRY COUNT (body = FindComponent_Storage -> QueryCount), and
    // it is the gate on state 0xd (LongTermPlan) = the haul-to-storage leg. If the
    // find ever returns null here, every citizen reads as "carrying nothing" and
    // NOBODY EVER DELIVERS, silently. So say so loudly (once) rather than let a
    // null-guard mask it -- root-cause the find, do not paper over it.
    if (!local_res8[0]) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            std::fprintf(stderr,
                "[Deliver] WARNING: citizen carry StorageComponent find (0x140006270) "
                "returned NULL -- carry count reads 0 for every citizen, so state 0xd "
                "(LongTermPlan / haul-to-storage) can never be a candidate and nothing "
                "will ever be delivered. Root-cause the find; do not null-guard it.\n");
            std::fflush(stderr);
        }
        return 0;
    }
    return CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(local_res8[0]));
}

// ============================================================
// 5. CitizenAIComponent_BuildCandidateStateList (0x14010dca0)
// ------------------------------------------------------------
// outList layout: outList[0]=ptr(8B), +8=count(int), +0xc=capacity(int).
// Appends {id,priority,0} triples (grow rule reproduced exactly), guarded by
// per-state need conditions, then Fisher-Yates shuffles and SortByPriority.
//
// "self" arg is the CitizenAIComponent; self+0x38 = sibling-CitizenComponent IBP
// (Ghidra "self" local = param_1+0x38). *self38 dereferences to the
// CitizenComponent; its fields: +0x44 hunger, +0x48 thirst, +0x50/+0x54 work,
// +0x68 Disease* (NOT a housing link -- see below), +0x80 leisure-accum,
// +0x208 elder-flag, +0x228 care-building link, +0x238
// workplace, +0x248 house, +0x250 reservation, +0x288 CitizenDescription, +0x1e4 edu, +0xac,
// +0x3c age, +0xb0 reservation-kind.  param_1+0xc0 = leisure Time accumulator.
//
// +0x68 IS THE DISEASE POINTER, NOT A HOUSING LINK (settled 2026-09-06).
// CitizenComponent::Infect (0x140116b90, ported in citizen_updatehealth.cpp) stores the
// DiseaseDescription* at +0x68 and then calls FindCareBuilding (0x140111590), which fills
// +0x228; RecoverOrDie clears +0x68 again and AnyCitizenSick (0x1400893c0) scans it.
// So every `+0x68` test in this function is "is this citizen SICK?":
//   * early single-candidate branch  = sick AND a care building is linked -> nothing but
//                                      state 5 (drops every other need)
//   * main branch `+0x68 == 0`       = healthy -> FindHousing / Education / GrowUp
//     else                            = sick    -> state 5 at prio 6
//   * elder arm `+0x68 == 0`         = FindWork only while healthy
// State 5's handler (0x14010d790) walks to the care building's entity (+0x228 -> +0x10)
// when one is linked, and otherwise navigates to +0x238. We carry it under the exe-derived
// name `CitizenAI_State_Work`, but its ONLY entry condition is illness; read it as the
// sick-citizen state. No string in Application-x64-profile.exe names it, so the symbol
// stays as-is rather than being renamed to a guess.
// ============================================================

// AppendCandidate: faithful reproduction of the per-state grow+append block.
//   uVar7  = old count            (param_2[1] low 32)
//   uVar15 = old count + 1        (new count)
//   if (uVar15 == uVar7) return;  // overflow guard (never true for these ranges)
// The triple to append is {id, prio, 0}.
static void AppendCandidate(int64_t* L, int id, int prio) {
    uint8_t* lb = reinterpret_cast<uint8_t*>(L);
    uint32_t uVar7  = *reinterpret_cast<uint32_t*>(lb + 8);    // old count
    uint32_t uVar15 = (uint32_t)(*reinterpret_cast<int*>(lb + 8) + 1);  // new count
    if (uVar15 == uVar7) return;                              // (int)+1==same -> overflow

    int32_t local_38[3] = { id, prio, 0 };

    void* pvVar13 = reinterpret_cast<void*>(L[0]);            // old buffer
    *reinterpret_cast<uint32_t*>(lb + 8) = uVar15;            // store new count

    if (*reinterpret_cast<int*>(lb + 0xc) < (int)uVar15) {    // grow
        *reinterpret_cast<uint32_t*>(lb + 0xc) = uVar15;
        if ((uVar15 == uVar7 + 1) && (uVar7 != 0)) {
            *reinterpret_cast<uint32_t*>(lb + 0xc) = uVar15 * 3;
        }
        int newcap = *reinterpret_cast<int*>(lb + 0xc);
        void* pvVar8 = System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)newcap * 0xc), 4);
        L[0] = reinterpret_cast<int64_t>(pvVar8);
    }

    int iVar20 = 0;
    if (pvVar13 == reinterpret_cast<void*>(L[0])) iVar20 = (int)uVar7;  // in-place: keep tail
    int64_t lVar12 = iVar20;
    if (lVar12 < (int)uVar15) {
        int64_t lVar19 = lVar12 * 0xc;
        do {
            int32_t* puVar18 = reinterpret_cast<int32_t*>(L[0] + lVar19);
            if (puVar18 != nullptr) {
                int32_t* puVar17;
                if (lVar12 < (int)uVar7) {
                    puVar17 = reinterpret_cast<int32_t*>(
                        reinterpret_cast<uint8_t*>(pvVar13) + (int64_t)iVar20 * 0xc);
                } else {
                    puVar17 = local_38;
                }
                puVar18[0] = puVar17[0];
                puVar18[1] = puVar17[1];
                puVar18[2] = puVar17[2];
            }
            lVar12++;
            iVar20++;
            lVar19 += 0xc;
        } while (lVar12 < (int)uVar15);
    }

    if (pvVar13 != reinterpret_cast<void*>(L[0])) {
        System::MemoryAllocator::MemFree(pvVar13);
    }
}

void CitizenAI_BuildCandidateStateList(void* selfV, int64_t* outList, char param3) {
    uint8_t* base = reinterpret_cast<uint8_t*>(selfV);   // CitizenAIComponent
    void* self = base + 0x38;                             // sibling CitizenComponent IBP

    // Census (open C): which exit the function took. Purely observational.

    // --- Special early branch: SICK (+0x68 = Disease*) with a care building (+0x228) ---
    int64_t cz = *reinterpret_cast<int64_t*>(self);
    if (cz != 0 && *reinterpret_cast<int64_t*>(cz + 0x68) != 0) {
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x228) != 0) {
            // Single candidate {5 (Work), 6, 0}, no shuffle/sort (function returns).
            AppendCandidate(outList, 5, 6);
            return;
        }
    }

    // --- hunger (Work-state 6 candidate id 0xe, prio 7) : desc[+0xcc] ---
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x44);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xcc);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0xe, 7);
    }

    // --- thirst (id 0xf, prio 7) : desc[+0xec] ---
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x48);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xec);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0xf, 7);
    }

    // --- hunger-other (id 0, prio 6) : desc[+0xc8] ---
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x44);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xc8);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0, 6);
    }

    // --- thirst-other (id 1, prio 6) : desc[+0xe8] ---
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x48);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xe8);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 1, 6);
    }

    // --- main branch: HEALTHY (+0x68 == 0) vs SICK (+0x68 != 0) ---
    cz = *reinterpret_cast<int64_t*>(self);
    if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {
        // FindHousing (id 2, prio 7) if CanFindHousing
        char canHouse = CitizenComp_CanFindHousing(*reinterpret_cast<int64_t*>(self));
        if (canHouse != '\0') AppendCandidate(outList, 2, 7);

        // Education (id 3, prio 3) if cz[+0x1e4] == 0
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int*>(cz + 0x1e4) == 0) AppendCandidate(outList, 3, 3);

        // GrowUp (id 4, prio 3) if desc[+0x74] is in (cz[+0x50], ..] and (cz[+0x54]>>9 & 1)==0
        cz = *reinterpret_cast<int64_t*>(self);
        {
            float fv = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0x74);
            float work50 = *reinterpret_cast<float*>(cz + 0x50);
            if ((work50 <= fv && fv != work50) &&
                ((*reinterpret_cast<uint32_t*>(cz + 0x54) >> 9 & 1) == 0)) {
                AppendCandidate(outList, 4, 3);
            }
        }
    } else {
        // Sick: state 5 (id 5, prio 6) -- go to the care building
        AppendCandidate(outList, 5, 6);
    }

    // --- param3 short-circuit: skip leisure/age block ---
    if (param3 != '\0') {
        return;
    }

    // --- Leisure (id 0xc) : prio 2 if leisure-thresholds tripped, else prio 0 ---
    {
        cz = *reinterpret_cast<int64_t*>(self);
        bool leisure = false;
        // (cz.desc[+0xb8] in (cz[+0x80],..] and AsSeconds(self.acc@+0xc0) > DAT_1721fc)
        float fv1 = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xb8);
        float* pf1 = reinterpret_cast<float*>(cz + 0x80);
        float c1721fc = kRdata_1401721fc;
        if ((*pf1 <= fv1 && fv1 != *pf1) &&
            (reinterpret_cast<const System::Time*>(base + 0xc0)->AsSeconds() > c1721fc)) {   // B.3.4b bridge
            leisure = true;
        }
        if (!leisure) {
            // OR (cz.desc[+0xb4] in (cz[+0x80],..] and AsSeconds(self.acc) > DAT_172234)
            cz = *reinterpret_cast<int64_t*>(self);
            float fv2 = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xb4);
            float* pf2 = reinterpret_cast<float*>(cz + 0x80);
            float c172234 = kRdata_140172234;
            if ((*pf2 <= fv2 && fv2 != *pf2) &&
                (reinterpret_cast<const System::Time*>(base + 0xc0)->AsSeconds() > c172234)) {   // B.3.4b bridge
                leisure = true;
            }
        }
        if (!leisure) {
            // OR AsSeconds(self.acc) > DAT_172254
            float c172254 = RdataF(kRdata_140172254);
            if (reinterpret_cast<const System::Time*>(base + 0xc0)->AsSeconds() > c172254) leisure = true;   // B.3.4b bridge
        }
        // id 0xc both ways; only the priority differs (2 when tripped, 0 otherwise).
        AppendCandidate(outList, 0xc, leisure ? 2 : 0);
    }

    // --- age / work-need block ---
    cz = *reinterpret_cast<int64_t*>(self);
    float age = *reinterpret_cast<float*>(cz + 0x3c);
    float ageThresh = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0x44);
    if ((age < ageThresh) || (*reinterpret_cast<int64_t*>(cz + 0x208) != 0)) {
        // (age >= thresh) OR elder-flag set  [negation of the not-grown branch]
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x208) == 0) {
            // not elder: housing/work candidates
            cz = *reinterpret_cast<int64_t*>(self);
            if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {
                AppendCandidate(outList, 0xb, 0);   // Wander (id 0xb, prio 0)
            }

            // GatherResources (id 6, prio 4) if !HasSufficientResources
            int hsr = CitizenComp_HasSufficientResources(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr == '\0') AppendCandidate(outList, 6, 4);

            // GatherResources2 (id 7, prio 4) if !HasSufficientResources2
            int hsr2 = CitizenComp_HasSufficientResources2(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr2 == '\0') AppendCandidate(outList, 7, 4);
            // (if hsr2 != 0, original goes to the shuffle/sort tail = LAB_14010f144)
        } else {
            // elder: FindWork (id 10, prio 2) only while healthy (+0x68 == 0)
            if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(self) + 0x68) == 0) {
                AppendCandidate(outList, 10, 2);
            }
        }
    } else {
        // not grown up yet (age < thresh, elder-flag clear): adult-with-workplace block
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x238) != 0) {
            // --- hunger gather (id 6) ---
            int hsr = CitizenComp_HasSufficientResources(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr == '\0') {
                bool reserved6 = false;
                cz = *reinterpret_cast<int64_t*>(self);
                if (*reinterpret_cast<int64_t*>(cz + 0x250) != 0) {
                    int64_t resv = *reinterpret_cast<int64_t*>(
                        *reinterpret_cast<int64_t*>(self) + 0x250);
                    if (*reinterpret_cast<int*>(resv + 0xb0) == 1) reserved6 = true;  // skip to LAB_14010e8e7
                }
                if (!reserved6) {
                    cz = *reinterpret_cast<int64_t*>(self);
                    float fv = *reinterpret_cast<float*>(cz + 0x44);
                    float* pf = reinterpret_cast<float*>(
                        *reinterpret_cast<int64_t*>(cz + 0x288) + 0xcc);
                    int prio = (*pf <= fv && fv != *pf) ? 6 : 4;
                    AppendCandidate(outList, 6, prio);
                }
            }

            // --- LAB_14010e8e7: thirst gather (id 7) ---
            int hsr2 = CitizenComp_HasSufficientResources2(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr2 == '\0') {
                bool reserved7 = false;
                cz = *reinterpret_cast<int64_t*>(self);
                if (*reinterpret_cast<int64_t*>(cz + 0x250) != 0) {
                    int64_t resv = *reinterpret_cast<int64_t*>(
                        *reinterpret_cast<int64_t*>(self) + 0x250);
                    if (*reinterpret_cast<int*>(resv + 0xb0) == 2) reserved7 = true;  // skip to LAB_14010ea39
                }
                if (!reserved7) {
                    cz = *reinterpret_cast<int64_t*>(self);
                    float fv = *reinterpret_cast<float*>(cz + 0x48);
                    float* pf = reinterpret_cast<float*>(
                        *reinterpret_cast<int64_t*>(cz + 0x288) + 0xec);
                    int prio = (*pf <= fv && fv != *pf) ? 6 : 4;
                    AppendCandidate(outList, 7, prio);
                }
            }
        }

        // --- LAB_14010ea39: idle / happiness / long-term-plan tail ---
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {   // else -> LAB_14010f144 (shuffle/sort)
            // SeekHappiness (id 8, prio 5) if cz[+0xac] < 1
            cz = *reinterpret_cast<int64_t*>(self);
            if (*reinterpret_cast<int*>(cz + 0xac) < 1) AppendCandidate(outList, 8, 5);

            // Idle (id 9, prio 2)  -- unconditional
            AppendCandidate(outList, 9, 2);

            // LongTermPlan (id 0xd) only if GetHappiness > 0
            int happy = CitizenComp_GetHappiness(*reinterpret_cast<int64_t*>(self));
            int ltpPrio = 0;
            if (happy >= 1) {
                // Time t(DAT_172234); prio = (t < self.acc@+0xc8) ? 3 : 1
                int64_t timeBuf = 0;
                float c172234 = kRdata_140172234;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeBuf), c172234);
                int64_t acc = *reinterpret_cast<int64_t*>(base + 0xc8);
                ltpPrio = (timeBuf < acc) ? 3 : 1;
                AppendCandidate(outList, 0xd, ltpPrio);
            }

        }
    }

    // ============================================================
    // LAB_14010f144: Fisher-Yates shuffle + SortByPriority
    // ============================================================
    int count = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(outList) + 8);
    if (0 < count) {
        void* rng = *reinterpret_cast<void**>(
            (uintptr_t)GameUpdateLoop::_singleton + 0xd8);
        int64_t off = 0;
        int i = 0;
        do {
            uint32_t r = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(outList) + 8)));   // B.3.4b bridge
            if (r != (uint32_t)i) {
                int64_t buf = outList[0];
                int64_t roff = (int64_t)(int)r * 0xc;
                int32_t u0 = *reinterpret_cast<int32_t*>(buf + off);
                int32_t u1 = *reinterpret_cast<int32_t*>(buf + 4 + off);
                int32_t u2 = *reinterpret_cast<int32_t*>(buf + 8 + off);
                *reinterpret_cast<int32_t*>(buf + off)     = *reinterpret_cast<int32_t*>(buf + roff);
                *reinterpret_cast<int32_t*>(buf + 4 + off) = *reinterpret_cast<int32_t*>(buf + 4 + roff);
                *reinterpret_cast<int32_t*>(buf + 8 + off) = *reinterpret_cast<int32_t*>(buf + 8 + roff);
                buf = outList[0];
                *reinterpret_cast<int32_t*>(buf + roff)     = u0;
                *reinterpret_cast<int32_t*>(buf + 4 + roff) = u1;
                *reinterpret_cast<int32_t*>(buf + 8 + roff) = u2;
            }
            i++;
            off += 0xc;
        } while (i < *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(outList) + 8));
    }
    if (1 < *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(outList) + 8)) {
        CitizenAI_SortByPriority(reinterpret_cast<void*>(outList[0]),
                                 *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(outList) + 8));
    }
}

// ============================================================
// 6. CitizenAIComponent_ValidateAndEnterState (0x14010f1f0)
// ------------------------------------------------------------
// if (*(int64*)(self + state*8 + 0x40) < 1) { switch(state){ dispatch handler } }
// return (low byte). Cooldown gate at self+0x40 indexed by state (8-byte stride).
// Each case calls its handler directly (all fourteen are ported). Case 0xd passes
// *(self+0x38), not self.
// ============================================================

int CitizenAI_ValidateAndEnterState(void* selfV, int state) {
    uint8_t* base = reinterpret_cast<uint8_t*>(selfV);

    // Cooldown gate: *(int64*)(self + state*8 + 0x40) < 1
    if (*reinterpret_cast<int64_t*>(base + (int64_t)state * 8 + 0x40) < 1) {
        const int64_t self = reinterpret_cast<int64_t>(base);
        uint64_t r = 0;
        switch (state) {
            case 0:  case 0xe: r = CitizenAI_State_AdultTransition(self); break;  // 0x14010cd80 EAT/GetFood (Ghidra "AdultTransition"=MISNOMER; +0x44=hunger, drains via msg2->EatFromStorage 0x1401152e0)
            case 1:  case 0xf: r = CitizenAI_State_ElderTransition(self); break;  // 0x14010d2c0 DRINK/GetWater (Ghidra "ElderTransition"=MISNOMER; +0x48=thirst)
            case 2:            r = CitizenAI_State_FindHousing(self); break;      // 0x14010b340
            case 3:            r = CitizenAI_State_Education(self); break;        // 0x14010c360
            case 4:            r = CitizenAI_State_GrowUp(self); break;           // 0x14010c890
            case 5:            r = CitizenAI_State_Work(self); break;             // 0x14010d790
            case 6:            r = CitizenAI_State_GatherResources(self); break;  // 0x14010bc90
            case 7:            r = CitizenAI_State_GatherResources2(self); break; // 0x14010b3e0
            case 8:            r = CitizenAI_State_SeekHappiness(self); break;    // 0x14010c580
            case 9:            r = CitizenAI_State_Idle(self); break;             // 0x14010b2d0
            case 10:           r = CitizenAI_State_FindWork(self); break;         // 0x14010d560
            case 0xb:
                // 0x14010a2b0 Wander: returns a bool in the low byte.
                return CitizenAI_State_Wander(self) ? 1 : 0;
            case 0xc:          r = CitizenAI_State_Leisure(self); break;          // 0x14010a560
            case 0xd: {
                // 0x140115e60 LongTermPlan: CheckPtr(self+0x38); handler(*(self+0x38));
                // the original returns CONCAT71(.., 1) -> low byte always 1.
                void* internal = *reinterpret_cast<void**>(base + 0x38);
                CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(internal));
                return 1;
            }
            default: return 0;
        }

        // all other cases: return the handler's low byte directly.
        return (int)(r & 0xff);
    }

    // default: original returns (uint7)(state>>8)<<8 — low byte is 0.
    return 0;
}

