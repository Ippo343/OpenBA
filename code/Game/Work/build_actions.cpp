// code/Game/Work/build_actions.cpp
// Faithful 1:1 port of FUN_1400c0690 — BuildStructure::SetupBuildActions: builds the
// citizen build-labor action chain and pushes it onto the CharacterComponent action
// stack. Called from BuildStructure::Execute (buildstructure.cpp, via the engine_funcs
// global fn_SetupBuildActions == ResolveAddr(Exe, 0x1400c0690)). Unported/NULL in
// standalone was the EXEC-of-0 crash blocking organic build labor.
//
// Signature (matches buildstructure.cpp's call = fn_SetupBuildActions(&citizenEntityVal,
// self->field_20, (void*)(bs+0x10), bestPos, chosenIndex, (uint8_t)self->field_48)):
//   param_1 = citizen entity InternalBasePtr* (the builder)
//   param_2 = priority / task index (CompleteTask +0x30)
//   param_3 = building entity InternalBasePtr* (the construction site = BuildStructure+0x10)
//   param_4 = target world position (float[3]) — MoveTo destination + heading source
//   param_5 = task param (CompleteTask +0x34)
//   param_6 = altAnim byte — selects the state-8 SendComponentMessage id + tool/anim variant
//
// The chain (order of PUSH via TaskListAppend 0x1400c54e0; the stack is LIFO so it runs
// bottom-to-top): a LEAD SendComponentMessage (target type-id 70e8 = CitizenAIComponent) is stored at CharacterComponent
// +0x58 (NOT pushed — it fires when the chain completes), then pushed in order:
// CompleteTask -> SendComponentMessage(c7008 idle / c75f0 alt) -> ShowTool(-1) ->
// PerformWork(3.0s, heading anim) -> MoveTo(Fast) -> ShowTool(Knife 7 / Saw 4). When the
// timed PerformWork fires, the lead message posts back to the BuildComponent and credits
// build progress (BuildComponent+0x48).
//
// Offset-exact against the 0x1400c0690 disassembly (Ghidra): float consts
// DAT_140172180=3.0f (PerformWork time), DAT_140172280=-1.0f (MoveTo speed sentinel =
// "randomize default"); MoveTo pathType=2 (Fast); ShowTool#1 tool=-1; ShowTool#2 tool=
// uVar2?4(Saw):7(Knife); PerformWork actionType=uVar2?0xd:0x10, p6=1, p7=-1, animIndex=
// Entity_impl_29a0(pos,&buildingEntity) heading; uVar2=Random::Rand(*(gw+0xd8),2). The
// msg-id .rdata globals (c70e8/c7008/c75f0) are 0 in the static image; in standalone the
// BuildComponent type-id (c7008) is healed via GetTypeIdByName so the state-8 message
// targets the BuildComponent (the others carry 0, matching citizenai_actionctors).
//
// fn_SetupBuildActions corresponds to the exe helper at c0690.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers

namespace {

// Already-ported action ctors + stack ops, called via ResolveAddr (all registered:
// reg_pf_ctors.cpp for the ctors, reg_action_* for 012d0). Sigs verified vs their reimpls.
typedef void  (*fn_wipe_t)(void* stack);                                   // 0x1400c5400 TaskListCleanup
typedef void  (*fn_push_t)(void* stack, void* entryIBP);                   // 0x1400c54e0 TaskListAppend
typedef float (*fn_heading_t)(float* worldPos, void* innerIBP);            // 0x1400c29a0 Entity_impl_29a0
typedef void* (*fn_family_t)(uintptr_t entityInner, void* retIBP);         // 0x1400012d0 Entity_FindComponent_Family
typedef void* (*fn_sendmsg_t)(void* self, void* entityInner, uint32_t* msgId,
                              uint32_t p4, void* outIBP, uint32_t p6);      // 0x1400cb1d0
typedef void* (*fn_complete_t)(void* self, uint32_t taskIndex, uint64_t* entityIBP,
                               uint64_t* outIBP, uint32_t taskParam);       // 0x1400c5da0
typedef void* (*fn_showtool_t)(void* self, void* entityIBP, int toolType);  // 0x140042890
typedef void* (*fn_perform_t)(void* self, void* entityIBP, float animIndex, uint32_t actionType,
                              int64_t time, uint8_t p6, int p7);            // 0x1400c9720
typedef void* (*fn_moveto_t)(void* self, void* familyComp, uint32_t pathType, int p4,
                             float* dest, int p6, int p7, float speed);     // 0x1400c9170

fn_wipe_t     s_wipe     = nullptr;
fn_push_t     s_push     = nullptr;
fn_heading_t  s_heading  = nullptr;
fn_family_t   s_family   = nullptr;
fn_sendmsg_t  s_sendmsg  = nullptr;
fn_complete_t s_complete = nullptr;
fn_showtool_t s_showtool = nullptr;
fn_perform_t  s_perform  = nullptr;
fn_moveto_t   s_moveto   = nullptr;
uint32_t s_msg70e8 = 0, s_msg7008 = 0, s_msg75f0 = 0;
bool s_init = false;

void EnsureInit() {
    if (s_init) return;
    s_wipe     = reinterpret_cast<fn_wipe_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_push     = reinterpret_cast<fn_push_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_heading  = reinterpret_cast<fn_heading_t>(ResolveAddr(Module::Exe, 0x1400c29a0));
    s_family   = reinterpret_cast<fn_family_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_sendmsg  = reinterpret_cast<fn_sendmsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_complete = reinterpret_cast<fn_complete_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_showtool = reinterpret_cast<fn_showtool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_perform  = reinterpret_cast<fn_perform_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_moveto   = reinterpret_cast<fn_moveto_t>(ResolveAddr(Module::Exe, 0x1400c9170));
    // msg-id globals: read the .rdata DWORD (0 in the static image / unfilled in SA).
    auto rd = [](uintptr_t a) -> uint32_t {
        uintptr_t p = ResolveAddr(Module::Exe, a);
        return p ? *reinterpret_cast<uint32_t*>(p) : 0;
    };
    s_msg70e8 = rd(0x1401c70e8);
    s_msg7008 = rd(0x1401c7008);
    s_msg75f0 = rd(0x1401c75f0);
    // c7008 = BuildComponent type-id (the state-8 message target); heal it so the message
    // routes to the site's BuildComponent (the raw .rdata id is 0 in standalone).
    if (s_msg7008 == 0) s_msg7008 = ::Object::GetTypeIdByName("BuildComponent");
    // c75f0 = DestroyComponent type-id (Ghidra: written by DestroyComponent_RegisterType
    // 0x14014ee75) — the altAnim/demolition variant of the same state-8 message. Without
    // this heal the destroy-mode SendComponentMessage carries id 0, matches no component,
    // and DestroyComponent+0x54 demolition progress never advances.
    if (s_msg75f0 == 0) s_msg75f0 = ::Object::GetTypeIdByName("DestroyComponent");
    s_init = true;
}


inline void  Ctor(void* self, void* obj) { if (fn_InternalBasePtr_Ctor) new (self) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge
inline void* Alloc(uint64_t sz)          { return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, sz, 8) : nullptr; }

} // namespace

extern "C" void BuildStructure_SetupBuildActions(void* citizenIBPptr, uint32_t priority,
                    int64_t* buildingIBPptr, float* position, uint32_t taskParam, uint32_t altAnim) {
    EnsureInit();
    if (!citizenIBPptr) return;

    uintptr_t citizenEntity = *reinterpret_cast<uintptr_t*>(citizenIBPptr);
    if (!citizenEntity) return;
    uintptr_t charComp = *reinterpret_cast<uintptr_t*>(citizenEntity + 0x278);
    if (!charComp) return;
    void* stack = reinterpret_cast<void*>(charComp + 0x48);   // action stack (base +0x48, top +0x60)

    // heading = Entity_impl_29a0(position, &buildingEntityValue) — used as PerformWork anim.
    int64_t buildingEntity = buildingIBPptr ? *buildingIBPptr : 0;
    float heading = 0.0f;
    if (s_heading) heading = s_heading(position, &buildingEntity);

    // WIPE the action stack (start the new chain fresh).
    if (s_wipe) s_wipe(stack);

    // --- LEAD SendComponentMessage (target type-id 70e8 = CitizenAIComponent) -> stored at CharacterComponent+0x58 (NOT pushed) ---
    {
        void* mem = Alloc(0x40);
        void* act = nullptr;
        if (mem && s_sendmsg) {
            int64_t outIBP = 0; Ctor(&outIBP, nullptr);
            uint32_t msgId = s_msg70e8;
            int64_t targetEntity = *reinterpret_cast<int64_t*>(citizenEntity + 0x10);  // *(citizenEntity+0x10)
            act = s_sendmsg(mem, &targetEntity, &msgId, 0, &outIBP, 0);
        }
        int64_t leadIBP = 0; Ctor(&leadIBP, act);
        if (*reinterpret_cast<int64_t*>(charComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)
            reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(charComp + 0x58))->DeletePtr(
                );   // B.3.3 bridge
        *reinterpret_cast<int64_t*>(charComp + 0x58) = leadIBP;
    }

    // uVar2 = Random::Rand(*(gw+0xd8), 2) — tool/anim variant (0 or 1).
    uint32_t uVar2 = 0;
    {
        uintptr_t gw = g_pGameWorldSingleton ? reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) : 0;
        void* rng = gw ? *reinterpret_cast<void**>(gw + 0xd8) : nullptr;
        if (rng) uVar2 = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(2));   // B.3.4b bridge
    }

    // --- CompleteTask (0x38) -> PUSH ---
    {
        void* mem = Alloc(0x38);
        void* act = nullptr;
        if (mem && s_complete) {
            int64_t outIBP = 0; Ctor(&outIBP, nullptr);
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);   // citizen entity
            act = s_complete(mem, priority, reinterpret_cast<uint64_t*>(&ent),
                             reinterpret_cast<uint64_t*>(&outIBP), taskParam);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        if (s_push) s_push(stack, &ibp);
    }

    // --- SendComponentMessage #2 (msg c7008 idle / c75f0 alt, target=building) -> PUSH ---
    {
        void* mem = Alloc(0x40);
        void* act = nullptr;
        if (mem && s_sendmsg) {
            int64_t outIBP = 0; Ctor(&outIBP, nullptr);
            uint32_t msgId = (altAnim & 0xff) ? s_msg75f0 : s_msg7008;
            int64_t bldg = buildingIBPptr ? *buildingIBPptr : 0;   // building entity
            act = s_sendmsg(mem, &bldg, &msgId, 0, &outIBP, 0);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        if (s_push) s_push(stack, &ibp);
    }

    // --- ShowTool #1 (tool = -1) -> PUSH ---
    {
        void* mem = Alloc(0x30);
        void* act = nullptr;
        if (mem && s_showtool) {
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);
            act = s_showtool(mem, &ent, -1);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        if (s_push) s_push(stack, &ibp);
    }

    // --- PerformWork (0x48, timed 3.0s, heading anim) -> PUSH ---
    {
        void* mem = Alloc(0x48);
        void* act = nullptr;
        if (mem && s_perform) {
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);
            int64_t timeVal[2] = { 0, 0 };
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(timeVal, 3.0f);   // DAT_140172180
            uint32_t actionType = (uVar2 != 0) ? 0xd : 0x10;
            act = s_perform(mem, &ent, heading, actionType, timeVal[0], 1, -1);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        if (s_push) s_push(stack, &ibp);
    }

    // --- MoveTo (0x480, Fast, to family-component pos) -> PUSH ---
    {
        void* mem = Alloc(0x480);
        void* act = nullptr;
        if (mem && s_moveto) {
            int64_t entInner = *reinterpret_cast<int64_t*>(citizenEntity + 0x10);
            int64_t famIBP = 0;
            void* fam = s_family ? s_family(static_cast<uintptr_t>(entInner), &famIBP) : nullptr;
            act = s_moveto(mem, fam, 2 /*pathType Fast*/, 0, position, 0xe1f, 0, -1.0f /*DAT_140172280*/);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        if (s_push) s_push(stack, &ibp);
    }

    // --- ShowTool #2 (tool = uVar2 ? 4(Saw) : 7(Knife)) -> PUSH ---
    {
        void* mem = Alloc(0x30);
        void* act = nullptr;
        if (mem && s_showtool) {
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);
            int toolType = (uVar2 != 0) ? 4 : 7;
            act = s_showtool(mem, &ent, toolType);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        if (s_push) s_push(stack, &ibp);
    }

}
