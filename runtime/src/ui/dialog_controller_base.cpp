// ============================================================================
// runtime/src/ui/dialog_controller_base.cpp — Wave 10B lane DLGV
//
// ALL NINE remaining slots of `??_7DialogController@UI@@6B@` (DLL 0x1801565f0).
// ============================================================================
//
// The vtable had 11 slots, 2 ported and NINE abort traps, which made it the
// furthest-away of the four minted UI vtables in
// `standalone/reg_mint_promoted.cpp`'s "NOT PROMOTED, and why" list. It is in
// fact the CLOSEST of them, because seven of the nine are empty.
//
//   slot  1  ?TypeName@DialogController@UI@@UEBAPEBDXZ              0x18003b6d0
//   slot  2  ?Type@DialogController@UI@@UEBAIXZ                     0x18003b6b0
//   slot  4  ?OnCreated@DialogController@UI@@UEAAXXZ                0x1800cd2b0
//   slot  5  ?OnOpened@DialogController@UI@@UEAAXXZ                 0x1800cd2a0
//   slot  6  ?OnClosed@DialogController@UI@@UEAAXXZ                 0x1800cd290
//   slot  7  ?OnUpdateLayout@DialogController@UI@@UEAAXXZ           0x1800cd280
//   slot  8  ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z 0x1800cd270
//   slot  9  ?OnElementEvent@DialogController@UI@@UEAAX…            0x1800cd260
//   slot 10  ?OnSerializeSettings@DialogController@UI@@UEAAX…       0x1800cd250
//
// ----------------------------------------------------------------------------
// THE SEVEN EMPTY BODIES — the bytes, read ONE AT A TIME
// ----------------------------------------------------------------------------
// The seven addresses 0x1800cd250..0x1800cd2b0 sit exactly 16 bytes apart, which
// is a HYPOTHESIS that they are the base class's do-nothing virtual defaults,
// not a finding. It was checked rather than assumed, three ways:
//
//  1. A single 128-byte `read_memory` at 0x1800cd250 returns, seven times over,
//     the identical 16-byte cell:
//
//         c2 00 00 cc cc cc cc cc cc cc cc cc cc cc cc cc
//
//     `C2 00 00` is `RET 0` — a near return that pops zero bytes of arguments,
//     which on the Microsoft x64 ABI is a plain return. The other thirteen bytes
//     are `INT3` alignment padding, so each body is 3 bytes long and nothing
//     falls through into the next.
//  2. Each of the seven was ALSO disassembled individually (Ghidra agrees:
//     `RET 0x0` and nothing else at every one of the seven), rather than seven
//     conclusions being drawn from one reading.
//  3. The byte at 0x1800cd240, immediately before the run, is `C3` followed by
//     `CC` padding — so 0x1800cd250 is a function START, not the tail of a
//     larger body that the 16-byte stride happened to sample.
//
// So all seven really are empty virtuals. **"Trivial" does not exempt them from
// the gates.** They are registered under BOTH doors like every other row in this
// namespace, and the promotion of `??_7DialogController@UI@@6B@` that they
// unlock is A/B'd in a real boot — see docs/runtime_port/WAVE10B_LANE_DLGV.md.
// An empty body is exactly the shape whose registration can be forgotten and
// read as absent later (reference_half_registered_port_reads_as_absent.md).
//
// A note on ARITY, because an empty body hides it. `OnUpdate` takes
// `const System::Time&`, `OnSerializeSettings` takes `IO::Stream&`, and
// `OnElementEvent` takes an `Object::InternalPtr<UI::Element>` BY VALUE plus
// five ints and a `void*`. None of that is observable through a body that reads
// nothing and returns nothing: on Win64 the caller cleans the stack and every
// argument is either in a volatile register or in the caller's own home space.
// The parameters below are spelled at the right ARITY with opaque types so the
// declaration documents the contract; they are deliberately not given precise
// types this file would have to invent. (`InternalPtr` is taken as `void*`,
// which is correct either way: if it is one pointer it is passed in a register,
// and if it were larger than 8 bytes MSVC would pass a hidden pointer to it.)
//
// ----------------------------------------------------------------------------
// THE TWO THAT ARE NOT EMPTY — both read one BaseTypeInfo
// ----------------------------------------------------------------------------
//     18003b6b0  MOV EAX,dword ptr [0x1801f9e48]   /  RET
//     18003b6d0  MOV RAX,qword ptr [0x1801f9e50]   /  RET
//
// 0x1801f9e48 is `?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A` —
// the SAME static `runtime/src/ui/dialog_create.cpp` already reads a type id out
// of, by that name, with the same `MOV EDX,dword ptr [0x1801f9e48]` recorded in
// its comment. `Object::BaseTypeInfo` is +0x00 `typeId` (dword), +0x04 an unused
// word, +0x08 `const char* name` (runtime/types/object/TypeInfo.h), so the two
// slots are `_typeInfo.typeId` and `_typeInfo.name` — 0x1801f9e50 is the same
// object at +0x08, not a string slot of its own. Two adjacent vtable slots
// reading +0x00 and +0x08 of one address is what makes this a BaseTypeInfo
// rather than a guess; it is the identical argument `dialog.cpp` makes for
// `?TypeName@DialogInstance@` and 0x1801faac0, and it holds here for the same
// reason.
//
// *** AEAA / `0V…` MEANS PRIVATE, NOT UNEXPORTED. *** This DLL exports its
// private members and `?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A`
// IS in the shipped export directory (reference_aeaa_is_private_not_unexported),
// so it is resolved BY NAME through RT_DATA_SLOT and there is NO address door to
// justify here. RT_DATA_SLOT is the retry-a-miss form: it caches a hit and
// re-resolves a miss, so a null early in startup does not freeze for the life of
// the process.
//
// Note which side of the mint that leaves this: unlike
// `?_typeInfo@DialogInstance@UI@@` (0x1801faac0), which lane MINTSEL PROMOTED,
// the DialogController row is NOT promoted, so this resolves the DLL's own live
// static and reads the value retail's own two instructions read. If that row is
// ever promoted these two bodies follow it automatically, which is the correct
// coupling — they read a symbol, not an address.
//
// Returning the string literal "DialogController" was rejected for `TypeName`,
// for the reason dialog.cpp records at the same slot of the sibling class: it
// would compare equal under every `strcmp` in the tree and would be a DIFFERENT
// POINTER from the one `Factory::Startup` CRC'd into the type id. A port whose
// only failure mode is pointer identity fails silently and late.
//
// ----------------------------------------------------------------------------
// NO ORACLE, AND WHY THAT IS NOT A HOLDBACK HERE
// ----------------------------------------------------------------------------
// `RT_DIFF_BEGIN` is deliberately absent. Seven of these bodies read nothing,
// write nothing and return nothing — there is no post-state to compare, and a
// probe over them would report `divergences=0` over work that never happened,
// which is the green-count-over-nothing shape this project already named
// (reference_probe_needs_an_effectiveness_control.md). The other two are pure
// one-instruction loads of a named exported static; the check that they are
// right is that the static resolves and that the value matches, and the mint
// shadow's `BaseTypeInfo` field comparison already compares that very static's
// typeId against the loaded DLL's own node on every run.
//
// What IS measured, and is in the lane doc: the promotion A/B. The boot that
// reaches gameplay with the row registered, matched against the same boot
// without it, is this lane's evidence.
//
// *** CORRECTED, WAVE 10E LANE FINAL. *** This paragraph used to claim "our
// ported constructors stamp that table into real objects, so the DLL's own
// dispatch runs these nine bodies". THAT IS FALSE and it kept a route hunt
// alive for two waves. A complete grep of the symbol over code/, runtime/,
// engine/ and standalone/ finds exactly ONE live read — `RtUiDialogController_
// Dtor` (runtime/src/ui/dialog.cpp) — plus one deliberate `ResolveExportRaw`
// ORACLE read in the same file. No constructor anywhere stamps it. See
// docs/runtime_port/WAVE10E_LANE_FINAL.md for the four independent proofs that
// the row is permanently unpromotable, including the one that does not depend
// on reachability at all: the single consumer's stamp is overwritten, unread,
// by the first statement of `??1Internal@Object@@`.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"

namespace RtUi {

namespace {

// ---------------------------------------------------------------------------
// ATTENDANCE COUNTERS — the EFFECTIVENESS CONTROL for the promotion A/B, and
// NOT a verification of anything.
// ---------------------------------------------------------------------------
// Nine of these bodies read nothing and write nothing, so an A/B of the boot
// with and without `??_7DialogController@UI@@6B@` registered is IDENTICAL
// whether our slots ran a thousand times or never once. Without a counter, "the
// candidate run reached the same place as the control" cannot tell a safe
// promotion from a promotion that was never exercised — the same shape as
// reference_probe_needs_an_effectiveness_control.md, and the same reason
// runtime/src/io/encoding.cpp carries its own ATTENDANCE block.
//
// These count ENTRIES, unconditionally, on every build. They are deliberately
// NOT RtDiff sites: an RtDiff site increments `calls` only from inside a
// comparison, so a body that ran but never compared prints `calls=0`, which
// reads as "never ran" and means the opposite
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md).
struct CtrlCounts {
    unsigned long long typeName = 0, type = 0, onCreated = 0, onOpened = 0,
                       onClosed = 0, onUpdateLayout = 0, onUpdate = 0,
                       onElementEvent = 0, onSerializeSettings = 0;
    ~CtrlCounts() {
        const unsigned long long total = typeName + type + onCreated + onOpened +
                                         onClosed + onUpdateLayout + onUpdate +
                                         onElementEvent + onSerializeSettings;
        if (total == 0) {
            printf("[DlgCtrl] ATTENDANCE: none of the nine bodies was entered "
                   "in this run.\n");
            return;
        }
        printf("[DlgCtrl] ATTENDANCE (entries, not comparisons): total=%llu "
               "TypeName=%llu Type=%llu OnCreated=%llu OnOpened=%llu "
               "OnClosed=%llu OnUpdateLayout=%llu OnUpdate=%llu "
               "OnElementEvent=%llu OnSerializeSettings=%llu\n",
               total, typeName, type, onCreated, onOpened, onClosed,
               onUpdateLayout, onUpdate, onElementEvent, onSerializeSettings);
    }
};
CtrlCounts g_counts;

// `Object::BaseTypeInfo` layout (runtime/types/object/TypeInfo.h): +0x00 typeId
// (uint32), +0x04 an unused word, +0x08 `const char* name`. Spelled as constants
// here for the same reason dialog.cpp spells them: that header cannot be pulled
// into this TU without dragging the object-model dependency chain with it.
constexpr int kBaseTypeInfoTypeId = 0x00;
constexpr int kBaseTypeInfoName   = 0x08;

// ?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A  (DLL 0x1801f9e48).
// Resolved BY NAME — see the banner on AEAA/private.
inline const uint8_t* DialogControllerTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A");
}

}  // namespace

// ---------------------------------------------------------------------------
// slot 2 — ?Type@DialogController@UI@@UEBAIXZ            0x18003b6b0
//     MOV EAX,dword ptr [0x1801f9e48] / RET
// ---------------------------------------------------------------------------
extern "C" uint32_t RtUiDialogController_Type(const void* /*self*/) {
    ++RtUi::g_counts.type;
    return *reinterpret_cast<const uint32_t*>(
        DialogControllerTypeInfo() + kBaseTypeInfoTypeId);
}

// ---------------------------------------------------------------------------
// slot 1 — ?TypeName@DialogController@UI@@UEBAPEBDXZ     0x18003b6d0
//     MOV RAX,qword ptr [0x1801f9e50] / RET      (= _typeInfo + 0x08)
// ---------------------------------------------------------------------------
extern "C" const char* RtUiDialogController_TypeName(const void* /*self*/) {
    ++RtUi::g_counts.typeName;
    return *reinterpret_cast<const char* const*>(
        DialogControllerTypeInfo() + kBaseTypeInfoName);
}

// ---------------------------------------------------------------------------
// The seven empty base virtuals. Each is `RET 0` and nothing else; see the
// banner for the byte evidence and for why the argument lists are opaque.
// ---------------------------------------------------------------------------

// slot  4 — ?OnCreated@DialogController@UI@@UEAAXXZ             0x1800cd2b0
extern "C" void RtUiDialogController_OnCreated(void* /*self*/) {
    ++RtUi::g_counts.onCreated;
}

// slot  5 — ?OnOpened@DialogController@UI@@UEAAXXZ              0x1800cd2a0
extern "C" void RtUiDialogController_OnOpened(void* /*self*/) {
    ++RtUi::g_counts.onOpened;
}

// slot  6 — ?OnClosed@DialogController@UI@@UEAAXXZ              0x1800cd290
extern "C" void RtUiDialogController_OnClosed(void* /*self*/) {
    ++RtUi::g_counts.onClosed;
}

// slot  7 — ?OnUpdateLayout@DialogController@UI@@UEAAXXZ        0x1800cd280
extern "C" void RtUiDialogController_OnUpdateLayout(void* /*self*/) {
    ++RtUi::g_counts.onUpdateLayout;
}

// slot  8 — ?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z  0x1800cd270
//     `const System::Time&` — one reference argument, never read.
extern "C" void RtUiDialogController_OnUpdate(void* /*self*/,
                                              const void* /*time*/) {
    ++RtUi::g_counts.onUpdate;
}

// slot  9 — ?OnElementEvent@DialogController@UI@@UEAAX
//               V?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z   0x1800cd260
//     InternalPtr<Element> by value, five ints, one void*. None read.
extern "C" void RtUiDialogController_OnElementEvent(void* /*self*/,
                                                    void* /*element*/,
                                                    int /*a*/, int /*b*/,
                                                    int /*c*/, int /*d*/,
                                                    int /*e*/,
                                                    void* /*userData*/) {
    ++RtUi::g_counts.onElementEvent;
}

// slot 10 — ?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z
//                                                                 0x1800cd250
//     `IO::Stream&` — one reference argument, never read. This is the slot
//     runtime/src/ui/dialog_settings.cpp dispatches per controller (+0x50) on
//     the write path and OnOpened dispatches on the read path; that both call
//     sites exist and this default does nothing is exactly why each controller's
//     settings record can be EMPTY and still framed.
extern "C" void RtUiDialogController_OnSerializeSettings(void* /*self*/,
                                                          void* /*stream*/) {
    ++RtUi::g_counts.onSerializeSettings;
}

}  // namespace RtUi

// ============================================================================
// Registration
// ============================================================================
// BOTH doors for every row, which in this namespace is not decoration:
//   * BY NAME    — what the exe-side vtable builders in code/Game/Dialog/ and
//                  engine/funcs/engine_funcs.cpp ask for.
//   * BY ADDRESS — what the minted `??_7DialogController@UI@@6B@` looks up
//                  through RtPortLookupAddr when it fills its slots, and the
//                  ONLY door the mint readiness screen consults. A body ported
//                  but registered under one door only would still read TRAP.
int RtUiDialogControllerRegister() {
    using namespace RtUi;
    int n = 0;

    RegisterRuntimeExport("?TypeName@DialogController@UI@@UEBAPEBDXZ",
                          (void*)&RtUiDialogController_TypeName);
    RegisterRuntimeAddr(0x18003b6d0, (void*)&RtUiDialogController_TypeName); ++n;

    RegisterRuntimeExport("?Type@DialogController@UI@@UEBAIXZ",
                          (void*)&RtUiDialogController_Type);
    RegisterRuntimeAddr(0x18003b6b0, (void*)&RtUiDialogController_Type); ++n;

    RegisterRuntimeExport("?OnCreated@DialogController@UI@@UEAAXXZ",
                          (void*)&RtUiDialogController_OnCreated);
    RegisterRuntimeAddr(0x1800cd2b0, (void*)&RtUiDialogController_OnCreated); ++n;

    RegisterRuntimeExport("?OnOpened@DialogController@UI@@UEAAXXZ",
                          (void*)&RtUiDialogController_OnOpened);
    RegisterRuntimeAddr(0x1800cd2a0, (void*)&RtUiDialogController_OnOpened); ++n;

    RegisterRuntimeExport("?OnClosed@DialogController@UI@@UEAAXXZ",
                          (void*)&RtUiDialogController_OnClosed);
    RegisterRuntimeAddr(0x1800cd290, (void*)&RtUiDialogController_OnClosed); ++n;

    RegisterRuntimeExport("?OnUpdateLayout@DialogController@UI@@UEAAXXZ",
                          (void*)&RtUiDialogController_OnUpdateLayout);
    RegisterRuntimeAddr(0x1800cd280, (void*)&RtUiDialogController_OnUpdateLayout); ++n;

    RegisterRuntimeExport("?OnUpdate@DialogController@UI@@UEAAXAEBVTime@System@@@Z",
                          (void*)&RtUiDialogController_OnUpdate);
    RegisterRuntimeAddr(0x1800cd270, (void*)&RtUiDialogController_OnUpdate); ++n;

    RegisterRuntimeExport("?OnElementEvent@DialogController@UI@@UEAAXV?$InternalPtr@VElement@UI@@@Object@@HHHHHPEAX@Z",
                          (void*)&RtUiDialogController_OnElementEvent);
    RegisterRuntimeAddr(0x1800cd260, (void*)&RtUiDialogController_OnElementEvent); ++n;

    RegisterRuntimeExport("?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtUiDialogController_OnSerializeSettings);
    RegisterRuntimeAddr(0x1800cd250, (void*)&RtUiDialogController_OnSerializeSettings); ++n;

    return n;
}
