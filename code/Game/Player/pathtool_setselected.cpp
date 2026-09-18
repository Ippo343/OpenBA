// code/Game/Player/pathtool_setselected.cpp
// Faithful 1:1 port of the PathTool "SetSelectedEntity" route-builder chain from
// Application-x64.exe. This is what fills the PathTool's point-marker (+0x40) and
// line-segment (+0x50) arrays that PathTool::Draw renders — i.e. it computes the
// worker home<->workplace routes shown by the "Paths" report tool.
//
// Ported functions (registered so pathtool.cpp's ResolveAddr(Exe, 0x1400dd710)
// resolves to us in the standalone build):
//   0x1400dd710  PathTool_helper_d710            SetSelectedEntity dispatcher
//   0x1400dd250  thunk_helper_d250               clear old selection highlight
//   0x1400dd0f0  PathTool_helper_d0f0            set new selection highlight
//   0x1400dd770  PathTool_helper_d770            the route builder (A* + AVL dedup)
//   0x1400de5d0  PathTool_helper_d770_sub_e5d0   workplace -> cell index
//   0x140026ba0  PathTool_helper_d770_sub_6ba0   route-array push (stride 0x200c)
//   0x140007080  thunk_helper_d250_sub_7080      FindComponent(HighlightComponent)
//
// Route-entry struct (stride 0x200c) in the PathTool +0x30 array:
//   +0x00 int srcCell, +0x04 int dstCell, +0x08 int pathCellCount,
//   +0x0c int pathCells[0x800]
// PathTool fields: +0x28 selectedEntity IBP; +0x30/+0x38/+0x3c route array {ptr,cnt,cap};
//   +0x40/+0x48/+0x4c point-marker int array; +0x50/+0x58/+0x5c line-segment 8B array.
//
// AVL dedup nodes are 0x28 bytes (identical layout for both the point tree — int key —
// and the segment tree — packed {min,max} 8-byte key):
//   +0x00 key (int or 8B), +0x08 parent, +0x10 left, +0x18 right,
//   +0x20 short balance, +0x22 short height.
// The rebalance/insert/flatten logic is byte-identical between the two trees, so it is
// factored into shared helpers (AvlRebalanceUp / *Tree_Insert / in-order flatten) — a
// faithful equivalent of the exe's 4x-inlined copies, transcribed from the disassembly.
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra); cross-checked vs disassembly.

#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName / ResolveTypeIdByNameLazy
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

namespace {

// ---- registered exe sub-callees (all resolve to our reimpls in standalone) ----
typedef void     (*fn_HighlightRefresh_t)(int64_t comp);                              // 0x14004c4a0
typedef void*    (*fn_FindComp_t)(int64_t entity, void* retIBP);                      // 0x140002e10 / 0x140008090
typedef uint32_t (*fn_AnchorCell_t)(void* ibp, float* p2);                            // 0x140087fb0
typedef void*    (*fn_AStar_t)(void* map, void* out2ints, uint64_t mask, uint32_t src, uint32_t dst); // 0x14009cfb0
typedef void     (*fn_ListFree_t)(void* listHdr, int flag);                          // 0x1400021d0
typedef void     (*fn_AvlErase_t)(void** root, void* node);                          // 0x140014c30
typedef void     (*fn_AvlRotate_t)(int64_t* root, int64_t node);                     // 0x140014c70 / 0x140014da0
typedef void*    (*fn_ResolvePos_t)(int64_t ent, void* outVec3, int a, int b, float* p5); // 0x140122e20
typedef void     (*fn_Yield_t)(void* app);                                            // Application::YieldToThreads

static fn_HighlightRefresh_t s_c4a0   = nullptr;
static fn_FindComp_t         s_2e10   = nullptr;   // WorkPlace-ish finder
static fn_FindComp_t         s_8090   = nullptr;   // RESIDENCE finder (Ghidra mislabels
                                                   // 0x140008090 "FindComponent_Storage"; the
                                                   // disasm tail-calls 0x140006270 with the
                                                   // ResidenceComponent type id).
static fn_AnchorCell_t       s_7fb0   = nullptr;
static fn_AStar_t            s_9cfb0  = nullptr;
static fn_ListFree_t         s_21d0   = nullptr;
static fn_AvlErase_t         s_14c30  = nullptr;
static fn_AvlRotate_t        s_14c70  = nullptr;
static fn_AvlRotate_t        s_14da0  = nullptr;
static fn_ResolvePos_t       s_122e20 = nullptr;
static fn_Yield_t            s_yield  = nullptr;

static const uint32_t* s_pTid_Highlight = nullptr;  // DAT_1401c4410 = HighlightComponent type-id
static bool s_init = false;

static void EnsureFns() {
    if (s_init) return;
    s_c4a0   = reinterpret_cast<fn_HighlightRefresh_t>(ResolveAddr(Module::Exe, 0x14004c4a0));
    s_2e10   = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_8090   = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140008090));
    s_7fb0   = reinterpret_cast<fn_AnchorCell_t>(ResolveAddr(Module::Exe, 0x140087fb0));
    s_9cfb0  = reinterpret_cast<fn_AStar_t>(ResolveAddr(Module::Exe, 0x14009cfb0));
    s_21d0   = reinterpret_cast<fn_ListFree_t>(ResolveAddr(Module::Exe, 0x1400021d0));
    s_14c30  = reinterpret_cast<fn_AvlErase_t>(ResolveAddr(Module::Exe, 0x140014c30));
    s_14c70  = reinterpret_cast<fn_AvlRotate_t>(ResolveAddr(Module::Exe, 0x140014c70));
    s_14da0  = reinterpret_cast<fn_AvlRotate_t>(ResolveAddr(Module::Exe, 0x140014da0));
    s_122e20 = reinterpret_cast<fn_ResolvePos_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_yield  = reinterpret_cast<fn_Yield_t>(ResolveExport("?YieldToThreads@Application@1@QEAAXXZ"));
    // DAT_1401c4410 (HighlightComponent type-id) is raw exe .rdata -> NULL in standalone.
    static uint32_t s_tidHi = 0;
    if (s_tidHi == 0) s_tidHi = ::Object::GetTypeIdByName("HighlightComponent");
    s_pTid_Highlight = &s_tidHi;
    s_init = true;
}

static inline uint32_t GetTypeId(void* obj) {
    if (!obj) return 0;
    return (*(uint32_t(**)(void*))(*(uintptr_t*)obj + 0x10))(obj);
}

// ---- AVL node accessors (0x28-byte node, shared layout) ----
static inline void*  NodeParent(void* n) { return *(void**)((uintptr_t)n + 0x08); }
static inline void*  NodeLeft  (void* n) { return *(void**)((uintptr_t)n + 0x10); }
static inline void*  NodeRight (void* n) { return *(void**)((uintptr_t)n + 0x18); }
static inline short  NodeBal   (void* n) { return *(short*)((uintptr_t)n + 0x20); }
static inline short  NodeHeight(void* n) { return *(short*)((uintptr_t)n + 0x22); }

// Allocate + init a fresh 0x28-byte AVL node with the given 8-byte key and parent.
static void* AvlNodeAlloc(uint64_t key, void* parent) {
    void* n = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!n) return nullptr;
    uintptr_t p = (uintptr_t)n;
    *(uint64_t*)(p + 0x00) = key;
    *(void**)(p + 0x08)    = parent;
    *(uint64_t*)(p + 0x10) = 0;   // left
    *(uint64_t*)(p + 0x18) = 0;   // right
    *(uint32_t*)(p + 0x20) = 0;   // balance(short)+height(short)
    return n;
}

// Retrace/rebalance walking up from `node` (the node a child was just linked to).
// Faithful transcription of the exe's inlined rebalance: recompute height/balance,
// then AVL single/double rotate (14c70 = rotate-left, 14da0 = rotate-right).
static void AvlRebalanceUp(void** root, void* node) {
    while (node != nullptr) {
        void* parent = NodeParent(node);
        void* left   = NodeLeft(node);
        void* right  = NodeRight(node);
        short lh = left  ? (short)(NodeHeight(left)  + 1) : 0;
        short rh = right ? (short)(NodeHeight(right) + 1) : 0;
        short h  = (rh < lh) ? lh : rh;
        *(short*)((uintptr_t)node + 0x22) = h;
        short bal = (short)(rh - lh);
        *(short*)((uintptr_t)node + 0x20) = bal;
        if (bal > 1 && right) {
            short rbal = NodeBal(right);
            if (rbal < 1 && rbal < 0) { if (s_14da0) s_14da0((int64_t*)root, (int64_t)right); }
            if (s_14c70) s_14c70((int64_t*)root, (int64_t)node);
        }
        // Second check re-reads balance from memory (a prior rotation may have changed it).
        short bal2 = NodeBal(node);
        if (bal2 < -1) {
            left = NodeLeft(node);
            if (left) {
                short lbal = NodeBal(left);
                if (lbal > -1 && lbal > 0) { if (s_14c70) s_14c70((int64_t*)root, (int64_t)left); }
                if (s_14da0) s_14da0((int64_t*)root, (int64_t)node);
            }
        }
        node = parent;
    }
}

// Insert `key` into the point AVL tree (int key, dedup on equal). count at *pCount.
static void PointTree_Insert(void** root, int* pCount, int key) {
    if (*root == nullptr) {
        void* n = AvlNodeAlloc((uint32_t)key, nullptr);
        if (!n) return;
        *pCount += 1;
        *root = n;
        return;
    }
    void* cur = *root;
    for (;;) {
        int nk = *(int*)cur;
        if (key < nk) {
            if (NodeLeft(cur) == nullptr) {
                void* n = AvlNodeAlloc((uint32_t)key, cur);
                *(void**)((uintptr_t)cur + 0x10) = n;
                AvlRebalanceUp(root, cur);
                *pCount += 1;
                return;
            }
            cur = NodeLeft(cur);
        } else if (key > nk) {
            if (NodeRight(cur) == nullptr) {
                void* n = AvlNodeAlloc((uint32_t)key, cur);
                *(void**)((uintptr_t)cur + 0x18) = n;
                AvlRebalanceUp(root, cur);
                *pCount += 1;
                return;
            }
            cur = NodeRight(cur);
        } else {
            return;  // equal → dedup
        }
    }
}

// Insert a packed {min,max} segment key into the segment AVL tree (dedup on equal pair).
static void SegTree_Insert(void** root, int* pCount, int minCell, int maxCell, uint64_t packedKey) {
    if (*root == nullptr) {
        void* n = AvlNodeAlloc(packedKey, nullptr);
        if (!n) return;
        *pCount += 1;
        *root = n;
        return;
    }
    void* cur = *root;
    for (;;) {
        int nmin = (int)(*(uint64_t*)cur);
        int nmax = (int)(*(uint64_t*)cur >> 32);
        bool goLeft = false, skip = false;
        if (minCell < nmin) {
            goLeft = true;
        } else if (minCell == nmin) {
            if (maxCell < nmax) goLeft = true;
            else if (maxCell <= nmax) skip = true;   // equal pair
        }
        if (skip) return;
        if (goLeft) {
            if (NodeLeft(cur) == nullptr) {
                void* n = AvlNodeAlloc(packedKey, cur);
                *(void**)((uintptr_t)cur + 0x10) = n;
                AvlRebalanceUp(root, cur);
                *pCount += 1;
                return;
            }
            cur = NodeLeft(cur);
        } else {
            if (NodeRight(cur) == nullptr) {
                void* n = AvlNodeAlloc(packedKey, cur);
                *(void**)((uintptr_t)cur + 0x18) = n;
                AvlRebalanceUp(root, cur);
                *pCount += 1;
                return;
            }
            cur = NodeRight(cur);
        }
    }
}

// In-order traversal helpers.
static void* InorderFirst(void* n) { if (!n) return nullptr; while (NodeLeft(n)) n = NodeLeft(n); return n; }
static void* InorderNext(void* n) {
    if (NodeRight(n)) { n = NodeRight(n); while (NodeLeft(n)) n = NodeLeft(n); return n; }
    void* p = NodeParent(n);
    while (p && NodeRight(p) == n) { n = p; p = NodeParent(p); }
    return p;
}

// The three output lists of PathTool::SetSelected, at self+0x30 / +0x40 / +0x50
// (each {data, size, cap}).
//
// ENDGAME B.3.2 step 2: IntArrayPush / U64ArrayPush / PathSel_RoutePush_6ba0 WERE
// hand-written copies of rtcoll::SetSizeCore and the three "reset" lines in
// SetSelected were inlined Collection::Array<T>::~Array bodies. Compared arm for arm
// before the swap: the two pushes always had newCount == oldCount + 1, so their
// `oldCount != 0 ? n*3 : n` is the reference capacity rule (and their missing
// exact-equal return is unreachable, n+1 != n); size stored before the capacity
// test; copy the prefix only when the block moved; the new slot takes the fill;
// free the old block when it moved. 6ba0 is the general form and matches the
// reference line for line, fill included. Rows:
//   * +0x40 point list: trait-derived Array<int32_t> (stride 4, align 4, byteCopy);
//   * +0x50 segment list: 8-byte elements at ALIGN 4, because this site allocates
//     `fn_MemAlloc(..., cap << 3, 4)` — kept as written;
//   * +0x30 route list: stride 0x200c, align 4, memcpy assign.
// All three: byteCopy == true, init == nullptr (a fill is always supplied),
// destroy == nullptr (trivial elements).
// ENDGAME B.3.2 closing pass: both hand-derived rows said only what a C++ type's
// traits say — a whole-stride byte copy, `init == nullptr` (retail's empty else
// arm: the new tail keeps whatever the allocation held), `destroy == nullptr`,
// and no deliberately-untouched extent — so each is now the trait-derived
// `Collection::Array<T>` over an element with the row's stride and align. The
// elements deliberately carry NO initialiser, which is what makes
// `SpecFor<T>()` derive `init == nullptr`.

// The +0x50 segment element: 8 bytes at ALIGN 4, because this site allocates
// with `fn_MemAlloc(..., cap << 3, 4)`. `GameColl::I32Pair` is exactly that
// shape, and coll_bridge.h's GAMECOLL_CHECK_ELEM_NOINIT already asserts it
// leaves its new tail alone.
static_assert(sizeof(GameColl::I32Pair)  == 8, "PathSeg stride");
static_assert(alignof(GameColl::I32Pair) == 4, "PathSeg align");

// The +0x30 route element, exe 0x140026ba0: 0x200c bytes at align 4.
struct alignas(4) PathRouteEntry { uint8_t b[0x200c]; };   // exe 0x140026ba0
static_assert(sizeof(PathRouteEntry)  == 0x200c, "PathRouteEntry stride");
static_assert(alignof(PathRouteEntry) == 4,      "PathRouteEntry align");

using PointArray = Collection::Array<int32_t>;
using SegArray   = Collection::Array<GameColl::I32Pair>;
using RouteArray = Collection::Array<PathRouteEntry>;

// Dynamic-array push (exe grow pattern: cap = newCount*3 on the first non-empty grow).
static void IntArrayPush(PointArray* a, int val)     { a->SetSize(a->_size + 1, val); }
static void U64ArrayPush(SegArray* a, uint64_t val)  { a->SetSizeFill(a->_size + 1, &val); }

}  // namespace

// ============================================================
// 0x140007080 — FindComponent(HighlightComponent) on a ComponentEntity.
// param_1 = entity (+0x30 component ptr array, +0x38 count); writes IBP into param_2.
// ============================================================
static void* PathSel_FindHighlight_7080(int64_t param_1, void* param_2) {
    EnsureFns();
    void* found = nullptr;
    int count = *(int*)(param_1 + 0x38);
    int64_t* arr = *(int64_t**)(param_1 + 0x30);
    uint32_t want = s_pTid_Highlight ? *s_pTid_Highlight : 0;
    for (int i = 0; i < count; i++) {
        void* comp = (void*)arr[i];
        if (GetTypeId(comp) == want && want != 0) {
            void* c = (void*)arr[i];
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, want)) found = c;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (param_2) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return param_2;
}

// ============================================================
// 0x1400de5d0 — sub_e5d0: workplace(+0x238 of the citizen) -> cell index.
// param_2 is an IBP whose inner (+0) is a CitizenAI; +0x238 = its workplace entity.
// ============================================================
static int PathSel_WorkplaceCell_e5d0(int64_t /*param_1*/, void* param_2) {
    EnsureFns();
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int64_t workplace = *(int64_t*)(*(int64_t*)param_2 + 0x238);
    uint64_t posIBP = 0;
    // 🐞 MISNAMED-FINDER TRAP (6th instance). The disasm at 0x1400de629 calls
    // 0x140002ed0 = fn_Entity_FindComponent_Position -> InteractComponent (type
    // DWORD_1401c7900), NOT the inline fn_FindComponent_Position in engine_funcs.h,
    // which is 0x14000d6c0 -> BuildComponent (DWORD_1401c7008). Both are spelled
    // "..._Position" and both return an IBP, so the wrong one compiles and runs.
    // It matters because 0x140122e20 casts the component's +0x18 description to
    // InteractDescription: an InteractComponent carries one, a BuildComponent
    // carries a BuildDescription (a SIBLING of InteractDescription, not a base —
    // factory_registrations.csv:232/273 both list parent "Description"). The cast
    // failed, our SA-GUARD returned a zero vector, and e5d0 mapped (0,0,0) to
    // cell (w>>1, w>>1) = the dead CENTRE of the map. Every route therefore started
    // at the map centre instead of the workplace door -- and because the centre is
    // the same cell for every workplace, N workers collapsed into N identical
    // src/dst pairs (the "duplicate routes" report).
    fn_Entity_FindComponent_Position((uintptr_t)workplace, &posIBP);
    float vec[3] = {0, 0, 0};
    if (s_122e20) s_122e20(*(int64_t*)&posIBP, vec, 0, 0, nullptr);
    int w = *mapGrid;
    int col = (int)(vec[0] + (float)(w >> 1));   // local_18 = x
    int row = (int)((float)(w >> 1) + vec[1]);   // local_14 = y
    if (col < 0) col = 0;
    if (w <= col) col = w - 1;
    if (row < 0) row = 0;
    if (w <= row) row = w - 1;
    return row * w + col;
}

// ============================================================
// 0x140026ba0 — sub_6ba0: push a route entry (stride 0x200c) into the +0x30 array.
// param_1 = array header {ptr@0, count@+8, cap@+0xc}; param_2 = new count; param_3 = src elem.
// ============================================================
static void PathSel_RoutePush_6ba0(int64_t* param_1, int param_2, uint64_t* param_3) {
    EnsureFns();
    // B.3.2 bridge: the +0x30 route header is still raw bytes in the PathTool struct.
    reinterpret_cast<RouteArray*>(param_1)->SetSizeFill(param_2, param_3);
}

// ============================================================
// 0x1400dd250 — clear the highlight flag (+0x11c) on the selected entity's related
// entities and refresh (c4a0). self+0x28 = selectedEntity IBP.
// ============================================================
// FUN_140004740 finds a component holding a related-entity list (+0x48 array, +0x50 count).
static fn_FindComp_t s_4740 = nullptr;
static void EnsureFind4740() {
    if (!s_4740) s_4740 = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140004740));
}

static void PathSel_ClearHighlight_d250(int64_t param_1) {
    EnsureFns();
    EnsureFind4740();
    void* self = (void*)(param_1 + 0x28);
    if (*(int64_t*)(param_1 + 0x28) == 0) return;
    uint64_t listIBP = 0;
    if (s_4740) s_4740(*(int64_t*)self, &listIBP);
    if (listIBP != 0) {
        int n = *(int*)(listIBP + 0x50);
        for (int i = 0; i < n; i++) {
            int64_t ent = *(int64_t*)(*(int64_t*)(listIBP + 0x48) + (int64_t)i * 8);
            uint64_t hi = 0;
            PathSel_FindHighlight_7080(ent, &hi);
            if (hi != 0) {
                if (*(char*)(hi + 0x11c) != '\0') {
                    *(char*)(hi + 0x11c) = 0;
                    if (s_c4a0) s_c4a0((int64_t)hi);
                }
            }
        }
    }
    uint64_t hi2 = 0;
    PathSel_FindHighlight_7080(*(int64_t*)self, &hi2);
    if (hi2 != 0) {
        if (*(char*)(hi2 + 0x11c) != '\0') {
            *(char*)(hi2 + 0x11c) = 0;
            if (s_c4a0) s_c4a0((int64_t)hi2);
        }
    }
}

// ============================================================
// 0x1400dd0f0 — set the highlight flag (+0x11c = 1) on the related entities. Begins by
// clearing (d250), then sets. Same shape as d250 with flag value 1.
// ============================================================
static void PathSel_SetHighlight_d0f0(int64_t param_1) {
    EnsureFns();
    PathSel_ClearHighlight_d250(param_1);
    if (*(int64_t*)(param_1 + 0x28) == 0) return;
    EnsureFind4740();
    void* self = (void*)(param_1 + 0x28);
    uint64_t listIBP = 0;
    if (s_4740) s_4740(*(int64_t*)self, &listIBP);
    if (listIBP != 0) {
        int n = *(int*)(listIBP + 0x50);
        for (int i = 0; i < n; i++) {
            int64_t ent = *(int64_t*)(*(int64_t*)(listIBP + 0x48) + (int64_t)i * 8);
            uint64_t hi = 0;
            PathSel_FindHighlight_7080(ent, &hi);
            if (hi != 0) {
                if (*(char*)(hi + 0x11c) != '\x01') {
                    *(char*)(hi + 0x11c) = 1;
                    if (s_c4a0) s_c4a0((int64_t)hi);
                }
            }
        }
    }
    uint64_t hi2 = 0;
    PathSel_FindHighlight_7080(*(int64_t*)self, &hi2);
    if (hi2 != 0) {
        if (*(char*)(hi2 + 0x11c) != '\x01') {
            *(char*)(hi2 + 0x11c) = 1;
            if (s_c4a0) s_c4a0((int64_t)hi2);
        }
    }
}

// ============================================================
// 0x1400dd770 — the route builder. Determines the home<->workplace cell pairs for the
// selected entity (citizen / workplace / storage), A*-routes each pair, and dedups the
// path cells (point tree) and segments (segment tree) into +0x40 / +0x50.
// ============================================================
static void PathSel_BuildRoutes_d770(int64_t param_1) {
    EnsureFns();

    uint64_t local_res20 = 0;   // FUN_140002e10 result (workplace-ish component)
    if (s_2e10) s_2e10(*(int64_t*)(param_1 + 0x28), &local_res20);
    uint64_t local_res18 = 0;   // ResidenceComponent (8090) - its +0x50/+0x58 is the
                                // occupant list: outer groups (0x10 stride) of citizens.
    if (s_8090) s_8090(*(int64_t*)(param_1 + 0x28), &local_res18);
    uint64_t local_2098 = 0;    // CitizenAI component
    fn_FindComponent_Citizen(*(void**)(param_1 + 0x28), &local_2098);

    int64_t* routeArr = (int64_t*)(param_1 + 0x30);   // plVar24: route array header
    // Reset the three output arrays: each WAS "MemFree the buffer, zero _data, zero
    // the _size/_capacity qword" — the Collection::Array<T>::~Array body for a
    // trivially destructible T — now the dtor itself, in place.
    // B.3.2 bridge ×3: the headers live in the unconverted PathTool struct.
    reinterpret_cast<RouteArray*>(param_1 + 0x30)->~Array();
    reinterpret_cast<PointArray*>(param_1 + 0x40)->~Array();
    reinterpret_cast<SegArray*>(param_1 + 0x50)->~Array();

    uint8_t entryBuf[0x200c];   // scratch route entry pushed via 6ba0 (garbage tail OK; count=0)

    // --- Determine the source route pairs. ---
    // (A) selected entity IS a citizen with both workplace (+0x238) and housing (+0x248).
    bool citizenHandled = false;
    if (local_2098 != 0) {
        if (*(int64_t*)(local_2098 + 0x238) != 0) {
            if (*(int64_t*)(local_2098 + 0x248) != 0) {
                citizenHandled = true;
                uint64_t housing = 0;
                { uint64_t tmp = local_2098; housing = *(uint64_t*)(tmp + 0x248); }
                int anchorCell = s_7fb0 ? (int)s_7fb0(&housing, nullptr) : -1;
                uint64_t selIBP = local_2098;   // retail uses a COPY here (local_2080),
                                                // not the live CitizenAI IBP slot.
                int wpCell = PathSel_WorkplaceCell_e5d0(local_2098, &selIBP);
                *(int32_t*)(entryBuf + 0x00) = wpCell;      // srcCell
                *(int32_t*)(entryBuf + 0x04) = anchorCell;  // dstCell
                *(int32_t*)(entryBuf + 0x08) = 0;           // pathCount
                PathSel_RoutePush_6ba0(routeArr, *(int*)(param_1 + 0x38) + 1, (uint64_t*)entryBuf);
            }
        }
    }
    (void)citizenHandled;

    // (B) not a workplace-with-workers → iterate storage occupants; else iterate workers.
    bool useStorage;
    if (local_res20 == 0) { useStorage = true; }
    else { useStorage = (*(int64_t*)(local_res20 + 0x48) == 0); }

    if (useStorage) {
        if (local_res18 != 0) {
            int outer = *(int*)(local_res18 + 0x58);
            for (int oi = 0; oi < outer; oi++) {
                int64_t innerHdr = *(int64_t*)(local_res18 + 0x50) + (int64_t)oi * 0x10;
                int inner = *(int*)(innerHdr + 8);
                for (int ii = 0; ii < inner; ii++) {
                    int64_t ent = *(int64_t*)((int64_t)ii * 8 + *(int64_t*)(*(int64_t*)(local_res18 + 0x50) + (int64_t)oi * 0x10));
                    if (*(int64_t*)(ent + 0x238) != 0) {
                        if (*(int64_t*)(ent + 0x248) != 0) {
                            uint64_t housing = 0;
                            { uint64_t tmp = ent; housing = *(uint64_t*)(tmp + 0x248); }
                            int anchorCell = s_7fb0 ? (int)s_7fb0(&housing, nullptr) : -1;
                            uint64_t entIBP = ent;
                            int wpCell = PathSel_WorkplaceCell_e5d0(ent, &entIBP);
                            *(int32_t*)(entryBuf + 0x00) = wpCell;
                            *(int32_t*)(entryBuf + 0x04) = anchorCell;
                            *(int32_t*)(entryBuf + 0x08) = 0;
                            PathSel_RoutePush_6ba0(routeArr, *(int*)(param_1 + 0x38) + 1, (uint64_t*)entryBuf);
                        }
                    }
                }
            }
        }
    } else {
        int workerCount = *(int*)(local_res20 + 0x60);
        for (int wi = 0; wi < workerCount; wi++) {
            int64_t ent = *(int64_t*)((int64_t)wi * 8 + *(int64_t*)(local_res20 + 0x58));
            if (*(int64_t*)(ent + 0x238) != 0) {
                if (*(int64_t*)(ent + 0x248) != 0) {
                    uint64_t housing = 0;
                    { uint64_t tmp = ent; housing = *(uint64_t*)(tmp + 0x248); }
                    int anchorCell = s_7fb0 ? (int)s_7fb0(&housing, nullptr) : -1;
                    uint64_t entIBP = ent;
                    int wpCell = PathSel_WorkplaceCell_e5d0(ent, &entIBP);
                    *(int32_t*)(entryBuf + 0x00) = wpCell;
                    *(int32_t*)(entryBuf + 0x04) = anchorCell;
                    *(int32_t*)(entryBuf + 0x08) = 0;
                    PathSel_RoutePush_6ba0(routeArr, *(int*)(param_1 + 0x38) + 1, (uint64_t*)entryBuf);
                }
            }
        }
    }

    // --- A*-route each pair, dedup cells/segments into the point/segment AVL trees. ---
    int* map = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    void* pointTree = nullptr;  int pointCount = 0;
    void* segTree   = nullptr;  int segCount   = 0;

    int routeCount = *(int*)(param_1 + 0x38);
    for (int r = 0; r < routeCount; r++) {
        int64_t routeBase = *routeArr;
        int64_t off = (int64_t)r * 0x200c;
        uint32_t srcCell = *(uint32_t*)(routeBase + off);
        uint32_t dstCell = *(uint32_t*)(routeBase + off + 4);

        // Async A* query: returns {idx@+0, queryId@+4}. Spin on the map's result table.
        uint64_t qr = 0;
        if (s_9cfb0) s_9cfb0(map, &qr, 0xe1f, srcCell, dstCell);
        int qIdx = (int)qr;
        int qId  = (int)(qr >> 32);
        if (qId == 0 || qIdx < 0) {
            continue;   // no query issued → skip this route
        }

        int64_t holder = *(int64_t*)((uintptr_t)map + 0x68);
        // Wait for the async worker to publish this query's cells.
        bool ready = false;
        // Standalone safety: bail the spin after a large bound (the exe spins forever;
        // if the async pathfinder worker is not feeding results we must not hang the game).
        for (int64_t spin = 0; spin < 20000000; ++spin) {
            int cap = *(int*)(holder + 0x168);
            int64_t table = *(int64_t*)(holder + 0x160);
            if (qIdx < cap &&
                *(int*)(table + 4 + (int64_t)qIdx * 0x18) == qId &&
                *(char*)(table + (int64_t)qIdx * 0x18) != '\0') { ready = true; break; }
            if (s_yield && g_pAppSingleton) s_yield(*g_pAppSingleton);
            else break;
        }
        if (!ready) {
            continue;
        }

        // Copy the path cells (capped at 0x800) into routeEntry.pathCells, release the slot.
        int64_t table = *(int64_t*)(holder + 0x160);
        int64_t slot = table + (int64_t)qIdx * 0x18;
        int cellCount = *(int*)(slot + 0x10);
        if (cellCount > 0x800) cellCount = 0x800;
        int64_t cellsPtr = *(int64_t*)(slot + 0x08);
        for (int c = 0; c < cellCount; c++)
            *(int32_t*)(routeBase + off + 0xc + (int64_t)c * 4) = *(int32_t*)(cellsPtr + (int64_t)c * 4);
        *(int32_t*)(slot + 4) = 0;      // clear queryId
        *(char*)(slot + 0) = 0;         // clear ready flag
        if (s_21d0) s_21d0((void*)(slot + 8), 0);   // free the slot's cell list
        *(int32_t*)(routeBase + off + 8) = cellCount;  // routeEntry.pathCount


        // Insert each cell into the point tree; each consecutive pair into the segment tree.
        int pc = *(int32_t*)(*routeArr + off + 8);
        for (int i = 0; i < pc; i++) {
            int cell = *(int32_t*)(*routeArr + off + 0xc + (int64_t)i * 4);
            PointTree_Insert(&pointTree, &pointCount, cell);
            if (i == 0) continue;   // no previous cell → no segment
            int prev = *(int32_t*)(*routeArr + off + 8 + (int64_t)i * 4);      // pathCells[i-1]
            int curc = *(int32_t*)(*routeArr + off + 0xc + (int64_t)i * 4);    // pathCells[i]
            int mn = prev, mx = curc;
            if (prev < curc) { mn = prev; mx = curc; } else { mn = curc; mx = prev; }
            uint64_t packed = ((uint64_t)(uint32_t)mx << 32) | (uint32_t)mn;
            SegTree_Insert(&segTree, &segCount, mn, mx, packed);
        }
    }

    // --- Flatten trees (in-order) into +0x40 (points) and +0x50 (segments). ---
    for (void* n = InorderFirst(pointTree); n; n = InorderNext(n)) {
        IntArrayPush(reinterpret_cast<PointArray*>(param_1 + 0x40), *(int*)n);   // B.3.2 bridge
    }
    for (void* n = InorderFirst(segTree); n; n = InorderNext(n)) {
        U64ArrayPush(reinterpret_cast<SegArray*>(param_1 + 0x50), *(uint64_t*)n);  // B.3.2 bridge
    }


    // --- Free the AVL trees (14c30 recursively frees a subtree). ---
    if (segTree) {
        if (s_14c30) { s_14c30(&segTree, NodeLeft(segTree)); s_14c30(&segTree, NodeRight(segTree)); }
        fn_MemFreeStatic(segTree);
    }
    if (pointTree) {
        if (s_14c30) { s_14c30(&pointTree, NodeLeft(pointTree)); s_14c30(&pointTree, NodeRight(pointTree)); }
        fn_MemFreeStatic(pointTree);
    }
}

// ============================================================
// 0x1400dd710 — SetSelectedEntity dispatcher (the registered entry point).
// ============================================================
extern "C" void PathTool_helper_d710(int64_t param_1, uint64_t* param_2) {
    EnsureFns();
    if (*(int64_t*)(param_1 + 0x28) != (int64_t)*param_2) {
        PathSel_ClearHighlight_d250(param_1);
        *(uint64_t*)(param_1 + 0x28) = *param_2;
        PathSel_SetHighlight_d0f0(param_1);
        PathSel_BuildRoutes_d770(param_1);
    }
}

#include "address_registry.h"
// ============================================================
// 0x1400dd250 — exported so PathTool's vtable slot +0x28 can reach it.
// ============================================================
// Retail's PathTool vtable (0x140160680) slot +0x28 is 0x1400de930, which is a bare
// JMP to this function — i.e. the tool's Shutdown IS "clear the selection highlight".
// Toolbar_ShowGroup calls that slot on the OUTGOING controller before deleting it
// (`(**(code **)(**(longlong **)(param_1 + 0x28) + 0x28))()` in FUN_140102b60), so it
// is what un-highlights the last selected building when the Paths tool is switched
// away or disabled. Our vtable had `Tool_Ret0` in that slot, so the highlight stayed
// lit on the last building forever. Reported live 2026-08-20.
extern "C" void PathTool_ClearHighlight_d250(int64_t self) {
    PathSel_ClearHighlight_d250(self);
}

extern "C" void RegisterPathToolSetSelected() {
    RegisterExeAddr(0x1400dd710, reinterpret_cast<uintptr_t>(&PathTool_helper_d710));
    RegisterExeAddr(0x1400dd250, reinterpret_cast<uintptr_t>(&PathTool_ClearHighlight_d250));
}
