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
// Ported here + registered by standalone/reg_citizen_25.cpp (openfuncs step 2.5c):
//   0x14010fde0  CitizenComp_helper_fde0 (msg 1: happiness credit)
//   0x1401117a0  CitizenComp_helper_17a0 (msg 4: attendance/schedule budget)
//   0x140111540  CitizenComp_helper_1540 (msg 5: countdown decay)
//   The dispatcher resolves each via ResolveAddr (now non-NULL) and calls it like
//   every other msg; the msg-1 inline duplicate was retired in favour of the port.
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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/CitizenComponent.h"
#include "Component/exe_bodies.h"
#include "Game/exe_constants.h"

// A .rdata constant stored as its bit pattern (exe_constants.h), read as the float.
static inline float RdataF(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

// ============================================================
// CitizenComp_helper_fde0 (0x14010fde0) — msg 1: happiness credit.
// Refresh the citizen's StatusIconComponent (+0x280, mode 4 = happiness icon) then
// accumulate the credit: +0xac is the running happiness total, +0xa8 the last delta.
// (Retail derefs +0x280 unconditionally; the != 0 guard is a standalone-only safety
// for a citizen whose StatusIconComponent is not yet live — a no-op in practice.)
// ============================================================
void CitizenComp_helper_fde0(int64_t selfV, int param) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfV);
    if (*reinterpret_cast<void**>(p + 0x280)) {
        CitizenComp_FUN_14012c3f0(*reinterpret_cast<int64_t**>(p + 0x280), 4, 0);
    }
    *reinterpret_cast<int32_t*>(p + 0xac) += param;
    *reinterpret_cast<int32_t*>(p + 0xa8)  = param;
}

// ============================================================
// CitizenComp_helper_17a0 (0x1401117a0) — msg 4: attendance/schedule budget check.
// +0x210 is a tick counter, bumped every call. If it is still under the descriptor's
// threshold (desc+0x118) AND the elapsed embedded Time (+0x218, AsSeconds) is still
// within budget (desc+0x11c × 3600s/year), do nothing. On overrun: reset the counter
// to the threshold, set flag +0x88 |= 0x400, release the partner slot (no reroll),
// and post life-event 9 via PostLifeEvent. (Retail derefs +0x288 unguarded.)
// ============================================================
void CitizenComp_helper_17a0(int64_t selfV) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfV);
    int32_t* counter = reinterpret_cast<int32_t*>(p + 0x210);
    *counter += 1;
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    int32_t threshold  = desc ? *reinterpret_cast<int32_t*>(desc + 0x118) : 0;
    float   perUnitSec = desc ? *reinterpret_cast<float*>(desc + 0x11c)   : 0.0f;
    float budgetSecs = perUnitSec * RdataF(kRdata_14017225c);   // 3600.0f
    float elapsed = reinterpret_cast<const System::Time*>(p + 0x218)->AsSeconds();   // B.3.4b bridge
    if (*counter < threshold && elapsed <= budgetSecs) return;

    *counter = threshold;
    *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x400;
    AttendanceComponent_ReleasePartnerSlot(reinterpret_cast<void*>(selfV), 0);
    CitizenComponent_PostLifeEvent(reinterpret_cast<void*>(selfV), 9, 0, nullptr);
}

// ============================================================
// CitizenComp_helper_1540 (0x140111540) — msg 5: countdown decay.
// Gated on +0x68 (an active link/target). When set, build a Time of
// desc+0x12c × 300s and subtract its raw 8-byte tick value from the +0x70
// accumulator (same "Time as raw uint64" representation the msg-6/7 tail uses).
// ============================================================
void CitizenComp_helper_1540(int64_t selfV) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfV);
    if (*reinterpret_cast<int64_t*>(p + 0x68) == 0) return;
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);   // retail: unguarded
    float base = desc ? *reinterpret_cast<float*>(desc + 0x12c) : 0.0f;
    uint64_t t = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), base * kRdata_140172234);
    *reinterpret_cast<int64_t*>(p + 0x70) -= static_cast<int64_t>(t);
}

// ============================================================
// CitizenComponent_helper_21e0 (0x1401121e0) — msg 0 (warm house).
// If the target entity (via *param_2) has a ResidenceComponent (finder
// 0x140008090) whose storage passes FUN_14012ac30 (warm-house check), zero the
// citizen's thirst/warmth need at +0x48. Faithful: the finder is called TWICE
// (retail re-runs it after the first null-check), each preceded by CheckPtr.
// ============================================================
void CitizenComp_helper_21e0(int64_t selfV, void* param_2) {
    if (*reinterpret_cast<int64_t*>(param_2) == 0) return;

    int64_t out1 = 0;
    void* r1 = CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(param_2), &out1);
    if (*reinterpret_cast<int64_t*>(r1) == 0) return;

    int64_t out2 = 0;
    void* r2 = CitizenComp_Entity_FindComponent_Storage(*reinterpret_cast<int64_t*>(param_2), &out2);
    if (ResidenceCheck_2ac30(*reinterpret_cast<int64_t*>(r2))) {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(selfV) + 0x48) = 0;
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

// [15] 0x140112260 — the component message handler SendComponentMessage
// (0x1400caef0) dispatches: EDX = message id, R8 = &target entity IBP, R9D =
// extra (read from the retail body's jump table).
void CitizenComponent::OnMessage(int message, void* data, uint32_t arg0, uint32_t /*arg1*/) {
    uint64_t* param_3 = static_cast<uint64_t*>(data);
    int param_4 = static_cast<int>(arg0);
    if (static_cast<unsigned>(message) > 8) return;   // retail: CMP EDX,8 / JA ret
    uint8_t* p = reinterpret_cast<uint8_t*>(this);

    switch (message) {
    case 0: {   // warm-house: zero thirst if target residence passes 2ac30
        uint64_t local = *param_3;
        CitizenComp_helper_21e0(reinterpret_cast<int64_t>(this), &local);
        return;
    }
    case 1: {   // happiness credit — helper_fde0 (0x14010fde0), now ported + registered
        CitizenComp_helper_fde0(reinterpret_cast<int64_t>(this), param_4);
        return;
    }
    case 2: {   // EAT — drain hunger against the target's storage
        uint64_t local = *param_3;
        CitizenComp_EatFromStorage(reinterpret_cast<int64_t>(this), &local);
        return;
    }
    case 3: {   // +0x54 |= 0x200 ; +0x50 += DAT_140172114 (0.5f)
        *reinterpret_cast<uint32_t*>(p + 0x54) |= 0x200;
        *reinterpret_cast<float*>(p + 0x50) += kRdata_140172114;   // 0.5f
        return;
    }
    case 4: {   // helper_17a0(self)
        CitizenComp_helper_17a0(reinterpret_cast<int64_t>(this));
        return;
    }
    case 5: {   // helper_1540(self)
        CitizenComp_helper_1540(reinterpret_cast<int64_t>(this));
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

    // Shared tail for message 6/7:
    //   Time t(desc(+0x288)->+0x130 * DAT_140172234 [300.0f]); +0x88 |= 0x800;
    //   *(Time*)(+0x1e8) = t.
    // Retail derefs +0x288 unguarded; guarded here only against a not-yet-BodyInit'd
    // standalone component (base contributes 0 seconds — flags/store still happen,
    // matching the retail side effects).
    uint64_t t = 0;
    int64_t desc = *reinterpret_cast<int64_t*>(p + 0x288);
    float base = desc ? *reinterpret_cast<float*>(desc + 0x130) : 0.0f;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&t), base * kRdata_140172234);
    *reinterpret_cast<uint32_t*>(p + 0x88) |= 0x800;
    *reinterpret_cast<uint64_t*>(p + 0x1e8) = t;
}
