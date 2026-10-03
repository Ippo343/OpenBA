// code/Game/Component/citizenai_candidatelist.cpp
//
// Faithful C++ port of the CitizenAIComponent candidate-state / need-check
// subsystem from Application-x64.exe (the SEPARATE, self-contained citizen
// action-state machine — NOT the building-controller SM).
//
// Functions ported (all faithful to the Ghidra decompile + disassembly):
//   0x14002fe10  SortByPriority                 -> CitizenAI_SortByPriority
//   0x1401156e0  CitizenComponent::HasSufficientResources
//   0x1401157f0  CitizenComponent::HasSufficientResources2
//   0x140115de0  CitizenComponent::CarryCount (Ghidra "GetHappiness")
//   0x14010dca0  CitizenAIComponent::BuildCandidateStateList
//   0x14010f1f0  CitizenAIComponent::ValidateAndEnterState
//
// Field names (B.4 W1 COMP-b, CitizenComponent.h): +0x238 is the HOME (B4_RULES
// correction 3), +0x208 the school, +0x1e4 clothingLevel, +0x250 the spouse.
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


#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Component/CitizenAIComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CitizenDescription.h"
#include "Component/ResidenceComponent.h"
#include "Component/StorageComponent.h"
#include "Entity/ComponentEntity.h"
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
namespace {

// B4 P2 CAI-1: the candidate list TransitionState (0x14010db90) hands
// BuildCandidateStateList: a {data, count, capacity} array of 12-byte {state id,
// priority, unused} records. Layout from the tree's port of 0x14010dca0 (the append
// allocates capacity * 0xc with alignment 4 and copies three dwords per record; the
// shuffle swaps the three dwords; TransitionState reads id at +0 with stride 3 ints).
struct StateCandidate {
    int32_t id;         // +0x00 CitizenAI state id
    int32_t priority;   // +0x04 sort key (descending)
    int32_t unused;     // +0x08 always 0
};
static_assert(sizeof(StateCandidate) == 0xc, "StateCandidate is 12 bytes");
static_assert(offsetof(StateCandidate, id) == 0x0, "");
static_assert(offsetof(StateCandidate, priority) == 0x4, "");
static_assert(offsetof(StateCandidate, unused) == 0x8, "");

struct StateCandidateList {
    StateCandidate* data;   // +0x00
    int32_t count;          // +0x08
    int32_t capacity;       // +0x0C
};
static_assert(sizeof(StateCandidateList) == 0x10, "StateCandidateList is {ptr, int, int}");
static_assert(offsetof(StateCandidateList, data) == 0x0, "");
static_assert(offsetof(StateCandidateList, count) == 0x8, "");
static_assert(offsetof(StateCandidateList, capacity) == 0xc, "");

} // namespace

static void CitizenAI_SortByPriority(StateCandidate* a, int count) {
    if (!a || count < 2) return;
    // DESCENDING by priority (highest at index 0). The original (0x14002fe10) is an
    // introsort whose small-array pass selection-sorts the MINIMUM to the END, i.e.
    // it sorts descending. TransitionState (0x14010db90) iterates the result
    // front-to-back and enters the first state whose handler pushes an action, so
    // the HIGHEST-priority satisfiable state must come first. (A prior port had this
    // ascending, which made citizens always pick the lowest-priority state — e.g.
    // Leisure/Wander over Work/needs.) Stable insertion sort, descending.
    for (int i = 1; i < count; i++) {
        int32_t id   = a[i].id;
        int32_t prio = a[i].priority;
        int32_t unu  = a[i].unused;
        int j = i - 1;
        while (j >= 0 && a[j].priority < prio) {
            a[j + 1].id       = a[j].id;
            a[j + 1].priority = a[j].priority;
            a[j + 1].unused   = a[j].unused;
            j--;
        }
        a[j + 1].id       = id;
        a[j + 1].priority = prio;
        a[j + 1].unused   = unu;
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
// 2. CitizenComponent::HasSufficientResources (0x1401156e0)
// ------------------------------------------------------------
// Early-false without a home. Counts the occupied household rows of the home's
// ResidenceComponent, multiplies by this household's row (residentSlots[homeSlot])
// and the CitizenDescription _fc4 factor (truncated to int), then compares against
// the home's edible GetResourceCount.
//   iVar2 = residentSlots[homeSlot].count      (+8, stride 0x10)
//   fVar1 = description->_fc4                  -> CVTTSS2SI (trunc to int)
//   iVar7 = count of rows whose count (+8, stride 0x10) > 0
//   iVar4 = Storage_GetResourceCount(residence)
//   return  iVar7 * iVar2 * (int)fVar1 <= iVar4
// (the finder is called twice on the home; both results are the same component
//  -- ported faithfully.) residentSlots is ResidenceComponent's untyped array: raw.
// ============================================================
int CitizenComponent::HasSufficientResources() {
    if (home == nullptr) return 0;

    // local_res8 = first find (feeds GetResourceCount + row scan)
    ResidenceComponent* local_res8 = home->FindComponent<ResidenceComponent>();

    CitizenDescription* lVar3desc = description;

    // second find (feeds the per-slot requirement read)
    ResidenceComponent* storageObj = home->FindComponent<ResidenceComponent>();

    // [STANDALONE GUARD] the find returns null when the ResidenceComponent type-id
    // is unfilled (the recurring standalone limitation). Guard so the scans can't
    // deref null+0x50/+0x58.
    int iVar2 = 0;
    if (storageObj) {
        iVar2 = storageObj->residentSlots[homeSlot].count;
    }
    float fVar1 = lVar3desc->_fc4;   // CVTTSS2SI = trunc
    int factor = (int)fVar1;

    // Row scan over local_res8.
    if (local_res8 == nullptr) return 0;   // no storage resolved -> "insufficient"
    int rowCount = local_res8->residentSlotCount;
    int iVar7 = 0;
    if (0 < rowCount) {
        const ResidenceComponent::Family* piVar5 = local_res8->residentSlots;
        int64_t lVar6 = rowCount;
        do {
            if (0 < piVar5->count) iVar7++;
            piVar5++;          // stride 0x10
            lVar6--;
        } while (lVar6 != 0);
    }

    int iVar4 = CitizenComp_Storage_GetResourceCount(reinterpret_cast<int64_t>(local_res8));   // LEFT: SIG CitizenComp_Storage_GetResourceCount
    return (iVar7 * iVar2 * factor <= iVar4) ? 1 : 0;
}

// ============================================================
// 3. CitizenComponent::HasSufficientResources2 (0x1401157f0)
// ------------------------------------------------------------
// Like (2) but a single find, uses Storage_GetResourceCount2, a float threshold
// description->_fd4, and a FLOAT comparison:
//   iVar6 = count of rows where slot count (+8, stride 0x10) > 0
//   iVar3 = Storage_GetResourceCount2(storage)
//   return (float)iVar6 * _fd4 <= (float)iVar3       (COMISS / SETNC)
// ============================================================
int CitizenComponent::HasSufficientResources2() {
    if (home == nullptr) return 0;

    ResidenceComponent* local_res8 = home->FindComponent<ResidenceComponent>();
    CitizenDescription* lVar1desc = description;

    if (local_res8 == nullptr) return 0;   // [STANDALONE GUARD] no storage resolved
    int rowCount = local_res8->residentSlotCount;
    int iVar6 = 0;
    if (0 < rowCount) {
        const ResidenceComponent::Family* piVar4 = local_res8->residentSlots;
        int64_t lVar5 = rowCount;
        do {
            if (0 < piVar4->count) iVar6++;
            piVar4++;          // stride 0x10
            lVar5--;
        } while (lVar5 != 0);
    }

    int iVar3 = Storage_GetResourceCount2(reinterpret_cast<int64_t>(local_res8));   // LEFT: SIG Storage_GetResourceCount2
    float threshold = lVar1desc->_fd4;
    // Distribution diagnostic (firewood-evenness): per-house fuel sufficiency
    // inputs — occupied roster rows x CitizenDesc+0xd4 vs fuel units on hand.
    { static int n = 0;
      const bool early = (n < 12);
      if (early) { ++n;
        fprintf(stderr, "[HSR2] residence=%p occupied=%d perOcc=%.1f have=%d -> %s\n",
                (void*)local_res8, iVar6, threshold, iVar3,
                ((float)iVar6 * threshold <= (float)iVar3) ? "sufficient" : "FETCH");
        fflush(stderr); } }
    // COMISS XMM1(=(float)iVar3), XMM0(=(float)iVar6*threshold); SETNC -> iVar3 >= product
    return ((float)iVar6 * threshold <= (float)iVar3) ? 1 : 0;
}

// ============================================================
// 4. CitizenComponent::CarryCount (0x140115de0) — Ghidra "GetHappiness", MISNAMED
// ------------------------------------------------------------
// Finds the citizen's own-entity StorageComponent and tail-returns
// StorageComponent_QueryCount's result (Ghidra shows void; the function tail-calls
// QueryCount, whose EAX is the return). Nothing to do with happiness — it answers
// "how much am I carrying?", and it is the gate on state 0xd (LongTermPlan) = the
// haul-to-storage leg.
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
int CitizenComponent::CarryCount() {
    StorageComponent* carry = _entity->FindComponent<StorageComponent>();
    // Standalone: the citizen's own-entity StorageComponent find returns null when
    // its type-id global is unfilled (the recurring keystone-B pattern). The real
    // game always has a carry-inventory storage here; guard so QueryCount(null) -
    // which derefs +0x30 - can't fault. No storage found => count 0.
    //
    // This is NOT a cosmetic guard: this function is the citizen's CARRY COUNT
    // (body = FindComponent_Storage -> QueryCount), and it is the gate on state 0xd
    // (LongTermPlan) = the haul-to-storage leg. If the find ever returns null here,
    // every citizen reads as "carrying nothing" and NOBODY EVER DELIVERS, silently.
    // So say so loudly (once) rather than let a null-guard mask it -- root-cause the
    // find, do not paper over it.
    if (!carry) {
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
    return CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(carry));   // LEFT: SIG CitizenComp_StorageComponent_QueryCount
}

// ============================================================
// 5. CitizenAIComponent::BuildCandidateStateList (0x14010dca0)
// ------------------------------------------------------------
// outList layout: outList[0]=ptr(8B), +8=count(int), +0xc=capacity(int).
// Appends {id,priority,0} triples (grow rule reproduced exactly), guarded by
// per-state need conditions, then Fisher-Yates shuffles and SortByPriority.
// The CitizenComponent's fields are read by name (CitizenComponent.h).
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
static void AppendCandidate(StateCandidateList* L, int id, int prio) {
    uint32_t uVar7  = (uint32_t)L->count;                     // old count
    uint32_t uVar15 = (uint32_t)(L->count + 1);               // new count
    if (uVar15 == uVar7) return;                              // (int)+1==same -> overflow

    StateCandidate local_38 = { id, prio, 0 };

    StateCandidate* pvVar13 = L->data;                        // old buffer
    L->count = (int32_t)uVar15;                               // store new count

    if (L->capacity < (int)uVar15) {                          // grow
        L->capacity = (int32_t)uVar15;
        if ((uVar15 == uVar7 + 1) && (uVar7 != 0)) {
            L->capacity = (int32_t)(uVar15 * 3);
        }
        int newcap = L->capacity;
        L->data = static_cast<StateCandidate*>(
            System::MemoryAllocator::MemAlloc((uint64_t)((int64_t)newcap * 0xc), 4));
    }

    int iVar20 = 0;
    if (pvVar13 == L->data) iVar20 = (int)uVar7;              // in-place: keep tail
    int64_t lVar12 = iVar20;
    if (lVar12 < (int)uVar15) {
        do {
            StateCandidate* puVar18 = &L->data[lVar12];
            if (puVar18 != nullptr) {
                const StateCandidate* puVar17;
                if (lVar12 < (int)uVar7) {
                    puVar17 = &pvVar13[iVar20];
                } else {
                    puVar17 = &local_38;
                }
                puVar18->id       = puVar17->id;
                puVar18->priority = puVar17->priority;
                puVar18->unused   = puVar17->unused;
            }
            lVar12++;
            iVar20++;
        } while (lVar12 < (int)uVar15);
    }

    if (pvVar13 != L->data) {
        System::MemoryAllocator::MemFree(pvVar13);
    }
}

void CitizenAIComponent::BuildCandidateStateList(int64_t* outListRaw, char param3) {
    StateCandidateList* outList = reinterpret_cast<StateCandidateList*>(outListRaw);   // LEFT: SIG CitizenAIComponent::BuildCandidateStateList (CitizenAIComponent.h, CIT-3 / CAI-3)
    CitizenComponent* cz = citizen;

    // --- Special early branch: SICK (disease) with a care building ---
    if (cz != nullptr && cz->disease != nullptr) {
        cz = citizen;
        if (cz->careBuilding != nullptr) {
            // Single candidate {5 (Work), 6, 0}, no shuffle/sort (function returns).
            AppendCandidate(outList, 5, 6);
            return;
        }
    }

    // --- hunger (state 0xe, prio 7) : desc _fcc ---
    cz = citizen;
    {
        float fv = cz->hunger;
        float* pf = &cz->description->_fcc;
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0xe, 7);
    }

    // --- coldness (id 0xf, prio 7) : desc _fec ---
    cz = citizen;
    {
        float fv = cz->coldness;
        float* pf = &cz->description->_fec;
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0xf, 7);
    }

    // --- hunger-other (id 0, prio 6) : desc _fc8 ---
    cz = citizen;
    {
        float fv = cz->hunger;
        float* pf = &cz->description->_fc8;
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 0, 6);
    }

    // --- coldness-other (id 1, prio 6) : desc _fe8 ---
    cz = citizen;
    {
        float fv = cz->coldness;
        float* pf = &cz->description->_fe8;
        if (*pf <= fv && fv != *pf) AppendCandidate(outList, 1, 6);
    }

    // --- main branch: HEALTHY (no disease) vs SICK ---
    cz = citizen;
    if (cz->disease == nullptr) {
        // FindHousing (id 2, prio 7) if CanFindHousing
        char canHouse = citizen->CanFindHousing();
        if (canHouse != '\0') AppendCandidate(outList, 2, 7);

        // "Education" (id 3, prio 3) if clothingLevel == 0
        cz = citizen;
        if (cz->clothingLevel == 0) AppendCandidate(outList, 3, 3);

        // GrowUp (id 4, prio 3) if desc _f74 is in (health, ..] and (statusFlags>>9 & 1)==0
        cz = citizen;
        {
            float fv = cz->description->_f74;
            float work50 = cz->health;
            if ((work50 <= fv && fv != work50) &&
                ((cz->statusFlags >> 9 & 1) == 0)) {
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
        cz = citizen;
        bool leisure = false;
        // (desc _fb8 in (happiness,..] and AsSeconds(leisureTimer) > DAT_1721fc)
        float fv1 = cz->description->_fb8;
        float* pf1 = &cz->happiness;
        float c1721fc = kRdata_1401721fc;
        if ((*pf1 <= fv1 && fv1 != *pf1) &&
            (reinterpret_cast<const System::Time*>(&leisureTimer)->AsSeconds() > c1721fc)) {   // B.3.4b bridge  // LEFT: CitizenAIComponent.h (CIT-3) leisureTimer is int64_t, retail System::Time
            leisure = true;
        }
        if (!leisure) {
            // OR (desc _fb4 in (happiness,..] and AsSeconds(leisureTimer) > DAT_172234)
            cz = citizen;
            float fv2 = cz->description->_fb4;
            float* pf2 = &cz->happiness;
            float c172234 = kRdata_140172234;
            if ((*pf2 <= fv2 && fv2 != *pf2) &&
                (reinterpret_cast<const System::Time*>(&leisureTimer)->AsSeconds() > c172234)) {   // B.3.4b bridge  // LEFT: CitizenAIComponent.h (CIT-3) leisureTimer is int64_t, retail System::Time
                leisure = true;
            }
        }
        if (!leisure) {
            // OR AsSeconds(leisureTimer) > DAT_172254
            float c172254 = RdataF(kRdata_140172254);
            if (reinterpret_cast<const System::Time*>(&leisureTimer)->AsSeconds() > c172254) leisure = true;   // B.3.4b bridge  // LEFT: CitizenAIComponent.h (CIT-3) leisureTimer is int64_t, retail System::Time
        }
        // id 0xc both ways; only the priority differs (2 when tripped, 0 otherwise).
        AppendCandidate(outList, 0xc, leisure ? 2 : 0);
    }

    // --- age / work-need block ---
    cz = citizen;
    float age = cz->age;
    float ageThresh = cz->description->adultAge;
    if ((age < ageThresh) || (cz->school != nullptr)) {
        // not adult yet, or at school
        cz = citizen;
        if (cz->school == nullptr) {
            // not at school
            cz = citizen;
            if (cz->disease == nullptr) {
                AppendCandidate(outList, 0xb, 0);   // Wander (id 0xb, prio 0)
            }

            // GatherResources (id 6, prio 4) if !HasSufficientResources
            int hsr = citizen->HasSufficientResources();
            if ((char)hsr == '\0') AppendCandidate(outList, 6, 4);

            // GatherResources2 (id 7, prio 4) if !HasSufficientResources2
            int hsr2 = citizen->HasSufficientResources2();
            if ((char)hsr2 == '\0') AppendCandidate(outList, 7, 4);
            // (if hsr2 != 0, original goes to the shuffle/sort tail = LAB_14010f144)
        } else {
            // at school: FindWork (id 10, prio 2) only while healthy
            if (citizen->disease == nullptr) {
                AppendCandidate(outList, 10, 2);
            }
        }
    } else {
        // adult, not at school: the household block
        cz = citizen;
        if (cz->home != nullptr) {
            // --- food gather (id 6) ---
            int hsr = citizen->HasSufficientResources();
            if ((char)hsr == '\0') {
                bool reserved6 = false;
                cz = citizen;
                if (cz->spouse != nullptr) {
                    CitizenComponent* resv = citizen->spouse;
                    if (resv->field_0xB0 == 1) reserved6 = true;  // skip to LAB_14010e8e7
                }
                if (!reserved6) {
                    cz = citizen;
                    float fv = cz->hunger;
                    float* pf = &cz->description->_fcc;
                    int prio = (*pf <= fv && fv != *pf) ? 6 : 4;
                    AppendCandidate(outList, 6, prio);
                }
            }

            // --- LAB_14010e8e7: fuel gather (id 7) ---
            int hsr2 = citizen->HasSufficientResources2();
            if ((char)hsr2 == '\0') {
                bool reserved7 = false;
                cz = citizen;
                if (cz->spouse != nullptr) {
                    CitizenComponent* resv = citizen->spouse;
                    if (resv->field_0xB0 == 2) reserved7 = true;  // skip to LAB_14010ea39
                }
                if (!reserved7) {
                    cz = citizen;
                    float fv = cz->coldness;
                    float* pf = &cz->description->_fec;
                    int prio = (*pf <= fv && fv != *pf) ? 6 : 4;
                    AppendCandidate(outList, 7, prio);
                }
            }
        }

        // --- LAB_14010ea39: idle / happiness / long-term-plan tail ---
        cz = citizen;
        if (cz->disease == nullptr) {   // else -> LAB_14010f144 (shuffle/sort)
            // SeekHappiness (id 8, prio 5) if field_0xAC < 1
            cz = citizen;
            if (cz->field_0xAC < 1) AppendCandidate(outList, 8, 5);

            // Idle (id 9, prio 2)  -- unconditional
            AppendCandidate(outList, 9, 2);

            // LongTermPlan (id 0xd) only if the citizen carries something
            int happy = citizen->CarryCount();
            int ltpPrio = 0;
            if (happy >= 1) {
                // Time t(DAT_172234); prio = (t < dataC8) ? 3 : 1
                System::Time timeBuf = {};
                float c172234 = kRdata_140172234;
                RtSystem_Time_Ctor(&timeBuf, c172234);
                int64_t acc = dataC8;
                ltpPrio = (timeBuf._ticks < acc) ? 3 : 1;
                AppendCandidate(outList, 0xd, ltpPrio);
            }

        }
    }

    // ============================================================
    // LAB_14010f144: Fisher-Yates shuffle + SortByPriority
    // ============================================================
    int count = outList->count;
    if (0 < count) {
        System::Random* rng = GameUpdateLoop::_singleton->m_random;
        int i = 0;
        do {
            uint32_t r = rng->Rand(static_cast<uint32_t>(outList->count));
            if (r != (uint32_t)i) {
                StateCandidate* buf = outList->data;
                int ri = (int)r;
                int32_t u0 = buf[i].id;
                int32_t u1 = buf[i].priority;
                int32_t u2 = buf[i].unused;
                buf[i].id       = buf[ri].id;
                buf[i].priority = buf[ri].priority;
                buf[i].unused   = buf[ri].unused;
                buf = outList->data;
                buf[ri].id       = u0;
                buf[ri].priority = u1;
                buf[ri].unused   = u2;
            }
            i++;
        } while (i < outList->count);
    }
    if (1 < outList->count) {
        CitizenAI_SortByPriority(outList->data, outList->count);
    }
}

// ============================================================
// 6. CitizenAIComponent::ValidateAndEnterState (0x14010f1f0)
// ------------------------------------------------------------
// if (timers[id] < 1) { switch(state){ dispatch handler } }
// return (low byte). Each case calls its handler directly (all fourteen are
// ported). Case 0xd runs the CitizenComponent's LongTermPlan.
// ============================================================

int CitizenAIComponent::ValidateAndEnterState(int id) {
    // Cooldown gate: timers[id] < 1
    if (timers[id] < 1) {
        uint64_t r = 0;
        switch (id) {
            case 0:  case 0xe: r = State_AdultTransition(); break;  // 0x14010cd80 EAT/GetFood (Ghidra "AdultTransition"=MISNOMER; +0x44=hunger, drains via msg2->EatFromStorage 0x1401152e0)
            case 1:  case 0xf: r = State_ElderTransition(); break;  // 0x14010d2c0 WARM (Ghidra "ElderTransition"=MISNOMER; +0x48=coldness)
            case 2:            r = State_FindHousing(); break;      // 0x14010b340
            case 3:            r = State_Education(); break;        // 0x14010c360
            case 4:            r = State_GrowUp(); break;           // 0x14010c890
            case 5:            r = State_Work(); break;             // 0x14010d790
            case 6:            r = State_GatherResources(); break;  // 0x14010bc90
            case 7:            r = State_GatherResources2(); break; // 0x14010b3e0
            case 8:            r = State_SeekHappiness(); break;    // 0x14010c580
            case 9:            r = State_Idle(); break;             // 0x14010b2d0
            case 10:           r = State_FindWork(); break;         // 0x14010d560
            case 0xb:
                // 0x14010a2b0 Wander: returns a bool in the low byte.
                return State_Wander() ? 1 : 0;
            case 0xc:          r = State_Leisure(); break;          // 0x14010a560
            case 0xd: {
                // 0x140115e60 LongTermPlan: CheckPtr(citizen); citizen->LongTermPlan();
                // the original returns CONCAT71(.., 1) -> low byte always 1.
                citizen->LongTermPlan();
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

