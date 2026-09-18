// code/Game/Component/citizenai_housing_idle.cpp
//
// Brick 3b handler wave (FindHousing + Idle) — faithful 1:1 ports transcribed from the
// Ghidra decompile, offset-by-offset, of the two "non-pushing" CitizenAIComponent state
// handlers and their entire shared deep AVL/BST helper subtree:
//
//   0x14010b340  CitizenAI_State_FindHousing  (state id 2)  -> helper_7550
//   0x14010b2d0  CitizenAI_State_Idle         (state id 9)  -> helper_7830
//   0x1400f7550  CitizenAI_helper_7550        (candidate-roll over the per-state remembered-
//                                              target trees; calls CanFindHousing, 3f010,
//                                              79b0, Random::Rand)
//   0x1400f7830  CitizenAI_helper_7830        (Idle: probe one specific tree key, else 7550)
//   0x1400f79b0  CitizenAI_helper_sub_79b0    (the AVL walker — builds a transient result tree
//                                              via 7c90/8000, walks it via 7b60, frees via e320)
//   0x1400f7b60  CitizenAI_helper_sub_79b0_sub_7b60  (in-order walk of the result tree;
//                                              dispatches each payload's vtable +0x48 / +0x20)
//   0x1400f7c90  CitizenAI_helper_sub_79b0_sub_7c90  (cost-keyed AVL insert, slope-aware cost)
//   0x1400f8000  CitizenAI_helper_sub_79b0_sub_8000  (cost-keyed AVL insert, simple cost field)
//   0x14003f010  FUN_14003f010                (memset-0xFF leaf; pure)
//   0x14000e360  TradeDescription_impl_e360   (AVL rebalance walker; calls registered rotations
//                                              e450/e580) — UNREGISTERED elsewhere, ported here.
//
// FindHousing/Idle are "non-pushing": they do NOT build/push an Action onto the citizen action
// stack themselves. They invoke the helper (which may itself push via the payload-object virtual
// at vtable+0x20), then return CONCAT71(citizen>>8, 0 < (*(citizen+0x278)+0x60) + 1). i.e. the
// boolean low byte = "the character-component action-stack-depth field at +0x60 is >= 0" — which
// is essentially always true (it is -1 only when uninitialised). They do NOT stamp cooldowns.
//
// SELF CHAIN: param_1 = CitizenAIComponent. self/+0x38 = sibling CitizenComponent IBP.
//   citizen = *self. citizen+0x278 = CharacterComponent (its +0x60 (int) = action depth used by
//   the return expr). citizen+0x1f0 = the citizen's "home/owner key" (a building-instance ptr,
//   compared against tree keys and against gameWorld+0xb0 +0x30/+0x38). The trees searched live
//   on the GameWorld-derived object passed as helper param_1 = *(g_pGameWorldSingleton + 0x118).
//
// TREE-ROOT ARRAY (helper param_1, base = *(g_pGameWorldSingleton+0x118)):
//   a flat array of {key, root}-ish 0x10-byte records. helper_7550 indexes it as
//   *(param_1 + (stateByte+3)*0x10) to fetch the AVL root for a candidate-state code, then
//   follows the node +0x18 (left) chain to its leftmost node before walking. helper_7830 reads
//   the secondary BST at param_1+0x120 (a {key@+0, valueRoot@+8, left@+0x20, right@+0x28}-style
//   ordered tree) keyed by the citizen's +0x1f0 owner key.
//
// === RECOVERED AVL NODE STRUCT (the transient result tree built by 7c90/8000, walked by 7b60,
//     freed by 79b0 via e320, rebalanced by e360) — 0x30 bytes, MemAlloc(0x30,8):
//     +0x00  void*   payload         (entity IBP / candidate object; node+0)
//     +0x08  int32   costKey         (sort key: path-cost / id; node[1] low dword)
//     +0x10  void*   parent
//     +0x18  void*   left
//     +0x20  void*   right
//     +0x28  int16   balance         (right.height - left.height)
//     +0x2a  int16   height          (max(childHeights)+1)
//   7c90 secondary tiebreak compares (payload+0x20) int field; 8000 uses costKey only.
//   7b60 reads (payload+0x2c) as a per-pass visit-stamp vs (helperCtx+0x8); dispatches the
//   payload object's vtable: (+0x48)() [refresh], then (+0x20)(payload, &citizenIBP, ctx+0x130)
//   returning int {0 skip, 1 mark-visited, 2 success-stop}.
//
// SOURCE TREE node layout (the per-state remembered-target trees at param_1[(b+3)*0x10] and the
//   param_1+0x120 BST) — read-only here:
//     +0x00 key ; +0x08 valueSubtreeRoot ; +0x10 ??? ; +0x18 left ; +0x20 right
//   (7550 walks +0x18 to leftmost; 7830 BST: <key -> +0x20(node[4]), >key -> +0x28(node[5]),
//    ==key -> value at +0x08(node[1]) then +0x18(node[3]) to leftmost.)
//
// REGISTERED exe sub-callees (resolved via ResolveAddr; bodies NOT duplicated):
//   0x140115be0  CitizenComponent_CanFindHousing   (reg_component_gap_A.cpp)
//   0x14000e320  WorkBase_TreeNodeFree_e320         (reg_work_base.cpp)  — frees node +0x18/+0x20
//   0x14009f810  Pf_IsDstReachable_f810/BuildStructure_impl_f810 (reg_pf_roots_route.cpp)
//   0x140132010  WorkImpl_Sqrtf_132010              (reg_work_impl2.cpp) — sqrtf
//   0x14000e450  Pf_AvlRotateB_e450                 (reg_pf_avl.cpp)     — called by e360
//   0x14000e580  Pf_AvlRotateB_e580                 (reg_pf_avl.cpp)     — called by e360
//
// e320 REG VERDICT: REGISTERED (standalone/reg_work_base.cpp:42). Confirmed by grep. Not re-ported.
//
// Engine externals (engine_funcs.h): fn_InternalBasePtr_CheckPtr, fn_InternalBasePtr_Ctor,
//   fn_MemAlloc(+g_pMemAllocSingleton), fn_MemFreeStatic, fn_Random_Rand, g_pGameWorldSingleton.
//


#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// 2c-B probe: the last BuildStructure work spawned by State8Enter (buildcomponent_state8.cpp).
extern "C" void* g_pBuildStructureWork;
// Same probe idiom, pointed at the crop field's PlantArea. The field posts its work and
// the scheduler assigns it an id, yet PlantArea::Execute is never called — so the open
// question is whether the citizen claim walk ever REACHES that work, or whether it is
// reached and rejected. Those need different fixes and are indistinguishable from
// outside. Set by CF_PostWork (cropfieldcomponent_states.cpp).
extern "C" void* g_pCropPlantWork;

namespace {

// --- registered exe sub-callees ---
typedef char     (*fn_canHouse_t)(void* citizen);                                       // 0x140115be0
typedef void     (*fn_e320_t)(void* root, void* child);                                 // 0x14000e320 (free)
typedef int      (*fn_f810_t)(int* mapGrid, uint32_t fromCell, uint32_t toCell, char addCache); // 0x14009f810
typedef float    (*fn_sqrtf_t)(float v);                                                // 0x140132010
typedef int64_t  (*fn_rot_t)(int64_t* rootSlot, int64_t node);                          // 0x14000e450 / 0x14000e580

static fn_canHouse_t s_canHouse = nullptr;
static fn_e320_t     s_e320     = nullptr;
static fn_f810_t     s_f810     = nullptr;
static fn_sqrtf_t    s_sqrtf    = nullptr;
static fn_rot_t      s_rot_e450 = nullptr;
static fn_rot_t      s_rot_e580 = nullptr;

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_canHouse = reinterpret_cast<fn_canHouse_t>(ResolveAddr(Module::Exe, 0x140115be0));
    s_e320     = reinterpret_cast<fn_e320_t>(ResolveAddr(Module::Exe, 0x14000e320));
    s_f810     = reinterpret_cast<fn_f810_t>(ResolveAddr(Module::Exe, 0x14009f810));
    s_sqrtf    = reinterpret_cast<fn_sqrtf_t>(ResolveAddr(Module::Exe, 0x140132010));
    s_rot_e450 = reinterpret_cast<fn_rot_t>(ResolveAddr(Module::Exe, 0x14000e450));
    s_rot_e580 = reinterpret_cast<fn_rot_t>(ResolveAddr(Module::Exe, 0x14000e580));
    s_init = true;
}

static inline void CheckPtr(void* p) { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline uintptr_t GW() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }

// A plausible heap node pointer: non-null, not the -1 "invalid IBP" sentinel, and canonical
// user-space (>= 64K, < 2^47). Used to fence the AVL rebalance walk (Helper_e360) against a
// corrupt/invalid child or parent link — observed in standalone when an idle citizen's work-
// candidate tree picks up a tool-placed building whose inert-factory component yields a -1
// field, which fed 0xffffffffffffffff into the tree and faulted on deref (READ of -1). When
// every link is valid this is a pure no-op; when one isn't, the walk treats it as absent
// rather than dereferencing it. The transient tree is freed immediately after, so a slightly
// less-balanced tree in that rare corrupt case only means a marginally different candidate pick.
static inline bool ValidNode(int64_t p) {
    uint64_t u = static_cast<uint64_t>(p);
    return u >= 0x10000ULL && u < 0x0000800000000000ULL;
}

static inline void* NodeAlloc() {
    return fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8) : nullptr;
}

// Idle work-search counters, read by standalone/road_probe.cpp. The road wave needs to
// tell "no builder ever searched" from "a builder searched and its bucket had nothing it
// would take" -- those look identical from the map.
extern "C" {
long long g_idleSearchCalls = 0;
long long g_idleSearchByBuilder = 0;
long long g_idleBuilderBucketHit = 0;
long long g_idleBuilderTookWork = 0;
}

// Forward decls for the helper subtree.
void          Helper_e360(int64_t* unused, int64_t* rootSlot);                          // 0x14000e360
void          Helper_sub_8000(void* p1, void* keyHolder, int64_t* srcRootSlot, int64_t* dstRootSlot);   // 0x1400f8000
void          Helper_sub_7c90(void* p1, void* keyHolder2, void* keyHolder3, int64_t* srcRootSlot, int64_t* dstRootSlot); // 0x1400f7c90
uint8_t       Helper_sub_7b60(int64_t ctx, void** citizenHolder, int64_t* dstRootSlot); // 0x1400f7b60
uint64_t      Helper_sub_79b0(int64_t param_1, void** param_2, void** param_3, int64_t param_4, char param_5, char param_6); // 0x1400f79b0
uint64_t      Helper_7550(int64_t param_1, void** param_2);                             // 0x1400f7550

// ============================================================
// 0x14003f010 — memset(dst, byteVal, count). Pure leaf; just call memset.
// ============================================================
void* Helper_3f010(void* dst, uint64_t byteVal, uint64_t count) {
    // Faithful to behaviour: fills `count` bytes with (byte)byteVal. The exe variant has
    // hand-unrolled 64-byte and 8-byte loops + a LOCK fence for large fills; the observable
    // effect is identical to memset.
    unsigned char* p = reinterpret_cast<unsigned char*>(dst);
    unsigned char b = static_cast<unsigned char>(byteVal);
    for (uint64_t i = 0; i < count; ++i) p[i] = b;
    return dst;
}

// ============================================================
// 0x14000e360 — AVL rebalance walk up the right-spine of an insertion path.
//   param_2 = &(node ptr); walks node = node->parent (+0x10) updating height/balance and
//   rotating via e450/e580 until null. param_1 is the tree-root slot passed to the rotations.
// ============================================================
void Helper_e360(int64_t* rootSlot, int64_t* param_2) {
    int64_t node = *param_2;
    while (ValidNode(node)) {
        int64_t n = *param_2;
        if (!ValidNode(n)) break;                                // corrupt slot -> stop the walk
        int64_t parent = *reinterpret_cast<int64_t*>(n + 0x10);  // next up
        int64_t left  = *reinterpret_cast<int64_t*>(n + 0x18);
        int64_t right = *reinterpret_cast<int64_t*>(n + 0x20);
        short hL = ValidNode(left)  ? static_cast<short>(*reinterpret_cast<short*>(left  + 0x2a) + 1) : 0;
        short hR = ValidNode(right) ? static_cast<short>(*reinterpret_cast<short*>(right + 0x2a) + 1) : 0;
        short h = (hR < hL) ? hL : hR;
        *reinterpret_cast<short*>(n + 0x2a) = h;
        *reinterpret_cast<short*>(n + 0x28) = static_cast<short>(hR - hL);

        if (static_cast<short>(hR - hL) > 1) {
            int64_t rc = *reinterpret_cast<int64_t*>(n + 0x20);
            if (ValidNode(rc)) {
                // right-heavy: if right child is left-heavy, rotate it first (e580), then e450.
                if (*reinterpret_cast<short*>(rc + 0x28) < 1 && *reinterpret_cast<short*>(rc + 0x28) < 0) {
                    if (s_rot_e580) s_rot_e580(rootSlot, rc);
                    n = *param_2;
                }
                if (ValidNode(n) && s_rot_e450) s_rot_e450(rootSlot, n);
            }
        }
        n = *param_2;
        if (ValidNode(n) && *reinterpret_cast<short*>(n + 0x28) < -1) {
            int64_t lc = *reinterpret_cast<int64_t*>(n + 0x18);
            if (ValidNode(lc)) {
                // left-heavy: if left child is right-heavy, rotate it first (e450), then e580.
                if (*reinterpret_cast<short*>(lc + 0x28) > -1 && *reinterpret_cast<short*>(lc + 0x28) > 0) {
                    if (s_rot_e450) s_rot_e450(rootSlot, lc);
                    n = *param_2;
                }
                if (ValidNode(n) && s_rot_e580) s_rot_e580(rootSlot, n);
            }
        }
        *param_2 = parent;
        node = parent;
    }
}

// ============================================================
// 0x1400f8000 — insert each source-tree node into the dst AVL keyed by (payload+0x24).
//   param_1 unused-ish holder; param_2 = &(skip-key) (when non-null, only inserts the node
//   whose payload+0x10 matches *param_2); param_3 = &srcRoot; param_4 = &dstRoot.
//   Source tree traversed in-order via node +0x10(parent)/+0x18(left)/+0x20(right).
// ============================================================
void Helper_sub_8000(void* param_1, void* param_2, int64_t* param_3, int64_t* param_4) {
    if (*param_3 == 0) return;
    int64_t cur = *param_3;
    void* local_res8 = param_1;
    do {
        bool doInsert = false;
        if (*reinterpret_cast<int64_t*>(param_2) == 0) {
            doInsert = true;
        } else {
            CheckPtr(reinterpret_cast<void*>(cur + 8));
            int64_t payload = *reinterpret_cast<int64_t*>(cur + 8);
            int64_t k = *reinterpret_cast<int64_t*>(payload + 0x10);
            CheckPtr(param_2);
            if (k == *reinterpret_cast<int64_t*>(param_2)) doInsert = true;
        }

        if (doInsert) {
            CheckPtr(reinterpret_cast<void*>(cur + 8));
            int64_t payload = *reinterpret_cast<int64_t*>(cur + 8);   // local_28
            int costKey = *reinterpret_cast<int*>(payload + 0x24);    // iVar6 / local_20
            bool inserted = true;   // false on duplicate (JG/JGE 0x81fc -> skip INC + rebalance)
            int64_t existing = *param_4;
            if (existing == 0) {
                int64_t* nn = reinterpret_cast<int64_t*>(NodeAlloc());
                if (nn == nullptr) { *param_4 = 0; }
                else {
                    nn[0] = payload;
                    *reinterpret_cast<int*>(nn + 1) = costKey;
                    nn[2] = 0; nn[3] = 0; nn[4] = 0;
                    *reinterpret_cast<int*>(nn + 5) = 0;
                    *param_4 = reinterpret_cast<int64_t>(nn);
                }
            } else {
                bool descendLeft = false;
                for (;;) {
                    int64_t self = existing;
                    // Track the descent node for the post-insert rebalance. The originals
                    // (0x1400f8000 / 0x1400f7c90) do `local_res8 = pIVar3` at the top of this
                    // loop, so Helper_e360 walks UP from the insertion node. The port left
                    // local_res8 pinned at param_1 (the citizen), so e360 read citizen fields
                    // as AVL links and faulted on the -1 they held (READ of -1 crash once an
                    // idle citizen's candidate tree grew past its root). Restore the update.
                    local_res8 = reinterpret_cast<void*>(self);
                    if (*reinterpret_cast<int*>(self + 8) <= costKey) {
                        // self.key <= costKey
                        if (costKey <= *reinterpret_cast<int*>(self + 8)) {
                            // equal key: tiebreak on entity field +0x20
                            CheckPtr(reinterpret_cast<void*>(payload + 0x20));  // &local_28 deref guard
                            int iv = *reinterpret_cast<int*>(payload + 0x20);
                            CheckPtr(reinterpret_cast<void*>(self));
                            int sv = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self) + 0x20);
                            if (iv < sv) { descendLeft = true; }
                        }
                        if (!descendLeft) {
                            if (costKey <= *reinterpret_cast<int*>(self + 8)) {
                                if (costKey < *reinterpret_cast<int*>(self + 8)) { inserted = false; break; }
                                CheckPtr(reinterpret_cast<void*>(self));
                                int sv2 = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self) + 0x20);
                                int iv2 = *reinterpret_cast<int*>(payload + 0x20);
                                if (iv2 <= sv2) { inserted = false; break; }
                            }
                            // go right (+0x20)
                            int64_t rc = *reinterpret_cast<int64_t*>(self + 0x20);
                            if (rc == 0) {
                                int64_t* nn = reinterpret_cast<int64_t*>(NodeAlloc());
                                if (nn == nullptr) { *reinterpret_cast<int64_t*>(self + 0x20) = 0; }
                                else {
                                    nn[0] = payload;
                                    *reinterpret_cast<int*>(nn + 1) = costKey;
                                    nn[2] = self; nn[3] = 0; nn[4] = 0;
                                    *reinterpret_cast<int*>(nn + 5) = 0;
                                    *reinterpret_cast<int64_t*>(self + 0x20) = reinterpret_cast<int64_t>(nn);
                                }
                                Helper_e360(param_4, reinterpret_cast<int64_t*>(&local_res8));
                                break;
                            }
                            existing = rc;
                            continue;
                        }
                    }
                    // go left (+0x18)
                    {
                        int64_t lc = *reinterpret_cast<int64_t*>(self + 0x18);
                        if (lc == 0) {
                            int64_t* nn = reinterpret_cast<int64_t*>(NodeAlloc());
                            if (nn != nullptr) {
                                nn[0] = payload;
                                *reinterpret_cast<int*>(nn + 1) = costKey;
                                nn[2] = self; nn[3] = 0; nn[4] = 0;
                                *reinterpret_cast<int*>(nn + 5) = 0;
                            }
                            *reinterpret_cast<int64_t*>(self + 0x18) = reinterpret_cast<int64_t>(nn);
                            Helper_e360(param_4, reinterpret_cast<int64_t*>(&local_res8));
                            break;
                        }
                        existing = lc;
                        descendLeft = false;
                        continue;
                    }
                }
            }
            if (inserted) *reinterpret_cast<int*>(reinterpret_cast<char*>(param_4) + 8) = *reinterpret_cast<int*>(reinterpret_cast<char*>(param_4) + 8) + 1;
        }
        // (else: skip-key set and key didn't match -> no insert; matches the decompile's
        //  `if (key == *param_2) goto LAB_805c` which doInsert already captured above.)

        // in-order successor in the source tree (parent=+0x10, left=+0x18, right=+0x20)
        int64_t rc = *reinterpret_cast<int64_t*>(cur + 0x20);
        int64_t next;
        if (rc == 0) {
            int64_t up = *reinterpret_cast<int64_t*>(cur + 0x10);
            if (up != 0) {
                if (*reinterpret_cast<int64_t*>(up + 0x18) == cur) { next = up; goto advance; }
                while (up != 0 && *reinterpret_cast<int64_t*>(up + 0x20) == cur) {
                    cur = up;
                    up = *reinterpret_cast<int64_t*>(up + 0x10);
                }
            }
            next = *reinterpret_cast<int64_t*>(cur + 0x10);
        } else {
            int64_t n2 = rc;
            while (*reinterpret_cast<int64_t*>(n2 + 0x18) != 0) n2 = *reinterpret_cast<int64_t*>(n2 + 0x18);
            next = n2;
        }
    advance:
        cur = next;
    } while (cur != 0);
}

// ============================================================
// 0x1400f7c90 — like 8000 but cost is computed (slope/distance via f810+sqrtf) and the source
//   node is gated on (payload+0x10 == *param_3 skip-key) OR (param_3 holds null). param_1 holder,
//   param_2 = &citizenIBP (for the citizen model cell), param_3 = &skipKey, param_4 = &srcRoot,
//   param_5 = &dstRoot.
// ============================================================
void Helper_sub_7c90(void* param_1, void* param_2, void* param_3, int64_t* param_4, int64_t* param_5) {
    int* mapGrid = *reinterpret_cast<int**>(GW() + 0xf8);
    void* local_res8 = param_1;

    // citizen model cell (clamped row*w+col) -> uVar11
    CheckPtr(param_2);
    int64_t cz = *reinterpret_cast<int64_t*>(param_2);
    CheckPtr(reinterpret_cast<void*>(cz + 0x278));
    int64_t ch = *reinterpret_cast<int64_t*>(cz + 0x278);
    CheckPtr(reinterpret_cast<void*>(ch + 0x68));
    int64_t model = *reinterpret_cast<int64_t*>(ch + 0x68);
    int w = *mapGrid;
    int col = static_cast<int>(*reinterpret_cast<float*>(model + 0x158) + static_cast<float>(w >> 1));
    int row = static_cast<int>(*reinterpret_cast<float*>(model + 0x15c) + static_cast<float>(w >> 1));
    if (col < 0) col = 0; if (w <= col) col = w - 1;
    if (row < 0) row = 0; if (w <= row) row = w - 1;
    uint32_t fromCell = static_cast<uint32_t>(w * row + col);

    int64_t cur = *param_4;
    while (cur != 0) {
        bool doInsert = false;
        if (*reinterpret_cast<int64_t*>(param_3) == 0) {
            doInsert = true;
        } else {
            CheckPtr(reinterpret_cast<void*>(cur + 8));
            int64_t key = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(cur + 8) + 0x10);
            CheckPtr(param_3);
            if (key == *reinterpret_cast<int64_t*>(param_3)) doInsert = true;
        }

        if (doInsert) {
            CheckPtr(reinterpret_cast<void*>(cur + 8));
            uint32_t toCell = *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(cur + 8) + 0x28);
            int cost = s_f810 ? s_f810(mapGrid, fromCell, toCell, '\x01') : -1;
            if (cost < 0) {
                // unreachable via path graph -> euclidean fallback on grid coords
                int gw = *mapGrid;
                int dr = static_cast<int>(fromCell) / gw - static_cast<int>(toCell) / gw;
                int dc = static_cast<int>(fromCell) % gw - static_cast<int>(toCell) % gw;
                float d = s_sqrtf ? s_sqrtf(static_cast<float>(dr * dr + dc * dc)) : 0.0f;
                cost = static_cast<int>(d) + gw;
            }
            int64_t payload = *reinterpret_cast<int64_t*>(cur + 8);   // local_38
            int costKey = cost;                                       // local_30
            bool inserted = true;    // false on duplicate -> skip the count INC + rebalance
            int64_t existing = *param_5;
            if (existing == 0) {
                int64_t* nn = reinterpret_cast<int64_t*>(NodeAlloc());
                if (nn == nullptr) { *param_5 = 0; }
                else {
                    nn[0] = payload; *reinterpret_cast<int*>(nn + 1) = costKey;
                    nn[2] = 0; nn[3] = 0; nn[4] = 0; *reinterpret_cast<int*>(nn + 5) = 0;
                    *param_5 = reinterpret_cast<int64_t>(nn);
                }
            } else {
                bool descendLeft = false;
                for (;;) {
                    int64_t self = existing;
                    // Track the descent node for the post-insert rebalance. The originals
                    // (0x1400f8000 / 0x1400f7c90) do `local_res8 = pIVar3` at the top of this
                    // loop, so Helper_e360 walks UP from the insertion node. The port left
                    // local_res8 pinned at param_1 (the citizen), so e360 read citizen fields
                    // as AVL links and faulted on the -1 they held (READ of -1 crash once an
                    // idle citizen's candidate tree grew past its root). Restore the update.
                    local_res8 = reinterpret_cast<void*>(self);
                    if (*reinterpret_cast<int*>(self + 8) <= costKey) {
                        if (costKey <= *reinterpret_cast<int*>(self + 8)) {
                            CheckPtr(reinterpret_cast<void*>(payload + 0x20));
                            int iv = *reinterpret_cast<int*>(payload + 0x20);
                            CheckPtr(reinterpret_cast<void*>(self));
                            int sv = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self) + 0x20);
                            if (iv < sv) descendLeft = true;
                        }
                        if (!descendLeft) {
                            if (costKey <= *reinterpret_cast<int*>(self + 8)) {
                                if (costKey < *reinterpret_cast<int*>(self + 8)) { inserted = false; break; }
                                CheckPtr(reinterpret_cast<void*>(self));
                                int sv2 = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self) + 0x20);
                                int iv2 = *reinterpret_cast<int*>(payload + 0x20);
                                if (iv2 <= sv2) { inserted = false; break; }
                            }
                            int64_t rc = *reinterpret_cast<int64_t*>(self + 0x20);
                            if (rc == 0) {
                                int64_t* nn = reinterpret_cast<int64_t*>(NodeAlloc());
                                if (nn == nullptr) { *reinterpret_cast<int64_t*>(self + 0x20) = 0; }
                                else {
                                    nn[0] = payload; *reinterpret_cast<int*>(nn + 1) = costKey;
                                    nn[2] = self; nn[3] = 0; nn[4] = 0; *reinterpret_cast<int*>(nn + 5) = 0;
                                    *reinterpret_cast<int64_t*>(self + 0x20) = reinterpret_cast<int64_t>(nn);
                                }
                                Helper_e360(param_5, reinterpret_cast<int64_t*>(&local_res8));
                                break;
                            }
                            existing = rc;
                            continue;
                        }
                    }
                    {
                        int64_t self2 = existing;
                        int64_t lc = *reinterpret_cast<int64_t*>(self2 + 0x18);
                        if (lc == 0) {
                            int64_t* nn = reinterpret_cast<int64_t*>(NodeAlloc());
                            if (nn != nullptr) {
                                nn[0] = payload; *reinterpret_cast<int*>(nn + 1) = costKey;
                                nn[2] = self2; nn[3] = 0; nn[4] = 0; *reinterpret_cast<int*>(nn + 5) = 0;
                            }
                            *reinterpret_cast<int64_t*>(self2 + 0x18) = reinterpret_cast<int64_t>(nn);
                            Helper_e360(param_5, reinterpret_cast<int64_t*>(&local_res8));
                            break;
                        }
                        existing = lc;
                        descendLeft = false;
                        continue;
                    }
                }
            }
            if (inserted) *reinterpret_cast<int*>(reinterpret_cast<char*>(param_5) + 8) = *reinterpret_cast<int*>(reinterpret_cast<char*>(param_5) + 8) + 1;
        }

        // in-order successor in src tree
        int64_t rc = *reinterpret_cast<int64_t*>(cur + 0x20);
        int64_t next;
        if (rc == 0) {
            next = *reinterpret_cast<int64_t*>(cur + 0x10);
            if (next != 0) {
                if (*reinterpret_cast<int64_t*>(next + 0x18) != cur) {
                    int64_t up = next;
                    while (up != 0 && *reinterpret_cast<int64_t*>(up + 0x20) == cur) {
                        cur = up;
                        up = *reinterpret_cast<int64_t*>(up + 0x10);
                    }
                    next = *reinterpret_cast<int64_t*>(cur + 0x10);
                }
            }
        } else {
            int64_t n2 = rc;
            while (*reinterpret_cast<int64_t*>(n2 + 0x18) != 0) n2 = *reinterpret_cast<int64_t*>(n2 + 0x18);
            next = n2;
        }
        cur = next;
    }
}

// ============================================================
// 0x1400f7b60 — in-order walk of the dst result tree (param_3 = &dstRoot). For each node whose
//   payload visit-stamp (payload+0x2c) != ctx.stamp (param_1+8): call payload vtable +0x48()
//   (refresh), then payload vtable +0x20(payload, &citizen, ctx+0x130) -> int. 2 => stop/success
//   (return 1). 1 => mark visited (payload+0x2c = ctx.stamp). 0 => skip. Walk uses node +0x10
//   parent / +0x18 left / +0x20 right.
// ============================================================
uint8_t Helper_sub_7b60(int64_t param_1, void** param_2, int64_t* param_3) {
    int64_t cur = *param_3;
    if (cur == 0) return 0;
    // descend to leftmost (+0x18)
    while (*reinterpret_cast<int64_t*>(cur + 0x18) != 0) cur = *reinterpret_cast<int64_t*>(cur + 0x18);

    while (cur != 0) {
        CheckPtr(reinterpret_cast<void*>(cur));
        int64_t payload = *reinterpret_cast<int64_t*>(cur);
        if (*reinterpret_cast<int*>(payload + 0x2c) != *reinterpret_cast<int*>(param_1 + 8)) {
            CheckPtr(reinterpret_cast<void*>(cur));
            payload = *reinterpret_cast<int64_t*>(cur);
            // vtable +0x48 refresh
            void** vt = *reinterpret_cast<void***>(payload);
            typedef void (*fn48_t)(int64_t);
            reinterpret_cast<fn48_t>(vt[0x48 / 8])(payload);
            CheckPtr(reinterpret_cast<void*>(cur));
            payload = *reinterpret_cast<int64_t*>(cur);
            int64_t citizen = reinterpret_cast<int64_t>(*param_2);  // local_res18[0] = *param_2 (citizen)
            vt = *reinterpret_cast<void***>(payload);
            typedef uint32_t (*fn20_t)(int64_t self, int64_t* citizenHolder, int64_t ctxArg);
            uint32_t r = reinterpret_cast<fn20_t>(vt[0x20 / 8])(payload, &citizen, param_1 + 0x130);
            if (static_cast<int>(r) == 2) return 1;
            if (static_cast<int>(r) == 1) {
                CheckPtr(reinterpret_cast<void*>(cur));
                *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(cur) + 0x2c) = *reinterpret_cast<int*>(param_1 + 8);
            }
        }
        // in-order successor: node +0x20 (right) subtree's leftmost, else climb +0x10 parents
        int64_t rc = *reinterpret_cast<int64_t*>(cur + 0x20);
        if (rc == 0) {
            int64_t up = *reinterpret_cast<int64_t*>(cur + 0x10);
            if (up == 0) { cur = 0; continue; }
            if (*reinterpret_cast<int64_t*>(up + 0x18) == cur) { cur = up; continue; }
            // climb while cur is the right child
            while (up != 0 && *reinterpret_cast<int64_t*>(up + 0x20) == cur) {
                cur = up;
                up = *reinterpret_cast<int64_t*>(up + 0x10);
            }
            cur = *reinterpret_cast<int64_t*>(cur + 0x10);
        } else {
            int64_t n2 = rc;
            while (*reinterpret_cast<int64_t*>(n2 + 0x18) != 0) n2 = *reinterpret_cast<int64_t*>(n2 + 0x18);
            cur = n2;
        }
    }
    return 0;
}

// ============================================================
// 0x1400f79b0 — the AVL walker driver. Builds a transient cost-keyed result tree (local_40)
//   from a source subtree (param_3), then walks it (7b60). param_5 selects the cost mode
//   (7c90 slope-aware vs 8000 plain). param_6 forces the "whole-list" mode (fresh empty key
//   holder). On finish, frees the result tree via e320 + MemFree.
//
//   param_1 = helper ctx (the tree-root array base; +8 = stamp, +0x130 = passed to payload+0x20)
//   param_2 = &citizenIBP
//   param_3 = &srcSubtreeRoot (a node in the per-state remembered-target tree)
//   param_4 = the citizen owner key (citizen+0x1f0)
//   param_5 = cost mode (1 => 7c90, 0 => 8000)
//   param_6 = "all" flag
// ============================================================
// VERIFIED against disassembly (offsets 45-95): srcRoot passed to 8000/7c90 is ALWAYS *param_3
// (the walked source node, [RBP+0x38]); the skip-key holder is the +0x248 gate object on the
// key-match path and a FRESH empty IBP on the common path; the citizen (*param_2) is 7c90's
// param_2 and 7b60's citizen. local_50=[RBP-0x28], local_58=[RBP-0x30].
//   8000(citizen, &skipKeyHolder, &srcRoot, &dstRoot)
//   7c90(citizen, &citizen,      &skipKeyHolder, &srcRoot, &dstRoot)
uint64_t Helper_sub_79b0(int64_t param_1, void** param_2, void** param_3, int64_t param_4,
                         char param_5, char param_6) {
    int64_t local_res20 = param_4;
    CheckPtr(param_2);
    int64_t citizen0 = *reinterpret_cast<int64_t*>(param_2);
    int64_t gate = *reinterpret_cast<int64_t*>(citizen0 + 0x248);   // pIVar5 = *(citizen+0x248)

    int64_t ctorBuf[2] = {0, 0};  // local_res10 (IBP-Ctor scratch + 7b60 citizen holder)
    if (param_6 != '\0') {
        if (fn_InternalBasePtr_Ctor) new (ctorBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        gate = ctorBuf[0];   // fresh empty IBP -> typically 0
    }

    int64_t resultPair[2] = {0, 0};  // local_40 (dstRoot) + local_38 (count)
    int64_t srcRoot = 0;             // [RBP+0x38] = *param_3 (walked source node)
    int64_t local_50 = 0;            // [RBP-0x28]
    int64_t local_58 = 0;            // [RBP-0x30]

    // BuildAndWalk(skipKeyHolder): srcRoot = *param_3; build the cost-keyed dst tree via 7c90
    // (slope cost) or 8000 (plain), then walk it via 7b60. citizen for 7b60 = *param_2.
    auto BuildAndWalk = [&](int64_t skipKeyHolder) -> uint8_t {
        srcRoot = *reinterpret_cast<int64_t*>(param_3);
        if (param_5 != '\0') {
            local_58 = *reinterpret_cast<int64_t*>(param_2);  // citizen (RDX for 7c90)
            local_50 = skipKeyHolder;                          // skip-key gate (R8 for 7c90)
            Helper_sub_7c90(reinterpret_cast<void*>(citizen0), &local_58, &local_50, &srcRoot, &resultPair[0]);
        } else {
            local_50 = skipKeyHolder;                          // skip-key gate (RDX for 8000)
            Helper_sub_8000(reinterpret_cast<void*>(citizen0), &local_50, &srcRoot, &resultPair[0]);
        }
        ctorBuf[0] = *reinterpret_cast<int64_t*>(param_2);     // citizen for 7b60
        return Helper_sub_7b60(param_1, reinterpret_cast<void**>(ctorBuf), &resultPair[0]);
    };

    uint64_t ret = 0;
    bool tryCommon = true;

    if (gate != 0) {
        CheckPtr(param_2);
        int64_t citizen = *reinterpret_cast<int64_t*>(param_2);
        if (local_res20 == *reinterpret_cast<int64_t*>(citizen + 0x1f0)) {
            // key match: build with the +0x248 gate object as the skip-key holder
            uint8_t hit = BuildAndWalk(gate);
            if (hit != 0) { ret = 1; tryCommon = false; }
        }
        // (key mismatch -> falls through to the common path, LAB_1400f7a89)
    }

    if (tryCommon && ret == 0) {
        // LAB_1400f7a89: build with a FRESH empty IBP as the skip-key holder (no skip filter),
        // but only when the citizen's "remember" flag (local_res20+0x4c) is set OR param_6.
        // SA-invariant guard: retail reads *(local_res20+0x4c) unconditionally because param_4
        // (the owner/target key) is never null at this idle-walk point. In the standalone exe a
        // citizen occasionally reaches State_Idle with a null owner key (citizen+0x1f0 == 0, not
        // yet home-assigned), so local_res20 can be 0 -> READ of 0x4c crash. Treat a null key as
        // "no remembered target" (remember flag unset); the common walk still runs when param_6.
        // TODO(2c-B follow-up): root-cause why some SA citizens have citizen+0x1f0 == 0.
        if ((local_res20 != 0 && *reinterpret_cast<char*>(local_res20 + 0x4c) != '\0') || (param_6 != '\0')) {
            int64_t freshIbp[2] = {0, 0};
            if (fn_InternalBasePtr_Ctor) new (freshIbp) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint8_t hit = BuildAndWalk(freshIbp[0]);
            ret = (hit != 0) ? 1 : 0;
        }
    }
    (void)local_58;

    // free the transient result tree
    int64_t r = resultPair[0];
    if (r != 0) {
        if (s_e320) {
            s_e320(&resultPair[0], *reinterpret_cast<void**>(r + 0x18));
            s_e320(&resultPair[0], *reinterpret_cast<void**>(r + 0x20));
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(r));
    }
    return ret;
}

// ============================================================
// 0x1400f7550 — roll a randomised list of candidate states, then for each (until success) walk
//   the corresponding remembered-target subtree via 79b0.
//   param_1 = tree-root array base (*(g_pGameWorldSingleton+0x118))
//   param_2 = &citizenIBP
//   Returns CONCAT71(.., walkSucceeded).
// ============================================================
uint64_t Helper_7550(int64_t param_1, void** param_2) {
    EnsureInit();
    // 0x78-byte candidate buffer, slots at +i*8, low byte = state code, init 0xFF (terminator).
    unsigned char buf[0x78];
    Helper_3f010(buf, 0xffffffff, 0x78);
    char allFlag = 0;        // cVar9 / R15B

    CheckPtr(param_2);
    char canHouse = s_canHouse ? s_canHouse(*reinterpret_cast<void**>(param_2)) : 0;

    uint32_t n;  // candidate count -> Rand range
    if (canHouse != 0) {
        allFlag = 1;
        buf[0] = 0x0e;
        n = 1;
    } else {
        // counters object at *(*(gw+0xf0) + 0x170)  -> piVar2 (int array)
        int64_t listEnt = *reinterpret_cast<int64_t*>(GW() + 0xf0);
        bool homeless = (*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_2) + 0x1f0)
                         == *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(GW() + 0xb0) + 0x38));
        if (homeless) buf[0] = 0x0d;
        uint32_t idx = homeless ? 1u : 0u;
        int* p = *reinterpret_cast<int**>(listEnt + 0x170);

        // Each resource category below threshold -> emit a gather/work state code into the next
        // slot. The packed multi-byte writes mirror the exe (slot low byte = primary state id).
        auto emit1 = [&](unsigned char b0) {
            buf[idx * 8 + 0] = b0; idx++;
        };
        auto emit2 = [&](unsigned char b0, unsigned char b1) {
            buf[idx * 8 + 0] = b0; buf[idx * 8 + 1] = b1; idx++;
        };
        auto emit4 = [&](unsigned char b0, unsigned char b1, unsigned char b2, unsigned char b3) {
            buf[idx * 8 + 0] = b0; buf[idx * 8 + 1] = b1; buf[idx * 8 + 2] = b2; buf[idx * 8 + 3] = b3; idx++;
        };

        if (p[0x28] <= p[0x2a] / 10) emit1(1);
        if (p[0x18] <= p[0x1a] / 10) { emit2(0x02, 0x04); buf[(idx - 1) * 8 + 2] = 10; }
        if (p[0x20] <= p[0x22] / 10) { emit4(0x03, 0x06, 0x04, 0x0a); buf[(idx - 1) * 8 + 4] = 0x0c; }
        if (p[0x00] <= p[0x02] / 10) emit2(0x04, 0x0a);
        if (p[0x10] <= p[0x12] / 10) emit2(0x06, 0x0c);
        if (p[0x08] <= p[0x0a] / 10) emit2(0x05, 0x0b);
        if (p[0x50] <= p[0x52] / 10) emit1(9);
        if (p[0x38] <= p[0x3a] / 10) emit2(0x07, 0x09);
        if (p[0x30] <= p[0x32] / 10) emit1(8);
        n = idx;
    }

    // Random start index in [0, n)
    void* rng = *reinterpret_cast<void**>(GW() + 0xd8);
    uint32_t r = fn_Random_Rand ? fn_Random_Rand(rng, n) : 0;

    // walk from buf[r*8] byte-by-byte until a negative (0xFF) terminator
    unsigned char* pc = buf + static_cast<int>(r) * 8;
    while (static_cast<signed char>(*pc) >= 0) {
        CheckPtr(param_2);
        int64_t citizen = *reinterpret_cast<int64_t*>(param_2);  // local_res18[0]
        int64_t ownerKey = *reinterpret_cast<int64_t*>(citizen + 0x1f0);

        // tree root for this state code: param_1[(byte+3)*0x10], then follow +0x18 to leftmost
        int64_t stateByte = static_cast<int64_t>(static_cast<signed char>(*pc));
        int64_t node = *reinterpret_cast<int64_t*>(param_1 + (stateByte + 3) * 0x10);
        int64_t walked = node;  // local_res10
        while (node != 0) { walked = node; node = *reinterpret_cast<int64_t*>(node + 0x18); }

        void* citHolder = reinterpret_cast<void*>(citizen);  // &local_res18
        void* walkedHolder = reinterpret_cast<void*>(walked);
        uint64_t hit = Helper_sub_79b0(param_1, reinterpret_cast<void**>(&citHolder),
                                       reinterpret_cast<void**>(&walkedHolder), ownerKey, '\x01', allFlag);
        if (static_cast<char>(hit) != '\0') {
            return (static_cast<uint64_t>(hit) & 0xffffffffffffff00ULL) | 1;
        }
        pc++;
    }
    return 0;
}

// ============================================================
// 0x1400f7830 — Idle: try the citizen's current owner-key subtree in the param_1+0x120 BST
//   first (cost mode 8000, all=0). If that misses, fall back to 7550; if THAT also misses, try
//   the "home key" (gameWorld+0xb0 +0x30) subtree once.
// ============================================================
void Helper_7830(int64_t param_1, void** param_2) {
    if (*reinterpret_cast<int*>(param_1 + 0x38) == 0) return;  // tree-array not populated
    ++g_idleSearchCalls;

    int64_t homeRoot = *reinterpret_cast<int64_t*>(GW() + 0xb0);
    uint64_t homeKey = *reinterpret_cast<uint64_t*>(homeRoot + 0x30);

    CheckPtr(param_2);
    uint64_t ownerKey = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_2) + 0x1f0);

    // (1) probe the param_1+0x120 BST for ownerKey
    if (ownerKey == *reinterpret_cast<uint64_t*>(homeRoot + 0x38)) ++g_idleSearchByBuilder;
    if (ownerKey != *reinterpret_cast<uint64_t*>(homeRoot + 0x30)) {
        uint64_t* node = *reinterpret_cast<uint64_t**>(param_1 + 0x120);
        while (node != nullptr) {
            if (ownerKey < node[0]) {
                node = reinterpret_cast<uint64_t*>(node[4]);          // +0x20 left
            } else if (ownerKey <= node[0]) {
                // equal: value subtree at node[1] (+0x08), descend +0x18 to leftmost
                uint64_t v = node[1];
                uint64_t walked = v;
                while (v != 0) { walked = v; v = *reinterpret_cast<uint64_t*>(v + 0x18); }
                int64_t citizen = *reinterpret_cast<int64_t*>(param_2);
                void* citHolder = reinterpret_cast<void*>(citizen);
                void* walkedHolder = reinterpret_cast<void*>(walked);
                uint64_t hit = Helper_sub_79b0(param_1, reinterpret_cast<void**>(&citHolder),
                                               reinterpret_cast<void**>(&walkedHolder),
                                               static_cast<int64_t>(ownerKey), '\0', '\0');
                if (ownerKey == *reinterpret_cast<uint64_t*>(homeRoot + 0x38)) {
                    ++g_idleBuilderBucketHit;
                    if (static_cast<char>(hit) != '\0') ++g_idleBuilderTookWork;
                }
                if (static_cast<char>(hit) != '\0') return;
                break;
            } else {
                node = reinterpret_cast<uint64_t*>(node[5]);          // +0x28 right
            }
        }
    }

    // (2) fall back to the full 7550 candidate roll
    void* citHolder2 = *reinterpret_cast<void**>(param_2);
    uint64_t hit2 = Helper_7550(param_1, reinterpret_cast<void**>(&citHolder2));
    if (static_cast<char>(hit2) != '\0') return;

    // (3) try the home-key subtree once
    uint64_t* node = *reinterpret_cast<uint64_t**>(param_1 + 0x120);
    while (node != nullptr) {
        if (homeKey < node[0]) {
            node = reinterpret_cast<uint64_t*>(node[4]);
        } else if (homeKey <= node[0]) {
            uint64_t v = node[1];
            uint64_t walked = v;
            while (v != 0) { walked = v; v = *reinterpret_cast<uint64_t*>(v + 0x18); }
            int64_t citizen = *reinterpret_cast<int64_t*>(param_2);
            void* citHolder = reinterpret_cast<void*>(citizen);
            void* walkedHolder = reinterpret_cast<void*>(walked);
            Helper_sub_79b0(param_1, reinterpret_cast<void**>(&citHolder),
                            reinterpret_cast<void**>(&walkedHolder),
                            static_cast<int64_t>(homeKey), '\0', '\0');
            return;
        } else {
            node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }
}

// return expression shared by both handlers:
//   CONCAT71(citizen>>8, 0 < *(int*)(*(citizen+0x278) + 0x60) + 1)
static uint64_t HandlerEpilogue(int64_t param_1) {
    CheckPtr(reinterpret_cast<void*>(param_1 + 0x38));
    int64_t citizen = *reinterpret_cast<int64_t*>(param_1 + 0x38);
    CheckPtr(reinterpret_cast<void*>(citizen + 0x278));
    int64_t ch = *reinterpret_cast<int64_t*>(citizen + 0x278);
    int truth = (0 < *reinterpret_cast<int*>(ch + 0x60) + 1) ? 1 : 0;
    return (static_cast<uint64_t>(ch) & 0xffffffffffffff00ULL) | static_cast<uint64_t>(truth);
}

} // namespace

// ============================================================
// 0x14010b340 — CitizenAI_State_FindHousing  (state id 2)
// ============================================================
extern "C" uint64_t CitizenAI_State_FindHousing(int64_t param_1) {
    EnsureInit();
    int64_t citizenHolder = *reinterpret_cast<int64_t*>(param_1 + 0x38);  // local_res8[0]
    int64_t treeBase = *reinterpret_cast<int64_t*>(GW() + 0x118);
    void* holder = reinterpret_cast<void*>(citizenHolder);
    Helper_7550(treeBase, reinterpret_cast<void**>(&holder));
    return HandlerEpilogue(param_1);
}

// ============================================================
// 0x14010b2d0 — CitizenAI_State_Idle  (state id 9)
// ============================================================
extern "C" uint64_t CitizenAI_State_Idle(int64_t param_1) {
    EnsureInit();
    int64_t citizenHolder = *reinterpret_cast<int64_t*>(param_1 + 0x38);  // local_res8[0]
    int64_t treeBase = *reinterpret_cast<int64_t*>(GW() + 0x118);
    void* holder = reinterpret_cast<void*>(citizenHolder);
    Helper_7830(treeBase, reinterpret_cast<void**>(&holder));
    return HandlerEpilogue(param_1);
}

// ============================================================
// Registration thunks — expose the anonymous-namespace helper bodies at their original exe
// addresses so any cross-TU ResolveAddr(Module::Exe, <addr>) resolves to our faithful port.
// (The handlers above call the helpers directly via normal C++ calls; these thunks exist solely
// so the address registry can map each sub-callee.)
// ============================================================
extern "C" uint64_t CitizenAI_Reg_helper_7550(int64_t a, void** b) { return Helper_7550(a, b); }
extern "C" void     CitizenAI_Reg_helper_7830(int64_t a, void** b) { Helper_7830(a, b); }
extern "C" uint64_t CitizenAI_Reg_helper_79b0(int64_t a, void** b, void** c, int64_t d, char e, char f) {
    return Helper_sub_79b0(a, b, c, d, e, f);
}
extern "C" uint8_t  CitizenAI_Reg_helper_7b60(int64_t a, void** b, int64_t* c) { return Helper_sub_7b60(a, b, c); }
extern "C" void     CitizenAI_Reg_helper_7c90(void* a, void* b, void* c, int64_t* d, int64_t* e) {
    Helper_sub_7c90(a, b, c, d, e);
}
extern "C" void     CitizenAI_Reg_helper_8000(void* a, void* b, int64_t* c, int64_t* d) {
    Helper_sub_8000(a, b, c, d);
}
extern "C" void*    CitizenAI_Reg_helper_3f010(void* a, uint64_t b, uint64_t c) { return Helper_3f010(a, b, c); }
extern "C" void     CitizenAI_Reg_helper_e360(int64_t* a, int64_t* b) { Helper_e360(a, b); }

