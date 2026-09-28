// code/Game/Work/pickup.cpp
// Clean C++ reimplementation of Pickup -- all methods.
// Source: Game/Work/Pickup.cpp (from profile build debug strings)
//
// Pickup (0x60 bytes) -- Item pickup work action.
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// Vtable: 0x140161810
//
// Pickup differs from most Work classes:
//   - Has its OWN Execute override (re-targets when citizen changes workplace)
//   - Fields: status (+0x48), citizenIBP (+0x50), candidateList (+0x58)
//   - Status values: 0=unassigned, 1=assigned, 2=completing
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/Pickup.h"
#include "Work/pickup_funcs.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "math/Math.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Game/exe_bodies.h"        // B3R: 0x14009f3e0
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/MemoryAllocator.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()


// ============================================================
// Sub-functions, called by name (B3R): 0x140018020 Reimpl_ExternalPtr_Serialize_StringTable,
// 0x1400f7100 WorkBase_ValidateTask_f7100, 0x1400c5400 WorkBase_TaskListCleanup_c5400.
// ============================================================


// ============================================================
// Pickup::Serialize (0x1400f3aa0)
// Pattern: Work_Serialize + Serializer(v2)
//   + Serializer::Serialize(status at +0x48)
//   + InternalBasePtr::Serialize(citizenIBP at +0x50) + IsDerivedFrom validate
//   + if version > 0: ArraySerialize(candidateList at +0x58)
// ============================================================
void Pickup::Persist(void* stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create serializer (version=2)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 2);   // B.3.4 bridge

    // Serialize uint32 status at +0x48
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&status), nullptr);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x50
    reinterpret_cast<Object::InternalBasePtr*>(&citizenIBP)->Serialize(*reinterpret_cast<IO::Serializer*>(serializerBuf));   // B.3.4 bridge

    // Validate the deserialized ptr against Pickup type
    if (citizenIBP != nullptr) {
        // DAT_1401c7158 = CitizenComponent (ENDGAME B.3.9 D5: by cell, not the legacy
        // variable name "s_typeId_Pickup" -- this is not Pickup's own cell).
        if (!Object::IsDerivedFromNullSafe(citizenIBP, CitizenComponent::TypeId())) {
            citizenIBP = nullptr;
        }
    }

    // Version >= 1: serialize candidate list at +0x58
    int version = reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion();   // B.3.4 bridge
    if (version > 0) {
        Reimpl_ExternalPtr_Serialize_StringTable(&candidateList, serializerBuf);
    }

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// Pickup::Execute (vtable +0x20, 0x1400f2f00)
//
// NAMING: Ghidra labels 0x1400f2f00 "Pickup_Serialize". It is NOT Serialize
// (that is 0x1400f3aa0) -- it is the real Execute, vtable slot 4. Verified by
// reading the vtable bytes at 0x140161810: [4]=0x1400f2f00, [5]=0x1400f38f0.
//
// WHAT IT DOES (plain terms): when this Pickup work is still unclaimed
// (status==0), a laborer runs it to claim the pile and start hauling:
//   1. Look up the target pile's RawMaterialComponent (0x1400223c0) and its
//      RawMaterialDescription (IsDerivedFrom vs DAT_1401c7cb8).
//   2. Compute the citizen's free carry capacity: StorageComponent+0x38 minus
//      StorageComponent::QueryCount (0x14012cf10). If one pile's worth
//      (desc+0x48 unitSize * rawmat+0x28 amount) does not fit -> tell the
//      citizen to re-plan (CitizenAI_State_LongTermPlan) and return 2.
//   3. Derive the citizen's cell from its ModelComponent Transform position
//      (model+0x158/+0x15c, i.e. Transform@+0x148 pos@+0x10/+0x14), clamped.
//   4. Seed a candidate array with this Pickup's own target, then sweep the
//      11x11 cell neighbourhood (dy,dx in -5..+5, skipping the centre) around
//      the *pile*, collecting every other reachable pile of the same kind whose
//      per-type Pickup work is not already claimed (vtable slot 11 == false).
//   5. Sort candidates nearest-first (0x14002c070 = std::sort by +0x08 distance;
//      reimplemented locally as Pickup_SortByDistance -- see the note there).
//   6. For each candidate that still fits in the remaining capacity: convert its
//      access cell to a world position and dispatch PerformWork (0x1400c0ad0) --
//      THIS is what actually makes the citizen walk over and haul the pile --
//      then mark that per-type Pickup work claimed (+0x48=1, +0x50=citizen).
//   7. Stamp the citizen (+0x30 = work type id, +0x54 |= 4, +0xb0 = 0, and
//      +0x88 |= 0x20000 when the work targets the citizen's own workplace).
//
// Returns 0/1/2 as an int (never bool -- trampoline bug class #7): 1 = no work
// available (also the early-out when status != 0), 2 = work dispatched OR the
// citizen is full and must re-plan, 0 = nothing dispatched this tick.
//
// The candidate record is 32 bytes; layout recovered from the stores at
// 0x1400f320c-0x1400f3221 and the reads at 0x1400f3712-0x1400f378a:
//   +0x00 int   cell        (pile cell index)
//   +0x04 int   accessCell  (walkable cell the citizen stands on; 0x14009ccf0)
//   +0x08 int   distance    (0x140008830) <- the sort key used by 0x14002c070
//   +0x0c int   typeId      (resource type id; BST key into gw+0x118)
//   +0x10 IBP   entity      (the pile entity)
//   +0x18 IBP   rawMat      (its RawMaterialComponent)
// ============================================================

struct PickupCandidate {
    int32_t cell;        // +0x00
    int32_t accessCell;  // +0x04
    int32_t distance;    // +0x08
    int32_t typeId;      // +0x0c
    void*   entity;      // +0x10
    void*   rawMat;      // +0x18
};
static_assert(sizeof(PickupCandidate) == 0x20, "PickupCandidate must be 32 bytes");

// 0x14002c070 is MSVC's inlined std::sort over these 32-byte records, ordering
// ascending on the int32 at +0x08 (distance): median-of-3 introsort with an
// explicit range stack, falling back to a selection sort (0x14002c380) once a
// range is <= 8 elements. Both are unstable, so std::sort reproduces the
// ordering semantics exactly; only the tie-break among equal-distance
// candidates may differ, which no caller observes. Reimplemented rather than
// registered -- porting a hand-rolled introsort would add nothing.
static void Pickup_SortByDistance(PickupCandidate* buf, int count) {
    std::sort(buf, buf + count,
              [](const PickupCandidate& a, const PickupCandidate& b) {
                  return a.distance < b.distance;
              });
}

// ---- sub-callees of the Execute port, called by name (B3R) ----
//   0x1400223c0 ActRm_FindComponent_223c0      0x140006270 WorkBase_FindComponent_6270
//   0x14012cf10 StorageComponent_QueryCount    0x140115e60 CitizenAI_State_LongTermPlan
//   0x14008d3c0 PickupGate_TownWants_8d3c0     0x14009ccf0 WorkGrid_FindCell_9ccf0
//   0x1400a00f0 WorkBase_ValidateCell_a00f0    0x1400f6ef0 WorkImpl_BuildStructureStep_6ef0
//   0x14008d590 Pf_PickBestDropTarget_8d590    0x140008830 WorkBase_CalcDistance_8830
//   0x14012cd80 Pf_StorageCanAccept_cd80       0x140003050 WorkBase_FindComponent_3050
//   0x14009f3e0 Reimpl_MapGetTerrainPos        0x1400c0ad0 WorkAct_PerformWorkCtor9_c0ad0
//   slots 0x140152b60 / 0x140152b68: System::MemoryAllocator::MemAlloc / MemFree

// DAT_140172114 = 0x3f000000 = 0.5f.
static float Pickup_CellCenterHalf() {
    return kRdata_140172114;
}

// Inlined Array<PickupCandidate>::Add. Mirrors the exe growth idiom byte-for-byte
// (0x1400f3545-0x1400f3630): capacity becomes newCount, or newCount*3 when the
// array was already non-empty; on realloc the old elements are copied and the old
// buffer freed; without realloc only the new tail element is written.
static void Pickup_CandAdd(PickupCandidate*& buf, int& count, int& capacity,
                           const PickupCandidate& v) {
    const int oldCount = count;
    const int newCount = oldCount + 1;
    PickupCandidate* oldBuf = buf;
    count = newCount;

    if (newCount > capacity) {
        capacity = newCount;
        if (oldCount != 0) capacity = newCount * 3;
        buf = reinterpret_cast<PickupCandidate*>(
            System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(capacity) * 0x20, 8));
    }
    if (!buf) { buf = oldBuf; count = oldCount; return; }  // alloc failed -> no-op

    for (int i = (oldBuf == buf) ? oldCount : 0; i < newCount; ++i) {
        buf[i] = (i < oldCount && oldBuf) ? oldBuf[i] : v;
    }
    if (oldBuf != buf && oldBuf) System::MemoryAllocator::MemFree(oldBuf);
}

// ============================================================
// Pickup::Execute [4] 0x1400f2f00. (It used to be a dispatcher that first
// tried `ResolveAddr(Exe, 0x1400f2f00)` -- this very slot, never registered, so
// always null -- and fell back to this body; CALLER_SURFACE section 9 defect 3.)
// Retail reads RCX/RDX/R8 only.
// ============================================================
int Pickup::Execute(void* citizenIBP, void* time) {
    // 0x1400f2f37: CMP [RCX+0x48],0 -> anything but "unassigned" means no work.
    if (status != 0) return 1;

    // g_pGameWorldSingleton is STORAGE (B3R): its pointer test stays.
    if (!GameUpdateLoop::_singleton) {
        return 1;
    }

    const uint32_t tidRawMatDesc = RawMaterialDescription::TypeId();
    // 0x1401c6be0 (Pickup's own cell) is UNREGISTERED -- Pickup_Tid's raw-.rdata read
    // always failed and fell through to this same Pickup::TypeId() call, so calling
    // it directly is not a behaviour change (ENDGAME B.3.9 D5 note).
    const uint32_t tidPickup     = Pickup::TypeId();

    const uintptr_t S = reinterpret_cast<uintptr_t>(this);
    void* targetIBP = reinterpret_cast<void*>(S + 0x10);  // RSI: Work's target entity IBP

    // --- pile's RawMaterialComponent + its RawMaterialDescription ---------------
    void* rawMatIBP = nullptr;
    ActRm_FindComponent_223c0(*reinterpret_cast<int64_t*>(targetIBP), &rawMatIBP);
    void* descRaw = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(rawMatIBP) + 0x18);
    void* desc = Object::IsDerivedFromNullSafe(descRaw, tidRawMatDesc) ? descRaw : nullptr;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    void* townMgr = *reinterpret_cast<void**>(gw + 0xf0);
    int*  grid    = *reinterpret_cast<int**>(gw + 0xf8);

    // --- citizen's free carry capacity -----------------------------------------
    uintptr_t citizen = *reinterpret_cast<uintptr_t*>(citizenIBP);
    void* storageIBP = nullptr;
    WorkBase_FindComponent_6270(*reinterpret_cast<int64_t*>(citizen + 0x10), &storageIBP);
    int cap = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(storageIBP) + 0x38);
    int freeCap = cap - static_cast<int>(CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(storageIBP)));

    // desc+0x48 = per-unit size, rawMat+0x28 = amount in the pile. (The original
    // dereferences desc unguarded here; kept faithful -- desc is non-null whenever
    // 0x1400223c0 found a RawMaterialComponent, which is the only way to get here.)
    const int needed = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x48) *
                       *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatIBP) + 0x28);
    if (freeCap < needed) {
        CitizenAI_State_LongTermPlan(*reinterpret_cast<int64_t*>(citizenIBP));
        return 2;  // citizen is full -> re-plan
    }

    // --- citizen's cell, from its ModelComponent transform ----------------------
    uintptr_t c = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t sm = *reinterpret_cast<uintptr_t*>(c + 0x278);
    uintptr_t model = *reinterpret_cast<uintptr_t*>(sm + 0x68);

    // Scratch Vector3 shared by 0x14008d590's out-position and the later
    // Vector3 ctor (both write all three floats). The exe pre-seeds only .z from
    // model+0x160 at 0x1400f3070 and never reads it back -- 0x14008d590 writes
    // param_7[0..2] unconditionally at its return. Kept for fidelity; it is dead.
    Math::Vector3 scratch(0.0f, 0.0f, 0.0f);
    scratch.z = *reinterpret_cast<float*>(model + 0x160);

    const int dim = *grid;
    int cx = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) +
                             static_cast<float>(dim >> 1));
    int cz = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) +
                             static_cast<float>(dim >> 1));
    if (cx < 0)    cx = 0;
    if (cx >= dim) cx = dim - 1;
    if (cz < 0)    cz = 0;
    if (cz >= dim) cz = dim - 1;
    const int citizenCell = cz * dim + cx;

    // Work+0x28 = this work's target cell (32-bit; the decompiler widens it to 64).
    const int selfCell = *reinterpret_cast<int*>(S + 0x28);
    const int pileCol  = selfCell % dim;
    const int pileRow  = selfCell / dim;

    // --- gate: does the town still want this resource? --------------------------
    int amount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatIBP) + 0x28);
    if (!static_cast<char>(PickupGate_TownWants_8d3c0(reinterpret_cast<int64_t>(townMgr),
                      *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x28),
                      amount)))   // the door read AL
        return 1;

    const int selfAccess = WorkGrid_FindCell_9ccf0(grid, selfCell, citizenCell, 0xe07);
    if (selfAccess < 0) return 1;
    if (!WorkBase_ValidateCell_a00f0(grid, 0xe1f, static_cast<uint64_t>(static_cast<uint32_t>(selfAccess)),
                          static_cast<uint32_t>(citizenCell)))
        return 1;

    void* assignIBP = *reinterpret_cast<void**>(citizenIBP);
    if (!static_cast<char>(WorkImpl_BuildStructureStep_6ef0(reinterpret_cast<uintptr_t>(this), &assignIBP, static_cast<uint32_t>(selfAccess),
                         static_cast<uint32_t>(citizenCell), time)))   // the door read AL
        return 0;

    // --- is there anywhere to drop it off? --------------------------------------
    void* dropIBP = nullptr;
    void* storeOut = nullptr;
    amount = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawMatIBP) + 0x28);
    Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(townMgr), reinterpret_cast<int64_t*>(&storeOut), static_cast<uint32_t>(selfAccess),
                     *reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x28),
                     amount, 1, reinterpret_cast<uint32_t*>(&scratch.x), &dropIBP);
    if (storeOut == nullptr) return 1;

    // Storage we will haul into -- used to test each candidate for acceptance.
    void* dstStorageIBP = nullptr;
    WorkBase_FindComponent_6270(reinterpret_cast<int64_t>(storeOut), &dstStorageIBP);

    // --- seed the candidate array with this Pickup's own target -----------------
    PickupCandidate cand;
    cand.cell       = selfCell;
    cand.accessCell = selfAccess;
    cand.distance   = WorkBase_CalcDistance_8830(grid, static_cast<uint32_t>(citizenCell),
                                   static_cast<uint32_t>(selfCell));
    cand.typeId     = *reinterpret_cast<int32_t*>(S + 0x20);
    cand.entity     = *reinterpret_cast<void**>(targetIBP);
    cand.rawMat     = rawMatIBP;

    int count = 1;
    int capacity = 1;
    PickupCandidate* buf =
        reinterpret_cast<PickupCandidate*>(System::MemoryAllocator::MemAlloc(0x20, 8));
    if (!buf) return 1;
    buf[0] = cand;
    // 0x1400f32cd: the exe frees the *previous* (null) buffer here -- a literal
    // MemFree(0) no-op. Mirrored for fidelity.
    System::MemoryAllocator::MemFree(nullptr);

    // --- sweep the 11x11 neighbourhood around the pile --------------------------
    const uintptr_t bstRoot = gw + 0x118;  // work/task registry; BST at +0x30

    int row = pileRow - 5;
    for (int dy = -5; dy <= 5; ++dy, ++row) {
        for (int dx = -5; dx <= 5; ++dx) {
            if (dx == 0 && dy == 0) continue;  // skip the pile's own cell
            const int col = pileCol + dx;
            if (col < 0 || col >= dim) continue;
            if (row < 0 || row >= dim) continue;

            const int nCell = row * dim + col;
            uintptr_t cellPtr =
                *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80) +
                static_cast<uintptr_t>(static_cast<int64_t>(nCell)) * 0x30;

            // Occupant: prefer cell+0x00, else cell+0x08, else a null IBP.
            void* occIBP = nullptr;
            if (*reinterpret_cast<uintptr_t*>(cellPtr) != 0) {
                occIBP = *reinterpret_cast<void**>(cellPtr);
            } else if (*reinterpret_cast<uintptr_t*>(cellPtr + 8) != 0) {
                occIBP = *reinterpret_cast<void**>(cellPtr + 8);
            } else {
                new (&occIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            }
            if (occIBP == nullptr) continue;

            void* nRawMatIBP = nullptr;
            ActRm_FindComponent_223c0(reinterpret_cast<int64_t>(occIBP), &nRawMatIBP);
            if (nRawMatIBP == nullptr) continue;

            int64_t nDesc = *reinterpret_cast<int64_t*>(
                reinterpret_cast<uintptr_t>(occIBP) + 0x28);
            if (!static_cast<char>(Pf_StorageCanAccept_cd80(reinterpret_cast<int64_t>(dstStorageIBP), nDesc, 1))) continue;   // the door read AL

            const int nAccess = WorkGrid_FindCell_9ccf0(grid, nCell, citizenCell, 0xe07);
            if (nAccess < 0) continue;
            if (!WorkBase_ValidateCell_a00f0(grid, 0xe1f,
                                  static_cast<uint64_t>(static_cast<uint32_t>(nAccess)),
                                  static_cast<uint32_t>(citizenCell)))
                continue;

            void* compOut = nullptr;
            void* compIBP = WorkBase_FindComponent_3050(reinterpret_cast<int64_t>(occIBP), &compOut);
            const int nTypeId = *reinterpret_cast<int*>(
                *reinterpret_cast<uintptr_t*>(compIBP) + 0x28);

            // Walk the type-id-keyed BST at gw+0x118 -> +0x30 for this resource's
            // Pickup work. Node: key @+0x00, value @+0x08, left @+0x18, right @+0x20.
            void* workIBP = nullptr;
            {
                int* node = *reinterpret_cast<int**>(*reinterpret_cast<uintptr_t*>(bstRoot) + 0x30);
                bool found = false;
                while (node != nullptr) {
                    if (nTypeId < *node) {
                        node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);
                    } else if (nTypeId > *node) {
                        node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);
                    } else {
                        workIBP = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x08);
                        found = true;
                        break;
                    }
                }
                if (!found) new (&workIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            }

            void* pickupRaw = workIBP;
            void* pickupWork = Object::IsDerivedFromNullSafe(pickupRaw, tidPickup) ? pickupRaw : nullptr;
            void* pickupIBP = nullptr;
            new (&pickupIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(pickupWork));   // B.3.3 bridge
            if (pickupIBP == nullptr) continue;

            // vtable slot 11 (+0x58) = "already has a worker" -> skip this pile.
            // The caller reads AL only (hence the char).
            if (static_cast<char>(reinterpret_cast<Work*>(pickupIBP)->GetPriority())) continue;   // B.3.x bridge (slot 11)

            PickupCandidate n;
            n.cell       = nCell;
            n.accessCell = nAccess;
            n.distance   = WorkBase_CalcDistance_8830(grid, static_cast<uint32_t>(citizenCell),
                                        static_cast<uint32_t>(nCell));
            n.typeId     = nTypeId;
            n.entity     = occIBP;
            n.rawMat     = nRawMatIBP;
            Pickup_CandAdd(buf, count, capacity, n);
        }
    }

    // --- nearest-first ----------------------------------------------------------
    if (count > 1) Pickup_SortByDistance(buf, count);

    // --- dispatch ---------------------------------------------------------------
    const float half = Pickup_CellCenterHalf();
    char first = 1;
    int  result = 0;

    for (int i = 0; i < count; ++i) {
        PickupCandidate* p = &buf[i];

        void* dRaw = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(p->rawMat) + 0x18);
        void* d = Object::IsDerivedFromNullSafe(dRaw, tidRawMatDesc) ? dRaw : nullptr;

        const int need = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(d) + 0x48) *
                         *reinterpret_cast<int*>(
                             reinterpret_cast<uintptr_t>(p->rawMat) + 0x28);
        if (need > freeCap) continue;
        freeCap -= need;

        // Access cell -> world position. off = (dim>>1) - 0.5
        const int gdim = *grid;
        const float off = static_cast<float>(gdim >> 1) - half;
        scratch = Math::Vector3(
                   static_cast<float>(p->accessCell % gdim) - off,
                   static_cast<float>(p->accessCell / gdim) - off,
                   0.0f);

        float worldPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        Reimpl_MapGetTerrainPos(grid, worldPos, &scratch.x, 0, nullptr);

        void* entIBP = p->entity;
        void* citIBP = *reinterpret_cast<void**>(citizenIBP);
        WorkAct_PerformWorkCtor9_c0ad0(&citIBP, static_cast<uint32_t>(p->typeId), &entIBP,
                        worldPos, p->cell, first);
        first = 0;


        // Mark this resource type's Pickup work as claimed by our citizen.
        void* wIBP = nullptr;
        {
            int* node = *reinterpret_cast<int**>(*reinterpret_cast<uintptr_t*>(bstRoot) + 0x30);
            bool found = false;
            while (node != nullptr) {
                if (p->typeId < *node) {
                    node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);
                } else if (p->typeId > *node) {
                    node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);
                } else {
                    wIBP = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x08);
                    found = true;
                    break;
                }
            }
            if (!found) new (&wIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        }

        void* wRaw = wIBP;
        void* wPickup = Object::IsDerivedFromNullSafe(wRaw, tidPickup) ? wRaw : nullptr;
        void* claimed = nullptr;
        new (&claimed) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(wPickup));   // B.3.3 bridge
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(claimed) + 0x48) = 1;
        void* citEnt = *reinterpret_cast<void**>(citizenIBP);
        *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(claimed) + 0x50) = citEnt;
    }

    // --- stamp the citizen ------------------------------------------------------
    const int32_t workTypeId = *reinterpret_cast<int32_t*>(S + 0x20);
    uintptr_t citEntity = *reinterpret_cast<uintptr_t*>(citizenIBP);

    // vtable slot 10 (+0x50) GetTarget: member value-return ABI -- RCX=this,
    // RDX=hidden retbuf (the exe reuses its own `self` home slot as the buffer).
    void* targetOut = nullptr;
    {
        void* ret = GetTarget(&targetOut);   // slot 10, virtual in retail
        targetOut = ret ? *reinterpret_cast<void**>(ret) : nullptr;
    }

    *reinterpret_cast<int32_t*>(citEntity + 0x30) = workTypeId;
    *reinterpret_cast<uint32_t*>(citEntity + 0x54) |= 4;
    *reinterpret_cast<uint32_t*>(citEntity + 0xb0) = 0;
    if (targetOut == *reinterpret_cast<void**>(citEntity + 0x1f0) && workTypeId >= 0) {
        *reinterpret_cast<uint32_t*>(citEntity + 0x88) |= 0x20000;
    }

    // first still set => nothing was dispatched => 0; else 2.
    if (first == 0) result = 2;
    System::MemoryAllocator::MemFree(buf);
    return result;
}

// ============================================================
// Pickup::Slot5 [5] 0x1400f38f0 (re-target)
// UNIQUE to Pickup -- handles re-targeting when citizen changes
// workplace during an active pickup.
//
// Algorithm:
// 1. If status == 0, return false (no active work)
// 2. If citizenIBP is null, return false
// 3. Check if citizen's current workplace matches citizenIBP's workplace
// 4. If different: cancel old work, re-run Slot4 for both entities
// 5. Return false (keep executing)
// ============================================================
int Pickup::Slot5(void* entityIBP, void* param3) {
    // Check status != 0 and citizenIBP != null
    if (status == 0 || citizenIBP == nullptr) {
        return 0;
    }

    // Check citizenIBP (+0x50)
    uintptr_t citizenEntity = *(uintptr_t*)(*(uintptr_t*)entityIBP + 0x1f0);

    uintptr_t storedCitizen = (uintptr_t)citizenIBP;

    if (citizenEntity != *(uintptr_t*)(storedCitizen + 0x1f0)) {
        // Citizen changed workplace -- cancel old work and re-target
        uintptr_t oldCitizen = (uintptr_t)citizenIBP;

        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(oldCitizen + 0x278) + 0x48));

        // Re-run Slot4 with the new citizen
        uintptr_t newCitizenRef = *(uintptr_t*)entityIBP;
        uintptr_t local_buf[1] = { newCitizenRef };

        // Call Slot4 via vtable (guard against freed self after CancelAllWork)
        // (port guard, not retail: a zeroed vptr means self was freed during
        // CancelAllWork)
        if (*reinterpret_cast<uintptr_t*>(this) == 0) {
            return 0;
        }
        // retail 0x1400f3991: CALL [RAX+0x20] with RCX/RDX/R8 only (slot 4
        // reads no R9), a virtual dispatch
        int result = Execute(local_buf, param3);

        if (result == 2) {
            return 1;  // Successfully re-targeted
        }

        // Try again with stored citizen
        uintptr_t storedBuf[1] = { storedCitizen };
        Execute(storedBuf, param3);
    }

    return 0;
}


// ============================================================
// Pickup::Slot6 / OnComplete (0x1400f2ec0)
// Resets citizenIBP to null, sets status to 2 (completing).
// Returns false.
// ============================================================
int Pickup::OnComplete(void* /*arg1*/, void* /*arg2*/, int /*arg3*/) {
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    citizenIBP = (void*)nullIBP;
    status = 2;
    return false;
}


// ============================================================
// Pickup::Slot7 / OnCancel (0x1400f2e80)
// Resets citizenIBP to null, sets status to 0 (unassigned).
// ============================================================
int Pickup::OnCancel(void* /*arg1*/, void* /*arg2*/, int /*arg3*/) {
    uintptr_t nullIBP = 0;
    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    citizenIBP = (void*)nullIBP;
    status = 0;
    return 0;
}


// ============================================================
// Pickup::Slot8 / CancelAll (0x1400f39f0)
// If status == 1 (assigned), cancels the citizen's active work.
// ============================================================
int Pickup::Cleanup() {
    if (status == 1) {
        uintptr_t citizenEntity = (uintptr_t)citizenIBP;

        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// Pickup::Slot9 / Validate (0x1400f2e20)
// Validates the assigned citizen entity.
// If invalid, resets citizenIBP to null and status to 0.
// Returns true if valid (or no citizen assigned).
// ============================================================
int Pickup::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t citizenRef = (uintptr_t)citizenIBP;

    if (citizenRef == 0) {
        return true;  // No citizen assigned -- nothing to validate
    }

    // Validate entity via FUN_1400f7100
    uintptr_t entityIBP[1] = { citizenRef };
    bool valid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), nullptr, entityIBP, nullptr);

    if (!valid) {
        // Entity no longer valid -- reset
        uintptr_t nullIBP = 0;
        new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        citizenIBP = (void*)nullIBP;
        status = 0;
        return false;
    }

    return true;
}


// ============================================================
// Pickup::Slot10 / GetTarget (0x1400f3a50)
// Returns the global entity from GameWorld+0xb0+0x30.
// This is a global target, not per-instance.
// ============================================================
void* Pickup::GetTarget(void* outPtr) {
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t entityList = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)outPtr = *(uintptr_t*)(entityList + 0x30);
    return outPtr;
}


// ============================================================
// Pickup::Slot11 / HasWorker (0x14002c000)
// Returns true if citizenIBP is non-null (worker assigned).
// 10 bytes, unhookable.
// ============================================================
int Pickup::GetPriority() {
    return citizenIBP != nullptr ? 1 : 0;
}











// ~Pickup is defaulted: the Pickup then Work stamps and `~Internal` are the compiler's
// chain — all retail's 0x14002c010 does before the free.



// Ctor 0x1400f3c00: `??0Internal`, the Work base fields, the leaf fields zeroed (the
// two stamps are the compiler's).
Pickup::Pickup() : Work(), status(0), citizenIBP(nullptr), candidateList(nullptr) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_Pickup_Vtable` (slot 1 null, slot 2 a lazily cached
// id, slot 12 Work's nop — see the PORT GAP in Work/Pickup.h), Pickup_Ctor_Impl (the
// ctor with both stamps written by hand) and REGISTER_TYPE over Pickup_Factory /
// _CreateInPlace, plus two vtable-pointer statics for the dtor's stamps.

// create 0x14002bfc0: MemAlloc(sizeof, 8), then the ctor.
void* Pickup::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Pickup), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) Pickup());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* Pickup::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) Pickup());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(Pickup, "Work")
