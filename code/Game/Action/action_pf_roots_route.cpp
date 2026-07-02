// code/Game/Action/action_pf_roots_route.cpp
// Wave 3 (Action) / Cluster A — routing/storage-selection ROOT callers.
//
// Faithful 1:1 ports of the four high-level routing roots that drive region routing and
// storage-drop selection. None is runtime-exercised on the autostart path, so a wrong port
// is undetectable silent corruption (strictly worse than leaving the addr unregistered).
// Transcribed literally from the Ghidra decompile; cross-checked against the disassembly.
//
// Sub-callees resolved via ResolveAddr(Module::Exe, 0xADDR) into once-resolved static
// fn-ptrs. Engine helpers (IBP/IsDerivedFrom/FindComponent) use the existing engine_funcs.h
// pointers. Registration state of each sub-callee (confirmed from disassembly CALL targets):
//   0x1400a00f0  WorkBase_ValidateCell      — REG (reg_work_base)
//   0x14009d0a0  Pf_RegionSpiral            — REG (reg_pf_geom)  [Ghidra mislabels as d0a0]
//   0x1400a5ac0  Pf_RegionPairLookup        — REG (reg_pf_geom)
//   0x1400a8ba0  Pf_RegionPathLength        — REG (reg_pf_region; its dep 1dc00 now reg)
//   0x1400a5b30  Pf_RegionPairCacheInsert   — REG (reg_pf_region)
//   0x14012c560  Pf_ResourceLimitFlagTest   — REG (reg_pf_ctors)  [12cd80 dep]
//   0x140123070  Pf_RecursiveChildSum       — REG (reg_pf_ctors)
//   0x140122e20  Pf_EntityResolvePos        — REG (reg_pf_road)
//   0x14009c480  ZoneRefresh                — REG (reg_pf_roots_road; the pathfinding wave landed it)
//   0x140008830  WorkBase_CalcDistance      — REG (reg_work_base)
//   0x14009f3e0  fn_MapGetTerrainPos        — REG (address_registry M5 5c)
//   0x140087fb0  Pf_EntityAnchorCell        — REG (reg_pf_road)
//   0x14004edb0  Pf_PickReachableCell       — REG (reg_pf_road)
//   0x140004680  FloorComponent_FindResData — REG (reg_component_nature)  [member-ABI FindComp]
//   0x140002ed0  Entity_FindComponent_Position — fn_Entity_FindComponent_Position (engine_funcs)
//   0x140132010  sqrtf                      — REG (reg_work_impl2)
//
// CLOSURE for the registrar (reg_pf_roots_route.cpp): ALL FOUR closure-satisfied → REGISTER.
//   * 0x14009f810  SATISFIED (a8ba0/a5ac0/a5b30/9d0a0/a00f0 all reg)
//   * 0x14012cd80  SATISFIED (12c560 + cf10 QueryCount + IsDerivedFrom)
//   * 0x14012d5d0  SATISFIED (9c480 ZoneRefresh now reg in reg_pf_roots_road)
//   * 0x14008d590  SATISFIED (its only code children are 12cd80/9f810/12d5d0 [this file] +
//                  87fb0/4edb0 [reg]; 12d5d0's closure now closed)
// NOTE: the "deferred" comment in reg_work_accessor.cpp re: 9c480 is STALE — it was landed
//       by the pathfinding wave (reg_pf_roots_road.cpp).
//
// ---- struct offsets reverse-engineered (for the lead's verification) ----
// g_pGameWorldSingleton + 0xf8 → the Map object (`piVar3`/`local_60`), an int*:
//   *(Map)      = mapStride (grid width in cells)
//   *(Map+0x20) = fine-cell array base (stride 0x30): +0x18 int regionId, +0x26 short zoneId/owner,
//                 +0x29 byte passability-flag (tested via `1<<(flag&0x1f) & maskE1f/E07`).
// StorageComponent (param_1 of 12cd80, param "self"): +0x38 int capacity-limit;
//   component-list at +0x28 (ptr array, stride 8) / +0x30 int count; matched storage's
//   ResourceLimit entry at +0x48 (int per-unit size) — DWORD_1401c7cb8 = RawMaterialDescription tid.
// ComponentEntity (8d590 local_c8): +0x30 component ptr-array, +0x38 int count.
//   Its components matched by GetTypeId (vtable+0x10) == DAT type-id, then IsDerivedFrom-narrowed.
//   Type ids: DAT_1401c7ee8 (route/path comp), DAT_1401c4560 (?), DWORD_1401c7e78 (StorageComponent),
//   DAT_1401c7f20 (BuildComponent-ish, +0x20 flags, +0x3c flags via residence +0x10 inner), and
//   pvVar19 (the drop-source comp on param_4 list, +0x5c flag mask vs storage +0x3c/+0x20).
//   Storage accept-count fast path (param_6==0): walk local_a8+0x28 pair array (stride 0x10,
//   key=param_4 at +0, value=+8 minus reserved +0xc) for headroom vs param_5.
// 8d590 caller args: param_1 = a pair of entity-list roots (param_1 and param_1+2, each
//   {ptr@0, int count@+8}); param_2 = out best-entity slot; param_3 = goal cell; param_4 =
//   source comp/list ptr; param_5 = qty; param_6 = use-capacity-check flag; param_7 = out vec3;
//   param_8 = exclude-entity IBP.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// ---- exe sub-callee fn-ptrs (resolved once) ----
typedef uint64_t (*fn_ValidateCell_t)(int* map, int mask, uint64_t a, uint32_t b);     // 0x1400a00f0
typedef int      (*fn_RegionSpiral_t)(int* map, int idx);                              // 0x14009d0a0
typedef uint32_t (*fn_RegionPairLookup_t)(int64_t cache, uint32_t a, uint32_t b);      // 0x1400a5ac0
typedef int      (*fn_RegionPathLen_t)(int* graph, int a, uint32_t b, uint64_t c);     // 0x1400a8ba0
typedef void     (*fn_RegionPairInsert_t)(uint64_t* cache, int a, int b, uint32_t v);  // 0x1400a5b30
typedef uint32_t (*fn_ResLimitFlag_t)(int64_t self, int64_t resDesc);                  // 0x14012c560
typedef int      (*fn_ChildSum_t)(int64_t node, int flag);                             // 0x140123070
typedef void*    (*fn_EntityResolvePos_t)(int64_t ent, void* outVec3, int p3, int idx, float* p5); // 0x140122e20
typedef void     (*fn_ZoneRefresh_t)(int* map, uint64_t a, uint64_t b, int c);         // 0x14009c480 (UNREG)
typedef int      (*fn_CalcDistance_t)(int* map, uint32_t a, uint32_t b);               // 0x140008830
typedef float*   (*fn_GridToWorld_t)(int* map, float* out3, float* in2, char f, uint32_t* p5); // 0x14009f3e0
typedef uint32_t (*fn_AnchorCell_t)(void* ibp, float* p2);                             // 0x140087fb0
typedef int      (*fn_PickReachable_t)(int64_t ent, uint32_t qty, float* out3);        // 0x14004edb0
typedef void*    (*fn_FindCompMember_t)(int64_t self, void* outIBP);                   // 0x140004680
typedef float    (*fn_Sqrtf_t)(float x);                                              // 0x140132010

static fn_ValidateCell_t      s_ValidateCell      = nullptr;
static fn_RegionSpiral_t      s_RegionSpiral      = nullptr;
static fn_RegionPairLookup_t  s_RegionPairLookup  = nullptr;
static fn_RegionPathLen_t     s_RegionPathLen     = nullptr;
static fn_RegionPairInsert_t  s_RegionPairInsert  = nullptr;
static fn_ResLimitFlag_t      s_ResLimitFlag      = nullptr;
static fn_ChildSum_t          s_ChildSum          = nullptr;
static fn_EntityResolvePos_t  s_EntityResolvePos  = nullptr;
static fn_ZoneRefresh_t       s_ZoneRefresh       = nullptr;
static fn_CalcDistance_t      s_CalcDistance      = nullptr;
static fn_GridToWorld_t       s_GridToWorld       = nullptr;
static fn_AnchorCell_t        s_AnchorCell        = nullptr;
static fn_PickReachable_t     s_PickReachable     = nullptr;
static fn_FindCompMember_t    s_FindComp_4680     = nullptr;
static fn_Sqrtf_t             s_Sqrtf             = nullptr;

// type-id .rdata globals (resolved as raw const uint32_t* — value read at use site).
static const uint32_t* s_pTid_c7cb8 = nullptr; // DWORD_1401c7cb8 RawMaterialDescription
static const uint32_t* s_pTid_c7ee8 = nullptr; // DAT_1401c7ee8
static const uint32_t* s_pTid_c4560 = nullptr; // DAT_1401c4560
static const uint32_t* s_pTid_c7e78 = nullptr; // DWORD_1401c7e78 StorageComponent
static const uint32_t* s_pTid_c7f20 = nullptr; // DAT_1401c7f20
static const float*    s_pHalfCell_172114 = nullptr; // DAT_140172114 (0.5 cell-centre offset)
// g_pGameWorldSingleton (DAT_1401c3dd8) comes from engine_funcs.h — used directly.
typedef uint32_t (*fn_QueryCount_t)(void* self);   // 0x14012cf10 StorageComponent::QueryCount
static fn_QueryCount_t s_QueryCount = nullptr;
static bool s_init = false;

static void EnsureFns() {
    if (s_init) return;
    s_ValidateCell     = reinterpret_cast<fn_ValidateCell_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_RegionSpiral     = reinterpret_cast<fn_RegionSpiral_t>(ResolveAddr(Module::Exe, 0x14009d0a0));
    s_RegionPairLookup = reinterpret_cast<fn_RegionPairLookup_t>(ResolveAddr(Module::Exe, 0x1400a5ac0));
    s_RegionPathLen    = reinterpret_cast<fn_RegionPathLen_t>(ResolveAddr(Module::Exe, 0x1400a8ba0));
    s_RegionPairInsert = reinterpret_cast<fn_RegionPairInsert_t>(ResolveAddr(Module::Exe, 0x1400a5b30));
    s_ResLimitFlag     = reinterpret_cast<fn_ResLimitFlag_t>(ResolveAddr(Module::Exe, 0x14012c560));
    s_ChildSum         = reinterpret_cast<fn_ChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_EntityResolvePos = reinterpret_cast<fn_EntityResolvePos_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_ZoneRefresh      = reinterpret_cast<fn_ZoneRefresh_t>(ResolveAddr(Module::Exe, 0x14009c480));
    s_CalcDistance     = reinterpret_cast<fn_CalcDistance_t>(ResolveAddr(Module::Exe, 0x140008830));
    s_GridToWorld      = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_AnchorCell       = reinterpret_cast<fn_AnchorCell_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_PickReachable    = reinterpret_cast<fn_PickReachable_t>(ResolveAddr(Module::Exe, 0x14004edb0));
    s_FindComp_4680    = reinterpret_cast<fn_FindCompMember_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_Sqrtf            = reinterpret_cast<fn_Sqrtf_t>(ResolveAddr(Module::Exe, 0x140132010));
    s_pTid_c7cb8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_pTid_c7ee8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ee8));
    s_pTid_c4560 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4560));
    s_pTid_c7e78 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_pTid_c7f20 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7f20));
    s_pHalfCell_172114 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_QueryCount = reinterpret_cast<fn_QueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_init = true;
}

static inline void  CheckPtr(void* p) { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline uint32_t GetTypeId(void* obj) {
    return (*(uint32_t(**)(void*))(*(uintptr_t*)obj + 0x10))(obj);
}

} // namespace

// Forward decls for in-batch cross-calls.
int Pf_IsDstReachable_f810(int* param_1, uint32_t param_2, uint32_t param_3, char param_4);
uint32_t Pf_StorageCanAccept_cd80(int64_t param_1, int64_t param_2, int param_3);
int Pf_FindBestAdjacentCell_d5d0(int64_t param_1, uint32_t param_2, float* param_3, int64_t param_4);

// ============================================================
// 0x14009f810 — Pf_IsDstReachable_f810(map, srcCell, dstCell, useCache) -> length
// ============================================================
// Region-routing reachability/length between two map cells. First ValidateCell (a00f0,
// mask 0xe1f) gates both cells; resolve each cell's region id (cell array +0x18); if either
// is unset, lazily compute it via the region spiral (9d0a0, Ghidra-named d0a0). If both
// resolved and equal → trivially reachable (return 1). Else look up the cached region-pair
// length (a5ac0); on a miss (-1) run the full region A* (a8ba0) and, if useCache, store the
// result back (a5b30). Returns the path length (or -1 unreachable).
//
// Faithfulness: uVar5 = param_3 (the original dstCell) is captured BEFORE param_3 is shadowed
// and is the value passed to a8ba0 as its 4th (ctx) arg. `BuildStructure_Serialize_sub_d0a0`
// in the decompile is 0x14009d0a0 (the region spiral) per the disassembly CALL target.
int Pf_IsDstReachable_f810(int* param_1, uint32_t param_2, uint32_t param_3, char param_4) {
    EnsureFns();
    uint64_t uVar5 = (uint64_t)param_3;
    uint64_t uVar4 = s_ValidateCell(param_1, 0xe1f, (uint64_t)param_2, param_3);
    if ((char)uVar4 != '\0') {
        uint32_t uVar1 = *(uint32_t*)(*(int64_t*)(param_1 + 0x20) + 0x18 + (int64_t)(int)param_2 * 0x30);
        uint32_t uVar2 = *(uint32_t*)(*(int64_t*)(param_1 + 0x20) + 0x18 + (int64_t)(int)param_3 * 0x30);
        if ((int)uVar1 < 0) {
            uVar1 = (uint32_t)s_RegionSpiral(param_1, (int)param_2);
        }
        if ((int)uVar2 < 0) {
            uVar2 = (uint32_t)s_RegionSpiral(param_1, (int)param_3);
        }
        int iVar3;
        if (((int)uVar1 < 0) || ((int)uVar2 < 0)) {
            iVar3 = -1;
        } else if (uVar1 == uVar2) {
            iVar3 = 1;
        } else {
            iVar3 = (int)s_RegionPairLookup(*(int64_t*)(param_1 + 0x1e), uVar1, uVar2);
            if (iVar3 < 0) {
                iVar3 = s_RegionPathLen(*(int**)(param_1 + 0x1c), uVar1, uVar2, uVar5);
                if (param_4 != '\0') {
                    s_RegionPairInsert(*(uint64_t**)(param_1 + 0x1e), uVar1, uVar2, iVar3);
                }
            }
        }
        return iVar3;
    }
    return -1;
}

// ============================================================
// 0x14012cd80 — Pf_StorageCanAccept_cd80(storage, srcComp, qty) -> bool
// ============================================================
// Capacity check: can `storage` accept `qty` units of the resource carried by `srcComp`?
// First a resource-limit/flag gate (12c560); if it fails, return that (0). Query the
// storage's current count (StorageComponent::QueryCount — inline below as fn pointer path).
// Then scan srcComp's component list (param_2+0x28 ptr array / +0x30 count) for the
// RawMaterialDescription component (GetTypeId == DWORD_1401c7cb8, IsDerivedFrom-narrowed);
// its per-unit size at +0x48. Return (size*qty + currentCount) <= storage capacity (+0x38).
//
// Faithfulness: QueryCount (StorageComponent_QueryCount) is reached via the same component
// machinery the engine uses; we call the registered 12c560 for the flag gate and read the
// fields directly. The Ghidra `uVar3 = QueryCount(...)` is the storage's current total; we
// replicate by reading storage+0x34 is NOT correct — QueryCount is a method. We instead call
// it through its exe address (StorageComponent_QueryCount). See note: QueryCount addr below.
uint32_t Pf_StorageCanAccept_cd80(int64_t param_1, int64_t param_2, int param_3) {
    EnsureFns();
    uint32_t uVar3 = s_ResLimitFlag(param_1, param_2);
    if ((char)uVar3 == '\0') {
        return uVar3;
    }
    // StorageComponent::QueryCount(self) — current stored total (0x14012cf10).
    uVar3 = s_QueryCount ? s_QueryCount((void*)param_1) : 0;

    void* pvVar5 = (void*)0x0;          // matched RawMaterialDescription component
    int   pvVar7 = 0;                   // running index (Ghidra reused as ptr; it is an int idx)
    int64_t pvVar8 = 0;                 // byte cursor into the +0x28 ptr array
    if (0 < *(int*)(param_2 + 0x30)) {
        do {
            uint32_t dVar1 = *s_pTid_c7cb8;
            void* comp = *(void**)((int64_t)pvVar8 + *(int64_t*)(param_2 + 0x28));
            uint32_t tid = GetTypeId(comp);
            if (tid == dVar1) {
                void* pvVar7c = *(void**)(*(int64_t*)(param_2 + 0x28) + (int64_t)pvVar7 * 8);
                bool bVar2 = fn_IsDerivedFrom(pvVar7c, *s_pTid_c7cb8);
                if (bVar2) {
                    pvVar5 = pvVar7c;
                }
                break;
            }
            uint32_t uVar6 = pvVar7 + 1;
            pvVar7 = (int)uVar6;
            pvVar8 = pvVar8 + 8;
            if ((int)uVar6 >= *(int*)(param_2 + 0x30)) break;
        } while (true);
    }
    return (uint32_t)((int)(*(int*)((int64_t)pvVar5 + 0x48) * param_3 + (int)uVar3) <=
                      *(int*)(param_1 + 0x38) ? 1 : 0);
}

// ============================================================
// 0x14012d5d0 — Pf_FindBestAdjacentCell_d5d0(citizenAction, goalCell, outVec3, ctx) -> cost
// ============================================================
// Pick the best build/move cell. Two modes by whether the entity has a Position component:
//  (A) No Position (a placement target): expand a rectangle around the target footprint
//      (short fields at compRoot+0x28..+0x2e = x/y/w/h), and for each passable cell
//      (flag mask 0xe07) call ZoneRefresh (9c480) then validate zone/owner-match against the
//      goal cell (flag mask 0xe1f, zone short at +0x26), keep the min-distance (8830) cell.
//      Convert the winning cell to world coords (9f3e0) into outVec3.
//  (B) Has Position (a mobile entity): iterate its position list (123070 count, 122e20 each),
//      snap each to a cell, ValidateCell (a00f0), then IsDstReachable (9f810) for the cost
//      (falling back to euclidean if unreachable), keep the min, copy its world coord.
// Returns the cost, or -1.
//
// CLOSURE: calls 0x14009c480 (ZoneRefresh), now REGISTERED (reg_pf_roots_road) → satisfied.
//
// Faithfulness traps: param_2 (goal cell) is shadowed into uVar9/local_res8 as the running
// best; the inner `goto LAB_14012d8ce` is the for-loop continue. local_64/local_68 hold the
// world pos; DAT_140172114 = 0.5 cell-centre offset. mode (B)'s `uVar20` reachable-flag is
// passed unchanged into the recursive 12d5d0 in 8d590 — preserved here as the `param_4` ctx.
int Pf_FindBestAdjacentCell_d5d0(int64_t param_1, uint32_t param_2, float* param_3, int64_t param_4) {
    EnsureFns();
    int64_t* self = (int64_t*)(param_1 + 0x10);
    int* piVar3 = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int64_t local_70 = 0;
    // Two CONTIGUOUS Vector3 stack slots (the decompiler split each into 3 named floats,
    // but the engine writers fill all 3 via one base pointer — must be contiguous):
    //   posVec[0..2] = local_68/local_64/local_60 (also the FindComp IBP retbuf + GridToWorld in2)
    //   worldVec[0..2] = local_58/local_54/local_50 (GridToWorld out)
    float posVec[3] = {0, 0, 0};
    float worldVec[3] = {0, 0, 0};
    CheckPtr(self);
    fn_Entity_FindComponent_Position(*self, &local_70);
    int iVar15;
    if (local_70 == 0) {
        CheckPtr(self);
        // FindComp writes an 8-byte IBP into the start of posVec (matches the original stack
        // aliasing: the IBP slot overlaps local_68/local_64).
        void* self_00 = s_FindComp_4680(*self, posVec);
        CheckPtr(self_00);
        int64_t lVar10 = *(int64_t*)self_00;
        uint64_t uVar9 = 0xffffffff;
        int iVar5 = *(short*)(lVar10 + 0x2a) + -1;
        int iVar4 = *(short*)(lVar10 + 0x28) + -1;
        uint32_t uVar16 = (int)*(short*)(lVar10 + 0x2c) + 2;
        iVar15 = 0x7fffffff;
        int iVar6 = *(short*)(lVar10 + 0x2e) + 2 + iVar5;
        uint32_t local_res8 = 0xffffffff;
        // [M-D diag, env BANISHED_STORAGE_TRACE] mode-A rejection detail (capped).
        static int s_d5Trace = -1;
        if (s_d5Trace < 0) { const char* e = getenv("BANISHED_STORAGE_TRACE"); s_d5Trace = (e && e[0] && e[0] != '0') ? 40 : 0; }
        if (s_d5Trace > 0) { --s_d5Trace;
            fprintf(stderr, "[D5D0] mapComp=%p fp x=%d y=%d w=%d h=%d goal=%u\n",
                    (void*)lVar10, (int)*(short*)(lVar10 + 0x28), (int)*(short*)(lVar10 + 0x2a),
                    (int)*(short*)(lVar10 + 0x2c), (int)*(short*)(lVar10 + 0x2e), param_2);
            fflush(stderr);
        }
        if (iVar5 <= iVar6) {
            do {
                uint64_t uVar18 = (uint64_t)uVar16;
                if ((-1 < iVar5) && (iVar5 < *piVar3)) {
                    for (int iVar13 = iVar4; iVar13 < (int)(iVar4 + uVar16); iVar13 = iVar13 + 1) {
                        if ((-1 < iVar13) && (iVar13 < *piVar3)) {
                            uint32_t uVar14 = *piVar3 * iVar5 + iVar13;
                            int64_t lVar10b = (int64_t)(int)uVar14;
                            if ((1 << (*(uint8_t*)(*(int64_t*)(piVar3 + 0x20) + 0x29 + lVar10b * 0x30) & 0x1f) &
                                 0xe07U) != 0) {
                                s_ZoneRefresh(piVar3, uVar9, uVar18, (int)param_4);
                                param_4 = *(int64_t*)(piVar3 + 0x20);
                                uint32_t uVar17 = 1 << (*(uint8_t*)(param_4 + 0x29 + (int64_t)(int)param_2 * 0x30) & 0x1f);
                                uVar18 = (uint64_t)uVar17;
                                uint32_t uVar7 = 1 << (*(uint8_t*)(param_4 + 0x29 + lVar10b * 0x30) & 0x1f);
                                uint32_t uVar11 = uVar7 | uVar17;
                                uint32_t uVar12 = uVar11 | 0xe1f;
                                if (s_d5Trace > 0) { --s_d5Trace;
                                    fprintf(stderr, "[D5D0]  cell=%u pass zone=%d goalZone=%d flags=0x%x/0x%x\n",
                                            uVar14,
                                            (int)*(short*)(param_4 + 0x26 + lVar10b * 0x30),
                                            (int)*(short*)(param_4 + 0x26 + (int64_t)(int)param_2 * 0x30),
                                            uVar7, uVar17);
                                    fflush(stderr);
                                }
                                if ((((uVar12 & uVar7) != 0) && ((uVar12 & uVar17) != 0)) &&
                                    (((uVar11 & 0x1e0) != 0 ||
                                      (((*(short*)(param_4 + 0x26 + lVar10b * 0x30) != 0 &&
                                         (*(short*)(param_4 + 0x26 + (int64_t)(int)param_2 * 0x30) != 0)) &&
                                        (*(short*)(param_4 + 0x26 + lVar10b * 0x30) ==
                                         *(short*)(param_4 + 0x26 + (int64_t)(int)param_2 * 0x30))))))) {
                                    uVar18 = (uint64_t)param_2;
                                    int iVar8 = s_CalcDistance(piVar3, uVar14, param_2);
                                    if (s_d5Trace > 0) { --s_d5Trace;
                                        fprintf(stderr, "[D5D0]  cell=%u ZONE-OK dist=%d\n", uVar14, iVar8);
                                        fflush(stderr);
                                    }
                                    if (iVar8 < iVar15) {
                                        uVar9 = (uint64_t)uVar14;
                                        iVar15 = iVar8;
                                        local_res8 = uVar14;
                                        goto LAB_14012d8ce;
                                    }
                                }
                                uVar9 = (uint64_t)local_res8;
                            }
                        }
LAB_14012d8ce:;
                    }
                }
                iVar5 = iVar5 + 1;
            } while (iVar5 <= iVar6);
            iVar6 = (int)uVar9;
            if (-1 < iVar6) {
                posVec[1] = (float)(*piVar3 >> 1) - *s_pHalfCell_172114;
                posVec[0] = (float)(iVar6 % *piVar3) - posVec[1];
                posVec[1] = (float)(iVar6 / *piVar3) - posVec[1];
                posVec[2] = 0.0f;
                s_GridToWorld(piVar3, worldVec, posVec, '\0', (uint32_t*)0x0);
                *param_3 = worldVec[0];
                param_3[1] = worldVec[1];
                param_3[2] = worldVec[2];
                return iVar15;
            }
        }
        iVar15 = -1;
    } else {
        int iVar6 = 0x7fffffff;
        CheckPtr(&local_70);
        int iVar4 = s_ChildSum(local_70, 0);
        // [M-D diag, env BANISHED_STORAGE_TRACE] mode-B detail (capped).
        static int s_d5TraceB = -1;
        if (s_d5TraceB < 0) { const char* e = getenv("BANISHED_STORAGE_TRACE"); s_d5TraceB = (e && e[0] && e[0] != '0') ? 40 : 0; }
        if (s_d5TraceB > 0) { --s_d5TraceB;
            fprintf(stderr, "[D5D0B] posComp=%p pointCount=%d goal=%u\n",
                    reinterpret_cast<void*>(local_70), iVar4, param_2);
            fflush(stderr);
        }
        iVar15 = 0;
        if (0 < iVar4) {
            do {
                CheckPtr(&local_70);
                s_EntityResolvePos(local_70, posVec, 0, iVar15, (float*)0x0);
                int iVar13 = *piVar3;
                int iVar4b = (int)(posVec[0] + (float)(iVar13 >> 1));
                int iVar5 = (int)(posVec[1] + (float)(iVar13 >> 1));
                if (iVar4b < 0) iVar4b = 0;
                if (iVar13 <= iVar4b) iVar4b = iVar13 + -1;
                if (iVar5 < 0) iVar5 = 0;
                if (iVar13 <= iVar5) iVar5 = iVar13 + -1;
                uint32_t uVar16 = iVar13 * iVar5 + iVar4b;
                uint64_t uVar9 = s_ValidateCell(piVar3, 0xe1f, (uint64_t)param_2, uVar16);
                if (s_d5TraceB > 0) { --s_d5TraceB;
                    fprintf(stderr, "[D5D0B]  pt=%d cell=%u valid=%d\n", iVar15, uVar16, (int)(char)uVar9);
                    fflush(stderr);
                }
                if ((char)uVar9 != '\0') {
                    int iVar4c = Pf_IsDstReachable_f810(piVar3, param_2, uVar16, '\x01');
                    if (iVar4c < 0) {
                        int iVar4d = *piVar3;
                        int iVar5b = (int)param_2 / iVar4d - (int)uVar16 / iVar4d;
                        int iVar13b = (int)param_2 % iVar4d - (int)uVar16 % iVar4d;
                        float fVar19 = s_Sqrtf((float)(iVar5b * iVar5b + iVar13b * iVar13b));
                        iVar4c = (int)fVar19 + iVar4d;
                        if (iVar4c < 0) goto LAB_14012d744;
                    }
                    if (iVar4c < iVar6) {
                        *param_3 = posVec[0];
                        param_3[1] = posVec[1];
                        param_3[2] = posVec[2];
                        iVar6 = iVar4c;
                    }
                }
LAB_14012d744:
                iVar15 = iVar15 + 1;
                CheckPtr(&local_70);
                iVar4 = s_ChildSum(local_70, 0);
            } while (iVar15 < iVar4);
        }
        iVar15 = -1;
        if (iVar6 < 0x7fffffff) {
            iVar15 = iVar6;
        }
    }
    return iVar15;
}

// ============================================================
// 0x14008d590 — Pf_PickBestDropTarget_8d590(roots, outEntity, goalCell, srcComp/list, qty, useCap, outVec3, excludeIBP)
// ============================================================
// Scan two storage-entity lists (roots: `roots` and `roots+2`, each {ptr@0,count@+8}), and
// pick the best entity to drop/deliver to. For each candidate entity (snapshotted into a
// MemAlloc'd local array per list to be reentrancy-safe), it:
//   1. skips the exclude entity (param_8),
//   2. finds three components by type-id on the entity (comp-list +0x30 ptr-array / +0x38
//      count): DAT_1401c7ee8 (-> local_c0), DAT_1401c4560 (-> local_70), DWORD_1401c7e78
//      StorageComponent (-> local_a8). If local_c0 is set, a BuildComponent-ish narrow
//      (DAT_1401c7f20) supplies a flag (+0x20, or via inner residence comp +0x10 -> +0x3c)
//      that is AND-tested against the source comp's accept mask (pvVar19+0x5c); 0 -> skip.
//   3. capacity gate: if useCap(param_6)!=0 → StorageCanAccept (12cd80); else fast headroom
//      scan of local_a8's reservation pairs (+0x28 array stride 0x10) vs param_5.
//   4. compute the routing cost: anchor cell (87fb0) → IsDstReachable (9f810) for distance
//      (euclidean fallback). If it improves the best, compute the drop world-pos via either
//      FindBestAdjacentCell (12d5d0, when local_c0 set) or PickReachableCell (4edb0), and on
//      success record best entity (*param_2) + world pos (local_98) + cost.
// Finally writes the best world pos into param_7 and returns param_2.
//
// CLOSURE: satisfied — calls 0x14012d5d0 (this file, now closed) + 12cd80/9f810 (this file) +
// 87fb0/4edb0 (reg).
//
// Faithfulness traps: the per-list snapshot copy (MemFree(0) then MemAlloc + element copy
// using the `(plVar12 - local_b8) + plVar2` self-relative addressing) is transcribed exactly.
// The three find-by-type-id blocks and the residence-inner-comp block are repeated verbatim.
// local_58[1] = -2 is an SEH/stack cookie the decompiler surfaces (kept). uVar20 carries a
// volatile reachable flag forwarded into 12d5d0; preserved.
int64_t* Pf_PickBestDropTarget_8d590(int64_t** param_1, int64_t* param_2, uint32_t param_3,
                                     uint64_t param_4, int param_5, char param_6,
                                     uint32_t* param_7, void* param_8) {
    EnsureFns();
    int64_t local_58[3];
    local_58[1] = (int64_t)0xfffffffffffffffe;
    int* local_60 = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int iVar21 = 0x7fffffff;
    *param_2 = 0;
    uint64_t local_98 = 0;
    uint32_t local_90 = 0;
    int iVar17 = 0;
    uint64_t uVar20 = param_4;

    // initial scan of param_4's component list for the drop-source comp (pvVar19).
    void* pvVar19 = (void*)0x0;
    if (0 < *(int*)(param_4 + 0x30)) {
        int64_t lVar22 = 0;
        do {
            uint32_t dVar5 = *s_pTid_c7cb8;
            uint32_t dVar8 = GetTypeId(*(void**)(lVar22 + *(int64_t*)(param_4 + 0x28)));
            if (dVar8 == dVar5) {
                void* pvVar3 = *(void**)(*(int64_t*)(param_4 + 0x28) + (int64_t)iVar17 * 8);
                bool bVar7 = fn_IsDerivedFrom(pvVar3, *s_pTid_c7cb8);
                pvVar19 = bVar7 ? pvVar3 : (void*)0x0;
                goto LAB_14008d65b;
            }
            iVar17 = iVar17 + 1;
            lVar22 = lVar22 + 8;
        } while (iVar17 < *(int*)(param_4 + 0x30));
    }
    pvVar19 = (void*)0x0;
LAB_14008d65b:;
    int local_a0 = 0;
    do {
        int64_t** pplVar18 = param_1;
        if (local_a0 != 0) {
            pplVar18 = param_1 + 2;
        }
        int64_t* local_b8 = (int64_t*)0x0;
        uint64_t uVar16 = 0;
        if (&local_b8 != (int64_t**)pplVar18) {
            fn_MemFreeStatic((void*)0x0);
            local_b8 = (int64_t*)0x0;
            uint32_t uVar10 = *(uint32_t*)((int64_t)pplVar18 + 8);
            uVar16 = (uint64_t)uVar10;
            if (0 < (int)uVar10) {
                local_b8 = (int64_t*)fn_MemAlloc(g_pMemAllocSingleton, (int64_t)(int)uVar10 << 3, 8);
                if (0 < (int)uVar10) {
                    uint64_t uVar14 = (uint64_t)uVar10;
                    int64_t* plVar12 = local_b8;
                    do {
                        if (plVar12 != (int64_t*)0x0) {
                            int64_t* plVar2 = *pplVar18;
                            *plVar12 = 0;
                            *plVar12 = *(int64_t*)(((int64_t)plVar12 - (int64_t)local_b8) + (int64_t)plVar2);
                        }
                        plVar12 = plVar12 + 1;
                        uVar14 = uVar14 - 1;
                    } while (uVar14 != 0);
                }
            }
        }
        int64_t* plVar12 = local_b8;
        if (0 < (int)uVar16) {
            uint64_t local_68;
            do {
                local_68 = uVar16;
                int64_t local_c8 = *plVar12;
                CheckPtr(param_8);
                if (local_c8 != *(int64_t*)param_8) {
                    CheckPtr(&local_c8);
                    // --- find comp DAT_1401c7ee8 -> local_c0 ---
                    int64_t local_c0;
                    {
                        int64_t lVar22 = local_c8;
                        int iVar17b = 0;
                        int64_t lVar23 = 0;
                        void* pvVar13 = (void*)0x0;
                        if (0 < *(int*)(local_c8 + 0x38)) {
                            do {
                                uint64_t* puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                CheckPtr(puVar1);
                                uint32_t iVar11 = *s_pTid_c7ee8;
                                uint32_t iVar9 = GetTypeId((void*)*puVar1);
                                if (iVar9 == iVar11) {
                                    puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                    CheckPtr(puVar1);
                                    void* pvVar3 = (void*)*puVar1;
                                    pvVar13 = fn_IsDerivedFrom(pvVar3, *s_pTid_c7ee8) ? pvVar3 : (void*)0x0;
                                    goto FOUND_c0;
                                }
                                iVar17b = iVar17b + 1;
                                lVar23 = lVar23 + 1;
                            } while (iVar17b < *(int*)(lVar22 + 0x38));
                        }
                        pvVar13 = (void*)0x0;
FOUND_c0:
                        fn_InternalBasePtr_Ctor(&local_c0, pvVar13);
                    }
                    CheckPtr(&local_c8);
                    // --- find comp DAT_1401c4560 -> local_70 ---
                    int64_t local_70v;
                    {
                        int64_t lVar22 = local_c8;
                        int iVar17b = 0;
                        int64_t lVar23 = 0;
                        void* pvVar13 = (void*)0x0;
                        if (0 < *(int*)(local_c8 + 0x38)) {
                            do {
                                uint64_t* puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                CheckPtr(puVar1);
                                uint32_t iVar11 = *s_pTid_c4560;
                                uint32_t iVar9 = GetTypeId((void*)*puVar1);
                                if (iVar9 == iVar11) {
                                    puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                    CheckPtr(puVar1);
                                    void* pvVar3 = (void*)*puVar1;
                                    pvVar13 = fn_IsDerivedFrom(pvVar3, *s_pTid_c4560) ? pvVar3 : (void*)0x0;
                                    goto FOUND_70;
                                }
                                iVar17b = iVar17b + 1;
                                lVar23 = lVar23 + 1;
                            } while (iVar17b < *(int*)(lVar22 + 0x38));
                        }
                        pvVar13 = (void*)0x0;
FOUND_70:
                        fn_InternalBasePtr_Ctor(&local_70v, pvVar13);
                    }
                    CheckPtr(&local_c8);
                    // --- find comp DWORD_1401c7e78 (StorageComponent) -> local_a8 ---
                    int64_t local_a8;
                    {
                        int64_t lVar22 = local_c8;
                        int iVar17b = 0;
                        int64_t lVar23 = 0;
                        void* pvVar13 = (void*)0x0;
                        if (0 < *(int*)(local_c8 + 0x38)) {
                            do {
                                uint64_t* puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                CheckPtr(puVar1);
                                uint32_t dVar5 = *s_pTid_c7e78;
                                uint32_t dVar8 = GetTypeId((void*)*puVar1);
                                if (dVar8 == dVar5) {
                                    puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                    CheckPtr(puVar1);
                                    void* pvVar3 = (void*)*puVar1;
                                    pvVar13 = fn_IsDerivedFrom(pvVar3, *s_pTid_c7e78) ? pvVar3 : (void*)0x0;
                                    goto FOUND_a8;
                                }
                                iVar17b = iVar17b + 1;
                                lVar23 = lVar23 + 1;
                            } while (iVar17b < *(int*)(lVar22 + 0x38));
                        }
                        pvVar13 = (void*)0x0;
FOUND_a8:
                        fn_InternalBasePtr_Ctor(&local_a8, pvVar13);
                    }
                    if (local_c0 != 0) {
                        CheckPtr(&local_c0);
                        int64_t lVar22 = local_c0;
                        void* pvVar3 = *(void**)(local_c0 + 0x18);
                        void* pvVar13 = fn_IsDerivedFrom(pvVar3, *s_pTid_c7f20) ? pvVar3 : (void*)0x0;
                        uint32_t uVar10 = *(uint32_t*)((int64_t)pvVar13 + 0x20);
                        if (uVar10 == 0) {
                            CheckPtr((void*)(lVar22 + 0x10));
                            lVar22 = *(int64_t*)(lVar22 + 0x10);
                            int iVar17b = 0;
                            int64_t lVar23 = 0;
                            void* pvVar13b = (void*)0x0;
                            if (0 < *(int*)(lVar22 + 0x38)) {
                                do {
                                    uint64_t* puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                    CheckPtr(puVar1);
                                    uint32_t dVar5 = *s_pTid_c7e78;
                                    uint32_t dVar8 = GetTypeId((void*)*puVar1);
                                    if (dVar8 == dVar5) {
                                        puVar1 = (uint64_t*)(*(int64_t*)(lVar22 + 0x30) + lVar23 * 8);
                                        CheckPtr(puVar1);
                                        void* pvVar3b = (void*)*puVar1;
                                        pvVar13b = fn_IsDerivedFrom(pvVar3b, *s_pTid_c7e78) ? pvVar3b : (void*)0x0;
                                        goto FOUND_78;
                                    }
                                    iVar17b = iVar17b + 1;
                                    lVar23 = lVar23 + 1;
                                } while (iVar17b < *(int*)(lVar22 + 0x38));
                            }
                            pvVar13b = (void*)0x0;
FOUND_78:
                            int64_t local_78;
                            fn_InternalBasePtr_Ctor(&local_78, pvVar13b);
                            CheckPtr(&local_78);
                            uVar10 = *(uint32_t*)(local_78 + 0x3c);
                        }
                        if ((*(uint32_t*)((int64_t)pvVar19 + 0x5c) & uVar10) == 0) goto LAB_14008db2a;
                    }
                    CheckPtr(&local_a8);
                    bool accepted = false;
                    if (param_6 == '\0') {
                        int iVar17c = 0;
                        int64_t lVar22b = 0;
                        uint32_t uVar10 = *(uint32_t*)(local_a8 + 0x30);
                        if (0 < (int)uVar10) {
                            uint64_t* puVar4 = *(uint64_t**)(local_a8 + 0x28);
                            uint64_t* puVar15 = puVar4;
                            do {
                                if (*puVar15 == param_4) {
                                    if (-1 < lVar22b) {
                                        iVar17c = (int)puVar4[lVar22b * 2 + 1] -
                                                  *(int*)((int64_t)puVar4 + lVar22b * 0x10 + 0xc);
                                        goto AFTER_HEADROOM;
                                    }
                                    break;
                                }
                                iVar17c = iVar17c + 1;
                                lVar22b = lVar22b + 1;
                                puVar15 = puVar15 + 2;
                            } while (iVar17c < (int)uVar10);
                        }
                        iVar17c = 0;
AFTER_HEADROOM:
                        if (-1 < iVar17c - param_5) accepted = true;
                    } else {
                        uint32_t uVar10b = Pf_StorageCanAccept_cd80(local_a8, param_4, param_5);
                        if ((char)uVar10b != '\0') accepted = true;
                    }
                    if (accepted) {
                        local_58[0] = local_c8;
                        uint32_t uVar10 = s_AnchorCell((void*)local_58, (float*)0x0);
                        int* piVar6 = local_60;
                        uVar20 = ((uVar20 >> 8) << 8) | 1;  // CONCAT71(.., 1)
                        int iVar17d = Pf_IsDstReachable_f810(local_60, uVar10, param_3, '\x01');
                        if (iVar17d < 0) {
                            iVar17d = *piVar6;
                            int iVar11 = (int)uVar10 % iVar17d - (int)param_3 % iVar17d;
                            int iVar9 = (int)uVar10 / iVar17d - (int)param_3 / iVar17d;
                            float fVar24 = s_Sqrtf((float)(iVar9 * iVar9 + iVar11 * iVar11));
                            iVar17d = (int)fVar24 + iVar17d;
                        }
                        if (iVar17d < iVar21) {
                            // CONTIGUOUS Vector3 out slot (decompiler split it across
                            // local_88 qword [floats 0,1] + local_80 dword [float 2]; the
                            // writer fills 3 floats via one base pointer — must be contiguous).
                            float dropVec[3] = {0, 0, 0};
                            int iVar11;
                            if (local_c0 == 0) {
                                CheckPtr(&local_70v);
                                iVar11 = s_PickReachable(local_70v, param_3, dropVec);
                            } else {
                                CheckPtr(&local_c0);
                                iVar11 = Pf_FindBestAdjacentCell_d5d0(local_c0, param_3,
                                                                     dropVec, uVar20);
                            }
                            if ((-1 < iVar11) && (iVar11 < iVar21)) {
                                // local_98 = qword of {dropVec[0],dropVec[1]}; local_90 = dropVec[2].
                                uint32_t f0, f1;
                                __builtin_memcpy(&f0, &dropVec[0], 4);
                                __builtin_memcpy(&f1, &dropVec[1], 4);
                                local_98 = ((uint64_t)f1 << 32) | (uint64_t)f0;
                                __builtin_memcpy(&local_90, &dropVec[2], 4);
                                *param_2 = local_c8;
                                iVar21 = iVar17d;
                            }
                        }
                    }
                }
LAB_14008db2a:
                plVar12 = plVar12 + 1;
                uVar16 = local_68 - 1;
            } while (local_68 - 1 != 0);
        }
        fn_MemFreeStatic(local_b8);
        local_a0 = local_a0 + 1;
        if (1 < local_a0) {
            param_7[0] = (uint32_t)local_98;
            param_7[1] = (uint32_t)(local_98 >> 32);
            param_7[2] = local_90;
            return param_2;
        }
    } while (true);
}
