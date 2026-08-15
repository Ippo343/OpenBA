// standalone/boot_state_watchdog.h
// Publishes a one-line, machine-readable boot state to standalone_boot_state.txt so an
// automated run can distinguish "blocked on a modal dialog" from "still loading".
// See boot_state_watchdog.cpp for the full rationale, and tools/boot_state.sh for the
// reader an agent should call.

#pragma once

namespace BootState {

// Start the watchdog thread. Call as early in WinMain as possible -- the dialog we most
// need to catch opens inside Engine::Startup, so this must already be running by then.
void Start();

// Flip BOOTING -> RUNNING. Call right after Engine::Startup returns.
void MarkStartupComplete();

// Stop the thread and stamp the file EXITED. Only reached on a clean shutdown; a killed
// process leaves its last line behind, which the reader script reports as STALE.
void Stop();

}  // namespace BootState
