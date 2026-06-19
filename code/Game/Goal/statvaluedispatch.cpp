// code/Game/Goal/statvaluedispatch.cpp
// Faithful C++ port of the FinishTutorialInstance stat-value dispatcher
// (exe 0x1400da4e0) and ALL of its sub-callees.
//
// The dispatcher is a switch(statType) that returns a goal-progress count
// queried from the GameWorld (citizen counts, resource totals, building
// counts, etc.). Each case calls a small helper at 0x1400d7xxx..0x1400da4xx
// (named FinishTutorialInstance_helper_<addr> in Ghidra) or reads a field
// inline. Several helpers in turn call deeper exe helpers (an AVL family at
// 0x140014cxx, the entity-scan helpers 0x140046dd0 / 0x140090f20 /
// 0x14011e710 / 0x140004680 / 0x140003ea0) — those are ALSO ported here as
// static functions so the whole cluster is closed (no ResolveAddr to exe
// code, which would create new unregistered-gap entries).
//
// Closure: CLEAN. Every sub-callee bottoms out in field reads +
// fn_IsDerivedFrom + fn_InternalBasePtr_CheckPtr/_Ctor + fn_MemAlloc/
// fn_MemFreeStatic + fn_Time_AsSeconds. No exe-internal call escapes.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "statvaluedispatch.h"
#include <cstdint>

// ============================================================
// GameWorld accessor — *g_pGameWorldSingleton is the world object; the exe
// reads world+0xNN, so we form (uintptr_t)*g_pGameWorldSingleton + 0xNN.
// (Mirrors engine_funcs.h usage of (uintptr_t)*g_pGameWorldSingleton + 0xd8.)
// ============================================================
static inline uintptr_t World() { return (uintptr_t)*g_pGameWorldSingleton; }

// Read a uint32 field at world+off.
static inline uint32_t WorldU32(uintptr_t off) {
    return *reinterpret_cast<uint32_t*>(World() + off);
}
// Read a pointer field at world+off.
static inline uintptr_t WorldPtr(uintptr_t off) {
    return *reinterpret_cast<uintptr_t*>(World() + off);
}

// ============================================================
// Lazily-resolved type-ID storage globals (exe .rdata uint32_t, filled by
// each type's RegisterType at startup). These 0x1401cXXXX / 0x140172XXX
// addresses are OUTSIDE the gap range — do NOT register them anywhere.
// A null resolve (standalone, not yet present) => treat the type-id as
// unmatchable (the scan returns null for that component).
// ============================================================
static const uint32_t* s_tidCitizenAI    = nullptr;  // 0x1401c7318 (the "citizen" container type)
static const uint32_t* s_tidProfComp      = nullptr;  // 0x1401c7cb8 (Profession-on-citizen component)
static const uint32_t* s_tidStorage       = nullptr;  // 0x1401c7e78
static const uint32_t* s_tid_4b10         = nullptr;  // 0x1401c4b10
static const uint32_t* s_tid_80e0         = nullptr;  // 0x1401c80e0
static const uint32_t* s_tid_8118         = nullptr;  // 0x1401c8118 (DAT_)
static const uint32_t* s_tid_7008         = nullptr;  // 0x1401c7008
static const uint32_t* s_tid_75f0         = nullptr;  // 0x1401c75f0
static const uint32_t* s_tid_7d98         = nullptr;  // 0x1401c7d98
static const uint32_t* s_tid_7040         = nullptr;  // 0x1401c7040
static const uint32_t* s_tid_79e0         = nullptr;  // 0x1401c79e0
static const uint32_t* s_tid_4a30         = nullptr;  // 0x1401c4a30
static const uint32_t* s_tid_48a8         = nullptr;  // 0x1401c48a8
static const uint32_t* s_tidProfession    = nullptr;  // 0x1401c5a98 (TypeInfo_Profession)
static const uint32_t* s_tidTerrainGen    = nullptr;  // 0x1401c5b40 (TypeInfo_TerrainGenerator)
static const uint32_t* s_tidClimateGen    = nullptr;  // 0x1401c5910 (TypeInfo_ClimateGenerator)

// Float constants used by helper_8c50 (season-progress weighting).
static const float* s_pDAT_14017225c = nullptr;
static const float* s_pDAT_1401721e4 = nullptr;

static bool s_tidInit = false;
static void EnsureTypeIds() {
    if (s_tidInit) return;
    s_tidCitizenAI = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7318));
    s_tidProfComp  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_tidStorage   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_tid_4b10     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4b10));
    s_tid_80e0     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c80e0));
    s_tid_8118     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8118));
    s_tid_7008     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7008));
    s_tid_75f0     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c75f0));
    s_tid_7d98     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7d98));
    s_tid_7040     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7040));
    s_tid_79e0     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    s_tid_4a30     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c4a30));
    s_tid_48a8     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c48a8));
    s_tidProfession= reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5a98));
    s_tidTerrainGen= reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5b40));
    s_tidClimateGen= reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5910));
    s_pDAT_14017225c = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017225c));
    s_pDAT_1401721e4 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721e4));
    s_tidInit = true;
}

// ============================================================
// Small primitives
// ============================================================

// IsDerivedFrom with a (possibly-null) type-id storage pointer. Returns the
// object if derived from *pTid, else null. If pTid is null (unresolved
// type-id) the original would compare against a stale/zero id; we conservatively
// treat that as "no match".
static inline void* DerivedOr0(void* obj, const uint32_t* pTid) {
    if (!pTid || !fn_IsDerivedFrom) return nullptr;
    return fn_IsDerivedFrom(obj, *pTid) ? obj : nullptr;
}

// Inlined FindComponent: scan an entity's component list for the first whose
// GetTypeId (vtable +0x10) == *pTid, then IsDerivedFrom-guard it.
// entity+0x30 = component ptr array, entity+0x38 = count.
// Mirrors the loop bodies inside the helpers exactly (CheckPtr before deref,
// vtable +0x10 GetTypeId, break on first id match).
static void* FindComponentInEntity(uintptr_t entity, const uint32_t* pTid) {
    if (!pTid) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(list + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        void* comp = *slot;
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (t == *pTid) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            return DerivedOr0(*slot, pTid);
        }
    }
    return nullptr;
}

// Same scan but the component array is laid out at base+0x28 (count at base+0x30)
// rather than the entity layout (+0x30/+0x38). Used by the citizen-Profession
// scans (DWORD_1401c7cb8) where the container's child list is at +0x28/+0x30.
static void* FindComponentAt28(uintptr_t base, const uint32_t* pTid) {
    if (!pTid) return nullptr;
    int n = *reinterpret_cast<int*>(base + 0x30);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(base + 0x28);
    for (int i = 0; i < n; i++) {
        void* comp = *reinterpret_cast<void**>(list + (int64_t)i * 8);
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (t == *pTid) {
            return DerivedOr0(*reinterpret_cast<void**>(list + (int64_t)i * 8), pTid);
        }
    }
    return nullptr;
}

// ============================================================
// AVL-tree helpers ported from FUN_140014c30 / c70 / da0.
// The 8d70 helper builds a temporary balanced tree to de-duplicate building
// objects while summing; nodes are 0x28 bytes:
//   +0x00 key, +0x08 parent, +0x10 left, +0x18 right, +0x20 balance(short), +0x22 height(short)
// ============================================================

// FUN_140014c30 — recursive post-order free (children at +0x10 / +0x18).
static void Avl_Free_14c30(void* node) {
    if (node) {
        Avl_Free_14c30(*reinterpret_cast<void**>((uintptr_t)node + 0x10));
        Avl_Free_14c30(*reinterpret_cast<void**>((uintptr_t)node + 0x18));
        if (fn_MemFreeStatic) fn_MemFreeStatic(node);
    }
}

// FUN_140014c70 — left rotation + height/balance recompute. Returns new subtree
// root. param_1 = &root pointer, param_2 = pivot node.
static uintptr_t Avl_RotateLeft_14c70(uintptr_t* root, uintptr_t node) {
    if (node == *root) *root = *reinterpret_cast<uintptr_t*>(node + 0x18);
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 8);
    if (parent != 0) {
        if (*reinterpret_cast<uintptr_t*>(parent + 0x10) == node)
            *reinterpret_cast<uintptr_t*>(parent + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x18);
        else
            *reinterpret_cast<uintptr_t*>(parent + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x18);
    }
    uintptr_t r = *reinterpret_cast<uintptr_t*>(node + 0x18);
    *reinterpret_cast<uintptr_t*>(r + 8) = *reinterpret_cast<uintptr_t*>(node + 8);
    uintptr_t rl = *reinterpret_cast<uintptr_t*>(r + 0x10);
    *reinterpret_cast<uintptr_t*>(node + 0x18) = rl;
    if (rl != 0) *reinterpret_cast<uintptr_t*>(rl + 8) = node;
    *reinterpret_cast<uintptr_t*>(r + 0x10) = node;
    *reinterpret_cast<uintptr_t*>(node + 8) = r;
    // recompute node, r->right, r heights/balances
    auto recompute = [](uintptr_t n) {
        short hl = (*reinterpret_cast<uintptr_t*>(n + 0x10) == 0)
                       ? 0 : (short)(*reinterpret_cast<short*>(*reinterpret_cast<uintptr_t*>(n + 0x10) + 0x22) + 1);
        short hr = (*reinterpret_cast<uintptr_t*>(n + 0x18) == 0)
                       ? 0 : (short)(*reinterpret_cast<short*>(*reinterpret_cast<uintptr_t*>(n + 0x18) + 0x22) + 1);
        short h = (hr < hl) ? hl : hr;
        *reinterpret_cast<short*>(n + 0x20) = (short)(hr - hl);
        *reinterpret_cast<short*>(n + 0x22) = h;
    };
    if (*reinterpret_cast<uintptr_t*>(r + 0x10) != 0) recompute(*reinterpret_cast<uintptr_t*>(r + 0x10));
    if (*reinterpret_cast<uintptr_t*>(r + 0x18) != 0) recompute(*reinterpret_cast<uintptr_t*>(r + 0x18));
    recompute(r);
    return r;
}

// FUN_140014da0 — right rotation (mirror of the above; pivot child at +0x10).
static uintptr_t Avl_RotateRight_14da0(uintptr_t* root, uintptr_t node) {
    if (node == *root) *root = *reinterpret_cast<uintptr_t*>(node + 0x10);
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(node + 8);
    if (parent != 0) {
        if (*reinterpret_cast<uintptr_t*>(parent + 0x10) == node)
            *reinterpret_cast<uintptr_t*>(parent + 0x10) = *reinterpret_cast<uintptr_t*>(node + 0x10);
        else
            *reinterpret_cast<uintptr_t*>(parent + 0x18) = *reinterpret_cast<uintptr_t*>(node + 0x10);
    }
    uintptr_t l = *reinterpret_cast<uintptr_t*>(node + 0x10);
    *reinterpret_cast<uintptr_t*>(l + 8) = *reinterpret_cast<uintptr_t*>(node + 8);
    uintptr_t lr = *reinterpret_cast<uintptr_t*>(l + 0x18);
    *reinterpret_cast<uintptr_t*>(node + 0x10) = lr;
    if (lr != 0) *reinterpret_cast<uintptr_t*>(lr + 8) = node;
    *reinterpret_cast<uintptr_t*>(l + 0x18) = node;
    *reinterpret_cast<uintptr_t*>(node + 8) = l;
    auto recompute = [](uintptr_t n) {
        short hl = (*reinterpret_cast<uintptr_t*>(n + 0x10) == 0)
                       ? 0 : (short)(*reinterpret_cast<short*>(*reinterpret_cast<uintptr_t*>(n + 0x10) + 0x22) + 1);
        short hr = (*reinterpret_cast<uintptr_t*>(n + 0x18) == 0)
                       ? 0 : (short)(*reinterpret_cast<short*>(*reinterpret_cast<uintptr_t*>(n + 0x18) + 0x22) + 1);
        short h = (hr < hl) ? hl : hr;
        *reinterpret_cast<short*>(n + 0x20) = (short)(hr - hl);
        *reinterpret_cast<short*>(n + 0x22) = h;
    };
    if (*reinterpret_cast<uintptr_t*>(l + 0x10) != 0) recompute(*reinterpret_cast<uintptr_t*>(l + 0x10));
    if (*reinterpret_cast<uintptr_t*>(l + 0x18) != 0) recompute(*reinterpret_cast<uintptr_t*>(l + 0x18));
    recompute(l);
    return l;
}

// Insert key into the AVL set rooted at *root (the dedup set used by 8d70).
// Returns true if the key was newly inserted, false if already present.
// Faithful to the inline insert/retrace loop in 8d70: walk left(+0x10)/
// right(+0x18) on key compare; on a missing child, MemAlloc a 0x28 node and
// retrace, rotating with c70/da0 when |balance| > 1.
static bool Avl_Insert(uintptr_t* root, uintptr_t key) {
    if (*root == 0) {
        uintptr_t n = (uintptr_t)(fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8) : nullptr);
        if (n) {
            *reinterpret_cast<uintptr_t*>(n)        = key;
            *reinterpret_cast<uintptr_t*>(n + 8)    = 0;
            *reinterpret_cast<uintptr_t*>(n + 0x10) = 0;
            *reinterpret_cast<uintptr_t*>(n + 0x18) = 0;
            *reinterpret_cast<uint32_t*>(n + 0x20)  = 0;  // balance(short)+height(short)
            *root = n;
        }
        return true;
    }
    uintptr_t cur = *root;
    while (true) {
        if (key < *reinterpret_cast<uintptr_t*>(cur)) {
            uintptr_t left = *reinterpret_cast<uintptr_t*>(cur + 0x10);
            if (left == 0) break;
            cur = left;
        } else if (key <= *reinterpret_cast<uintptr_t*>(cur)) {
            return false;  // already present
        } else {
            uintptr_t right = *reinterpret_cast<uintptr_t*>(cur + 0x18);
            if (right == 0) break;
            cur = right;
        }
    }
    bool goLeft = key < *reinterpret_cast<uintptr_t*>(cur);
    uintptr_t n = (uintptr_t)(fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8) : nullptr);
    uintptr_t newNode = 0;
    if (n) {
        *reinterpret_cast<uintptr_t*>(n)        = key;
        *reinterpret_cast<uintptr_t*>(n + 8)    = cur;
        *reinterpret_cast<uintptr_t*>(n + 0x10) = 0;
        *reinterpret_cast<uintptr_t*>(n + 0x18) = 0;
        *reinterpret_cast<uint32_t*>(n + 0x20)  = 0;
        newNode = n;
    }
    *reinterpret_cast<uintptr_t*>(cur + (goLeft ? 0x10 : 0x18)) = newNode;
    // retrace from cur up to the root
    uintptr_t p = cur;
    while (p != 0) {
        uintptr_t up = *reinterpret_cast<uintptr_t*>(p + 8);
        short hl = (*reinterpret_cast<uintptr_t*>(p + 0x10) == 0)
                       ? 0 : (short)(*reinterpret_cast<short*>(*reinterpret_cast<uintptr_t*>(p + 0x10) + 0x22) + 1);
        uintptr_t rChild = *reinterpret_cast<uintptr_t*>(p + 0x18);
        short hr = (rChild == 0) ? 0 : (short)(*reinterpret_cast<short*>(rChild + 0x22) + 1);
        short h = (hr < hl) ? hl : hr;
        *reinterpret_cast<short*>(p + 0x22) = h;
        *reinterpret_cast<short*>(p + 0x20) = (short)(hr - hl);
        if ((short)(hr - hl) > 1 && rChild != 0) {
            if (*reinterpret_cast<short*>(rChild + 0x20) < 0)
                Avl_RotateRight_14da0(root, rChild);
            Avl_RotateLeft_14c70(root, p);
        }
        if (*reinterpret_cast<short*>(p + 0x20) < -1) {
            uintptr_t lChild = *reinterpret_cast<uintptr_t*>(p + 0x10);
            if (lChild != 0) {
                if (*reinterpret_cast<short*>(lChild + 0x20) > 0)
                    Avl_RotateLeft_14c70(root, lChild);
                Avl_RotateRight_14da0(root, p);
            }
        }
        p = up;
    }
    return true;
}

// ============================================================
// FUN_140003ea0 / FUN_140004680 / FUN_14011e710 — sub-callees of 7cf0/81b0.
// ============================================================

// FUN_140003ea0 — FindComponent(DWORD_1401c7d98) into out IBP. (entity layout)
static void* Find_3ea0(uintptr_t entity, void* outIBP) {
    void* found = FindComponentInEntity(entity, s_tid_7d98);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
    return outIBP;
}

// FUN_140004680 — FindComponent(DWORD_1401c79e0) into out IBP. (entity layout)
static void* Find_4680(uintptr_t entity, void* outIBP) {
    void* found = FindComponentInEntity(entity, s_tid_79e0);
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(outIBP, found);
    return outIBP;
}

// FUN_14011e710 — given a building (param_1), CheckPtr its +0x10 IBP, find the
// "buildState/desc" component (3ea0), then test its category byte (+0x52 signed,
// else +0x50) is one of {3,4,5}. Returns 1 if so, else 0.
static int Sub_e710(uintptr_t param1) {
    uintptr_t ibp10 = param1 + 0x10;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(ibp10));
    int64_t res = 0;
    Find_3ea0(*reinterpret_cast<uintptr_t*>(param1 + 0x10), &res);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&res);
    char c = *reinterpret_cast<char*>(res + 0x52);
    if (c < 0) c = *reinterpret_cast<char*>(res + 0x50);
    if (c != 3) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&res);
        c = *reinterpret_cast<char*>(res + 0x52);
        if (c < 0) c = *reinterpret_cast<char*>(res + 0x50);
        if (c != 4) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&res);
            c = *reinterpret_cast<char*>(res + 0x52);
            if (c < 0) c = *reinterpret_cast<char*>(res + 0x50);
            if (c != 5) return 0;
        }
    }
    return 1;
}

// CemeteryUI_OnCreate_sub_6dd0 (0x140046dd0) — counts how many of a building's
// tile-occupants have a (DWORD_1401c4a30) component. param_1 is the building.
static int Sub_6dd0(uintptr_t param1) {
    int matched = 0;
    uintptr_t world2 = WorldPtr(0xf8);
    int slotCount = *reinterpret_cast<int*>(param1 + 0x50);
    if (slotCount > 0) {
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(param1 + 0x48);
        for (int i = 0; i < slotCount; i++) {
            int tileIndex = *reinterpret_cast<int*>(arr + (int64_t)i * 4);
            uintptr_t occupant = *reinterpret_cast<uintptr_t*>(
                *reinterpret_cast<uintptr_t*>(world2 + 0x80) + 8 + (int64_t)tileIndex * 0x30);
            if (occupant != 0) {
                int64_t res = (int64_t)occupant;
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&res);
                void* comp = FindComponentInEntity((uintptr_t)res, s_tid_4a30);
                int64_t resIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&resIBP, comp);
                if (resIBP != 0) matched++;
            }
        }
    }
    return matched;
}

// FUN_140090f20 — sum two interleaved int columns over a [param_3..param_4] row
// range of table param_2 within self+0x150. Returns col0sum + col1sum (+ tail).
static int Sub_0f20(uintptr_t param1, int param2, int param3, int param4) {
    int64_t* row = reinterpret_cast<int64_t*>((int64_t)param2 * 0x10 + *reinterpret_cast<uintptr_t*>(param1 + 0x150));
    if (param3 < 0) param3 = 0;
    int rowCount = (int)row[1];
    if (rowCount <= param4) param4 = rowCount - 1;
    int64_t lo = param3, hi = param4;
    int colA = 0, colB = 0, tail = 0;
    if (lo <= hi) {
        if ((hi - lo) + 1 > 1) {
            int* p = reinterpret_cast<int*>(row[0] + lo * 4);
            int64_t pairs = ((hi - lo) - 1) / 2 + 1;
            lo = lo + pairs * 2;
            do {
                colA += p[0];
                colB += p[1];
                p += 2;
                pairs--;
            } while (pairs != 0);
        }
        if (lo <= hi) tail = *reinterpret_cast<int*>(row[0] + lo * 4);
        return colB + colA + tail;
    }
    return 0;
}

// ============================================================
// Dispatcher helpers (0x1400d7cf0 .. 0x1400da410)
// ============================================================

// 0x1400da410 — count "adult employable" citizens.
static uint32_t Helper_a410() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        float age   = *reinterpret_cast<float*>(c + 0x3c);
        float adult = *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(c + 0x288) + 0x44);
        if (adult <= age && *reinterpret_cast<int64_t*>(c + 0x208) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
            int minAge = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(c + 0x288) + 0x118);
            if (minAge <= *reinterpret_cast<int*>(c + 0x210)) count++;
        }
    }
    return (uint32_t)count;
}

// 0x1400da2a0 — count citizens whose home-house index matches the profession's
// house-id (or whose +0xa8 is < 0). ext is a citizen container; we find its
// Profession component (DWORD_1401c7cb8) for the +0x54 house id.
static uint32_t Helper_a2a0(uintptr_t /*self*/, void* ext) {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    void* container = DerivedOr0(ext, s_tidCitizenAI);
    void* prof = container ? FindComponentAt28((uintptr_t)container, s_tidProfComp) : nullptr;
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        float age   = *reinterpret_cast<float*>(c + 0x3c);
        float adult = *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(c + 0x288) + 0x44);
        if (adult <= age && *reinterpret_cast<int64_t*>(c + 0x208) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
            int homeId = *reinterpret_cast<int*>(c + 0xa8);
            int wantId = prof ? *reinterpret_cast<int*>((uintptr_t)prof + 0x54) : 0;
            if (homeId == wantId) { count++; continue; }
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
            if (*reinterpret_cast<int*>(c + 0xa8) < 0) count++;
        }
    }
    return (uint32_t)count;
}

// 0x1400da150 — count citizens by health-state vs a flag bit (prof+0x5c & 0x8000).
static uint32_t Helper_a150(uintptr_t /*self*/, void* ext) {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    void* container = DerivedOr0(ext, s_tidCitizenAI);
    void* prof = container ? FindComponentAt28((uintptr_t)container, s_tidProfComp) : nullptr;
    int count = 0;
    uint32_t flag = prof ? *reinterpret_cast<uint32_t*>((uintptr_t)prof + 0x5c) : 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        int state = *reinterpret_cast<int*>(c + 0x1e0);
        bool hi = (flag & 0x8000) != 0;
        if ((((state - 2U) & 0xfffffffd) == 0 && hi) ||
            ((state == 1 || state == 4) && !hi)) {
            count++;
        }
    }
    return (uint32_t)count;
}

// 0x1400da0a0 — average of citizen field +0x80 (rounded toward zero).
static uint32_t Helper_a0a0() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        sum += *reinterpret_cast<float*>(c + 0x80);
    }
    return (n == 0) ? 0u : (uint32_t)(int)(sum / (float)n);
}

// 0x1400d9ff0 — average of citizen field +0x50.
static uint32_t Helper_9ff0() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        sum += *reinterpret_cast<float*>(c + 0x50);
    }
    return (n == 0) ? 0u : (uint32_t)(int)(sum / (float)n);
}

// 0x1400d9f20 — count adult citizens (no min-age sub-test, no house test).
static uint32_t Helper_9f20() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        float age   = *reinterpret_cast<float*>(c + 0x3c);
        float adult = *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(c + 0x288) + 0x44);
        if (adult <= age && *reinterpret_cast<int64_t*>(c + 0x208) == 0) count++;
    }
    return (uint32_t)count;
}

// 0x1400d9e80 — count citizens with +0x208 != 0 (children / dependents).
static uint32_t Helper_9e80() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        if (*reinterpret_cast<int64_t*>(c + 0x208) != 0) count++;
    }
    return (uint32_t)count;
}

// 0x1400d9dc0 — count citizens that are (under-adult-age OR have +0x208) AND
// have +0x208 == 0. (Faithful to the exact two-predicate form.)
static uint32_t Helper_9dc0() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        float age   = *reinterpret_cast<float*>(c + 0x3c);
        float adult = *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(c + 0x288) + 0x44);
        if ((age < adult || *reinterpret_cast<int64_t*>(c + 0x208) != 0)) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
            if (*reinterpret_cast<int64_t*>(c + 0x208) == 0) count++;
        }
    }
    return (uint32_t)count;
}

// 0x1400d9cd0 — count adult employed citizens whose profession (+0x1f0) == ext
// (a Profession). TypeInfo_Profession used for the IsDerivedFrom of ext.
static uint32_t Helper_9cd0(uintptr_t /*self*/, void* ext) {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0xa8);
    void* prof = DerivedOr0(ext, s_tidProfession);
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t c = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0xa0) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
        float age   = *reinterpret_cast<float*>(c + 0x3c);
        float adult = *reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(c + 0x288) + 0x44);
        if (adult <= age && *reinterpret_cast<int64_t*>(c + 0x208) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&c);
            if (*reinterpret_cast<void**>(c + 0x1f0) == prof) count++;
        }
    }
    return (uint32_t)count;
}

// 0x1400d9c80 — true if world+0x48 == (ext IsDerivedFrom TerrainGenerator ? ext : 0).
static uint32_t Helper_9c80(uintptr_t /*self*/, void* ext) {
    void* p = *reinterpret_cast<void**>(World() + 0x48);
    void* q = DerivedOr0(ext, s_tidTerrainGen);
    return (p == q) ? 1u : 0u;
}

// 0x1400d9c10 — true if world+0x50 == (ext IsDerivedFrom ClimateGenerator ? ext : 0).
static uint32_t Helper_9c10(uintptr_t /*self*/, void* ext) {
    void* p = *reinterpret_cast<void**>(World() + 0x50);
    void* q = DerivedOr0(ext, s_tidClimateGen);
    return (p == q) ? 1u : 0u;
}

// 0x1400d9b80 — sum Sub_6dd0 over world buildings (world+0x40 array, +0x48 count).
static uint32_t Helper_9b80() {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0x48);
    int total = 0;
    for (int i = 0; i < n; i++) {
        int64_t b = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0x40) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&b);
        total += Sub_6dd0((uintptr_t)b);
    }
    return (uint32_t)total;
}

// Generic body for 0x1400d9560 / 9470 / 9380: scan the limited-resource list
// (world+0x120 array, +0x128 count); each element holds a component of type
// DWORD_1401c48a8; count those whose +0x40 == `wantState`.
static int LimitedResourceCount(int wantState) {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0x128);
    int count = 0;
    for (int i = 0; i < n; i++) {
        uintptr_t elem = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0x120) + (int64_t)i * 8);
        void* comp = s_tid_48a8 ? FindComponentAt28(elem, s_tid_48a8) : nullptr;
        if (comp && *reinterpret_cast<int*>((uintptr_t)comp + 0x40) == wantState) count++;
    }
    return count;
}

// 0x1400d9560 — limited-resource count with state == 2.
static uint32_t Helper_9560() { return (uint32_t)LimitedResourceCount(2); }
// 0x1400d9470 — limited-resource count with state == 0.
static uint32_t Helper_9470() { return (uint32_t)LimitedResourceCount(0); }
// 0x1400d9380 — limited-resource count with state == 1.
static uint32_t Helper_9380() { return (uint32_t)LimitedResourceCount(1); }

// 0x1400d92d0 — sum field +0x38 over world list (+0x90 array, +0x98 count) for
// elements whose +0x28 == ext (a building pointer).
static uint32_t Helper_92d0(uintptr_t /*self*/, void* ext) {
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0x98);
    int total = 0;
    for (int i = 0; i < n; i++) {
        int64_t e = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0x90) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&e);
        if (*reinterpret_cast<void**>(e + 0x28) == ext) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&e);
            total += *reinterpret_cast<int*>(e + 0x38);
        }
    }
    return (uint32_t)total;
}

// 0x1400d8d70 — count distinct buildings (AVL-deduped) that hold a Storage
// (DWORD_1401c4b10) component containing a Profession (DWORD_1401c7cb8) whose
// flags (+0x5c) intersect self+0x78, optionally filtered by owner == param_2.
static uint32_t Helper_8d70(uintptr_t self, void* extPtr) {
    uintptr_t param2 = (uintptr_t)extPtr;
    uintptr_t world2 = WorldPtr(0xf0);
    uintptr_t dedupRoot = 0;   // AVL set "local_60"
    int distinct = 0;          // "local_58"

    // world+0xb0 is the cell-region container; +0x20 array, +0x28 count.
    uintptr_t regionC = WorldPtr(0xb0);
    int regionCount = *reinterpret_cast<int*>(regionC + 0x28);
    for (int ri = 0; ri < regionCount; ri++) {
        uint64_t regionKey = *reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(WorldPtr(0xb0) + 0x20) + (int64_t)ri * 8);

        // BST lookup at world2+0x100 keyed by regionKey -> node[2] = bucket count.
        int bucketCount = 0;
        {
            uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x100);
            while (node) {
                if (regionKey < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
                else if (regionKey <= node[0]) { bucketCount = (int)node[2]; break; }
                else node = reinterpret_cast<uint64_t*>(node[5]);
            }
        }
        for (int bi = 0; bi < bucketCount; bi++) {
            // re-lookup node, then bucket entry [1][bi].
            uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x100);
            uint64_t* match = nullptr;
            while (node) {
                if (regionKey < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
                else { match = node; if (regionKey <= node[0]) break; node = reinterpret_cast<uint64_t*>(node[5]); }
            }
            if (!match) continue;
            int64_t entityRef = *reinterpret_cast<int64_t*>(match[1] + (int64_t)bi * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityRef);
            // find Storage component (DWORD_1401c4b10) on the entity (entity layout).
            void* storageComp = FindComponentInEntity((uintptr_t)entityRef, s_tid_4b10);
            int64_t storageIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&storageIBP, storageComp);
            if (storageIBP == 0) continue;
            // optional owner filter (param_2): entity+0x28 == param_2.
            if (param2 != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityRef);
                if (*reinterpret_cast<uintptr_t*>(entityRef + 0x28) != param2) continue;
            }
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
            int childN = *reinterpret_cast<int*>(storageIBP + 0x30);
            for (int ci = 0; ci < childN; ci++) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
                uint64_t childObj = *reinterpret_cast<uint64_t*>((int64_t)ci * 0x10 + *reinterpret_cast<uintptr_t*>(storageIBP + 0x28));
                // find Profession (DWORD_1401c7cb8) within childObj (at +0x28/+0x30).
                void* prof = FindComponentAt28((uintptr_t)childObj, s_tidProfComp);
                uint32_t selfFlags = *reinterpret_cast<uint32_t*>(self + 0x78);
                uint32_t profFlags = prof ? *reinterpret_cast<uint32_t*>((uintptr_t)prof + 0x5c) : 0;
                if ((selfFlags & profFlags) != 0) {
                    if (Avl_Insert(&dedupRoot, childObj)) distinct++;
                }
            }
        }
    }
    if (dedupRoot != 0) {
        Avl_Free_14c30(*reinterpret_cast<void**>(dedupRoot + 0x10));
        Avl_Free_14c30(*reinterpret_cast<void**>(dedupRoot + 0x18));
        if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(dedupRoot));
    }
    return (uint32_t)distinct;
}

// 0x1400d8c50 — season-weighted production lookup. Picks a column index from
// self+0x78 flag bits, then sums the table window via Sub_0f20.
static uint32_t Helper_8c50(uintptr_t self) {
    uint32_t flags = *reinterpret_cast<uint32_t*>(self + 0x78);
    uintptr_t world2 = WorldPtr(0xf0);
    int col;
    if ((flags >> 0x11) & 1)       col = 0x22;
    else if ((flags >> 0xe) & 1)   col = 0x1f;
    else if (flags & 0x40)         col = 0x13;
    else if (flags & 0x20)         col = 0x19;
    else if ((flags >> 0xd) & 1)   col = 0x1c;
    else if ((flags >> 0xc) & 1)   col = 0x10;
    else if ((flags >> 0xb) & 1)   col = 0xd;
    else if ((flags >> 0xa) & 1)   col = 0xa;
    else if ((flags >> 0x9) & 1)   col = 0x16;
    else                           return 0;

    int* clock = *reinterpret_cast<int**>(World() + 0x128);
    int dayOfYear = clock[0];
    float secs = fn_Time_AsSeconds ? fn_Time_AsSeconds(reinterpret_cast<void*>(clock + 2)) : 0.0f;
    float div = s_pDAT_14017225c ? *s_pDAT_14017225c : 1.0f;
    float mul = s_pDAT_1401721e4 ? *s_pDAT_1401721e4 : 0.0f;
    int frac = (int)((secs / div) * mul);
    if (frac >= 0x30) frac = 0;
    int lo = ((clock[0] - *reinterpret_cast<int*>(self + 0x7c)) - 1) * 0x30;
    int hi = dayOfYear * 0x30 - 0x30 + frac;
    return (uint32_t)Sub_0f20(world2, col, lo, hi);
}

// 0x1400d88e0 — sum a per-storage quantity (+0x28+0x08 columns) over storages
// whose Profession flags intersect self+0x78, filtered to ext's stock list.
static uint32_t Helper_88e0(uintptr_t self, void* extPtr) {
    uintptr_t param2 = (uintptr_t)extPtr;
    uintptr_t world2 = WorldPtr(0xf0);
    void* stockComp = nullptr;
    {   // FindComponent(DAT_1401c8118) at ext (+0x28/+0x30 layout)
        void* container = DerivedOr0((void*)param2, s_tidCitizenAI);
        stockComp = container ? FindComponentAt28((uintptr_t)container, s_tid_8118) : nullptr;
    }
    uint64_t key = *reinterpret_cast<uint64_t*>((uintptr_t)stockComp + 0x20);

    // BST at world2+0x100 keyed by `key` -> node[2] = bucket count.
    int bucketCount = 0;
    {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x100);
        while (node) {
            if (key < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else if (key <= node[0]) { bucketCount = (int)node[2]; break; }
            else node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }
    int total = 0;
    for (int bi = 0; bi < bucketCount; bi++) {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x100);
        while (node) {
            if (key < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else { if (key <= node[0]) break; node = reinterpret_cast<uint64_t*>(node[5]); }
        }
        if (!node) node = nullptr;
        int64_t entityRef = node ? *reinterpret_cast<int64_t*>(node[1] + (int64_t)bi * 8) : 0;
        if (param2 != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityRef);
            if (*reinterpret_cast<uintptr_t*>(entityRef + 0x28) != param2) continue;
        }
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityRef);
        void* storageComp = FindComponentInEntity((uintptr_t)entityRef, s_tid_4b10);
        int64_t storageIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&storageIBP, storageComp);
        if (storageIBP == 0) continue;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
        int childN = *reinterpret_cast<int*>(storageIBP + 0x30);
        for (int ci = 0; ci < childN; ci++) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
            uintptr_t slotBase = (int64_t)ci * 0x10 + *reinterpret_cast<uintptr_t*>(storageIBP + 0x28);
            uintptr_t childObj = *reinterpret_cast<uintptr_t*>(slotBase);
            void* prof = FindComponentAt28(childObj, s_tidProfComp);
            uint32_t selfFlags = *reinterpret_cast<uint32_t*>(self + 0x78);
            uint32_t profFlags = prof ? *reinterpret_cast<uint32_t*>((uintptr_t)prof + 0x5c) : 0;
            if ((selfFlags & profFlags) != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
                total += *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(storageIBP + 0x28) + 8 + (int64_t)ci * 0x10);
            }
        }
    }
    return (uint32_t)total;
}

// 0x1400d86e0 — sum field +0x60 of the (DWORD_1401c80e0) component over the
// entities in the bucket keyed by ext's stock-component (+0x20).
static uint32_t Helper_86e0(uintptr_t /*self*/, void* extPtr) {
    uintptr_t param2 = (uintptr_t)extPtr;
    uintptr_t world2 = WorldPtr(0xf0);
    void* stockComp = nullptr;
    {
        void* container = DerivedOr0((void*)param2, s_tidCitizenAI);
        stockComp = container ? FindComponentAt28((uintptr_t)container, s_tid_8118) : nullptr;
    }
    uint64_t key = *reinterpret_cast<uint64_t*>((uintptr_t)stockComp + 0x20);

    int bucketCount = 0;
    {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x100);
        while (node) {
            if (key < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else if (key <= node[0]) { bucketCount = (int)node[2]; break; }
            else node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }
    int total = 0;
    for (int bi = 0; bi < bucketCount; bi++) {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x100);
        while (node) {
            if (key < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else { if (key <= node[0]) break; node = reinterpret_cast<uint64_t*>(node[5]); }
        }
        if (!node) node = nullptr;
        int64_t entityRef = node ? *reinterpret_cast<int64_t*>(node[1] + (int64_t)bi * 8) : 0;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityRef);
        void* comp = FindComponentInEntity((uintptr_t)entityRef, s_tid_80e0);
        int64_t compIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&compIBP, comp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&compIBP);
        total += *reinterpret_cast<int*>(compIBP + 0x60);
    }
    return (uint32_t)total;
}

// 0x1400d86a0 — look up self+0x7c into the world player-array (world+0x110:
// +0x220 array / +0x228 count); return the u32 entry or 0.
static uint32_t Helper_86a0(uintptr_t self) {
    int idx = *reinterpret_cast<int*>(self + 0x7c);
    uintptr_t pc = WorldPtr(0x110);
    if (idx >= 0 && idx < *reinterpret_cast<int*>(pc + 0x228)) {
        return *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(pc + 0x220) + (int64_t)idx * 4);
    }
    return 0;
}

// 0x1400d85f0 — count world list (+0x20 array, +0x28 count) entries whose
// +0x28 == ext. If ext is null, returns the raw count.
static uint32_t Helper_85f0(uintptr_t /*self*/, void* extPtr) {
    uintptr_t param2 = (uintptr_t)extPtr;
    uintptr_t world2 = WorldPtr(0xf0);
    int n = *reinterpret_cast<int*>(world2 + 0x28);
    if (param2 == 0) return (uint32_t)n;
    int count = 0;
    for (int i = 0; i < n; i++) {
        int64_t e = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(world2 + 0x20) + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&e);
        if (*reinterpret_cast<uintptr_t*>(e + 0x28) == param2) count++;
    }
    return (uint32_t)count;
}

// 0x1400d81b0 — count "buildable/finished" structures owned by ext, by walking
// the BST at world2+0x160 keyed by ext.  Each entity is examined for either a
// completed work (+0x18 quantity vs capacity) or an in-progress build state.
static uint32_t Helper_81b0(uintptr_t /*self*/, void* extPtr) {
    uintptr_t param2 = (uintptr_t)extPtr;
    uintptr_t world2 = WorldPtr(0xf0);
    void* keyObj = DerivedOr0((void*)param2, s_tidCitizenAI);

    // BST at world2+0x160 keyed by keyObj -> node[2] = bucket count.
    int bucketCount = 0;
    {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x160);
        while (node) {
            if ((uintptr_t)keyObj < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else if ((uintptr_t)keyObj <= node[0]) { bucketCount = (int)node[2]; break; }
            else node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }
    int result = 0;
    for (int bi = 0; bi < bucketCount; bi++) {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x160);
        int64_t entRef = 0;
        bool found = false;
        while (node) {
            if ((uintptr_t)keyObj < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else if ((uintptr_t)keyObj <= node[0]) { entRef = *reinterpret_cast<int64_t*>(node[1] + (int64_t)bi * 8); found = true; break; }
            else node = reinterpret_cast<uint64_t*>(node[5]);
        }
        if (!found) { if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entRef, nullptr); }

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        // find work-progress component (DWORD_1401c7008)
        void* workComp = FindComponentInEntity((uintptr_t)entRef, s_tid_7008);
        int64_t workIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIBP, workComp);
        // find build component (DWORD_1401c75f0)
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        void* buildComp = FindComponentInEntity((uintptr_t)entRef, s_tid_75f0);
        int64_t buildIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&buildIBP, buildComp);

        bool done;
        if (workIBP == 0) {
            done = true;
        } else {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workIBP);
            uintptr_t w = (uintptr_t)workIBP;
            void* desc = DerivedOr0(*reinterpret_cast<void**>(w + 0x18), s_tid_7040);
            int needed = desc ? (*reinterpret_cast<int*>((uintptr_t)desc + 0x30) * *reinterpret_cast<int*>(w + 0x4c)) : 0;
            done = (needed <= *reinterpret_cast<int*>(w + 0x48));
        }
        if (!done) continue;

        // done-path: look at the build's child entity (+0x10 IBP) for a
        // category (DWORD_1401c7d98) component; skip if category 6 or 7.
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildIBP);
        int64_t* child10 = reinterpret_cast<int64_t*>(buildIBP + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(child10);
        uintptr_t childEnt = (uintptr_t)*child10;
        void* catComp = FindComponentInEntity(childEnt, s_tid_7d98);
        int64_t catIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&catIBP, catComp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&catIBP);
        char cat = *reinterpret_cast<char*>(catIBP + 0x52);
        if (cat < 0) cat = *reinterpret_cast<char*>(catIBP + 0x50);
        if (cat == 6) continue;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&catIBP);
        cat = *reinterpret_cast<char*>(catIBP + 0x52);
        if (cat < 0) cat = *reinterpret_cast<char*>(catIBP + 0x50);
        if (cat == 7) continue;

        // second category fetch (local_50) — skip if 6.
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildIBP);
        int64_t* child10b = reinterpret_cast<int64_t*>(buildIBP + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(child10b);
        void* catComp2 = FindComponentInEntity((uintptr_t)*child10b, s_tid_7d98);
        int64_t catIBP2 = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&catIBP2, catComp2);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&catIBP2);
        char cat2 = *reinterpret_cast<char*>(catIBP2 + 0x52);
        if (cat2 < 0) cat2 = *reinterpret_cast<char*>(catIBP2 + 0x50);
        if (cat2 == 6) continue;

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildIBP);
        if (Sub_e710((uintptr_t)buildIBP) == 0) result++;
    }
    return (uint32_t)result;
}

// 0x1400d7cf0 — like 81b0 but on the done-path computes a max tool-tier over a
// second component (FUN_140004680 -> DWORD_1401c79e0; reads +0x2c/+0x2e shorts).
static uint32_t Helper_7cf0(uintptr_t /*self*/, void* extPtr) {
    uintptr_t param2 = (uintptr_t)extPtr;
    uintptr_t world2 = WorldPtr(0xf0);
    void* keyObj = DerivedOr0((void*)param2, s_tidCitizenAI);

    int bucketCount = 0;
    {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x160);
        while (node) {
            if ((uintptr_t)keyObj < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else if ((uintptr_t)keyObj <= node[0]) { bucketCount = (int)node[2]; break; }
            else node = reinterpret_cast<uint64_t*>(node[5]);
        }
    }
    int result = 0;
    for (int bi = 0; bi < bucketCount; bi++) {
        uint64_t* node = *reinterpret_cast<uint64_t**>(world2 + 0x160);
        int64_t entRef = 0;
        bool found = false;
        while (node) {
            if ((uintptr_t)keyObj < node[0]) node = reinterpret_cast<uint64_t*>(node[4]);
            else if ((uintptr_t)keyObj <= node[0]) { entRef = *reinterpret_cast<int64_t*>(node[1] + (int64_t)bi * 8); found = true; break; }
            else node = reinterpret_cast<uint64_t*>(node[5]);
        }
        if (!found) { if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&entRef, nullptr); }

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        void* workComp = FindComponentInEntity((uintptr_t)entRef, s_tid_7008);
        int64_t workIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&workIBP, workComp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        void* buildComp = FindComponentInEntity((uintptr_t)entRef, s_tid_75f0);
        int64_t buildIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&buildIBP, buildComp);

        bool done;
        if (workIBP == 0) {
            done = true;
        } else {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workIBP);
            uintptr_t w = (uintptr_t)workIBP;
            void* desc = DerivedOr0(*reinterpret_cast<void**>(w + 0x18), s_tid_7040);
            int needed = desc ? (*reinterpret_cast<int*>((uintptr_t)desc + 0x30) * *reinterpret_cast<int*>(w + 0x4c)) : 0;
            done = (needed <= *reinterpret_cast<int*>(w + 0x48));
        }
        if (!done) continue;

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildIBP);
        int64_t* child10 = reinterpret_cast<int64_t*>(buildIBP + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(child10);
        void* catComp = FindComponentInEntity((uintptr_t)*child10, s_tid_7d98);
        int64_t catIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&catIBP, catComp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&catIBP);
        char cat = *reinterpret_cast<char*>(catIBP + 0x52);
        if (cat < 0) cat = *reinterpret_cast<char*>(catIBP + 0x50);
        if (cat == 6) continue;
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&catIBP);
        cat = *reinterpret_cast<char*>(catIBP + 0x52);
        if (cat < 0) cat = *reinterpret_cast<char*>(catIBP + 0x50);
        if (cat == 7) continue;

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildIBP);
        int64_t* child10b = reinterpret_cast<int64_t*>(buildIBP + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(child10b);
        void* catComp2 = FindComponentInEntity((uintptr_t)*child10b, s_tid_7d98);
        int64_t catIBP2 = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&catIBP2, catComp2);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&catIBP2);
        char cat2 = *reinterpret_cast<char*>(catIBP2 + 0x52);
        if (cat2 < 0) cat2 = *reinterpret_cast<char*>(catIBP2 + 0x50);
        if (cat2 == 6) continue;

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&buildIBP);
        if (Sub_e710((uintptr_t)buildIBP) == 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
            int64_t tierIBP = 0;
            Find_4680((uintptr_t)entRef, &tierIBP);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&tierIBP);
            short s1 = *reinterpret_cast<short*>(tierIBP + 0x2c);
            short s2 = *reinterpret_cast<short*>(tierIBP + 0x2e);
            if (result < s1) result = s1;
            if (result < s2) result = s2;
        }
    }
    return (uint32_t)result;
}

// 0x1400d9650 — gather entities from three world sub-lists into one flat array,
// then for each (optionally filtered by owner == ext) find its Storage
// (DWORD_1401c7e78) component, walk the Storage's Profession (DWORD_1401c7cb8)
// children, and sum each child's quantity (slot +0x08) where the child's
// Profession flags (+0x5c) intersect self+0x78. ext is matched against the
// gathered entity's +0x28 (via its component-list base [+0x28]).
static uint32_t Helper_9650(uintptr_t self, void* extPtr) {
    void* owner = nullptr;
    if (extPtr != nullptr) owner = DerivedOr0(extPtr, s_tidCitizenAI);

    uintptr_t* world2 = reinterpret_cast<uintptr_t*>(World() + 0xf0);
    // Build a flat array of the three sub-lists' entity refs:
    //   world2[0]/[1]  (count world2[1])
    //   world2[2]/[3]
    //   world2[6]/[7]
    int total = 0;
    {
        int c0 = (int)world2[1], c1 = (int)world2[3], c2 = (int)world2[7];
        total = c0 + c1 + c2;
    }
    int64_t* gathered = nullptr;
    if (total > 0 && fn_MemAlloc) {
        gathered = reinterpret_cast<int64_t*>(fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)total * 8, 8));
    }
    int gi = 0;
    if (gathered) {
        int c0 = (int)world2[1];
        for (int i = 0; i < c0; i++) gathered[gi++] = *reinterpret_cast<int64_t*>(world2[0] + (int64_t)i * 8);
        int c1 = (int)world2[3];
        for (int i = 0; i < c1; i++) gathered[gi++] = *reinterpret_cast<int64_t*>(world2[2] + (int64_t)i * 8);
        int c2 = (int)world2[7];
        for (int i = 0; i < c2; i++) gathered[gi++] = *reinterpret_cast<int64_t*>(world2[6] + (int64_t)i * 8);
    }

    int result = 0;
    for (int i = 0; i < gi; i++) {
        int64_t entRef = gathered[i];
        if (owner != nullptr) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
            // entRef is the entity object ptr (used as such below); owner field
            // is at entity+0x28 — NOT *(entity)+0x28. (Matches the entityRef+0x28
            // owner filter in Helper_8d70 / Helper_81b0 / Helper_7cf0.)
            if (owner != *reinterpret_cast<void**>((uintptr_t)entRef + 0x28)) continue;
        }
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        uintptr_t entity = (uintptr_t)entRef;
        // find Storage (DWORD_1401c7e78) on the entity (entity +0x30/+0x38 layout).
        void* storageComp = FindComponentInEntity(entity, s_tidStorage);
        int64_t storageIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&storageIBP, storageComp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
        int childN = *reinterpret_cast<int*>(storageIBP + 0x30);
        for (int ci = 0; ci < childN; ci++) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&storageIBP);
            uintptr_t slot = *reinterpret_cast<uintptr_t*>(storageIBP + 0x28) + (int64_t)ci * 0x10;
            uintptr_t childObj = *reinterpret_cast<uintptr_t*>(slot);
            void* prof = FindComponentAt28(childObj, s_tidProfComp);
            uint32_t selfFlags = *reinterpret_cast<uint32_t*>(self + 0x78);
            uint32_t profFlags = prof ? *reinterpret_cast<uint32_t*>((uintptr_t)prof + 0x5c) : 0;
            if ((selfFlags & profFlags) != 0) {
                result += (int)*reinterpret_cast<int64_t*>(slot + 8);
            }
        }
    }
    if (gathered && fn_MemFreeStatic) fn_MemFreeStatic(gathered);
    return (uint32_t)result;
}

// ============================================================
// Dispatcher
// ============================================================
uint32_t StatGoal_StatValueDispatch(void* self, uint32_t statType, void* ext) {
    EnsureTypeIds();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    switch (statType) {
    case 0:    return Helper_a410();
    case 1:    return Helper_a2a0(s, ext);
    case 2:    return Helper_a150(s, ext);
    case 3:    return Helper_a0a0();
    case 4:    return Helper_9ff0();
    case 5:    return *reinterpret_cast<uint32_t*>(WorldPtr(0xf0) + 0xa8);
    case 6:    return Helper_9f20();
    case 7:    return Helper_9e80();
    case 8:    return Helper_9dc0();
    case 9:    return Helper_9cd0(s, ext);
    case 0xa:  return Helper_9c80(s, ext);
    case 0xb:  return Helper_9c10(s, ext);
    case 0xc:  return Helper_9b80();
    case 0xd:  return *reinterpret_cast<uint32_t*>(WorldPtr(0xf0) + 0x1ac);
    case 0xe:  return Helper_9650(s, ext);
    case 0xf:  return Helper_9560();
    case 0x10: return Helper_9470();
    case 0x11: return Helper_9380();
    case 0x12: return Helper_92d0(s, ext);
    case 0x13: return Helper_8d70(s, ext);
    case 0x14: return Helper_8c50(s);
    case 0x15: return Helper_88e0(s, ext);
    case 0x16: return Helper_86e0(s, ext);
    case 0x17: return Helper_86a0(s);
    case 0x18: return Helper_85f0(s, ext);
    case 0x19: return Helper_81b0(s, ext);
    case 0x1a: return **reinterpret_cast<uint32_t**>(World() + 0x128);
    case 0x1b: return *reinterpret_cast<uint32_t*>(World() + 0x34);
    case 0x1c: return *reinterpret_cast<uint32_t*>(WorldPtr(0xf0) + 0x1b0);
    case 0x1d: return Helper_7cf0(s, ext);
    default:   return 0;
    }
}
