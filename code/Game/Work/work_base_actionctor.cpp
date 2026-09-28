// code/Game/Work/work_base_actionctor.cpp
// Work action/animation constructors (Wave 2 — Work action-ctor sub-batch).
//
// These 4 exe helpers build the action sequence for a citizen work task: each
// allocates a chain of Action objects (SendComponentMessage / CompleteTask /
// ShowTool / PerformWork / MoveTo / *NaturalResource / inline PerformWork or
#include "Work/exe_bodies.h"
// PlayAnimation) and appends them to the CitizenAIComponent's task action list
// (entity+0x278, list base +0x48). They are resolved via ResolveAddr(Module::Exe,
// 0xADDR) from the Work .cpp call sites (cleararea / foresterwork / herbalistwork /
// pasturework / tendarea / plantorchard / fishingwork). They resolve to 0
// (silently skipped) unless registered. This file provides faithful 1:1 ports + a single registration
// point (RegisterWorkActionCtorBatch in standalone/reg_work_actionctor.cpp).
//
// Each function is an instruction-faithful port of the decompiled exe helper; do
// NOT "improve" behavior. Every exe sub-callee these ctors invoke is one of our own
// bodies, called by name (B3R; the table is below).
//

#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "Action/PerformWork.h"   // ENDGAME B.3.8 CB3
#include "Action/PickupRawMaterial.h"   // ENDGAME B.3.8 CB3
#include "Action/PlayAnimation.h"   // ENDGAME B.3.8 CB3
#include "math/Math.h"
#include "Action/exe_bodies.h"      // B3R: the action ctor bodies called by name
#include "Component/exe_bodies.h"   // B3R: CitizenComp_StorageComponent_QueryCount
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/Value.h"           // B3R: RtSystem_Time_Ctor
#include <cstring>
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/NaturalResourceDescription.h"   // ENDGAME B.3.9 D5

// ============================================================
// Sub-callees, called by name (B3R)
// ============================================================
// Engine base helpers (InternalBasePtr ctor/deleteptr, IsDerivedFrom) are the real
// Object classes. The exe sub-callees are our own bodies, declared in the module
// exe_bodies.h headers:
//   0x1400c29a0 Entity_impl_29a0 (heading, XMM0)   ActChild_Entity29a0_c29a0
//   0x1400c5400 TaskListCleanup                    WorkBase_TaskListCleanup_c5400
//   0x1400c54e0 TaskListAppend                     Pf_ActStackPush_54e0
//   0x1400c3470 Work-target detach (c1eb0 tail)    ActHunt_NotifyEntityChanged_c3470
//   0x1400012d0 Entity_FindComponent_Family        ActMv_FindComponentFamily_12d0
//   0x1400cb1d0 SendComponentMessage copy-ctor     Pf_SendComponentMessageCopyCtor_b1d0
//   0x1400c5da0 CompleteTask copy-ctor             Pf_CompleteTaskCopyCtor_5da0
//   0x140042890 ShowTool ctor                      ActChild_ShowToolCtor_42890
//   0x1400cadf0 RemoveNaturalResource copy-ctor    ActChild_RemoveNRCopyCtor_cadf0
//   0x1400c57d0 AnimateNaturalResource copy-ctor   ActChild_AnimateNRCopyCtor_c57d0
//   0x1400c6a20 CreateNaturalResource copy-ctor    ActChild_CreateNRCopyCtor_c6a20
//   0x1400c9720 PerformWork_Action_Ctor7           Pf_PerformWorkCtor7_9720
//               (RCX=self, RDX=entity, XMM2=heading, R9D=actionType, [stack]=time,
//                byte p6, int p7 -- verified via disassembly)
//   0x1400c9170 MoveTo ctor                        Pf_MoveToCtor_9170
//   0x1400c9b70 PickupRawMaterial copy-ctor        WorkAct_PickupRawMaterialCopyCtor_c9b70 (below)
//   0x140006270 Entity_FindComponent_Storage       WorkBase_FindComponent_6270
//   0x14012cf10 StorageComponent::QueryCount       CitizenComp_StorageComponent_QueryCount
//   slot 0x140152b60 MemoryAllocator::MemAlloc     System::MemoryAllocator::MemAlloc
//   slot 0x140152b00 System::Time::Time(float)     RtSystem_Time_Ctor
// Math::Vector3 Normalize / Heading are direct Math::Vector3 member calls (B.3.1 step 2).
//
// .rdata floats (code/Game/Game/exe_constants.h):
//   0x140172124 = 0.75f  (c0ad0 PerformWork time)
//   0x140172280 = -1.0f  (MoveTo speed sentinel)
//   0x140172100 = 0.25f  (PlayAnimation time)
//   0x1401721b0 = 10.0f  (grazing PerformWork time)
//   0x140172178 = 2.1f   (plant PerformWork time)
//   0x1401722a8 = -1000  (plant PerformWork heading const; stored as a bit pattern)
// 0x1401c7b68 = NaturalResourceDescription (ENDGAME B.3.9 D5: read directly below).

static inline float WA_RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

static inline void WA_IbpCtor(void* ibp, void* obj) { new (ibp) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge
static inline void WA_Append(void* list, void* ibp) {
    Pf_ActStackPush_54e0(static_cast<int64_t*>(list), ibp);
}

static inline float WA_C_172124() { return kRdata_140172124; }
static inline float WA_MoveSpeed() { return kRdata_140172280; }

// c0ad0 repeats this exact idiom three times (@0x1400c0d06, @0x1400c0f04, @0x1400c0fdc):
//   CheckPtr(citizenIBP); e = *citizenIBP; CheckPtr(e+0x10);
//   Entity_FindComponent_Storage(*(e+0x10), &ibp); CheckPtr(&ibp);
//   StorageComponent_QueryCount(ibp)
// i.e. "how much is the citizen carrying right now". Re-evaluated each time in the
// original (not cached), so this mirrors that.
static int WA_CitizenCarryCount(void* citizenIBP) {
    uintptr_t e = *reinterpret_cast<uintptr_t*>(citizenIBP);
    void* storageIBP = nullptr;
    WorkBase_FindComponent_6270(static_cast<uintptr_t>(*reinterpret_cast<int64_t*>(e + 0x10)), &storageIBP);
    return CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>(storageIBP));
}

// ============================================================
// PerformWork_Action_Ctor3 (0x1400c1eb0)
// ============================================================
// Build the work-action chain for a resource-clearing / performwork task and store it
// on the citizen's CitizenAIComponent task list (citizen+0x278, list base +0x48).
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId (u32)
//   param_3 = target InternalBasePtr* (NaturalResource/work entity)
//   param_4 = float* worldPos
//   param_5 = int (consumed/recomputed internally)
//   param_6 = byte (RemoveNaturalResource flag)
void WorkAct_PerformWorkCtor_c1eb0(void* citizenIBP, uint32_t workId, void* targetIBP,
                                   float* worldPos, int /*zero*/, uint8_t zeroByte) {

    // --- derive the NaturalResource component off the target (param_3) ---
    void* targetComp = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x18);
    void* derivedTarget = nullptr;
    if (Object::IsDerivedFromNullSafe(targetComp, NaturalResourceDescription::TypeId()))
        derivedTarget = targetComp;

    int64_t targetInner = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x10);

    // heading = Entity_impl_29a0(worldPos, &{target inner}) (XMM0 float)
    int64_t headingArg = targetInner;
    float heading = 0.0f;
    heading = ActChild_Entity29a0_c29a0(worldPos, &headingArg);

    // list base = (*(citizen)+0x278) + 0x48
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    WorkBase_TaskListCleanup_c5400(list);

    void* obj;
    uintptr_t ibpA = 0, ibpB = 0;

    // --- SendComponentMessage action → stored at aiComp+0x58 (replaces prior, DeletePtr) ---
    obj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibpA, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(aiComp + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA;

    // --- CompleteTask action ---
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, workId, &entityInner, &targetInnerSlot, 0);
    }
    WA_IbpCtor(&ibpB, ct);
    WA_Append(list, &ibpB);

    // --- ShowTool (~Hoe = -2) action ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        st = ActChild_ShowToolCtor_42890(obj, &entityInner, ~0 /* ~Hoe = -1 (Hoe==0); disasm OR R8D,0xFFFFFFFF */);
    }
    WA_IbpCtor(&ibpA, st);
    WA_Append(list, &ibpA);

    // --- RemoveNaturalResource action ---
    // ABI (Ghidra-verified vs RemoveNaturalResource_Action_CopyCtor @0x1400cadf0 and its
    // caller c1eb0): the ctor stores *param_2 → self+0x28 (resourceEntity/TREE) and
    // *param_3 → self+0x20 (citizenEntity). c1eb0 calls it as
    //   RemoveNaturalResource_Action_CopyCtor(self, &tree(lVar2), &citizen(*param_1), flag)
    // i.e. param_2 = &TREE, param_3 = &CITIZEN. A prior port swapped these (passed citizen
    // as param_2 → +0x28), which made RemoveNaturalResource::Main FindNRComp(citizen) return
    // null (citizen has no NR component) and the organic produce got skipped. Pass tree first.
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* rnr = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
        rnr = ActChild_RemoveNRCopyCtor_cadf0(obj, &targetInnerSlot, &entityInner, zeroByte);
    }
    WA_IbpCtor(&ibpB, rnr);
    WA_Append(list, &ibpB);

    // --- compute performWork param_5 from derivedTarget+0x80 ---
    int pwParam = 8;
    if (derivedTarget && *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80) < 0xb)
        pwParam = static_cast<int>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80)) + 9;

    // --- if derivedTarget+0x64 (byte) set: inline PerformWork + two AnimateNaturalResource ---
    if (derivedTarget && *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x64) != 0) {
        void* inlineObj = System::MemoryAllocator::MemAlloc(0x48, 8);
        void* pw = nullptr;
        if (inlineObj) {
            uint32_t resKind = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80);
            uint64_t res70   = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x70);
            // the inlined PerformWork ctor 0x1400c9720 (useTool = 0, +0x44 = resKind): the real ctor.
            new (inlineObj) PerformWork(citizenIBP, heading, static_cast<uint32_t>(pwParam),
                                        static_cast<int64_t>(res70), 0, static_cast<int>(resKind));
            pw = inlineObj;
        }
        WA_IbpCtor(&ibpA, pw);
        WA_Append(list, &ibpA);

        // AnimateNaturalResource(targetInner, 1, 0)
        obj = System::MemoryAllocator::MemAlloc(0x30, 8);
        void* anr1 = nullptr;
        if (obj) {
            uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
            anr1 = ActChild_AnimateNRCopyCtor_c57d0(obj, &targetInnerSlot, 1, 0);
        }
        WA_IbpCtor(&ibpA, anr1);
        WA_Append(list, &ibpA);

        // AnimateNaturalResource(targetInner, 0, 1)
        obj = System::MemoryAllocator::MemAlloc(0x30, 8);
        void* anr2 = nullptr;
        if (obj) {
            uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
            anr2 = ActChild_AnimateNRCopyCtor_c57d0(obj, &targetInnerSlot, 0, 1);
        }
        WA_IbpCtor(&ibpA, anr2);
        WA_Append(list, &ibpA);
    }

    // --- PerformWork_Ctor7: heading(XMM2)=heading, actionType(R9)=pwParam,
    //     time=derivedTarget+0x68, p6=1, p7=derivedTarget+0x80 (resKind) ---
    obj = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj && derivedTarget) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        int resKind  = static_cast<int>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80));
        int64_t time = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x68);
        pw7 = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, pwParam, time, 1, resKind);
    }
    WA_IbpCtor(&ibpA, pw7);
    WA_Append(list, &ibpA);

    // --- MoveTo (path type from derivedTarget+0x80 switch) ---
    obj = System::MemoryAllocator::MemAlloc(0x480, 8);
    void* mv = nullptr;
    if (obj && derivedTarget) {
        int pathType = 1; // Slow default (disasm: EBX=1 at 0x1400c2123)
        switch (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80)) {
            case 0: case 1: case 4: case 5: case 6: case 7: pathType = 2; break; // Fast
            case 2: case 3: pathType = 3; break;                                  // Fastest
            case 0xffffffff: pathType = 1; break;                                 // Slow
            default: pwParam = 1; break;                                          // resKind>=8: pathType stays 1
        }
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
        float speed = kRdata_140172280;
        mv = Pf_MoveToCtor_9170(obj, familyIBP, pathType, 0, worldPos, 0xe1f, 0, speed);
    }
    WA_IbpCtor(&ibpA, mv);
    WA_Append(list, &ibpA);

    // --- ShowTool (derivedTarget+0x80 tool type) action ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st2 = nullptr;
    if (obj && derivedTarget) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        int toolType = static_cast<int>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80));
        st2 = ActChild_ShowToolCtor_42890(obj, &entityInner, toolType);
    }
    WA_IbpCtor(&ibpA, st2);
    WA_Append(list, &ibpA);

    // --- tail: set bit 0x14 (0x100000) on target's +0x20 flags; on newly-set bit
    //     (low 16 changed) detach via FUN_1400c3470 if target+0x10 non-null ---
    uintptr_t targetObj = *reinterpret_cast<uintptr_t*>(targetIBP);
    uint32_t oldFlags = *reinterpret_cast<uint32_t*>(targetObj + 0x20);
    uint32_t newFlags = oldFlags | 0x100000u;
    *reinterpret_cast<uint32_t*>(targetObj + 0x20) = newFlags;
    if (static_cast<uint16_t>(oldFlags ^ newFlags) != 0 &&
        *reinterpret_cast<uintptr_t*>(targetObj + 0x10) != 0) {
        void* detach = *reinterpret_cast<void**>(targetObj + 0x10);
        ActHunt_NotifyEntityChanged_c3470(detach);
    }
}

// ============================================================
// PickupRawMaterial_Action_CopyCtor (0x1400c9b70)
// ============================================================
// Trivial field-copy ctor for the PickupRawMaterial action.
// Disassembly-verified: RCX=self, RDX=p2, R8=p3.
//   Object::Internal::Internal(self)
//   self+0x00 = vtable_Action (0x14015faa0)   [immediately overwritten]
//   self+0x10 = 0 ; self+0x18 = 0
//   self+0x00 = vtable_PickupRawMaterial (0x14015ff10)
//   self+0x20 = 0 then *p2   (citizen inner)
//   self+0x28 = 0 then *p3   (target/pile inner)
// Returns self (RAX).
void* WorkAct_PickupRawMaterialCopyCtor_c9b70(void* self, void* p2, void* p3) {
    if (!self) return nullptr;

    // The real ctor (Action(): Internal, +0x10/+0x18 = 0; +0x20/+0x28 zeroed), then
    // the two field copies.
    PickupRawMaterial* a = new (self) PickupRawMaterial();
    a->_field20 = p2 ? *reinterpret_cast<void**>(p2) : nullptr;
    a->_field28 = p3 ? *reinterpret_cast<void**>(p3) : nullptr;
    return static_cast<Object::Base*>(a);
}

// ============================================================
// PerformWork_Action_Ctor9 (0x1400c0ad0) — THE pile-haul action builder
// ============================================================
// Called from Pickup::Execute (0x1400f2f00) once per accepted candidate. Builds the
// citizen's LIFO action stack, so the pushed order below EXECUTES IN REVERSE:
//   pushed:   ShowTool(~Hoe) → [SendComponentMessage → aiComp+0x58, not pushed]
//             → PerformWork(kind 7) → [ShowTool(~Axe) if carrying nothing]
//             → PickupRawMaterial → CompleteTask → PerformWork(kind 6) → MoveTo
//             → [ShowTool(~Axe) if carrying something]
//   executes: [ShowTool] → MoveTo(walk to pile) → PerformWork(6) → CompleteTask
//             → PickupRawMaterial(pick it up) → [ShowTool] → PerformWork(7) → ShowTool(~Hoe)
//
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId / resource type id (u32) → CompleteTask
//   param_3 = target (pile) InternalBasePtr*
//   param_4 = float* worldPos (MoveTo destination + heading source)
//   param_5 = int cell index (pile cell; indexes the map grid for the path-mask byte)
//   param_6 = byte — when non-zero, cancel any pending actions first (first candidate only)
//
// Ghidra-verified constants: ~Hoe = -1 (OR R8D,0xffffffff @0x1400c0b68),
// ~Axe = -2 (MOV R8D,0xfffffffe @0x1400c0d5b/@0x1400c1024),
// pathType = QueryCount>0 ? 4 : 1 (MOV EAX,4 / MOV EBX,1 / CMOVNZ @0x1400c0f61).
void WorkAct_PerformWorkCtor9_c0ad0(void* citizenIBP, uint32_t workId, void* targetIBP,
                                    float* worldPos, int cellIdx, uint8_t cancelFirst) {

    // list base = (*(citizen)+0x278) + 0x48
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (cancelFirst != 0) WorkBase_TaskListCleanup_c5400(list);

    // heading = Entity_impl_29a0(worldPos, &target inner) — returned in XMM0.
    uintptr_t targetInner = targetIBP ? *reinterpret_cast<uintptr_t*>(targetIBP) : 0;
    float heading = 0.0f;
    {
        uintptr_t headingArg = targetInner;
        heading = ActChild_Entity29a0_c29a0(worldPos, &headingArg);
    }

    void* obj;
    uintptr_t ibpA = 0, ibpB = 0;

    // --- ShowTool (~Hoe = -1) ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        st = ActChild_ShowToolCtor_42890(obj, &entityInner, -1 /* ~Hoe */);
    }
    WA_IbpCtor(&ibpA, st);
    WA_Append(list, &ibpA);

    // --- SendComponentMessage → aiComp+0x58 (replaces prior, DeletePtr; NOT pushed) ---
    obj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    uintptr_t ibpSm = 0;
    WA_IbpCtor(&ibpSm, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(aiComp + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpSm;

    // --- path-mask byte from the pile's map cell: cells(+0x80) + cellIdx*0x30, byte +0x29 ---
    uint8_t cellFlag = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gw   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
        uintptr_t grid = *reinterpret_cast<uintptr_t*>(gw + 0xf8);
        uintptr_t cells = *reinterpret_cast<uintptr_t*>(grid + 0x80);
        cellFlag = *reinterpret_cast<uint8_t*>(cells + 0x29 +
                       static_cast<uintptr_t>(static_cast<int64_t>(cellIdx)) * 0x30);
    }

    // --- inline PerformWork action, kind 7 ---
    obj = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), WA_C_172124());
        // the inlined PerformWork ctor 0x1400c9720 (useTool = 0, +0x44 = -1): the real ctor.
        new (obj) PerformWork(&entityInner, heading, 7, static_cast<int64_t>(timeVal), 0, -1);
        pw7 = obj;
    }
    WA_IbpCtor(&ibpA, pw7);
    WA_Append(list, &ibpA);

    // --- ShowTool (~Axe = -2) when the citizen is currently carrying nothing ---
    if (WA_CitizenCarryCount(citizenIBP) < 1) {
        obj = System::MemoryAllocator::MemAlloc(0x30, 8);
        void* stAxe = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
            stAxe = ActChild_ShowToolCtor_42890(obj, &entityInner, -2 /* ~Axe */);
        }
        WA_IbpCtor(&ibpA, stAxe);
        WA_Append(list, &ibpA);
    }

    // --- PickupRawMaterial action (self+0x20 = citizen, self+0x28 = pile) ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* prm = nullptr;
    if (obj) {
        uintptr_t targetSlot  = targetInner;
        uintptr_t citizenSlot = *reinterpret_cast<uintptr_t*>(citizenIBP);
        prm = WorkAct_PickupRawMaterialCopyCtor_c9b70(obj, &citizenSlot, &targetSlot);
    }
    WA_IbpCtor(&ibpB, prm);
    WA_Append(list, &ibpB);

    // --- CompleteTask action ---
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t targetSlot  = targetInner;
        uintptr_t citizenSlot = *reinterpret_cast<uintptr_t*>(citizenIBP);
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, workId, &citizenSlot, &targetSlot, 0);
    }
    WA_IbpCtor(&ibpB, ct);
    WA_Append(list, &ibpB);

    // --- inline PerformWork action, kind 6 ---
    obj = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* pw6 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), WA_C_172124());
        // the inlined PerformWork ctor 0x1400c9720 (useTool = 0, +0x44 = -1): the real ctor.
        new (obj) PerformWork(&entityInner, heading, 6, static_cast<int64_t>(timeVal), 0, -1);
        pw6 = obj;
    }
    WA_IbpCtor(&ibpA, pw6);
    WA_Append(list, &ibpA);

    // --- MoveTo: walk to the pile. pathType = carrying>0 ? 4 : 1; mask = (1<<(cellFlag&0x1f)) | 0xe1f ---
    obj = System::MemoryAllocator::MemAlloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        int carried = WA_CitizenCarryCount(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        int pathType = (carried > 0) ? 4 : 1;
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
        int mask = static_cast<int>((1u << (cellFlag & 0x1f)) | 0xe1fu);
        mv = Pf_MoveToCtor_9170(obj, familyIBP, pathType, 0, worldPos, mask, 0, WA_MoveSpeed());
    }
    WA_IbpCtor(&ibpB, mv);
    WA_Append(list, &ibpB);

    // --- ShowTool (~Axe = -2) when the citizen IS carrying something ---
    if (WA_CitizenCarryCount(citizenIBP) > 0) {
        obj = System::MemoryAllocator::MemAlloc(0x30, 8);
        void* stAxe2 = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
            stAxe2 = ActChild_ShowToolCtor_42890(obj, &entityInner, -2 /* ~Axe */);
        }
        WA_IbpCtor(&ibpA, stAxe2);
        WA_Append(list, &ibpA);
    }
}

// ============================================================
// PlayAnimation_Action_Ctor (0x1400c2490)
// ============================================================
//   param_1 = citizen InternalBasePtr*
//   param_2 = priority/workId (u32)
//   param_3 = citizenIndex (u32, forwarded to CompleteTask param_5)
// Builds SendComponentMessage (→ aiComp+0x58), CompleteTask, and an inline
// PlayAnimation action, appending the latter two to the task list.
void WorkAct_PlayAnimationCtor_c2490(void* citizenIBP, uint32_t priority, uint32_t citizenIndex) {

    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    WorkBase_TaskListCleanup_c5400(list);

    uintptr_t ibp30 = 0, ibpRes20 = 0;

    // --- SendComponentMessage → aiComp+0x58 ---
    void* obj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp30, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(aiComp + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibp30;

    // --- CompleteTask action (param_3 forwarded as param_5) ---
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, priority, &entityInner, &innerIbp, citizenIndex);
    }
    WA_IbpCtor(&ibp30, ct);
    WA_Append(list, &ibp30);

    // --- inline PlayAnimation action ---
    void* inlineObj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* pa = nullptr;
    if (inlineObj) {
        // System::Time::Time(&t, DAT_140172100)
        uint64_t timeVal = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), kRdata_140172100);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        // The real ctor (Action(), fields zeroed; +0x28 animation = 0), then the stores.
        PlayAnimation* a = new (inlineObj) PlayAnimation();
        a->_field20 = reinterpret_cast<void*>(entityInner);
        a->_field30 = reinterpret_cast<void*>(timeVal);
        pa = inlineObj;
    }
    WA_IbpCtor(&ibpRes20, pa);
    WA_Append(list, &ibpRes20);
}

// ============================================================
// FUN_1400bb0a0 — grazing/tend work-action ctor (pasturework / tendarea)
// ============================================================
//   param_1 = citizen InternalBasePtr*
//   param_2 = priority (u32)
//   param_3 = workEntity InternalBasePtr* (u64* — *param_3 = inner)
//   param_4 = unused (u32, used as SendComponentMessage msg id slot init)
//   param_5 = float* targetPos (MoveTo destination)
//   param_6 = float* srcPos (used to compute heading; the original calls Vector3 sub
//             + Normalize + Heading but discards the result)
//   param_7 = typeId (u32, second SendComponentMessage msg id)
//   param_8 = u32 (SendComponentMessage param_4)
//   param_9 = u32 (SendComponentMessage param_6)
void WorkAct_GrazingWorkCtor_bb0a0(void* citizenIBP, uint32_t priority, void* workEntityIBP,
                                   uint32_t /*unused4*/, float* targetPos, float* srcPos,
                                   uint32_t typeId, uint32_t flag8, uint32_t cellIdx9) {

    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    WorkBase_TaskListCleanup_c5400(list);

    uintptr_t ibp30 = 0, ibpResA = 0, ibpResB = 0;

    // --- SendComponentMessage #1 → aiComp+0x58 ---
    void* obj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp30, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(aiComp + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibp30;

    // --- CompleteTask action ---
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, priority, &entityInner, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp30, ct);
    WA_Append(list, &ibp30);

    // --- heading: Vector3(srcPos-targetPos) → Normalize → Heading (XMM0 → used by Ctor7) ---
    // Disasm: dst=[param_6=srcPos], src=[param_5=targetPos]; v = srcPos - targetPos.
    float heading = 0.0f;
    if (targetPos && srcPos) {
        Math::Vector3 v3(srcPos[0] - targetPos[0], srcPos[1] - targetPos[1], srcPos[2] - targetPos[2]);
        Math::Vector3 nv = v3.Normalize();   // was the hidden RDX return slot
        heading = nv.Heading();
    }

    // --- ShowTool (~Hoe = -2) ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        st = ActChild_ShowToolCtor_42890(obj, &entityInner, ~0 /* ~Hoe = -1 (Hoe==0); disasm OR R8D,0xFFFFFFFF */);
    }
    WA_IbpCtor(&ibpResA, st);
    WA_Append(list, &ibpResA);

    // --- SendComponentMessage #2 (msg id = param_7, params 8/9) ---
    obj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* sm2 = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = typeId;
        void* entityInner = reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(workEntityIBP));
        sm2 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, flag8, &innerIbp, cellIdx9);
    }
    WA_IbpCtor(&ibpResB, sm2);
    WA_Append(list, &ibpResB);

    // --- PerformWork_Ctor7: heading(XMM2)=computed heading, actionType(R9)=0xC,
    //     time=DAT_1401721b0, p6=1, p7=3 (disasm: R9D=0xc, not the decompile's Cancel|Normal) ---
    obj = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), kRdata_1401721b0);
        pw7 = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, 0xC, static_cast<int64_t>(timeVal), 1, 3);
    }
    WA_IbpCtor(&ibpResA, pw7);
    WA_Append(list, &ibpResA);

    // --- MoveTo (Fastest=3, destination=targetPos) ---
    obj = System::MemoryAllocator::MemAlloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
        float speed = kRdata_140172280;
        mv = Pf_MoveToCtor_9170(obj, familyIBP, 3 /*Fastest*/, 0, targetPos, 0xe1f, 0, speed);
    }
    WA_IbpCtor(&ibpResA, mv);
    WA_Append(list, &ibpResA);

    // --- ShowTool (Shovel) ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        st2 = ActChild_ShowToolCtor_42890(obj, &entityInner, 3 /*Shovel; disasm MOV R8D,3*/);
    }
    WA_IbpCtor(&ibpResA, st2);
    WA_Append(list, &ibpResA);
}

// ============================================================
// FUN_1400be6e0 — plant/orchard work-action ctor (foresterwork / plantorchard)
// ============================================================
//   param_1  = citizen InternalBasePtr*
//   param_2  = priority (u32)
//   param_3  = workEntity inner (u64; CompleteTask/CreateNaturalResource param_2)
//   param_4  = unused (u32, SendComponentMessage msg id slot init)
//   param_5  = float* position (MoveTo destination)
//   param_6  = u32 (CreateNaturalResource param_4)
//   param_7  = u32 (CompleteTask param_5)
//   param_8  = longlong* — if *param_8 != 0, emit optional SendComponentMessage #2
//   param_9  = u32* (msg id for optional SendComponentMessage #2 = *param_9)
//   param_10 = u64 unused (SendComponentMessage #2 param_4 = 0 in disasm)
//   param_11 = u32 (SendComponentMessage #2 param_6)
void WorkAct_PlantWorkCtor_be6e0(void* citizenIBP, uint32_t priority, void* workEntityInner,
                                 uint32_t /*unused4*/, float* position, uint32_t createParam6,
                                 uint32_t completeParam7, int64_t* condPtr8, uint32_t* msgIdPtr9,
                                 uint64_t /*unused10*/, uint32_t smParam11) {

    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    WorkBase_TaskListCleanup_c5400(list);

    uintptr_t ibp40 = 0, ibpRes8 = 0;

    // --- SendComponentMessage #1 → aiComp+0x58 ---
    void* obj = System::MemoryAllocator::MemAlloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp40, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(aiComp + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibp40;

    // --- CompleteTask action (param_5 = param_7) ---
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, priority, &entityInner, &innerIbp, completeParam7);
    }
    WA_IbpCtor(&ibp40, ct);
    WA_Append(list, &ibp40);

    // --- optional SendComponentMessage #2 when *param_8 != 0 ---
    if (condPtr8 && *condPtr8 != 0) {
        obj = System::MemoryAllocator::MemAlloc(0x40, 8);
        void* sm2 = nullptr;
        if (obj) {
            uintptr_t innerIbp = 0;
            WA_IbpCtor(&innerIbp, nullptr);
            uint32_t msgId = msgIdPtr9 ? *msgIdPtr9 : 0;
            uintptr_t entityInner = static_cast<uintptr_t>(*condPtr8);
            sm2 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, smParam11);
        }
        WA_IbpCtor(&ibp40, sm2);
        WA_Append(list, &ibp40);
    }

    // --- ShowTool (~Hoe = -2) ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        st = ActChild_ShowToolCtor_42890(obj, &entityInner, ~0 /* ~Hoe = -1 (Hoe==0); disasm OR R8D,0xFFFFFFFF */);
    }
    WA_IbpCtor(&ibpRes8, st);
    WA_Append(list, &ibpRes8);

    // --- CreateNaturalResource action (param_2 = workEntity inner = *param_8, param_4 = param_6) ---
    obj = System::MemoryAllocator::MemAlloc(0x38, 8);
    void* cnr = nullptr;
    if (obj) {
        uintptr_t srcInner = condPtr8 ? static_cast<uintptr_t>(*condPtr8) : 0;
        cnr = ActChild_CreateNRCopyCtor_c6a20(obj, reinterpret_cast<uint64_t>(workEntityInner), &srcInner, createParam6);
    }
    WA_IbpCtor(&ibpRes8, cnr);
    WA_Append(list, &ibpRes8);

    // --- PerformWork_Ctor7: heading(XMM2)=const DAT_1401722a8, actionType(R9)=0xC,
    //     time=DAT_140172178, p6=1, p7=-1 (disasm: R9D=0xc, XMM2=[0x1401722a8]) ---
    obj = System::MemoryAllocator::MemAlloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), kRdata_140172178);
        float headingC = WA_RdataF(kRdata_1401722a8);
        pw7 = Pf_PerformWorkCtor7_9720(obj, &entityInner, headingC, 0xC, static_cast<int64_t>(timeVal), 1, -1);
    }
    WA_IbpCtor(&ibpRes8, pw7);
    WA_Append(list, &ibpRes8);

    // --- MoveTo (Fastest=3, destination=position) ---
    obj = System::MemoryAllocator::MemAlloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
        float speed = kRdata_140172280;
        mv = Pf_MoveToCtor_9170(obj, familyIBP, 3 /*Fastest*/, 0, position, 0xe1f, 0, speed);
    }
    WA_IbpCtor(&ibpRes8, mv);
    WA_Append(list, &ibpRes8);

    // --- ShowTool (Shovel) ---
    obj = System::MemoryAllocator::MemAlloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        st2 = ActChild_ShowToolCtor_42890(obj, &entityInner, 3 /*Shovel; disasm MOV R8D,3*/);
    }
    WA_IbpCtor(&ibpRes8, st2);
    WA_Append(list, &ibpRes8);
}
