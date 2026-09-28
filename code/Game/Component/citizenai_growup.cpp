// code/Game/Component/citizenai_growup.cpp
//
// Brick 3b handler wave 4 — faithful 1:1 ports of two CitizenAIComponent state
// handlers plus one private find-helper:
//
//   0x14010c890  CitizenAI_State_GrowUp        (state id 4)
//   0x140115e60  CitizenAI_State_LongTermPlan  (state id 13)  — SHARED: also reached by
//                Gather/Gather2/GrowUp via ResolveAddr(0x140115e60).
//   0x14008f230  Entity_impl_f230 (private GrowUp helper; ported here, not registered
//                separately as a reused symbol — but DOES get a RegisterExeAddr so the
//                handler's `ResolveAddr(0x14008f230)` resolves to it).
//
// ─────────────────────────────────────────────────────────────────────────────
// SELF / CHAIN (verified against the Ghidra decompile + disassembly):
//   param_1 (GrowUp) = CitizenAIComponent. self = param_1+0x38 = sibling CitizenComponent
//     IBP. citizen = *self. citizen+0x238 = workplace/residence entity, citizen+0x278 =
//     CharacterComponent (+0x68 = ModelComponent, whose transform +0x158/+0x15c/+0x160 =
//     world pos), citizen+0x18 = controller-comp IBP.
//   LongTermPlan's param_1 IS the CitizenComponent (the CITIZEN, not the AI component) —
//     GrowUp calls it as LongTermPlan(*(param_1+0x38)). param_1+0x10 = entity, +0x278 =
//     CharacterComponent, +0x30/+0x54/+0xb0 = the action-state flag triplet, +0x278 then
//     +0x48 = the action stack object run by the executor.
//   g_pGameWorldSingleton (DAT_1401c3dd8): +0xf0 = entity-list pair, +0xf8 = map-grid
//     (int* dim at [0]), +0xd8 = Random.
//
//   GrowUp success epilogue (both branches): *self → +0x30=-1, +0x54|=4, +0xb0=0,
//     stamp Time(DAT_1401721fc * desc_walktime) into citizen+0x60, return 1.
//   GrowUp early/adult branch (age > _maxAge 15.0): tail-call LongTermPlan, set
//     param_1+0x2c = 4 (next-state), return 1.
//   GrowUp food-shortage branch (QueryCount < 1): Time(DAT_1401721fc) → citizen+0x60,
//     run dd30 find-best-food-source, scan its storage occupants for the right slot,
//     and if found push a PerformWork(Ctor11) chain (eat) → return 1, else return 0.
//
// ─────────────────────────────────────────────────────────────────────────────
// Registered exe sub-callees (resolved via ResolveAddr; bodies NOT duplicated):
//   0x140006270  Entity_FindComponent_Storage  (reg_work_base WorkBase_FindComponent_6270)
//   0x14012cf10  StorageComponent_QueryCount    (reg_component_people)
//   0x14008dd30  CitizenAI_State_sub_dd30       (reg_citizenai_findtarget)
//   0x140087fb0  Entity_impl_7fb0               (reg_pf_road Pf_EntityAnchorCell_7fb0)
//   0x1400bc790  PerformWork_Action_Ctor11      (reg_citizenai_actionctors)
//   0x14008d590  FUN_14008d590  (find-storage-with-resource)   (reg elsewhere)
//   0x1400c1070  PerformWork_Action_Ctor4       (reg_citizenai_actionctors)
//   0x14012cd80  StorageComponent_impl_cd80     (reg elsewhere)
//   0x1400c4c50  CitizenAI_ActionExecutor_c4c50 (reg_citizenai_executor)
//   --- f230 closure (all registered) ---
//   0x14009f3e0  CharacterComponent_impl_f3e0 == fn_MapGetTerrainPos (mapquery)
//   0x140002ed0  Entity_FindComponent_Position  (reg_work_base WorkBase_FindComponent_2ed0)
//   0x140122e20  Entity_impl_2e20               (reg_pf_road Pf_EntityResolvePos_2e20)
//   0x140008830  FUN_140008830 (cell distance)  (reg_work_base WorkBase_CalcDistance_8830)
//   0x1400a00f0  FUN_1400a00f0 (cell validate)  (reg_work_base WorkBase_ValidateCell_a00f0)
//
// Engine dispatch slots used (all already wired by their owning reimpl batches; resolved
//   here via ResolveSlot for portability):
//   [0x140152ab0] fn_InternalBasePtr_CheckPtr   [0x140152a90] fn_InternalBasePtr_Ctor
//   [0x140152a98] fn_IsDerivedFrom              [0x140152b60] MemAlloc  [0x140152b68] MemFree
//   [0x140152b00] System::Time::Time(Time*,float)  [0x140151320] Math::Vector3::Vector3(Scalar&)
//   (All of the above are also exposed as engine_funcs.h externs; this file uses those
//    externs where they exist and ResolveSlot for the Vector3-from-Scalar ctor.)
//
// SELF-CONTAINED-BRICK type-ids (the DAT_1401c7xxx globals are UNFILLED 0xFFFFFFFF in the
//   standalone build → resolved here via Object::GetTypeIdByName, derived from each global's
//   *_Register/_RegisterType [DATA] writer; scan-result derefs are null-guarded):
//     DWORD_1401c7cb8 -> "RawMaterialDescription"  (RawMaterialDescription_Register@14014f7b5;
//                        it is a *Description*, used by GrowUp's storage-slot scan + cd80 + d590)
//     DWORD_1401c7e78 -> "StorageComponent"        (LongTermPlan's component-type scan filter)
//     DWORD_1401c80e0 -> "WorkPlaceComponent"      (WorkPlaceComponent_RegisterType@14014feb5;
//                        f230's nearest-workplace scan filter)
//
// .rdata float const: DAT_1401721fc (GrowUp food-cooldown / walk-time scale). Read from the
//   exe via ResolveAddr; null-guarded fallback 0.0f.
//


#include "engine_funcs.h"
#include "Component/WorkPlaceComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/StorageComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/RawMaterialDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/CitizenDescription.h"   // B.3.8 CB1a (TypeId)
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_bodies.h"
#include "Game/exe_constants.h"

namespace {

// --- registered exe sub-callees (incl. the f230 closure): called by name ---
// --- Math::Vector3::Vector3(const Scalar&) (import slot 0x140151320): Math::Vector3's own ---

static uint32_t s_tid_RawMaterialDescription = 0;  // DWORD_1401c7cb8
static uint32_t s_tid_StorageComponent       = 0;  // DWORD_1401c7e78
static uint32_t s_tid_WorkPlaceComponent     = 0;  // DWORD_1401c80e0
static uint32_t s_tid_CitizenDescription     = 0;  // DWORD_1401c7190 (GrowUp cooldown desc)

static const float s_walkScale = kRdata_1401721fc;  // DAT_1401721fc (75.0)

// Resolves no door: it only caches the type ids.
static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    if (s_tid_RawMaterialDescription == 0) s_tid_RawMaterialDescription = RawMaterialDescription::TypeId();
    if (s_tid_StorageComponent == 0)       s_tid_StorageComponent       = StorageComponent::TypeId();
    if (s_tid_WorkPlaceComponent == 0)     s_tid_WorkPlaceComponent     = WorkPlaceComponent::TypeId();
    if (s_tid_CitizenDescription == 0)     s_tid_CitizenDescription     = CitizenDescription::TypeId();
    s_init = true;
}

static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
static inline int* MapGrid() { return *reinterpret_cast<int**>(GW() + 0xf8); }
static inline int64_t* ListPair() { return *reinterpret_cast<int64_t**>(GW() + 0xf0); }

// model world pos (+0x158/+0x15c) -> clamped grid cell (row*w + col).
static uint32_t ModelCell(int64_t model, int* mapGrid) {
    int w = *mapGrid;
    int col = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) + static_cast<float>(w >> 1));
    int row = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) + static_cast<float>(w >> 1));
    if (col < 0) col = 0; if (w <= col) col = w - 1;
    if (row < 0) row = 0; if (w <= row) row = w - 1;
    return static_cast<uint32_t>(w * row + col);
}

// Scan a node's object array for the first entry whose GetTypeId() matches typeId,
// returning the derived object (null-guarded). Mirrors the inline
// GetTypeId()/IsDerivedFrom scan repeated throughout these handlers.
//
// THE ARRAY/COUNT PAIR IS NOT THE SAME AT EVERY CALL SITE — it depends on what kind of
// node the caller passes, and retail spells both out:
//
//   +0x30 array / +0x38 count  — a component ENTITY, e.g. inside 0x140115e60:
//       if (0 < *(int *)(lVar4 + 0x38)) {                            // count = +0x38
//         puVar1 = (undefined8*)(*(longlong*)(lVar4 + 0x30) + i*8);  // array = +0x30
//     (the ENT_COMP_ARR/ENT_COMP_CNT pair starttown.cpp names.)
//
//   +0x28 array / +0x30 count  — a storage ROW node, e.g. inside 0x14010c890:
//       if (0 < *(int *)(lVar4 + 0x30)) {                            // count = +0x30
//         ... *(longlong *)(lVar4 + 0x28) + idx*8                    // array = +0x28
//
// HISTORY — this helper was born reading +0x28/+0x30 for everyone, which crashed the
// 0x140115e60 (LongTermPlan) caller. The 2026-08-14 "fix" flipped it to +0x30/+0x38 for
// everyone, which fixed that caller and silently broke the two 0x14010c890 (GrowUp) row
// scans in the other direction. Both edits shared the same false premise: that one pair
// fits every call site. It does not, so the pair is now the caller's to state.
//
// The GrowUp breakage stayed hidden for ten days because it needs a citizen actually
// reaching the eat arm with a stocked storage. Its signature is an ACCESS_VIOLATION
// reading 0x00000005_00000005 — reading 8 bytes at row+0x30 when the row keeps a
// count/capacity int32 PAIR there, so a 5-item/5-capacity row becomes a "pointer".
//
// Retail also CheckPtrs EVERY element slot before dereferencing it — these are
// InternalBasePtrs, and a stale one is exactly what a raw deref turns into a crash.
static void* ScanForType(int64_t node, uint32_t typeId,
                         int arrOff = 0x30, int cntOff = 0x38) {
    if (node == 0) return nullptr;
    int64_t base = *reinterpret_cast<int64_t*>(node + arrOff);
    int count = *reinterpret_cast<int*>(node + cntOff);
    if (base == 0) return nullptr;
    for (int i = 0; i < count; ++i) {
        int64_t* slot = reinterpret_cast<int64_t*>(base + static_cast<int64_t>(i) * 8);
        int64_t obj = *slot;
        if (obj == 0) continue;
        int64_t vt = *reinterpret_cast<int64_t*>(obj);
        if (vt == 0) continue;
        // vtable+0x10 = GetTypeId()
        uint32_t tid = reinterpret_cast<Object::Base*>(obj)->Type();   // B.3.8 bridge (slot 2)
        if (tid == typeId) {
            bool ok = Object::IsDerivedFromNullSafe(reinterpret_cast<void*>(obj), typeId);
            return ok ? reinterpret_cast<void*>(obj) : nullptr;
        }
    }
    return nullptr;
}

} // namespace

// ============================================================
// 0x14008f230 — Entity_impl_f230  (private GrowUp helper)
//   Finds the nearest reachable WorkPlaceComponent-bearing entity to a grid cell.
//   param_1 = entity-list pair (the *(GW+0xf0) object; +0x78 = count, +0x70 = array of
//             InternalBasePtr, 8-byte stride). param_2 = out InternalBasePtr (best entity).
//   param_3 = cell index (clamped row*w+col origin). param_4 = out Vector3 (best world pos).
// ============================================================
extern "C" void* CitizenAI_Entity_impl_f230(int64_t param_1, void* param_2, uint32_t param_3, float* param_4) {
    EnsureInit();
    if (*reinterpret_cast<int*>(param_1 + 0x78) == 0) {
        new (param_2) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        return param_2;
    }
    int* mapGrid = MapGrid();
    int w = *mapGrid;
    // worldPos = ((cell % w) - (w>>1 - DAT_140172114), (cell / w) - ..., 0) — but the only
    // use is feeding f3e0 (terrain sample) which the original immediately overwrites with a
    // fresh per-candidate position; the seed call's result is discarded (local_80 zeroed
    // right after). We faithfully zero the running pos and rely on the per-candidate path.
    float bias = static_cast<float>(w >> 1) - kRdata_140172114;   // DAT_140172114 = 0.5
    float seedXY[3] = {
        static_cast<float>(static_cast<int>(param_3) % w) - bias,
        static_cast<float>(static_cast<int>(param_3) / w) - bias,
        0.0f
    };
    float seedTri[4] = {0, 0, 0, 0};
    Reimpl_MapGetTerrainPos(mapGrid, seedTri, seedXY, '\0', nullptr);

    // running best
    float bestX = 0.0f, bestY = 0.0f, bestZ = 0.0f;
    int   bestCost = 0xffff;          // local_res8
    int64_t bestEntity = 0;           // lVar12

    int n = *reinterpret_cast<int*>(param_1 + 0x78);
    int64_t arr = *reinterpret_cast<int64_t*>(param_1 + 0x70);
    for (int idx = 0; idx < n; ++idx) {
        int64_t* slot = reinterpret_cast<int64_t*>(arr + static_cast<int64_t>(idx) * 8);
        int64_t entity = *reinterpret_cast<int64_t*>(*slot + 0x10);   // *plVar11 + 0x10
        // scan entity's components for a WorkPlaceComponent (node+0x38 count, +0x30 array)
        int64_t compArrCount = *reinterpret_cast<int*>(entity + 0x38);
        int64_t compArr = *reinterpret_cast<int64_t*>(entity + 0x30);
        void* wp = nullptr;
        for (int64_t j = 0; j < compArrCount; ++j) {
            int64_t* cslot = reinterpret_cast<int64_t*>(compArr + j * 8);
            int64_t obj = *cslot;
            uint32_t tid = reinterpret_cast<Object::Base*>(obj)->Type();   // B.3.8 bridge (slot 2)
            if (tid == s_tid_WorkPlaceComponent) {
                bool ok = Object::IsDerivedFromNullSafe(reinterpret_cast<void*>(obj), s_tid_WorkPlaceComponent);
                wp = ok ? reinterpret_cast<void*>(obj) : nullptr;
                break;
            }
        }
        int64_t wpIBP = 0;  // local_98
        new (&wpIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(wp));   // B.3.3 bridge
        // wp+0x60 != 0 → this workplace accepts a worker
        if (wpIBP != 0 && *reinterpret_cast<int*>(wpIBP + 0x60) != 0) {
            int64_t posBuf[2] = {0, 0};
            WorkBase_FindComponent_2ed0(entity, posBuf);
            float wpos[4] = {0, 0, 0, 0};
            if (posBuf[0]) Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posBuf[0]), wpos, 0, 0, nullptr);
            int dim = *mapGrid;
            int ix = static_cast<int>(wpos[0] + static_cast<float>(dim >> 1));
            int iy = static_cast<int>(wpos[1] + static_cast<float>(dim >> 1));
            if (ix < 0) ix = 0; if (dim <= ix) ix = dim - 1;
            if (iy < 0) iy = 0; if (dim <= iy) iy = dim - 1;
            uint32_t cell = static_cast<uint32_t>(dim * iy + ix);
            uint64_t reachable = WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, param_3, cell);
            if (static_cast<char>(reachable) != '\0') {
                int cost = WorkBase_CalcDistance_8830(mapGrid, param_3, cell);
                if (cost < bestCost) {
                    bestX = wpos[0]; bestY = wpos[1]; bestZ = wpos[2];
                    bestEntity = *reinterpret_cast<int64_t*>(*slot + 0x10);
                    bestCost = cost;
                }
            }
        }
    }
    param_4[0] = bestX; param_4[1] = bestY; param_4[2] = bestZ;
    *reinterpret_cast<int64_t*>(param_2) = bestEntity;
    return param_2;
}

// ============================================================
// 0x140115e60 — CitizenAI_State_LongTermPlan  (state id 13)
//   param_1 = CitizenComponent (the citizen). Builds two parallel dynamic arrays of
//   (storage-entity, world-pos) candidates by scanning the storage occupants of the
//   citizen's home region for matching RawMaterialDescription items, then pushes a
//   PerformWork(Ctor4) chain per candidate and runs the executor.
// ============================================================
extern "C" void CitizenAI_State_LongTermPlan(int64_t param_1) {
    EnsureInit();
    int64_t* listPair = ListPair();
    int* mapGrid = MapGrid();

    // citizen storage component (local_res20)
    int64_t storBuf[2] = {0, 0};
    WorkBase_FindComponent_6270(*reinterpret_cast<int64_t*>(param_1 + 0x10), storBuf);
    int64_t citStorNode = storBuf[0];

    // dynamic array A: entity IBP pairs (stride 0x10 bytes = 2*8). base/cap at local_70/local_68.
    float*   arrA = nullptr;  int arrACount = 0; int arrACap = 0;          // local_70 / (int)local_68 / local_68._4_4_
    // dynamic array B: float[3] world positions (stride 0xc). base/cap at local_60/local_58.
    float*   arrB = nullptr;  int arrBCount = 0; int arrBCap = 0;          // local_60 / (int)local_58 / local_58._4_4_

    // citizen home cell (model pos) — uVar15 / local_res18
    int64_t cch = *reinterpret_cast<int64_t*>(param_1 + 0x278);
    int64_t model = *reinterpret_cast<int64_t*>(cch + 0x68);
    uint32_t homeCell = ModelCell(model, mapGrid);

    // iterate the citizen-storage occupant rows (citStorNode+0x30 count, +0x28 row array,
    // each row is 0x10 bytes: [+0] entity-ptr, [+8] occupancy count).
    int rowCount = (citStorNode != 0) ? *reinterpret_cast<int*>(citStorNode + 0x30) : 0;
    bool committed = false;        // matched cd80 → break to commit
    for (int row = 0; row < rowCount; ++row) {
        int64_t rowArr = *reinterpret_cast<int64_t*>(citStorNode + 0x28);
        int64_t rowBase = rowArr + static_cast<int64_t>(row) * 0x10;
        int occ = *reinterpret_cast<int*>(rowBase + 8);
        if (occ == 0) {
            // empty row: nothing to add here; advance.
            continue;
        }
        // search the already-collected array A for a storage whose held resource satisfies
        // this row (StorageComponent_impl_cd80). If satisfied, commit.
        bool satisfied = false;
        for (int k = 0; k < arrACount; ++k) {
            // CheckPtr BEFORE the read — retail is `CheckPtr(pfVar21); lVar4 = *pfVar21;`.
            // Reading first and refreshing after uses the pre-refresh pointer, which is
            // the stale value CheckPtr exists to replace.
            int64_t entry = *reinterpret_cast<int64_t*>(arrA + static_cast<int64_t>(k) * 2);
            // find that entry's StorageComponent
            void* sc = ScanForType(entry, s_tid_StorageComponent);
            int64_t scIBP = 0;
            new (&scIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(sc));   // B.3.3 bridge
            int64_t descComp = *reinterpret_cast<int64_t*>(rowBase);   // row entity (RawMaterialDescription)
            int amount = *reinterpret_cast<int*>(rowBase + 8);
            uint32_t hit = (scIBP) ? Pf_StorageCanAccept_cd80(scIBP, descComp, amount) : 0;
            if (static_cast<char>(hit) != '\0') { satisfied = true; break; }
        }
        if (satisfied) { committed = true; break; }

        // not satisfied by current set: ask d590 for a new storage candidate.
        uint32_t outVec[3] = {0, 0, 0};      // local_80
        int64_t  matchIBP  = 0;              // local_a8
        int64_t  outBest   = 0;              // local_98
        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(listPair), &outBest, homeCell,
                           static_cast<uint64_t>(*reinterpret_cast<int64_t*>(rowBase)),
                           *reinterpret_cast<int*>(rowBase + 8), '\x01',
                           outVec, &matchIBP);
        if (outBest == 0) {
            // d590 found nothing for this row → treat as committed-with-empty (the original
            // jumps to the no-more-rows path that sets up the action push from whatever was
            // collected). We simply continue; arrays retain prior entries.
            continue;
        }

        // grow array A (append outBest as an entity-IBP pair).
        {
            int newCount = arrACount + 1;
            if (newCount != arrACount) {
                if (arrACap < newCount) {
                    int newCap = newCount;
                    if (arrACount != 0) newCap = newCount * 3;
                    arrACap = newCap;
                    float* nb = reinterpret_cast<float*>(fn_MemAlloc
                        ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(newCap) << 3, 8) : nullptr);
                    // copy old
                    // 8-byte stride (one int64 per entry, alloc is newCap << 3) — matches the
                    // search loop's `arrA + k*2` float arithmetic and the original's pfVar14 += 2.
                    for (int c = 0; c < arrACount; ++c) {
                        reinterpret_cast<int64_t*>(nb)[c] = reinterpret_cast<int64_t*>(arrA)[c];
                    }
                    if (arrA && fn_MemFreeStatic) fn_MemFreeStatic(arrA);
                    arrA = nb;
                }
                reinterpret_cast<int64_t*>(arrA)[arrACount] = outBest;
                arrACount = newCount;
            }
        }
        // grow array B (append outVec as float[3]).
        {
            int newCount = arrBCount + 1;
            if (newCount != arrBCount) {
                if (arrBCap < newCount) {
                    int newCap = newCount;
                    if (arrBCount != 0) newCap = newCount * 3;
                    arrBCap = newCap;
                    float* nb = reinterpret_cast<float*>(fn_MemAlloc
                        ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(newCap) * 0xc, 4) : nullptr);
                    for (int c = 0; c < arrBCount; ++c) {
                        nb[c * 3 + 0] = arrB[c * 3 + 0];
                        nb[c * 3 + 1] = arrB[c * 3 + 1];
                        nb[c * 3 + 2] = arrB[c * 3 + 2];
                    }
                    if (arrB && fn_MemFreeStatic) fn_MemFreeStatic(arrB);
                    arrB = nb;
                }
                arrB[arrBCount * 3 + 0] = *reinterpret_cast<float*>(&outVec[0]);
                arrB[arrBCount * 3 + 1] = *reinterpret_cast<float*>(&outVec[1]);
                arrB[arrBCount * 3 + 2] = *reinterpret_cast<float*>(&outVec[2]);
                arrBCount = newCount;
            }
        }
    }

    // commit: if any candidate collected (arrACount != 0 — the original's `if (uVar16 != 0)`),
    // push a PerformWork(Ctor4) per candidate and run the executor.
    if (arrACount != 0) {
        int64_t firstIBP = 0;       // local_90
        new (&firstIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        int64_t selfIBP = 0;        // local_98
        new (&selfIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(param_1));   // B.3.3 bridge

        // Vector3((Scalar&)0) — the first push uses a zero-vector built from local_res10 (=0).
        float zeroScalar = 0.0f;
        float vecBuf[3] = {0, 0, 0};   // Vector3 retbuf (local_80)
        ::new (static_cast<void*>(vecBuf)) Math::Vector3(
            *reinterpret_cast<const Math::Scalar*>(&zeroScalar));   // B.3.1 bridge (splat ctor)
        void* vec = vecBuf;

        CitizenAI_PerformWorkCtor4_c1070(&selfIBP, &firstIBP, reinterpret_cast<float*>(vec), homeCell, 1, 1);

        // remaining candidates (the original loops uVar16 times over arrA/arrB).
        for (int i = 0; i < arrACount; ++i) {
            int64_t entIBP = reinterpret_cast<int64_t*>(arrA)[i];  // local_res8 = *pfVar11
            int64_t citizenIBP = 0;     // local_res10
            new (&citizenIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(param_1));   // B.3.3 bridge
            float* destB = &arrB[i * 3];
            CitizenAI_PerformWorkCtor4_c1070(&citizenIBP, &entIBP, destB, homeCell, 0, 0);
        }

        // commit the action-state flags on the citizen and run the executor.
        *reinterpret_cast<int32_t*>(param_1 + 0x30) = static_cast<int32_t>(0xffffffff);
        *reinterpret_cast<uint32_t*>(param_1 + 0x54) |= 4;
        *reinterpret_cast<int32_t*>(param_1 + 0xb0) = 0;

        int64_t timeStamp = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeStamp), 0.0f);
        int64_t ch = *reinterpret_cast<int64_t*>(param_1 + 0x278);
        if (-1 < *reinterpret_cast<int*>(ch + 0x48 + 0x18)) {
            CitizenAI_ActionExecutor_c4c50(reinterpret_cast<int64_t*>(ch + 0x48), &timeStamp);
        }
        (void)committed;
    }

    // free the dynamic arrays (LAB_140116387 epilogue).
    if (arrB && fn_MemFreeStatic) fn_MemFreeStatic(arrB);
    arrB = nullptr; arrBCount = 0;
    if (arrA && fn_MemFreeStatic) fn_MemFreeStatic(arrA);
    arrA = nullptr; arrACount = 0;
}

// ============================================================
// 0x14010c890 — CitizenAI_State_GrowUp  (state id 4)
// ============================================================
extern "C" uint64_t CitizenAI_State_GrowUp(int64_t param_1) {
    EnsureInit();
    int64_t* listPair = ListPair();
    int* mapGrid = MapGrid();

    float destA[4] = {0, 0, 0, 0};   // local_a0
    float destB[4] = {0, 0, 0, 0};   // local_90

    int64_t citizen = *reinterpret_cast<int64_t*>(param_1 + 0x38);
    // citizen+0x278 = CharacterComponent; +0x68 = ModelComponent; +0x160 = walk-time src (local_70)
    int64_t ch = *reinterpret_cast<int64_t*>(citizen + 0x278);
    int64_t model = *reinterpret_cast<int64_t*>(ch + 0x68);
    // model+0x160 (local_70) is read by the original but only the *(desc+0x78)*scale path is
    // used for the cooldown below; the raw value is not consumed. home cell from model pos:
    uint32_t homeCell = ModelCell(model, mapGrid);

    // citizen home StorageComponent → QueryCount (food on hand?)
    int64_t citizen2 = *reinterpret_cast<int64_t*>(param_1 + 0x38);
    int64_t storBuf[2] = {0, 0};
    WorkBase_FindComponent_6270(*reinterpret_cast<int64_t*>(citizen2 + 0x10), storBuf);
    int count = (storBuf[0]) ? CitizenComp_StorageComponent_QueryCount(storBuf[0]) : 0;

    if (count >= 1) {
        // Citizen's own storage is non-empty (QueryCount >= 1) → defer to LongTermPlan
        // (housing/marriage/work life-planning) and queue next state 4. (decompile `else`).
        CitizenAI_State_LongTermPlan(*reinterpret_cast<int64_t*>(param_1 + 0x38));
        *reinterpret_cast<int32_t*>(param_1 + 0x2c) = 4;
        return (static_cast<uint64_t>(citizen) & 0xffffffffffffff00ULL) | 1;
    }

    // FOOD-SHORTAGE branch (count < 1): stamp the food-cooldown into citizen+0x60,
    // find the best food source via dd30, then scan its storage for an eatable item.
    int64_t cd = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&cd), s_walkScale);
    *reinterpret_cast<int64_t*>(param_1 + 0x60) = cd;

    int64_t resEntity = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x38) + 0x238); // citizen+0x238
    uint32_t bGoal;
    if (resEntity == 0) {
        bGoal = 0xffffffff;
    } else {
        int64_t resIBP = resEntity;
        bGoal = Pf_EntityAnchorCell_7fb0(&resIBP, nullptr);
    }

    int64_t found = 0;        // local_b0
    int64_t outComp = 0;      // local_res18
    CitizenAI_FindBestTarget_dd30(reinterpret_cast<int64_t*>(listPair), &found, homeCell, bGoal, &outComp, destA, 3);
    if (found == 0) {
        return static_cast<uint64_t>(citizen) & 0xffffffffffffff00ULL;   // no source → return 0
    }

    // found a source entity. find its StorageComponent (local_b8).
    int64_t target = found;
    int64_t tStorBuf[2] = {0, 0};
    WorkBase_FindComponent_6270(target, tStorBuf);
    int64_t tStorNode = tStorBuf[0];
    if (tStorNode == 0) {
        return static_cast<uint64_t>(citizen) & 0xffffffffffffff00ULL;
    }

    // scan the source storage's occupant rows for an eatable RawMaterialDescription item
    // that still has stock (count[+8] - reserved[+0xc] > 0 and the RawMaterialDescription's
    // +0x5c bit13 "edible" flag is set), picking one (random if multiple, biased by +0x58).
    int rowCount = *reinterpret_cast<int*>(tStorNode + 0x30);
    int eligible = 0;          // (uint)local_res18
    int preferred = -1;        // uVar21 (the +0x58-flagged candidate index, or random)
    int64_t rowArrBase = *reinterpret_cast<int64_t*>(tStorNode + 0x28);   // local_b8+0x28
    for (int row = 0; row < rowCount; ++row) {
        int64_t rowEnt = *reinterpret_cast<int64_t*>(rowArrBase + static_cast<int64_t>(row) * 0x10);
        // storage ROW node: array +0x28 / count +0x30 (retail 0x14010c890), NOT the
        // +0x30/+0x38 entity pair — see the ScanForType header.
        void* rm = ScanForType(rowEnt, s_tid_RawMaterialDescription, 0x28, 0x30);
        if (!rm) continue;
        uint32_t flag = (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rm) + 0x5c)) >> 0xd;
        if ((flag & 1) == 0) continue;
        int stock = *reinterpret_cast<int*>(rowArrBase + static_cast<int64_t>(row) * 0x10 + 8)
                  - *reinterpret_cast<int*>(rowArrBase + static_cast<int64_t>(row) * 0x10 + 0xc);
        if (stock <= 0) continue;
        if (*reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(rm) + 0x58) != '\0') {
            preferred = eligible;   // remember the flagged candidate's ordinal
        }
        ++eligible;
    }
    if (eligible == 0) {
        return static_cast<uint64_t>(citizen) & 0xffffffffffffff00ULL;
    }

    int chosen = preferred;
    if (chosen < 0) {
        // System::Random::Rand(GW+0xd8, eligible)
        void* rng = *reinterpret_cast<void**>(GW() + 0xd8);
        chosen = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(eligible));   // B.3.4b bridge
    }

    // second pass: locate the chosen eatable row, build the eat (Ctor11) action.
    int ordinal = 0;
    for (int row = 0; row < rowCount; ++row) {
        int64_t rowEnt = *reinterpret_cast<int64_t*>(rowArrBase + static_cast<int64_t>(row) * 0x10);
        // storage ROW node: array +0x28 / count +0x30 (retail 0x14010c890), NOT the
        // +0x30/+0x38 entity pair — see the ScanForType header.
        void* rm = ScanForType(rowEnt, s_tid_RawMaterialDescription, 0x28, 0x30);
        if (!rm) continue;
        uint32_t flag = (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rm) + 0x5c)) >> 0xd;
        if ((flag & 1) == 0) continue;
        int stock = *reinterpret_cast<int*>(rowArrBase + static_cast<int64_t>(row) * 0x10 + 8)
                  - *reinterpret_cast<int*>(rowArrBase + static_cast<int64_t>(row) * 0x10 + 0xc);
        if (stock <= 0) continue;
        if (ordinal == chosen) {
            int64_t resCountObj = *reinterpret_cast<int64_t*>(rowArrBase + static_cast<int64_t>(row) * 0x10);
            if (resCountObj != 0) {
                // f230 → nearest workplace world pos into destB, entity into local_60.
                int64_t bestWp[2] = {0, 0};   // local_60
                CitizenAI_Entity_impl_f230(reinterpret_cast<int64_t>(listPair), bestWp, homeCell, destB);
                if (bestWp[0] != 0) {
                    // Ctor11 holder = *(param_1+0x38) = the CitizenComponent (citizen) wrapped
                    // (disasm: local_res8 := param_1; then local_res8 := *(local_res8+0x38)).
                    int64_t holderCitizen = *reinterpret_cast<int64_t*>(param_1 + 0x38);
                    int64_t srcIBP = found;        // local_res18 = lVar4 = local_b0 (food source entity)
                    int64_t dstIBP = bestWp[0];    // local_res10 (workplace entity from f230)
                    CitizenAI_PerformWorkCtor11_bc790(&holderCitizen, resCountObj, &srcIBP, destA, &dstIBP, destB);

                    // success epilogue: flag triplet on cz = *(param_1+0x38) = citizen.
                    int64_t cz = *reinterpret_cast<int64_t*>(param_1 + 0x38);
                    *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
                    *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
                    *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;

                    // walk-time-scaled cooldown: desc = *(cz+0x18), require CitizenDescription
                    // (DWORD_1401c7190); wt = desc+0x78 * DAT_1401721fc. Stamp → param_1+0x60.
                    int64_t cdesc = *reinterpret_cast<int64_t*>(cz + 0x18);
                    float wt = 0.0f;
                    if (cdesc && Object::IsDerivedFromNullSafe(reinterpret_cast<void*>(cdesc), s_tid_CitizenDescription)) {
                        wt = *reinterpret_cast<float*>(static_cast<uintptr_t>(cdesc) + 0x78);
                    }
                    int64_t cd2 = 0;
                    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&cd2), wt * s_walkScale);
                    *reinterpret_cast<int64_t*>(param_1 + 0x60) = cd2;

                    return (static_cast<uint64_t>(citizen) & 0xffffffffffffff00ULL) | 1;
                }
            }
            break;
        }
        ++ordinal;
    }

    return static_cast<uint64_t>(citizen) & 0xffffffffffffff00ULL;
}

