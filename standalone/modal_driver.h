#pragma once
// standalone/modal_driver.h — Wave 10B lane SHOW
//
// A harness callback placed in Engine::MainLoop's PER-TICK CALLBACK ARRAY at
// MainLoop+0x90. That array is the one thing that runs INSIDE the nested modal
// pump `UI::UserInterface::ShowDialog` (0x18011fd10) spins, and it is therefore
// the only place from which an unattended driver can dismiss a modal dialog.
//
// Everything else in the harness ticks from `MenuDLT::Stub_Update` or from a
// GameUpdateLoop, both of which hang off `MainLoop::Update` — and the nested
// pump calls `MainLoop::Frame(true)`, whose `param_1 == true` SKIPS
// `MainLoop::Update` entirely (0x1800dd28e is inside `if (!param_1)`). That is
// the mechanical reason `BANISHED_MODAL_ANSWER` had to be a BYPASS rather than
// a driver: with a modal up, no existing harness tick ever runs again until a
// human clicks.
//
// See docs/runtime_port/WAVE10B_LANE_SHOW.md.
//
// SWITCH (off by default):
//   BANISHED_MODAL_DRIVER=<answer>[,<ticks>]
//       Arm the driver. When a modal nested UI is seen for <ticks> consecutive
//       nested ticks (default 3), dismiss its front dialog with result
//       <answer>, exactly the way a MessageDialog button does:
//           *(uint32_t*)(dialog + 0x24) = answer;          // SetDialogResult
//           UI::UserInterface::CloseDialog(dialog->_ui, InternalPtr(dialog));
//       <answer> may be negative; use BANISHED_MODAL_DRIVER=0 to arm with
//       answer 0 (an explicit "0" arms — it is NOT read as "off", unlike the
//       other BANISHED_* switches, because 0 is a meaningful dialog result).
//       Leave the variable UNSET to disable.
//
//   BANISHED_MODAL_DRIVER_TEST=1
//       After the driver is armed and the menu has settled, run the
//       twin-subject differential over ShowDialog: build two identical
//       synthetic MessageDialogs and show one through the shipped DLL body and
//       one through whatever the port registry currently resolves, then compare
//       the post-state each leaves. See ModalDriver::RunSelfTest.

#include <cstdint>

namespace ModalDriver {

// True when BANISHED_MODAL_DRIVER is set. Cheap; safe before Arm().
bool Enabled();

// Idempotent. Reads Engine::MainLoop::_singleton (0x1801fe750) and, once it is
// non-null, appends our callback pair to MainLoop's draw (+0x80) and update
// (+0x90) arrays. Returns true once armed. Safe to call every frame.
bool Poll();

// Number of modals this driver has dismissed. This is the EFFECTIVENESS
// COUNTER: a run that reports armed=1 dismissed=0 proves the route was never
// exercised, which is a different verdict from "the route does not work".
int DismissedCount();

// BANISHED_MODAL_DRIVER_TEST. `outerUi` is the UserInterface the synthetic
// dialogs are parented to (the menu's). Runs at most once per process.
void RunSelfTest(void* outerUi);

}  // namespace ModalDriver
