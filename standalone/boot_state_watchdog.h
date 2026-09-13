// standalone/boot_state_watchdog.h
// Publishes a one-line, machine-readable boot state to standalone_boot_state.txt so an
// automated run can distinguish "blocked on a modal dialog" from "still loading" from
// "frozen". See boot_state_watchdog.cpp for the full rationale, and tools/boot_state.sh
// for the reader an agent should call.

#pragma once

namespace BootState {

// Start the watchdog thread. Call as early in WinMain as possible -- the dialog we most
// need to catch opens inside Engine::Startup, so this must already be running by then.
// Also records the calling thread as THE MAIN THREAD for the hang dump.
void Start();

// Flip BOOTING -> RUNNING. Call right after Engine::Startup returns.
void MarkStartupComplete();

// Stop the thread and stamp the file EXITED. Only reached on a clean shutdown; a killed
// process leaves its last line behind, which the reader script reports as STALE.
void Stop();

}  // namespace BootState

// ---------------------------------------------------------------------------
// The heartbeat, as extern "C" so the runtime port and code/Game can call it
// without taking a dependency on standalone/ headers.
// ---------------------------------------------------------------------------

// Bump the frame counter. Called once per main-loop iteration from
// RtEngine_MainLoop_Run (runtime/src/engine/mainloop_run.cpp) -- the single
// point every loop type (loader, splash, menu DialogLoopT, GameUpdateLoop)
// passes through. A counter that stops advancing is what "frozen" means here.
extern "C" void BootState_MarkFrame();

// MainLoop::Run has returned and teardown has begun. Teardown legitimately
// renders no frames, so it gets its own (longer) budget measured from this
// moment. Called from code/Game/Game/game_main.cpp right after Run returns,
// before Engine shutdown.
extern "C" void BootState_MarkQuitting();

// ---------------------------------------------------------------------------
// The exe's ONE stack walker, defined in standalone/main.cpp next to the VEH
// crash handler that has always used it. Declared here because the hang dump
// reuses it rather than growing a second copy of the unwind logic.
//
// `origCtx` is not modified (the walker copies it). The sink must not touch a
// CRT FILE*: the hang dump calls this with other threads SUSPENDED.
// ---------------------------------------------------------------------------
struct _CONTEXT;
typedef void (*StackLineSink)(void* user, const char* line);
extern "C" void StandaloneWalkStack(struct _CONTEXT* ctx, StackLineSink sink, void* user);
