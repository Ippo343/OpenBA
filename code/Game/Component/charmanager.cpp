// code/Game/Component/charmanager.cpp
// Phase 3 Keystone D / move-tick tail — the CHARACTER SPATIAL-HASH MANAGER.
//
// The object at *(GameWorld+0x88) (GameWorld = *g_pGameWorldSingleton) is a loose
// implicit-quadtree spatial hash of all live characters. CharacterComponent_OnAdd
// (0x1401099c0) registers a character into it via sub_83f0, which returns the char
// render-index written to CharacterComponent+0x40 — the value that GATES the move-tick
// TAIL (the tail early-returns while +0x40 < 0). sub_8660 (re-)places a character into
// its spatial cell on every move-tick. The manager backs proximity queries
// (FUN_140092260/990) + culling; it does NOT itself touch the transform write, but the
// char-index it produces is the tail's enabling key.
//
// MANAGER STRUCT (byte offsets; verified vs disasm of sub_83f0/sub_8660/8c40/8d00/8560):
//   +0x00 float  worldSize        (full world extent in world units)
//   +0x04 float  center.x         (= worldSize/2, so worldX+center maps to [0,worldSize))
//   +0x08 float  center.y
//   +0x0c int    levels           (quadtree depth; sub_8660 clamps the computed level to this)
//   +0x10 void*  slotArray        (stride 0x18 entry, indexed by char-index)
//   +0x18 int    slotCount
//   +0x1c int    slotCapacity
//   +0x20 int*   freeList         (recycled char-indices)
//   +0x28 int    freeCount
//   +0x2c int    freeCapacity
//   +0x30 int*   cellHead         (per global cell: head char-index, -1 = empty)
//   +0x40 int*   levelOffsetTable (cell base index per level = (4^L-1)/3)
//   +0x50 int*   cellOccupancy    (per global cell: subtree occupancy count; walked up the tree)
// SLOT ENTRY (0x18 bytes): +0x00 InternalBasePtr(8) -> CharacterComponent;
//   +0x08 int prevLink; +0x0c int nextLink (circular list within a cell); +0x10 int cellIdx.
//
// Implicit-quadtree layout (PROVEN): a node's children are 4i+1..4i+4, parent = (i-1)>>2,
// level-L base = (4^L-1)/3. Identity: levelBase[L] = 4*levelBase[L-1]+1, so for a global
// index g = levelBase[L]+morton, (g-1)>>2 = levelBase[L-1] + (morton>>2) — the parent walk
// in 8c40/8d00 is consistent with the per-level morton indexing in sub_8660.
//
// Faithful 1:1 ports (extern "C", INT-width returns where the original reads EAX). My
// reimpls call each other directly by name; reg_charmanager.cpp registers each exe addr so
// external callers (OnAdd, the move-tick, the radius queries) route here via ResolveAddr.
//
// Inlined trivial ops (mathematically exact; avoids resolving 5 extra Runtime exports —
// flagged for lead review): Volume::Rectangle::Center/Size (rect = {min.xy,max.xy}),
// Math::Vector2::operator+=/operator*= (component-wise), Math::Scalar::Max.
//


#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <vector>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

namespace {

// --- exe .rdata consts (resolved lazily) ---
static const float* s_pUVScale   = nullptr;   // g_fUVScale @ 0x14017214c (= 1.0f)
static const float* s_pRadiusEps = nullptr;   // DAT_1401720b8 (radius bias)
static const float* s_pHalf      = nullptr;   // DAT_140172114 (= 0.5f)
static bool s_constInit = false;
static void EnsureConsts() {
    if (s_constInit) return;
    s_pUVScale   = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pRadiusEps = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401720b8));
    s_pHalf      = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_constInit = true;
}


} // namespace

// ===========================================================================
// 0x1400928c0  helper_28c0 — character bounding rectangle.
//   param_1 = &slot (slot[0] = InternalBasePtr -> CharacterComponent)
//   param_2 = out Rectangle {min.x, min.y, max.x, max.y} (4 floats)
//   rect = char model pos (+0x158/+0x15c) +/- CharacterDescription radius (+0x20).
// ===========================================================================
extern "C" float* CharMgr_helper_28c0(void* slotPtr, float* outRect) {
    int64_t ch = *reinterpret_cast<int64_t*>(slotPtr);             // CharacterComponent (via IBP)
    int64_t model = *reinterpret_cast<int64_t*>(ch + 0x68);        // ModelComponent
    float px = *reinterpret_cast<float*>(model + 0x158);
    float py = *reinterpret_cast<float*>(model + 0x15c);
    float r = *reinterpret_cast<float*>(
                  *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(slotPtr) + 0x78) + 0x20);
    outRect[0] = px - r;   // min.x
    outRect[1] = py - r;   // min.y
    outRect[2] = px + r;   // max.x
    outRect[3] = py + r;   // max.y
    return outRect;
}

// ===========================================================================
// 0x140018c40  helper_8c40 — insert char-index into its cell's circular list +
//   bump the subtree occupancy counts up the tree.
// ===========================================================================
extern "C" void CharMgr_helper_8c40(int64_t mgr, int idx) {
    uint8_t* slots    = *reinterpret_cast<uint8_t**>(mgr + 0x10);
    int*     cellHead = *reinterpret_cast<int**>(mgr + 0x30);
    int*     occ      = *reinterpret_cast<int**>(mgr + 0x50);
    uint8_t* slot     = slots + static_cast<int64_t>(idx) * 0x18;
    int cellIdx = *reinterpret_cast<int*>(slot + 0x10);
    int head    = cellHead[cellIdx];
    if (head < 0) {
        cellHead[cellIdx] = idx;
        *reinterpret_cast<int*>(slot + 0x08) = idx;   // prev = self
        *reinterpret_cast<int*>(slot + 0x0c) = idx;   // next = self
    } else {
        uint8_t* headSlot = slots + static_cast<int64_t>(head) * 0x18;
        int headNext = *reinterpret_cast<int*>(headSlot + 0x0c);
        *reinterpret_cast<int*>(slot + 0x0c) = headNext;   // next = head.next
        *reinterpret_cast<int*>(slot + 0x08) = head;       // prev = head
        *reinterpret_cast<int*>(slots + static_cast<int64_t>(headNext) * 0x18 + 0x08) = idx; // head.next.prev = idx
        *reinterpret_cast<int*>(slots + static_cast<int64_t>(head)     * 0x18 + 0x0c) = idx; // head.next = idx
    }
    uint32_t c = static_cast<uint32_t>(*reinterpret_cast<int*>(slot + 0x10));
    occ[static_cast<int>(c)] += 1;
    while (c != 0) {
        c = static_cast<uint32_t>((static_cast<int>(c) - 1) >> 2);
        occ[static_cast<int>(c)] += 1;
    }
}

// ===========================================================================
// 0x140018d00  helper_8d00 — unlink char-index from its cell's circular list +
//   decrement the subtree occupancy counts up the tree.
// ===========================================================================
extern "C" void CharMgr_helper_8d00(int64_t mgr, int idx) {
    uint8_t* slots    = *reinterpret_cast<uint8_t**>(mgr + 0x10);
    int*     cellHead = *reinterpret_cast<int**>(mgr + 0x30);
    int*     occ      = *reinterpret_cast<int**>(mgr + 0x50);
    int64_t  off      = static_cast<int64_t>(idx) * 0x18;
    uint8_t* slot     = slots + off;

    int cellIdx = *reinterpret_cast<int*>(slot + 0x10);
    int* headP = &cellHead[cellIdx];
    if (*headP == idx) {
        *headP = *reinterpret_cast<int*>(slots + static_cast<int64_t>(idx) * 0x18 + 0x08); // head = self.prev
    }
    // self.prev.next = self.next ; self.next.prev = self.prev
    int prev = *reinterpret_cast<int*>(slot + 0x08);
    int next = *reinterpret_cast<int*>(slot + 0x0c);
    *reinterpret_cast<int*>(slots + static_cast<int64_t>(prev) * 0x18 + 0x0c) = next;
    *reinterpret_cast<int*>(slots + static_cast<int64_t>(next) * 0x18 + 0x08) = prev;
    *reinterpret_cast<int*>(slot + 0x0c) = -1;
    *reinterpret_cast<int*>(slot + 0x08) = -1;

    // if this cell's head still points at idx, clear it (single-element cell)
    int cIdx2 = *reinterpret_cast<int*>(slot + 0x10);
    if (cellHead[cIdx2] == idx) cellHead[cIdx2] = -1;

    uint32_t c = static_cast<uint32_t>(*reinterpret_cast<int*>(slot + 0x10));
    occ[static_cast<int>(c)] -= 1;
    while (c != 0) {
        c = static_cast<uint32_t>((static_cast<int>(c) - 1) >> 2);
        occ[static_cast<int>(c)] -= 1;
    }
}

// ===========================================================================
// 0x140018660  sub_8660 — compute the global cell for a character's current world
//   position and, if it changed, move it (8d00 out of old cell, 8c40 into new).
// ===========================================================================
extern "C" void CharMgr_sub_8660(float* mgrF, int idx) {
    EnsureConsts();
    uint8_t* mgr = reinterpret_cast<uint8_t*>(mgrF);
    uint8_t* slots = *reinterpret_cast<uint8_t**>(mgr + 0x10);
    uint8_t* slot  = slots + static_cast<int64_t>(idx) * 0x18;

    float rect[4];
    CharMgr_helper_28c0(slot, rect);                       // {min.xy, max.xy}

    // PERF (2026-09-05): the cell is a pure function of (rect, manager constants). Every
    // character re-derives it every tick, but an idle one's rect is bit-identical to the
    // last tick's, and the only effect of the body below is "move the slot to the cell it
    // is already in". Remember the last rect per slot (keyed with the slot's character so
    // a reused index never matches; CharMgr_impl_8560 also resets +0x10 to -1 on free)
    // and return early while it is unchanged AND the slot is still registered (+0x10 >= 0).
    // Identical registration, no divides/Morton/relink for a standing citizen.
    {
        struct RectMemo { uint64_t who; uint32_t b0, b1, b2, b3; };
        static std::vector<RectMemo> s_memo;
        if (static_cast<size_t>(idx) >= s_memo.size()) s_memo.resize(static_cast<size_t>(idx) + 256, RectMemo{0, 0, 0, 0, 0});
        RectMemo& m = s_memo[static_cast<size_t>(idx)];
        uint32_t b0, b1, b2, b3;
        std::memcpy(&b0, &rect[0], 4); std::memcpy(&b1, &rect[1], 4);
        std::memcpy(&b2, &rect[2], 4); std::memcpy(&b3, &rect[3], 4);
        const uint64_t who = *reinterpret_cast<const uint64_t*>(slot);
        if (m.who == who && m.b0 == b0 && m.b1 == b1 && m.b2 == b2 && m.b3 == b3 &&
            *reinterpret_cast<int*>(slot + 0x10) >= 0)
            return;
        m.who = who; m.b0 = b0; m.b1 = b1; m.b2 = b2; m.b3 = b3;
    }

    // Rectangle::Center / Size (inlined) ; radius = max(half-extent.x, half-extent.y)
    float half = s_pHalf ? *s_pHalf : 0.5f;
    float cx = (rect[0] + rect[2]) * half;                 // center.x
    float cy = (rect[1] + rect[3]) * half;                 // center.y
    float hx = (rect[2] - rect[0]) * half;                 // half-extent.x
    float hy = (rect[3] - rect[1]) * half;                 // half-extent.y
    float radius = hx > hy ? hx : hy;                      // Scalar::Max

    // center += manager center offset (so worldX+offset maps into [0,worldSize))
    cx += *reinterpret_cast<float*>(mgr + 0x04);
    cy += *reinterpret_cast<float*>(mgr + 0x08);

    float worldSize = *reinterpret_cast<float*>(mgr + 0x00);
    int levels = *reinterpret_cast<int*>(mgr + 0x0c);

    // PERF (2026-09-04): level / cellsPerSide / cellsPerUnit are a pure function of
    // (radius, worldSize, levels) — three float divides that every one of the ~1000
    // characters recomputes every tick with the SAME inputs (one description radius, one
    // world). Memoise the last inputs; the cached values are the identical float results,
    // so cell assignment is bit-for-bit what the divides produce.
    static uint32_t s_mRadiusBits = 0xffffffffu, s_mWorldBits = 0xffffffffu;
    static int s_mLevels = -1, s_mLevel = 0, s_mCellsPerSide = 1;
    static float s_mCellsPerUnit = 0.0f;
    uint32_t radiusBits, worldBits;
    std::memcpy(&radiusBits, &radius, 4);
    std::memcpy(&worldBits, &worldSize, 4);
    if (radiusBits != s_mRadiusBits || worldBits != s_mWorldBits || levels != s_mLevels) {
        float eps = s_pRadiusEps ? *s_pRadiusEps : 0.0f;
        float lr = worldSize / (radius + eps);

        // level = floor(log2(lr)) via float-exponent extraction, clamped to [0, levels-1]
        uint32_t lrBits;
        std::memcpy(&lrBits, &lr, 4);
        int level = static_cast<int>((lrBits >> 0x17) - 0x7f);
        if (levels <= level) level = levels - 1;
        if (level < 0) level = 0;

        int cellsPerSide = 1 << level;
        float uvScale = s_pUVScale ? *s_pUVScale : 1.0f;
        s_mCellsPerUnit = uvScale / (worldSize / static_cast<float>(cellsPerSide));
        s_mLevel = level; s_mCellsPerSide = cellsPerSide;
        s_mRadiusBits = radiusBits; s_mWorldBits = worldBits; s_mLevels = levels;
    }
    const int level = s_mLevel;
    const int cellsPerSide = s_mCellsPerSide;
    const float cellsPerUnit = s_mCellsPerUnit;
    cx *= cellsPerUnit;
    cy *= cellsPerUnit;

    uint32_t ux = static_cast<uint32_t>(static_cast<int>(cx));
    uint32_t uy = static_cast<uint32_t>(static_cast<int>(cy));
    if (static_cast<int>(ux) < 0) ux = 0;
    if (static_cast<int>(uy) < 0) uy = 0;
    if (cellsPerSide <= static_cast<int>(ux)) ux = cellsPerSide - 1;
    if (cellsPerSide <= static_cast<int>(uy)) uy = cellsPerSide - 1;

    // Morton interleave (transcribed bit-for-bit from the decompile).
    ux = (ux << 8 | ux) & 0xff00ff;
    ux = (ux << 4 | ux) & 0xf0f0f0f;
    ux = ux * 4 | ux;                       // NOT masked here (matches disasm)
    uy = (uy << 8 | uy) & 0xff00ff;
    uy = (uy << 4 | uy) & 0xf0f0f0f;
    uy = (uy * 4 | uy) & 0x33333333;
    // result = (((A ^ B) & 0x55555555) ^ A) | E   where:
    uint32_t A = (uy * 2 | uy) * 2;
    uint32_t B = ux & 0x33333333;
    uint32_t E = (ux & 0x22222222) * 2;
    uint32_t morton = ((((A ^ B) & 0x55555555) ^ A) | E);
    int* offTbl = *reinterpret_cast<int**>(mgr + 0x40);
    int globalCell = static_cast<int>(morton) + offTbl[level];

    if (*reinterpret_cast<int*>(slot + 0x10) != globalCell) {
        if (-1 < *reinterpret_cast<int*>(slot + 0x10))
            CharMgr_helper_8d00(reinterpret_cast<int64_t>(mgr), idx);
        *reinterpret_cast<int*>(slot + 0x10) = globalCell;
        CharMgr_helper_8c40(reinterpret_cast<int64_t>(mgr), idx);
    }
}

// ===========================================================================
// 0x1400021d0  FUN_1400021d0 — generic int-array resize to `newCount` (grow x3 on
//   first realloc, copy survivors, free old). Used by sub_83f0 to pop the free-list.
//   arr layout: [0]=int* data, +0x08 int count, +0x0c int capacity.
// ===========================================================================
extern "C" void CharMgr_IntArrayResize_21d0(int64_t* arr, int newCount) {
    int oldCount = static_cast<int>(arr[1]);
    if (newCount == oldCount) return;
    void* oldData = reinterpret_cast<void*>(arr[0]);
    *reinterpret_cast<int*>(arr + 1) = newCount;            // count = newCount (low 32 of arr[1])
    if (*reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc) < newCount) {
        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0)
            *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc) = newCount * 3;
        int cap = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(arr) + 0xc);
        void* nd = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                                             static_cast<uint64_t>(cap) << 2, 4) : nullptr;
        arr[0] = reinterpret_cast<int64_t>(nd);
    }
    int start = (oldData == reinterpret_cast<void*>(arr[0])) ? oldCount : 0;
    int* dst = reinterpret_cast<int*>(arr[0]);
    for (int i = start; i < newCount; ++i) {
        if (i < oldCount && dst)
            dst[i] = reinterpret_cast<int*>(oldData)[i];
    }
    if (oldData != reinterpret_cast<void*>(arr[0]) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldData);
}

// ===========================================================================
// 0x1400183f0  sub_83f0 — register a character (IBP at *ibp) into the manager and
//   return its char-index. Pops the free-list if non-empty, else appends (growing the
//   slot array). Then spatial-places it via sub_8660.
//   param_1 = manager (float*), param_2 = &IBP-to-CharacterComponent.
// ===========================================================================
extern "C" int64_t CharMgr_sub_83f0(float* mgrF, int64_t* ibp) {
    uint8_t* mgr = reinterpret_cast<uint8_t*>(mgrF);
    int freeCount = *reinterpret_cast<int*>(mgr + 0x28);
    uint64_t index;

    if (freeCount < 1) {
        // ---- append path ----
        int count = *reinterpret_cast<int*>(mgr + 0x18);
        index = static_cast<uint32_t>(count);
        int newCount = count + 1;
        if (newCount != count) {
            void* oldSlots = *reinterpret_cast<void**>(mgr + 0x10);
            *reinterpret_cast<int*>(mgr + 0x18) = newCount;
            if (*reinterpret_cast<int*>(mgr + 0x1c) < newCount) {
                *reinterpret_cast<int*>(mgr + 0x1c) = newCount;
                if (count != 0) *reinterpret_cast<int*>(mgr + 0x1c) = newCount * 3;
                void* ns = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                              static_cast<uint64_t>(*reinterpret_cast<int*>(mgr + 0x1c)) * 0x18, 8)
                              : nullptr;
                *reinterpret_cast<void**>(mgr + 0x10) = ns;
            }
            int start = (oldSlots == *reinterpret_cast<void**>(mgr + 0x10)) ? count : 0;
            uint8_t* slots = *reinterpret_cast<uint8_t**>(mgr + 0x10);
            for (int i = start; i < newCount; ++i) {
                uint8_t* dst = slots + static_cast<int64_t>(i) * 0x18;
                // source: existing slot (copy) for survivors, else the {ibp,0,-1} template
                int64_t srcIbp; int64_t srcLinks; int srcCell;
                if (i < count) {
                    uint8_t* src = reinterpret_cast<uint8_t*>(oldSlots) + static_cast<int64_t>(i) * 0x18;
                    srcIbp   = *reinterpret_cast<int64_t*>(src + 0x00);
                    srcLinks = *reinterpret_cast<int64_t*>(src + 0x08);
                    srcCell  = *reinterpret_cast<int*>(src + 0x10);
                } else {
                    srcIbp   = *ibp;     // the new IBP
                    srcLinks = 0;        // prev/next = 0
                    srcCell  = -1;       // cellIdx = -1
                }
                *reinterpret_cast<int64_t*>(dst + 0x00) = 0;
                *reinterpret_cast<int64_t*>(dst + 0x00) = srcIbp;
                *reinterpret_cast<int64_t*>(dst + 0x08) = srcLinks;
                *reinterpret_cast<int*>(dst + 0x10) = srcCell;
            }
            if (oldSlots != *reinterpret_cast<void**>(mgr + 0x10) && fn_MemFreeStatic)
                fn_MemFreeStatic(oldSlots);
        }
    } else {
        // ---- free-list pop path ----
        int* freeList = *reinterpret_cast<int**>(mgr + 0x20);
        index = static_cast<uint32_t>(freeList[freeCount - 1]);
        CharMgr_IntArrayResize_21d0(reinterpret_cast<int64_t*>(mgr + 0x20), freeCount - 1);
        uint8_t* slots = *reinterpret_cast<uint8_t**>(mgr + 0x10);
        uint8_t* slot = slots + index * 0x18;
        *reinterpret_cast<int64_t*>(slot + 0x00) = *ibp;
        *reinterpret_cast<int64_t*>(slot + 0x08) = 0;
        *reinterpret_cast<int*>(slot + 0x10) = -1;
    }

    CharMgr_sub_8660(mgrF, static_cast<int>(index));
    return static_cast<int64_t>(index & 0xffffffff);
}

// ===========================================================================
// 0x140018560  impl_8560 — release a char-index back to the manager (OnRemove path):
//   unlink from its cell (8d00), reset the slot to a null IBP, push index onto free-list.
// ===========================================================================
extern "C" void CharMgr_impl_8560(int64_t mgr, int idx) {
    uint8_t* m = reinterpret_cast<uint8_t*>(mgr);
    uint8_t* slot = *reinterpret_cast<uint8_t**>(m + 0x10) + static_cast<int64_t>(idx) * 0x18;
    CharMgr_helper_8d00(mgr, idx);
    int64_t nullIbp = 0;
    if (fn_InternalBasePtr_Ctor) new (&nullIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *reinterpret_cast<int64_t*>(slot + 0x00) = nullIbp;
    *reinterpret_cast<int64_t*>(slot + 0x08) = 0;
    *reinterpret_cast<int*>(slot + 0x10) = -1;

    // push idx onto the free-list (grow by 3x on first realloc)
    int freeCount = *reinterpret_cast<int*>(m + 0x28);
    int newCount = freeCount + 1;
    if (newCount != freeCount) {
        void* oldFL = *reinterpret_cast<void**>(m + 0x20);
        *reinterpret_cast<int*>(m + 0x28) = newCount;
        if (*reinterpret_cast<int*>(m + 0x2c) < newCount) {
            *reinterpret_cast<int*>(m + 0x2c) = newCount;
            if (freeCount != 0) *reinterpret_cast<int*>(m + 0x2c) = newCount * 3;
            void* nf = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                          static_cast<uint64_t>(*reinterpret_cast<int*>(m + 0x2c)) << 2, 4) : nullptr;
            *reinterpret_cast<void**>(m + 0x20) = nf;
        }
        int start = (oldFL == *reinterpret_cast<void**>(m + 0x20)) ? freeCount : 0;
        int* fl = *reinterpret_cast<int**>(m + 0x20);
        for (int i = start; i < newCount; ++i) {
            if (fl) fl[i] = (i < freeCount) ? reinterpret_cast<int*>(oldFL)[i] : idx;
        }
        if (oldFL != *reinterpret_cast<void**>(m + 0x20) && fn_MemFreeStatic)
            fn_MemFreeStatic(oldFL);
    }
}

