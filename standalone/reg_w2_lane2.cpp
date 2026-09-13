// standalone/reg_w2_lane2.cpp
// `.rdata` census wave 2, LANE 2 — the single registration this lane recommends.
//
// Lane 2's six addresses are all exe .rdata VTABLES (no constants — nothing in
// this lane can be silently 6.7x off). Five of the six are verdict
// "deliberate — leave" or "dead". This file exists for the one address where
// registering restores a retail dispatch that is currently missing:
//
//   0x140160bf8 — TutorialInstance vtable, 4 slots. Read out of the binary:
//     [0] 0x1400281a0 ScalarDtor   — ported: TutorialInstance_Dtor
//     [1] 0x140028110 GetTypeInfo  — `MOV RAX,[0x1401c6670]; RET`
//     [2] 0x140028100 GetTypeId    — `MOV EAX,[0x1401c6668]; RET`
//     [3] 0x1400e1520 Serialize    — ported: TutorialInstance_Serialize
//   (the table ends at +0x20, where the "TutorialSetup" type-name string starts).
//
// Why it matters. FinishTutorialInstance::Slot4 (code/Game/Tutorial/
// finishtutorialinstance.cpp:138) allocates the 0x38-byte TutorialInstance on the
// live tutorial path and installs this vtable behind `if (g_TutorialInstanceVtable)`.
// That global is `ResolveAddr(Exe, 0x140160bf8)` (engine_funcs.cpp:4767) and is NULL,
// so the object keeps the Object::Internal base vtable that fn_ObjectInternal_Ctor
// left in slot 0. Consequences, in order of severity:
//   * slot 3 never reaches TutorialInstance_Serialize — the object is reachable from
//     the save stream (FinishTutorialInstance::Slot3, retail 0x1400d7bf0, serializes
//     an InternalBasePtr to it at +0x20), so the persist pass vcalls slot 3 and gets
//     Object::Internal's base Persist instead of the ExternalPtr<Tutorial> at +0x30
//     plus the int currentStep at +0x10. Fewer bytes on both legs — the round trip
//     cannot detect it (reference_null_guard_on_a_serializer_call_is_a_desync);
//     only a RETAIL-authored tutorial save is an oracle.
//   * slot 2 GetTypeId answers Object::Internal's id, not TutorialInstance's, so any
//     IsDerivedFrom narrow onto the restored object fails.
//   * TutorialInstance_Serialize and TutorialInstance_Dtor currently have ZERO
//     callers anywhere in the tree — the classic
//     reference_vtable_slot3_serialize_never_installed shape.
//
// Precedent for the shape of this table: the SIBLING class in the same file,
// FinishTutorialInstance, has exactly this treatment already — a static C++ array
// registered at its exe vtable address (0x140160478, finishtutorialinstance.cpp:365)
// with slot 1 (GetTypeInfo) left null and slot 2 supplied by
// Object::GetTypeIdByName. That table is live in production today. Slot 1 is safe to
// leave null for the same reason it is there: retail's slot 1 returns
// qword [0x1401c6670], which has exactly ONE xref in the whole exe (the slot-1 body
// itself) and is written only by Runtime-side registration we do not replicate — so
// there is nothing truthful to put in it, and the sibling proves nothing dispatches
// through it on these paths.
//
// INTEGRATOR: this is a live-path behaviour change on the tutorial, and this lane is
// not allowed to run the game, so it ships OPT-IN. Add `RegisterW2Lane2();` to the
// standalone registration sequence and A/B it with
// BANISHED_LIVE_TUTORIALINSTANCE_VT=1 against a RETAIL-authored tutorial save
// (BANISHED_AUTOLOAD, not BANISHED_AUTOTUTORIAL — the latter loads a different save).
// If the retail save round-trips clean with the flag on, promote it to unconditional
// / the default-on table. See the lane-2 report for the other five verdicts.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "resolve.h"
#include "Object/factory_registration.h"
#include "Tutorial/tutorialinstance_funcs.h"
#include "options.h"

namespace {

// Slot 2 (+0x10) GetTypeId. Retail reads dword [0x1401c6668]; that storage IS
// registered and filled (reg_tutorial_typeid.cpp:83 / FillTypeIdGlobals_tutorial),
// but we go through GetTypeIdByName for the same reason the sibling table does —
// it is correct regardless of registration order.
uint32_t TutorialInstance_GetTypeId_SA(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TutorialInstance");
    return cached;
}

// Mirror of exe vtable 0x140160bf8. Slot 1 (+0x08 GetTypeInfo) is deliberately null
// — see the header comment.
void* g_TutorialInstance_Vtable_SA[4] = {
    reinterpret_cast<void*>(&TutorialInstance_Dtor),          // +0x00 ScalarDtor
    nullptr,                                                  // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&TutorialInstance_GetTypeId_SA),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&TutorialInstance_Serialize),     // +0x18 Serialize
};

bool Enabled() {
    return Opt().liveTutorialInstVt;
}

}  // namespace

extern "C" void RegisterW2Lane2() {
    if (!Enabled()) return;
    RegisterExeAddr(0x140160bf8,
                    reinterpret_cast<uintptr_t>(g_TutorialInstance_Vtable_SA));
    std::fprintf(stderr, "[w2_lane2] TutorialInstance vtable 0x140160bf8 -> %p "
                         "(slot3 Serialize now dispatches)\n",
                 reinterpret_cast<void*>(g_TutorialInstance_Vtable_SA));
    std::fflush(stderr);
}
