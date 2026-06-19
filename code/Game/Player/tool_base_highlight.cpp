// code/Game/Player/tool_base_highlight.cpp
// Player wave 5 (TIER-2 geometry) — PlacementTool radius-highlight tessellation.
//
// Closure for HighlightAdd (0x140055430), called from PlacementTool::Serialize/Slot7
// (placementtool.cpp) to draw a building's radius overlay. Worker-thread-free and
// dialog-free (unlike RemovalTool CalcPath / OpenToolDialog). Registered bottom-up by
// RegisterPlayerHighlightBatch (standalone/reg_player_highlight.cpp).
//
//   0x140055430 FUN_140055430        HighlightAdd          -> 0x1400b0d80
//   0x1400b0d80 TradeWork_impl_0d80  tile-slot manager     -> 0x14001fa80(reg), 0x1400ace40
//   0x1400ace40 TradeWork_impl_ace40 radius tessellation   (separate file, agent draft)
//   0x14001f6b0 TradeWork_impl_1f6b0 AVL insert            -> 0x14001f850, 0x140014b40
//   0x14001fc00 TradeWork_impl_1fc00 AVL find              (leaf)
//   0x140014b40 TradeWork_impl_4b40  AVL retrace+rebalance -> 0x140015000(reg), 0x140014ed0(reg)
//   0x14001f850 TradeWork_impl_1f850 AVL node alloc        (leaf, static MemAlloc)
//   0x140014b00 TradeWork_helper_14b00 recursive BST free  (leaf, static MemFree)
//
// AVL node = 0x38 bytes, key = the 6-int (0x18-byte) tile descriptor at node+0x00
// (shorts @+0x00/+0x08, bytes @+0x10..+0x13): +0x18 parent / +0x20 left / +0x28 right
// / +0x30 short balance / +0x32 short height. Same node shape + rotation helpers as
// tool_base_trade.cpp (which exposes PlayerTrade_Rot_15000 = 0x140015000 pivot-left and
// PlayerTrade_Rot_14ed0 = 0x140014ed0 pivot-right, both already registered).
//
// Port is 1:1 with the Ghidra decompile + disasm-verified — do NOT "improve".

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// Shared AVL rotations from tool_base_trade.cpp (0x140015000 / 0x140014ed0, registered).
extern "C" int64_t PlayerTrade_Rot_15000(int64_t* root, int64_t node);
extern "C" int64_t PlayerTrade_Rot_14ed0(int64_t* root, int64_t node);

// The radius tessellation (0x1400ace40) is resolved at runtime via ResolveAddr (NOT a
// link-time extern): it is a large geometry builder still under verification (draft in
// notes/wave5_drafts/ace40.draft.cpp). Until it is registered, ResolveAddr returns null
// and b0d80 skips the tessellation (manages the slot but emits no geometry) — safe.
namespace {

typedef void (*Ace40_t)(int64_t param_1, int param_2, char param_3, int param_4, int param_5);
Ace40_t s_ace40 = nullptr;

// ---- static System::MemoryAllocator MemAlloc/MemFree (2-arg static form via slot
// 0x140152b60 / 0x140152b68, ResolveExport fallback) — mirrors tool_base_trade.cpp. ----
typedef void* (*MemAllocStatic_t)(uint64_t size, uint32_t align);  // SAPEAXII
typedef void  (*MemFreeStatic_t)(void* ptr);                       // SAXPEAX
MemAllocStatic_t s_memAlloc = nullptr;
MemFreeStatic_t  s_memFree  = nullptr;
bool s_memInit = false;
void EnsureMem() {
    if (s_memInit) return;
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_memAlloc = reinterpret_cast<MemAllocStatic_t>(slot(0x140152b60));
    if (!s_memAlloc) s_memAlloc = reinterpret_cast<MemAllocStatic_t>(
        ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAXII@Z"));
    s_memFree = reinterpret_cast<MemFreeStatic_t>(slot(0x140152b68));
    if (!s_memFree) s_memFree = reinterpret_cast<MemFreeStatic_t>(
        ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));
    s_memInit = true;
}

// AVL node field accessors (offsets verified against decompile/disasm).
inline int64_t&  NodeParent(int64_t n) { return *reinterpret_cast<int64_t*>(n + 0x18); }
inline int64_t&  NodeLeft(int64_t n)   { return *reinterpret_cast<int64_t*>(n + 0x20); }
inline int64_t&  NodeRight(int64_t n)  { return *reinterpret_cast<int64_t*>(n + 0x28); }

// 0x14001fa80 TradeWork_impl_fa80 (registered) — grows the tile-renderer entry array.
typedef void (*Fa80_t)(int64_t* arrHolder, int newCount, void* templateEntry);
Fa80_t s_fa80 = nullptr;

} // namespace

// ============================================================
// 0x140014b00 — recursive BST free: free left (+0x20), right (+0x28), then self.
// 1:1 port of FUN_140014b00.
// ============================================================
extern "C" void TradeWork_helper_14b00(int64_t /*ctx*/ param_1, void* node) {
    if (node != nullptr) {
        TradeWork_helper_14b00(param_1, *reinterpret_cast<void**>((int64_t)node + 0x20));
        TradeWork_helper_14b00(param_1, *reinterpret_cast<void**>((int64_t)node + 0x28));
        EnsureMem();
        if (s_memFree) s_memFree(node);
    }
}

// ============================================================
// 0x14001f850 — AVL node allocator. MemAlloc(0x38), copy the 6-int (0x18-byte) key
// from param_2[0..5], set parent (+0x18)=param_3, null children/balance. 1:1 port.
// Returns the new node (or null on OOM).
// ============================================================
extern "C" int64_t TradeWork_impl_1f850(int64_t /*ctx*/ param_1, const void* keySrc,
                                        int64_t parent) {
    (void)param_1;
    EnsureMem();
    uint32_t* p = s_memAlloc ? reinterpret_cast<uint32_t*>(s_memAlloc(0x38, 8)) : nullptr;
    if (p == nullptr) return 0;
    const uint32_t* src = reinterpret_cast<const uint32_t*>(keySrc);
    p[0] = src[0];           // +0x00
    p[1] = src[1];           // +0x04
    p[2] = src[2];           // +0x08
    p[3] = src[3];           // +0x0c
    p[4] = src[4];           // +0x10
    p[5] = src[5];           // +0x14
    *reinterpret_cast<int64_t*>(p + 6)  = parent;  // +0x18 parent
    *reinterpret_cast<int64_t*>(p + 8)  = 0;       // +0x20 left
    *reinterpret_cast<int64_t*>(p + 10) = 0;       // +0x28 right
    p[12] = 0;                                      // +0x30 balance/height
    return reinterpret_cast<int64_t>(p);
}

// ============================================================
// 0x140014b40 — AVL retrace + rebalance, walking up via parent (+0x18) from the start
// node (*param_2). Recomputes height (+0x32) / balance (+0x30) and rotates via the
// shared 0x140015000 (pivot-left) / 0x140014ed0 (pivot-right). 1:1 port of
// TradeWork_impl_4b40.
// ============================================================
extern "C" void TradeWork_impl_4b40(int64_t* param_1, int64_t* param_2) {
    int64_t lVar1 = *param_2;
    while (lVar1 != 0) {
        int64_t node = *param_2;
        lVar1 = *reinterpret_cast<int64_t*>(node + 0x18);          // parent (next)

        int64_t left  = *reinterpret_cast<int64_t*>(node + 0x20);
        int64_t right = *reinterpret_cast<int64_t*>(node + 0x28);
        int16_t sVar6 = (left  == 0) ? 0 : (int16_t)(*reinterpret_cast<int16_t*>(left  + 0x32) + 1);
        int16_t sVar4 = (right == 0) ? 0 : (int16_t)(*reinterpret_cast<int16_t*>(right + 0x32) + 1);
        int16_t sVar3 = (sVar4 < sVar6) ? sVar6 : sVar4;
        *reinterpret_cast<int16_t*>(node + 0x32) = sVar3;
        *reinterpret_cast<int16_t*>(node + 0x30) = (int16_t)(sVar4 - sVar6);

        node = *param_2;
        int16_t bal = *reinterpret_cast<int16_t*>(node + 0x30);
        int64_t r2 = *reinterpret_cast<int64_t*>(node + 0x28);
        if (1 < bal && r2 != 0) {
            int16_t rbal = *reinterpret_cast<int16_t*>(r2 + 0x30);
            if (rbal < 1 && rbal < 0) {           // right child left-heavy
                PlayerTrade_Rot_15000(param_1, r2);
                node = *param_2;
            }
            PlayerTrade_Rot_14ed0(param_1, node);
        }

        node = *param_2;
        bal = *reinterpret_cast<int16_t*>(node + 0x30);
        int64_t l2 = *reinterpret_cast<int64_t*>(node + 0x20);
        if (bal < -1 && l2 != 0) {
            int16_t lbal = *reinterpret_cast<int16_t*>(l2 + 0x30);
            if (-1 < lbal && 0 < lbal) {          // left child right-heavy
                PlayerTrade_Rot_14ed0(param_1, l2);
                node = *param_2;
            }
            PlayerTrade_Rot_15000(param_1, node);
        }

        *param_2 = lVar1;                         // ascend to parent
    }
}

// ============================================================
// 0x14001f6b0 — AVL insert keyed by the 6-int tile descriptor. Returns (in *param_2)
// the matching/created node. 1:1 port of FUN_14001f6b0 (param_1 = {root, count} holder,
// param_3 = key src). param_1[0] = root, param_1[1] (low 32) = count.
// ============================================================
extern "C" int64_t* TradeWork_impl_1f6b0(int64_t* param_1, int64_t* param_2, uint32_t* param_3) {
    int64_t node = *param_1;
    if (node == 0) {
        int64_t n = TradeWork_impl_1f850(reinterpret_cast<int64_t>(param_1), param_3, 0);
        *reinterpret_cast<int*>(param_1 + 1) = (int)param_1[1] + 1;
        *param_1 = n;
        *param_2 = n;
        return param_2;
    }
    for (;;) {                                    // LAB_14001f710
        int64_t cur   = node;
        int64_t lVar11 = cur - reinterpret_cast<int64_t>(param_3);
        // orig (short*)(param_3 + 2) where param_3 is undefined4* -> +8 bytes
        int16_t* p = reinterpret_cast<int16_t*>(param_3 + 2);
        int  iVar10 = 0;
        int64_t lVar8 = 0;
        bool descend = false;     // took a child pointer this pass
        bool inserted = false;
        int64_t newNode = 0;
        int64_t lVar5 = cur;
        // ---- first comparison loop: descend-left / break(equal-or-greater) ----
        for (;;) {
            int16_t sVar3 = *reinterpret_cast<int16_t*>(lVar11 - 8 + reinterpret_cast<int64_t>(p));
            if (p[-4] < sVar3) {                  // LAB_14001f7dd: go right or insert-right
                int64_t r = *reinterpret_cast<int64_t*>(cur + 0x20);
                if (r != 0) { node = r; descend = true; break; }
                newNode = TradeWork_impl_1f850(lVar8, param_3, cur);
                *reinterpret_cast<int64_t*>(lVar5 + 0x20) = newNode;
                inserted = true; break;           // LAB_14001f813
            }
            if (sVar3 < p[-4]) break;
            if (*p < *reinterpret_cast<int16_t*>(lVar11 + reinterpret_cast<int64_t>(p))) {
                int64_t r = *reinterpret_cast<int64_t*>(cur + 0x20);
                if (r != 0) { node = r; descend = true; break; }
                newNode = TradeWork_impl_1f850(lVar8, param_3, cur);
                *reinterpret_cast<int64_t*>(lVar5 + 0x20) = newNode;
                inserted = true; break;
            }
            if (*reinterpret_cast<int16_t*>(lVar11 + reinterpret_cast<int64_t>(p)) < *p) break;
            {
                uint8_t b1 = *reinterpret_cast<uint8_t*>(lVar8 + 0x10 + reinterpret_cast<int64_t>(param_3));
                uint8_t b2 = *reinterpret_cast<uint8_t*>(lVar8 + 0x10 + cur);
                if (b1 < b2) {
                    int64_t r = *reinterpret_cast<int64_t*>(cur + 0x20);
                    if (r != 0) { node = r; descend = true; break; }
                    newNode = TradeWork_impl_1f850(lVar8, param_3, cur);
                    *reinterpret_cast<int64_t*>(lVar5 + 0x20) = newNode;
                    inserted = true; break;
                }
                if (b1 != b2) break;
            }
            iVar10++; lVar8++; p++;
            if (iVar10 >= 4) break;
        }
        if (descend) continue;                    // goto LAB_14001f710
        if (inserted) {
            TradeWork_impl_4b40(param_1, &cur);
            *reinterpret_cast<int*>(param_1 + 1) = (int)param_1[1] + 1;
            *param_2 = newNode;
            return param_2;
        }
        // ---- second comparison loop: exact-match(return) / descend-right / insert-right ----
        iVar10 = 0;
        p = reinterpret_cast<int16_t*>(param_3 + 2);   // orig (short*)(param_3 + 2)
        lVar8 = 0;
        bool found = false;
        for (;;) {
            int16_t sVar3 = *reinterpret_cast<int16_t*>(reinterpret_cast<int64_t>(p) + lVar11 - 8);
            if (sVar3 < p[-4]) break;
            if (p[-4] < sVar3) { found = true; break; }   // LAB_14001f7c2
            int16_t nodeS = *reinterpret_cast<int16_t*>(reinterpret_cast<int64_t>(p) + lVar11);
            if (nodeS < *p) break;
            if (*p < nodeS) { found = true; break; }
            uint8_t b1 = *reinterpret_cast<uint8_t*>(lVar8 + 0x10 + cur);
            uint8_t b2 = *reinterpret_cast<uint8_t*>(lVar8 + 0x10 + reinterpret_cast<int64_t>(param_3));
            if (b1 < b2) break;
            if (b1 != b2) { found = true; break; }
            iVar10++; lVar8++; p++;
            if (3 < iVar10) { found = true; break; }
        }
        if (found) { *param_2 = cur; return param_2; }
        // descend right, or insert as right child
        int64_t r = *reinterpret_cast<int64_t*>(cur + 0x28);
        if (r == 0) {
            newNode = TradeWork_impl_1f850(0, param_3, cur);
            *reinterpret_cast<int64_t*>(lVar5 + 0x28) = newNode;
            TradeWork_impl_4b40(param_1, &cur);
            *reinterpret_cast<int*>(param_1 + 1) = (int)param_1[1] + 1;
            *param_2 = newNode;
            return param_2;
        }
        node = r;                                 // continue outer loop with right child
    }
}

// ============================================================
// 0x14001fc00 — AVL find keyed by the 6-int tile descriptor. Returns the matching node
// or 0. 1:1 port of FUN_14001fc00 (param_1 = root holder, param_2 = key src).
// ============================================================
extern "C" int64_t TradeWork_impl_1fc00(int64_t* param_1, int64_t param_2) {
    int64_t node = *param_1;
    for (;;) {
        if (node == 0) return 0;
        int  iVar7 = 0;
        int64_t lVar6 = 0;
        int64_t lVar8 = node - param_2;
        int16_t* p = reinterpret_cast<int16_t*>(param_2 + 8);
        bool descendedLeft = false;
        for (;;) {                                 // first loop
            int16_t sVar3 = *reinterpret_cast<int16_t*>(lVar8 - 8 + reinterpret_cast<int64_t>(p));
            if (p[-4] < sVar3) { node = *reinterpret_cast<int64_t*>(node + 0x20); descendedLeft = true; break; }
            if (sVar3 < p[-4]) break;
            if (*p < *reinterpret_cast<int16_t*>(lVar8 + reinterpret_cast<int64_t>(p))) {
                node = *reinterpret_cast<int64_t*>(node + 0x20); descendedLeft = true; break;
            }
            if (*reinterpret_cast<int16_t*>(lVar8 + reinterpret_cast<int64_t>(p)) < *p) break;
            {
                uint8_t b1 = *reinterpret_cast<uint8_t*>(lVar6 + 0x10 + param_2);
                uint8_t b2 = *reinterpret_cast<uint8_t*>(lVar6 + 0x10 + node);
                if (b1 < b2) { node = *reinterpret_cast<int64_t*>(node + 0x20); descendedLeft = true; break; }
                if (b1 != b2) break;
            }
            iVar7++; lVar6++; p++;
            if (iVar7 >= 4) break;
        }
        if (descendedLeft) continue;
        iVar7 = 0;
        p = reinterpret_cast<int16_t*>(param_2 + 8);
        lVar6 = 0;
        for (;;) {                                 // second loop
            int16_t sVar3 = *reinterpret_cast<int16_t*>(reinterpret_cast<int64_t>(p) + lVar8 - 8);
            if (sVar3 < p[-4]) break;
            if (p[-4] < sVar3) return node;
            int16_t nodeS = *reinterpret_cast<int16_t*>(reinterpret_cast<int64_t>(p) + lVar8);
            if (nodeS < *p) break;
            if (*p < nodeS) return node;
            uint8_t b1 = *reinterpret_cast<uint8_t*>(lVar6 + 0x10 + node);
            uint8_t b2 = *reinterpret_cast<uint8_t*>(lVar6 + 0x10 + param_2);
            if (b1 < b2) break;
            if (b1 != b2) return node;
            iVar7++; lVar6++; p++;
            if (3 < iVar7) return node;
        }
        node = *reinterpret_cast<int64_t*>(node + 0x28);   // descend right
    }
}

// ============================================================
// 0x1400b0d80 — tile-slot manager: find/grow a free 0x60-byte slot in the renderer's
// entry array (param_1+0x1f0 base, +0x1f8 count), write the tile entry, tessellate it
// via 0x1400ace40. Returns the slot index (or -1 if param_8/target is null). 1:1 port
// of TradeWork_impl_0d80 (19 args; param_18 is unused/dead).
// ============================================================
extern "C" int TradeWork_impl_0d80(int64_t param_1, int param_2, int param_3, int param_4,
                                   int param_5, uint8_t param_6, uint32_t param_7, int64_t param_8,
                                   uint8_t param_9, uint32_t param_10, uint32_t param_11,
                                   uint32_t param_12, uint32_t param_13, uint32_t param_14,
                                   char param_15, int param_16, int param_17,
                                   uint64_t /*param_18 unused*/, uint8_t param_19) {
    if (param_8 == 0) return -1;

    if (!s_fa80) s_fa80 = reinterpret_cast<Fa80_t>(ResolveAddr(Module::Exe, 0x14001fa80));

    int iVar3 = 0;
    int iVar2 = *reinterpret_cast<int*>(param_1 + 0x1f8);          // count
    bool grew = false;
    if (0 < iVar2) {
        int* piVar1 = reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 0x1f0) + 0x28);
        do {
            if (*piVar1 < 0) {                                    // free slot marker
                if (-1 < iVar3) { grew = false; goto haveSlot; }
                break;
            }
            iVar3++;
            piVar1 += 0x18;                                       // stride 0x60 bytes
        } while (iVar3 < iVar2);
    }
    // no free slot -> grow the array by 1 from a zeroed template (entry+0x28 = -1)
    {
        uint8_t tmpl[0x60];
        std::memset(tmpl, 0, sizeof(tmpl));
        *reinterpret_cast<int32_t*>(tmpl + 0x28) = -1;
        if (s_fa80) s_fa80(reinterpret_cast<int64_t*>(param_1 + 0x1f0),
                           *reinterpret_cast<int*>(param_1 + 0x1f8) + 1, tmpl);
        EnsureMem();
        if (s_memFree) { s_memFree(*reinterpret_cast<void**>(tmpl + 0x18));
                         s_memFree(*reinterpret_cast<void**>(tmpl + 0x08)); }
        iVar3 = iVar2;
        grew = true;
    }
haveSlot:
    (void)grew;
    iVar2 = (*reinterpret_cast<int*>(param_1 + 0xc0) - *reinterpret_cast<int*>(param_1 + 0xc4)) >> 1;
    int64_t entry = (int64_t)iVar3 * 0x60 + *reinterpret_cast<int64_t*>(param_1 + 0x1f0);
    *reinterpret_cast<int64_t*>(entry + 0x00) = param_8;
    *reinterpret_cast<int*>(entry + 0x28) = iVar3;
    *reinterpret_cast<int*>(entry + 0x2c) = iVar2 + param_2;
    *reinterpret_cast<int*>(entry + 0x30) = iVar2 + param_3;
    *reinterpret_cast<int*>(entry + 0x34) = iVar2 + param_4;
    *reinterpret_cast<int*>(entry + 0x38) = param_5 + iVar2;
    *reinterpret_cast<uint8_t*>(entry + 0x3c) = param_6;
    *reinterpret_cast<uint32_t*>(entry + 0x40) = param_7;
    *reinterpret_cast<uint8_t*>(entry + 0x44) = param_9;
    *reinterpret_cast<uint32_t*>(entry + 0x48) = param_10;
    *reinterpret_cast<uint32_t*>(entry + 0x4c) = param_11;
    *reinterpret_cast<uint32_t*>(entry + 0x50) = param_12;
    *reinterpret_cast<uint32_t*>(entry + 0x54) = param_13;
    *reinterpret_cast<uint32_t*>(entry + 0x58) = param_14;
    *reinterpret_cast<uint8_t*>(entry + 0x5c) = 1;
    *reinterpret_cast<uint8_t*>(entry + 0x5d) = param_19;
    if (!s_ace40) s_ace40 = reinterpret_cast<Ace40_t>(ResolveAddr(Module::Exe, 0x1400ace40));
    if (s_ace40) s_ace40(param_1, iVar3, param_15, param_16, param_17);
    return iVar3;
}

// ============================================================
// 0x140055430 — HighlightAdd. Builds the radius-highlight rectangle around centerCell
// and submits it to the tile renderer (gameWorld+0x110). param_1 = radiusComponent
// (needs +0x20 highlight target != null, +0x28 radius). Returns highlight slot or -1.
// 1:1 port of FUN_140055430 (b0d80 args disasm-verified; uvScale = g_fUVScale = 1.0f).
// ============================================================
extern "C" int Player_HighlightAdd_55430(int64_t param_1, int param_2) {
    int64_t target = *reinterpret_cast<int64_t*>(param_1 + 0x20);
    if (target == 0) return -1;

    int64_t gameWorld = reinterpret_cast<int64_t>(*g_pGameWorldSingleton);
    int mapSize = **reinterpret_cast<int**>(gameWorld + 0xf8);
    int row = param_2 / mapSize;
    int col = param_2 % mapSize;
    int r   = *reinterpret_cast<int*>(param_1 + 0x28);

    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    uint32_t uv = *reinterpret_cast<uint32_t*>(&uvScale);

    return TradeWork_impl_0d80(gameWorld + 0x110,
                               col - r, row - r, r + col, r + row,
                               0xff, 0, target, 0,
                               uv, 0, 0, uv, uv,
                               1, r - 2, r + 2, 0, 1);
}
