#pragma once
// ============================================================================
// standalone/engine_loader_gate.h — "is the ONE loader thread still working?"
// ============================================================================
//
// THE RULE THIS FILE EXISTS TO ENFORCE: there is exactly ONE resource loader,
// the Runtime `Engine::Loader` thread, and nothing else may be inside
// `ExternalFactory::FindOrLoad` / `External::Load` while it is running.
//
// That is retail's rule, and retail keeps it structurally rather than with a
// lock. Verified in Ghidra against `Application-x64.exe` and recorded in
// docs/runtime_port/endgame_b/LOADER_READ_SPIN_GHIDRA.md §Q5:
//
//   * The exe imports neither `Engine::Loader` nor `UpdateLoop::LoadResources`
//     (zero matches for "@Loader@Engine@@" and "LoadResources" in its import
//     name table), so the only dispatcher of `UpdateLoop::Load` in the shipped
//     game is `Engine::Loader::Run` inside the DLL.
//   * The splash loop's `Startup` (exe 0x140098fa0) kicks that one loader by
//     chaining `?Startup@DialogLoaderLoopT@Engine@@`, whose tail call IS
//     `Loader::Load`. It starts nothing else.
//   * The splash loop's `Update` (exe 0x140098da0) opens with
//     `?IsFinished@LoaderLoop@Engine@@` (Runtime 0x1800dc150 ==
//     `!(_loader._thread.runFlags & 1) && elapsed >= delay`) and only fades out
//     and calls `LoaderLoop::Exit` once that is true.
//   * `DialogLoaderLoopT::Shutdown`, `LoaderLoop::Shutdown` and
//     `LoaderLoop::OnVideoError` each `Thread::WaitForFinish` on that same
//     loader, so no other loop can be running while it works.
//
// Our standalone had drifted away from that: `standalone/async_loader.{h,cpp}`
// (OUR invention, not ported code) dispatched the SAME `next->vtable->Load` a
// second time, and `ChunkedSceneLoader` ran `FindOrLoad` on the main thread
// while the loader thread was mid-load. Two loaders on one unlocked package
// file pointer is the read race in
// docs/runtime_port/endgame_b/LOADER_READ_SPIN.md. This gate is how the splash
// re-learns retail's question.
//
// WHY NOT READ `runFlags & 1` DIRECTLY, THE WAY `LoaderLoop::IsFinished` DOES.
// `Loader::Load` ends in `Thread::Start`, which is
// `ResetEvent(hFinished); SetEvent(hStart)` — and `ThreadFunc` only sets
// `runFlags |= 1` after it wakes. A poll landing in that window reads
// runFlags == 0 and answers "finished" for a load that has not started. Retail
// never notices because `IsFinished` also requires `elapsed >= delayTicks` (a
// ~2 s splash hold) which covers the wake-up, but our chunked-scene gate has to
// ask on the very first tick. `hFinished` has no such window: `ResetEvent` runs
// synchronously ON THE CALLER THREAD inside `Loader::Load`, so from the instant
// `Loader::Load` returns until the work is done, the event is reliably
// non-signalled. Same signal, no race, no new primitive, no lock. The HANDLE is
// `_loader._thread.hFinished` (Runtime 0x1801fe7e0), already resolved into
// `g_pLoaderCompletionEvent` (engine/funcs/engine_funcs.h), and it is
// manual-reset so a zero-timeout wait observes it without consuming it.
//
// The `NoteKicked` latch covers the other end: before the first kick of a run
// the event's state is whatever the `Thread` ctor left, so "has anyone started
// this loader yet" has to be tracked rather than inferred. A splash that never
// reaches its `Loader::Load` (no dialog resource) therefore reports "not busy"
// and is not stranded.
//
// THERE IS DELIBERATELY NO TIMEOUT. If the loader thread ever genuinely stalls,
// this must present as a hang that `standalone_hang.log` can symbolize — not as
// a silent fallback that hides it.
// ============================================================================

namespace EngineLoader {

// Called immediately after the one `Loader::Load` kick (i.e. after
// `Engine::DialogLoaderLoopT::Startup` returns), to arm `IsBusy`.
void NoteKicked();

// True while the single Runtime `Engine::Loader` thread has a load outstanding.
// False before any kick, and false again once the loader has signalled.
// Main-thread only; cheap enough to call per frame.
bool IsBusy();

}  // namespace EngineLoader
