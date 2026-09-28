// code/Game/Work/work_act_clear.cpp
// Work clear-area / harvest-area action-chain constructor.
//
// Faithful 1:1 port of ShowTool_Action_Ctor4 @ 0x1400c1530 (Application-x64.exe),
// exposed here as WorkAct_HarvestAreaCtor_c1530. Despite the (mislabeled) Ghidra
#include "Work/exe_bodies.h"
// name, this is a LARGE work-action-queue builder (~0x948 bytes): it builds the
// citizen's action chain on the CitizenAIComponent task list and stores it on the
// entity (citizen+0x278 -> aiComp, list base aiComp+0x48; the SendComponentMessage
// action is stored at aiComp+0x58 after DeletePtr of the previous one).
//
// This is a sibling of WorkAct_PerformWorkCtor_c1eb0 in work_base_actionctor.cpp,
// but differs in real ways (verified against the actual disassembly):
//   * param_3 is a LOOP COUNT: it iterates param_3 times over the param_4[] array
//     (param_4 advances by 8 each iteration), and for each element builds an INLINE
//     RemoveNaturalResource action (vtable 0x140160050) + appends it + sets bit 0x14
//     (0x100000) on that target's +0x20 flags, detaching via 0x1400c3470 on change.
//   * the post-loop chain is: PerformWork_Ctor7 (0x1400c9720), MoveTo (0x1400c9170),
//     then an INLINE ShowTool action (vtable 0x1401589b0). There is NO inline-
//     PerformWork / AnimateNaturalResource block (that belongs to the c1eb0 sibling).
//   * the list "append" is INLINED in the disasm for every action except the first
//     two (CompleteTask + the first ShowTool), which use FUN_1400c54e0. We mirror
//     the inlined append faithfully via WA_AppendInline (same list-growth logic the
//     out-of-line FUN_1400c54e0 / FUN_1400032e0 perform on aiComp+0x48..+0x60).
//
// Instruction-faithful: do NOT "improve" behavior. Every offset / constant below
// cites its disasm address. Self-contained: resolves every sub-callee via
// ResolveAddr(Module::Exe, 0xADDR). No RegisterExeAddr. In standalone,
// addresses resolve to 0 unless the address is registered, in which case the
// guarded call is skipped (project norm).

#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "Action/Action.h"   // ENDGAME B.3.8 CB3
#include "Action/RemoveNaturalResource.h"   // ENDGAME B.3.8 CB3
#include "Action/ShowTool.h"   // ENDGAME B.3.8 CB3
#include "Action/exe_bodies.h"      // B3R: the action ctor bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "object/Internal.h"
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include "Component/coll_bridge.h"
#include "Component/NaturalResourceDescription.h"   // ENDGAME B.3.9 D5
#include <cstdint>

// ============================================================
// Sub-callees, called by name (B3R)
// ============================================================
// Every exe sub-callee is one of our own bodies (Action/, Work/ exe_bodies.h):
//   0x1400c29a0 Entity_impl_29a0 (heading, XMM0)    ActChild_Entity29a0_c29a0
//               (disasm 0x1400c1586: RCX=param_5 worldPos, RDX=&local_res8 entity inner)
//   0x1400c5400 TaskListCleanup                     WorkBase_TaskListCleanup_c5400
//   0x1400c54e0 TaskListAppend                      Pf_ActStackPush_54e0
//   0x1400c3470 Work-target detach                  ActHunt_NotifyEntityChanged_c3470
//   0x1400012d0 Entity_FindComponent_Family         ActMv_FindComponentFamily_12d0
//   0x1400cb1d0 SendComponentMessage copy-ctor      Pf_SendComponentMessageCopyCtor_b1d0
//   0x1400c5da0 CompleteTask copy-ctor              Pf_CompleteTaskCopyCtor_5da0
//   0x140042890 ShowTool ctor (R8D=toolType)        ActChild_ShowToolCtor_42890
//   0x1400c9720 PerformWork_Action_Ctor7            Pf_PerformWorkCtor7_9720
//   0x1400c9170 MoveTo ctor                         Pf_MoveToCtor_9170
// Dispatch slots:
//   0x140152a98 IsDerivedFrom        -> the member Object::Base::IsDerivedFrom
//   0x140152b60 MemAlloc             -> System::MemoryAllocator::MemAlloc
//   0x140152b68 MemFree              -> System::MemoryAllocator::MemFree
//   0x140152a90 InternalBasePtr ctor -> placement-new Object::InternalBasePtr
//   PORT GAP 0x140151ab8 InternalBasePtr::DeletePtr: the import slot is not filled; the
//   fallback the reader took (the member Object::InternalBasePtr::DeletePtr) is the code.
// .rdata: 0x140172280 = -1.0f (MoveTo speed).

static inline void* WAC_Alloc(uint64_t size, uint32_t align) {
    return System::MemoryAllocator::MemAlloc(size, static_cast<int>(align));
}
static inline void WAC_IbpCtor(void* ibp, void* o)  {
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(o));   // B.3.3 bridge
}

// Inlined task-list append, mirroring the disasm's inlined FUN_1400c54e0 body
// (the list lives at aiComp+0x48; its header fields are at lVar3+0x50/+0x54/+0x60,
// where lVar3 = aiComp and plVar15 = &aiComp[0x48] is the backing-array pointer).
//   listArr = (void**)*plVar15 ; count@+0x50 ; cap@+0x54 ; index@+0x60.
// Faithful to disasm @0x1400c17ad..0x1400c18ec (RemoveNaturalResource append) and
// the identical blocks for PerformWork / inline ShowTool.
static void WAC_AppendInline(uintptr_t aiComp, void** plVar15, uintptr_t actionIbpVal) {
    int iVar16 = *reinterpret_cast<int*>(aiComp + 0x50);
    if (iVar16 == 0) {
        // First append: initialize to capacity 0x10, copy any prior contents.
        void* pvVar11 = *reinterpret_cast<void**>(plVar15);
        *reinterpret_cast<uint32_t*>(aiComp + 0x50) = 0x10;
        if (*reinterpret_cast<int*>(aiComp + 0x54) < 0x10) {
            *reinterpret_cast<uint32_t*>(aiComp + 0x54) = 0x10;
            void* nb = WAC_Alloc(static_cast<uint64_t>(*reinterpret_cast<int*>(aiComp + 0x54)) << 3, 8);
            *reinterpret_cast<void**>(plVar15) = nb;
        }
        // The original copies index 0 (iVar16==0 ⇒ start = 0), zeroing the rest.
        // Disasm: start = (RDI==*plVar15) ? old-count(EBX) : 0; here EBX(old count)=0.
        int64_t start = 0;
        if (pvVar11 == *reinterpret_cast<void**>(plVar15)) start = iVar16; // ==0 here
        for (int64_t i = start; i < 0x10; ++i) {
            uint64_t* slot = reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(plVar15) + i * 8);
            if (i < static_cast<int64_t>(iVar16)) {
                if (slot) { *slot = 0; *slot = reinterpret_cast<uint64_t*>(pvVar11)[i]; }
            } else if (slot) {
                *slot = 0;
            }
        }
        if (pvVar11 != *reinterpret_cast<void**>(plVar15)) System::MemoryAllocator::MemFree(pvVar11);
    } else if (*reinterpret_cast<int*>(aiComp + 0x60) + 1 == iVar16) {
        // B.3.2 bridge: FUN_1400032e0 == Collection::Array<Ptr8>::SetSize
        // (ImmigrationComp_DynArrayResize in immigrationcomponent.cpp). plVar15
        // IS the array header (aiComp+0x48: data@0, count@+8==+0x50, cap@+0xc==+0x54).
        GameColl::AsArray<GameColl::Ptr8>(plVar15).SetSize(iVar16 * 2);
    }

    // Drain any pending nodes from the current index slot (cancel/detach chain).
    if (*reinterpret_cast<int*>(aiComp + 0x60) >= 0) {
        for (void* node = reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(plVar15))[*reinterpret_cast<int*>(aiComp + 0x60)];
             node != nullptr;
             node = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x10)) {
            if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(node) + 0x18) != 0) {
                *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(node) + 0x18) = 0;
                // slot 7 (vtable +0x38) Action::Exit(list): the node is an action on the
                // citizen's action stack (+0x10 next, +0x18 _status, as the walk 0x1400c4f10)
                reinterpret_cast<Action*>(node)->Exit(plVar15);   // B.3.8 bridge
            }
        }
    }

    *reinterpret_cast<int*>(aiComp + 0x60) += 1;
    reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(plVar15))[*reinterpret_cast<int*>(aiComp + 0x60)] = actionIbpVal;
}

// ============================================================
// WorkAct_HarvestAreaCtor_c1530 (0x1400c1530)
// ============================================================
//   param_1 (RCX) = citizen InternalBasePtr*
//   param_2 (EDX) = workId (u32, CompleteTask param_2)
//   param_3 (R8D) = count of param_4[] entries to clear (loop count; signed)
//   param_4 (R9)  = InternalBasePtr* array (count entries; advances by 8/iter)
//   param_5 (stack, [RBP+0x60]) = float* worldPos (heading + MoveTo destination)
//   param_6 (stack, [RBP+0x68]) = u32 (CompleteTask param_5)
//   param_7 (stack, [RBP+0x70]) = dword (unused on entry; reloaded as SendComponentMessage msg id)
void WorkAct_HarvestAreaCtor_c1530(void* param_1, uint32_t param_2, int param_3,
                                   void** param_4, float* param_5, uint32_t param_6,
                                   uint32_t /*param_7*/) {

    // --- heading = Entity_impl_29a0(param_5, &{param_4[0] inner}) → XMM6 (0x1400c1563..0x1400c158b) ---
    uintptr_t firstEntityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_4) + 0x10);
    float heading = 0.0f;
    heading = ActChild_Entity29a0_c29a0(param_5, &firstEntityInner);

    // --- derive target component off param_4[0]+0x18; keep if IsDerivedFrom(c7b68) (0x1400c158e..0x1400c15b6) ---
    void* targetComp = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(param_4) + 0x18);
    uintptr_t local_60 = 0; // derived target (the "pIVar8 = local_60" used post-loop)
    bool derived = false;
    derived = reinterpret_cast<const Object::Base*>(targetComp)->IsDerivedFrom(   // B.3.x bridge (slot 0x140152a98)
        NaturalResourceDescription::TypeId());
    if (derived) local_60 = reinterpret_cast<uintptr_t>(targetComp);

    // --- list base = (*(citizen)+0x278) + 0x48 ; cleanup (0x1400c15ba..0x1400c15e2) ---
    uintptr_t lVar3 = *reinterpret_cast<uintptr_t*>(param_1);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(lVar3 + 0x278);
    void** plVar15 = reinterpret_cast<void**>(aiComp + 0x48);
    WorkBase_TaskListCleanup_c5400(plVar15);

    uintptr_t ibpA = 0; // local_68 / local_res20 scratch IBP
    uintptr_t ibpB = 0; // second scratch IBP (local_res20 alt)

    // --- SendComponentMessage action → stored at aiComp+0x58 (DeletePtr prior) (0x1400c15e7..0x1400c1677) ---
    {
        void* obj = WAC_Alloc(0x40, 8); // LEA EDX,[R12+8]; LEA ECX,[RDX+0x38] = 0x40
        void* sm = nullptr;
        if (obj) {
            uintptr_t innerIbp = 0;
            WAC_IbpCtor(&innerIbp, nullptr);
            uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);
            sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
        }
        WAC_IbpCtor(&ibpA, sm);
        if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)   // PORT GAP 0x140151ab8: the fallback member DeletePtr is the code
            reinterpret_cast< ::Object::InternalBasePtr*>(aiComp + 0x58)->DeletePtr();   // B.3.3 bridge
        *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA;
    }

    // --- CompleteTask action; append via FUN_1400c54e0 (0x1400c1677..0x1400c16dc) ---
    {
        void* obj = WAC_Alloc(0x38, 8); // MOV EDX,8; LEA ECX,[RDX+0x30] = 0x38
        void* ct = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);
            // RDX=&entityInner, R8=&local_res8(innerIbp from prior IbpCtor), but disasm
            // reuses local_res8 (just IbpCtor'd to 0 at +0x1693). Mirror with fresh inner.
            uintptr_t innerIbp = 0;
            WAC_IbpCtor(&innerIbp, nullptr);
            ct = Pf_CompleteTaskCopyCtor_5da0(obj, param_2, &entityInner, &innerIbp, param_6);
        }
        WAC_IbpCtor(&ibpB, ct);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(plVar15), &ibpB);
    }

    // --- ShowTool (~Hoe = -1; disasm OR R8D,0xffffffff) action; append via FUN_1400c54e0 (0x1400c16dc..0x1400c1729) ---
    {
        void* obj = WAC_Alloc(0x30, 8); // MOV EDX,8; LEA ECX,[RDX+0x28] = 0x30
        void* st = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);
            st = ActChild_ShowToolCtor_42890(obj, &entityInner, ~0); // OR R8D,0xffffffff @0x1400c16fb
        }
        WAC_IbpCtor(&ibpA, st);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(plVar15), &ibpA);
    }

    // --- LOOP: for i in [0, param_3): inline RemoveNaturalResource action per param_4[i]
    //     (0x1400c1729..0x1400c1930). param_4 advances by 8 each iter; lVar18=param_3 counter. ---
    if (param_3 > 0) {
        int64_t loopCtr = static_cast<int64_t>(param_3); // R12=R14=param_3
        do {
            uintptr_t innerR4 = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_4) + 0x10); // RDI

            void* obj = WAC_Alloc(0x38, 8); // MOV EDX,8; LEA ECX,[RDX+0x30] @0x1400c174a = 8+0x30 = 0x38
            uintptr_t actionObj = 0;        // R14 (the built action, or 0)
            if (obj) {
                uint64_t entityInner = *reinterpret_cast<uintptr_t*>(param_1); // RBX = *(*param_1)
                uint64_t targetInner = innerR4;
                // 0x1400c176d..0x1400c1796: the inlined RemoveNaturalResource ctor
                // (+0x20 citizen inner, +0x28 param_4[i] inner, +0x30 = 1) — the real ctor.
                new (obj) RemoveNaturalResource(&targetInner, &entityInner, 1);
                actionObj = reinterpret_cast<uintptr_t>(obj);
            }
            WAC_IbpCtor(&ibpA, reinterpret_cast<void*>(actionObj)); // &local_res8 → ibpA scratch

            // inline append of ibpA (0x1400c17ad..0x1400c18ec)
            WAC_AppendInline(aiComp, plVar15, ibpA);

            // set bit 0x14 (0x100000) on param_4[i] target's +0x20 flags; on change & +0x10 set,
            // detach via FUN_1400c3470 (0x1400c18f0..0x1400c1929)
            uintptr_t targetObj = *reinterpret_cast<uintptr_t*>(param_4);
            uint32_t oldFlags = *reinterpret_cast<uint32_t*>(targetObj + 0x20);
            uint32_t newFlags = oldFlags | 0x100000u; // BTS EAX,0x14
            *reinterpret_cast<uint32_t*>(targetObj + 0x20) = newFlags;
            if (static_cast<uint16_t>(oldFlags ^ newFlags) != 0 &&            // TEST CX,CX
                *reinterpret_cast<uintptr_t*>(targetObj + 0x10) != 0) {
                void* detach = *reinterpret_cast<void**>(targetObj + 0x10);
                ActHunt_NotifyEntityChanged_c3470(detach);
            }

            param_4 = param_4 + 1; // ADD R15,0x8 (param_4 is void** → +1 entry = +8 bytes)
            --loopCtr;             // DEC R12
        } while (loopCtr != 0);
    }

    // --- actionType: EBX=8 default; if dword[derivedTarget+0x80] <= 0xa: EBX = that+9 (0x1400c1936..0x1400c194b) ---
    int actionType = 8;
    if (derived && *reinterpret_cast<uint32_t*>(local_60 + 0x80) <= 0xa)
        actionType = static_cast<int>(*reinterpret_cast<uint32_t*>(local_60 + 0x80)) + 9;

    // --- PerformWork_Ctor7: heading(XMM2)=heading, actionType(R9)=actionType,
    //     time=*(derivedTarget+0x68), p6=1, p7=*(derivedTarget+0x80) (0x1400c194e..0x1400c1998) ---
    {
        void* obj = WAC_Alloc(0x48, 8); // MOV EDX,8; LEA ECX,[RDX+0x40] = 0x48
        uintptr_t pwIbp = 0;
        if (obj && derived) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);
            int p7  = static_cast<int>(*reinterpret_cast<uint32_t*>(local_60 + 0x80)); // [RSP+0x30]
            int64_t time = *reinterpret_cast<int64_t*>(local_60 + 0x68);               // [RSP+0x20]
            void* pw = nullptr;
            pw = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, actionType, time, 1, p7);
            WAC_IbpCtor(&ibpA, pw);
            pwIbp = ibpA;
        } else {
            WAC_IbpCtor(&ibpA, nullptr);
            pwIbp = ibpA;
        }
        WAC_AppendInline(aiComp, plVar15, pwIbp);
    }

    // --- MoveTo (path type from switch on dword[derivedTarget+0x80]) (0x1400c1aed..0x1400c1ba9) ---
    {
        void* obj = WAC_Alloc(0x480, 8); // MOV ECX,0x480
        uintptr_t mvIbp = 0;
        if (obj && derived) {
            // switch(dword[local_60+0x80]); ECX = that+1 (INC ECX); CMP ECX,8; JA → default Slow(1).
            // jump table @0x1400c1e7c (idx = resKind+1, 0-based):
            //   idx0→0x1b3f Slow(1); idx1,2,5,6,7,8→0x1b31 Fast(2); idx3,4→0x1b38 Fastest(3).
            int pathType;
            uint32_t resKind = *reinterpret_cast<uint32_t*>(local_60 + 0x80);
            uint32_t idx = resKind + 1; // INC ECX @0x1400c1b14
            if (idx > 8) {
                pathType = 1; // Slow (default; CMP ECX,8 / JA @0x1400c1b16)
            } else {
                switch (idx) {
                    case 0:                                  pathType = 1; break; // Slow
                    case 1: case 2: case 5: case 6:
                    case 7: case 8:                          pathType = 2; break; // Fast
                    case 3: case 4:                          pathType = 3; break; // Fastest
                    default:                                 pathType = 1; break;
                }
            }
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);
            uintptr_t famRet = 0;
            void* familyIBP = nullptr;
            familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
            float speed = kRdata_140172280; // -1.0f
            void* mv = nullptr;
            mv = Pf_MoveToCtor_9170(obj, familyIBP, pathType, 0, param_5, 0xe1f, 0, speed);
            WAC_IbpCtor(&ibpA, mv);
            mvIbp = ibpA;
        } else {
            WAC_IbpCtor(&ibpA, nullptr);
            mvIbp = ibpA;
        }
        WAC_AppendInline(aiComp, plVar15, mvIbp);
    }

    // --- inline ShowTool action (vtable 0x1401589b0); toolType = dword[derivedTarget+0x80]
    //     (0x1400c1cef..0x1400c1e58) ---
    {
        void* obj = WAC_Alloc(0x30, 8); // MOV EDX,8; LEA ECX,[RDX+0x28] = 0x30
        uintptr_t stIbp = 0;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);            // RBX
            uint32_t toolType = derived ? *reinterpret_cast<uint32_t*>(local_60 + 0x80) : 0; // EDI = [R15+0x80]
            // 0x1400c1d1e..0x1400c1d3f: the inlined ShowTool ctor — the real ctor.
            new (obj) ShowTool(&entityInner, static_cast<int>(toolType));
            uintptr_t o = reinterpret_cast<uintptr_t>(obj);
            stIbp = o;
            WAC_IbpCtor(&ibpB, reinterpret_cast<void*>(o)); // &local_res20
            stIbp = ibpB;
        } else {
            WAC_IbpCtor(&ibpB, nullptr);
            stIbp = ibpB;
        }
        WAC_AppendInline(aiComp, plVar15, stIbp);
    }
}
