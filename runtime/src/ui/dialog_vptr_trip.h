// ============================================================================
// runtime/src/ui/dialog_vptr_trip.h — WAVE 10E LANE FINAL
//   Does ANYTHING dispatch a `UI::Dialog` virtual through the vtable pointer
//   our ports stamp?
// ============================================================================
//
// `??_7Dialog@UI@@6B@` is ported 9/9 with TRAP=0 and has been held unpromoted
// across two waves on one measurement: lane DLG8's seven-way ATTENDANCE counter
// reads zero on every route. That counter has TWO STRUCTURAL BLIND SPOTS and
// no positive control:
//
//   * it covers seven of the nine slots. Slot 0 (the vector deleting dtor)
//     lives in runtime/src/object/deleting_dtor.cpp and slot 4 is the shared
//     `?Compile@External@Object@@`, so a dispatch through EITHER of those two
//     increments nothing and reads exactly like no dispatch at all;
//   * nothing ever demonstrated the counter CAN move. A counter that has only
//     ever printed zero is indistinguishable from a counter that is wired
//     wrong (reference_probe_needs_an_effectiveness_control.md), and this
//     project has already shipped one field that was structurally incapable of
//     carrying the answer it was read for
//     (reference_diagnostic_field_cannot_express_the_answer.md).
//
// This instrument answers the question the row actually turns on, and answers
// it for all NINE slots at once: it replaces the table our ports stamp with a
// nine-entry TRIPWIRE table whose every entry records the slot index and then
// TAIL-JUMPS to the real body, so an armed run behaves exactly as an unarmed
// one and any dispatch — from our code, from the DLL, through any slot — is
// counted and named.
//
// It is also independent of the mint: it does not need
// `BANISHED_RT_MINT_ROWS=??_7Dialog@UI@@6B@`, so it does not drag in the three
// permanent divergences that arming the row puts into lane DLG2's ctor oracle.
// The two instruments are therefore genuinely separate screens rather than one
// screen read twice (reference_two_checks_one_hidden_premise.md).
//
//   BANISHED_DLG_VPTR_TRIP=1   arm the tripwire
//   BANISHED_DLG_VPTR_TRIP=2   arm it AND fire ONE dispatch on the first
//                              stamped object — the POSITIVE CONTROL. Slot 3
//                              (`?GetFileVersion@Dialog@UI@@`, `MOV EAX,6;RET`)
//                              is chosen because it reads nothing and writes
//                              nothing, so the control cannot perturb the run
//                              it is validating.
//
// Off (unset or 0) the two entry points are a branch on a cached int and the
// stamp is the pointer it always was.
// ============================================================================
#ifndef RT_UI_DIALOG_VPTR_TRIP_H
#define RT_UI_DIALOG_VPTR_TRIP_H

namespace RtUiDlgTrip {

// The pointer to stamp into a `UI::Dialog`'s +0x00. Returns `real` unchanged
// unless the instrument is armed; never returns null when `real` is non-null.
void* TableOrReal(void* real);

// Call immediately after stamping. In mode 2, and only for the FIRST object
// ever stamped, this dispatches slot 3 through `obj`'s own vptr — the positive
// control. In every other mode it does nothing.
void AfterStamp(void* obj);

} // namespace RtUiDlgTrip

#endif // RT_UI_DIALOG_VPTR_TRIP_H
