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
#include <windows.h>
#include <new>
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "perf_terrain.h"
#include <cstdint>
#include "Component/CreateDragDescription.h"
#include "Component/MapComponent.h"
#include "Component/DestroyComponent.h"
#include "Component/WorkComponent.h"
#include "Work/BuildRoad.h"
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Player/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Component/exe_bodies.h"
#include "Component/controller_state_table.h"
#include "Entity/componententity_funcs.h"
#include "system/MemoryAllocator.h"
#include "system/Value.h"
#include "object/TypeInfo.h"
#include "Component/ComponentDescription.h"
#include "Component/CreateRemovalDescription.h"
#include "Component/StateMachineComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/EntityList.h"
#include "Game/Map.h"
#include "Game/PlayerToolManager.h"
#include "Action/PathAvl.h"
#include "Work/Work.h"

// (The removal-eligibility checks are RemovalTool members: tool_base_path.cpp.)

// ============================================================
// Exe helpers called by name (their declaring headers):
//   0x1400c3720 ComponentEntity_CtorFromDescription, 0x1400c48a0
//   EntityList_RegisterEntity_Impl (Entity/componententity_funcs.h);
//   0x1400c2ea0 ActHunt_MarkEntityRemoval_c2ea0, 0x140003110
//   ActHunt_DynArraySetCount_3110, 0x1400af370 Pf_RoadMeshRebuild_f370,
//   0x1400f8e30 ActCt_TaskCleanup_8e30, 0x14001a8c0 Pf_AvlRebalanceA_a8c0,
//   0x1400a8c90 Pf_FineCellDirty_a8c90, 0x1400a6fe0 Pf_EnqueueCmd2_a6fe0,
//   0x14009f2f0 Pf_SetTileRoadFlag_9f2f0 (Action/exe_bodies.h);
//   0x14001a6e0 WorkBST_BSTRemove_a6e0 (Work/exe_bodies.h);
//   0x14012ead0 WorkComponent_CheckWorkSlot (Component/exe_bodies.h);
//   0x1400a9660 StateMachine_TickReimpl (Component/controller_state_table.h);
//   0x140003ea0 / 0x140005780 PlayerPath_FindComponent_*, 0x14011e710
//   PlayerPath_Sub_11e710 (Player/exe_bodies.h).
// Import slot 0x140152b60 is System::MemoryAllocator::MemAlloc; 0x140152b00 is
// System::Time::Time(float) (RtSystem_Time_Ctor).
// ============================================================

// Type-id storage: 0x1401c7430 is CreateDragDescription's cell, 0x1401c6860
// BuildRoad's (Table 1). The MapComponent / DestroyComponent / WorkComponent
// lookups are FindComponent<T> (ComponentEntity.h) since B4 PLAYER.
static uint32_t s_tid_7430_v, s_tid_6860_v;
static const uint32_t* s_tid_7430 = &s_tid_7430_v;  // 0x1401c7430 -> CreateDragDescription
static const uint32_t* s_tid_6860 = &s_tid_6860_v;  // 0x1401c6860 -> BuildRoad

static bool s_init = false;

static void InitTypeIds() {
    if (s_init) return;
    s_tid_7430_v = CreateDragDescription::TypeId();
    s_tid_6860_v = BuildRoad::TypeId();
    s_init = true;
}

// ============================================================
// 0x14002d960  RemovalTool_helper_d960
// Find-description-by-type over a ComponentDescription's sub-descriptions (count
// +0x30, array +0x28; byte offsets confirmed in disasm). Type id DAT_1401c7430
// (CreateDragDescription). Writes the match (or 0) into *param_2 and returns param_2.
// ============================================================
extern "C" void* RemovalCalc_helper_d960(const ComponentDescription* param_1, void** param_2) {
    InitTypeIds();
    void* result = nullptr;
    int n = param_1->_subDescCount;
    Description** base = param_1->_subDescs;
    for (int i = 0; i < n; i++) {
        Description* comp = base[i];
        if (s_tid_7430 && comp->Type() == *s_tid_7430) {
            Description* c = base[i];
            if (s_tid_7430 && Object::IsDerivedFromNullSafe(c, *s_tid_7430))
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
    InitTypeIds();
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
            new (param_2) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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
    InitTypeIds();
    int64_t resIBP[4] = {0};
    PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), resIBP);
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
    InitTypeIds();
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
        PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), &buf);
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
        PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), &buf);
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
    PlayerPath_FindComponent_3ea0(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), &local_res10);
    // Time(0.0) into the shared 8-byte slot (see timeOutSlot note above).
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeOutSlot), 0.0f);
    int64_t state = local_res10;
    int64_t* stackPtrField = reinterpret_cast<int64_t*>(state + 0x48);

    // Phase 1 (transition slot +0x30 passes the shared slot, same as phase 2)
    SM_RunPhase(state, stackPtrField, outState);

    // ---- between-phase: choose transition + kick Tick ----
    state = local_res10;
    *outState = 3;  // outState seed
    if (*reinterpret_cast<char*>(state + 0x51) < 0) {
        int64_t timeBuf[1] = {0};
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), 0.0f);
        // The door passed the Time buffer's ADDRESS where the body takes the time
        // VALUE (int64_t); the bits the door passed are kept.
        StateMachine_TickReimpl(reinterpret_cast<void*>(state + 0x48),
                                reinterpret_cast<int64_t>(timeBuf), 0, 0);
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
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeOutSlot), 0.0f);
    state = local_res10;
    stackPtrField = reinterpret_cast<int64_t*>(state + 0x48);

    // Phase 2 (transition slot +0x30 passes the shared slot)
    SM_RunPhase(state, stackPtrField, outState);
}

// ============================================================
// 0x14009c700  RemovalTool_helper_c700
// Road-removal cell handler. param_1 = the Map. Walks the road BST (+0xE8) by cell
// id param_2; on a found road node, resolves the BuildRoad work (RemovalTool_helper
// _8d50 + IsA BuildRoad), reads its remove-kind fields (+0x60,+0x64,+0x68), notifies
// the work scheduler (0x1400f8e30) and erases the BST node (0x14001a6e0). Then
// updates the map cell (+0x80, stride 0x30): if cell+0x2a==0xff it dirties the
// region (0x1400a8c90) and queues a worker job (0x1400a6fe0); else applies the
// removal (0x14009f2f0) and rebuilds the road mesh (0x1400af370). Returns bool.
// ============================================================
extern "C" uint64_t RemovalCalc_helper_c700(Map* param_1, int param_2) {
    InitTypeIds();
    const AvlNode28* root = param_1->field_0xE8;
    uint32_t retLow = 2;
    int removeKind = 0xff;   // iVar9 / R12D
    uint8_t ebx = 0;         // the +0x64 byte (road branch) / the IBP's low byte
    uint32_t res10 = 2;      // local_res10
    uint8_t r7 = 0;          // uVar7 byte
    bool found = false;      // bVar5
    uint8_t r13 = 1;         // bVar10 (subtract amount); EBP/R13 start = 1
    void* res18 = nullptr;   // local_res18 (InternalBasePtr)

    const AvlNode28* node = root;
    while (node != nullptr) {
        if (param_2 < node->key) {
            node = node->left;
        } else if (node->key < param_2) {
            node = node->right;
        } else {
            // exact match — node found (key == param_2)
            // RemovalTool_helper_8d50(the work scheduler, &res18, param_2)
            // LEFT: 0x1400f8d50 (below) still takes the scheduler as an integer.
            RemovalCalc_helper_8d50(reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_tutorial),
                                    &res18, param_2);
            r13 = 1;  // bVar10 = true by default
            BuildRoad* road = nullptr;
            if (res18 != nullptr) {
                // LEFT: 0x1400f8d50 hands the work back untyped (the scheduler's
                // node payload); IsA / IsDerivedFrom prove it a BuildRoad.
                Object::Base* work = reinterpret_cast<Object::Base*>(res18);
                bool isRoad = s_tid_6860 ? work->IsA(*s_tid_6860) : false;
                if (isRoad) {
                    road = (s_tid_6860 && Object::IsDerivedFromNullSafe(work, *s_tid_6860))
                           ? static_cast<BuildRoad*>(work) : nullptr;
                    removeKind = road->field_60;
                    ebx = (uint8_t)road->field_64;   // ptr = *(uint*)(bp+100)
                    res10 = road->field_68;
                    ActCt_TaskCleanup_8e30(GameUpdateLoop::_singleton->m_tutorial,
                                           static_cast<Work*>(work)->field_20);
                    r13 = (removeKind == 0xff) ? 1 : 0;  // bVar10 = (removeKind==0xff)
                } else {
                    ebx = 0;   // the low byte of the still-null IBP
                    r13 = 1;
                }
            } else {
                // LAB_14009c85e: res18 == null -> EBX = the null IBP's low byte
                ebx = 0;
            }
            r7 = ebx;  // uVar7 = the +0x64 byte for the road branch

            // second BST pass: erase the matched node (0x14001a6e0)
            AvlNode28* n2 = param_1->field_0xE8;
            while (n2 != nullptr) {
                if (param_2 < n2->key) {
                    n2 = n2->left;
                } else if (n2->key < param_2) {
                    n2 = n2->right;
                } else {
                    // LEFT: 0x14001a6e0 is a C door in block WORK's Work/exe_bodies.h.
                    WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&param_1->field_0xE8), n2);
                    break;
                }
            }
            retLow = res10;
            found = true;
            break;
        }
    }

    // ---- LAB_14009c8a0: apply to map cell ----
    MapCell* cell = &param_1->cells[param_2];
    uint64_t result;
    if (static_cast<int8_t>(cell->field_0x2A) == (int8_t)0xff) {
        uint8_t bVar1 = cell->terrainType;
        uint8_t setBit = ((1u << (bVar1 & 0x1f)) & 0x1e0u) != 0 ? 1 : 0;
        param_1->field_0xA2 = param_1->field_0xA2 | (setBit != 0);
        cell->terrainType = 2;
        PerfX::TerrainNoteWrite(reinterpret_cast<const uint8_t*>(cell));   // [PerfX] mirror (LEFT: block STANDALONE's perf_terrain.h takes the cell as bytes)
        if (((1u << (bVar1 & 0x1f)) & 0x1e0u) != 0) {
            int div = param_1->size;
            // LEFT: 0x1400a8c90 / 0x1400a6fe0 are C doors in block ACT-b's
            // Action/exe_bodies.h taking the region graph / pathfinder as integers.
            Pf_FineCellDirty_a8c90(reinterpret_cast<int64_t>(param_1->regionGraph),
                                   param_2 % div, param_2 / div);
        }
        // 0x1400a6fe0 reads the cell and the type (EDX, R8D) only; retail's R9 is not read.
        Pf_EnqueueCmd2_a6fe0(reinterpret_cast<int64_t>(param_1->pathfinder),
                             static_cast<int>(param_2), 2);
        result = 0;  // uVar8 & 0xff..00
    } else {
        if (found) {
            cell->footprintCount = static_cast<uint8_t>(cell->footprintCount - r13);
            cell->field_0x2A = static_cast<uint8_t>(removeKind);
            cell->field_0x2B = r7;
            param_1->SetTerrainType(param_2, retLow);   // 0x14009f2f0
            if (removeKind == 0xff) {
                result = 0;
            } else {
                Pf_RoadMeshRebuild_f370(reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_worldGeometry),
                                        reinterpret_cast<uint32_t*>(param_1));   // LEFT: as above
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
    InitTypeIds();
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
                    WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(plVar8), pk);
                    break;
                }
            }
            *reinterpret_cast<int*>(*self + 0x24) = iVar6;
            uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*self + 0x20);

            int* cur = reinterpret_cast<int*>(*plVar8);
            if (cur == nullptr) {
                int64_t* nodeMem = reinterpret_cast<int64_t*>(System::MemoryAllocator::MemAlloc(0x28, 8));
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
                                int64_t* nodeMem = reinterpret_cast<int64_t*>(System::MemoryAllocator::MemAlloc(0x28, 8));
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
                        int64_t* nodeMem = reinterpret_cast<int64_t*>(System::MemoryAllocator::MemAlloc(0x28, 8));
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
                    Pf_AvlRebalanceA_a8c0(plVar8, &res18local);
                }
            }
            *reinterpret_cast<int*>(param_1 + 0x18) = *reinterpret_cast<int*>(param_1 + 0x18) + 1;
            return;
        }
        if (piVar4 == nullptr) return;
    }
}

// ============================================================
// 0x1401035e0  RemovalTool::CalcPath (slot 13)
// Walks the selection rectangle (param_2 corner, param_3 corner) over the tile
// grid and, per cell, applies the active CreateRemovalDescription. See file header.
// Faithful to the decompile; B4 PLAYER reads every field by name.
// ============================================================

// A placed removal marker: a fresh entity of the description's +0x38 template whose
// MapComponent covers the one cell, registered in the world entity list
// (0x1400c3720 + FindComponent<MapComponent> 0x140004680 + 0x1400c48a0). Retail
// inlines it twice in CalcPath.
static void PlaceRemovalMarker(const CreateRemovalDescription* desc, int col, int row) {
    ComponentEntity* mem = static_cast<ComponentEntity*>(System::MemoryAllocator::MemAlloc(0x48, 8));
    ComponentEntity* cloned = nullptr;
    if (mem) cloned = ComponentEntity_CtorFromDescription(mem, desc->_ref38);
    MapComponent* comp = cloned->FindComponent<MapComponent>();
    comp->tileX0 = (int16_t)col;
    comp->tileY0 = (int16_t)row;
    comp->tileX1 = 1;   // +0x2c / +0x2e: one qword store of 0x10001 in retail (H7: two stores)
    comp->tileY1 = 1;
    ComponentEntity* local_70 = cloned;
    EntityList_RegisterEntity_Impl(GameUpdateLoop::_singleton->m_entityList, &local_70,
                                   static_cast<int>(desc->_ref38->_category));
}

bool RemovalTool::CalcPath(int param_2, int param_3) {
    // Decompile variable names preserved verbatim for offset-by-offset fidelity.
    Map* piVarDim = GameUpdateLoop::_singleton->m_game;
    int dim = piVarDim->size;

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

    // the owner's single-rectangle constraint (+0x80 flag)
    const PlayerToolManager* lVar21 = dialogPtr;
    if (lVar21->field_0x80) {
        int iVar24 = dim;
        int64_t uVar16b = (int64_t)lVar21->field_0x9C / iVar24;
        if (((int)uVar4 != lVar21->field_0x98 % iVar24) ||
            ((int)uVar26 != lVar21->field_0x98 / iVar24) ||
            (lVar21->field_0x9C % iVar24 != (int)uVar3) ||
            ((int)uVar16b != (int)uVar13)) {
            return false;   // AL = 0 (retail: uVar16b & ~0xff in RAX; callers test AL)
        }
    }

    // RemovalTool_helper_d960 on the description's +0x38 template, if any
    if (removalDesc->_ref38 != nullptr) {
        void* tmp58 = nullptr;
        RemovalCalc_helper_d960(removalDesc->_ref38, &tmp58);
        uVar22 = (int64_t)local_res18;
    }

    Map* puVar18 = GameUpdateLoop::_singleton->m_game;
    Map* local_a0 = puVar18;
    bool bVar5 = false;          // roadMeshDirty

    if ((int)uVar26 <= (int)uVar13) {
        do {
            int iVar24 = (int)uVar26;  // current row
            if ((int)uVar20 <= (int)uVar22) {
                do {
                    int iVar23 = (int)uVar20;  // current col
                    int iRow = iVar24;
                    int iCol = iVar23;
                    int cellId = (int)(puVar18->size * iVar24 + iVar23);
                    const MapCell* cellSlot = &puVar18->cells[cellId];

                    // resolve the entity occupying this cell (blocker, else occupant)
                    ComponentEntity* local_e8;
                    if (cellSlot->blocker == nullptr) {
                        if (cellSlot->occupant == nullptr) {
                            local_e8 = nullptr;   // the null InternalBasePtr
                        } else {
                            local_e8 = cellSlot->occupant;
                        }
                    } else {
                        local_e8 = cellSlot->blocker;
                    }
                    ComponentEntity* local_e0 = local_e8;

                    // ---- field removal (0x1401048a0) ----
                    if ((char)CanRemoveField(&local_e0, iCol, iRow) != 0) {
                        PlaceRemovalMarker(removalDesc, iVar23, iVar24);
                        puVar18 = local_a0;
                    }

                    // ---- road removal (flag desc+0x40) ----
                    Map* piVar15 = GameUpdateLoop::_singleton->m_game;
                    if (removalDesc->_b40) {
                        int cid = piVar15->size * iRow + iCol;
                        const MapCell* cl = &piVar15->cells[cid];
                        uint32_t mask = 1u << (cl->terrainType & 0x1f);
                        bool hit = false;
                        if (((mask & 2) == 0) && ((mask & 1) == 0) && (static_cast<int8_t>(cl->field_0x2A) == (int8_t)-1)) {
                            const AvlNode28* bn = piVar15->field_0xE8;
                            while (bn != nullptr) {
                                if (cid < bn->key) {
                                    bn = bn->left;
                                } else if (bn->key < cid) {
                                    bn = bn->right;
                                } else { hit = true; break; }
                            }
                        } else {
                            hit = true;
                        }
                        if (hit) {
                            uint64_t rc = RemovalCalc_helper_c700(piVar15, piVar15->size * iRow + iCol);
                            if ((char)rc == 0) {
                                bVar5 = true;
                            } else {
                                PlaceRemovalMarker(removalDesc, iVar23, iVar24);
                            }
                        }
                    }

                    // ---- mark-for-removal of the resident entity (0x140104770) ----
                    local_e0 = local_e8;
                    if ((char)CanRemoveRoad(&local_e0) != 0) {
                        if (local_e8->_description == removalDesc->_ref38) {
                            local_e0 = local_e8;
                            EntityList* lst = GameUpdateLoop::_singleton->m_entityList;
                            if ((local_e0->_entityFlags >> 10 & 1) == 0) {
                                MarkEntityForRemoval(local_e0);
                                ActHunt_DynArraySetCount_3110(&lst->removalList,
                                                              lst->removalList.count + 1, &local_e0);
                            }
                        } else {
                            DestroyComponent* comp = local_e8->FindComponent<DestroyComponent>();
                            if (comp != nullptr) {
                                DestroyComponent* pi = local_e8->FindComponent<DestroyComponent>();
                                if (pi->InRemovalState()) {   // 0x14011e710 (the door read AL)
                                    DestroyComponent* lr = local_e8->FindComponent<DestroyComponent>();
                                    StateMachineComponent* machine =
                                        lr->_entity->FindComponent<StateMachineComponent>();
                                    // the current node's slot 2 (+0x10): (node, &machine, active, 0)
                                    // LEFT: the state-node slots are dispatched by offset here
                                    // (ControllerComponent::Controller's slot names are block
                                    // COMP's; this site keeps the tree's call form).
                                    int64_t* head = reinterpret_cast<int64_t*>(machine->stateMachine.current);
                                    (*reinterpret_cast<void(**)(int64_t*, void*, int, int)>(*head + 0x10))(
                                        head, &machine->stateMachine,
                                        (int)machine->stateMachine.activeState, 0);
                                }
                            }
                        }
                    }

                    // ---- StateMachine kick (0x140104670) ----
                    ComponentEntity* local_b8 = local_e8;
                    if ((char)CanRemoveBuilding(&local_b8) != 0) {
                        DestroyComponent* comp = local_e8->FindComponent<DestroyComponent>();
                        // LEFT: 0x14011dfa0 (below) still takes the component as an integer;
                        // three outside callers bind that C signature.
                        RemovalCalc_StateMachine_dfa0(reinterpret_cast<int64_t>(comp));
                    }

                    // ---- citizen-AI notify (flag desc+0x43) ----
                    local_e0 = local_e8;
                    if (removalDesc->_b43 && (local_e8 != nullptr)) {
                        WorkComponent* comp = local_e0->FindComponent<WorkComponent>();
                        if (comp != nullptr) {
                            if (comp->IsWorkPosted()) {   // 0x14012ead0
                                WorkComponent* comp2 = local_e8->FindComponent<WorkComponent>();
                                if (comp2 != nullptr) {
                                    // LEFT: the scheduler's +0x30 work BST (WorkScheduler,
                                    // block GAME-a) and 0x1400f9ad0's receiver are read as
                                    // the tree's raw node walk.
                                    WorkScheduler* sched = GameUpdateLoop::_singleton->m_tutorial;
                                    int* bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(sched) + 0x30);
                                    int64_t local_c8 = 0;
                                    int key = comp2->workId;
                                    while (bn != nullptr) {
                                        if (key < *bn) {
                                            bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(bn) + 0x18); // +6
                                        } else if (*bn < key) {
                                            bn = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(bn) + 0x20); // +8
                                        } else {
                                            local_c8 = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(bn) + 8); // bn+2
                                            break;
                                        }
                                    }
                                    if (local_c8 != 0) {
                                        RemovalCalc_CitizenAI_9ad0(reinterpret_cast<int64_t>(sched), comp2->workId);
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

        // 0x1400af370 is void; the door read a uint64_t nobody uses.
        // LEFT: its C door (block ACT-b) takes the decal system as an integer.
        if (bVar5)
            Pf_RoadMeshRebuild_f370(reinterpret_cast<int64_t>(GameUpdateLoop::_singleton->m_worldGeometry),
                                    reinterpret_cast<uint32_t*>(puVar18));
    }

    // FIX(lead-verify): faithful CONCAT71 — roadMesh return in the upper bytes, low byte 1.
    // ENDGAME B.3.8: slot 13 is `bool` (the callers test AL), so only the low byte is
    // returned.
    return true;
}
