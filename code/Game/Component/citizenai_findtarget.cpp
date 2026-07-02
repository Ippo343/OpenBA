// code/Game/Component/citizenai_findtarget.cpp
//
// Faithful 1:1 port of the CitizenAI "find-best-target" linchpin helper
// CitizenAI_State_sub_dd30 (0x14008dd30) plus its only UNREGISTERED sub-callee
// CitizenAI_sub_78c0 (0x1400178c0). dd30 is needed by 6 CitizenAI state handlers
// (Gather / Education / GrowUp / SeekHappiness etc.) — it scans two candidate
// lists (a "primary" list at param_1[2]/[3] and a "fallback" list at param_1[0]/[1]),
// path-distance-ranks each candidate with BuildStructure_impl_f810 / FUN_140132010,
// and returns (via out-params) the best entity + a target vec3 + a found component.
//
// IMPORTANT ADDRESS CORRECTIONS (the task brief used wrong addresses):
//   BuildStructure_impl_f810 is actually at 0x14009f810 (NOT 0x14008f810) and is
//       ALREADY REGISTERED as Pf_IsDstReachable_f810 (reg_pf_roots_route.cpp).
//   CitizenAI_sub_edb0      is actually at 0x14004edb0 (NOT 0x14008edb0) and is
//       ALREADY REGISTERED as Pf_PickReachableCell_edb0 (reg_pf_road.cpp).
//   CitizenAI_sub_78c0      is actually at 0x1400178c0 (NOT 0x1400878c0) and is
//       the ONLY function in this set that was unregistered → ported here.
//
// So this file ports exactly TWO functions:
//   CitizenAI_FindBestTarget_dd30  (0x14008dd30)  — the main helper
//   CitizenAI_sub_78c0             (0x1400178c0)  — find MarketLocationComponent on an entity
//
// All other sub-callees are already registered and called via ResolveAddr:
//   0x14009f810 Pf_IsDstReachable_f810        (BuildStructure_impl_f810; path cost)
//   0x14004edb0 Pf_PickReachableCell_edb0     (CitizenAI_sub_edb0; pick reachable goal)
//   0x140087fb0 Pf_EntityAnchorCell_7fb0      (Entity_impl_7fb0; entity -> cell idx)
//   0x14008ef40 CitizenComp_Entity_impl_ef40  (Entity_impl_ef40; pick free slot)
//   0x14012d5d0 Pf_FindBestAdjacentCell_d5d0  (FUN_14012d5d0; cost-to-adjacent)
//   0x140132010 WorkImpl_Sqrtf_132010         (FUN_140132010; sqrtf)
//   0x140006270 WorkBase_FindComponent_6270   (Entity_FindComponent_Storage)
//
// TYPE-ID GLOBALS (.rdata BaseTypeInfo, the first DWORD = the runtime CRC type id).
// In standalone these are UNFILLED (0xFFFFFFFF) so the original's type-matched
// component scans would find NOTHING and then dereference a NULL result (crash).
// Fix (project "Brick" pattern): resolve each one self-contained via
// Object::GetTypeIdByName("<TypeName>") so the scans actually match. TypeNames
// recovered from each global's *_RegisterType writer (Ghidra-verified):
//   DWORD_1401c4870 -> "RadiusDescription"          (RadiusDescription_RegisterType)
//   DWORD_1401c7cb8 -> "RawMaterialDescription"     (RawMaterialDescription_Register)
//   DAT_1401c7ee8   -> "StorageLocationComponent"   (StorageLocationComponent_RegisterType)
//   DAT_1401c7f20   -> "StorageLocationDescription" (StorageLocationDescription_Register)
//   DAT_1401c4560   -> "MarketLocationComponent"    (MarketLocationComponent_RegisterType; used by 78c0)
// Every dereference of a scan-result pointer is additionally NULL-guarded so the
// port is crash-safe even if a type id is unresolvable.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#ifdef STANDALONE_EXE

#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>

namespace {

// --- registered exe sub-callees (resolved lazily via ResolveAddr) ---
typedef uint32_t (*fn_7fb0_t)(void* ibpOut, float* p2);                       // 0x140087fb0
typedef int      (*fn_f810_t)(int* mapGrid, uint32_t fromCell, uint32_t toCell, char addCache); // 0x14009f810
typedef int      (*fn_edb0_t)(int64_t entity, uint32_t goalCell, float* outVec3); // 0x14004edb0
typedef void     (*fn_ef40_t)(int* mapGrid, int64_t* outRes, void* ibp, uint32_t cat); // 0x14008ef40
typedef int      (*fn_d5d0_t)(int64_t entity, uint32_t goalCell, float* outVec3, uint64_t ctx); // 0x14012d5d0
typedef float    (*fn_sqrtf_t)(float v);                                      // 0x140132010
typedef void*    (*fn_findStorage_t)(int64_t entity, void* outIBP);           // 0x140006270

static fn_7fb0_t        s_fn_7fb0        = nullptr;
static fn_f810_t        s_fn_f810        = nullptr;
static fn_edb0_t        s_fn_edb0        = nullptr;
static fn_ef40_t        s_fn_ef40        = nullptr;
static fn_d5d0_t        s_fn_d5d0        = nullptr;
static fn_sqrtf_t       s_fn_sqrtf       = nullptr;
static fn_findStorage_t s_fn_findStorage = nullptr;

// --- type ids (self-contained via GetTypeIdByName; see header note) ---
static uint32_t s_tid_RadiusDescription      = 0;  // DWORD_1401c4870
static uint32_t s_tid_RawMaterialDescription = 0;  // DWORD_1401c7cb8
static uint32_t s_tid_StorageLocationComp    = 0;  // DAT_1401c7ee8
static uint32_t s_tid_StorageLocationDesc    = 0;  // DAT_1401c7f20
static uint32_t s_tid_MarketLocationComp     = 0;  // DAT_1401c4560

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_fn_7fb0        = reinterpret_cast<fn_7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_f810        = reinterpret_cast<fn_f810_t>(ResolveAddr(Module::Exe, 0x14009f810));
    s_fn_edb0        = reinterpret_cast<fn_edb0_t>(ResolveAddr(Module::Exe, 0x14004edb0));
    s_fn_ef40        = reinterpret_cast<fn_ef40_t>(ResolveAddr(Module::Exe, 0x14008ef40));
    s_fn_d5d0        = reinterpret_cast<fn_d5d0_t>(ResolveAddr(Module::Exe, 0x14012d5d0));
    s_fn_sqrtf       = reinterpret_cast<fn_sqrtf_t>(ResolveAddr(Module::Exe, 0x140132010));
    s_fn_findStorage = reinterpret_cast<fn_findStorage_t>(ResolveAddr(Module::Exe, 0x140006270));

    if (s_tid_RadiusDescription == 0)      s_tid_RadiusDescription      = ::Object::GetTypeIdByName("RadiusDescription");
    if (s_tid_RawMaterialDescription == 0) s_tid_RawMaterialDescription = ::Object::GetTypeIdByName("RawMaterialDescription");
    if (s_tid_StorageLocationComp == 0)    s_tid_StorageLocationComp    = ::Object::GetTypeIdByName("StorageLocationComponent");
    if (s_tid_StorageLocationDesc == 0)    s_tid_StorageLocationDesc    = ::Object::GetTypeIdByName("StorageLocationDescription");
    if (s_tid_MarketLocationComp == 0)     s_tid_MarketLocationComp     = ::Object::GetTypeIdByName("MarketLocationComponent");
    s_init = true;
}

// vtable[+0x10] == Object::Base::GetTypeId(this) — read the runtime type id of a component.
static inline uint32_t CompTypeId(void* comp) {
    uintptr_t vt = *reinterpret_cast<uintptr_t*>(comp);
    typedef uint32_t (*getType_t)();
    return (*reinterpret_cast<getType_t*>(vt + 0x10))();
}

} // namespace

// ============================================================
// CitizenAI_sub_78c0 (0x1400178c0) — scan an entity's component list for the
// MarketLocationComponent (type id DAT_1401c4560) and return it wrapped in an
// InternalBasePtr (out param_2). Faithful to the decompile: walk components at
// (param_1+0x30)[i], match GetTypeId()==tid, then IsDerivedFrom-confirm; the
// confirmed component is stored into the out IBP via InternalBasePtr::Ctor.
//   param_1 = entity (ComponentEntity*); param_2 = out InternalBasePtr storage.
// ============================================================
void* CitizenAI_sub_78c0(int64_t param_1, void* param_2) {
    EnsureInit();
    void* ptr = nullptr;
    uint32_t tid = s_tid_MarketLocationComp;
    int count = *reinterpret_cast<int*>(param_1 + 0x38);
    if (0 < count) {
        int i = 0;
        int64_t byteOff = 0;
        do {
            int64_t* ibp = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + byteOff);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
            void* comp = reinterpret_cast<void*>(*ibp);
            if (comp && CompTypeId(comp) == tid) {
                int64_t* ibp2 = reinterpret_cast<int64_t*>(
                    *reinterpret_cast<int64_t*>(param_1 + 0x30) + static_cast<int64_t>(i) * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp2);
                void* cand = reinterpret_cast<void*>(*ibp2);
                if (cand && fn_IsDerivedFrom && fn_IsDerivedFrom(cand, tid)) {
                    ptr = cand;
                }
                goto done;
            }
            i = i + 1;
            byteOff += 8;
        } while (i < count);
    }
    ptr = nullptr;
done:
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(param_2, ptr);
    return param_2;
}

// ============================================================
// CitizenAI_FindBestTarget_dd30 (0x14008dd30)
//   param_1 = candidate-list pair struct:
//             param_1[0]=fallbackArr, param_1[1]=fallbackCount,
//             param_1[2]=primaryArr,  param_1[3]=primaryCount
//   param_2 = out: best entity ptr (longlong)        [written 0 at entry]
//   param_3 = goal cell / dest A (uint)
//   param_4 = goal cell / dest B (uint, -1 => skip the "B" range check)
//   param_5 = out: found component ptr (longlong)
//   param_6 = out: target vec3 (float[3])
//   param_7 = a category passed to Entity_impl_ef40
// Returns param_1 (matches the original; callers ignore it).
//
// Field offsets used (Ghidra-verified against the decompile + disassembly):
//   g_pGameWorldSingleton + 0xf8  -> int* mapGrid (local_80); mapGrid[0] = grid width
//   candidate entity: +0x28 -> component-list2 holder; +0x30 -> component array (count at +0x38)
//   component-list2 holder +0x28 -> array, +0x30 -> count (RadiusDescription scan)
//   matched RadiusDescription (pvVar12) +0x28 -> a comparison threshold (iVar15)
//   matched RadiusDescription (pvVar12) +0x5c -> a flag bitmask (2nd phase)
//   StorageLocationDescription sub-object: (found+0x18) -> base for IsDerivedFrom(+0x20 mask)
//   if mask==0: Entity_FindComponent_Storage(found+0x10) -> *ptr +0x3c = mask
// ============================================================
int64_t* CitizenAI_FindBestTarget_dd30(int64_t* param_1, int64_t* param_2, uint32_t param_3,
                                       uint32_t param_4, int64_t* param_5, float* param_6,
                                       uint32_t param_7) {
    EnsureInit();

    int* local_80 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    *param_2 = 0;
    uint32_t local_98_flag = 0;       // low byte = "early-return" gate (set when a "B"-qualified hit found)
    int local_a8_best = 0x7fffffff;   // best cost so far

    // -------- Phase 1: scan the PRIMARY list (param_1[2], count param_1[3]) --------
    if (0 < static_cast<int>(param_1[3])) {
        int iVar20 = 0;
        int64_t lVar21 = 0;
        do {
            int64_t local_a0 = *reinterpret_cast<int64_t*>(lVar21 + param_1[2]);  // candidate entity
            int64_t local_90 = local_a0;                                         // IBP scratch
            uint32_t cell = s_fn_7fb0 ? s_fn_7fb0(&local_90, nullptr) : 0;        // entity -> cell idx
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_a0);

            // -- find RadiusDescription (DWORD_1401c4870) in candidate component-list2 (+0x28) --
            int64_t lVar16 = *reinterpret_cast<int64_t*>(local_a0 + 0x28);
            void* pvVar12 = nullptr;
            {
                int n = *reinterpret_cast<int*>(lVar16 + 0x30);
                int idx = 0; int64_t off = 0;
                while (idx < n) {
                    int64_t* compSlot = *reinterpret_cast<int64_t**>(
                        *reinterpret_cast<int64_t*>(lVar16 + 0x28) + off * 8);
                    if (CompTypeId(compSlot) == s_tid_RadiusDescription) {
                        void* c = reinterpret_cast<void*>(
                            *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar16 + 0x28) + off * 8));
                        if (c && fn_IsDerivedFrom && fn_IsDerivedFrom(c, s_tid_RadiusDescription))
                            pvVar12 = c;
                        break;
                    }
                    idx++; off++;
                }
            }
            int iVar15 = pvVar12 ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pvVar12) + 0x28) : 0;

            // -- compute whether candidate is within the "B" (param_4) distance band --
            bool bVar5 = false;
            if (-1 < static_cast<int>(param_4)) {
                int iVar8 = s_fn_f810 ? s_fn_f810(local_80, cell, param_4, '\x01') : -1;
                if (iVar8 < 0) {
                    int w = *local_80;
                    int dRow = static_cast<int>(cell) / w - static_cast<int>(param_4) / w;
                    int dCol = static_cast<int>(cell) % w - static_cast<int>(param_4) % w;
                    float r = s_fn_sqrtf ? s_fn_sqrtf(static_cast<float>(dRow * dRow + dCol * dCol)) : 0.0f;
                    iVar8 = static_cast<int>(r) + w;
                }
                bVar5 = (iVar8 <= iVar15);
            }

            // -- compute path cost from candidate to the "A" (param_3) goal cell --
            int iVar15cost = s_fn_f810 ? s_fn_f810(local_80, cell, param_3, '\x01') : -1;
            if (iVar15cost < 0) {
                int w = *local_80;
                int dRow = static_cast<int>(cell) / w - static_cast<int>(param_3) / w;
                int dCol = static_cast<int>(cell) % w - static_cast<int>(param_3) % w;
                float r = s_fn_sqrtf ? s_fn_sqrtf(static_cast<float>(dRow * dRow + dCol * dCol)) : 0.0f;
                iVar15cost = static_cast<int>(r) + w;
            }

            if (iVar15cost < local_a8_best) {
                local_90 = local_a0;
                int64_t local_88 = 0;
                if (s_fn_ef40) s_fn_ef40(local_80, &local_88, &local_90, param_7);  // pick free slot
                if (local_88 != 0) {
                    uint32_t newFlag = local_98_flag & 0xff;
                    if (bVar5) newFlag = 1;
                    local_98_flag = newFlag;

                    float local_78[3] = {0.0f, 0.0f, 0.0f};
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_a0);
                    int64_t local_50_ibp[2] = {0, 0};
                    void* pIVar10 = CitizenAI_sub_78c0(local_a0, local_50_ibp);  // find MarketLocationComponent
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pIVar10);
                    int64_t marketComp = *reinterpret_cast<int64_t*>(pIVar10);
                    int cost = (marketComp && s_fn_edb0) ? s_fn_edb0(marketComp, param_3, local_78) : -1;
                    if ((-1 < cost) && (cost < local_a8_best)) {
                        *param_2 = local_a0;
                        param_6[0] = local_78[0];
                        param_6[1] = local_78[1];
                        param_6[2] = local_78[2];
                        *param_5 = local_88;
                        local_a8_best = cost;
                    }
                }
            }
            iVar20 = iVar20 + 1;
            lVar21 = lVar21 + 8;
        } while (iVar20 < static_cast<int>(param_1[3]));

        // If a "B"-qualified primary hit was found, return immediately (skip fallback).
        if ((static_cast<char>(local_98_flag) != '\0') && (*param_2 != 0)) {
            return param_1;
        }
    }

    // -------- Phase 2: scan the FALLBACK list (param_1[0], count param_1[1]) --------
    if (0 < static_cast<int>(param_1[1])) {
        int iVar20 = 0;
        int64_t lVarItr = 0;
        int iVar15loop = local_a8_best;
        do {
            int64_t local_a0 = *reinterpret_cast<int64_t*>(lVarItr + *param_1);  // candidate entity
            int64_t local_58 = local_a0;                                        // IBP scratch
            uint32_t cell = s_fn_7fb0 ? s_fn_7fb0(&local_58, nullptr) : 0;

            // path cost candidate -> param_3
            int iVar8 = s_fn_f810 ? s_fn_f810(local_80, cell, param_3, '\x01') : -1;
            if (iVar8 < 0) {
                int w = *local_80;
                int dRow = static_cast<int>(cell) / w - static_cast<int>(param_3) / w;
                int dCol = static_cast<int>(cell) % w - static_cast<int>(param_3) % w;
                float r = s_fn_sqrtf ? s_fn_sqrtf(static_cast<float>(dRow * dRow + dCol * dCol)) : 0.0f;
                iVar8 = static_cast<int>(r) + w;
            }

            if (iVar8 < iVar15loop) {
                uint64_t uVar19 = static_cast<uint64_t>(param_7);  // ctx forwarded to d5d0
                local_58 = local_a0;
                int64_t local_50[2] = {0, 0};
                if (s_fn_ef40) s_fn_ef40(local_80, local_50, &local_58, param_7);  // pick free slot
                int64_t lVar21 = local_50[0];
                if (local_50[0] != 0) {
                    // -- find RawMaterialDescription (DWORD_1401c7cb8) in slot's list (lVar21+0x28) --
                    void* pvVar12 = nullptr;
                    {
                        int n = *reinterpret_cast<int*>(local_50[0] + 0x30);
                        int idx = 0; int64_t off = 0;
                        while (idx < n) {
                            int64_t* compSlot = *reinterpret_cast<int64_t**>(
                                *reinterpret_cast<int64_t*>(lVar21 + 0x28) + off * 8);
                            if (CompTypeId(compSlot) == s_tid_RawMaterialDescription) {
                                void* c = reinterpret_cast<void*>(
                                    *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar21 + 0x28) + off * 8));
                                if (c && fn_IsDerivedFrom && fn_IsDerivedFrom(c, s_tid_RawMaterialDescription))
                                    pvVar12 = c;
                                break;
                            }
                            idx++; off++;
                        }
                    }

                    // -- find StorageLocationComponent (DAT_1401c7ee8) in candidate entity list (+0x30/+0x38) --
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_a0);
                    int64_t lVar16 = local_a0;
                    void* pvVar14 = nullptr;
                    {
                        int n = *reinterpret_cast<int*>(local_a0 + 0x38);
                        int idx = 0; int64_t off = 0;
                        while (idx < n) {
                            int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar16 + 0x30) + off * 8);
                            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                            void* comp = reinterpret_cast<void*>(*slot);
                            if (comp && CompTypeId(comp) == s_tid_StorageLocationComp) {
                                int64_t* slot2 = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar16 + 0x30) + off * 8);
                                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
                                void* c = reinterpret_cast<void*>(*slot2);
                                if (c && fn_IsDerivedFrom && fn_IsDerivedFrom(c, s_tid_StorageLocationComp))
                                    pvVar14 = c;
                                break;
                            }
                            idx++; off++;
                        }
                    }

                    int64_t local_98_ibp = 0;  // CONCAT44(uStack_94, local_98) — IBP for pvVar14
                    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_98_ibp, pvVar14);

                    if (local_98_ibp != 0) {
                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_98_ibp);
                        int64_t lVarComp = local_98_ibp;
                        // IsDerivedFrom((found+0x18), StorageLocationDescription) -> read +0x20 mask
                        void* sub = *reinterpret_cast<void**>(lVarComp + 0x18);
                        void* derived = (sub && fn_IsDerivedFrom && fn_IsDerivedFrom(sub, s_tid_StorageLocationDesc)) ? sub : nullptr;
                        uint32_t mask = derived ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derived) + 0x20) : 0;
                        if (mask == 0) {
                            // fall back to Entity_FindComponent_Storage(found+0x10)->+0x3c
                            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVarComp + 0x10));
                            float local_78s[4] = {0,0,0,0};
                            void* pIVar10 = s_fn_findStorage ? s_fn_findStorage(*reinterpret_cast<int64_t*>(lVarComp + 0x10), local_78s) : nullptr;
                            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pIVar10);
                            int64_t stor = pIVar10 ? *reinterpret_cast<int64_t*>(pIVar10) : 0;
                            // [M-D SCOPED FALLBACK] the registered 6270-family finder reads
                            // dead .rdata typeid 0x1401c7e78 (unregistered in standalone) ->
                            // never matches; self-contained scan of the owning entity's comp
                            // list instead (shared global stays unfilled by design).
                            if (stor == 0) {
                                static uint32_t s_tidStorageComp = 0;
                                if (s_tidStorageComp == 0)
                                    s_tidStorageComp = ::Object::GetTypeIdByName("StorageComponent");
                                int64_t ownEnt = *reinterpret_cast<int64_t*>(lVarComp + 0x10);
                                if (ownEnt && s_tidStorageComp) {
                                    int n2 = *reinterpret_cast<int*>(ownEnt + 0x38);
                                    int64_t arr2 = *reinterpret_cast<int64_t*>(ownEnt + 0x30);
                                    for (int k = 0; k < n2 && arr2; ++k) {
                                        void* c2 = *reinterpret_cast<void**>(arr2 + (int64_t)k * 8);
                                        if (c2 && CompTypeId(c2) == s_tidStorageComp) { stor = reinterpret_cast<int64_t>(c2); break; }
                                    }
                                }
                            }
                            mask = stor ? *reinterpret_cast<uint32_t*>(stor + 0x3c) : 0;
                        }

                        uint32_t radiusFlags = pvVar12 ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(pvVar12) + 0x5c) : 0;
                        if ((radiusFlags & mask) != 0) {
                            float local_68[3] = {0.0f, 0.0f, 0.0f};

                            // -- 2nd RawMaterialDescription-style scan? NO: this is the StorageLocationComponent
                            //    scan again over the candidate entity (+0x30/+0x38) for the COST source --
                            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_a0);
                            int64_t lVar16b = local_a0;
                            void* pvVar12b = nullptr;
                            {
                                int n = *reinterpret_cast<int*>(local_a0 + 0x38);
                                int idx = 0; int64_t off = 0;
                                while (idx < n) {
                                    int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar16b + 0x30) + off * 8);
                                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                                    void* comp = reinterpret_cast<void*>(*slot);
                                    if (comp && CompTypeId(comp) == s_tid_StorageLocationComp) {
                                        int64_t* slot2 = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(lVar16b + 0x30) + off * 8);
                                        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot2);
                                        void* c = reinterpret_cast<void*>(*slot2);
                                        if (c && fn_IsDerivedFrom && fn_IsDerivedFrom(c, s_tid_StorageLocationComp))
                                            pvVar12b = c;
                                        break;
                                    }
                                    idx++; off++;
                                }
                            }

                            int64_t local_88_ibp = 0;
                            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&local_88_ibp, pvVar12b);
                            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&local_88_ibp);
                            int64_t costSrc = local_88_ibp;
                            int cost = (costSrc && s_fn_d5d0) ? s_fn_d5d0(costSrc, param_3, local_68, uVar19) : -1;
                            if ((-1 < cost) && (cost < local_a8_best)) {
                                *param_2 = local_a0;
                                param_6[0] = local_68[0];
                                param_6[1] = local_68[1];
                                param_6[2] = local_68[2];
                                *param_5 = lVar21;
                                iVar15loop = cost;
                                local_a8_best = cost;
                            }
                        }
                    }
                }
            }
            lVarItr = lVarItr + 8;
            iVar20 = iVar20 + 1;
        } while (iVar20 < static_cast<int>(param_1[1]));
    }

    return param_1;
}

#endif // STANDALONE_EXE
