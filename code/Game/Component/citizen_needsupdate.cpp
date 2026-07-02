// code/Game/Component/citizen_needsupdate.cpp
// Economy Brick M-D (citizen/eat-message half) — CitizenComponent message
// dispatcher (vtable slot +0x78 / index 15) + the msg-0 warm-house helper.
//
// Faithful ports (Ghidra-verified against Application-x64.exe disassembly):
//   0x140112260  CitizenComponent_NeedsUpdate   — the switch(msg) dispatcher the
//                SendComponentMessage action executes via component vtable+0x78.
//   0x1401121e0  CitizenComponent_helper_21e0   — msg 0: if the target entity has
//                a ResidenceComponent (finder 0x140008090, type DAT_1401c7cf0)
//                that passes the warm-house check FUN_14012ac30, zero the
//                citizen's thirst (+0x48).
//
// msg 2 (EAT) delegates to CitizenComp_ReleaseWorkplace (0x1401152e0) in
// citizencomponent.cpp — NOTE the retail Ghidra label "ReleaseWorkplace" is a
// MISNOMER: the function is EatFromStorage (drains hunger +0x44 against the
// target's StorageComponent, one unit per g_fUVScale). A named alias
// CitizenComp_EatFromStorage is provided below.
//
// NOT ported here (skip-with-one-time-trace when unresolved — standalone):
//   0x14010fde0  helper_fde0 (msg 1)
//   0x1401117a0  helper_17a0 (msg 4)
//   0x140111540  helper_1540 (msg 5)
// Already ported/registered elsewhere (NOT duplicated):
//   0x1401152e0  CitizenComp_ReleaseWorkplace   (citizencomponent.cpp, reg_component_people)
//   0x140088f90  GetResourceCategory            (action_base_rawmat.cpp, reg_action_rawmat)
//   0x14012ac30  ResidenceCheck_2ac30           (citizenai_findhelpers.cpp, reg_citizenai_findhelpers)
//   0x140008090  Entity_FindComponent_Residence (citizenaicomponent.cpp, reg_component_people)
//
// Disasm notes (0x140112260):
//   - Signature: RCX=this, EDX=msg (0..8, JA out-of-range returns), R8=ptr to the
//     target-entity IBP VALUE (msg 0/2 copy *R8 to a stack local and pass its
//     address), R9D=int param (only msg 1 consumes it: MOV EDX,R9D).
//   - msg 3: +0x54 |= 0x200; +0x50 += DAT_140172114 (=0.5f in retail .rdata).
//   - msg 6/7: +0x1e0/+0x1e4 = 1/1 (2/2), then the shared tail:
//     Time ctor (dispatch slot [0x140152b00], RCX=&out XMM1=secs) of
//     desc(+0x288)+0x130 * DAT_140172234 (=300.0f), then +0x88 |= 0x800 and the
//     8-byte Time stored at +0x1e8.
//   - msg 8: +0x88 |= 0x40000.
//
// This file must NOT depend on hooks.h — portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// From citizencomponent.cpp (faithful port of 0x1401152e0).
extern void CitizenComp_ReleaseWorkplace(int64_t selfV, void* param2);

// ============================================================
// Local resolutions
// ============================================================
typedef void (*fn_helper_fde0_t)(int64_t self, int param);   // 0x14010fde0
typedef void (*fn_helper_17a0_t)(int64_t self);              // 0x1401117a0
typedef void (*fn_helper_1540_t)(int64_t self);              // 0x140111540
typedef bool (*fn_ResidenceCheck_t)(int64_t storageComp);    // 0x14012ac30
typedef void* (*fn_FindResidence8090_t)(int64_t entity, void* outIBP); // 0x140008090
typedef void (*fn_TimeCtorFloat_t)(void* outTime, float secs); // dispatch slot 0x140152b00

static fn_helper_fde0_t      s_fn_fde0 = nullptr;
static fn_helper_17a0_t      s_fn_17a0 = nullptr;
static fn_helper_1540_t      s_fn_1540 = nullptr;
static fn_ResidenceCheck_t   s_fn_2ac30 = nullptr;
static fn_FindResidence8090_t s_fn_find8090 = nullptr;
static fn_TimeCtorFloat_t    s_fn_TimeCtorF = nullptr;
static const float*          s_pDAT_172114 = nullptr;  // 0.5f  (msg-3 increment)
static const float*          s_pDAT_172234 = nullptr;  // 300.0f (msg-6/7 Time scale)
static bool s_resolved = false;

static void EnsureNeedsUpdateFuncs() {
    if (s_resolved) return;
    s_resolved = true;   // latch on ATTEMPT, not on success (anti-pattern note)
    s_fn_fde0    = reinterpret_cast<fn_helper_fde0_t>(ResolveAddr(Module::Exe, 0x14010fde0));
    s_fn_17a0    = reinterpret_cast<fn_helper_17a0_t>(ResolveAddr(Module::Exe, 0x1401117a0));
    s_fn_1540    = reinterpret_cast<fn_helper_1540_t>(ResolveAddr(Module::Exe, 0x140111540));
    s_fn_2ac30   = reinterpret_cast<fn_ResidenceCheck_t>(ResolveAddr(Module::Exe, 0x14012ac30));
    s_fn_find8090 = reinterpret_cast<fn_FindResidence8090_t>(ResolveAddr(Module::Exe, 0x140008090));
    // .rdata float constants — NULL in standalone unless registered; real-value
    // fallbacks are applied at the use sites (reference_standalone_rdata_null_fallback).
    s_pDAT_172114 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_pDAT_172234 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172234));
    // System::Time::Time(float) — read the fn ptr out of the dispatch slot.
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140152b00);
    if (slot) s_fn_TimeCtorF = *reinterpret_cast<fn_TimeCtorFloat_t*>(slot);
}

// [EatMsg] trace gate (env BANISHED_STORAGE_TRACE — shared with the world-side
// storage brick).
static bool EatMsgTraceOn() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_STORAGE_TRACE");
        s = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s == 1;
}

static void SkipTraceOnce(const char* which) {
    static int seen[3] = {0, 0, 0};
    int idx = (which[0] == 'f') ? 0 : (which[0] == '1' && which[1] == '7') ? 1 : 2;
    if (!seen[idx]) {
        seen[idx] = 1;
        fprintf(stderr, "[EatMsg] NeedsUpdate: helper %s unresolved — msg skipped\n", which);
        fflush(stderr);
    }
}

// ============================================================
// CitizenComponent_helper_21e0 (0x1401121e0) — msg 0 (warm house).
// If the target entity (via *param_2) has a ResidenceComponent (finder
// 0x140008090) whose storage passes FUN_14012ac30 (warm-house check), zero the
// citizen's thirst/warmth need at +0x48. Faithful: the finder is called TWICE
// (retail re-runs it after the first null-check), each preceded by CheckPtr.
// ============================================================
void CitizenComp_helper_21e0(int64_t selfV, void* param_2) {
    EnsureNeedsUpdateFuncs();
    if (*reinterpret_cast<int64_t*>(param_2) == 0) return;
    if (!s_fn_find8090) { SkipTraceOnce("8090"); return; }

    fn_InternalBasePtr_CheckPtr(param_2);
    int64_t out1 = 0;
    void* r1 = s_fn_find8090(*reinterpret_cast<int64_t*>(param_2), &out1);
    if (*reinterpret_cast<int64_t*>(r1) == 0) return;

    fn_InternalBasePtr_CheckPtr(param_2);
    int64_t out2 = 0;
    void* r2 = s_fn_find8090(*reinterpret_cast<int64_t*>(param_2), &out2);
    fn_InternalBasePtr_CheckPtr(r2);
    if (!s_fn_2ac30) { SkipTraceOnce("2ac30"); return; }
    if (s_fn_2ac30(*reinterpret_cast<int64_t*>(r2))) {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(selfV) + 0x48) = 0;
        if (EatMsgTraceOn()) {
            static int n = 0;
            if (n < 8) { ++n;
                fprintf(stderr, "[EatMsg] warm-house: citizen=%p thirst(+0x48) zeroed\n",
                        reinterpret_cast<void*>(selfV));
                fflush(stderr);
            }
        }
    }
}

// ============================================================
// CitizenComp_EatFromStorage — named alias for 0x1401152e0.
// The retail Ghidra label "CitizenComponent::ReleaseWorkplace" is a MISNOMER
// (profile-build source line attribution); the body is the EAT loop: while
// hunger(+0x44) > 0, pick a stocked edible from the target's StorageComponent
// (0x140006270 + Entity_impl_ef40), SubtractItem (FUN_14012d150), bump the town
// per-category consumed counter (townMgr+0x170[cat*0x20+0xC]), OR the material's
// diet bits into +0x54, hunger -= g_fUVScale.
// ============================================================
void CitizenComp_EatFromStorage(int64_t selfV, void* targetIBP) {
    CitizenComp_ReleaseWorkplace(selfV, targetIBP);
}

// ============================================================
// CitizenComponent_NeedsUpdate (0x140112260) — the component message handler
// dispatched by SendComponentMessage::AbstractImpl via vtable+0x78.
//   self, msg, param_3 = ptr to the target-entity IBP value,
//   param_4 = int extra (+0x38 of the action), param_5 = +0x3c (unused in retail).
// ============================================================
void CitizenComponent_NeedsUpdate(void* selfV, int msg, uint64_t* param_3,
                                  int param_4, int param_5) {
    (void)param_5;
    EnsureNeedsUpdateFuncs();
    if (static_cast<unsigned>(msg) > 8) return;   // retail: CMP EDX,8 / JA ret
    uint8_t* p = reinterpret_cast<uint8_t*>(selfV);

    if (EatMsgTraceOn()) {
        static int n = 0;
        if (n < 24) { ++n;
            fprintf(stderr, "[EatMsg] NeedsUpdate: citizen=%p msg=%d target=%p extra=%d\n",
                    selfV, msg, param_3 ? reinterpret_cast<void*>(*param_3) : nullptr,
                    param_4);
            fflush(stderr);
        }
    }

    switch (msg) {
    case 0: {   // warm-house: zero thirst if target residence passes 2ac30
        uint64_t local = *param_3;
        CitizenComp_helper_21e0(reinterpret_cast<int64_t>(selfV), &local);
        return;
    }
    case 1: {   // helper_fde0(self, param_4) — unported, resolve-or-skip
        if (s_fn_fde0) s_fn_fde0(reinterpret_cast<int64_t>(selfV), param_4);
        else SkipTraceOnce("fde0");
        return;
    }
    case 2: {   // EAT — drain hunger against the target's storage
        uint64_t local = *param_3;
        if (EatMsgTraceOn()) {
            static int n = 0;
            if (n < 24) { ++n;
                fprintf(stderr, "[EatMsg] msg2 EAT: citizen=%p hungerBefore=%.3f target=%p\n",
                        selfV, *reinterpret_cast<float*>(p + 0x44),
                        reinterpret_cast<void*>(local));
                fflush(stderr);
            }
        }
        CitizenComp_EatFromStorage(reinterpret_cast<int64_t>(selfV), &local);
        if (EatMsgTraceOn()) {
            static int n = 0;
            if (n < 24) { ++n;
                fprintf(stderr, "[EatMsg] msg2 EAT done: citizen=%p hungerAfter=%.3f\n",
                        selfV, *reinterpret_cast<float*>(p + 0x44));
                fflush(stderr);
            }
        }
        return;
    }
    case 3: {   // +0x54 |= 0x200 ; +0x50 += DAT_140172114 (0.5f)
        *reinterpret_cast<uint32_t*>(p + 0x54) |= 0x200;
        *reinterpret_cast<float*>(p + 0x50) += (s_pDAT_172114 ? *s_pDAT_172114 : 0.5f);
        return;
    }
    case 4: {   // helper_17a0(self) — unported, resolve-or-skip
        if (s_fn_17a0) s_fn_17a0(reinterpret_cast<int64_t>(selfV));
        else SkipTraceOnce("17a0");
        return;
    }
    case 5: {   // helper_1540(self) — unported, resolve-or-skip
        if (s_fn_1540) s_fn_1540(reinterpret_cast<int64_t>(selfV));
        else SkipTraceOnce("1540");
        return;
    }
    case 6:
        *reinterpret_cast<int32_t*>(p + 0x1e0) = 1;
        *reinterpret_cast<int32_t*>(p + 0x1e4) = 1;
        break;      // falls through to the shared Time tail
    case 7:
        *reinterpret_cast<int32_t*>(p + 0x1e0) = 2;
        *reinterpret_cast<int32_t*>(p + 0x1e4) = 2;
        break;      // falls through to the shared Time tail
    case 8:
        *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x40000;
        return;
    default:
        return;
    }

    // Shared tail for msg 6/7:
    //   Time t(desc(+0x288)->+0x130 * DAT_140172234 [300.0f]); +0x88 |= 0x800;
    //   *(Time*)(+0x1e8) = t.
    // Retail derefs +0x288 unguarded; guarded here only against a not-yet-BodyInit'd
    // standalone component (base contributes 0 seconds — flags/store still happen,
    // matching the retail side effects).
    uint64_t t = 0;
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    float base = desc ? *reinterpret_cast<float*>(desc + 0x130) : 0.0f;
    if (s_fn_TimeCtorF)
        s_fn_TimeCtorF(&t, base * (s_pDAT_172234 ? *s_pDAT_172234 : 300.0f));
    *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x800;
    *reinterpret_cast<uint64_t*>(p + 0x1e8) = t;
}
