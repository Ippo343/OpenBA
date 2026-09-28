// code/Game/Component/citizenai_actionctors.cpp
//
// CitizenAI "action-ctor family" (Brick 4b) — the multi-action sequence BUILDERS that
// the CitizenAI state handlers (citizenai_handlers / the State_* dispatchers) call to
// assemble the citizen's Action/Work queue and push it onto the action stack at
// CharacterComponent+0x48. Faithful 1:1 ports, transcribed from the Ghidra decompile and
// cross-checked against the disassembly (NavigateTo @0x1400bda50 + WalkTowards
// @0x1400bb490 disasm verified offset-by-offset; the six PerformWork builders are the
// same shape).
//
// VERIFIED real addresses (reconciled vs each calling handler's get_function_callees):
//   0x1400c1070  PerformWork_Action_Ctor4   <- State_LongTermPlan  (the big chop/build seq)
//   0x1400bd070  PerformWork_Action_Ctor5   <- State_SeekHappiness
//   0x1400bd420  PerformWork_Action_Ctor6   <- State_AdultTransition
//   0x1400bd790  PerformWork_Action_Ctor8   <- State_ElderTransition
//   0x1400bccb0  PerformWork_Action_Ctor10  <- State_Education
//   0x1400bc790  PerformWork_Action_Ctor11  <- State_GrowUp
//   0x1400bda50  Pathfinding_NavigateTo     <- State_Work
//   0x1400bb490  Pathfinding_WalkTowards    <- State_Work / State_FindWork
//   0x1400ba6b0  PlayAnimation_Action_Ctor4 <- (sibling of the already-ported Ctor5 c26c0)
//
// Each builder follows the same skeleton:
//   self    = *param_1 (a CitizenComponent) ; char = *(self+0x278) ; stack = char+0x48
//   FUN_1400c5400(stack)                       — clear/cleanup the action stack
//   alloc a SendComponentMessage action (0x40) -> CharacterComponent+0x58 (DeletePtr old)
//   ... alloc & CopyCtor each per-step Action (SendComponentMessage / ConsumeRaw /
//       DropOffRaw / PickupStored / ShowTool / CompleteTask / a raw inline PerformWork /
//       PlayAnimation), wrap in an InternalBasePtr, FUN_1400c54e0(stack, &ibp) — push.
//   ... alloc a MoveTo action (0x480) via MoveTo_Action_Ctor, push.
//
// SELF-CONTAINED-BRICK NOTE: the SendComponentMessage message-id globals
// DWORD_1401c70e8 / DWORD_1401c7158 and the HappinessComponent id DAT_1401c42c0 are NOT
// registered in standalone (read 0). We null-guard them exactly like the existing
// citizenai_handlers.cpp PlayAnimationActionCtor5 — the message actions then carry id 0.
// The RawMaterialDescription id DAT_1401c7cb8 IS registered+filled, so the Ctor5/Ctor10
// storage-limit component scan reads the real CRC.
//
// ALL exe sub-callees are already registered (resolved via ResolveAddr) — bodies NOT
// duplicated here:
//   0x1400c5400  FUN_1400c5400  action-stack cleanup       (reg_pf_ctors / reg_work_base)
//   0x1400c54e0  FUN_1400c54e0  action-stack push          (reg_pf_ctors)
//   0x1400cb1d0  SendComponentMessage_Action_CopyCtor      (reg_pf_ctors)
//   0x1400c9170  MoveTo_Action_Ctor                        (reg_pf_ctors)
//   0x1400c5da0  CompleteTask_Action_CopyCtor              (reg_pf_ctors)
//   0x1400012d0  Entity_FindComponent_Family               (reg_action_move / reg_pf_ctors)
//   0x1400c29a0  Entity_impl_29a0 (returns float anim-idx) (reg_action_child_ctors)
//   0x1400c7cd0  DropOffRawMaterial_Action_CopyCtor        (reg_action_child_ctors2)
//   0x1400c63b0  ConsumeRawMaterial_Action_CopyCtor        (reg_action_child_ctors2)
//   0x1400ca520  PickupStoredRawMaterial_Action_CopyCtor   (reg_action_child_ctors2)
//   0x140042890  ShowTool_Action_Ctor                      (reg_action_child_ctors)
//
// Engine helpers (engine_funcs.h): fn_ObjectInternal_Ctor, fn_InternalBasePtr_Ctor,
//   fn_InternalBasePtr_CheckPtr, fn_InternalBasePtr_DeletePtr, fn_Time_Ctor_Float,
//   System::MemoryAllocator::MemAlloc(size, 8), g_pGameWorldSingleton.
//
// Action vtables (exe .rdata): vtable_Action 0x14015faa0, vtable_PerformWork 0x14015fea0,
//   vtable_PlayAnimation 0x140158950 (all confirmed from the ctor disasm `LEA RAX,[...]`).
//
// Float consts (exe .rdata, read with read_memory):
//   0x140172124 = 0.75   (Ctor4 PerformWork Time)
//   0x140172174 = 2.0    (Ctor6 PerformWork Time)
//   0x140172194 = 4.0    (Ctor8 NavigateTo's "g_pAgeMultiplier" sibling Time)
//   0x140172198 = 5.0    (NavigateTo PerformWork Time; "g_pAgeMultiplier" in decompile)
//   0x14017214c = 1.0    (Ctor5/Ctor10/Ctor11 "g_fUVScale" Time)
//   0x140172280 = -1.0   (MoveTo randomized-speed sentinel)
//


#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Action/PlayAnimation.h"   // B.3.8 CB1a (Vtable)
#include "Action/PerformWork.h"   // B.3.8 CB1a (Vtable)
#include "Action/Action.h"   // B.3.8 CB1a (Vtable)
#include "resolve.h"
#include "Component/HappinessComponent.h"   // ENDGAME B.3.9 CB: kHappinessMessageTargetTypeId
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include "Object/factory_registration.h"   // Object::GetTypeIdByName (c7158 heal)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>   // std::memcpy (B.3.9 EXA)
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CIT (TypeId)
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 C1: 0x1401c7cb8 cell class
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Game/exe_constants.h"
#include "system/Value.h"   // RtSystem_Time_Ctor
#include "system/MemoryAllocator.h"   // B3R: the allocator doors

namespace {

// Registered exe sub-callees, called by name: WorkBase_TaskListCleanup_c5400 0x1400c5400,
// Pf_ActStackPush_54e0 0x1400c54e0, Pf_SendComponentMessageCopyCtor_b1d0 0x1400cb1d0,
// Pf_MoveToCtor_9170 0x1400c9170, Pf_CompleteTaskCopyCtor_5da0 0x1400c5da0,
// ActMv_FindComponentFamily_12d0 0x1400012d0, ActChild_Entity29a0_c29a0 0x1400c29a0,
// ActChild_DropOffRawCopyCtor_c7cd0 0x1400c7cd0, ActChild_ConsumeRawCopyCtor_c63b0
// 0x1400c63b0, ActChild_PickupStoredCopyCtor_ca520 0x1400ca520,
// ActChild_ShowToolCtor_42890 0x140042890. Message targets: 70e8 (CitizenAIComponent) /
// 42c0 (HappinessComponent) are the PORT GAP constants of their class headers
// (ENDGAME B.3.9 CB). The .rdata floats are exe_constants.h's kRdata_140172124 (0.75),
// _140172174 (2.0), _140172194 (4.0), _140172198 (5.0), _14017214c (1.0), _140172280 (-1.0).

// ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c7158 is CitizenComponent's registered
// type-id cell (INVENTORY_C Table 1); it always holds CitizenComponent's id after
// Fill*, so the previous "heal to the class static when the raw read is 0" fallback
// (kept as history above) is now mechanically equivalent to calling the static
// directly. Unambiguous here: every c7158 use in this file targets CitizenComponent.
static inline uint32_t MsgId7158() {
    return CitizenComponent::TypeId();
}
static inline void*    MemA(uint64_t sz) {
    return System::MemoryAllocator::MemAlloc(sz, 8);
}

// Build a raw inline PerformWork action at `mem` (0x48 bytes). animIdx is the XMM float
// stored at +0x28 (or a literal int bit-pattern when the original used 0xc47a0000 = -1000.0f).
// timeBits = packed System::Time (u64) for +0x30, type = +0x2c work type.
// ENDGAME B.3.9 EXA: the real class — PerformWork's parameterised ctor
// (0x1400c9720) with useTool = 0 and field44 = -1 writes exactly the stores this
// helper wrote by hand after the `??0Internal` door and the two stamps (the
// entity IBP is copied raw at +0x20, as retail's inlined ctor does).
static void InitPerformWork(void* mem, uint64_t entityVal, uint32_t animIdxBits,
                            uint32_t type, uint64_t timeBits) {
    float animIdx;
    std::memcpy(&animIdx, &animIdxBits, sizeof animIdx);
    ::new (mem) PerformWork(&entityVal, animIdx, type,
                            static_cast<int64_t>(timeBits), /*useTool=*/0, /*field44=*/-1);
}

// Resolve the (self+0x278)+0x48 action-stack; return the stack pointer and store the
// resolved char object via `outChar`. `doCleanup` runs FUN_1400c5400 cleanup (all builders
// clean unconditionally at the top EXCEPT Ctor4, which gates it behind its build flag).
static int64_t* PrologueResolveStack(void* param_1, int64_t* outChar, bool doCleanup = true) {
    int64_t self = *reinterpret_cast<int64_t*>(param_1);
    int64_t ch = *reinterpret_cast<int64_t*>(self + 0x278);
    *outChar = ch;
    int64_t* stack = reinterpret_cast<int64_t*>(ch + 0x48);
    if (doCleanup) WorkBase_TaskListCleanup_c5400(stack);
    return stack;
}

// Build + install the leading SendComponentMessage action into CharacterComponent+0x58
// (msgId = DWORD_1401c70e8, the standard "begin work" message). char = resolved object.
static void InstallLeadMessage(void* param_1, int64_t ch) {
    void* mem = MemA(0x40);
    void* act = nullptr;
    if (mem) {
        int64_t destIBP = 0;
        new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
        act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 0, reinterpret_cast<uint64_t*>(&destIBP), 0);
    }
    int64_t ibp = 0;
    new (&ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
    if (*reinterpret_cast<int64_t*>(ch + 0x58) != 0) {
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(ch + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    }
    *reinterpret_cast<int64_t*>(ch + 0x58) = ibp;
}

// Build + push the trailing MoveTo action toward `dest`, mask = the pathfinding flags.
static void PushMoveTo(void* param_1, int64_t* stack, uint32_t pathType, float* dest, int mask) {
    void* mem = MemA(0x480);
    void* act = nullptr;
    if (mem) {
        int64_t entityVal = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
        int64_t fam = 0;
        ActMv_FindComponentFamily_12d0(static_cast<uintptr_t>(entityVal), &fam);
        float speed = kRdata_140172280;
        act = Pf_MoveToCtor_9170(mem, &fam, pathType, 0, dest, mask, 0, speed);
    }
    int64_t ibp = 0;
    new (&ibp) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
    Pf_ActStackPush_54e0(stack, &ibp);
}

// Locate the storage-limit component (GetTypeId == DWORD_1401c7cb8) on the entity-list
// object `listObj` (ptr +0x28, count +0x30, stride 8). Returns the matching component or
// null. Used by Ctor5/Ctor10 (the original's pIVar6 scan).
static void* FindLimitComponent(int64_t listObj) {
    void* found = nullptr;
    uint32_t want = RawMaterialDescription::TypeId();
    int n = *reinterpret_cast<int*>(listObj + 0x30);
    if (0 < n) {
        int idx = 0; int64_t byteOff = 0;
        for (;;) {
            int64_t arr = *reinterpret_cast<int64_t*>(listObj + 0x28);
            void* child = *reinterpret_cast<void**>(arr + byteOff);
            uint32_t tid = reinterpret_cast<Object::Base*>(child)->Type();   // B.3.8 bridge (slot 2)
            if (tid == want) {
                int64_t arr2 = *reinterpret_cast<int64_t*>(listObj + 0x28);
                void* match = *reinterpret_cast<void**>(arr2 + static_cast<int64_t>(idx) * 8);
                if (Object::IsDerivedFromNullSafe(match, want))
                    found = match;
                break;
            }
            idx++; byteOff += 8;
            if (idx >= n) break;
        }
    }
    return found;
}

} // namespace

// ============================================================
// 0x1400c1070 — PerformWork_Action_Ctor4  (called by State_LongTermPlan)
// The full "fetch axe / chop / drop raw / build" sequence. Only runs when param_6 (the
// build flag) is set. Pushes: SendComponentMessage(lead)+DropOffRaw(into +0x58); then
// PerformWork(type 7) + ShowTool(~Axe=-? see note) + DropOffRaw + PerformWork(type 6) +
// MoveTo + ShowTool. animIdx = Entity_impl_29a0(dest). The work-mask bit comes from the
// terrain cell flag byte at grid+0x80 + param_4*0x30 + 0x29.
//   param_1 = CitizenComponent IBP ; param_2 = &targetEntityIBP ; param_3 = float* dest
//   param_4 = cell index ; param_5 = drop-raw flag byte ; param_6 = build flag.
// NOTE: ShowTool takes ~Axe (the original passes the bitwise-NOT of the Axe tool enum).
// Axe's enum value isn't separately known here; the original literal is reproduced as the
// disasm immediate. We pass the same immediate ShowTool received (see s_showToolAxeArg).
// ============================================================
// ShowTool's 3rd arg is the immediate 0xfffffffe (-2), verified from the Ctor4 disasm
// (`MOV R8D,0xfffffffe` before both CALL 0x140042890). The decompile rendered it as "~Axe".
namespace { static const int kShowToolAxeArg = static_cast<int>(0xfffffffe); }

void CitizenAI_PerformWorkCtor4_c1070(void* param_1, int64_t* param_2, float* param_3,
                                      int param_4, uint8_t param_5, uint32_t param_6) {
    int64_t ch = 0;
    // Ctor4 gates BOTH the stack cleanup and the lead block behind param_6 (disasm:
    // `CMP byte[RBP+0x68],DIL; JZ` skips cleanup + lead to the cell-flags math).
    int64_t* stack = PrologueResolveStack(param_1, &ch, /*doCleanup=*/false);

    if (static_cast<char>(param_6) != '\0') {
        WorkBase_TaskListCleanup_c5400(stack);
        // lead SendComponentMessage -> CharacterComponent+0x58
        InstallLeadMessage(param_1, ch);
        // DropOffRaw action (also stored at +0x58, replacing the message) — faithful: the
        // original re-derives local_68 and OVERWRITES char+0x58 with the drop-raw IBP.
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);  // *(undefined8*)param_1
            act = ActChild_DropOffRawCopyCtor_c7cd0(mem, &entityIBP, reinterpret_cast<uint64_t*>(&destIBP),
                                                    0, 0xffffffff, param_5);
        }
        int64_t ibp = 0;
        new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }

    // work mask bit from the cell flags byte
    uint8_t bit = *reinterpret_cast<uint8_t*>(
        *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xf8) + 0x80)
        + 0x29 + static_cast<int64_t>(param_4) * 0x30);

    int64_t target = *param_2;
    if (target == 0) return;

    uint32_t animIdx = 0;
    {
        int64_t targetIBP = target;
        float f = ActChild_Entity29a0_c29a0(param_3, &targetIBP);
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    // PerformWork (type 7) @0.75s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_140172124);
            InitPerformWork(mem, entityVal, animIdx, 7, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // ShowTool (equip)
    {
        void* mem = MemA(0x30);
        void* act = nullptr;
        if (mem) {
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = ActChild_ShowToolCtor_42890(mem, &entityIBP, kShowToolAxeArg);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // DropOffRaw
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t targetIBP = *reinterpret_cast<uint64_t*>(param_2);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = ActChild_DropOffRawCopyCtor_c7cd0(mem, &entityIBP, &targetIBP, 0, 0xffffffff, param_5);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 6) @0.75s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_140172124);
            InitPerformWork(mem, entityVal, animIdx, 6, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // MoveTo (Faster=4, disasm `LEA R8D,[R9+0x4]`) with mask = (1<<bit)|0xe1f
    PushMoveTo(param_1, stack, 4 /*Faster*/, param_3, (1 << (bit & 0x1f)) | 0xe1f);
    // trailing ShowTool (unequip)
    {
        void* mem = MemA(0x30);
        void* act = nullptr;
        if (mem) {
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = ActChild_ShowToolCtor_42890(mem, &entityIBP, kShowToolAxeArg);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
}

// ============================================================
// 0x1400bd070 — PerformWork_Action_Ctor5  (called by State_SeekHappiness)
// lead SendComponentMessage(70e8) -> +0x58 ; scan for limit component (7cb8) -> pIVar6 ;
// SendComponentMessage(7158, a4=1, a6 = pIVar6->+0x54) ; ConsumeRaw ; PerformWork(type 8,
// time 1.0) ; MoveTo(Slow). animIdx = Entity_impl_29a0(dest).
//   param_1 = CitizenComponent IBP ; param_2 = &srcEntityIBP ; param_3 = float* dest ;
//   param_4 = entity-list object (for the limit scan + ConsumeRaw resType list).
// ============================================================
void CitizenAI_PerformWorkCtor5_bd070(void* param_1, uint64_t* param_2, float* param_3, int64_t param_4) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdx;
    {
        uint64_t srcIBP = *param_2;
        float f = ActChild_Entity29a0_c29a0(param_3, &srcIBP);
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    void* limit = FindLimitComponent(param_4);

    // SendComponentMessage(7158, a4=1, a6 = limit->+0x54)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            uint32_t a6 = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(limit) + 0x54);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 1, reinterpret_cast<uint64_t*>(&destIBP), a6);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // ConsumeRaw(resType list = *param_2, list = param_4, flag a4=1?  see disasm: param_4(arg) is `param_4=1` byte)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t resType = *param_2;  // local_res10 = *local_res10
            act = ActChild_ConsumeRawCopyCtor_c63b0(mem, &resType, param_4, 1, 1);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 8) @1.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_14017214c);
            InitPerformWork(mem, entityVal, animIdx, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // MoveTo (Slow=1) mask 0xe1f
    PushMoveTo(param_1, stack, 1 /*Slow*/, param_3, 0xe1f);
}

// ============================================================
// 0x1400bd420 — PerformWork_Action_Ctor6  (called by State_AdultTransition)
// lead SendComponentMessage(70e8) -> +0x58 ; animIdx = Entity_impl_29a0(dest) ; if param_4:
// SendComponentMessage(7158, a4=0) ; SendComponentMessage(7158, a4=2) ; PerformWork(type 8,
// time 2.0) ; MoveTo(Slow).
//   param_1 = CitizenComponent IBP ; param_2 = &targetEntityIBP ; param_3 = float* dest ;
//   param_4 = "first message" flag byte.
// ============================================================
void CitizenAI_PerformWorkCtor6_bd420(void* param_1, int64_t* param_2, float* param_3, char param_4) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdx;
    {
        int64_t targetIBP = *param_2;
        float f = ActChild_Entity29a0_c29a0(param_3, &targetIBP);
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    // helper to push a SendComponentMessage(7158, a4) toward *param_2
    auto pushMsg7158 = [&](uint32_t a4) {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t destIBP = *reinterpret_cast<uint64_t*>(param_2);  // local_res8 = *param_2
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, a4, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    };

    if (param_4 != '\0') pushMsg7158(0);
    pushMsg7158(2);

    // PerformWork (type 8) @2.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_140172174);
            InitPerformWork(mem, entityVal, animIdx, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, param_3, 0xe1f);
}

// ============================================================
// 0x1400bd790 — PerformWork_Action_Ctor8  (called by State_ElderTransition)
// lead SendComponentMessage(70e8) -> +0x58 ; SendComponentMessage(7158, a4=0, dest=*param_2)
// ; PerformWork(type 8, animIdx literal -1000.0f, time 4.0) ; MoveTo(Slow).
//   param_1 = CitizenComponent IBP ; param_2 = &targetEntityIBP ; param_3 = float* dest.
// ============================================================
void CitizenAI_PerformWorkCtor8_bd790(void* param_1, uint64_t* param_2, float* param_3) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    // SendComponentMessage(7158, a4=0) toward *param_2
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t destIBP = *param_2;
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 0, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 8) animIdx = 0xc47a0000 literal, time 4.0
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_140172194);
            InitPerformWork(mem, entityVal, 0xc47a0000u, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, param_3, 0xe1f);
}

// ============================================================
// 0x1400bccb0 — PerformWork_Action_Ctor10  (called by State_Education)
// lead SendComponentMessage(70e8) -> +0x58 ; animIdx = Entity_impl_29a0(dest) ; scan for
// limit component (7cb8) -> pIVar7, read flags pIVar7->+0x5c ; SendComponentMessage(7158,
// a4 = 6 + ((flags>>15)&1)) ; ConsumeRaw(resType=*param_2, list=param_4, flag = that a4) ;
// PerformWork(type 8, time 1.0) ; MoveTo(Slow).
//   param_1 = CitizenComponent IBP ; param_2 = &srcEntityIBP ; param_3 = float* dest ;
//   param_4 = entity-list object.
// ============================================================
void CitizenAI_PerformWorkCtor10_bccb0(void* param_1, uint64_t* param_2, float* param_3, uint64_t param_4) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdx;
    {
        uint64_t srcIBP = *param_2;
        float f = ActChild_Entity29a0_c29a0(param_3, &srcIBP);
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    void* limit = FindLimitComponent(static_cast<int64_t>(param_4));
    uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(limit) + 0x5c);
    uint32_t a4 = ((flags >> 0xf) & 1) + 6;

    // SendComponentMessage(7158, a4)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, a4, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // ConsumeRaw(resType=*param_2, list=param_4, flag=a4)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t resType = *param_2;
            act = ActChild_ConsumeRawCopyCtor_c63b0(mem, &resType, static_cast<int64_t>(param_4), a4, 1);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 8) @1.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_14017214c);
            InitPerformWork(mem, entityVal, animIdx, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, param_3, 0xe1f);
}

// ============================================================
// 0x1400bc790 — PerformWork_Action_Ctor11  (called by State_GrowUp)
// The longest builder. lead SendComponentMessage(70e8) -> +0x58 ;
//   animIdx_a = Entity_impl_29a0(param_4) ; animIdx_b = Entity_impl_29a0(param_6) ;
//   SendComponentMessage(7158, a4=3) ; ConsumeRaw(resType=*param_1.entity, list=param_2, flag=3,reserve=0) ;
//   PerformWork(type 8, animIdx_b, time 5.0) ; MoveTo(Slow, dest=param_6) ;
//   PickupStored(src=*param_3, list=param_2, requested=1, flag=0) ;
//   PerformWork(type 8, animIdx_a, time 1.0) ; MoveTo(Slow, dest=param_4).
//   param_1 = CitizenComponent IBP ; param_2 = resource-list count obj ; param_3 = &srcEntityIBP ;
//   param_4 = float* destA ; param_5 = &dstEntityIBP ; param_6 = float* destB.
// ============================================================
void CitizenAI_PerformWorkCtor11_bc790(void* param_1, int64_t param_2, int64_t* param_3,
                                       float* param_4, int64_t* param_5, float* param_6) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdxA, animIdxB;
    {
        int64_t srcIBP = *param_3;
        float f = ActChild_Entity29a0_c29a0(param_4, &srcIBP);
        animIdxA = *reinterpret_cast<uint32_t*>(&f);
    }
    {
        int64_t dstIBP = *param_5;
        float f = ActChild_Entity29a0_c29a0(param_6, &dstIBP);
        animIdxB = *reinterpret_cast<uint32_t*>(&f);
    }

    // SendComponentMessage(7158, a4=3)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 3, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // ConsumeRaw(entity=*param_1.entity, list=param_2, flag=3, reserve=0)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = ActChild_ConsumeRawCopyCtor_c63b0(mem, &entityIBP, param_2, 3, 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 8) animIdxB @5.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_140172198);
            InitPerformWork(mem, entityVal, animIdxB, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // MoveTo(Slow) dest = param_6
    PushMoveTo(param_1, stack, 1 /*Slow*/, param_6, 0xe1f);
    // PickupStored(src=*param_3, list=param_2, requested=1, flag=0)
    {
        void* mem = MemA(0x48);
        void* act = nullptr;
        if (mem) {
            uint64_t srcIBP = *reinterpret_cast<uint64_t*>(param_3);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = ActChild_PickupStoredCopyCtor_ca520(mem, &entityIBP, &srcIBP, param_2, 1, 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 8) animIdxA @1.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_14017214c);
            InitPerformWork(mem, entityVal, animIdxA, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // MoveTo(Slow) dest = param_4
    PushMoveTo(param_1, stack, 1 /*Slow*/, param_4, 0xe1f);
}

// ============================================================
// 0x1400bda50 — Pathfinding_NavigateTo  (called by State_Work) — disasm-verified.
// lead SendComponentMessage(70e8) -> +0x58 ; PerformWork(type 8, animIdx literal -1000.0f,
// time 5.0 @0x140172198) ; SendComponentMessage(7158, a4=0, dest=*param_2) ; MoveTo(Slow).
//   param_1 = CitizenComponent IBP ; param_2 = &targetEntityIBP ; param_3 = u32* dest(=float*) ;
//   param_4 = byte (unused after the local_res20 store).
// ============================================================
void CitizenAI_Pathfinding_NavigateTo_bda50(void* param_1, int64_t* param_2,
                                            uint32_t* param_3, uint8_t /*param_4*/) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    // PerformWork (type 8) animIdx 0xc47a0000, time 5.0
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), kRdata_140172198);
            InitPerformWork(mem, entityVal, 0xc47a0000u, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // SendComponentMessage(7158, a4=0) toward *param_2
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t destIBP = *reinterpret_cast<uint64_t*>(param_2);
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 0, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, reinterpret_cast<float*>(param_3), 0xe1f);
}

// ============================================================
// 0x1400bb490 — Pathfinding_WalkTowards  (called by State_Work / State_FindWork) — disasm-verified.
// lead SendComponentMessage(70e8) -> +0x58 ; then if param_6==0: CompleteTask(param_2,...)
// else: SendComponentMessage(7158, a4=param_4) ; PerformWork(type 8, animIdx literal -1000.0f,
// time = param_5) ; MoveTo(Slow).
//   param_1 = CitizenComponent IBP ; param_2 = u32 (CompleteTask first arg) ; param_3 = float* dest ;
//   param_4 = u32 (msg a4) ; param_5 = float (PerformWork time) ; param_6 = byte flag.
// ============================================================
void CitizenAI_Pathfinding_WalkTowards_bb490(void* param_1, uint32_t param_2, uint32_t* param_3,
                                             uint32_t param_4, float param_5, uint32_t param_6) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    {
        void* act = nullptr;
        if (static_cast<char>(param_6) == '\0') {
            void* mem = MemA(0x38);
            if (mem) {
                int64_t destIBP = 0;
                new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);  // local_48 = *param_1
                act = Pf_CompleteTaskCopyCtor_5da0(mem, param_2, &entityIBP,
                                                            reinterpret_cast<uint64_t*>(&destIBP), 0);
            }
        } else {
            void* mem = MemA(0x40);
            if (mem) {
                int64_t destIBP = 0;
                new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                uint32_t msgId = MsgId7158();
                uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
                act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, param_4, reinterpret_cast<uint64_t*>(&destIBP), 0);
            }
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // PerformWork (type 8) animIdx 0xc47a0000, time = param_5
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), param_5);
            InitPerformWork(mem, entityVal, 0xc47a0000u, 8, t);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, reinterpret_cast<float*>(param_3), 0xe1f);
}

// ============================================================
// 0x1400ba6b0 — PlayAnimation_Action_Ctor4  (sibling of the ported Ctor5 c26c0)
// lead SendComponentMessage(70e8) -> +0x58 ; if param_5: SendComponentMessage(7158, a4=8) ;
// if *param_2: SendComponentMessage(DAT_1401c42c0, dest=*param_2, a4=0) ; a raw inline
// PlayAnimation action (vtable_PlayAnimation, +0x20=entity, +0x28=0, +0x30=Time(param_4)) ;
// MoveTo(Slow, dest=param_3).
//   param_1 = CitizenComponent IBP ; param_2 = &optionalTargetIBP ; param_3 = u32* dest(=float*) ;
//   param_4 = float anim time ; param_5 = byte flag.
// ============================================================
void CitizenAI_PlayAnimationCtor4_ba6b0(void* param_1, int64_t* param_2, uint32_t* param_3,
                                        float param_4, uint32_t param_5) {
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    // optional SendComponentMessage(7158, a4=8)
    if (static_cast<char>(param_5) != '\0') {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint32_t msgId = MsgId7158();
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 8, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // optional SendComponentMessage(DAT_1401c42c0, dest=*param_2)
    if (*param_2 != 0) {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            new (&destIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint32_t msgId = kHappinessMessageTargetTypeId;  // PORT GAP: HappinessComponent::OnMessage (see HappinessComponent.h)
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_2);  // local_res10 = *param_2
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, &entityIBP, &msgId, 0, reinterpret_cast<uint64_t*>(&destIBP), 0);
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(act));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    // raw inline PlayAnimation action (0x40 bytes)
    {
        void* mem = MemA(0x40);
        if (mem) {
            uint64_t t = 0; RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), param_4);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            // ENDGAME B.3.9 EXA: the real class (was the `??0Internal` door and
            // two stamps); then retail's inlined leaf stores.
            PlayAnimation* a = ::new (mem) PlayAnimation();
            *reinterpret_cast<uint64_t*>(&a->_field20) = entityIBP;   // raw IBP copy, as retail
            *reinterpret_cast<uint32_t*>(a->_pad28)    = 0;
            *reinterpret_cast<uint64_t*>(&a->_field30) = t;
            a->_field38                                 = nullptr;
        }
        int64_t ibp = 0; new (&ibp) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(mem));   // B.3.3 bridge
        Pf_ActStackPush_54e0(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, reinterpret_cast<float*>(param_3), 0xe1f);
}

