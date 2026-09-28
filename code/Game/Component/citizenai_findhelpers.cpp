// code/Game/Component/citizenai_findhelpers.cpp
//
// Brick 3b — the DEEP find-helpers that unblock the State_Work / State_AdultTransition /
// State_ElderTransition handler trio. Faithful 1:1 ports transcribed from the Ghidra
// decompile, offset-by-offset, of:
//
//   0x14012ac30  FUN_14012ac30            -> ResidenceCheck_2ac30      (residence has free
//                                            capacity / is below cur-time threshold; bool)
//   0x14010db20  CitizenAI_helper_db20    -> CitizenAI_helper_db20     (State_Work: workplace
//                                            still valid for this citizen)
//   0x1400170b0  CitizenAI_helper_70b0    -> CitizenAI_helper_70b0     (sum of an int field
//                                            over a stride-0x10 record array; pure leaf)
//   0x14008ec70  Entity_impl_ec70         -> Entity_impl_ec70          (ElderTransition:
//                                            pick nearest valid residence w/ an InteractComponent)
//   0x14008e370  CitizenAI_helper_e370    -> CitizenAI_helper_e370     (AdultTransition:
//                                            find best work/storage/residence target — 4 phases)
//
// ADDRESS RE-VERIFICATION (this session, via get_function_callees on the real handler parents):
//   State_Work (0x14010d790)           -> CitizenAI_helper_db20 @ 14010db20  ✓
//   State_AdultTransition (0x14010cd80)-> CitizenAI_helper_e370 @ 14008e370  ✓ + FUN_14012ac30 @ 14012ac30 ✓
//   State_ElderTransition (0x14010d2c0)-> Entity_impl_ec70 @ 14008ec70       ✓ + FUN_14012ac30 @ 14012ac30 ✓
//   e370's only otherwise-UNregistered exe sub-callee = CitizenAI_helper_70b0 @ 1400170b0 (leaf).
//
// CLOSURE (every exe sub-callee is now registered — verified by grep of standalone/*.cpp):
//   0x1401158d0 CitizenComp_CheckWorkplaceValid (reg_component_gap_A.cpp)   [db20]
//   0x14009f810 Pf_IsDstReachable_f810          (reg_pf_roots_route.cpp)    [ec70, e370]
//   0x140008090 CitizenComp_Entity_FindComponent_Storage (reg_component_people.cpp) [ec70]
//   0x140006270 WorkBase_FindComponent_6270     (reg_work_base.cpp)         [e370]
//   0x140122e20 Pf_EntityResolvePos_2e20        (reg_pf_road.cpp)           [ec70, e370]
//   0x1400a00f0 WorkBase_ValidateCell_a00f0     (reg_work_base.cpp)         [ec70, e370]
//   0x140132010 WorkImpl_Sqrtf_132010           (reg_work_impl2.cpp)        [ec70, e370]
//   0x140087fb0 Pf_EntityAnchorCell_7fb0        (reg_pf_road.cpp)           [e370]
//   0x14008ef40 CitizenComp_Entity_impl_ef40    (reg_component_gap_A.cpp)   [e370]
//   0x14004edb0 Pf_PickReachableCell_edb0       (reg_pf_road.cpp)           [e370]
//   0x140008830 WorkBase_CalcDistance_8830      (reg_work_base.cpp)         [e370]
//   0x14009c480 Pf_ZoneRefresh_9c480            (reg_pf_roots_road.cpp)     [e370]
//   0x14012d5d0 Pf_FindBestAdjacentCell_d5d0    (reg_pf_roots_route.cpp)    [e370]
//   0x1400178c0 CitizenAI_sub_78c0              (citizenai_findtarget.cpp)  [e370]
//   ec70 -> ResidenceCheck_2ac30 and e370 -> CitizenAI_helper_70b0 are in THIS TU (direct call).
//
// KEY ABI NOTE (Ghidra-verified): Entity_impl_ef40 (0x14008ef40) IGNORES its first argument.
//   Its real signature is (ignoredArg1, int64_t* out, void* entityIBP, int category) — it only
//   reads param_3 (the IBP, via Entity_FindComponent_Storage(*ibp)) and param_4 (the category).
//   So in e370 sections A/D where Ghidra shows arg1 = a candidate/cell value (not the mapGrid that
//   sections B/C pass), that's harmless register noise; the port mirrors the decompile but arg1 is
//   don't-care.
//
// SELF-CONTAINED-BRICK type-ids (.rdata BaseTypeInfo first DWORD = runtime CRC type id; UNFILLED
//   0xFFFFFFFF in standalone). Resolved self-contained via Object::GetTypeIdByName so the
//   type-matched component scans actually match; every scan-result deref is additionally
//   NULL-guarded. TypeNames recovered from each global's *_Register/_RegisterType [DATA] writer:
//     DAT_1401c7d28  -> ResidenceDescription_Register     -> "ResidenceDescription"      [2ac30]
//     DAT_1401c7900  -> InteractComponent_RegisterType    -> "InteractComponent"         [ec70, e370]
//     DWORD_1401c7cb8-> RawMaterialDescription_Register   -> "RawMaterialDescription"    [e370]
//     DAT_1401c7ee8  -> StorageLocationComponent_RegisterType   -> "StorageLocationComponent"   [e370]
//     DAT_1401c7f20  -> StorageLocationDescription_Register     -> "StorageLocationDescription" [e370]
//     DWORD_1401c7cf0-> ResidenceComponent_RegisterType   -> "ResidenceComponent"        [e370]
//


#include "engine_funcs.h"
#include "Component/StorageLocationDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/StorageLocationComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/StorageComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/ResidenceDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/ResidenceComponent.h"   // B.3.8 CB1a (TypeId)
#include "Component/RawMaterialDescription.h"   // B.3.8 CB1a (TypeId)
#include "Component/InteractComponent.h"   // B.3.8 CB1a (TypeId)
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/exe_bodies.h"

namespace {

// --- registered exe sub-callees (resolved lazily via ResolveAddr) ---
typedef uint64_t (*fn_checkWork_t)(void* citizen, void* arg);                       // 0x1401158d0
typedef int      (*fn_f810_t)(int* mapGrid, uint32_t fromCell, uint32_t toCell, char addCache); // 0x14009f810
typedef void*    (*fn_findStorage_t)(int64_t entity, void* outIBP);                 // 0x140008090 / 0x140006270
typedef void     (*fn_2e20_t)(int64_t comp, void* outVec3, int, int, void*);        // 0x140122e20
typedef uint64_t (*fn_a00f0_t)(int* mapGrid, uint32_t cat, uint32_t a, uint32_t b); // 0x1400a00f0
typedef float    (*fn_sqrtf_t)(float v);                                            // 0x140132010
typedef uint32_t (*fn_7fb0_t)(void* ibp, float* outVec3orNull);                     // 0x140087fb0
typedef void*    (*fn_ef40_t)(void* ignored, int64_t* out, void* entityIBP, int cat); // 0x14008ef40
typedef int      (*fn_edb0_t)(int64_t entity, uint32_t goalCell, float* outVec3);   // 0x14004edb0
typedef int      (*fn_8830_t)(int* mapGrid, uint32_t fromCell, uint32_t toCell);    // 0x140008830
typedef void     (*fn_9c480_t)(int* mapGrid, uint64_t a, uint64_t b, int c);        // 0x14009c480
typedef int      (*fn_d5d0_t)(int64_t entity, uint32_t goalCell, float* outVec3, uint64_t ctx); // 0x14012d5d0
typedef void*    (*fn_78c0_t)(int64_t entity, void* outIBP);                        // 0x1400178c0

static fn_checkWork_t   s_fn_checkWork    = nullptr;
static fn_f810_t        s_fn_f810         = nullptr;
static fn_findStorage_t s_fn_findStor8090 = nullptr;
static fn_findStorage_t s_fn_findStor6270 = nullptr;
static fn_2e20_t        s_fn_2e20         = nullptr;
static fn_a00f0_t       s_fn_a00f0        = nullptr;
static fn_sqrtf_t       s_fn_sqrtf        = nullptr;
static fn_7fb0_t        s_fn_7fb0         = nullptr;
static fn_ef40_t        s_fn_ef40         = nullptr;
static fn_edb0_t        s_fn_edb0         = nullptr;
static fn_8830_t        s_fn_8830         = nullptr;
static fn_9c480_t       s_fn_9c480        = nullptr;
static fn_d5d0_t        s_fn_d5d0         = nullptr;
static fn_78c0_t        s_fn_78c0         = nullptr;

// --- type ids (self-contained via GetTypeIdByName; see header note) ---
static uint32_t s_tid_ResidenceDescription = 0;  // DAT_1401c7d28
static uint32_t s_tid_InteractComponent    = 0;  // DAT_1401c7900
static uint32_t s_tid_RawMaterialDesc      = 0;  // DWORD_1401c7cb8
static uint32_t s_tid_StorageLocationComp  = 0;  // DAT_1401c7ee8
static uint32_t s_tid_StorageLocationDesc  = 0;  // DAT_1401c7f20
static uint32_t s_tid_ResidenceComponent   = 0;  // DWORD_1401c7cf0

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_fn_checkWork    = reinterpret_cast<fn_checkWork_t>(ResolveAddr(Module::Exe, 0x1401158d0));
    s_fn_f810         = reinterpret_cast<fn_f810_t>(ResolveAddr(Module::Exe, 0x14009f810));
    s_fn_findStor8090 = reinterpret_cast<fn_findStorage_t>(ResolveAddr(Module::Exe, 0x140008090));
    s_fn_findStor6270 = reinterpret_cast<fn_findStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_fn_2e20         = reinterpret_cast<fn_2e20_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_fn_a00f0        = reinterpret_cast<fn_a00f0_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_sqrtf        = reinterpret_cast<fn_sqrtf_t>(ResolveAddr(Module::Exe, 0x140132010));
    s_fn_7fb0         = reinterpret_cast<fn_7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_ef40         = reinterpret_cast<fn_ef40_t>(ResolveAddr(Module::Exe, 0x14008ef40));
    s_fn_edb0         = reinterpret_cast<fn_edb0_t>(ResolveAddr(Module::Exe, 0x14004edb0));
    s_fn_8830         = reinterpret_cast<fn_8830_t>(ResolveAddr(Module::Exe, 0x140008830));
    s_fn_9c480        = reinterpret_cast<fn_9c480_t>(ResolveAddr(Module::Exe, 0x14009c480));
    s_fn_d5d0         = reinterpret_cast<fn_d5d0_t>(ResolveAddr(Module::Exe, 0x14012d5d0));
    s_fn_78c0         = reinterpret_cast<fn_78c0_t>(ResolveAddr(Module::Exe, 0x1400178c0));

    if (s_tid_ResidenceDescription == 0) s_tid_ResidenceDescription = ResidenceDescription::TypeId();
    if (s_tid_InteractComponent == 0)    s_tid_InteractComponent    = InteractComponent::TypeId();
    if (s_tid_RawMaterialDesc == 0)      s_tid_RawMaterialDesc      = RawMaterialDescription::TypeId();
    if (s_tid_StorageLocationComp == 0)  s_tid_StorageLocationComp  = StorageLocationComponent::TypeId();
    if (s_tid_StorageLocationDesc == 0)  s_tid_StorageLocationDesc  = StorageLocationDescription::TypeId();
    if (s_tid_ResidenceComponent == 0)   s_tid_ResidenceComponent   = ResidenceComponent::TypeId();
    s_init = true;
}

// vtable[+0x10] == Object::Base::GetTypeId(this) — read the runtime type id of a component.
static inline uint32_t CompTypeId(void* comp) {
    return reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge (slot 2)
}

// gameWorld base (g_pGameWorldSingleton is void** → *g_pGameWorldSingleton = the object).
static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
static inline int* MapGrid() { return *reinterpret_cast<int**>(GW() + 0xf8); }

// Scan an entity's comp-list1 (entity+0x30 array, entity+0x38 count) for a component whose
// GetTypeId()==tid, IsDerivedFrom-confirm, return it (or null). CheckPtr each slot.
static void* ScanCompList1(int64_t entity, uint32_t tid) {
    int n = *reinterpret_cast<int*>(entity + 0x38);
    int64_t arr = *reinterpret_cast<int64_t*>(entity + 0x30);
    for (int i = 0; i < n; ++i) {
        int64_t* slot = reinterpret_cast<int64_t*>(arr + static_cast<int64_t>(i) * 8);
        void* comp = reinterpret_cast<void*>(*slot);
        if (comp && CompTypeId(comp) == tid) {
            int64_t* slot2 = reinterpret_cast<int64_t*>(arr + static_cast<int64_t>(i) * 8);
            void* obj = reinterpret_cast<void*>(*slot2);
            if (obj && fn_IsDerivedFrom && fn_IsDerivedFrom(obj, tid)) return obj;
            return nullptr;
        }
    }
    return nullptr;
}

// Scan a comp-list2 (base+0x28 array, base+0x30 count) — the description/sub-component list of
// a slot. Entries are raw component pointers (no CheckPtr in the original). Return matching or null.
static void* ScanCompList2(int64_t base, uint32_t tid) {
    int n = *reinterpret_cast<int*>(base + 0x30);
    int64_t arr = *reinterpret_cast<int64_t*>(base + 0x28);
    for (int i = 0; i < n; ++i) {
        int64_t comp = *reinterpret_cast<int64_t*>(arr + static_cast<int64_t>(i) * 8);
        if (comp && CompTypeId(reinterpret_cast<void*>(comp)) == tid) {
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(reinterpret_cast<void*>(comp), tid))
                return reinterpret_cast<void*>(comp);
            return nullptr;
        }
    }
    return nullptr;
}

} // namespace

// ============================================================
// FUN_14012ac30 (0x14012ac30) — ResidenceCheck.
//   param_1 = a StorageComponent-ish object. obj = *(param_1+0x18) (its description holder).
//   Returns true if the residence's threshold field (desc+0x28) is <= the gameworld's current
//   value (gw+0x130 -> +0x18), OR there is still pending capacity (param_1+0x48 > 0).
// ============================================================
bool ResidenceCheck_2ac30(int64_t param_1) {
    EnsureInit();
    void* obj = *reinterpret_cast<void**>(param_1 + 0x18);
    float cur = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(GW() + 0x130) + 0x18);
    void* desc = (obj && fn_IsDerivedFrom && fn_IsDerivedFrom(obj, s_tid_ResidenceDescription)) ? obj : nullptr;
    float thresh = desc ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x28) : 0.0f;
    if (thresh <= cur) return true;
    return 0 < *reinterpret_cast<int64_t*>(param_1 + 0x48);
}

// ============================================================
// CitizenAI_helper_db20 (0x14010db20) — State_Work workplace-still-valid check.
//   param_1 = CitizenAIComponent. self = param_1+0x38 (sibling CitizenComponent IBP).
//   *self = CitizenComponent, +0x238 = workplace entity. If no workplace -> return *self with the
//   low byte cleared (i.e. "false"). Else CitizenComponent::CheckWorkplaceValid(citizen, &workplace).
//   Faithful: returns ulonglong (caller reads it as bool / low byte).
// ============================================================
uint64_t CitizenAI_helper_db20(int64_t param_1) {
    EnsureInit();
    uint64_t* self = reinterpret_cast<uint64_t*>(param_1 + 0x38);
    if (*reinterpret_cast<int64_t*>(*self + 0x238) == 0) {
        return *self & 0xffffffffffffff00ULL;
    }
    uint64_t local_res8[4] = {0, 0, 0, 0};
    local_res8[0] = *reinterpret_cast<uint64_t*>(*self + 0x238);
    if (!s_fn_checkWork) return *self & 0xffffffffffffff00ULL;
    return s_fn_checkWork(reinterpret_cast<void*>(*self), local_res8);
}

// ============================================================
// CitizenAI_helper_70b0 (0x1400170b0) — pure leaf: sum the int field at +8 of each stride-0x10
//   record in the array at (param_1+0x50), over (param_1+0x58) records. Returns the 32-bit sum.
// ============================================================
uint64_t CitizenAI_helper_70b0(int64_t param_1) {
    uint32_t count = *reinterpret_cast<uint32_t*>(param_1 + 0x58);
    int64_t arr = *reinterpret_cast<int64_t*>(param_1 + 0x50);
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        sum += *reinterpret_cast<int*>(arr + static_cast<int64_t>(i) * 0x10 + 8);
    }
    return static_cast<uint64_t>(sum);
}

// Path cost from cell -> goal: f810 (real A* cost), else euclidean-on-grid fallback (sqrt+width).
static int CellCost(int* mapGrid, uint32_t cell, uint32_t goal) {
    int c = s_fn_f810 ? s_fn_f810(mapGrid, cell, goal, '\x01') : -1;
    if (c < 0) {
        int w = *mapGrid;
        int dRow = static_cast<int>(cell) / w - static_cast<int>(goal) / w;
        int dCol = static_cast<int>(cell) % w - static_cast<int>(goal) % w;
        float r = s_fn_sqrtf ? s_fn_sqrtf(static_cast<float>(dRow * dRow + dCol * dCol)) : 0.0f;
        c = static_cast<int>(r) + w;
    }
    return c;
}

// World pos (vec3) -> grid cell index, clamped to [0, w-1] in each axis. (cell = row*w + col)
static uint32_t WorldToCell(int* mapGrid, const float* vec) {
    int w = *mapGrid;
    int col = static_cast<int>(vec[0] + static_cast<float>(w >> 1));
    int row = static_cast<int>(vec[1] + static_cast<float>(w >> 1));
    if (col < 0) col = 0;
    if (w <= col) col = w - 1;
    if (row < 0) row = 0;
    if (w <= row) row = w - 1;
    return static_cast<uint32_t>(w * row + col);
}

// ============================================================
// Entity_impl_ec70 (0x14008ec70) — State_ElderTransition find-target.
//   Scan the entity list at (param_1+0x20 array, param_1+0x28 count); for each candidate find its
//   InteractComponent, resolve its world position -> cell, rank by path cost to goal (param_3),
//   keep the cheapest whose ValidateCell passes AND whose Storage passes ResidenceCheck_2ac30.
//   Out: *param_2 = best entity, param_4 = best target vec3. Returns param_2.
// ============================================================
int64_t* Entity_impl_ec70(int64_t param_1, int64_t* param_2, uint32_t param_3, float* param_4) {
    EnsureInit();
    int* mapGrid = MapGrid();
    int best = 0x7fffffff;
    *param_2 = 0;
    if (*reinterpret_cast<int*>(param_1 + 0x28) < 1) return param_2;

    int idx = 0;
    int64_t byteOff = 0;
    do {
        int64_t cand = *reinterpret_cast<int64_t*>(byteOff + *reinterpret_cast<int64_t*>(param_1 + 0x20));

        void* interact = ScanCompList1(cand, s_tid_InteractComponent);
        int64_t interactIBP = 0;
        if (fn_InternalBasePtr_Ctor) new (&interactIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(interact));   // B.3.3 bridge

        float vec[4] = {0, 0, 0, 0};
        if (s_fn_2e20) s_fn_2e20(interactIBP, vec, 0, 0, nullptr);
        uint32_t cell = WorldToCell(mapGrid, vec);
        int cost = CellCost(mapGrid, cell, param_3);

        if (cost < best) {
            uint64_t valid = s_fn_a00f0 ? s_fn_a00f0(mapGrid, 0xe1f, param_3, cell) : 0;
            if (static_cast<char>(valid) != '\0') {
                int64_t* self_00 = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x20) + byteOff);
                int64_t storIBP[2] = {0, 0};
                void* storPtr = s_fn_findStor8090 ? s_fn_findStor8090(*self_00, storIBP) : nullptr;
                int64_t storComp = storPtr ? *reinterpret_cast<int64_t*>(storPtr) : 0;
                bool ok = storComp ? ResidenceCheck_2ac30(storComp) : false;
                if (ok && (-1 < cost)) {
                    *param_2 = cand;
                    param_4[0] = vec[0];
                    param_4[1] = vec[1];
                    param_4[2] = vec[2];
                    best = cost;
                }
            }
        }
        ++idx;
        byteOff += 8;
    } while (idx < *reinterpret_cast<int*>(param_1 + 0x28));
    return param_2;
}

// ============================================================
// CitizenAI_helper_e370 (0x14008e370) — State_AdultTransition find-best-target. 4 phases:
//   A) a preferred candidate (*param_7): if its anchor cell validates, take it (cost = CalcDistance).
//   B) a MARKET candidate list (param_1[2] array / param_1[3] count): rank by path cost, then by
//      MarketLocationComponent reachability (sub_78c0 + edb0).
//   C) a RAW-MATERIAL/STORAGE candidate list (param_1[0]/[1]): match RawMaterialDescription on the
//      ef40-picked production slot, StorageLocationComponent on the entity, flag-mask compatibility
//      (RadiusDescription+0x5c & storage mask), then cost via FindBestAdjacentCell (d5d0).
//   D) a RESIDENCE candidate list (param_1[4]/[5]): skip residences whose occupancy sum (70b0) >= 1,
//      else use the InteractComponent position, refresh the zone (9c480) and check cell-flag
//      compatibility before taking the entity (ef40 pick).
//   Out: *param_2 = best entity, *param_4 = best found-component, param_5 = best vec3.
//   param_6 is an unused incoming reg the original repurposes as the running "best cost".
//   Returns param_2 (callers ignore the return).
// ============================================================
int64_t* CitizenAI_helper_e370(int64_t* param_1, int64_t* param_2, uint32_t param_3,
                               int64_t* param_4, float* param_5, uint64_t /*param_6*/,
                               int64_t* param_7) {
    EnsureInit();
    int* mapGrid = MapGrid();
    int64_t* origP7 = param_7;
    int64_t* ret = param_2;
    *param_2 = 0;
    int best = 0x7fffffff;

    // -------- Phase A: the preferred candidate (*param_7) --------
    int64_t preferred = *param_7;
    if (preferred != 0) {
        float vec[4] = {0, 0, 0, 0};
        int64_t candIBP = preferred;
        uint32_t cell = s_fn_7fb0 ? s_fn_7fb0(&candIBP, vec) : 0;
        uint64_t valid = s_fn_a00f0 ? s_fn_a00f0(mapGrid, 0xe1f, param_3, cell) : 0;
        if (static_cast<char>(valid) != '\0') {
            int dist = s_fn_8830 ? s_fn_8830(mapGrid, cell, param_3) : 0;
            int64_t candA = *origP7;
            int64_t outSlot = 0;
            if (s_fn_ef40) s_fn_ef40(reinterpret_cast<void*>(candA), &outSlot, &candA, 0);
            if (outSlot != 0) {
                *param_2 = *origP7;
                param_5[0] = vec[0];
                param_5[1] = vec[1];
                param_5[2] = vec[2];
                best = dist;
            }
        }
    }

    // -------- Phase B: market candidate list (param_1[2] / count param_1[3]) --------
    if (0 < static_cast<int>(param_1[3])) {
        int i = 0;
        int64_t off = 0;
        do {
            int64_t cand = *reinterpret_cast<int64_t*>(off + param_1[2]);
            int64_t candIBP = cand;
            uint32_t cell = s_fn_7fb0 ? s_fn_7fb0(&candIBP, nullptr) : 0;
            int cost = CellCost(mapGrid, cell, param_3);
            if (cost < best) {
                int64_t candE = cand;
                int64_t outSlot = 0;
                if (s_fn_ef40) s_fn_ef40(reinterpret_cast<void*>(mapGrid), &outSlot, &candE, 0);
                int64_t slot = outSlot;
                if (outSlot != 0) {
                    float vec[4] = {0, 0, 0, 0};
                    int64_t marketIBP[2] = {0, 0};
                    void* mp = s_fn_78c0 ? s_fn_78c0(cand, marketIBP) : nullptr;
                    int64_t marketComp = mp ? *reinterpret_cast<int64_t*>(mp) : 0;
                    int mcost = (marketComp && s_fn_edb0) ? s_fn_edb0(marketComp, param_3, vec) : -1;
                    if ((-1 < mcost) && (mcost < best)) {
                        *param_2 = cand;
                        param_5[0] = vec[0];
                        param_5[1] = vec[1];
                        param_5[2] = vec[2];
                        *param_4 = slot;
                        best = mcost;
                    }
                }
            }
            ++i;
            off += 8;
        } while (i < static_cast<int>(param_1[3]));
    }

    // -------- Phase C: raw-material / storage candidate list (param_1[0] / count param_1[1]) --------
    if (0 < static_cast<int>(param_1[1])) {
        int i = 0;
        int64_t off = 0;
        do {
            int64_t cand = *reinterpret_cast<int64_t*>(off + *param_1);
            int64_t candIBP = cand;
            uint32_t cell = s_fn_7fb0 ? s_fn_7fb0(&candIBP, nullptr) : 0;
            int cost = CellCost(mapGrid, cell, param_3);
            if (cost < best) {
                int64_t candE = cand;
                int64_t slotOut[2] = {0, 0};
                if (s_fn_ef40) s_fn_ef40(reinterpret_cast<void*>(mapGrid), slotOut, &candE, 0);
                int64_t slot = slotOut[0];
                if (slot != 0) {
                    // RawMaterialDescription on the ef40-picked slot's comp-list2 (slot+0x28 / +0x30)
                    void* rawMat = ScanCompList2(slot, s_tid_RawMaterialDesc);

                    // StorageLocationComponent on the candidate entity (comp-list1)
                    void* storComp = ScanCompList1(cand, s_tid_StorageLocationComp);
                    int64_t storIBP = 0;
                    if (fn_InternalBasePtr_Ctor) new (&storIBP) ::Object::InternalBasePtr(
                        reinterpret_cast<RtObject::Internal*>(storComp));   // B.3.3 bridge

                    if (storIBP != 0) {
                        int64_t storC = storIBP;
                        // mask from StorageLocationDescription sub-object at storC+0x18, +0x20
                        void* sub = *reinterpret_cast<void**>(storC + 0x18);
                        void* derived = (sub && fn_IsDerivedFrom && fn_IsDerivedFrom(sub, s_tid_StorageLocationDesc)) ? sub : nullptr;
                        uint32_t mask = derived ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derived) + 0x20) : 0;
                        if (mask == 0) {
                            float tmp[4] = {0, 0, 0, 0};
                            void* sp = s_fn_findStor6270 ? s_fn_findStor6270(*reinterpret_cast<int64_t*>(storC + 0x10), tmp) : nullptr;
                            int64_t s = sp ? *reinterpret_cast<int64_t*>(sp) : 0;
                            // [M-D SCOPED FALLBACK] the registered 6270 port's typeid is dead
                            // .rdata (0x1401c7e78 unregistered) -> never matches in standalone;
                            // self-contained scan of the owning entity instead (do NOT fill the
                            // shared global). Same lesson as GrowthComponent +0x30.
                            if (s == 0) {
                                static uint32_t s_tidStorageComp = 0;
                                if (s_tidStorageComp == 0)
                                    s_tidStorageComp = StorageComponent::TypeId();
                                s = reinterpret_cast<int64_t>(
                                    ScanCompList1(*reinterpret_cast<int64_t*>(storC + 0x10),
                                                  s_tidStorageComp));
                            }
                            mask = s ? *reinterpret_cast<uint32_t*>(s + 0x3c) : 0;
                        }
                        uint32_t radiusFlags = rawMat ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x5c) : 0;
                        if ((radiusFlags & mask) != 0) {
                            float vec[4] = {0, 0, 0, 0};
                            // 2nd StorageLocationComponent scan over the entity (cost source)
                            void* costComp = ScanCompList1(cand, s_tid_StorageLocationComp);
                            int64_t costIBP = 0;
                            if (fn_InternalBasePtr_Ctor) new (&costIBP) ::Object::InternalBasePtr(
                                reinterpret_cast<RtObject::Internal*>(costComp));   // B.3.3 bridge
                            int64_t costSrc = costIBP;
                            int dcost = (costSrc && s_fn_d5d0) ? s_fn_d5d0(costSrc, param_3, vec, 0) : -1;
                            if ((-1 < dcost) && (dcost < best)) {
                                param_5[0] = vec[0];
                                param_5[1] = vec[1];
                                *param_2 = cand;
                                param_5[2] = vec[2];
                                *param_4 = slot;
                                best = dcost;
                            }
                        }
                    }
                }
            }
            ++i;
            off += 8;
        } while (i < static_cast<int>(param_1[1]));
    }

    // -------- Phase D: residence candidate list (param_1[4] / count param_1[5]) --------
    if (0 < static_cast<int>(param_1[5])) {
        int i = 0;
        int64_t off = 0;
        do {
            int64_t cand = *reinterpret_cast<int64_t*>(off + param_1[4]);

            // ResidenceComponent on the entity (comp-list1 at cand+0x30 / +0x38)
            void* resComp = ScanCompList1(cand, s_tid_ResidenceComponent);
            int64_t resIBP = 0;
            if (fn_InternalBasePtr_Ctor) new (&resIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(resComp));   // B.3.3 bridge
            uint64_t occ = CitizenAI_helper_70b0(resIBP);

            if (static_cast<int>(occ) < 1) {
                // InteractComponent on the entity -> world position
                void* interact = ScanCompList1(cand, s_tid_InteractComponent);
                int64_t interactIBP = 0;
                if (fn_InternalBasePtr_Ctor) new (&interactIBP) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(interact));   // B.3.3 bridge

                float vec[4] = {0, 0, 0, 0};
                if (s_fn_2e20) s_fn_2e20(interactIBP, vec, 0, 0, nullptr);
                uint32_t cell = WorldToCell(mapGrid, vec);

                // path cost + ZoneRefresh args (uVar30/uVar22 carry cell/goal or %w/dist² per fallback)
                uint64_t zr_a = cell;
                uint64_t zr_b = param_3;
                int cost = s_fn_f810 ? s_fn_f810(mapGrid, cell, param_3, '\x01') : -1;
                if (cost < 0) {
                    int w = *mapGrid;
                    zr_a = static_cast<uint32_t>(static_cast<int>(param_3) % w);
                    int dRow = static_cast<int>(cell) / w - static_cast<int>(param_3) / w;
                    int dCol = static_cast<int>(cell) % w - static_cast<int>(param_3) % w;
                    uint32_t d2 = static_cast<uint32_t>(dRow * dRow + dCol * dCol);
                    zr_b = d2;
                    float r = s_fn_sqrtf ? s_fn_sqrtf(static_cast<float>(static_cast<int>(d2))) : 0.0f;
                    cost = static_cast<int>(r) + w;
                }
                if (cost < best) {
                    if (s_fn_9c480) s_fn_9c480(mapGrid, zr_a, zr_b, 1);
                    int64_t cellArr = *reinterpret_cast<int64_t*>(mapGrid + 0x20);  // int* +0x20 = +0x80 bytes
                    int64_t cGoal = static_cast<int64_t>(static_cast<int>(param_3)) * 0x30 + cellArr;
                    int64_t cCand = static_cast<int64_t>(static_cast<int>(cell)) * 0x30 + cellArr;
                    uint32_t fGoal = 1u << (*reinterpret_cast<uint8_t*>(cGoal + 0x29) & 0x1f);
                    uint32_t fCand = 1u << (*reinterpret_cast<uint8_t*>(cCand + 0x29) & 0x1f);
                    uint32_t both = fCand | fGoal;
                    uint32_t flags = both | 0xe1f;
                    bool proceed = true;
                    if (((flags & fGoal) != 0) && ((flags & fCand) != 0)) {
                        if ((both & 0x1e0) == 0) {
                            uint16_t gz = *reinterpret_cast<uint16_t*>(cGoal + 0x26);
                            if (gz == 0) {
                                proceed = false;
                            } else {
                                uint16_t cz = *reinterpret_cast<uint16_t*>(cCand + 0x26);
                                if (cz == 0 || gz != cz) proceed = false;
                            }
                        }
                        if (proceed) {
                            int64_t entHolder[2] = {cand, 0};
                            int64_t outSlot = 0;
                            if (s_fn_ef40) s_fn_ef40(reinterpret_cast<void*>(static_cast<uintptr_t>(flags)), &outSlot, entHolder, 0);
                            if ((outSlot != 0) && (-1 < cost)) {
                                *param_2 = cand;
                                param_5[0] = vec[0];
                                param_5[1] = vec[1];
                                param_5[2] = vec[2];
                                *param_4 = outSlot;
                                best = cost;
                            }
                        }
                    }
                }
            }
            ++i;
            off += 8;
        } while (i < static_cast<int>(param_1[5]));
    }

    return ret;
}

