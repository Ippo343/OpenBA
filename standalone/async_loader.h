// standalone/async_loader.h
// Standalone-only loader for the UpdateLoop::Load slot (menu splash chain).
//
// Mirrors the role of Runtime Engine::Loader, which runs `next->vtable->Load`
// while the main thread draws the splash. The Runtime Loader::Run dereferences
// Engine+0x6c0 (an audio sub-object) which deadlocks/hangs in standalone, so we
// drive the next loop's Load slot ourselves.
//
// IMPORTANT: By DEFAULT this now runs Load SYNCHRONOUSLY on the caller thread,
// NOT a worker thread. The engine allocator (MemoryAllocator::Allocate) is not
// thread-safe (it skips the critical section that Free takes), so loading on a
// worker while the main thread allocates (ChunkedSceneLoader::Tick, UI/Draw)
// raced the free-list and crashed ~1/3 of menu launches. Synchronous load
// removes the concurrency; the heavy scene chunking still animates the splash
// afterward on the main thread. Set BANISHED_ASYNC_LOAD=1 to opt back into the
// (racy) worker thread. See async_loader.cpp for the full root-cause writeup.
//
// Usage (unchanged):
//   StartAsyncLoad(menuDlt);    // from SplashDLT::Stub_Startup
//   while (IsAsyncLoadBusy()) { ... }   // SplashDLT::Stub_Update polls
//   // transition to menuDlt once IsAsyncLoadBusy() returns false
//   (in the default sync path IsAsyncLoadBusy() returns false immediately.)

#pragma once

#ifdef STANDALONE_EXE

struct UpdateLoop;

namespace AsyncLoader {

// Kick a load on a background thread. The worker calls
// `loop->vtable->Load(loop)`. Safe to call repeatedly — each call waits for
// the previous load to finish before starting a new one.
//
// By default the load runs SYNCHRONOUSLY on the caller thread (see the header
// note above) unless BANISHED_ASYNC_LOAD is set. Pass forceThreaded=true to
// ALWAYS use a worker thread regardless of the env — used for the new-game
// world build (GameUpdateLoop::Load), which is multi-second: running it on a
// worker keeps the loading screen animating (tips cycling) on the main thread,
// which only draws the splash UI (never the game scene) until the build's
// completion signal lets the loop transition. The free-list race that made the
// worker path unsafe is now covered by standalone/allocator_lock.cpp (Allocate
// AND Free take the allocator critical section).
void StartAsyncLoad(UpdateLoop* loop, bool forceThreaded = false);

// True while the most recent StartAsyncLoad's worker is still running.
bool IsAsyncLoadBusy();

} // namespace AsyncLoader

#endif // STANDALONE_EXE
