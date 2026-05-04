// standalone/async_loader.h
// Standalone-only async loader for UpdateLoop::Load slot.
//
// Mirrors the role of Runtime Engine::Loader (a System::Thread that runs
// `next->vtable->Load(next)` in the background while the main thread keeps
// drawing the splash). The Runtime Loader::Run dereferences Engine+0x6c0
// (some audio sub-object) which deadlocks/hangs in our standalone exe; rather
// than chase that, we run our own worker thread that calls the next loop's
// Load slot directly. The Runtime Loader thread still gets kicked by
// fn_DialogLoaderLoopT_Startup but its hung state is harmless — we don't
// gate the splash transition on its completion event.
//
// Usage:
//   StartAsyncLoad(menuDlt);    // from SplashDLT::Stub_Startup
//   while (IsAsyncLoadBusy()) { ... }   // SplashDLT::Stub_Update polls
//   // transition to menuDlt once IsAsyncLoadBusy() returns false

#pragma once

#ifdef STANDALONE_EXE

struct UpdateLoop;

namespace AsyncLoader {

// Kick a load on a background thread. The worker calls
// `loop->vtable->Load(loop)`. Safe to call repeatedly — each call waits for
// the previous load to finish before starting a new one.
void StartAsyncLoad(UpdateLoop* loop);

// True while the most recent StartAsyncLoad's worker is still running.
bool IsAsyncLoadBusy();

} // namespace AsyncLoader

#endif // STANDALONE_EXE
