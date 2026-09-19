// code/Game/Component/citizenai_leisure.cpp
//
// Brick 3b handler wave (Leisure) — faithful 1:1 port of the deepest single
// CitizenAIComponent state handler plus its 3 private helpers:
//
//   0x14010a560  CitizenAI_State_Leisure           (state id 12, ~931 instr)
//   0x1400899d0  CitizenAI_State_Leisure_sub_99d0  (private; tavern/social find-target)
//   0x14002fd50  CitizenAI_State_Leisure_sub_fd50  (private; FindComponent scan, type 0x1c42c0)
//   0x14004a720  CitizenAI_helper_a720             (private; "is this Storage usable?" predicate)
//
// Leisure makes the citizen do something recreational: it picks a destination tile
// (an InteractComponent-bearing building, e.g. a tavern, or a random nearby walkable
// cell), walks the citizen there, and pushes a PlayAnimation action sequence. Returning
// 1 ("pushed") commits the state; the "no destination" tail returns 0 (low byte clear).
// Transcribed from the Ghidra decompile, offset-by-offset.
//
// SELF CHAIN: param_1 = CitizenAIComponent. self = param_1+0x38 (sibling CitizenComponent
//   IBP). citizen = *self. citizen+0x18 = CitizenDescription (IsDerivedFrom 0x1c7190).
//   citizen+0x238 = home/workplace entity. citizen+0x278 = CharacterComponent (+0x68 =
//   ModelComponent, whose transform +0x158/+0x15c/+0x160 = world pos). Success epilogue
//   writes the action-state flags to *self (+0x30 = -1, +0x54 |= 4, +0xb0 = 0) and
//   *(param_1+0xc0) = 0, then returns 1.
//
// GameWorld layout: GW+0xf8 = MapGrid (int*, [0]=width, +0x20=cell array base, cell
//   stride 0x30). GW+0xd8 = System::Random. GW+0xf0 = entity list pair (EntityList**).
//
// ALREADY-REGISTERED exe sub-callees (resolved via ResolveAddr; bodies NOT duplicated):
//   0x14009f3e0 CharacterComponent_impl_f3e0 (GridToWorld; writes Vector3, XMM1=2nd ret) [reg]
//   0x14001a8c0 Pf_AvlRebalanceA_a8c0       (AVL retrace; (root, node))               [reg]
//   0x140004680 FUN_140004680               (FindComponent MapComponent, type 0x1c79e0)[reg]
//   0x140014c30 FUN_140014c30               (recursive BST free; (root, node))         [reg]
//   0x14009c480 FUN_14009c480 / Pf_ZoneRefresh (zone refresh; (grid,?,?,int))          [reg]
//   0x1400a00f0 FUN_1400a00f0               (cell reachability test; (grid,mask,from,to))[reg]
//   0x1400ba6b0 PlayAnimation_Action_Ctor4  (push PlayAnimation; 5-arg)                [reg]
//   0x140002ed0 Entity_FindComponent_Position (FindComponent; (entity,outIBP))         [reg]
//   0x140122e20 Entity_impl_2e20            (resolve entity world pos -> Vector3)       [reg]
//   0x140087fb0 Entity_impl_7fb0            (entity anchor cell index; (ibp,outVec?))   [reg]
//   0x1401158d0 CitizenComponent_CheckWorkplaceValid ((citizen, &entIBP))              [reg]
//   0x140021a30 CitizenAI_impl_1a30 (AVL/BST insert; (tree,outNode,keyIBP))            [reg]*
//
//   * COLLISION NOTE: 0x140021a30 is registered under the (misleading) name
//     WorkBST_RegisterWork_1a30(uintptr_t*, void*, void*). Verified by decompile that the
//     function AT 0x140021a30 IS CitizenAI_impl_1a30 — a key-ordered AVL insert that
//     reads the key from *param_3 and calls Pf_AvlRebalanceA_a8c0. The 3-arg signature
//     (tree, outNode, key) matches Leisure's use exactly. We call it via ResolveAddr.
//
// SELF-CONTAINED-BRICK type-ids (UNFILLED 0xFFFFFFFF in standalone; resolved via
//   Object::GetTypeIdByName, scan-result derefs null-guarded). TypeNames recovered from
//   each global's *_Register[Type] DATA writer:
//     DWORD_1401c7190 -> "CitizenDescription"  (CitizenDescription_Register)
//     DAT_1401c42f8   -> "HappinessDescription"(HappinessDescription_Register)
//     DAT_1401c42c0   -> "HappinessComponent"  (HappinessComponent_RegisterType)
//     DWORD_1401c79e0 -> "MapComponent"        (MapComponent_RegisterType)
//     DAT_1401c7900   -> "InteractComponent"   (InteractComponent_RegisterType)
//
// .rdata float consts (real exe bit-patterns read via read_memory; hardcoded — pure
// scalars, no indirection):
//     DAT_140172100 = 0.25  (RandFloat threshold: chance to clear the candidate cell)
//     DAT_140172114 = 0.5   (cell-centre bias subtracted from grid -> world)
//     DAT_14017210c = 0.33  (random animation-offset disk radius scale)
//     DAT_1401721b0 = 10.0  (animation-time base, added to RandFloatTol*ageMult)
//     g_pAgeMultiplier (0x140172198) = 5.0 (animation-time age multiplier)
//


#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

namespace {

// --- registered exe sub-callees ---
typedef float*   (*fn_gridToWorld_t)(void* grid, float* out3, float* inXY2, char flag, void* outTri); // 0x14009f3e0
typedef void     (*fn_avl_a8c0_t)(int64_t* root, int64_t* node);                     // 0x14001a8c0
typedef void*    (*fn_findMap_t)(int64_t entity, void* outIBP);                      // 0x140004680
typedef void     (*fn_bstFree_t)(void* root, void* node);                            // 0x140014c30
typedef void     (*fn_zoneRefresh_t)(int* grid, void* p2, void* p3, int p4);         // 0x14009c480
typedef uint8_t  (*fn_reach_t)(int* grid, uint32_t mask, uint64_t from, uint32_t to);// 0x1400a00f0
typedef void     (*fn_playAnim_t)(void* holder, int64_t* entityIBP, uint32_t* dest, float time, uint32_t flag); // 0x1400ba6b0
typedef void*    (*fn_findPos_t)(int64_t entity, void* outIBP);                      // 0x140002ed0
typedef void     (*fn_2e20_t)(int64_t comp, void* outVec3, int, int, void*);         // 0x140122e20
typedef uint32_t (*fn_7fb0_t)(void* ibp, float* outVec3orNull);                      // 0x140087fb0
typedef char     (*fn_checkWork_t)(void* citizen, void* arg);                        // 0x1401158d0
typedef void*    (*fn_bstInsert_t)(int64_t* tree, void* outNode, void* keyIBP);      // 0x140021a30

static fn_gridToWorld_t s_gridToWorld = nullptr;
static fn_avl_a8c0_t    s_avl         = nullptr;
static fn_findMap_t     s_findMap     = nullptr;
static fn_bstFree_t     s_bstFree     = nullptr;
static fn_zoneRefresh_t s_zoneRefresh = nullptr;
static fn_reach_t       s_reach       = nullptr;
static fn_playAnim_t    s_playAnim    = nullptr;
static fn_findPos_t     s_findPos     = nullptr;
static fn_2e20_t        s_2e20        = nullptr;
static fn_7fb0_t        s_7fb0        = nullptr;
static fn_checkWork_t   s_checkWork   = nullptr;
static fn_bstInsert_t   s_bstInsert   = nullptr;

// AVL/BST node allocator (System::MemoryAllocator::MemAlloc(0x28,8)) and free wrapper.
static inline void* NodeAlloc(uint64_t sz) {
    return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, sz, 8) : nullptr;
}

static uint32_t s_tid_CitizenDescription   = 0;  // DWORD_1401c7190
static uint32_t s_tid_HappinessDescription  = 0;  // DAT_1401c42f8
static uint32_t s_tid_HappinessComponent    = 0;  // DAT_1401c42c0
static uint32_t s_tid_MapComponent          = 0;  // DWORD_1401c79e0
static uint32_t s_tid_InteractComponent     = 0;  // DAT_1401c7900

// .rdata float consts (real exe bit-patterns).
static const float kRandThresh   = 0.25f;  // DAT_140172100
static const float kCellBias     = 0.5f;   // DAT_140172114
static const float kDiskScale    = 0.33f;  // DAT_14017210c
static const float kAnimTimeBase = 10.0f;  // DAT_1401721b0
static const float kAgeMult      = 5.0f;   // g_pAgeMultiplier (0x140172198)

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_gridToWorld = reinterpret_cast<fn_gridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_avl         = reinterpret_cast<fn_avl_a8c0_t>(ResolveAddr(Module::Exe, 0x14001a8c0));
    s_findMap     = reinterpret_cast<fn_findMap_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_bstFree     = reinterpret_cast<fn_bstFree_t>(ResolveAddr(Module::Exe, 0x140014c30));
    s_zoneRefresh = reinterpret_cast<fn_zoneRefresh_t>(ResolveAddr(Module::Exe, 0x14009c480));
    s_reach       = reinterpret_cast<fn_reach_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_playAnim    = reinterpret_cast<fn_playAnim_t>(ResolveAddr(Module::Exe, 0x1400ba6b0));
    s_findPos     = reinterpret_cast<fn_findPos_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_2e20        = reinterpret_cast<fn_2e20_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_7fb0        = reinterpret_cast<fn_7fb0_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_checkWork   = reinterpret_cast<fn_checkWork_t>(ResolveAddr(Module::Exe, 0x1401158d0));
    s_bstInsert   = reinterpret_cast<fn_bstInsert_t>(ResolveAddr(Module::Exe, 0x140021a30));
    if (s_tid_CitizenDescription == 0)   s_tid_CitizenDescription   = ::Object::GetTypeIdByName("CitizenDescription");
    if (s_tid_HappinessDescription == 0) s_tid_HappinessDescription = ::Object::GetTypeIdByName("HappinessDescription");
    if (s_tid_HappinessComponent == 0)   s_tid_HappinessComponent   = ::Object::GetTypeIdByName("HappinessComponent");
    if (s_tid_MapComponent == 0)         s_tid_MapComponent         = ::Object::GetTypeIdByName("MapComponent");
    if (s_tid_InteractComponent == 0)    s_tid_InteractComponent    = ::Object::GetTypeIdByName("InteractComponent");
    s_init = true;
}

static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }
static inline int* MapGrid() { return *reinterpret_cast<int**>(GW() + 0xf8); }
static inline void* Rng() { return *reinterpret_cast<void**>(GW() + 0xd8); }
// Cell-array base lives at MapGrid+0x80 (Ghidra prints `piVar5 + 0x20` for int* piVar5;
// disasm confirms MOV RCX,[R15+0x80]). Width is at MapGrid[0] (+0x00); stride 0x30.
static inline int64_t CellArrayBase(int* mapGrid) {
    return *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(mapGrid) + 0x80);
}

// Virtual GetTypeId: original is `(**(code**)(**slot + 0x10))()` where slot is &entry,
// *slot is the object, **slot is its vtable, vtable+0x10 (index 2) is GetTypeId, called
// with the object as `this`. Returns the runtime type id of the object at *slot.
typedef uint32_t (*fn_getTypeId_t)(void* self);
static inline uint32_t VGetTypeId(int64_t* slot) {
    void* obj = *reinterpret_cast<void**>(slot);
    if (obj == nullptr) return 0xffffffff;
    void** vt = *reinterpret_cast<void***>(obj);
    fn_getTypeId_t f = reinterpret_cast<fn_getTypeId_t>(vt[2]);  // +0x10
    return f(obj);
}

// Scan a component array [base @+arrOff, count @+cntOff of `node`] for the first entry
// whose runtime type id == wantTid; returns the IsDerivedFrom-narrowed object or null.
// Mirrors the inline scan loops in the original.
static void* ScanArrAt(int64_t* node, int arrOff, int cntOff, uint32_t wantTid) {
    if (node == nullptr) return nullptr;
    int count = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(node) + cntOff);
    uint64_t arr = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(node) + arrOff);
    for (int i = 0; i < count; ++i) {
        int64_t* slot = reinterpret_cast<int64_t*>(arr + static_cast<uint64_t>(i) * 8);
        if (VGetTypeId(slot) == wantTid) {
            int64_t* slot2 = reinterpret_cast<int64_t*>(arr + static_cast<uint64_t>(i) * 8);
            void* obj = *reinterpret_cast<void**>(slot2);
            bool ok = fn_IsDerivedFrom && fn_IsDerivedFrom(obj, wantTid);
            return ok ? obj : nullptr;
        }
    }
    return nullptr;
}

// node[6]/[7] scan (the bottom InteractComponent/Happiness/Map bound scans).
static inline void* ScanArr(int64_t* node, uint32_t wantTid) {
    return ScanArrAt(node, 0x30, 0x38, wantTid);
}
// node[5]'s Object::Type array scan (LAB_14010abbc validated-target radius lookup).
//   node[5] = Object::Type; its array @+0x28, count @+0x30.
static void* ScanType5(int64_t* node, uint32_t wantTid) {
    if (node == nullptr) return nullptr;
    uint64_t type = static_cast<uint64_t>(node[5]);
    if (type == 0) return nullptr;
    return ScanArrAt(reinterpret_cast<int64_t*>(type), 0x28, 0x30, wantTid);
}

} // namespace

// ============================================================
// 0x14004a720 — CitizenAI_helper_a720
// "Is this entity's primary component (+0x18) a usable Storage?" — its state field
// (+0x20) must not be 5 (closed/destroyed) and its +0x28 flag must be set.
// ============================================================
static char CitizenAI_helper_a720(int64_t param_1) {
    void* obj = *reinterpret_cast<void**>(param_1 + 0x18);
    void* narrowed = (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, s_tid_HappinessDescription)) ? obj : nullptr;
    if ((narrowed == nullptr || *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(narrowed) + 0x20) != 5)
        && *reinterpret_cast<char*>(param_1 + 0x28) != '\0') {
        // NOTE: original derefs narrowed+0x20 even when narrowed==0 (reads 0x20). Guarded
        // here: a null narrowed cannot be state-5, so treat as "usable" by the flag alone.
        if (narrowed != nullptr && *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(narrowed) + 0x20) == 5)
            return 0;
        return 1;
    }
    return 0;
}

// ============================================================
// 0x14002fd50 — CitizenAI_State_Leisure_sub_fd50
// FindComponent scan: walk the Object::Type array at (param_1+0x30) [count @ +0x38],
// find the first child whose type id == HappinessComponent (DAT_1401c42c0), wrap the
// IsDerivedFrom-narrowed result in an InternalBasePtr at param_2, return param_2.
// ============================================================
static int64_t* CitizenAI_State_Leisure_sub_fd50(int64_t param_1, int64_t* param_2) {
    void* result = nullptr;
    int count = *reinterpret_cast<int*>(param_1 + 0x38);
    uint64_t arrBase = *reinterpret_cast<uint64_t*>(param_1 + 0x30);
    int idx = 0;
    int64_t byteOff = 0;
    while (idx < count) {
        int64_t* slot = reinterpret_cast<int64_t*>(arrBase + byteOff);
        if (VGetTypeId(slot) == s_tid_HappinessComponent) {
            int64_t* slot2 = reinterpret_cast<int64_t*>(arrBase + static_cast<int64_t>(idx) * 8);
            void* obj = *reinterpret_cast<void**>(slot2);
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, s_tid_HappinessComponent))
                result = obj;
            break;
        }
        ++idx;
        byteOff += 8;
    }
    if (fn_InternalBasePtr_Ctor) new (param_2) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(result));   // B.3.3 bridge
    return param_2;
}

// ============================================================
// 0x1400899d0 — CitizenAI_State_Leisure_sub_99d0
// Social/tavern find-target. Iterates a candidate entity list at (param_1+0x50)
// [count @ +0x58]; for each, finds its InteractComponent (DAT_1401c7900), resolves its
// world pos -> grid cell, queries BuildStructure_impl_f810 (NOT used here — see note),
// and accumulates the reachable candidates into a grown array, then picks one at random.
//
// NOTE: the original calls BuildStructure_impl_f810 (0x14009f810), FUN_140132010 (sqrtf,
// 0x140132010) and FUN_1400a00f0 — all REGISTERED. f810 is resolved here directly; the
// brief stated f810/2e20/a00f0/132010 are reg + closure-safe.
//
// Writes the chosen entity's wrapped pointer (its +0x10 IBP payload) into *param_2, or a
// null InternalBasePtr if no candidate qualifies. Returns param_2.
//   param_3 = source grid cell (citizen pos), param_4 = max distance (in cells, *2 budget)
// ============================================================
typedef int      (*fn_buildStruct_f810_t)(int* grid, uint32_t fromCell, uint32_t toCell, char flag); // 0x14009f810
typedef float    (*fn_sqrtf_132010_t)(float v);                                                       // 0x140132010
static fn_buildStruct_f810_t s_f810  = nullptr;
static fn_sqrtf_132010_t     s_sqrtf = nullptr;

static int64_t* CitizenAI_State_Leisure_sub_99d0(int64_t param_1, int64_t* param_2,
                                                 uint32_t param_3, int param_4) {
    if (s_f810 == nullptr)  s_f810  = reinterpret_cast<fn_buildStruct_f810_t>(ResolveAddr(Module::Exe, 0x14009f810));
    if (s_sqrtf == nullptr) s_sqrtf = reinterpret_cast<fn_sqrtf_132010_t>(ResolveAddr(Module::Exe, 0x140132010));

    int* mapGrid = MapGrid();
    uint32_t outCap = 0;          // (local_50 high dword)
    int matchCount = 0;           // pvVar18 / pvVar19 (entries written)
    void* curArr = nullptr;       // pvVar21 / pvVar6 (current array ptr)

    int listCount = *reinterpret_cast<int*>(param_1 + 0x58);
    if (listCount > 0) {
        int idx = 0;
        int64_t byteOff = 0;
        do {
            int64_t* slot = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x50) + byteOff);
            int64_t entObj = *reinterpret_cast<int64_t*>(*slot + 0x10);

            // find the entity's InteractComponent (scan Object::Type array @ entObj+0x30)
            void* interact = nullptr;
            interact = ScanArrAt(reinterpret_cast<int64_t*>(entObj), 0x30, 0x38, s_tid_InteractComponent);
            int64_t interactIBP[2] = {0, 0};
            if (fn_InternalBasePtr_Ctor) new (interactIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(interact));   // B.3.3 bridge

            // resolve world pos -> clamped grid cell
            float posVec[4] = {0, 0, 0, 0};
            if (s_2e20) s_2e20(interactIBP[0], posVec, 0, 0, nullptr);
            int w = *mapGrid;
            int col = static_cast<int>(posVec[0] + static_cast<float>(w >> 1));
            int row = static_cast<int>(posVec[1] + static_cast<float>(w >> 1));
            if (col < 0) col = 0; if (w <= col) col = w - 1;
            if (row < 0) row = 0; if (w <= row) row = w - 1;
            uint32_t destCell = static_cast<uint32_t>(w * row + col);

            // structure-path cost (or manhattan-ish euclidean fallback if unreachable)
            int cost = s_f810 ? s_f810(mapGrid, param_3, destCell, '\x01') : -1;
            if (cost < 0) {
                int wgrid = *mapGrid;
                int dx = static_cast<int>(param_3) % wgrid - static_cast<int>(destCell) % wgrid;
                int dy = static_cast<int>(param_3) / wgrid - static_cast<int>(destCell) / wgrid;
                float d = s_sqrtf ? s_sqrtf(static_cast<float>(dy * dy + dx * dx)) : 0.0f;
                cost = static_cast<int>(d) + wgrid;
            }

            if (cost < param_4 * 2 &&
                s_reach && (static_cast<char>(s_reach(mapGrid, 0xe1f, param_3, destCell)) != '\0')) {
                int64_t* slot2 = reinterpret_cast<int64_t*>(byteOff + *reinterpret_cast<int64_t*>(param_1 + 0x50));
                int64_t node = *slot2;
                void* prim = *reinterpret_cast<void**>(node + 0x18);
                void* narrowed = (fn_IsDerivedFrom && fn_IsDerivedFrom(prim, s_tid_HappinessDescription)) ? prim : nullptr;
                if ((narrowed == nullptr || *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(narrowed) + 0x20) != 5)
                    && *reinterpret_cast<char*>(node + 0x28) != '\0') {
                    int64_t listBase = *reinterpret_cast<int64_t*>(param_1 + 0x50);
                    uint32_t oldN = static_cast<uint32_t>(matchCount);
                    uint32_t newN = oldN + 1;
                    // grow array geometrically when needed
                    if (static_cast<int>(outCap) < static_cast<int>(newN)) {
                        uint32_t grow = (oldN != 0) ? newN * 3 : newN;
                        outCap = grow;
                        void* fresh = NodeAlloc(static_cast<uint64_t>(static_cast<int64_t>(static_cast<int>(grow)) << 3));
                        // copy existing entries into the fresh array
                        if (fresh) {
                            int copy = (curArr == fresh) ? matchCount : 0;
                            for (int k = copy; k < static_cast<int>(newN); ++k) {
                                int64_t* dst = reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(fresh) + static_cast<int64_t>(k) * 8);
                                int64_t* srcOld = reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(curArr) + static_cast<int64_t>(k) * 8);
                                int64_t* srcList = reinterpret_cast<int64_t*>(listBase + static_cast<int64_t>(idx) * 8);
                                int64_t* src = (k < static_cast<int>(oldN)) ? srcOld : srcList;
                                if (dst) { *dst = 0; *dst = *src; }
                            }
                        }
                        if (curArr != fresh && curArr != nullptr && fn_MemFreeStatic) fn_MemFreeStatic(curArr);
                        curArr = fresh;
                    } else {
                        // in-place append of the current entity into curArr[oldN]
                        int64_t* dst = reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(curArr) + static_cast<int64_t>(static_cast<int>(oldN)) * 8);
                        int64_t* srcList = reinterpret_cast<int64_t*>(listBase + static_cast<int64_t>(idx) * 8);
                        if (dst) { *dst = 0; *dst = *srcList; }
                    }
                    matchCount = static_cast<int>(newN);
                }
            }

            ++idx;
            byteOff += 8;
        } while (idx < listCount);

        if (matchCount != 0) {
            uint32_t pick = reinterpret_cast<System::Random*>(Rng())->Rand(static_cast<uint32_t>(matchCount));   // B.3.4b bridge
            int64_t* chosen = reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(curArr) + static_cast<uint64_t>(pick) * 8);
            int64_t node = *chosen;
            *param_2 = 0;
            *param_2 = *reinterpret_cast<int64_t*>(node + 0x10);
            if (curArr && fn_MemFreeStatic) fn_MemFreeStatic(curArr);
            return param_2;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (param_2) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    if (curArr && fn_MemFreeStatic) fn_MemFreeStatic(curArr);
    return param_2;
}

// ============================================================
// 0x14010a560 — CitizenAI_State_Leisure  (state id 12)
// Faithful goto-structured transliteration of the ~931-instruction original.
// Local-name map to the Ghidra decompile:
//   self=local_90 (param_1+0x38), pvVar26=CitizenDescription/scan-result, piVar5=mapGrid,
//   pRVar6/local_88=rng, uVar37=srcCell/local_res10, uVar14=homeCell, local_res20=target,
//   iVar27=blockRadius, local_a0/9c/98=animDest, local_d8/d0=homeBst/count,
//   local_c8/c0=cellBst/count, happinessMode=local_res10 low byte.
// ============================================================
extern "C" uint64_t CitizenAI_State_Leisure(int64_t param_1) {
    EnsureInit();

    int* piVar5 = MapGrid();
    void* pRVar6 = Rng();

    int64_t* self = reinterpret_cast<int64_t*>(param_1 + 0x38);   // local_90

    // pvVar26 = CitizenDescription on the citizen (citizen+0x18, IsDerivedFrom 0x1c7190).
    void* descRaw = *reinterpret_cast<void**>(*self + 0x18);
    void* pvVar26 = (fn_IsDerivedFrom && fn_IsDerivedFrom(descRaw, s_tid_CitizenDescription)) ? descRaw : nullptr;

    // citizen ModelComponent transform -> grid cell (uVar37=srcCell).
    uintptr_t citizen = static_cast<uintptr_t>(*self);
    int64_t character = *reinterpret_cast<int64_t*>(citizen + 0x278);
    int64_t model = *reinterpret_cast<int64_t*>(character + 0x68);
    int w0 = *piVar5;
    int colC = static_cast<int>(static_cast<float>(w0 >> 1) + *reinterpret_cast<float*>(model + 0x158));
    int rowC = static_cast<int>(static_cast<float>(w0 >> 1) + *reinterpret_cast<float*>(model + 0x15c));
    if (colC < 0) colC = 0; if (w0 <= colC) colC = w0 - 1;
    if (rowC < 0) rowC = 0; if (w0 <= rowC) rowC = w0 - 1;
    uint32_t uVar37 = static_cast<uint32_t>(w0 * rowC + colC);    // srcCell / local_res10

    // homeCell (uVar14) from workplace anchor (+0x238) or -1.
    uint32_t uVar14;
    if (*reinterpret_cast<int64_t*>(*self + 0x238) == 0) {
        uVar14 = 0xffffffff;
    } else {
        int64_t homeIBP = *reinterpret_cast<int64_t*>(*self + 0x238);
        uVar14 = s_7fb0 ? s_7fb0(&homeIBP, nullptr) : 0xffffffff;
    }

    int64_t local_a8 = static_cast<int64_t>(static_cast<int>(uVar37)) * 0x30;   // srcCell byte off
    int64_t* local_res20 = *reinterpret_cast<int64_t**>(local_a8 + CellArrayBase(piVar5));  // target node

    {
        float fVar39 = reinterpret_cast<System::Random*>(pRVar6)->RandFloat();   // B.3.4b bridge
        if (fVar39 < kRandThresh) {
            int64_t nullIBP = 0;
            if (fn_InternalBasePtr_Ctor) new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            local_res20 = reinterpret_cast<int64_t*>(nullIBP);
        }
    }

    int iVar27 = 0;             // blockRadius (set per path before LAB_14010ae8a)
    int iVar30 = 0, iVar13 = 0, iVar24 = 0; // box origin col, row, height
    int local_res18_w = 0;      // box width (low dword of local_res18)
    float local_a0 = 0, local_9c = 0, local_98 = 0;  // animDest vec
    bool happinessMode = false; // local_res10 low byte
    int64_t* local_d8 = nullptr; uint32_t local_d0 = 0;   // home-box BST root/count

    if (local_res20 == nullptr) goto LAB_14010a7e0;

    // ---- validated-target path: target must have a usable Storage + be reachable ----
    {
        int64_t happyIBP = 0;
        CitizenAI_State_Leisure_sub_fd50(reinterpret_cast<int64_t>(local_res20), &happyIBP);
        if (happyIBP == 0) goto LAB_14010a7e0;

        int64_t happyIBP2 = 0;
        int64_t* pIVar17 = CitizenAI_State_Leisure_sub_fd50(reinterpret_cast<int64_t>(local_res20), &happyIBP2);
        char cVar12 = CitizenAI_helper_a720(*pIVar17);
        if (cVar12 == '\0') goto LAB_14010a7e0;

        int64_t posIBP = 0;
        int64_t* pp = s_findPos ? reinterpret_cast<int64_t*>(s_findPos(reinterpret_cast<int64_t>(local_res20), &posIBP)) : nullptr;
        float dpos[4] = {0, 0, 0, 0};
        if (pp && s_2e20) s_2e20(*pp, dpos, 0, 0, nullptr);
        int w = *piVar5;
        int tcol = static_cast<int>(static_cast<float>(w >> 1) + dpos[0]);
        int trow = static_cast<int>(static_cast<float>(w >> 1) + dpos[1]);
        if (tcol < 0) tcol = 0; if (w <= tcol) tcol = w - 1;
        if (trow < 0) trow = 0; if (w <= trow) trow = w - 1;
        uint32_t uVar35 = static_cast<uint32_t>(w * trow + tcol);
        uint8_t reach = s_reach ? s_reach(piVar5, 0xe1f, uVar37, uVar35) : 0;
        if (static_cast<char>(reach) == '\0') goto LAB_14010a7e0;
        goto LAB_14010a83d;   // target validated
    }

LAB_14010a7e0:
    // social find-target (sub_99d0) at the CitizenDescription leisure radius (pvVar26+0xbc),
    // first from srcCell then homeCell.
    {
        uint32_t radius = pvVar26 ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(pvVar26) + 0xbc) : 0;
        int64_t findBuf = 0;
        int64_t* pr = CitizenAI_State_Leisure_sub_99d0(*reinterpret_cast<int64_t*>(GW() + 0xf0),
                                                       &findBuf, uVar37, static_cast<int>(radius));
        local_res20 = reinterpret_cast<int64_t*>(*pr);
        if (local_res20 == nullptr) {
            if (static_cast<int>(uVar14) >= 0) {
                uint32_t radius2 = pvVar26 ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(pvVar26) + 0xbc) : 0;
                int64_t findBuf2 = 0;
                int64_t* pr2 = CitizenAI_State_Leisure_sub_99d0(*reinterpret_cast<int64_t*>(GW() + 0xf0),
                                                                &findBuf2, uVar14, static_cast<int>(radius2));
                local_res20 = reinterpret_cast<int64_t*>(*pr2);
                goto LAB_14010a83d;
            }
            goto LAB_14010a846;
        }
    }

LAB_14010a83d:
    if (local_res20 != nullptr) goto LAB_14010abbc;

LAB_14010a846:
    // home-box BST: insert reachable, work-valid cells in a 13x13 box around srcCell, then
    // (rand%count)-th in-order becomes local_res20. iVar27 ends = 6 for the bloom.
    {
        local_d8 = nullptr; local_d0 = 0;
        int baseCol = static_cast<int>(uVar37) % *piVar5;
        int boxRow = static_cast<int>(uVar37) / *piVar5 - 6;
        for (int rows = 0xd; rows != 0; --rows) {
            for (int dxo = -6; dxo < 7; ++dxo) {
                int cx = dxo + baseCol;
                int w = *piVar5;
                if (cx >= 0 && cx < w && boxRow >= 0 && boxRow < w) {
                    int64_t* plVar20 = reinterpret_cast<int64_t*>(
                        static_cast<int64_t>(w * boxRow + dxo + baseCol) * 0x30 + CellArrayBase(piVar5));
                    int64_t cand = 0;     // local_res18
                    if (plVar20[0] == 0) {
                        if (plVar20[1] == 0) { if (fn_InternalBasePtr_Ctor) new (&cand) ::Object::InternalBasePtr(
                            nullptr); }   // B.3.3 bridge
                        else cand = plVar20[1];
                    } else cand = plVar20[0];
                    if (cand != 0) {
                        int64_t mapBuf = 0;     // local_b0
                        if (s_findMap) s_findMap(cand, &mapBuf);
                        if (mapBuf != 0) {
                            int64_t workArg = cand;   // local_b8 = cand
                            char ok = s_checkWork ? s_checkWork(reinterpret_cast<void*>(*self), &workArg) : 0;
                            if (ok != '\0') {
                                int64_t outNode = 0;   // &local_a0 (out-node, result unused)
                                if (s_bstInsert) { s_bstInsert(reinterpret_cast<int64_t*>(&local_d8), &outNode, &cand);
                                                   local_d0 = local_d0 + 1; }
                            }
                        }
                    }
                }
            }
            ++boxRow;
        }
        // also consider the citizen's own workplace (+0x238) if valid (manual key-ordered insert).
        uVar37 = static_cast<uint32_t>(w0 * rowC + colC);   // uVar37 = (uint)local_res10 = srcCell
        iVar27 = 6;
        if (*reinterpret_cast<int64_t*>(*self + 0x238) != 0) {
            int64_t key = *reinterpret_cast<int64_t*>(*self + 0x238);   // local_res10
            char ok = s_checkWork ? s_checkWork(reinterpret_cast<void*>(*self), &key) : 0;
            if (ok != '\0') {
                key = *reinterpret_cast<int64_t*>(*self + 0x238);
                if (local_d8 == nullptr) {
                    int64_t* nd = reinterpret_cast<int64_t*>(NodeAlloc(0x28));
                    if (nd) { nd[0] = key; nd[1] = 0; nd[2] = 0; nd[3] = 0; *reinterpret_cast<int*>(nd + 4) = 0; }
                    local_d0 = local_d0 + 1;
                    local_d8 = nd;
                } else {
                    uint64_t keyVal = static_cast<uint64_t>(key);   // compared as ulonglong (pointer key)
                    int64_t* cur = local_d8; int64_t* ins = cur; bool done = false;
                    do {
                        ins = cur;
                        if (static_cast<uint64_t>(cur[0]) <= keyVal) {
                            if (keyVal <= static_cast<uint64_t>(cur[0])) { done = true; break; }  // present
                            cur = reinterpret_cast<int64_t*>(cur[3]);
                            if (cur == nullptr) {
                                int64_t* nd = reinterpret_cast<int64_t*>(NodeAlloc(0x28));
                                if (nd) { nd[0] = static_cast<int64_t>(keyVal); nd[1] = reinterpret_cast<int64_t>(ins); nd[2] = 0; nd[3] = 0; *reinterpret_cast<int*>(nd + 4) = 0; }
                                ins[3] = reinterpret_cast<int64_t>(nd);
                                if (s_avl) s_avl(reinterpret_cast<int64_t*>(&local_d8), reinterpret_cast<int64_t*>(&ins));
                                local_d0 = local_d0 + 1; done = true; break;
                            }
                        } else {
                            cur = reinterpret_cast<int64_t*>(cur[2]);
                            if (cur == nullptr) {
                                int64_t* nd = reinterpret_cast<int64_t*>(NodeAlloc(0x28));
                                if (nd) { nd[0] = static_cast<int64_t>(keyVal); nd[1] = reinterpret_cast<int64_t>(ins); nd[2] = 0; nd[3] = 0; *reinterpret_cast<int*>(nd + 4) = 0; }
                                ins[2] = reinterpret_cast<int64_t>(nd);
                                if (s_avl) s_avl(reinterpret_cast<int64_t*>(&local_d8), reinterpret_cast<int64_t*>(&ins));
                                local_d0 = local_d0 + 1; done = true; break;
                            }
                        }
                    } while (!done);
                }
            }
        }
        // pick (rand%count)-th in-order node; its key (+0x00) is local_res20.
        if (static_cast<int>(local_d0) > 0) {
            uint32_t pick = reinterpret_cast<System::Random*>(pRVar6)->Rand(static_cast<uint32_t>(local_d0));   // B.3.4b bridge
            int64_t* node = local_d8; int64_t* chosen = local_d8;
            while (node != nullptr) { chosen = node; node = reinterpret_cast<int64_t*>(node[2]); }
            for (uint32_t s = pick; s > 0; --s) {
                if (chosen != nullptr) {
                    int64_t* right = reinterpret_cast<int64_t*>(chosen[3]);
                    if (right == nullptr) {
                        int64_t* parent = reinterpret_cast<int64_t*>(chosen[1]);
                        if (parent != nullptr && reinterpret_cast<int64_t*>(parent[2]) != chosen) {
                            while (parent != nullptr && reinterpret_cast<int64_t*>(parent[3]) == chosen) {
                                chosen = parent; parent = reinterpret_cast<int64_t*>(parent[1]);
                            }
                        }
                        chosen = reinterpret_cast<int64_t*>(chosen[1]);
                    } else {
                        do { chosen = right; right = reinterpret_cast<int64_t*>(chosen[2]); } while (right != nullptr);
                    }
                }
            }
            local_res20 = (chosen != nullptr) ? reinterpret_cast<int64_t*>(*chosen) : nullptr;
        }
        if (local_d8 != nullptr) {
            if (s_bstFree) { s_bstFree(&local_d8, reinterpret_cast<void*>(local_d8[2]));
                             s_bstFree(&local_d8, reinterpret_cast<void*>(local_d8[3])); }
            if (fn_MemFreeStatic) fn_MemFreeStatic(local_d8);
        }
        local_d8 = nullptr; local_d0 = 0;
        goto LAB_after_target;   // local_res20 set (or still null) -> bound decision
    }

LAB_14010abbc:
    // validated/found target: scan its node[5] Object::Type array for HappinessDescription
    // (0x1c42f8); iVar27 = that InteractComponent's radius (+0x2c).
    {
        void* pvr = ScanType5(local_res20, s_tid_HappinessDescription);
        iVar27 = pvr ? *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(pvr) + 0x2c) : 0;
    }

LAB_after_target:
    // ---- decide box bounds (LAB_14010ae8a feeder) ----
    happinessMode = false;   // local_res10 low byte = 0
    if (local_res20 == nullptr) {
        iVar13 = static_cast<int>(uVar37) / *piVar5;   // box row
        iVar30 = static_cast<int>(uVar37) % *piVar5;   // box col
        iVar24 = 1;                                    // box height
        local_res18_w = 1;                             // box width
        // iVar27 keeps the value from the path that reached here (6 from home-box, or the
        // validated-target radius). For a pure-null target with no home-box build iVar27=6.
        goto LAB_14010ae8a;
    }

    // local_res20 != 0: scan node[6]/[7] for HappinessComponent (0x1c42c0) -> IBP local_res18.
    {
        void* hcomp = ScanArr(local_res20, s_tid_HappinessComponent);
        int64_t local_res18 = 0;
        if (fn_InternalBasePtr_Ctor) new (&local_res18) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(hcomp));   // B.3.3 bridge
        if (local_res18 == 0) {
            happinessMode = false;   // LAB_14010adba
        } else {
            void* hcomp2 = ScanArr(local_res20, s_tid_HappinessComponent);
            int64_t local_res10n = 0;
            if (fn_InternalBasePtr_Ctor) new (&local_res10n) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(hcomp2));   // B.3.3 bridge
            // LAB_14010ad7a: node[3] = HappinessDescription (0x1c42f8); if +0x20==5 || node[5]==0 -> false.
            int64_t* hnode = reinterpret_cast<int64_t*>(local_res10n);
            void* hdRaw = reinterpret_cast<void*>(hnode[3]);
            void* hd = (fn_IsDerivedFrom && fn_IsDerivedFrom(hdRaw, s_tid_HappinessDescription)) ? hdRaw : nullptr;
            if (!((hd && *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(hd) + 0x20) == 5)
                  || static_cast<char>(hnode[5]) == '\0')) {
                happinessMode = true;   // local_res10 low byte = 1
            }
        }
        // third scan: MapComponent (0x1c79e0) -> local_b8; read bounds shorts (+0x28..+0x2e).
        void* mapc = ScanArr(local_res20, s_tid_MapComponent);
        int64_t local_b8 = 0;
        if (fn_InternalBasePtr_Ctor) new (&local_b8) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mapc));   // B.3.3 bridge
        int64_t* mnode = reinterpret_cast<int64_t*>(local_b8);
        iVar30          = static_cast<int>(*reinterpret_cast<short*>(reinterpret_cast<uintptr_t>(mnode) + 0x28));
        iVar13          = static_cast<int>(*reinterpret_cast<short*>(reinterpret_cast<uintptr_t>(mnode) + 0x2a));
        local_res18_w   = static_cast<int>(*reinterpret_cast<short*>(reinterpret_cast<uintptr_t>(mnode) + 0x2c));
        iVar24          = static_cast<int>(*reinterpret_cast<short*>(reinterpret_cast<uintptr_t>(mnode) + 0x2e));
    }

LAB_14010ae8a:
    {
        iVar13 = iVar13 - iVar27;                       // box row origin
        int iVar36 = local_res18_w + iVar27 * 2;        // box width
        uint32_t boxH = static_cast<uint32_t>(iVar24 + iVar27 * 2);  // box height
        int64_t* local_c8 = nullptr; uint32_t local_c0 = 0;          // cell BST

        for (int rr = static_cast<int>(boxH); rr > 0; --rr) {
            for (int cc = 0; cc < iVar36; ++cc) {
                int iVar16 = cc + (iVar30 - iVar27);
                int w = *piVar5;
                if (iVar16 >= 0 && iVar16 < w && iVar13 >= 0 && iVar13 < w) {
                    int cellIdx = w * iVar13 + cc + (iVar30 - iVar27);
                    int64_t lv = static_cast<int64_t>(cellIdx);
                    if (*reinterpret_cast<uint8_t*>(CellArrayBase(piVar5) + 0x29 + lv * 0x30) < 3) {
                        if (s_zoneRefresh) s_zoneRefresh(piVar5, reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(boxH))), nullptr, iVar27);
                        int64_t arrBase = CellArrayBase(piVar5);
                        uint32_t mNew = 1u << (*reinterpret_cast<uint8_t*>(arrBase + lv * 0x30 + 0x29) & 0x1f);
                        uint32_t mSrc = 1u << (*reinterpret_cast<uint8_t*>(arrBase + local_a8 + 0x29) & 0x1f);
                        uint32_t both = mSrc | mNew;
                        uint32_t mask = both | 0xe1f;
                        bool match = false;
                        if ((mask & mSrc) != 0 && (mask & mNew) != 0) {
                            if ((both & 0x1e0) != 0) match = true;
                            else {
                                short z0 = *reinterpret_cast<short*>(arrBase + local_a8 + 0x26);
                                short z1 = *reinterpret_cast<short*>(arrBase + lv * 0x30 + 0x26);
                                if (z0 != 0 && z1 != 0 && z0 == z1) match = true;
                            }
                        }
                        if (match) {
                            if (local_c8 == nullptr) {
                                int* nd = reinterpret_cast<int*>(NodeAlloc(0x28));
                                if (nd) { nd[0] = cellIdx; *reinterpret_cast<int64_t*>(nd + 2) = 0; *reinterpret_cast<int64_t*>(nd + 4) = 0; *reinterpret_cast<int64_t*>(nd + 6) = 0; nd[8] = 0; }
                                local_c8 = reinterpret_cast<int64_t*>(nd);
                                local_c0 = local_c0 + 1;
                            } else {
                                int64_t* cur = local_c8; int64_t* ins = cur; bool done = false;
                                do {
                                    ins = cur;
                                    if (cellIdx < *reinterpret_cast<int*>(cur)) {
                                        cur = reinterpret_cast<int64_t*>(cur[2]);
                                        if (cur == nullptr) {
                                            int* nd = reinterpret_cast<int*>(NodeAlloc(0x28));
                                            if (nd) { nd[0] = cellIdx; *reinterpret_cast<int64_t*>(nd + 2) = reinterpret_cast<int64_t>(ins); *reinterpret_cast<int64_t*>(nd + 4) = 0; *reinterpret_cast<int64_t*>(nd + 6) = 0; nd[8] = 0; }
                                            ins[2] = reinterpret_cast<int64_t>(nd);
                                            if (s_avl) s_avl(reinterpret_cast<int64_t*>(&local_c8), reinterpret_cast<int64_t*>(&ins));
                                            local_c0 = local_c0 + 1; done = true; break;
                                        }
                                    } else if (cellIdx <= *reinterpret_cast<int*>(cur)) {
                                        done = true; break;  // present
                                    } else {
                                        cur = reinterpret_cast<int64_t*>(cur[3]);
                                        if (cur == nullptr) {
                                            int* nd = reinterpret_cast<int*>(NodeAlloc(0x28));
                                            if (nd) { nd[0] = cellIdx; *reinterpret_cast<int64_t*>(nd + 2) = reinterpret_cast<int64_t>(ins); *reinterpret_cast<int64_t*>(nd + 4) = 0; *reinterpret_cast<int64_t*>(nd + 6) = 0; nd[8] = 0; }
                                            ins[3] = reinterpret_cast<int64_t>(nd);
                                            if (s_avl) s_avl(reinterpret_cast<int64_t*>(&local_c8), reinterpret_cast<int64_t*>(&ins));
                                            local_c0 = local_c0 + 1; done = true; break;
                                        }
                                    }
                                } while (!done);
                            }
                        }
                    }
                }
            }
            ++iVar13;
        }

        uint64_t uVar41 = 0;
        if (local_c0 != 0) {
            uint32_t pick = reinterpret_cast<System::Random*>(pRVar6)->Rand(static_cast<uint32_t>(local_c0));   // B.3.4b bridge
            int64_t* node = local_c8; int64_t* puVar21 = local_c8;
            while (node != nullptr) { puVar21 = node; node = reinterpret_cast<int64_t*>(node[2]); }
            for (uint32_t s = pick; s > 0; --s) {
                if (puVar21 != nullptr) {
                    int64_t* right = reinterpret_cast<int64_t*>(puVar21[3]);
                    if (right == nullptr) {
                        int64_t* parent = reinterpret_cast<int64_t*>(puVar21[1]);
                        if (parent != nullptr && reinterpret_cast<int64_t*>(parent[2]) != puVar21) {
                            while (parent != nullptr && reinterpret_cast<int64_t*>(parent[3]) == puVar21) {
                                puVar21 = parent; parent = reinterpret_cast<int64_t*>(parent[1]);
                            }
                        }
                        puVar21 = reinterpret_cast<int64_t*>(puVar21[1]);
                    } else {
                        do { puVar21 = right; right = reinterpret_cast<int64_t*>(puVar21[2]); } while (right != nullptr);
                    }
                }
            }
            int chosenCell = puVar21 ? *reinterpret_cast<int*>(puVar21) : 0;

            // cell -> world centre.
            int iv = *piVar5;
            float fVar39c = static_cast<float>(iv >> 1) - kCellBias;
            float inXY[2] = { static_cast<float>(chosenCell % iv) - fVar39c,
                              static_cast<float>(chosenCell / iv) - fVar39c };
            uVar41 = 0;
            if (s_gridToWorld) {
                float out3[4] = {0, 0, 0, 0};
                s_gridToWorld(piVar5, out3, inXY, '\0', nullptr);
                local_a0 = out3[0]; local_9c = out3[1]; local_98 = out3[2];
            }
            // random offset within a disk.
            float fVar39 = reinterpret_cast<System::Random*>(pRVar6)->RandFloatTol() * kDiskScale;   // B.3.4b bridge
            float fVar40 = reinterpret_cast<System::Random*>(pRVar6)->RandFloatTol();   // B.3.4b bridge
            float off[4] = { fVar40 * kDiskScale, fVar39, 0.0f, 0.0f };
            float dest[4] = { local_a0 + off[0], local_9c + off[1], local_98 + off[2], 0.0f };

            // entity to anim toward (local_res18): if happinessMode use target else null IBP.
            char cVar12 = happinessMode ? 1 : 0;
            int64_t entIBP;
            if (cVar12 == '\0') { entIBP = 0; if (fn_InternalBasePtr_Ctor) new (&entIBP) ::Object::InternalBasePtr(
                nullptr); }   // B.3.3 bridge
            else entIBP = reinterpret_cast<int64_t>(local_res20);
            int64_t local_res18 = entIBP;

            int64_t local_a8h = *self;   // holder citizen (PlayAnimation self IBP)
            float fVar39t = reinterpret_cast<System::Random*>(pRVar6)->RandFloatTol();   // B.3.4b bridge
            if (s_playAnim) s_playAnim(&local_a8h, &local_res18, reinterpret_cast<uint32_t*>(dest),
                                       fVar39t * kAgeMult + kAnimTimeBase,
                                       static_cast<uint32_t>(static_cast<unsigned char>(cVar12)));

            uintptr_t cz = static_cast<uintptr_t>(*self);
            *reinterpret_cast<int32_t*>(cz + 0x30) = static_cast<int32_t>(0xffffffff);
            *reinterpret_cast<uint32_t*>(cz + 0x54) |= 4;
            *reinterpret_cast<int32_t*>(cz + 0xb0) = 0;
            *reinterpret_cast<int64_t*>(param_1 + 0xc0) = 0;
            uVar41 = 1;
        }

        if (local_c8 != nullptr) {
            if (s_bstFree) { s_bstFree(&local_c8, reinterpret_cast<void*>(local_c8[2]));
                             s_bstFree(&local_c8, reinterpret_cast<void*>(local_c8[3])); }
            if (fn_MemFreeStatic) fn_MemFreeStatic(local_c8);
        }
        return uVar41;
    }
}

// ------------------------------------------------------------
// Registration trampolines (the 3 helpers are file-static; these give them external
// linkage so standalone/reg_citizenai_leisure.cpp can register their exe addresses).
// EnsureInit() must run before the helpers touch the resolved fn pointers / type-ids;
// Leisure calls it, but a direct exe-address call into a helper might not, so we guard.
// ------------------------------------------------------------
extern "C" int64_t* CitizenAI_State_Leisure_sub_99d0_Reg(int64_t p1, int64_t* p2, uint32_t p3, int p4) {
    EnsureInit();
    return CitizenAI_State_Leisure_sub_99d0(p1, p2, p3, p4);
}
extern "C" int64_t* CitizenAI_State_Leisure_sub_fd50_Reg(int64_t p1, int64_t* p2) {
    EnsureInit();
    return CitizenAI_State_Leisure_sub_fd50(p1, p2);
}
extern "C" char CitizenAI_helper_a720_Reg(int64_t p1) {
    EnsureInit();
    return CitizenAI_helper_a720(p1);
}

