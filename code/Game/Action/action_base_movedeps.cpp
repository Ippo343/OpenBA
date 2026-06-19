// code/Game/Action/action_base_movedeps.cpp
// Wave 3 (Action) — closure-safe code sub-callees of the MoveTo "unsafe" helpers.
//
// These two exe helpers are the *only* members of the move-helper sub-callee set whose
// own transitive closure is fully satisfiable in standalone (engine_funcs export-resolved
// pointers + already-registered helpers + data globals). Registering them unblocks wiring
// the two closure-safe members of the deferred move-unsafe-6 set:
//   - 0x1401250f0 (this file)  ->  unblocks 0x14010a1f0 SetAnimation
//   - 0x1400a9290 (this file)  ->  unblocks 0x140116650 CitizenBreakCheck (and is also a
//                                  callee of 0x14010a1f0's helper, indirectly)
//
// The remaining move sub-callees (0x14009c480 ConnRefresh, 0x1400a68b0 / 0x1400a6a30 /
// 0x1400a6b20 / 0x1400a6c10 terrain-cost queue ops) are NOT registered here: they are
// commands to a background pathfinding worker Thread — they call System::Thread::Start and
// depend on FUN_1400a6430 -> FUN_14001ce00 (unregistered path-builder) and 9c480 ->
// FUN_14009c170 -> FUN_14001a9b0 (the deferred ZoneRefresh region flood-fill). Wiring them
// would either start an unwired background thread or build half-populated path records —
// strictly worse than the safe no-op of leaving them skipped. They stay deferred (and so do
// e550 / 9cfb0 / c8c10 that depend on them).
//
// Each function below is a 1:1 port of the decompiled exe helper (Ghidra), ABI verified
// against the disassembly. Do NOT "improve" behavior.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// FUN_1400a9290 — task-due / break probability check.
// ============================================================
// bool fn(Time* time, float* pair, Random* rng)
//   pair[1] (the weight) must be > 0, else clears *time and returns false.
//   Otherwise: denom = pair[0] * DAT_1401721b8 * DAT_140172234 * pair[1];
//   roll = AsSeconds(time) / denom; clears *time; returns RandFloat(rng) < roll.
// Verified ABI (disasm 0x1400a9290): RCX=time, RDX=pair, R8=rng; AsSeconds slot 0x140151318
// (== fn_Time_AsSeconds export), RandFloat slot 0x1401512f8 (== fn_Random_RandFloat export);
// constants DAT_1401721b8 / DAT_140172234. The original reads BOTH pair[0] and pair[1], so
// the caller must hand it 8 bytes (two floats), not one.
namespace {
static const float* s_pK1 = nullptr;  // DAT_1401721b8
static const float* s_pK2 = nullptr;  // DAT_140172234
static bool s_a9290Init = false;
static void Ensure_a9290() {
    if (s_a9290Init) return;
    s_pK1 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721b8));
    s_pK2 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172234));
    s_a9290Init = true;
}
} // namespace

bool ActMvDep_TimeCheck_a9290(void* timeV, float* pair, void* rng) {
    Ensure_a9290();
    float weight = pair[1];
    if (!(0.0f < weight)) {                       // weight <= 0 (COMISS 0 < weight)
        *reinterpret_cast<uint64_t*>(timeV) = 0;
        return false;
    }
    float k1 = s_pK1 ? *s_pK1 : 1.0f;
    float k2 = s_pK2 ? *s_pK2 : 1.0f;
    float denom = pair[0] * k1 * k2;
    float secs = fn_Time_AsSeconds ? fn_Time_AsSeconds(timeV) : 0.0f;
    *reinterpret_cast<uint64_t*>(timeV) = 0;
    float roll = fn_Random_RandFloat ? fn_Random_RandFloat(rng) : 0.0f;
    return roll < secs / (denom * weight);
}

// ============================================================
// AnimalComponent_helper_50f0 (0x1401250f0) — start a mesh animation.
// ============================================================
// void fn(animObj* self, uint32_t animId, float p3, float p4, float p5)
//   obj = *(self+0x18); if IsDerivedFrom(obj, FenceDescription type) keep it, else null.
//   Time tB(p3), tA(p4).  animData = *(*(obj+0x20) + (short)self[0x164]*0x18).
//   MeshInstance(self+0x28)::PlayAnimation(animData, animId, tB, tA, p5).
//   self[0x20] |= 4; if the low-16 bits changed and self+0x10 != 0, CheckPtr(self+0x10)
//   then NotifyEntityChanged (FUN_1400c3470, already registered).
// Verified ABI (disasm 0x1401250f0): RCX=self, EDX=animId, XMM2=p3, XMM3=p4, [stack]=p5;
// IsDerivedFrom slot 0x140152a98, Time::Time(float) slot 0x140152b00, PlayAnimation slot
// 0x140152988 (?PlayAnimation@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup@
// Graphics@@@Object@@HAEBVTime@System@@1M@Z), CheckPtr slot 0x140152ab0, FUN_1400c3470.
// Time is passed by const-ref (pointer); ExternalPtr<AnimationGroup> animData is a single
// pointer-sized value passed by value.
namespace {
typedef void (*fn_TimeCtor_t)(void* retTime, float seconds);                  // 0x140152b00
typedef void (*fn_PlayAnimation_t)(void* meshInst, void* animData, int animId,
                                   const void* tB, const void* tA, float p5);  // 0x140152988
typedef void (*fn_Notify_c3470_t)(void* internal);                            // 0x1400c3470

static fn_TimeCtor_t      s_TimeCtor      = nullptr;
static fn_PlayAnimation_t s_PlayAnimation = nullptr;
static fn_Notify_c3470_t  s_Notify_c3470  = nullptr;
static bool s_50f0Init = false;
static void Ensure_50f0() {
    if (s_50f0Init) return;
    s_TimeCtor      = reinterpret_cast<fn_TimeCtor_t>(ResolveExport("??0Time@System@@QEAA@M@Z"));
    s_PlayAnimation = reinterpret_cast<fn_PlayAnimation_t>(ResolveExport(
        "?PlayAnimation@MeshInstance@Graphics@@QEAAXV?$ExternalPtr@VAnimationGroup@Graphics@@@Object@@HAEBVTime@System@@1M@Z"));
    s_Notify_c3470  = reinterpret_cast<fn_Notify_c3470_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_50f0Init = true;
}
} // namespace

void ActMvDep_AnimHelper_50f0(uintptr_t self, uint32_t animId, float p3, float p4, float p5) {
    Ensure_50f0();

    void* obj = *reinterpret_cast<void**>(self + 0x18);
    uint32_t typeId = g_TypeId_FenceDescription ? *g_TypeId_FenceDescription : 0;
    bool derived = (fn_IsDerivedFrom && typeId) ? fn_IsDerivedFrom(obj, typeId) : false;
    void* animHolder = derived ? obj : nullptr;

    uint8_t timeA[8] = {0};  // p4
    uint8_t timeB[8] = {0};  // p3
    if (s_TimeCtor) { s_TimeCtor(timeA, p4); s_TimeCtor(timeB, p3); }

    // Defensive: the original reads *(animHolder+0x20) unconditionally (it relies on the
    // type always being derived here). Guard so a type miss in the inert standalone world
    // is a safe no-op rather than a null deref.
    if (animHolder && s_PlayAnimation) {
        short animIndex = *reinterpret_cast<short*>(self + 0x164);
        uintptr_t animArray = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(animHolder) + 0x20);
        void* animData = *reinterpret_cast<void**>(animArray + static_cast<uintptr_t>(animIndex) * 0x18);
        // PlayAnimation(meshInst=self+0x28, animData, animId, &tB=p3, &tA=p4, p5)
        s_PlayAnimation(reinterpret_cast<void*>(self + 0x28), animData, static_cast<int>(animId),
                        timeB, timeA, p5);
    }

    uint32_t old = *reinterpret_cast<uint32_t*>(self + 0x20);
    uint32_t nw = old | 4u;
    *reinterpret_cast<uint32_t*>(self + 0x20) = nw;
    if (((nw ^ old) & 0xffff) != 0 && *reinterpret_cast<uintptr_t*>(self + 0x10) != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(self + 0x10));
        if (s_Notify_c3470) s_Notify_c3470(*reinterpret_cast<void**>(self + 0x10));
    }
}
