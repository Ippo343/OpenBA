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
// No oracle. Seven of these bodies read nothing,
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
// Dtor` (runtime/src/ui/dialog.cpp). No constructor anywhere stamps it. See
// docs/runtime_port/WAVE10E_LANE_FINAL.md for the four independent proofs that
// the row is permanently unpromotable, including the one that does not depend
// on reachability at all: the single consumer's stamp is overwritten, unread,
// by the first statement of `??1Internal@Object@@`.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/ui/DialogController.h"

namespace RtUi {

namespace {

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

}  // namespace RtUi

// ============================================================================
// ENDGAME B.3.6 block B — the NINE bodies are members of UI::DialogController
// (runtime/types/ui/DialogController.h). ENDGAME B.3.9 block UI-D: the minted
// `??_7DialogController@UI@@6B@` is retired, and with it the six slot doors
// whose only reader it was and the `[DlgCtrl] ATTENDANCE` counters that
// measured its promotion. Three doors STAY (below): OnOpened, OnClosed and
// OnSerializeSettings are reached BY ADDRESS from standalone/dlgctrl_route.cpp
// (block RF's, env-gated) — a door reached by address survives (B9-6).
// ============================================================================
namespace UI {

// slot 2 — ?Type@DialogController@UI@@UEBAIXZ            0x18003b6b0
//     MOV EAX,dword ptr [0x1801f9e48] / RET
uint32_t DialogController::Type() const { return TypeId(); }
// ?TypeId@DialogController@UI@@SAIXZ 0x18003b6c0 — the same dword.
uint32_t DialogController::TypeId() {
    return *reinterpret_cast<const uint32_t*>(
        RtUi::DialogControllerTypeInfo() + RtUi::kBaseTypeInfoTypeId);
}

// slot 1 — ?TypeName@DialogController@UI@@UEBAPEBDXZ     0x18003b6d0
//     MOV RAX,qword ptr [0x1801f9e50] / RET      (= _typeInfo + 0x08)
const char* DialogController::TypeName() const {
    return *reinterpret_cast<const char* const*>(
        RtUi::DialogControllerTypeInfo() + RtUi::kBaseTypeInfoName);
}

// The seven empty base virtuals. Each is `RET 0` in retail and nothing else;
// see the banner for the byte evidence.
void DialogController::OnCreated() {}                                  // [4]  0x1800cd2b0
void DialogController::OnOpened() {}                                   // [5]  0x1800cd2a0
void DialogController::OnClosed() {}                                   // [6]  0x1800cd290
void DialogController::OnUpdateLayout() {}                             // [7]  0x1800cd280
void DialogController::OnUpdate(const System::Time& /*time*/) {}       // [8]  0x1800cd270
void DialogController::OnElementEvent(Object::InternalPtr<Element>* /*element*/,
                                      int32_t, int32_t, int32_t, int32_t, int32_t,
                                      void* /*userData*/) {}           // [9]  0x1800cd260
// [10] 0x1800cd250 — `IO::Stream&`, never read. This is the slot
// dialog_settings.cpp dispatches per controller on the write path and OnOpened
// on the read path; that both call sites exist and this default does nothing
// is exactly why each controller's settings record can be EMPTY and still framed.
void DialogController::OnSerializeSettings(void* /*stream*/) {}

// ?CreateNewObject@ / ?CreateInPlaceObject@DialogController@UI@@
// 0x18003b6e0 / 0x18003b6f0: each is `XOR EAX,EAX ; RET` — the base is never
// built by reflection (the FACTUI1 row this replaces returned null too).
Object::Base* DialogController::CreateNewObject() { return nullptr; }
Object::Base* DialogController::CreateInPlaceObject(void*) { return nullptr; }

// ??1DialogController@UI@@UEAA@XZ 0x1800cd2f0 — EMPTY, retail's body: the
// restamp and the inlined ~Internal are the compiler's (ENDGAME B.3.9 UI-D:
// the retired `RtUiDialogController_Dtor` door did the same two by hand).
DialogController::~DialogController() {}

}  // namespace UI

namespace {
inline UI::DialogController* AsController(void* self) {
    return reinterpret_cast<UI::DialogController*>(self);   // B.3.6 bridge
}
inline const UI::DialogController* AsController(const void* self) {
    return reinterpret_cast<const UI::DialogController*>(self);   // B.3.6 bridge
}
}  // namespace

extern "C" void RtUiDialogController_OnOpened(void* self) {
    AsController(self)->UI::DialogController::OnOpened();
}
extern "C" void RtUiDialogController_OnClosed(void* self) {
    AsController(self)->UI::DialogController::OnClosed();
}
extern "C" void RtUiDialogController_OnSerializeSettings(void* self, void* stream) {
    AsController(self)->UI::DialogController::OnSerializeSettings(stream);
}

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
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics.
    RegisterRuntimeExport("?CreateNewObject@DialogController@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::DialogController::CreateNewObject);
    RegisterRuntimeAddr(0x18003b6e0, (void*)&UI::DialogController::CreateNewObject); ++n;
    RegisterRuntimeExport("?CreateInPlaceObject@DialogController@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::DialogController::CreateInPlaceObject);
    RegisterRuntimeAddr(0x18003b6f0, (void*)&UI::DialogController::CreateInPlaceObject); ++n;

    RegisterRuntimeExport("?OnOpened@DialogController@UI@@UEAAXXZ",
                          (void*)&RtUiDialogController_OnOpened);
    RegisterRuntimeAddr(0x1800cd2a0, (void*)&RtUiDialogController_OnOpened); ++n;

    RegisterRuntimeExport("?OnClosed@DialogController@UI@@UEAAXXZ",
                          (void*)&RtUiDialogController_OnClosed);
    RegisterRuntimeAddr(0x1800cd290, (void*)&RtUiDialogController_OnClosed); ++n;

    RegisterRuntimeExport("?OnSerializeSettings@DialogController@UI@@UEAAXAEAVStream@IO@@@Z",
                          (void*)&RtUiDialogController_OnSerializeSettings);
    RegisterRuntimeAddr(0x1800cd250, (void*)&RtUiDialogController_OnSerializeSettings); ++n;

    return n;
}
