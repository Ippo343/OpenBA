// code/Game/Work/work_leaf_clear.cpp
// Work-gap leaf batch: closure-safe exe sub-helpers referenced from
// code/Game/Work/{cleararea,harvestarea,plantarea}.cpp.
//
// Like the other work_base_*.cpp files, these are exe helpers resolved via
// ResolveAddr(Module::Exe, 0xADDR) from the Work class reimpls. In DLL mode they
// resolve to original exe code; in standalone mode they resolve to 0 (silently
// skipped) unless registered. This file provides faithful 1:1 ports so the call
// sites fire real logic in the standalone exe. A single registration point lives
// in standalone (see lead-integrated reg_*.cpp).
//
// Each function is a 1:1 port of the decompiled exe helper; do not "improve"
// behavior. Struct field offsets match the decompile EXACTLY (this[0x18] = byte
// offset +0x18, BST node children left=+0x18 / right=+0x20, etc.).
//
// CLOSURE-SAFETY: every exe-code sub-callee below is either already registered
// (snapshot-verified), a sibling ported in this file, or a Runtime engine export
// reachable through engine_funcs.h globals. The three big Action-construction
// ctors that several of these call sites ALSO reference (PerformWork ctor
// 0x1400bffd0, ShowTool ctor4 0x1400c1530, ShowTool ctor3 0x1400bdd20) are
// DEFERRED — their sub-callee closure (action copy-ctors 0x1400c59d0 / 0x1400c63b0
// / 0x1400ca520, Entity_impl 0x1400c29a0, ShowTool_Action_Ctor 0x140042890) is
// unregistered, so porting them now would build a half-null/drifted action and
// silently corrupt — strictly worse than the safe no-op skip.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.
// Bodies are unconditional so it compiles in BOTH dll and standalone builds.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdlib>   // qsort

// ============================================================
// Shared lazily-resolved exe sub-callees (registered, snapshot-verified)
// ============================================================
// 0x14001ca60 — BST single-node remove + rebalance (TradeDescription_impl_ca60).
typedef void* (*fn_BST_Remove_ca60_t)(void* bstRoot, void* node);
static fn_BST_Remove_ca60_t s_fn_BST_Remove_ca60 = nullptr;

// 0x140013480 — BST rebalance/fixup after insert (TradeDescription_impl_3480).
typedef void (*fn_BST_Rebalance_3480_t)(void* bstRoot, void* nodeSlot);
static fn_BST_Rebalance_3480_t s_fn_BST_Rebalance_3480 = nullptr;

static bool s_leafResolved = false;
static void EnsureLeafFuncs() {
    if (s_leafResolved) return;
    s_fn_BST_Remove_ca60 = reinterpret_cast<fn_BST_Remove_ca60_t>(
        ResolveAddr(Module::Exe, 0x14001ca60));
    s_fn_BST_Rebalance_3480 = reinterpret_cast<fn_BST_Rebalance_3480_t>(
        ResolveAddr(Module::Exe, 0x140013480));
    s_leafResolved = true;
}

// ============================================================
// 0x1400298a0 — ClearArea_helper_98a0
// ============================================================
// BST search-and-remove for an InternalBasePtr-keyed tree (the +0x48 entity
// tree). param_1 = &treeRoot (slot holding root ptr). param_2 = InternalBasePtr*
// whose dereferenced 8-byte value is the search key. Nodes: key at +0x00,
// left child at +0x18 (idx 3), right child at +0x20 (idx 4). On match calls
// the registered single-node remove (0x14001ca60).
// ABI: void fn(longlong* treeRootSlot, InternalBasePtr* key)
// Closure: 0x14001ca60 (reg) + InternalBasePtr_CheckPtr (engine). SAFE.
void WorkLeaf_BstRemoveByIBP_298a0(void* treeRootSlot, void* keyIBP) {
    EnsureLeafFuncs();
    uint64_t* node = *reinterpret_cast<uint64_t**>(treeRootSlot);
    if (node == nullptr) return;
    do {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node);
        uint64_t keyVal = *reinterpret_cast<uint64_t*>(keyIBP);
        if (keyVal < *node) {
            node = reinterpret_cast<uint64_t*>(node[3]);       // left (+0x18)
        } else {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(keyIBP);
            if (*reinterpret_cast<uint64_t*>(keyIBP) <= *node) {
                if (s_fn_BST_Remove_ca60) s_fn_BST_Remove_ca60(treeRootSlot, node);
                return;
            }
            node = reinterpret_cast<uint64_t*>(node[4]);       // right (+0x20)
        }
    } while (node != nullptr);
}

// ============================================================
// 0x140029b40 — ClearArea_helper_9920_sub_9b40 (inlined leaf)
// ============================================================
// Allocates + inits a 0x30-byte BST node copying the 16-byte IBP pair from
// param_2 and storing the parent ptr param_3 at node+0x10. Only callee is
// MemAlloc (engine). Ported inline so 9920's closure is self-contained.
static void* WorkLeaf_BstNewNode_9b40(const void* ibpPair, void* parent) {
    uint64_t* n = reinterpret_cast<uint64_t*>(
        fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8) : nullptr);
    if (n == nullptr) return nullptr;
    const uint64_t* src = reinterpret_cast<const uint64_t*>(ibpPair);
    n[0] = 0; n[0] = src[0];           // key / IBP entity (+0x00)
    n[1] = 0; n[1] = src[1];           // IBP work ref     (+0x08)
    n[2] = reinterpret_cast<uint64_t>(parent);  // parent  (+0x10)
    n[3] = 0;                          // left  (+0x18)
    n[4] = 0;                          // right (+0x20)
    *reinterpret_cast<uint32_t*>(n + 5) = 0;    // color/balance (+0x28)
    return n;
}

// ============================================================
// 0x140029920 — ClearArea_helper_9920
// ============================================================
// BST insert (IBP-keyed) into the +0x48 entity tree, returning the inserted /
// existing node through param_2. param_1 = &treeRoot (root at [0], count int at
// [1]). param_3 = InternalBasePtr* (16-byte pair) to insert. On empty tree it
// allocates the root directly; otherwise it walks to a leaf, allocates a child
// via 9b40, then rebalances via 0x140013480. Count (int at treeRoot+0x08) is
// incremented on every insert (including duplicate-found path, matching exe).
// ABI: undefined8* fn(longlong* treeRootSlot, undefined8* outNode, InternalBasePtr* ins)
// Closure: 0x140029b40 (inlined) + 0x140013480 (reg) + MemAlloc/CheckPtr (engine). SAFE.
void* WorkLeaf_BstInsertByIBP_29920(void* treeRootSlot, void* outNode, void* insIBP) {
    EnsureLeafFuncs();
    uint64_t* selfSlot = reinterpret_cast<uint64_t*>(treeRootSlot);
    uint64_t* node = reinterpret_cast<uint64_t*>(selfSlot[0]);
    uint64_t* result;

    if (node == nullptr) {
        // Empty tree — allocate root directly (matches exe's open-coded alloc).
        uint64_t* n = reinterpret_cast<uint64_t*>(
            fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8) : nullptr);
        result = nullptr;
        if (n != nullptr) {
            const uint64_t* src = reinterpret_cast<const uint64_t*>(insIBP);
            n[0] = 0; n[0] = src[0];
            n[1] = 0; n[1] = src[1];
            n[2] = 0;
            n[3] = 0;
            n[4] = 0;
            *reinterpret_cast<uint32_t*>(n + 5) = 0;
            result = n;
        }
        selfSlot[0] = reinterpret_cast<uint64_t>(result);
        *reinterpret_cast<uint64_t*>(outNode) = reinterpret_cast<uint64_t>(result);
    } else {
        uint64_t insKey = *reinterpret_cast<uint64_t*>(insIBP);
        void* newChildSlotOwner = nullptr;  // = local_res8 / pivot for rebalance
        for (;;) {
            // descend-left while key < node key (re-reading key each step via CheckPtr)
            while (true) {
                newChildSlotOwner = node;
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node);
                insKey = *reinterpret_cast<uint64_t*>(insIBP);
                if (insKey >= *node) break;
                uint64_t* left = reinterpret_cast<uint64_t*>(node[3]);  // +0x18
                if (left == nullptr) {
                    result = reinterpret_cast<uint64_t*>(
                        WorkLeaf_BstNewNode_9b40(insIBP, node));
                    node[3] = reinterpret_cast<uint64_t>(result);
                    goto inserted;
                }
                node = left;
            }
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(insIBP);
            if (*reinterpret_cast<uint64_t*>(insIBP) <= *node) {
                // duplicate found — return existing node; early return, NO insert,
                // NO rebalance, and NO count increment (matches exe early `return`).
                *reinterpret_cast<uint64_t*>(outNode) = reinterpret_cast<uint64_t>(node);
                return outNode;
            }
            uint64_t* right = reinterpret_cast<uint64_t*>(node[4]);     // +0x20
            if (right == nullptr) {
                result = reinterpret_cast<uint64_t*>(
                    WorkLeaf_BstNewNode_9b40(insIBP, node));
                node[4] = reinterpret_cast<uint64_t>(result);
                goto inserted;
            }
            node = right;
        }
    inserted:
        if (s_fn_BST_Rebalance_3480)
            s_fn_BST_Rebalance_3480(treeRootSlot, &newChildSlotOwner);
        *reinterpret_cast<uint64_t*>(outNode) = reinterpret_cast<uint64_t>(result);
    }
    *reinterpret_cast<int*>(selfSlot + 1) = static_cast<int>(selfSlot[1]) + 1;
    return outNode;
}

// ============================================================
// 0x140029c10 — ClearArea_helper_9c10
// ============================================================
// Serialize the +0x48 entity BST (pairs of InternalBasePtr: a ComponentEntity
// and a CitizenAIComponent-family ref). param_1 = &treeRoot (count int at [1]),
// param_2 = IO::Serializer*. Writing branch (serializer mode != 0): in-order
// traverse, serialize each IBP pair (with IsDerivedFrom type recheck nulling
// mismatches). Reading branch (mode == 0): read count, then loop reading two
// IBPs per entry and BST-insert via 9920 (sibling).
// ABI: void fn(longlong* treeRootSlot, Serializer* ser)
// Closure: 9920 (sibling) + Serialize(int)/IBP::Serialize/IsDerivedFrom + type
//          IDs (all engine globals). SAFE.
void WorkLeaf_BstSerializeIBPpair_29c10(void* treeRootSlot, void* ser) {
    uint64_t* selfSlot = reinterpret_cast<uint64_t*>(treeRootSlot);

    int count = static_cast<int>(selfSlot[1]);
    if (g_fn_Serialize_Int) g_fn_Serialize_Int(ser, &count, nullptr);

    // serializer mode lives at *(*ser + 8); 0 => reading.
    int mode = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(ser) + 8);

    const uint32_t* tidEntity =
        g_TypeId_ComponentEntity;          // DAT_1401c5d70
    const uint32_t* tidCitizenAI =
        g_TypeId_CitizenAIComponent_FC;    // DAT_1401c7158

    if (mode == 0) {
        // ---- reading ----
        if (count <= 0) return;
        int i = 0;
        do {
            void* ibpA = nullptr;
            void* ibpB = nullptr;
            if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(&ibpA, ser);
            if (ibpA != nullptr && tidEntity && fn_IsDerivedFrom &&
                !fn_IsDerivedFrom(ibpA, *tidEntity)) {
                ibpA = nullptr;
            }
            if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(&ibpB, ser);
            if (ibpB != nullptr && tidCitizenAI && fn_IsDerivedFrom &&
                !fn_IsDerivedFrom(ibpB, *tidCitizenAI)) {
                ibpB = nullptr;
            }
            // pair {ibpA, ibpB} laid out contiguously for the insert key.
            void* pair[2] = { ibpA, ibpB };
            uint64_t outNode = 0;
            WorkLeaf_BstInsertByIBP_29920(treeRootSlot, &outNode, pair);
            i++;
        } while (i < count);
    } else {
        // ---- writing ---- (in-order traversal, serialize each pair)
        uint8_t* root = *reinterpret_cast<uint8_t**>(selfSlot);
        if (root == nullptr) return;
        // find leftmost
        uint8_t* node = root;
        while (*reinterpret_cast<uint8_t**>(node + 0x18) != nullptr) {
            node = *reinterpret_cast<uint8_t**>(node + 0x18);
        }
        while (node != nullptr) {
            // IBP A at node+0x00
            if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(node, ser);
            void* a = *reinterpret_cast<void**>(node);
            if (a != nullptr && tidEntity && fn_IsDerivedFrom &&
                !fn_IsDerivedFrom(a, *tidEntity)) {
                *reinterpret_cast<void**>(node) = nullptr;
            }
            // IBP B at node+0x08
            if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(node + 8, ser);
            void* b = *reinterpret_cast<void**>(node + 8);
            if (b != nullptr && tidCitizenAI && fn_IsDerivedFrom &&
                !fn_IsDerivedFrom(b, *tidCitizenAI)) {
                *reinterpret_cast<void**>(node + 8) = nullptr;
            }
            // in-order successor
            uint8_t* right = *reinterpret_cast<uint8_t**>(node + 0x20);
            if (right != nullptr) {
                node = right;
                while (*reinterpret_cast<uint8_t**>(node + 0x18) != nullptr) {
                    node = *reinterpret_cast<uint8_t**>(node + 0x18);
                }
            } else {
                uint8_t* parent = *reinterpret_cast<uint8_t**>(node + 0x10);
                if (parent != nullptr &&
                    *reinterpret_cast<uint8_t**>(parent + 0x18) != node) {
                    while (parent != nullptr &&
                           *reinterpret_cast<uint8_t**>(parent + 0x20) == node) {
                        node = parent;
                        parent = *reinterpret_cast<uint8_t**>(node + 0x10);
                    }
                    node = parent;
                } else {
                    node = parent;
                }
            }
        }
    }
}

// ============================================================
// 0x14009d7c0 — WorldGrid::SortTilesByDistance (FUN_14009d7c0)
// ============================================================
// For each 16-byte tile entry in the dynamic array (ptr at param_2[0], count int
// at param_2[1]), compute squared grid distance from refCell (param_3) into the
// entry's +0x04 int field, then qsort ascending by that field. Grid width is the
// int at *param_1. Comparator (exe stub @0x14009d7b0): (int)a[+4] - (int)b[+4].
// ABI: void fn(int* mapGrid, longlong* tileArray, int refCell)
// Closure: qsort (CRT) + inline comparator. SAFE.
namespace {
int WorkLeaf_TileDistCompare(const void* a, const void* b) {
    int da = *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(a) + 4);
    int db = *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(b) + 4);
    return da - db;   // MOV EAX,[RCX+4]; SUB EAX,[RDX+4]; RET
}
}
void WorkLeaf_SortTilesByDistance_9d7c0(int* mapGrid, void* tileArray, int refCell) {
    uint64_t* arr = reinterpret_cast<uint64_t*>(tileArray);
    int gridW = *mapGrid;
    int count = *reinterpret_cast<int*>(arr + 1);
    uint8_t* base = reinterpret_cast<uint8_t*>(arr[0]);
    if (count > 0) {
        for (int i = 0; i < count; i++) {
            uint8_t* e = base + static_cast<int64_t>(i) * 0x10;
            int tileIdx = *reinterpret_cast<int*>(e);          // +0x00
            int dy = tileIdx / gridW - refCell / gridW;
            int dx = tileIdx % gridW - refCell % gridW;
            *reinterpret_cast<int*>(e + 4) = dy * dy + dx * dx; // +0x04
        }
    }
    qsort(base, static_cast<size_t>(count), 0x10, &WorkLeaf_TileDistCompare);
}

// ============================================================
// 0x14009fc00 — WorldGrid::GetTilesInArea (FUN_14009fc00)
// ============================================================
// Scan the rectangle [x..x+w) x [y..y+h) of the grid, pushing 16-byte tile
// entries into the dynamic array param_6 ({ptr@0, count int@8, cap int@0xC}).
// For every cell with an entity (grid cell at gridData+idx*0x30, entity IBP at
// +0x08) push {cellIdx, 0, <entity 8 bytes>}. If param_7 (collect-empty) is set,
// for cells whose pass byte (+0x29) lacks bits 0/1 AND flag byte (+0x2a)==0xFF
// AND not already in the +0xE8 BST, push {cellIdx, 0, <null IBP>}.
// Grid width *param_1; cell array param_1[0x20] (=byte +0x80); BST root
// param_1[0x3a] (=byte +0xE8), node key@0 left@+0x10 right@+0x18.
// ABI: void fn(int* grid, int x, int y, int w, int h, longlong* outArr, char collectEmpty)
// Closure: MemAlloc/MemFree/InternalBasePtr_Ctor (engine). SAFE.
//
// Dynamic-array push (16-byte stride, grow x3 on first realloc past the seed,
// copy-then-free) ported verbatim from the exe's open-coded inline grow.
namespace {
// push one 16-byte record `rec` (already laid out: int idx@0, int pad@4, 8B@8)
// into array `arr` ({ptr@0, count@8, cap@0xC}). Faithful to the exe inline.
void WorkLeaf_TileArrPush(uint64_t* arr, const int* rec) {
    int oldCount = *reinterpret_cast<int*>(arr + 1);
    int newCount = oldCount + 1;
    if (newCount == oldCount) return;            // overflow guard (exe: iVar10!=iVar11)
    void* oldPtr = reinterpret_cast<void*>(arr[0]);
    *reinterpret_cast<int*>(arr + 1) = newCount;
    int* capPtr = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc);
    if (*capPtr < newCount) {
        *capPtr = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *capPtr = newCount * 3;
        }
        void* nb = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                                             static_cast<uint64_t>(*capPtr) << 4, 8)
                               : nullptr;
        arr[0] = reinterpret_cast<uint64_t>(nb);
    }
    int copyStart = (oldPtr == reinterpret_cast<void*>(arr[0])) ? oldCount : 0;
    if (copyStart < newCount) {
        int64_t off = static_cast<int64_t>(copyStart) * 0x10;
        int* srcOld = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(oldPtr) + off);
        for (int idx = copyStart; idx < newCount; idx++) {
            int* dst = reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr[0]) + off);
            if (dst != nullptr) {
                const int* s = (idx < oldCount) ? srcOld : rec;
                dst[0] = s[0];
                dst[1] = s[1];
                dst[2] = 0;
                dst[3] = 0;
                *reinterpret_cast<uint64_t*>(dst + 2) =
                    *reinterpret_cast<const uint64_t*>(s + 2);
            }
            srcOld += 4;
            off += 0x10;
        }
    }
    if (oldPtr != reinterpret_cast<void*>(arr[0])) {
        if (fn_MemFree) fn_MemFree(g_pMemAllocSingleton, oldPtr);
    }
}
}

void WorkLeaf_GetTilesInArea_9fc00(int* grid, int x, int y, int w, int h,
                                   void* outArr, char collectEmpty) {
    uint64_t* arr = reinterpret_cast<uint64_t*>(outArr);
    int gridW = *grid;
    uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(grid + 0x20); // byte +0x80
    int rowEnd = h + y;
    if (y >= rowEnd) return;

    int row = y;
    do {
        for (int col = x; col < x + w; col++) {
            int cellIdx = gridW * row + col;
            int64_t cellOff = static_cast<int64_t>(cellIdx) * 0x30;
            uint64_t entity = *reinterpret_cast<uint64_t*>(cellArr + cellOff + 8);
            if (entity != 0) {
                int rec[4];
                rec[0] = cellIdx;
                rec[1] = 0;
                *reinterpret_cast<uint64_t*>(rec + 2) = entity;
                WorkLeaf_TileArrPush(arr, rec);
            }

            if (collectEmpty != '\0') {
                uint8_t passByte = *reinterpret_cast<uint8_t*>(cellArr + cellOff + 0x29);
                uint32_t mask = 1u << (passByte & 0x1f);
                bool emit;
                if ((mask & 2) == 0 && (mask & 1) == 0 &&
                    *reinterpret_cast<int8_t*>(cellArr + cellOff + 0x2a) == -1) {
                    // Pass/flag condition HOLDS: walk the WorldGrid +0xE8 BST.
                    // Exe: found (cellIdx <= *node) -> EMIT; exhausted -> SKIP.
                    int* bnode = *reinterpret_cast<int**>(grid + 0x3a); // byte +0xE8
                    emit = false;
                    while (bnode != nullptr) {
                        if (cellIdx < *bnode) {
                            bnode = *reinterpret_cast<int**>(bnode + 4);  // left  +0x10
                        } else if (cellIdx <= *bnode) {
                            emit = true;                                  // found -> emit
                            break;
                        } else {
                            bnode = *reinterpret_cast<int**>(bnode + 6);  // right +0x18
                        }
                    }
                } else {
                    // Pass/flag condition FAILS -> emit (LAB_14009fdce direct).
                    emit = true;
                }
                if (emit) {
                    uint64_t nullIBP = 0;
                    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
                    int rec[4];
                    rec[0] = cellIdx;
                    rec[1] = 0;
                    *reinterpret_cast<uint64_t*>(rec + 2) = nullIBP;
                    WorkLeaf_TileArrPush(arr, rec);
                }
            }
        }
        row++;
    } while (row < rowEnd);
}
