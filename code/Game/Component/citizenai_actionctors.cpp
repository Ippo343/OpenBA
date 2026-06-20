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
//   fn_MemAlloc(g_pMemAllocSingleton,size,8), g_pGameWorldSingleton.
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
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#ifdef STANDALONE_EXE

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {

// --- registered exe sub-callees (resolved via ResolveAddr) ---
typedef void  (*fn_actCleanup_t)(int64_t* stackAt48);                                 // 0x1400c5400
typedef void  (*fn_actPush_t)(int64_t* stackAt48, void* srcIBP);                      // 0x1400c54e0
typedef void* (*fn_sendMsg_t)(void* mem, uint64_t* entityIBP, uint32_t* msgId,
                              uint32_t a4, void* destIBP, uint32_t a6);               // 0x1400cb1d0
typedef void* (*fn_moveTo_t)(void* self, void* familyIBP, uint32_t pathType, int a4,
                             float* dest, int a6, int a7, float speed);              // 0x1400c9170
typedef void* (*fn_completeTask_t)(void* mem, uint32_t p2, uint64_t* p3,
                                   uint64_t* p4, uint32_t p5);                        // 0x1400c5da0
typedef void* (*fn_findFamily_t)(uintptr_t entity, void* retIBP);                    // 0x1400012d0
typedef float (*fn_entity29a0_t)(float* dest, void* entityIBP);                       // 0x1400c29a0
typedef void* (*fn_dropOffRaw_t)(void* mem, uint64_t* p2, uint64_t* p3, uint64_t p4,
                                 uint32_t p5, uint8_t p6);                            // 0x1400c7cd0
typedef void* (*fn_consumeRaw_t)(void* mem, uint64_t* p2, int64_t p3, uint64_t p4,
                                 uint8_t p5);                                          // 0x1400c63b0
typedef void* (*fn_pickupStored_t)(void* mem, uint64_t* p2, uint64_t* p3, int64_t p4,
                                   int p5, uint8_t p6);                               // 0x1400ca520
typedef void* (*fn_showTool_t)(void* mem, void* p2, int p3);                          // 0x140042890

static fn_actCleanup_t   s_fn_cleanup     = nullptr;
static fn_actPush_t      s_fn_push        = nullptr;
static fn_sendMsg_t      s_fn_sendMsg     = nullptr;
static fn_moveTo_t       s_fn_moveTo      = nullptr;
static fn_completeTask_t s_fn_completeTask= nullptr;
static fn_findFamily_t   s_fn_findFamily  = nullptr;
static fn_entity29a0_t   s_fn_entity29a0  = nullptr;
static fn_dropOffRaw_t   s_fn_dropOffRaw  = nullptr;
static fn_consumeRaw_t   s_fn_consumeRaw  = nullptr;
static fn_pickupStored_t s_fn_pickupStored= nullptr;
static fn_showTool_t     s_fn_showTool    = nullptr;

// --- action vtable addresses (exe .rdata) ---
static uintptr_t s_vt_Action        = 0;   // 0x14015faa0
static uintptr_t s_vt_PerformWork   = 0;   // 0x14015fea0
static uintptr_t s_vt_PlayAnimation = 0;   // 0x140158950

// --- message-id / type-id globals (exe .rdata). Self-contained bricks: 70e8/7158/42c0
//     are unregistered (read 0); 7cb8 (RawMaterialDescription) is registered+filled. ---
static const uint32_t* s_pMsgId_70e8 = nullptr;  // DWORD_1401c70e8
static const uint32_t* s_pMsgId_7158 = nullptr;  // DWORD_1401c7158
static const uint32_t* s_pMsgId_42c0 = nullptr;  // DAT_1401c42c0 (HappinessComponent id)
static const uint32_t* s_pTid_7cb8   = nullptr;  // DWORD_1401c7cb8 (RawMaterialDescription/limit)

// --- float consts (exe .rdata) ---
static const float* s_pf_172124 = nullptr;  // 0.75
static const float* s_pf_172174 = nullptr;  // 2.0
static const float* s_pf_172194 = nullptr;  // 4.0
static const float* s_pf_172198 = nullptr;  // 5.0
static const float* s_pf_17214c = nullptr;  // 1.0
static const float* s_pf_172280 = nullptr;  // -1.0

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_fn_cleanup      = reinterpret_cast<fn_actCleanup_t  >(ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_push         = reinterpret_cast<fn_actPush_t     >(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_fn_sendMsg      = reinterpret_cast<fn_sendMsg_t     >(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_fn_moveTo       = reinterpret_cast<fn_moveTo_t      >(ResolveAddr(Module::Exe, 0x1400c9170));
    s_fn_completeTask = reinterpret_cast<fn_completeTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_fn_findFamily   = reinterpret_cast<fn_findFamily_t  >(ResolveAddr(Module::Exe, 0x1400012d0));
    s_fn_entity29a0   = reinterpret_cast<fn_entity29a0_t  >(ResolveAddr(Module::Exe, 0x1400c29a0));
    s_fn_dropOffRaw   = reinterpret_cast<fn_dropOffRaw_t  >(ResolveAddr(Module::Exe, 0x1400c7cd0));
    s_fn_consumeRaw   = reinterpret_cast<fn_consumeRaw_t  >(ResolveAddr(Module::Exe, 0x1400c63b0));
    s_fn_pickupStored = reinterpret_cast<fn_pickupStored_t>(ResolveAddr(Module::Exe, 0x1400ca520));
    s_fn_showTool     = reinterpret_cast<fn_showTool_t    >(ResolveAddr(Module::Exe, 0x140042890));
    s_vt_Action        = ResolveAddr(Module::Exe, 0x14015faa0);
    s_vt_PerformWork   = ResolveAddr(Module::Exe, 0x14015fea0);
    s_vt_PlayAnimation = ResolveAddr(Module::Exe, 0x140158950);
    s_pMsgId_70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_pMsgId_7158 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    s_pMsgId_42c0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c42c0));
    s_pTid_7cb8   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_pf_172124 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172124));
    s_pf_172174 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172174));
    s_pf_172194 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172194));
    s_pf_172198 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172198));
    s_pf_17214c = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pf_172280 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    s_init = true;
}

// Convenience: guarded reads.
static inline uint32_t MsgId(const uint32_t* p) { return p ? *p : 0u; }
static inline float    Fc(const float* p, float dflt) { return p ? *p : dflt; }
static inline void*    MemA(uint64_t sz) {
    return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, sz, 8) : nullptr;
}

// Build a raw inline PerformWork action at `mem` (0x48 bytes). animIdx is the XMM float
// stored at +0x28 (or a literal int bit-pattern when the original used 0xc47a0000 = -1000.0f).
// timeBits = packed System::Time (u64) for +0x30, type = +0x2c work type.
static void InitPerformWork(void* mem, uint64_t entityVal, uint32_t animIdxBits,
                            uint32_t type, uint64_t timeBits) {
    uint8_t* p = reinterpret_cast<uint8_t*>(mem);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(mem);
    *reinterpret_cast<uintptr_t*>(p)       = s_vt_Action;
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    p[0x18]                                = 0;
    *reinterpret_cast<uintptr_t*>(p)       = s_vt_PerformWork;
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x20) = entityVal;
    *reinterpret_cast<uint32_t*>(p + 0x28) = animIdxBits;
    *reinterpret_cast<uint32_t*>(p + 0x2c) = type;
    *reinterpret_cast<uint64_t*>(p + 0x30) = timeBits;
    *reinterpret_cast<uint64_t*>(p + 0x38) = 0;
    p[0x40]                                = 0;
    *reinterpret_cast<uint32_t*>(p + 0x44) = 0xffffffff;
}

// Resolve the (self+0x278)+0x48 action-stack; return the stack pointer and store the
// resolved char object via `outChar`. `doCleanup` runs FUN_1400c5400 cleanup (all builders
// clean unconditionally at the top EXCEPT Ctor4, which gates it behind its build flag).
static int64_t* PrologueResolveStack(void* param_1, int64_t* outChar, bool doCleanup = true) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
    int64_t self = *reinterpret_cast<int64_t*>(param_1);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x278));
    int64_t ch = *reinterpret_cast<int64_t*>(self + 0x278);
    *outChar = ch;
    int64_t* stack = reinterpret_cast<int64_t*>(ch + 0x48);
    if (doCleanup && s_fn_cleanup) s_fn_cleanup(stack);
    return stack;
}

// Build + install the leading SendComponentMessage action into CharacterComponent+0x58
// (msgId = DWORD_1401c70e8, the standard "begin work" message). char = resolved object.
static void InstallLeadMessage(void* param_1, int64_t ch) {
    void* mem = MemA(0x40);
    void* act = nullptr;
    if (mem) {
        int64_t destIBP = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
        uint32_t msgId = MsgId(s_pMsgId_70e8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
        uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
        act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 0, &destIBP, 0) : mem;
    }
    int64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
    if (*reinterpret_cast<int64_t*>(ch + 0x58) != 0) {
        if (fn_InternalBasePtr_DeletePtr) fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(ch + 0x58));
    }
    *reinterpret_cast<int64_t*>(ch + 0x58) = ibp;
}

// Build + push the trailing MoveTo action toward `dest`, mask = the pathfinding flags.
static void PushMoveTo(void* param_1, int64_t* stack, uint32_t pathType, float* dest, int mask) {
    void* mem = MemA(0x480);
    void* act = nullptr;
    if (mem) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
        int64_t entityVal = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entityVal);
        int64_t fam = 0;
        if (s_fn_findFamily) s_fn_findFamily(static_cast<uintptr_t>(entityVal), &fam);
        float speed = Fc(s_pf_172280, -1.0f);
        act = s_fn_moveTo ? s_fn_moveTo(mem, &fam, pathType, 0, dest, mask, 0, speed) : mem;
    }
    int64_t ibp = 0;
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
    if (s_fn_push) s_fn_push(stack, &ibp);
}

// Locate the storage-limit component (GetTypeId == DWORD_1401c7cb8) on the entity-list
// object `listObj` (ptr +0x28, count +0x30, stride 8). Returns the matching component or
// null. Used by Ctor5/Ctor10 (the original's pIVar6 scan).
static void* FindLimitComponent(int64_t listObj) {
    void* found = nullptr;
    uint32_t want = s_pTid_7cb8 ? *s_pTid_7cb8 : 0;
    int n = *reinterpret_cast<int*>(listObj + 0x30);
    if (0 < n) {
        int idx = 0; int64_t byteOff = 0;
        for (;;) {
            int64_t arr = *reinterpret_cast<int64_t*>(listObj + 0x28);
            void* child = *reinterpret_cast<void**>(arr + byteOff);
            uint32_t tid = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<int64_t*>(child) + 0x10))(child);
            if (tid == want) {
                int64_t arr2 = *reinterpret_cast<int64_t*>(listObj + 0x28);
                void* match = *reinterpret_cast<void**>(arr2 + static_cast<int64_t>(idx) * 8);
                if (s_pTid_7cb8 && fn_IsDerivedFrom && fn_IsDerivedFrom(match, *s_pTid_7cb8))
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
    EnsureInit();
    int64_t ch = 0;
    // Ctor4 gates BOTH the stack cleanup and the lead block behind param_6 (disasm:
    // `CMP byte[RBP+0x68],DIL; JZ` skips cleanup + lead to the cell-flags math).
    int64_t* stack = PrologueResolveStack(param_1, &ch, /*doCleanup=*/false);

    if (static_cast<char>(param_6) != '\0') {
        if (s_fn_cleanup) s_fn_cleanup(stack);
        // lead SendComponentMessage -> CharacterComponent+0x58
        InstallLeadMessage(param_1, ch);
        // DropOffRaw action (also stored at +0x58, replacing the message) — faithful: the
        // original re-derives local_68 and OVERWRITES char+0x58 with the drop-raw IBP.
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);  // *(undefined8*)param_1
            act = s_fn_dropOffRaw ? s_fn_dropOffRaw(mem, &entityIBP, reinterpret_cast<uint64_t*>(&destIBP),
                                                    0, 0xffffffff, param_5) : mem;
        }
        int64_t ibp = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }

    // work mask bit from the cell flags byte
    uint8_t bit = *reinterpret_cast<uint8_t*>(
        *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8) + 0x80)
        + 0x29 + static_cast<int64_t>(param_4) * 0x30);

    int64_t target = *param_2;
    if (target == 0) return;

    uint32_t animIdx = 0;
    {
        int64_t targetIBP = target;
        float f = s_fn_entity29a0 ? s_fn_entity29a0(param_3, &targetIBP) : 0.0f;
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    // PerformWork (type 7) @0.75s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_172124, 0.75f));
            InitPerformWork(mem, entityVal, animIdx, 7, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // ShowTool (equip)
    {
        void* mem = MemA(0x30);
        void* act = nullptr;
        if (mem) {
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = s_fn_showTool ? s_fn_showTool(mem, &entityIBP, kShowToolAxeArg) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // DropOffRaw
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t targetIBP = *reinterpret_cast<uint64_t*>(param_2);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = s_fn_dropOffRaw ? s_fn_dropOffRaw(mem, &entityIBP, &targetIBP, 0, 0xffffffff, param_5) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 6) @0.75s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_172124, 0.75f));
            InitPerformWork(mem, entityVal, animIdx, 6, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // MoveTo (Faster=4, disasm `LEA R8D,[R9+0x4]`) with mask = (1<<bit)|0xe1f
    PushMoveTo(param_1, stack, 4 /*Faster*/, param_3, (1 << (bit & 0x1f)) | 0xe1f);
    // trailing ShowTool (unequip)
    {
        void* mem = MemA(0x30);
        void* act = nullptr;
        if (mem) {
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);
            act = s_fn_showTool ? s_fn_showTool(mem, &entityIBP, kShowToolAxeArg) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdx;
    {
        uint64_t srcIBP = *param_2;
        float f = s_fn_entity29a0 ? s_fn_entity29a0(param_3, &srcIBP) : 0.0f;
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    void* limit = FindLimitComponent(param_4);

    // SendComponentMessage(7158, a4=1, a6 = limit->+0x54)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            uint32_t a6 = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(limit) + 0x54);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 1, &destIBP, a6) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // ConsumeRaw(resType list = *param_2, list = param_4, flag a4=1?  see disasm: param_4(arg) is `param_4=1` byte)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t resType = *param_2;  // local_res10 = *local_res10
            act = s_fn_consumeRaw ? s_fn_consumeRaw(mem, &resType, param_4, 1, 1) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 8) @1.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_17214c, 1.0f));
            InitPerformWork(mem, entityVal, animIdx, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdx;
    {
        int64_t targetIBP = *param_2;
        float f = s_fn_entity29a0 ? s_fn_entity29a0(param_3, &targetIBP) : 0.0f;
        animIdx = *reinterpret_cast<uint32_t*>(&f);
    }

    // helper to push a SendComponentMessage(7158, a4) toward *param_2
    auto pushMsg7158 = [&](uint32_t a4) {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t destIBP = *reinterpret_cast<uint64_t*>(param_2);  // local_res8 = *param_2
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, a4, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    };

    if (param_4 != '\0') pushMsg7158(0);
    pushMsg7158(2);

    // PerformWork (type 8) @2.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_172174, 2.0f));
            InitPerformWork(mem, entityVal, animIdx, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    // SendComponentMessage(7158, a4=0) toward *param_2
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t destIBP = *param_2;
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 0, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 8) animIdx = 0xc47a0000 literal, time 4.0
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_172194, 4.0f));
            InitPerformWork(mem, entityVal, 0xc47a0000u, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdx;
    {
        uint64_t srcIBP = *param_2;
        float f = s_fn_entity29a0 ? s_fn_entity29a0(param_3, &srcIBP) : 0.0f;
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
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, a4, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // ConsumeRaw(resType=*param_2, list=param_4, flag=a4)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t resType = *param_2;
            act = s_fn_consumeRaw ? s_fn_consumeRaw(mem, &resType, static_cast<int64_t>(param_4), a4, 1) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 8) @1.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_17214c, 1.0f));
            InitPerformWork(mem, entityVal, animIdx, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    uint32_t animIdxA, animIdxB;
    {
        int64_t srcIBP = *param_3;
        float f = s_fn_entity29a0 ? s_fn_entity29a0(param_4, &srcIBP) : 0.0f;
        animIdxA = *reinterpret_cast<uint32_t*>(&f);
    }
    {
        int64_t dstIBP = *param_5;
        float f = s_fn_entity29a0 ? s_fn_entity29a0(param_6, &dstIBP) : 0.0f;
        animIdxB = *reinterpret_cast<uint32_t*>(&f);
    }

    // SendComponentMessage(7158, a4=3)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 3, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // ConsumeRaw(entity=*param_1.entity, list=param_2, flag=3, reserve=0)
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_consumeRaw ? s_fn_consumeRaw(mem, &entityIBP, param_2, 3, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 8) animIdxB @5.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_172198, 5.0f));
            InitPerformWork(mem, entityVal, animIdxB, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
            act = s_fn_pickupStored ? s_fn_pickupStored(mem, &entityIBP, &srcIBP, param_2, 1, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 8) animIdxA @1.0s
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_17214c, 1.0f));
            InitPerformWork(mem, entityVal, animIdxA, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    // PerformWork (type 8) animIdx 0xc47a0000, time 5.0
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, Fc(s_pf_172198, 5.0f));
            InitPerformWork(mem, entityVal, 0xc47a0000u, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // SendComponentMessage(7158, a4=0) toward *param_2
    {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            uint64_t destIBP = *reinterpret_cast<uint64_t*>(param_2);
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 0, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    {
        void* act = nullptr;
        if (static_cast<char>(param_6) == '\0') {
            void* mem = MemA(0x38);
            if (mem) {
                int64_t destIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
                uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_1);  // local_48 = *param_1
                act = s_fn_completeTask ? s_fn_completeTask(mem, param_2, &entityIBP,
                                                            reinterpret_cast<uint64_t*>(&destIBP), 0) : mem;
            }
        } else {
            void* mem = MemA(0x40);
            if (mem) {
                int64_t destIBP = 0;
                if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
                uint32_t msgId = MsgId(s_pMsgId_7158);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
                uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
                act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, param_4, &destIBP, 0) : mem;
            }
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // PerformWork (type 8) animIdx 0xc47a0000, time = param_5
    {
        void* mem = MemA(0x48);
        if (mem) {
            uint64_t entityVal = *reinterpret_cast<uint64_t*>(param_1);
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, param_5);
            InitPerformWork(mem, entityVal, 0xc47a0000u, 8, t);
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
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
    EnsureInit();
    int64_t ch = 0;
    int64_t* stack = PrologueResolveStack(param_1, &ch);
    InstallLeadMessage(param_1, ch);

    // optional SendComponentMessage(7158, a4=8)
    if (static_cast<char>(param_5) != '\0') {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
            uint32_t msgId = MsgId(s_pMsgId_7158);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 8, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // optional SendComponentMessage(DAT_1401c42c0, dest=*param_2)
    if (*param_2 != 0) {
        void* mem = MemA(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t destIBP = 0;
            if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&destIBP, nullptr);
            uint32_t msgId = MsgId(s_pMsgId_42c0);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(param_2);  // local_res10 = *param_2
            act = s_fn_sendMsg ? s_fn_sendMsg(mem, &entityIBP, &msgId, 0, &destIBP, 0) : mem;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, act);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    // raw inline PlayAnimation action (0x40 bytes)
    {
        void* mem = MemA(0x40);
        if (mem) {
            uint64_t t = 0; if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&t, param_4);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1);
            uint64_t entityIBP = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            uint8_t* a = reinterpret_cast<uint8_t*>(mem);
            if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(mem);
            *reinterpret_cast<uintptr_t*>(a)       = s_vt_Action;
            *reinterpret_cast<uint64_t*>(a + 0x10) = 0;
            a[0x18]                                = 0;
            *reinterpret_cast<uintptr_t*>(a)       = s_vt_PlayAnimation;
            *reinterpret_cast<uint64_t*>(a + 0x20) = entityIBP;
            *reinterpret_cast<uint32_t*>(a + 0x28) = 0;
            *reinterpret_cast<uint64_t*>(a + 0x30) = t;
            *reinterpret_cast<uint64_t*>(a + 0x38) = 0;
        }
        int64_t ibp = 0; if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&ibp, mem);
        if (s_fn_push) s_fn_push(stack, &ibp);
    }
    PushMoveTo(param_1, stack, 1 /*Slow*/, reinterpret_cast<float*>(param_3), 0xe1f);
}

#endif // STANDALONE_EXE
