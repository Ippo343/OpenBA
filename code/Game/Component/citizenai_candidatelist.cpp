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
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
// FindComponent_Residence(entity, outIBP) -> InternalBasePtr* (0x140008090)
typedef void* (*fn_FindComponentStorage_t)(int64_t entity, void* outIBP);
// FindComponent_Storage(entity, outIBP) -> InternalBasePtr* (0x140006270)
typedef void* (*fn_FindComponentCarry_t)(int64_t entity, void* outIBP);
// Storage_GetResourceCount(storageComp) -> int (0x14012a9d0)
typedef int (*fn_GetResourceCount_t)(int64_t storageComp);
// Storage_GetResourceCount2(storageComp) -> int (0x14012ab00)
typedef int (*fn_GetResourceCount2_t)(int64_t storageComp);
// StorageComponent_QueryCount(storageComp) -> int (0x14012cf10)
typedef int (*fn_StorageQueryCount_t)(int64_t storageComp);
// CitizenComponent_CanFindHousing(citizenComp) -> char (0x140115be0)
typedef char (*fn_CanFindHousing_t)(int64_t citizenComp);

static fn_FindComponentStorage_t s_fn_FindComponentStorage = nullptr;  // 8090 residence
static fn_FindComponentCarry_t    s_fn_FindComponentCarry   = nullptr;  // 6270 storage
static fn_GetResourceCount_t      s_fn_GetResourceCount     = nullptr;
static fn_GetResourceCount2_t     s_fn_GetResourceCount2    = nullptr;
static fn_StorageQueryCount_t     s_fn_StorageQueryCount    = nullptr;
static fn_CanFindHousing_t        s_fn_CanFindHousing       = nullptr;

// Wander-time .rdata float constants (read by BuildCandidateStateList case 0xc).
static float* s_pf_1721fc = nullptr;   // DAT_1401721fc  (75.0)
static float* s_pf_172234 = nullptr;   // DAT_140172234  (300.0)
static float* s_pf_172254 = nullptr;   // _DAT_140172254 (1800.0)

static bool s_resolved = false;
static void EnsureResolved() {
    if (s_resolved) return;
    s_fn_FindComponentStorage = reinterpret_cast<fn_FindComponentStorage_t>(
        ResolveAddr(Module::Exe, 0x140008090));
    s_fn_FindComponentCarry = reinterpret_cast<fn_FindComponentCarry_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_GetResourceCount = reinterpret_cast<fn_GetResourceCount_t>(
        ResolveAddr(Module::Exe, 0x14012a9d0));
    s_fn_GetResourceCount2 = reinterpret_cast<fn_GetResourceCount2_t>(
        ResolveAddr(Module::Exe, 0x14012ab00));
    s_fn_StorageQueryCount = reinterpret_cast<fn_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_CanFindHousing = reinterpret_cast<fn_CanFindHousing_t>(
        ResolveAddr(Module::Exe, 0x140115be0));
    s_pf_1721fc = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x1401721fc));
    s_pf_172234 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_pf_172254 = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172254));
    s_resolved = true;
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
    EnsureResolved();
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    if (*reinterpret_cast<int64_t*>(p + 0x238) == 0) return 0;

    // local_res8 = first find (feeds GetResourceCount + row scan)
    int64_t local_res8 = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x238);
    if (s_fn_FindComponentStorage)
        s_fn_FindComponentStorage(*reinterpret_cast<int64_t*>(p + 0x238), &local_res8);

    int64_t lVar3desc = *reinterpret_cast<int64_t*>(p + 0x288);  // CitizenDescription

    // local_res10 = second find (feeds the per-slot requirement read)
    int64_t local_res10 = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x238);
    void* self2 = nullptr;
    if (s_fn_FindComponentStorage)
        self2 = s_fn_FindComponentStorage(*reinterpret_cast<int64_t*>(p + 0x238), &local_res10);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self2);
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
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
    int64_t lVar3 = local_res8;  // storage obj (for GetResourceCount)
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
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

    int iVar4 = s_fn_GetResourceCount ? s_fn_GetResourceCount(lVar3) : 0;
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
    EnsureResolved();
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    if (*reinterpret_cast<int64_t*>(p + 0x238) == 0) return 0;

    int64_t local_res8 = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x238);
    if (s_fn_FindComponentStorage)
        s_fn_FindComponentStorage(*reinterpret_cast<int64_t*>(p + 0x238), &local_res8);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
    int64_t lVar2 = local_res8;                                   // storage obj
    int64_t lVar1desc = *reinterpret_cast<int64_t*>(p + 0x288);   // CitizenDescription

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_res8);
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

    int iVar3 = s_fn_GetResourceCount2 ? s_fn_GetResourceCount2(lVar2) : 0;
    float threshold = *reinterpret_cast<float*>(lVar1desc + 0xd4);
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
    EnsureResolved();
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void* local_res8[4] = {nullptr, nullptr, nullptr, nullptr};

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p + 0x10);
    if (s_fn_FindComponentCarry)
        s_fn_FindComponentCarry(*reinterpret_cast<int64_t*>(p + 0x10),
                                reinterpret_cast<void*>(local_res8));
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(local_res8);
    if (!s_fn_StorageQueryCount) return 0;
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
    return s_fn_StorageQueryCount(reinterpret_cast<int64_t>(local_res8[0]));
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
// +0x68 housing-link, +0x80 leisure-accum, +0x208 elder-flag, +0x228, +0x238
// workplace, +0x250 reservation, +0x288 CitizenDescription, +0x1e4 edu, +0xac,
// +0x3c age, +0xb0 reservation-kind.  param_1+0xc0 = leisure Time accumulator.
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
        void* pvVar8 = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)((int64_t)newcap * 0xc), 4);
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
        fn_MemFreeStatic(pvVar13);
    }
}

void CitizenAI_BuildCandidateStateList(void* selfV, int64_t* outList, char param3) {
    EnsureResolved();
    uint8_t* base = reinterpret_cast<uint8_t*>(selfV);   // CitizenAIComponent
    void* self = base + 0x38;                             // sibling CitizenComponent IBP

    // --- Special early branch: working at a workplace (link +0x68 != 0 and +0x228 != 0) ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    int64_t cz = *reinterpret_cast<int64_t*>(self);
    if (cz != 0 && *reinterpret_cast<int64_t*>(cz + 0x68) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x228) != 0) {
            // Single candidate {5 (Work), 6, 0}, no shuffle/sort (function returns).
            AppendCandidate(outList, 5, 6);
            return;
        }
    }

    // --- hunger (Work-state 6 candidate id 0xe, prio 7) : desc[+0xcc] ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x44);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xcc);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0xe, 7);
    }

    // --- thirst (id 0xf, prio 7) : desc[+0xec] ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x48);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xec);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0xf, 7);
    }

    // --- hunger-other (id 0, prio 6) : desc[+0xc8] ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x44);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xc8);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0, 6);
    }

    // --- thirst-other (id 1, prio 6) : desc[+0xe8] ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    cz = *reinterpret_cast<int64_t*>(self);
    {
        float fv = *reinterpret_cast<float*>(cz + 0x48);
        float* pf = reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xe8);
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 1, 6);
    }

    // --- main branch: NOT housed (+0x68 == 0) vs housed (+0x68 != 0) ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    cz = *reinterpret_cast<int64_t*>(self);
    if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {
        // FindHousing (id 2, prio 7) if CanFindHousing
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        char canHouse = s_fn_CanFindHousing
            ? s_fn_CanFindHousing(*reinterpret_cast<int64_t*>(self)) : 0;
        if (canHouse != '\0') AppendCandidate(outList, 2, 7);

        // Education (id 3, prio 3) if cz[+0x1e4] == 0
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int*>(cz + 0x1e4) == 0) AppendCandidate(outList, 3, 3);

        // GrowUp (id 4, prio 3) if desc[+0x74] is in (cz[+0x50], ..] and (cz[+0x54]>>9 & 1)==0
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
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
        // Housed: Work (id 5, prio 6)
        AppendCandidate(outList, 5, 6);
    }

    // --- param3 short-circuit: skip leisure/age block ---
    if (param3 != '\0') return;

    // --- Leisure (id 0xc) : prio 2 if leisure-thresholds tripped, else prio 0 ---
    {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        cz = *reinterpret_cast<int64_t*>(self);
        bool leisure = false;
        // (cz.desc[+0xb8] in (cz[+0x80],..] and AsSeconds(self.acc@+0xc0) > DAT_1721fc)
        float fv1 = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xb8);
        float* pf1 = reinterpret_cast<float*>(cz + 0x80);
        float c1721fc = s_pf_1721fc ? *s_pf_1721fc : 0.0f;
        if ((*pf1 <= fv1 && fv1 != *pf1) &&
            (fn_Time_AsSeconds && fn_Time_AsSeconds(base + 0xc0) > c1721fc)) {
            leisure = true;
        }
        if (!leisure) {
            // OR (cz.desc[+0xb4] in (cz[+0x80],..] and AsSeconds(self.acc) > DAT_172234)
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            cz = *reinterpret_cast<int64_t*>(self);
            float fv2 = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0xb4);
            float* pf2 = reinterpret_cast<float*>(cz + 0x80);
            float c172234 = s_pf_172234 ? *s_pf_172234 : 0.0f;
            if ((*pf2 <= fv2 && fv2 != *pf2) &&
                (fn_Time_AsSeconds && fn_Time_AsSeconds(base + 0xc0) > c172234)) {
                leisure = true;
            }
        }
        if (!leisure) {
            // OR AsSeconds(self.acc) > DAT_172254
            float c172254 = s_pf_172254 ? *s_pf_172254 : 0.0f;
            if (fn_Time_AsSeconds && fn_Time_AsSeconds(base + 0xc0) > c172254) leisure = true;
        }
        // id 0xc both ways; only the priority differs (2 when tripped, 0 otherwise).
        AppendCandidate(outList, 0xc, leisure ? 2 : 0);
    }

    // --- age / work-need block ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
    cz = *reinterpret_cast<int64_t*>(self);
    float age = *reinterpret_cast<float*>(cz + 0x3c);
    float ageThresh = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(cz + 0x288) + 0x44);
    if ((age < ageThresh) || (*reinterpret_cast<int64_t*>(cz + 0x208) != 0)) {
        // (age >= thresh) OR elder-flag set  [negation of the not-grown branch]
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x208) == 0) {
            // not elder: housing/work candidates
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            cz = *reinterpret_cast<int64_t*>(self);
            if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {
                AppendCandidate(outList, 0xb, 0);   // Wander (id 0xb, prio 0)
            }

            // GatherResources (id 6, prio 4) if !HasSufficientResources
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            int hsr = CitizenComp_HasSufficientResources(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr == '\0') AppendCandidate(outList, 6, 4);

            // GatherResources2 (id 7, prio 4) if !HasSufficientResources2
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            int hsr2 = CitizenComp_HasSufficientResources2(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr2 == '\0') AppendCandidate(outList, 7, 4);
            // (if hsr2 != 0, original goes to the shuffle/sort tail = LAB_14010f144)
        } else {
            // elder: FindWork (id 10, prio 2) if not housed/working (+0x68 == 0)
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            if (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(self) + 0x68) == 0) {
                AppendCandidate(outList, 10, 2);
            }
        }
    } else {
        // not grown up yet (age < thresh, elder-flag clear): adult-with-workplace block
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x238) != 0) {
            // --- hunger gather (id 6) ---
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            int hsr = CitizenComp_HasSufficientResources(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr == '\0') {
                bool reserved6 = false;
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
                cz = *reinterpret_cast<int64_t*>(self);
                if (*reinterpret_cast<int64_t*>(cz + 0x250) != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
                    int64_t resv = *reinterpret_cast<int64_t*>(
                        *reinterpret_cast<int64_t*>(self) + 0x250);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&resv);
                    if (*reinterpret_cast<int*>(resv + 0xb0) == 1) reserved6 = true;  // skip to LAB_14010e8e7
                }
                if (!reserved6) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
                    cz = *reinterpret_cast<int64_t*>(self);
                    float fv = *reinterpret_cast<float*>(cz + 0x44);
                    float* pf = reinterpret_cast<float*>(
                        *reinterpret_cast<int64_t*>(cz + 0x288) + 0xcc);
                    int prio = (*pf <= fv && fv != *pf) ? 6 : 4;
                    AppendCandidate(outList, 6, prio);
                }
            }

            // --- LAB_14010e8e7: thirst gather (id 7) ---
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            int hsr2 = CitizenComp_HasSufficientResources2(*reinterpret_cast<int64_t*>(self));
            if ((char)hsr2 == '\0') {
                bool reserved7 = false;
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
                cz = *reinterpret_cast<int64_t*>(self);
                if (*reinterpret_cast<int64_t*>(cz + 0x250) != 0) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
                    int64_t resv = *reinterpret_cast<int64_t*>(
                        *reinterpret_cast<int64_t*>(self) + 0x250);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&resv);
                    if (*reinterpret_cast<int*>(resv + 0xb0) == 2) reserved7 = true;  // skip to LAB_14010ea39
                }
                if (!reserved7) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
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
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
        cz = *reinterpret_cast<int64_t*>(self);
        if (*reinterpret_cast<int64_t*>(cz + 0x68) == 0) {   // else -> LAB_14010f144 (shuffle/sort)
            // SeekHappiness (id 8, prio 5) if cz[+0xac] < 1
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            cz = *reinterpret_cast<int64_t*>(self);
            if (*reinterpret_cast<int*>(cz + 0xac) < 1) AppendCandidate(outList, 8, 5);

            // Idle (id 9, prio 2)  -- unconditional
            AppendCandidate(outList, 9, 2);

            // LongTermPlan (id 0xd) only if GetHappiness > 0
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(self);
            int happy = CitizenComp_GetHappiness(*reinterpret_cast<int64_t*>(self));
            int ltpPrio = 0;
            if (happy >= 1) {
                // Time t(DAT_172234); prio = (t < self.acc@+0xc8) ? 3 : 1
                int64_t timeBuf = 0;
                float c172234 = s_pf_172234 ? *s_pf_172234 : 0.0f;
                if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeBuf, c172234);
                int64_t acc = *reinterpret_cast<int64_t*>(base + 0xc8);
                ltpPrio = (timeBuf < acc) ? 3 : 1;
                // [test-only] BANISHED_DELIVER_BOOST: skip the Time(300.0) carry-timer
                // wait so a pile->barn delivery can be observed inside a short test run.
                // Faithful behaviour (env unset): a carrying citizen Idles until +0xc8
                // exceeds 300.0 time units, only then does LongTermPlan (deliver) beat
                // Idle in the priority contest.
                static int s_delivBoost = -1;
                if (s_delivBoost < 0) {
                    const char* e = getenv("BANISHED_DELIVER_BOOST");
                    s_delivBoost = (e && e[0] && e[0] != '0') ? 1 : 0;
                }
                if (s_delivBoost) ltpPrio = 3;
                AppendCandidate(outList, 0xd, ltpPrio);
            }

            // [diag] BANISHED_DELIVER_TRACE — why a carrying citizen does (or does
            // not) go put its load in a barn. This is the haul-to-storage gate:
            //   * 0x140115de0 is MISNAMED "GetHappiness" — its body is
            //     FindComponent_Storage(self+0x10) -> StorageComponent::QueryCount,
            //     i.e. the CARRY COUNT. So this gate really reads "am I carrying
            //     anything?", and carry==0 (or a null carry storage, see the guard
            //     in CitizenComp_GetHappiness) silently suppresses delivery.
            //   * Even when it passes, state 0xd only RUNS if it out-prioritises the
            //     unconditional Idle (id 9, prio 2): prio is 3 once the carry timer
            //     (+0xc8) exceeds Time(DAT_140172234 = 300.0), else 1 => Idle wins.
            //     CitizenAIComponent::Update zeroes +0xc8 whenever carry count is 0.
            // Capped so a per-tick, per-citizen gate cannot flood the log.
            static int s_delivTrace = -1;
            if (s_delivTrace < 0) {
                const char* e = getenv("BANISHED_DELIVER_TRACE");
                s_delivTrace = (e && e[0] && e[0] != '0') ? 1 : 0;
            }
            if (s_delivTrace) {
                // Cap only the prio-1 "still accumulating" spam; ALWAYS log prio 3,
                // which is the moment delivery actually wins and the citizen sets off
                // for a barn -- that is the event worth watching for.
                static int s_delivLogs = 0;
                if (happy >= 1 && (ltpPrio >= 3 || s_delivLogs < 40)) {
                    if (ltpPrio < 3) ++s_delivLogs;
                    int64_t timeBuf = 0;
                    float c172234 = s_pf_172234 ? *s_pf_172234 : 0.0f;
                    if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&timeBuf, c172234);
                    std::fprintf(stderr,
                        "[Deliver] citizen=%p carry(115de0)=%d acc(+0xc8)=%lld "
                        "time(300.0)=%lld -> LongTermPlan(0xd) prio=%d %s\n",
                        reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(self)),
                        happy,
                        (long long)*reinterpret_cast<int64_t*>(base + 0xc8),
                        (long long)timeBuf, ltpPrio,
                        ltpPrio >= 3 ? "(beats Idle prio 2)" : "(LOSES to Idle prio 2)");
                    std::fflush(stderr);
                }
            }
        }
    }

    // ============================================================
    // LAB_14010f144: Fisher-Yates shuffle + SortByPriority
    // ============================================================
    int count = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(outList) + 8);
    if (0 < count) {
        void* rng = *reinterpret_cast<void**>(
            (uintptr_t)*g_pGameWorldSingleton + 0xd8);
        int64_t off = 0;
        int i = 0;
        do {
            uint32_t r = fn_Random_Rand ? fn_Random_Rand(rng,
                *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(outList) + 8)) : (uint32_t)i;
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

    // Brick-3a verification trace (env BANISHED_CITIZENAI_TRACE, default off):
    // reports the prioritized candidate list ONE time so we can confirm the brain
    // actually selects states (handlers still guarded => no entry yet).
    {
        static int s_ts = -1; static int s_n = 0;
        if (s_ts < 0) { const char* e = getenv("BANISHED_CITIZENAI_TRACE"); s_ts = (e && e[0] && e[0] != '0') ? 1 : 0; }
        int cnt = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(outList) + 8);
        if (s_ts == 1 && s_n < 4 && cnt > 0) {
            int32_t* arr = reinterpret_cast<int32_t*>(outList[0]);
            fprintf(stderr, "[CandList] self=%p count=%d : ", selfV, cnt);
            for (int k = 0; k < cnt && k < 16; k++)
                fprintf(stderr, "{id=%d prio=%d} ", arr[3*k], arr[3*k+1]);
            fprintf(stderr, "\n"); fflush(stderr);
            s_n++;
        }
    }
}

// ============================================================
// 6. CitizenAIComponent_ValidateAndEnterState (0x14010f1f0)
// ------------------------------------------------------------
// if (*(int64*)(self + state*8 + 0x40) < 1) { switch(state){ dispatch handler } }
// return (low byte). Cooldown gate at self+0x40 indexed by state (8-byte stride).
// Each handler resolved via ResolveAddr; if the resolved ptr is NULL (not yet
// ported/registered in standalone), SKIP and return 0 (required — handlers land
// in a later wave). Case 0xd passes *(self+0x38), not self.
// ============================================================

// Per-case handler typedefs (from the decompile).
typedef uint64_t (*fn_StateHandler_t)(int64_t self);        // most cases: (self) -> ulonglong/bool
typedef uint64_t (*fn_StateHandlerPtr_t)(void* internal);   // case 0xd: (*(self+0x38)) -> *

int CitizenAI_ValidateAndEnterState(void* selfV, int state) {
    uint8_t* base = reinterpret_cast<uint8_t*>(selfV);

    // [BuilderTrace] throughput diagnostic (gated BANISHED_BUILD_TRACE): flag a seated
    // builder (owner CitizenComponent +0x1f0 == builderMarker = *(*(gw+0xb0)+0x38)) so we
    // can log which state it actually enters — to see why seated builders don't sustain
    // build labor (build is claimed only from Idle/id 9 via the 7b60 work-tree walk).
    static int s_bldrTrace = -1;
    if (s_bldrTrace < 0) { const char* e = getenv("BANISHED_BUILD_TRACE"); s_bldrTrace = (e && e[0] && e[0] != '0') ? 1 : 0; }
    void* bt_cit = nullptr;
    bool  bt_isBuilder = false;
    if (s_bldrTrace && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gw  = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
        uintptr_t reg = *reinterpret_cast<uintptr_t*>(gw + 0xb0);
        uint64_t marker = reg ? *reinterpret_cast<uint64_t*>(reg + 0x38) : 0;
        bt_cit = *reinterpret_cast<void**>(base + 0x38);
        if (marker && bt_cit &&
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(bt_cit) + 0x1f0) == marker)
            bt_isBuilder = true;
    }
    auto bt_logEnter = [&](int r) -> int {
        if (bt_isBuilder && r) {
            std::fprintf(stderr, "[BuilderTrace] citizen=%p ENTERED state=%d\n", bt_cit, state);
            // One-shot field dump per builder (resolve child-vs-adult / workplace / housing).
            static void* s_dumped[8] = {0,0,0,0,0,0,0,0}; static int s_dn = 0;
            bool seen = false;
            for (int i = 0; i < s_dn && i < 8; ++i) if (s_dumped[i] == bt_cit) { seen = true; break; }
            if (!seen && s_dn < 8) {
                s_dumped[s_dn++] = bt_cit;
                uint8_t* c = reinterpret_cast<uint8_t*>(bt_cit);
                int64_t desc = *reinterpret_cast<int64_t*>(c + 0x288);
                float age = *reinterpret_cast<float*>(c + 0x3c);
                float thr = desc ? *reinterpret_cast<float*>(desc + 0x44) : -1.0f;
                float hunger = *reinterpret_cast<float*>(c + 0x44);
                float thirst = *reinterpret_cast<float*>(c + 0x48);
                float hThrC8 = desc ? *reinterpret_cast<float*>(desc + 0xc8) : -1.0f;  // state-0 hunger-other
                float hThrCC = desc ? *reinterpret_cast<float*>(desc + 0xcc) : -1.0f;  // state-0xe hunger urgent
                float hsr2Thr = desc ? *reinterpret_cast<float*>(desc + 0xd4) : -1.0f; // HSR2 threshold
                std::fprintf(stderr, "[BuilderTrace] FIELDS citizen=%p hunger(+0x44)=%.2f thirst(+0x48)=%.2f "
                             "hThr(desc+0xc8)=%.2f hThrUrg(desc+0xcc)=%.2f hsr2Thr(desc+0xd4)=%.2f age(+0x3c)=%.2f adultThr(desc+0x44)=%.2f\n",
                             bt_cit, hunger, thirst, hThrC8, hThrCC, hsr2Thr, age, thr);
                std::fprintf(stderr, "[BuilderTrace] FIELDS2 citizen=%p "
                             "elder(+0x208)=%p wp(+0x238)=%p house(+0x248)=%p houseGate(+0x68)=%p workTarget(+0x228)=%p prof(+0x1f0)=0x%llx\n",
                             bt_cit,
                             reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(c + 0x208)),
                             reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(c + 0x238)),
                             reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(c + 0x248)),
                             reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(c + 0x68)),
                             reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(c + 0x228)),
                             (unsigned long long)*reinterpret_cast<int64_t*>(c + 0x1f0));
            }
        }
        return r;
    };

    // Cooldown gate: *(int64*)(self + state*8 + 0x40) < 1
    if (*reinterpret_cast<int64_t*>(base + (int64_t)state * 8 + 0x40) < 1) {
        uintptr_t addr = 0;
        switch (state) {
            case 0:  case 0xe: addr = 0x14010cd80; break;  // EAT/GetFood (Ghidra "AdultTransition"=MISNOMER; +0x44=hunger, drains via msg2->EatFromStorage 0x1401152e0)
            case 1:  case 0xf: addr = 0x14010d2c0; break;  // DRINK/GetWater (Ghidra "ElderTransition"=MISNOMER; +0x48=thirst)
            case 2:            addr = 0x14010b340; break;  // FindHousing
            case 3:            addr = 0x14010c360; break;  // Education
            case 4:            addr = 0x14010c890; break;  // GrowUp
            case 5:            addr = 0x14010d790; break;  // Work
            case 6:            addr = 0x14010bc90; break;  // GatherResources
            case 7:            addr = 0x14010b3e0; break;  // GatherResources2
            case 8:            addr = 0x14010c580; break;  // SeekHappiness
            case 9:            addr = 0x14010b2d0; break;  // Idle
            case 10:           addr = 0x14010d560; break;  // FindWork
            case 0xb:          addr = 0x14010a2b0; break;  // Wander
            case 0xc:          addr = 0x14010a560; break;  // Leisure
            case 0xd:          addr = 0x140115e60; break;  // LongTermPlan (special: arg = *(self+0x38))
            default: return 0;
        }

        void* fp = reinterpret_cast<void*>(ResolveAddr(Module::Exe, addr));
        if (!fp) return 0;   // handler not yet ported/registered -> skip

        if (state == 0xd) {
            // case 0xd: CheckPtr(self+0x38); ret = handler(*(self+0x38)); return low byte 1.
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(base + 0x38);
            void* internal = *reinterpret_cast<void**>(base + 0x38);
            (reinterpret_cast<fn_StateHandlerPtr_t>(fp))(internal);
            return bt_logEnter(1);   // original returns CONCAT71(.., 1) -> low byte always 1
        }

        if (state == 0xb) {
            // case 0xb returns a bool in low byte.
            bool r = (bool)(reinterpret_cast<fn_StateHandler_t>(fp))((int64_t)base);
            return bt_logEnter(r ? 1 : 0);
        }

        // all other cases: return the handler's low byte directly.
        uint64_t r = (reinterpret_cast<fn_StateHandler_t>(fp))((int64_t)base);
        return bt_logEnter((int)(r & 0xff));
    }

    // default: original returns (uint7)(state>>8)<<8 — low byte is 0.
    return 0;
}

