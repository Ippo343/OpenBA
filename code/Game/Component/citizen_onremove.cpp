// code/Game/Component/citizen_onremove.cpp
// CitizenComponent::OnRemove (0x140113ca0) — the removed-from-world teardown that
// clears EVERY reference the rest of the town holds to a dying citizen.
//
// Why this matters: CitizenComponent_OnAdd (0x140114000) appends the citizen to the
// town manager's citizen list (townMgr+0xa0) and nothing ever removed it again, so
// every death left a permanent dangling pointer in the town's own list. Retail does
// not survive that by luck — Object::InternalBasePtr::CheckPtr is an EMPTY function
// in the shipping Runtime (see Object/internal_factory_audit.h), so retail's whole
// correctness story for removal IS this function clearing the refs explicitly.
// ObjAudit reporting bad=0 does not contradict this: the InternalFactory handle
// table is intact; it is the town's lists that hold the stale pointers.
//
// Ghidra-verified 1:1 against the decompile of 0x140113ca0. Eight steps, in order:
//   1. prologue teardown — hide the carried tool (0x1401176e0), release the partner
//      slot +0x208 (0x140115a50, no re-roll) and the two queued-worker link slots
//      +0x228 / +0x230 (0x1401159b0 / 0x140111650)
//   2. AssignWorkplace(self, null, -1)         — workplace link +0x238 / slot +0x240
//   3. LeaveRoster_2040(self->+0x258, &self)   — unlink from the host's roster ring
//   4. family ring +0x250 (spouse) / +0x260 (ring head):
//        no spouse  -> per member: under-age (age +0x3c <= desc(+0x288)+0x44) dies via
//                      HandleLifeEvent(member, 4); otherwise it is emancipated —
//                      sub_1fa0(member, null), AssignWorkplace(null,-1), release its
//                      partner slot
//        spouse     -> re-home the whole ring onto the spouse (sub_1fa0(member,&spouse))
//      then clear the spouse's back-ref and our own +0x250
//   5. AssignHousing(self, null)               — housing link +0x248
//   6. profession list — when +0x1f0 is not the default profession (*(gw+0xb0)+0x30),
//      ask the town for a replacement worker (0x14008c3d0) and post life event 0xf
//      (replacement found, named after the deceased) or 0x10 (nobody available)
//   7. TradeWork_impl_3480(gw+0x120, &ownerEntity)  — trade-work list + open dialogs
//   8. FUN_140002cc0(townMgr+0xa0, &self)      — the citizen-list removal itself
//
// Every callee is already ported AND registered (checked explicitly — "ported but
// unregistered" is how a pause button ended up calling address 0):
//   0x1401176e0 ActCt_HideTool_1176e0                 reg_action_complete.cpp
//   0x140115a50 AttendanceComponent_ReleasePartnerSlot reg_component_gap_A.cpp
//   0x1401159b0 AttendanceComponent_ReleaseLink59b0    reg_component_gap_A.cpp
//   0x140111650 AttendanceComponent_ReleaseLink1650    reg_component_gap_A.cpp
//   0x1401163d0 CitizenComponent_AssignWorkplace       reg_citizen_assignworkplace.cpp
//   0x140112040 CitizenComp_LeaveRoster_2040           reg_citizen_25.cpp
//   0x140111fa0 CitizenComp_sub_1fa0                   reg_component_people.cpp
//   0x140111ee0 CitizenComp_AssignHousing              reg_component_gap_A.cpp
//   0x140116dd0 CitizenComponent_HandleLifeEvent       reg_citizen_lifeevent_handler.cpp
//   0x14008c3d0 CitizenList_AssignWorker_c3d0          reg_townhallui.cpp
//   0x140113af0 CitizenComponent_PostLifeEvent         reg_eventlog_citizen.cpp
//   0x140103480 NomadComp_FUN_140103480                reg_component_people.cpp
//   0x140002cc0 CemeteryListErase_Reimpl               reg_component_visuals.cpp
//
// Dispatch: 0x140113ca0 has no CALL xrefs in the exe — all four are DATA, the
// CitizenComponent vtable slot 7 (0x140162580+0x38) and three sibling vtables. Slot 7
// is the removed-from-world broadcast that MarkForRemoval (0x1400c2ea0) fires over
// every component whose capability flags (+0x20) have bit 1 set; CitizenComponent's
// flags are 0xCB, so the broadcast already reaches us — it has just been landing on
// the inert no-op stub. citizencomponent.cpp installs this into
// CitizenComponent_LiveVtable[7] under BANISHED_LIVE_CITIZEN_ONREMOVE.
//
// TWO STANDALONE DEVIATIONS, both marked DEVIATION below and both there because our
// component bodies come from the inert factory (zero-filled from +0x28) rather than
// from the real ctor, and because an unterminated ring walk is an in-world freeze
// rather than a crash.

#include <new>
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CitizenComponent.h"
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"

namespace {

inline void Ctor(void* dst, void* obj) { new (dst) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge

// The family/roster ring is doubly linked through member +0x268 (next) / +0x270
// (prev) with the head cached at host +0x260 — see CitizenComp_LeaveRoster_2040 /
// CitizenComp_JoinRoster_68a0 (citizencomponent_lifecycle.cpp). Each pass of the
// ring loops below MUST unlink the current head, or the head never changes.
//
// DEVIATION (freeze guard): retail's loop is `while (*(head) != 0)` with no progress
// test. The under-age branch relies on HandleLifeEvent(member, 4) reaching
// MarkForRemoval -> this very function on the member -> LeaveRoster, and
// HandleLifeEvent skips the removal queue entirely when the member's entity is
// already flagged bit 10 (+0x40 & 0x400). A member in that state is never unlinked
// and retail's loop would spin forever — which in this codebase is an in-world
// freeze (rising CPU, no crash log), the single worst failure mode to debug. Break
// out loudly instead; the remaining members keep their (now stale) back-ref, which
// is strictly better than hanging, and the trace names the member so it can be
// diagnosed.
constexpr int kRingWalkLimit = 4096;

bool RingProgress(const char* what, int64_t before, int64_t after, int& iterations) {
    if (after == before) {
        std::fprintf(stderr, "[CitizenOnRemove] ring walk (%s) made no progress at "
                     "member=%p — breaking to avoid a spin\n", what, (void*)(uintptr_t)before);
        std::fflush(stderr);
        return false;
    }
    if (++iterations > kRingWalkLimit) {
        std::fprintf(stderr, "[CitizenOnRemove] ring walk (%s) exceeded %d members — "
                     "breaking (corrupt ring?)\n", what, kRingWalkLimit);
        std::fflush(stderr);
        return false;
    }
    return true;
}

}  // namespace

// ============================================================
// CitizenComponent::OnRemove (0x140113ca0) — vtable slot 7 (+0x38)
// ============================================================

// [7] 0x140113ca0 — the removed-from-world broadcast (MarkForRemoval 0x1400c2ea0,
// flags bit 1): leave the town citizen list.
void CitizenComponent::OnRemove() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);

    // ---- 1. prologue teardown -------------------------------------------------
    // DEVIATION (uninitialized-body guard): HideTool walks +0x278 -> +0x68 -> +0x28
    // and, when the carried-tool scene id at +0x1dc is >= 0, calls
    // Graphics::Scene::RemoveObject with it. The real ctor writes -1 to +0x1d8 (both
    // halves) and populates +0x278; our inert factory zero-fills from +0x28 and only
    // CitizenComponent_BodyInit (the Startup slot) repairs that. A component torn down
    // before Startup ran would therefore null-deref +0x278, and — with sceneId reading
    // as 0 rather than -1 — RemoveObject(scene, 0) would take out whichever object
    // genuinely owns id 0 (the exact in-world freeze fixed in 0f97d16). Skip the call
    // when the body was never populated; nothing of ours is in the scene in that case.
    if (*reinterpret_cast<void**>(p + 0x278) != nullptr)
        ActCt_HideTool_1176e0(this);                            // 0x1401176e0
    AttendanceComponent_ReleasePartnerSlot(this, '\0');         // 0x140115a50, +0x208
    AttendanceComponent_ReleaseLink59b0(this);                  // 0x1401159b0, +0x228
    AttendanceComponent_ReleaseLink1650(this);                  // 0x140111650, +0x230

    // ---- 2. workplace link (+0x238 / +0x240) ----------------------------------
    {
        int64_t nullIBP = 0;
        Ctor(&nullIBP, nullptr);
        CitizenComponent_AssignWorkplace(this, &nullIBP, -1);   // 0x1401163d0
    }

    // ---- 3. leave the host's roster ring (+0x258 back-ref) --------------------
    if (*reinterpret_cast<int64_t*>(p + 0x258) != 0) {
        int64_t selfIBP = 0;
        Ctor(&selfIBP, this);
        CitizenComp_LeaveRoster_2040(*reinterpret_cast<int64_t*>(p + 0x258), &selfIBP);   // 0x140112040
    }

    // ---- 4. family ring (+0x250 spouse / +0x260 ring head) --------------------
    int64_t* spouseSlot = reinterpret_cast<int64_t*>(p + 0x250);
    if (*spouseSlot == 0) {
        // No surviving parent: each ring member either dies with us (still a child)
        // or is emancipated into an unattached adult.
        int64_t* ringHead = reinterpret_cast<int64_t*>(p + 0x260);
        int iterations = 0;
        int64_t member = *ringHead;
        while (member != 0) {
            float memberAge = *reinterpret_cast<float*>(member + 0x3c);
            // DEVIATION (uninitialized-body guard): retail derefs the CitizenDescription
            // at +0x288 unguarded. Ours can still be null before BodyInit; with no
            // description there is no adult-age threshold to compare against, so take
            // the emancipate branch — it clears refs without firing a death chain.
            void* desc = *reinterpret_cast<void**>(p + 0x288);
            bool underAge = desc &&
                memberAge <= *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(desc) + 0x44);

            if (underAge) {
                CitizenComponent_HandleLifeEvent(reinterpret_cast<void*>(*ringHead), 4, nullptr);   // 0x140116dd0
            } else {
                int64_t memberIBP = member;
                int64_t nullIBP = 0;
                Ctor(&nullIBP, nullptr);
                CitizenComp_sub_1fa0(memberIBP, &nullIBP);   // 0x140111fa0: detach from our ring

                int64_t nullWp = 0;
                Ctor(&nullWp, nullptr);
                CitizenComponent_AssignWorkplace(reinterpret_cast<void*>(memberIBP), &nullWp, -1);

                // Retail inlines FUN_140115a50's body (reroll = 0) here verbatim:
                // detach the member from its partner's list, clear the partner's
                // status-icon flag 2, null the +0x208 slot.
                AttendanceComponent_ReleasePartnerSlot(reinterpret_cast<void*>(memberIBP), '\0');
            }

            int64_t next = *ringHead;
            if (!RingProgress(underAge ? "orphan" : "emancipate", member, next, iterations)) break;
            member = next;
        }
    } else {
        // A spouse survives: re-home the whole ring onto them.
        int64_t* ringHead = reinterpret_cast<int64_t*>(p + 0x260);
        int iterations = 0;
        int64_t member = *ringHead;
        while (member != 0) {
            int64_t spouseIBP = *spouseSlot;
            CitizenComp_sub_1fa0(*ringHead, &spouseIBP);   // 0x140111fa0
            int64_t next = *ringHead;
            if (!RingProgress("re-home", member, next, iterations)) break;
            member = next;
        }
    }

    // Clear the spouse's back-ref to us and our own spouse link (no-op with no spouse).
    if (*spouseSlot != 0) {
        int64_t nullBack = 0;
        Ctor(&nullBack, nullptr);
        *reinterpret_cast<int64_t*>(*spouseSlot + 0x250) = nullBack;
        int64_t nullOwn = 0;
        Ctor(&nullOwn, nullptr);
        *spouseSlot = nullOwn;
    }

    // ---- 5. housing (+0x248) --------------------------------------------------
    // Read BEFORE AssignHousing clears it: the vacated house is what step 6 passes
    // to the replacement-worker search as the "near here" hint.
    uint64_t formerHouse = *reinterpret_cast<uint64_t*>(p + 0x248);
    if (formerHouse != 0) {
        int64_t nullIBP = 0;
        Ctor(&nullIBP, nullptr);
        CitizenComp_AssignHousing(this, &nullIBP);   // 0x140111ee0
    }

    // ---- 6. profession list ---------------------------------------------------
    uintptr_t gw = GameUpdateLoop::_singleton ? reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) : 0;
    if (gw) {
        int64_t professionRoot = *reinterpret_cast<int64_t*>(gw + 0xb0);
        int64_t defaultProfession = professionRoot
                                        ? *reinterpret_cast<int64_t*>(professionRoot + 0x30) : 0;
        int64_t profession = *reinterpret_cast<int64_t*>(p + 0x1f0);
        if (professionRoot && profession != defaultProfession) {
            alignas(8) uint8_t retbuf[16] = {0};
            int64_t nearIBP = static_cast<int64_t>(formerHouse);
            void* result = CitizenList_AssignWorker_c3d0(*reinterpret_cast<int64_t*>(gw + 0xf0),
                                                         retbuf, &nearIBP, profession);   // 0x14008c3d0
            int64_t replacement = result ? *reinterpret_cast<int64_t*>(result) : 0;

            void* target;
            int   eventType;
            const char* name;
            if (replacement == 0) {
                target = this; eventType = 0x10; name = nullptr;   // nobody to take the job
            } else {
                name = *reinterpret_cast<const char**>(p + 0x28);   // the deceased's name
                target = reinterpret_cast<void*>(replacement);
                eventType = 0xf;                                    // replacement found
            }
            CitizenComponent_PostLifeEvent(target, eventType, 1, name);   // 0x140113af0
        }

        // ---- 7. trade-work list + any dialog bound to our entity ---------------
        int64_t ownerIBP = *reinterpret_cast<int64_t*>(p + 0x10);
        NomadComp_FUN_140103480(*reinterpret_cast<int64_t*>(gw + 0x120), &ownerIBP);   // 0x140103480

        // ---- 8. the town citizen list (townMgr+0xa0) — the whole point ---------
        int64_t selfIBP = 0;
        Ctor(&selfIBP, this);
        int64_t townMgr = *reinterpret_cast<int64_t*>(gw + 0xf0);
        if (townMgr)
            CemeteryListErase_Reimpl(reinterpret_cast<int64_t*>(townMgr + 0xa0), &selfIBP);   // 0x140002cc0
    }
}
