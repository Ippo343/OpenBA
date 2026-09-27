// code/Game/Player/keystone_a_removalcalcpath.cpp
// Faithful 1:1 ports of RemovalTool::Slot13 (CalcPath, 0x1401035e0) and its
// previously-unregistered exe sub-callee closure.
//
// RemovalTool::Slot13 walks the rectangular tile selection (param_2..param_3
// corner cells) and, per tile, applies the active RemovalDescription's actions:
//   - building/item removal (FUN_1400c48a0 register a fresh ComponentEntity into
//     the world entity list when sub_48a0 says the tile is removable),
//   - road removal (RemovalTool_helper_c700 -> road BST + queue worker),
//   - mark-for-removal of the resident entity (sub_4770 + FUN_1400c2ea0 +
//     FUN_140003110),
//   - state-machine kick (sub_4670 -> StateMachine_impl_dfa0),
//   - citizen-AI notify (sub_43-flag -> FUN_14012ead0 + CitizenAI_impl_9ad0).
// On finishing the rect it rebuilds the road mesh once (TradeWork_impl_f370).
//
// This file ports the 6 functions whose exe sub-callee closure was NOT yet
// registered before this wave:
//   0x14002d960  RemovalTool_helper_d960  (FindComponent-by-type, type 0x1401c7430)
//   0x1400f8d50  RemovalTool_helper_8d50  (in-order AVL successor lookup by key)
//   0x14011e4d0  DestroyUI_helper_e4d0    (StateMachine slot+0x10 dispatch)
//   0x14011dfa0  StateMachine_impl_dfa0   (full StateMachine action-tick driver)
//   0x14009c700  RemovalTool_helper_c700  (road-removal cell handler)
//   0x1400f9ad0  CitizenAI_impl_9ad0      (citizen-AI per-entity AVL update)
//   0x1401035e0  RemovalTool_Slot13       (the CalcPath itself)
//
// Every other callee reached from these bodies is already registered elsewhere
// (ComponentEntity_CopyCtor 0x1400c3720, FUN_1400c48a0, FUN_1400c2ea0,
// FUN_140003110, FUN_140003ea0, FUN_140005780, FUN_14011e710, FUN_14012ead0,
// CitizenAI_impl_8e30 0x1400f8e30, CitizenAI_impl_a6e0 0x14001a6e0,
// CitizenAI_State_Leisure_sub_a8c0 0x14001a8c0, FUN_14009f2f0 0x14009f2f0,
// FUN_1400a8c90, FUN_1400a6fe0, StateMachine_Tick 0x1400a9660,
// RemovalTool_helper_8e30 etc.), is a Runtime export (engine_funcs fn ptrs), or a
// registered dispatch slot (static MemAlloc 0x140152b60, Time::Time 0x140152b00).
//
// Faithful offset-by-offset to the Ghidra disassembly. int*-arith trap handled:
// e.g. the decompile's `param_1 + 0x3a` (int*) == byte +0xe8; `param_1 + 0x20`
// (int*) == byte +0x80; confirmed in the disassembly.
//
// NOT autostart-exercised (player RemovalTool path) — Ghidra-verified only.

#include "Player/RemovalTool.h"
#include "engine_funcs.h"
#include "perf_terrain.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// PlayerPath removability checks (sub_48a0/4770/4670) are defined with C++ linkage in
// tool_base_path.cpp. Declare them at FILE SCOPE so the linkage matches — declaring them
// inside the extern "C" bodies below gives them C linkage and the link fails.
uint64_t PlayerPath_Sub_48a0(uintptr_t, void*, int, int);
uint64_t PlayerPath_Sub_4770(uintptr_t, void*);
uint64_t PlayerPath_Sub_4670(uintptr_t, void*);

// ============================================================
// Already-registered exe helpers we call via ResolveAddr (they route through the
// registry).
// ============================================================
typedef void* (*fn_copyCtor_t)(void* mem, void* desc);                       // 0x1400c3720
typedef void  (*fn_regEntity_t)(void* entityList, void* entityIBP, int cat);  // 0x1400c48a0
typedef void  (*fn_markRemoval_t)(int64_t entity);                            // 0x1400c2ea0
typedef void  (*fn_arrayPush110_t)(int64_t* arr, int idx, void* valIBP);      // 0x140003110
typedef void* (*fn_find3ea0_t)(int64_t entity, void* outIBP);                 // 0x140003ea0
typedef void* (*fn_find5780_t)(int64_t entity, void* outIBP);                 // 0x140005780
typedef char  (*fn_sub11e710_t)(int64_t obj);                                 // 0x14011e710
typedef char  (*fn_sub12ead0_t)(int64_t obj);                                 // 0x14012ead0
typedef uint64_t (*fn_roadMesh_f370_t)(int64_t game, void* map);              // 0x1400af370 (TradeWork_impl_f370)
typedef void* (*fn_find780_t)(int64_t entity, void* outIBP);                  // 0x140005780 (dup alias)
typedef void  (*fn_charAI8e30_t)(int64_t citizenAI, int cellId);             // 0x1400f8e30
typedef void  (*fn_avlErase_a6e0_t)(int64_t* root, void* node);             // 0x14001a6e0
typedef void  (*fn_avlBalance_a8c0_t)(int64_t* root, int64_t* node);        // 0x14001a8c0
typedef void  (*fn_smTick_a9660_t)(int64_t* stack, void* time, char a, char b); // 0x1400a9660
typedef void  (*fn_dirtyMark_a8c90_t)(int64_t self, int x, int y);          // 0x1400a8c90
typedef void  (*fn_queueWork_a6fe0_t)(void* thread, int cell, int two, uint64_t v); // 0x1400a6fe0
typedef void  (*fn_setCell_9f2f0_t)(int* self, int cell, uint32_t pass);     // 0x14009f2f0

static fn_copyCtor_t        s_copyCtor    = nullptr;
static fn_regEntity_t       s_regEntity   = nullptr;
static fn_markRemoval_t     s_markRemoval = nullptr;
static fn_arrayPush110_t    s_arrayPush   = nullptr;
static fn_find3ea0_t        s_find3ea0    = nullptr;
static fn_find5780_t        s_find5780    = nullptr;
static fn_sub11e710_t       s_sub11e710   = nullptr;
static fn_sub12ead0_t       s_sub12ead0   = nullptr;
static fn_roadMesh_f370_t   s_roadMesh    = nullptr;
static fn_charAI8e30_t      s_charAI8e30  = nullptr;
static fn_avlErase_a6e0_t   s_avlErase    = nullptr;
static fn_avlBalance_a8c0_t s_avlBalance  = nullptr;
static fn_smTick_a9660_t    s_smTick      = nullptr;
static fn_dirtyMark_a8c90_t s_dirtyMark   = nullptr;
static fn_queueWork_a6fe0_t s_queueWork   = nullptr;
static fn_setCell_9f2f0_t   s_setCell     = nullptr;

// static System::MemoryAllocator::MemAlloc — dispatch slot 0x140152b60 (SAPEAXII)
typedef void* (*fn_memAllocStatic_t)(uint64_t size, uint32_t align);
static fn_memAllocStatic_t s_memAlloc = nullptr;

// System::Time::Time(float) — dispatch slot 0x140152b00 (writes a Time at retbuf)
typedef void* (*fn_timeCtor_t)(void* retTime, float seconds);
static fn_timeCtor_t s_timeCtor = nullptr;

// type-ID storage globals (exe .rdata, filled by RegisterType after Startup)
static const uint32_t* s_tid_7430 = nullptr;  // RemovalTool_helper_d960
static const uint32_t* s_tid_79e0 = nullptr;  // BuildComponent slot (DWORD_1401c79e0)
static const uint32_t* s_tid_75f0 = nullptr;  // DestroyComponent (DWORD_1401c75f0)
static const uint32_t* s_tid_8070 = nullptr;  // StorageLocationComponent (DWORD_1401c8070)
static const uint32_t* s_tid_6860 = nullptr;  // TypeInfo_BuildRoad (0x1401c6860)

static bool s_init = false;

static inline void* SlotFn(uintptr_t slotAddr) {
    uintptr_t s = ResolveAddr(Module::Exe, slotAddr);
    return s ? *reinterpret_cast<void**>(s) : nullptr;
}

static void EnsureInit() {
    if (s_init) return;
    s_copyCtor    = reinterpret_cast<fn_copyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_regEntity   = reinterpret_cast<fn_regEntity_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_markRemoval = reinterpret_cast<fn_markRemoval_t>(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_arrayPush   = reinterpret_cast<fn_arrayPush110_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_find3ea0    = reinterpret_cast<fn_find3ea0_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_find5780    = reinterpret_cast<fn_find5780_t>(ResolveAddr(Module::Exe, 0x140005780));
    s_sub11e710   = reinterpret_cast<fn_sub11e710_t>(ResolveAddr(Module::Exe, 0x14011e710));
    s_sub12ead0   = reinterpret_cast<fn_sub12ead0_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_roadMesh    = reinterpret_cast<fn_roadMesh_f370_t>(ResolveAddr(Module::Exe, 0x1400af370));
    s_charAI8e30  = reinterpret_cast<fn_charAI8e30_t>(ResolveAddr(Module::Exe, 0x1400f8e30));
    s_avlErase    = reinterpret_cast<fn_avlErase_a6e0_t>(ResolveAddr(Module::Exe, 0x14001a6e0));
    s_avlBalance  = reinterpret_cast<fn_avlBalance_a8c0_t>(ResolveAddr(Module::Exe, 0x14001a8c0));
    s_smTick      = reinterpret_cast<fn_smTick_a9660_t>(ResolveAddr(Module::Exe, 0x1400a9660));
    s_dirtyMark   = reinterpret_cast<fn_dirtyMark_a8c90_t>(ResolveAddr(Module::Exe, 0x1400a8c90));
    s_queueWork   = reinterpret_cast<fn_queueWork_a6fe0_t>(ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_setCell     = reinterpret_cast<fn_setCell_9f2f0_t>(ResolveAddr(Module::Exe, 0x14009f2f0));

    s_memAlloc    = reinterpret_cast<fn_memAllocStatic_t>(SlotFn(0x140152b60));
    s_timeCtor    = reinterpret_cast<fn_timeCtor_t>(SlotFn(0x140152b00));

    s_tid_7430 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7430));
    s_tid_79e0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c79e0));
    s_tid_75f0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c75f0));
    s_tid_8070 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8070));
    s_tid_6860 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6860));
    s_init = true;
}

// Component virtual slot 2 (+0x10) = Type(). Helper to read it.
static inline uint32_t CompTypeId(void* comp) {
    return reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
}

// Shared FindComponent-by-type-id scan over an entity's component list
// (ptr array at entity+0x30, count at entity+0x38). Returns the first component
// whose GetTypeId matches *pTid and that IsDerivedFrom *pTid. Used by Slot13's
// inline scans (the LAB_... blocks).
static void* FindCompInEntity(uintptr_t entity, const uint32_t* pTid) {
    if (!pTid) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
        void* comp = *slot;
        if (CompTypeId(comp) == *pTid) {
            void* c = *slot;
            return (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTid)) ? c : nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// 0x14002d960  RemovalTool_helper_d960
// FindComponent-by-type over a *different* container layout: count at param_1+0x30,
// ptr-array base at param_1+0x28 (byte offsets confirmed in disasm — the array is
// indexed RSI=index*8 added to [param_1+0x28]). Type id DAT_1401c7430. Writes the
// match (or 0) into *param_2 and returns param_2.
// ============================================================
extern "C" void* RemovalCalc_helper_d960(int64_t param_1, void** param_2) {
    EnsureInit();
    void* result = nullptr;
    int n = *reinterpret_cast<int*>(param_1 + 0x30);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(param_1 + 0x28);
    for (int i = 0; i < n; i++) {
        void* comp = *reinterpret_cast<void**>(base + static_cast<int64_t>(i) * 8);
        if (s_tid_7430 && CompTypeId(comp) == *s_tid_7430) {
            void* c = *reinterpret_cast<void**>(base + static_cast<int64_t>(i) * 8);
            if (fn_IsDerivedFrom && s_tid_7430 && fn_IsDerivedFrom(c, *s_tid_7430))
                result = c;
            break;
        }
    }
    *param_2 = result;
    return param_2;
}

// ============================================================
// 0x1400f8d50  RemovalTool_helper_8d50
// AVL-tree in-order search keyed by the int at node->payload(+8)+0x28 == param_3.
// Tree layout: node+0x08 payload(IBP), node+0x10 left, node+0x18 right,
// node+0x20 parent. param_1+0x30 = tree root. The walk goes to the minimum node,
// then iterates the in-order successor chain until payload key == param_3 (writes
// the payload into *param_2) or exhausts (writes 0). Faithful to the decompile.
// ============================================================
extern "C" void* RemovalCalc_helper_8d50(int64_t param_1, void** param_2, int param_3) {
    EnsureInit();
    // Descend to leftmost node (min) starting from root.
    int64_t cur = *reinterpret_cast<int64_t*>(param_1 + 0x30);
    int64_t lo  = *reinterpret_cast<int64_t*>(param_1 + 0x30);
    while (cur != 0) {
        lo = cur;
        cur = *reinterpret_cast<int64_t*>(cur + 0x18);  // node+0x18 (decompile: +0x18)
    }
    // NOTE: the decompile descends via node+0x18 first then successor via +0x20 /
    // +0x10. We reproduce it byte-for-byte.
    int64_t node = lo;
    for (;;) {
        if (node == 0) {
            // FIX(lead-verify): disasm calls InternalBasePtr_Ctor(param_2, 0), not a
            // bare null store — callers expect a fully-constructed 16-byte IBP.
            if (fn_InternalBasePtr_Ctor) new (param_2) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            return param_2;
        }
        int64_t payload = *reinterpret_cast<int64_t*>(node + 8);
        if (*reinterpret_cast<int*>(payload + 0x28) == param_3) {
            *param_2 = nullptr;
            *param_2 = *reinterpret_cast<void**>(node + 8);
            return param_2;
        }
        // in-order successor
        int64_t right = *reinterpret_cast<int64_t*>(node + 0x20);
        if (right != 0) {
            int64_t s = right;
            do {
                lo = s;
                s = *reinterpret_cast<int64_t*>(lo + 0x18);
            } while (s != 0);
            node = lo;
            continue;
        }
        // climb up while node is the right child of its parent
        int64_t parent = *reinterpret_cast<int64_t*>(node + 0x10);
        if (parent != 0 && *reinterpret_cast<int64_t*>(parent + 0x18) != node) {
            int64_t p = parent;
            while (p != 0 && *reinterpret_cast<int64_t*>(p + 0x20) == node) {
                node = p;
                p = *reinterpret_cast<int64_t*>(p + 0x10);
            }
        }
        node = *reinterpret_cast<int64_t*>(node + 0x10);
    }
}

// ============================================================
// 0x14011e4d0  DestroyUI_helper_e4d0
// CheckPtr(param_1+0x10), FUN_140003ea0 on param_1+0x10 -> state, then dispatch
// the state's action-stack head (state+0x48) vtable+0x10 with
// (stackHead, &state+0x48, (int)state[+0x51], 1).
// ============================================================
extern "C" void RemovalCalc_helper_e4d0(int64_t param_1) {
    EnsureInit();
    int64_t resIBP[4] = {0};
    if (s_find3ea0) s_find3ea0(*reinterpret_cast<int64_t*>(param_1 + 0x10), resIBP);
    int64_t state = resIBP[0];
    if (!state) return;  // SA-guard: entity has no StateMachineComponent
    int64_t* stackHead = *reinterpret_cast<int64_t**>(state + 0x48);
    (*reinterpret_cast<void(**)(int64_t*, void*, int, int)>(*stackHead + 0x10))(
        stackHead, reinterpret_cast<void*>(state + 0x48),
        (int)*reinterpret_cast<char*>(state + 0x51), 1);
}

// ============================================================
// StateMachine action-stack tick helper. The driver runs two near-identical
// phases over the action stack at state+0x48; this reproduces one phase pass
// exactly as the decompile (LAB_14011e0..LAB_14011e181 and the +0x48
// LAB_14011e2..LAB_14011e351 copies are structurally identical with the only
// difference being which transition slot (+0x30) signature is used).
//
// `stackPtrField` points at state+0x48 (a longlong* holding the current node).
// `state` is the state object base. `useExtraArg` chooses the +0x30 call form
// (phase 1 = no extra; phase 2 = passes &outState). Faithful to disasm.
// ============================================================
static void SM_RunPhase(int64_t state, int64_t* stackPtrField, int32_t* outState) {
    bool first = true;
    if (*reinterpret_cast<char*>(state + 0x52) < 0)
        goto skipInner;
    do {
        do {
            if (-1 < *reinterpret_cast<char*>(state + 0x50)) {
                int it = (int)*reinterpret_cast<char*>(state + 0x51);
                int64_t* p = reinterpret_cast<int64_t*>(*stackPtrField);
                // FIX(lead 2026-07-20, bug-class #6): the node ABI carries the
                // state index in R8D (Ghidra drops it) — slot +0x20 is
                // Update(node, sm, idx). Without `it` the ControllerLive
                // dispatcher read garbage and bounds-rejected every dispatch.
                while (((it < 0) ||
                        ((*reinterpret_cast<char(**)(int64_t*, int64_t*, int)>(*p + 0x20))(p, stackPtrField, it) == 0)) &&
                       ((p = reinterpret_cast<int64_t*>(p[1])) != nullptr)) {
                    it = (*reinterpret_cast<int(**)(int64_t*, int)>(*p))(
                        p, (int)*reinterpret_cast<char*>(state + 0x50));
                }
                if (-1 < *reinterpret_cast<char*>(state + 0x53)) {
                    // FIX(lead-verify vs disasm 0x14011e0d7): this = N (*stackPtrField),
                    // func = *(*N + 0x28), arg2 = stackPtrField (the state+0x48 field
                    // address). The prior draft double-derefed (this=*N, func=*(*(*N)),
                    // arg2=N) — a crash-level pointer-level error.
                    int64_t* node = reinterpret_cast<int64_t*>(*stackPtrField);
                    (*reinterpret_cast<void(**)(int64_t*, int64_t*, int, int)>(*node + 0x28))(
                        node, stackPtrField,
                        (int)*reinterpret_cast<char*>(state + 0x51),
                        (int)*reinterpret_cast<char*>(state + 0x53));
                }
            }
            char cur52 = *reinterpret_cast<char*>(state + 0x52);
            // FIX(lead-verify vs disasm 0x14011e0f0 MOV RCX,[RBX]): head = N
            // (*stackPtrField), NOT *N. The prior draft derefed one level too far.
            int64_t head = *stackPtrField;
            *reinterpret_cast<char*>(state + 0x50) = cur52;
            // descend to the deepest child via node[2] chain
            int64_t deepest = head;
            for (int64_t c = *reinterpret_cast<int64_t*>(head + 0x10); c != 0;
                 c = *reinterpret_cast<int64_t*>(c + 0x10)) {
                deepest = c;
            }
            *stackPtrField = deepest;
            char st = (*reinterpret_cast<char(**)(int64_t, int)>(*reinterpret_cast<int64_t*>(deepest)))(
                deepest, (int)cur52);
            *reinterpret_cast<char*>(state + 0x51) = st;
            while (st < 0) {
                int64_t* nxt = *reinterpret_cast<int64_t**>(*stackPtrField + 8);
                *stackPtrField = reinterpret_cast<int64_t>(nxt);
                st = (*reinterpret_cast<char(**)(int64_t*, int)>(*nxt))(
                    nxt, (int)*reinterpret_cast<char*>(state + 0x50));
                *reinterpret_cast<char*>(state + 0x51) = st;
            }
            int it = (int)*reinterpret_cast<char*>(state + 0x51);
            int64_t* p = reinterpret_cast<int64_t*>(*stackPtrField);
            *reinterpret_cast<int16_t*>(state + 0x52) = (int16_t)0xffff;
            // FIX(lead 2026-07-20, bug-class #6): slot +0x18 is
            // Enter(node, sm, idx) — idx rides R8D, dropped by the decompile.
            while (((it < 0) ||
                    ((*reinterpret_cast<char(**)(int64_t*, int64_t*, int)>(*p + 0x18))(p, stackPtrField, it) == 0)) &&
                   ((p = reinterpret_cast<int64_t*>(p[1])) != nullptr)) {
                it = (*reinterpret_cast<int(**)(int64_t*, int)>(*p))(
                    p, (int)*reinterpret_cast<char*>(state + 0x50));
            }
        } while (-1 < *reinterpret_cast<char*>(state + 0x52));
    skipInner:
        if (first && (-1 < *reinterpret_cast<char*>(state + 0x50))) {
            int it = (int)*reinterpret_cast<char*>(state + 0x51);
            int64_t* p = reinterpret_cast<int64_t*>(*stackPtrField);
            // FIX(lead-verify vs disasm 0x14011e199 / 0x14011e369): BOTH phases' +0x30
            // call pass &outState (LEA R8,[RBP+0x20] in each). The prior "phase 1 =
            // 2-arg" form was a decompiler artifact that left R8 uninitialized.
            // FIX(lead 2026-07-20, bug-class #6): slot +0x30 is
            // Transition(node, sm, time/out, idx) — idx rides R9D, also dropped.
            while (((it < 0) ||
                    ((*reinterpret_cast<char(**)(int64_t*, int64_t*, int32_t*, int)>(*p + 0x30))(
                        p, stackPtrField, outState, it) == 0)) &&
                   ((p = reinterpret_cast<int64_t*>(p[1])) != nullptr)) {
                it = (*reinterpret_cast<int(**)(int64_t*, int)>(*p))(
                    p, (int)*reinterpret_cast<char*>(state + 0x50));
            }
            first = false;
        }
    } while (-1 < *reinterpret_cast<char*>(state + 0x52));
}

// ============================================================
// 0x14011dfa0  StateMachine_impl_dfa0
// State machine action driver. Reads the state object (FUN_140003ea0 on
// self+0x10), short-circuits when its top state byte (+0x52/+0x50 fallback) is 6
// (first two checks) or 7. The FIRST ==6 returns with no cleanup (JZ 0x14011e3b9);
// the SECOND ==6 and the ==7 exits jump to 0x14011e3b1 which calls
// DestroyUI_helper_e4d0(param_1) first. Otherwise runs the two-phase action tick.
// Faithful to the decompile control flow.
// ============================================================
extern "C" void RemovalCalc_StateMachine_dfa0(int64_t param_1) {
    EnsureInit();
    int32_t local_res8 = 0;   // CONCAT44(uStackX_c,local_res8) -> state ptr (8 bytes)
    int32_t uStackX_c = 0;
    int64_t local_res10 = 0;
    // Retail reuses ONE 8-byte stack slot as both the Time(0.0) buffer and the 4-byte
    // outState the transition handlers write. The port previously modeled that slot as
    // the int32 local_res8 — and System::Time::Time writes 8 bytes, clobbering whatever
    // the compiler placed in the adjacent 4 bytes. Observed live 2026-07-26 (debug
    // build): the adjacent local was local_res10 (the SMC pointer), its low dword was
    // zeroed to 0x2b900000000, and SM_RunPhase faulted reading +0x52 off it on the
    // first upgrade-click pump. An explicit 8-byte slot with the outState aliased to
    // its low dword reproduces the retail frame exactly and is layout-proof.
    int64_t timeOutSlot = 0;
    int32_t* outState = reinterpret_cast<int32_t*>(&timeOutSlot);

    {
        int64_t buf = 0;
        if (s_find3ea0) s_find3ea0(*reinterpret_cast<int64_t*>(param_1 + 0x10), &buf);
        local_res8  = (int32_t)(uint64_t)buf;
        uStackX_c   = (int32_t)((uint64_t)buf >> 32);
    }
    int64_t st0 = ((int64_t)(uint32_t)local_res8) | ((int64_t)uStackX_c << 32);
    if (!st0) {  // SA-guard: no StateMachineComponent on the entity (crashed
                 // live 2026-07-20 on the first upgrade click, pre-7d98 fill)
        static bool warned = false;
        if (!warned) {
            warned = true;
            fprintf(stderr, "[SM-dfa0] entity has no StateMachineComponent — "
                            "pump skipped\n");
            fflush(stderr);
        }
        return;
    }
    char cVar5 = *reinterpret_cast<char*>(st0 + 0x52);
    if (cVar5 < 0) cVar5 = *reinterpret_cast<char*>(st0 + 0x50);
    if (cVar5 == 6) return;

    {
        int64_t buf = 0;
        if (s_find3ea0) s_find3ea0(*reinterpret_cast<int64_t*>(param_1 + 0x10), &buf);
        local_res8  = (int32_t)(uint64_t)buf;
        uStackX_c   = (int32_t)((uint64_t)buf >> 32);
    }
    int64_t st1 = ((int64_t)(uint32_t)local_res8) | ((int64_t)uStackX_c << 32);
    cVar5 = *reinterpret_cast<char*>(st1 + 0x52);
    if (cVar5 < 0) cVar5 = *reinterpret_cast<char*>(st1 + 0x50);
    // FIX(lead-verify vs disasm 0x14011e01a JZ 0x14011e3b1): the 2nd ==6 exit calls e4d0.
    if (cVar5 == 6) { RemovalCalc_helper_e4d0(param_1); return; }

    cVar5 = *reinterpret_cast<char*>(st1 + 0x52);
    if (cVar5 < 0) cVar5 = *reinterpret_cast<char*>(st1 + 0x50);
    // FIX(lead-verify vs disasm 0x14011e03c JZ 0x14011e3b1): the ==7 exit calls e4d0.
    if (cVar5 == 7) { RemovalCalc_helper_e4d0(param_1); return; }

    // ---- main tick path ----
    if (s_find3ea0) s_find3ea0(*reinterpret_cast<int64_t*>(param_1 + 0x10), &local_res10);
    // Time(0.0) into the shared 8-byte slot (see timeOutSlot note above).
    if (s_timeCtor) s_timeCtor(&timeOutSlot, 0.0f);
    int64_t state = local_res10;
    int64_t* stackPtrField = reinterpret_cast<int64_t*>(state + 0x48);

    // Phase 1 (transition slot +0x30 passes the shared slot, same as phase 2)
    SM_RunPhase(state, stackPtrField, outState);

    // ---- between-phase: choose transition + kick Tick ----
    state = local_res10;
    *outState = 3;  // outState seed
    if (*reinterpret_cast<char*>(state + 0x51) < 0) {
        int64_t timeBuf[1] = {0};
        if (s_timeCtor) s_timeCtor(timeBuf, 0.0f);
        if (s_smTick) s_smTick(reinterpret_cast<int64_t*>(state + 0x48), timeBuf, 0, 0);
    }
    {
        int64_t* head = *reinterpret_cast<int64_t**>(state + 0x48);
        int iVar8 = (*reinterpret_cast<int(**)(int64_t*, int, int32_t*)>(*head + 8))(
            head, (int)*reinterpret_cast<char*>(state + 0x51), outState);
        if (-1 < iVar8) {
            *reinterpret_cast<char*>(state + 0x53) = (char)iVar8;
            *reinterpret_cast<char*>(state + 0x52) = (char)(uint32_t)*outState;
        }
    }
    if (s_timeCtor) s_timeCtor(&timeOutSlot, 0.0f);
    state = local_res10;
    stackPtrField = reinterpret_cast<int64_t*>(state + 0x48);

    // Phase 2 (transition slot +0x30 passes the shared slot)
    SM_RunPhase(state, stackPtrField, outState);
}

// ============================================================
// 0x14009c700  RemovalTool_helper_c700
// Road-removal cell handler. param_1 = map grid (RSI). Walks the road BST root at
// byte +0xe8 by cell id param_2; on a found road node, resolves the BuildRoad
// component (RemovalTool_helper_8d50 + IsA TypeInfo_BuildRoad), reads its
// remove-kind fields (+0x60,+0x64,+0x68), notifies CitizenAI (FUN_1400f8e30) and
// erases the BST node (CitizenAI_impl_a6e0). Then updates the map cell at +0x80
// (stride 0x30): if cell+0x2a==0xff it dirties the region (FUN_1400a8c90) and
// queues a worker job (FUN_1400a6fe0); else applies the removal (FUN_14009f2f0)
// and rebuilds the road mesh (TradeWork_impl_f370). Returns bool.
// int*-arith: +0x3a==byte 0xe8 root; +0x1c==0x70; +0x1a==0x68; +0x20==0x80.
// ============================================================
extern "C" uint64_t RemovalCalc_helper_c700(uint32_t* param_1, int param_2) {
    EnsureInit();
    int* root = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(param_1) + 0xe8);
    void* ptr = nullptr;
    uint64_t retLow = 2;
    int removeKind = 0xff;   // iVar9 / R12D
    uint8_t ebx = 0;         // local res @ +0x64 read (R13/EBX region) — overload note below
    int32_t res8 = 0;        // local_res8 (InternalBasePtr storage)
    int32_t resC = 0;        // uStackX_c
    uint32_t res10 = 2;      // local_res10
    uint8_t r7 = 0;          // uVar7 byte
    bool found = false;      // bVar5
    uint8_t r13 = 1;         // bVar10 (subtract amount); EBP/R13 start = 1
    void* res18 = nullptr;   // local_res18 (InternalBasePtr)

    int* node = root;
    while (node != nullptr) {
        if (param_2 < *node) {
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x10);
        } else if (*node < param_2) {
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);
        } else {
            // exact match — node found (key == param_2)
            // RemovalTool_helper_8d50(gameWorld+0x118, &res18, param_2)
            RemovalCalc_helper_8d50(
                *reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0x118),
                reinterpret_cast<void**>(&res18), param_2);
            r13 = 1;  // bVar10 = true by default
            if (res18 != nullptr) {
                bool isRoad = (fn_IsA && s_tid_6860) ? fn_IsA(res18, *s_tid_6860) : false;
                if (isRoad) {
                    void* base = res18;
                    void* derived = (fn_IsDerivedFrom && s_tid_6860 && fn_IsDerivedFrom(base, *s_tid_6860))
                                    ? base : nullptr;
                    int64_t bp = ((int64_t)(uint32_t)res8) | ((int64_t)resC << 32);
                    if (fn_InternalBasePtr_Ctor) {
                        void* tmp = derived;
                        // fn_InternalBasePtr_Ctor(&res8, derived)
                        int64_t ibpBuf = 0;
                        new (&ibpBuf) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(tmp));   // B.3.3 bridge
                        res8 = (int32_t)(uint64_t)ibpBuf;
                        resC = (int32_t)((uint64_t)ibpBuf >> 32);
                    }
                    bp = ((int64_t)(uint32_t)res8) | ((int64_t)resC << 32);
                    removeKind = *reinterpret_cast<int*>(bp + 0x60);
                    bp = ((int64_t)(uint32_t)res8) | ((int64_t)resC << 32);
                    ebx = (uint8_t)*reinterpret_cast<uint32_t*>(bp + 0x64);  // ptr = *(uint*)(bp+100)
                    bp = ((int64_t)(uint32_t)res8) | ((int64_t)resC << 32);
                    res10 = *reinterpret_cast<uint32_t*>(bp + 0x68);
                    if (s_charAI8e30)
                        s_charAI8e30(*reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0x118),
                                     *reinterpret_cast<int*>((uintptr_t)res18 + 0x20));
                    r13 = (removeKind == 0xff) ? 1 : 0;  // bVar10 = (removeKind==0xff)
                } else {
                    ebx = (uint8_t)(uint32_t)res8;
                    r13 = 1;
                }
            } else {
                // LAB_14009c85e: res18 == null -> EBX = local_res8 low byte
                ebx = (uint8_t)(uint32_t)res8;
            }
            r7 = ebx;  // uVar7 = SUB81(ptr,0); ptr held the +0x64 byte for the road branch

            // second BST pass: erase the matched node (CitizenAI_impl_a6e0)
            int* n2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(param_1) + 0xe8);
            while (n2 != nullptr) {
                if (param_2 < *n2) {
                    n2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(n2) + 0x10);
                } else if (*n2 < param_2) {
                    n2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(n2) + 0x18);
                } else {
                    if (s_avlErase)
                        s_avlErase(reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(param_1) + 0xe8), n2);
                    break;
                }
            }
            retLow = res10;
            found = true;
            (void)ptr; (void)res8;
            break;
        }
    }

    // ---- LAB_14009c8a0: apply to map cell ----
    int64_t cell = (int64_t)param_2 * 0x30 + *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(param_1) + 0x80);
    uint64_t result;
    if (*reinterpret_cast<int8_t*>(cell + 0x2a) == (int8_t)0xff) {
        uint8_t bVar1 = *reinterpret_cast<uint8_t*>(cell + 0x29);
        uint8_t setBit = ((1u << (bVar1 & 0x1f)) & 0x1e0u) != 0 ? 1 : 0;
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(param_1) + 0xa2) |= setBit;
        *reinterpret_cast<uint8_t*>(cell + 0x29) = 2;
        PerfX::TerrainNoteWrite(reinterpret_cast<const uint8_t*>(cell));   // [PerfX] mirror
        if (((1u << (bVar1 & 0x1f)) & 0x1e0u) != 0) {
            int div = (int)*param_1;
            if (s_dirtyMark)
                s_dirtyMark(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(param_1) + 0x70),
                            param_2 % div, param_2 / div);
        }
        if (s_queueWork)
            s_queueWork(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(param_1) + 0x68),
                        param_2, 2, retLow);
        result = 0;  // uVar8 & 0xff..00
    } else {
        if (found) {
            *reinterpret_cast<int8_t*>(cell + 0x28) -= (int8_t)r13;
            *reinterpret_cast<int8_t*>(cell + 0x2a) = (int8_t)removeKind;
            *reinterpret_cast<uint8_t*>(cell + 0x2b) = r7;
            if (s_setCell) s_setCell(reinterpret_cast<int*>(param_1), param_2, (uint32_t)retLow);
            if (removeKind == 0xff) {
                result = 0;
            } else {
                if (s_roadMesh)
                    s_roadMesh(*reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0x110), param_1);
                result = 1;
            }
        } else {
            result = 1;
        }
    }
    return result;
}

// ============================================================
// 0x1400f9ad0  CitizenAI_impl_9ad0
// Per-entity citizen-AI AVL update. param_1 = CitizenAI subsystem, param_2 = cell
// id. Finds the entity node in the BST at param_1+0x30 (key == *node, left=+0x18
// i.e. piVar4+6, right=+0x20 i.e. piVar4+8 — NOTE this tree uses the opposite
// child convention from c700). Then re-keys an id AVL at param_1+0x10 by the
// found entity's id (+0x24), inserting/erasing as needed; bumps count at +0x18.
// Faithful to the decompile (int*-arith: piVar+6==+0x18, piVar+8==+0x20,
// piVar+4==+0x10, piVar+2==+0x08; *self == node payload at +0x08).
// ============================================================
extern "C" void RemovalCalc_CitizenAI_9ad0(int64_t param_1, int param_2) {
    EnsureInit();
    int* piVar4 = *reinterpret_cast<int**>(param_1 + 0x30);
    if (piVar4 == nullptr) return;

    for (;;) {
        if (param_2 < *piVar4) {
            piVar4 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(piVar4) + 0x18);  // piVar4+6
        } else if (*piVar4 < param_2) {
            piVar4 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(piVar4) + 0x20);  // piVar4+8
        } else {
            // exact match
            if (piVar4 == nullptr) return;
            int64_t* self = reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(piVar4) + 8); // piVar4+2
            int64_t* puVar7 = nullptr;
            int iVar6 = 0;
            if (*reinterpret_cast<int*>(param_1 + 0x18) != 0) {
                int* p3 = *reinterpret_cast<int**>(param_1 + 0x10);
                int* p4 = *reinterpret_cast<int**>(param_1 + 0x10);
                while (p3 != nullptr) {
                    int* p2 = p3;
                    p4 = p2;
                    p3 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(p2) + 0x10);  // piVar2+4
                }
                iVar6 = *p4 + -1;
            }
            // erase any node already keyed by the entity id (+0x24)
            int* pk = *reinterpret_cast<int**>(param_1 + 0x10);
            int64_t* plVar8 = reinterpret_cast<int64_t*>(param_1 + 0x10);
            while (pk != nullptr) {
                int entId = *reinterpret_cast<int*>(*self + 0x24);
                if (entId < *pk) {
                    pk = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(pk) + 0x10);  // pk+4
                } else if (*pk < entId) {
                    pk = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(pk) + 0x18);  // pk+6
                } else {
                    if (s_avlErase) s_avlErase(plVar8, pk);
                    break;
                }
            }
            *reinterpret_cast<int*>(*self + 0x24) = iVar6;
            uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*self + 0x20);

            int* cur = reinterpret_cast<int*>(*plVar8);
            if (cur == nullptr) {
                int64_t* nodeMem = reinterpret_cast<int64_t*>(s_memAlloc ? s_memAlloc(0x28, 8) : nullptr);
                if (nodeMem != nullptr) {
                    nodeMem[1] = 0; nodeMem[2] = 0; nodeMem[3] = 0;
                    *reinterpret_cast<int32_t*>(nodeMem + 4) = 0;
                    nodeMem[0] = ((int64_t)(uint32_t)iVar6) | ((int64_t)uVar1 << 32);
                    puVar7 = nodeMem;
                }
                *plVar8 = reinterpret_cast<int64_t>(puVar7);
            } else {
                int* localRes18 = cur;
                int* prev;
                for (;;) {
                    bool inserted = false;
                    do {
                        localRes18 = cur;
                        prev = localRes18;
                        if (iVar6 < *localRes18) {
                            cur = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(localRes18) + 0x10); // +4
                            if (cur == nullptr) {
                                int64_t* nodeMem = reinterpret_cast<int64_t*>(s_memAlloc ? s_memAlloc(0x28, 8) : nullptr);
                                if (nodeMem != nullptr) {
                                    nodeMem[2] = 0; nodeMem[3] = 0;
                                    *reinterpret_cast<int32_t*>(nodeMem + 4) = 0;
                                    nodeMem[1] = reinterpret_cast<int64_t>(prev);
                                    nodeMem[0] = ((int64_t)(uint32_t)iVar6) | ((int64_t)uVar1 << 32);
                                    puVar7 = nodeMem;
                                }
                                *reinterpret_cast<int64_t**>(reinterpret_cast<uintptr_t>(prev) + 0x10) = puVar7; // prev+4
                                inserted = true;
                                goto balance;
                            }
                        } else {
                            break;
                        }
                    } while (true);
                    if (iVar6 <= *localRes18) {
                        return;  // already present
                    }
                    cur = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(localRes18) + 0x18); // +6
                    if (cur == nullptr) {
                        int64_t* nodeMem = reinterpret_cast<int64_t*>(s_memAlloc ? s_memAlloc(0x28, 8) : nullptr);
                        if (nodeMem != nullptr) {
                            nodeMem[2] = 0; nodeMem[3] = 0;
                            *reinterpret_cast<int32_t*>(nodeMem + 4) = 0;
                            nodeMem[1] = reinterpret_cast<int64_t>(prev);
                            nodeMem[0] = ((int64_t)(uint32_t)iVar6) | ((int64_t)uVar1 << 32);
                            puVar7 = nodeMem;
                        }
                        *reinterpret_cast<int64_t**>(reinterpret_cast<uintptr_t>(prev) + 0x18) = puVar7; // prev+6
                        goto balance;
                    }
                    (void)inserted;
                }
            balance:
                {
                    int64_t res18local = reinterpret_cast<int64_t>(prev);
                    if (s_avlBalance) s_avlBalance(plVar8, &res18local);
                }
            }
            *reinterpret_cast<int*>(param_1 + 0x18) = *reinterpret_cast<int*>(param_1 + 0x18) + 1;
            return;
        }
        if (piVar4 == nullptr) return;
    }
}

// ============================================================
// 0x1401035e0  RemovalTool_Slot13 (CalcPath)
// Walks the selection rectangle (param_2 corner, param_3 corner) over the tile
// grid and, per cell, applies the active RemovalDescription. See file header.
// Faithful to the decompile.
// ============================================================
bool RemovalTool::CalcPath(int param_2, int param_3) {
    EnsureInit();

    // Decompile variable names preserved verbatim for offset-by-offset fidelity.
    int* piVarDim = *reinterpret_cast<int**>((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    int dim = *piVarDim;

    int64_t uVar13 = (int64_t)param_2 / dim;   // row of corner A
    int64_t uVar26 = (int64_t)param_2 % dim;   // col of corner A
    uint32_t uVar9 = (uint32_t)uVar26;
    int64_t uVar25 = uVar13 & 0xffffffff;
    int64_t uVar16 = (int64_t)param_3 / dim;   // row of corner B
    int64_t uVar3  = (int64_t)param_3 % dim;   // col of corner B
    uint32_t local_res18 = (uint32_t)uVar3;
    int64_t uVar4 = uVar26;
    uint32_t local_res8 = uVar9;
    if ((int)local_res18 < (int)uVar9) {
        uVar4 = uVar3;
        uVar3 = uVar26;
        local_res8 = local_res18;
        local_res18 = uVar9;
    }
    int64_t uVar22 = uVar3 & 0xffffffff;       // inner-loop col end
    int64_t uVar20 = uVar4 & 0xffffffff;       // inner-loop col start
    uVar9 = (uint32_t)uVar13;
    uVar13 = uVar16 & 0xffffffff;              // outer-loop row end
    uVar26 = uVar25;                           // outer-loop row start
    uint32_t local_res20 = (uint32_t)uVar16;
    if ((int)(uint32_t)uVar16 < (int)uVar9) {
        uVar26 = uVar16 & 0xffffffff;
        uVar13 = uVar25;
        local_res20 = uVar9;
    }

    // single-cell drag-snapshot validity (lVar21+0x80 flag)
    int64_t lVar21 = reinterpret_cast<int64_t>(dialogPtr);
    if (*reinterpret_cast<char*>(lVar21 + 0x80) != 0) {
        int iVar24 = dim;
        int64_t uVar16b = (int64_t)*reinterpret_cast<int*>(lVar21 + 0x9c) / iVar24;
        if (((int)uVar4 != *reinterpret_cast<int*>(lVar21 + 0x98) % iVar24) ||
            ((int)uVar26 != *reinterpret_cast<int*>(lVar21 + 0x98) / iVar24) ||
            (*reinterpret_cast<int*>(lVar21 + 0x9c) % iVar24 != (int)uVar3) ||
            ((int)uVar16b != (int)uVar13)) {
            return false;   // AL = 0 (retail: uVar16b & ~0xff in RAX; callers test AL)
        }
    }

    // RemovalTool_helper_d960 on the active desc's container (desc+0x38), if any
    if (*reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(removalDesc) + 0x38) != 0) {
        int64_t tmp58 = 0;
        RemovalCalc_helper_d960(*reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(removalDesc) + 0x38),
                                reinterpret_cast<void**>(&tmp58));
        uVar22 = (int64_t)local_res18;
    }

    uint32_t* puVar18 = *reinterpret_cast<uint32_t**>((uintptr_t)*g_pGameWorldSingleton + 0xf8);
    uint32_t* local_a0 = puVar18;
    bool bVar5 = false;          // roadMeshDirty
    uint64_t uVar16ret = 0;

    if ((int)uVar26 <= (int)uVar13) {
        do {
            int iVar24 = (int)uVar26;  // current row
            if ((int)uVar20 <= (int)uVar22) {
                do {
                    int iVar23 = (int)uVar20;  // current col
                    int iRow = iVar24;
                    int iCol = iVar23;
                    int cellId = (int)(*puVar18 * iVar24 + iVar23);
                    int64_t* cellSlot = reinterpret_cast<int64_t*>(
                        (int64_t)cellId * 0x30 + *reinterpret_cast<int64_t*>(puVar18 + 0x20));

                    // resolve the entity occupying this cell into local_e8 (IBP)
                    int64_t local_e8;
                    if (cellSlot[0] == 0) {
                        if (cellSlot[1] == 0) {
                            int64_t buf = 0;
                            if (fn_InternalBasePtr_Ctor) new (&buf) ::Object::InternalBasePtr(
                                nullptr);   // B.3.3 bridge
                            local_e8 = buf;
                        } else {
                            local_e8 = cellSlot[1];
                        }
                    } else {
                        local_e8 = cellSlot[0];
                    }
                    int64_t local_e0 = local_e8;

                    // ---- building/item removal (sub_48a0) ----
                    if ((char)PlayerPath_Sub_48a0(reinterpret_cast<uintptr_t>(this), &local_e0, iCol, iRow) != 0) {
                        void* mem = s_memAlloc ? s_memAlloc(0x48, 8) : nullptr;
                        void* cloned = nullptr;
                        if (mem) cloned = s_copyCtor(mem,
                            *reinterpret_cast<void**>(reinterpret_cast<int64_t>(removalDesc) + 0x38));
                        int64_t local_90 = 0;
                        if (fn_InternalBasePtr_Ctor) new (&local_90) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(cloned));   // B.3.3 bridge
                        void* comp = FindCompInEntity(local_90, s_tid_79e0);
                        int64_t local_b0 = 0;
                        if (fn_InternalBasePtr_Ctor) new (&local_b0) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
                        *reinterpret_cast<int16_t*>(local_b0 + 0x28) = (int16_t)iVar23;
                        *reinterpret_cast<int16_t*>(local_b0 + 0x2a) = (int16_t)iVar24;
                        *reinterpret_cast<int64_t*>(local_b0 + 0x2c) = 0x10001;
                        int64_t local_70 = local_90;
                        if (s_regEntity)
                            s_regEntity(*reinterpret_cast<void**>((uintptr_t)*g_pGameWorldSingleton + 0xe8),
                                        &local_70,
                                        *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(removalDesc) + 0x38) + 0x20));
                        puVar18 = local_a0;
                    }

                    // ---- road removal (flag desc+0x40) ----
                    int* piVar15 = *reinterpret_cast<int**>((uintptr_t)*g_pGameWorldSingleton + 0xf8);
                    if (*reinterpret_cast<char*>(reinterpret_cast<int64_t>(removalDesc) + 0x40) != 0) {
                        int cid = *piVar15 * iRow + iCol;
                        int64_t cl = (int64_t)cid * 0x30 + *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(piVar15) + 0x80);
                        uint32_t mask = 1u << (*reinterpret_cast<uint8_t*>(cl + 0x29) & 0x1f);
                        bool hit = false;
                        if (((mask & 2) == 0) && ((mask & 1) == 0) && (*reinterpret_cast<int8_t*>(cl + 0x2a) == (int8_t)-1)) {
                            int* bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(piVar15) + 0xe8); // piVar15+0x3a
                            while (bn != nullptr) {
                                if (cid < *bn) {
                                    bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(bn) + 0x10); // +4
                                } else if (*bn < cid) {
                                    bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(bn) + 0x18); // +6
                                } else { hit = true; break; }
                            }
                        } else {
                            hit = true;
                        }
                        if (hit) {
                            uint64_t rc = RemovalCalc_helper_c700(reinterpret_cast<uint32_t*>(piVar15),
                                                                  *piVar15 * iRow + iCol);
                            if ((char)rc == 0) {
                                bVar5 = true;
                            } else {
                                void* mem = s_memAlloc ? s_memAlloc(0x48, 8) : nullptr;
                                void* cloned = nullptr;
                                if (mem) cloned = s_copyCtor(mem,
                                    *reinterpret_cast<void**>(reinterpret_cast<int64_t>(removalDesc) + 0x38));
                                int64_t local_a8 = 0;
                                if (fn_InternalBasePtr_Ctor) new (&local_a8) ::Object::InternalBasePtr(
                                    reinterpret_cast<RtObject::Internal*>(cloned));   // B.3.3 bridge
                                void* comp = FindCompInEntity(local_a8, s_tid_79e0);
                                int64_t local_98 = 0;
                                if (fn_InternalBasePtr_Ctor) new (&local_98) ::Object::InternalBasePtr(
                                    reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
                                *reinterpret_cast<int16_t*>(local_98 + 0x28) = (int16_t)iVar23;
                                *reinterpret_cast<int16_t*>(local_98 + 0x2a) = (int16_t)iVar24;
                                *reinterpret_cast<int64_t*>(local_98 + 0x2c) = 0x10001;
                                int64_t local_78 = local_a8;
                                if (s_regEntity)
                                    s_regEntity(*reinterpret_cast<void**>((uintptr_t)*g_pGameWorldSingleton + 0xe8),
                                                &local_78,
                                                *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(removalDesc) + 0x38) + 0x20));
                            }
                        }
                    }

                    // ---- mark-for-removal of resident entity (sub_4770) ----
                    local_e0 = local_e8;
                    if ((char)PlayerPath_Sub_4770(reinterpret_cast<uintptr_t>(this), &local_e0) != 0) {
                        if (*reinterpret_cast<int64_t*>(local_e8 + 0x28) ==
                            *reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(removalDesc) + 0x38)) {
                            local_e0 = local_e8;
                            int64_t lst = *reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0xe8);
                            if ((*reinterpret_cast<uint32_t*>(local_e0 + 0x40) >> 10 & 1) == 0) {
                                if (s_markRemoval) s_markRemoval(local_e0);
                                if (s_arrayPush)
                                    s_arrayPush(reinterpret_cast<int64_t*>(lst + 0xd0),
                                                *reinterpret_cast<int*>(lst + 0xd8) + 1, &local_e0);
                            }
                        } else {
                            void* comp = FindCompInEntity(local_e8, s_tid_75f0);
                            int64_t local_80 = 0;
                            if (fn_InternalBasePtr_Ctor) new (&local_80) ::Object::InternalBasePtr(
                                reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
                            if (local_80 != 0) {
                                int64_t local_60[16] = {0};
                                int64_t* pi = reinterpret_cast<int64_t*>(s_find5780 ? (s_find5780(local_e8, local_60), local_60) : local_60);
                                if (s_sub11e710 && (char)s_sub11e710(*pi) != 0) {
                                    int64_t local_50[16] = {0};
                                    int64_t* pi2 = reinterpret_cast<int64_t*>(s_find5780 ? (s_find5780(local_e8, local_50), local_50) : local_50);
                                    int64_t lr = *pi2;
                                    int64_t local_88buf = 0;
                                    if (s_find3ea0) s_find3ea0(*reinterpret_cast<int64_t*>(lr + 0x10), &local_88buf);
                                    int64_t* head = *reinterpret_cast<int64_t**>(local_88buf + 0x48);
                                    (*reinterpret_cast<void(**)(int64_t*, void*, int, int)>(*head + 0x10))(
                                        head, reinterpret_cast<void*>(local_88buf + 0x48),
                                        (int)*reinterpret_cast<char*>(local_88buf + 0x51), 0);
                                }
                            }
                        }
                    }

                    // ---- StateMachine kick (sub_4670) ----
                    int64_t local_b8 = local_e8;
                    if ((char)PlayerPath_Sub_4670(reinterpret_cast<uintptr_t>(this), &local_b8) != 0) {
                        void* comp = FindCompInEntity(local_e8, s_tid_75f0);
                        int64_t local_c0 = 0;
                        if (fn_InternalBasePtr_Ctor) new (&local_c0) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
                        RemovalCalc_StateMachine_dfa0(local_c0);
                    }

                    // ---- citizen-AI notify (flag desc+0x43) ----
                    local_e0 = local_e8;
                    if ((*reinterpret_cast<char*>(reinterpret_cast<int64_t>(removalDesc) + 0x43) != 0) &&
                        (local_e8 != 0)) {
                        void* comp = FindCompInEntity(local_e0, s_tid_8070);
                        int64_t local_d0 = 0;
                        if (fn_InternalBasePtr_Ctor) new (&local_d0) ::Object::InternalBasePtr(
                            reinterpret_cast<RtObject::Internal*>(comp));   // B.3.3 bridge
                        if (local_d0 != 0) {
                            if (s_sub12ead0 && s_sub12ead0(local_d0)) {
                                void* comp2 = FindCompInEntity(local_e8, s_tid_8070);
                                int64_t local_d8 = 0;
                                if (fn_InternalBasePtr_Ctor) new (&local_d8) ::Object::InternalBasePtr(
                                    reinterpret_cast<RtObject::Internal*>(comp2));   // B.3.3 bridge
                                if (local_d8 != 0) {
                                    int* bn = *reinterpret_cast<int**>(*reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0x118) + 0x30);
                                    int64_t local_c8 = 0;
                                    bool gotC8 = false;
                                    int key = *reinterpret_cast<int*>(local_d8 + 0x28);
                                    while (bn != nullptr) {
                                        if (key < *bn) {
                                            bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(bn) + 0x18); // +6
                                        } else if (*bn < key) {
                                            bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(bn) + 0x20); // +8
                                        } else {
                                            local_c8 = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(bn) + 8); // bn+2
                                            gotC8 = true;
                                            break;
                                        }
                                    }
                                    if (!gotC8) {
                                        if (fn_InternalBasePtr_Ctor) new (&local_c8) ::Object::InternalBasePtr(
                                            nullptr);   // B.3.3 bridge
                                    }
                                    if (local_c8 != 0) {
                                        RemovalCalc_CitizenAI_9ad0(
                                            *reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0x118),
                                            *reinterpret_cast<int*>(local_d8 + 0x28));
                                    }
                                }
                            }
                        }
                    }

                    uVar20 = (int64_t)(iVar23 + 1);
                    uVar22 = (int64_t)local_res18;
                    puVar18 = local_a0;
                } while ((int)uVar20 <= (int)local_res18);  // uVar20 == iVar23+1 here
                uVar20 = (int64_t)local_res8;
                uVar13 = (int64_t)local_res20;
            }
            uVar26 = (int64_t)(iVar24 + 1);
        } while ((int)uVar26 <= (int)uVar13);  // uVar26 == iVar24+1 here

        uVar16ret = 0;
        if (bVar5 && s_roadMesh)
            uVar16ret = s_roadMesh(*reinterpret_cast<int64_t*>((uintptr_t)*g_pGameWorldSingleton + 0x110), puVar18);
    }

    // FIX(lead-verify): faithful CONCAT71 — roadMesh return in the upper bytes, low byte 1.
    // ENDGAME B.3.8: slot 13 is `bool` (the callers test AL), so only the low byte is
    // returned; the upper bytes retail leaves in RAX are not part of the slot's contract.
    (void)uVar16ret;
    return true;
}
