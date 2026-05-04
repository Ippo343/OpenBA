// standalone/chunked_scene_loader.h
// Standalone-only chunked driver for MenuDLT::Load's heavy PhaseB work.
//
// MenuDialogLoopT_LoadStaticMeshes_PhaseB iterates 12K-15K elements per scene
// (Scene[0..9].rsc) — ~542ms of synchronous work. Running it from MenuDLT::
// Stub_Startup blocks the main thread for that whole time. The PhaseB loop
// can NOT run on a worker thread because Scene::AddObject mutates global
// scene state in a non-thread-safe way (see async_loader.h notes).
//
// Solution: split LoadScene_PhaseA into three pieces (LoadSceneA1 + chunked
// PhaseB + LoadSceneA3) and drive PhaseB incrementally from MenuDLT::
// Stub_Update — each tick processes ~500 elements (~17ms of work).
//
// Usage from menudialogloopt_stubs.cpp:
//   ChunkedSceneLoader::Init(menuDltSelf);   // from Stub_Startup (main thread)
//   while (...) {
//     ChunkedSceneLoader::Tick(500);          // from Stub_Update (per-tick)
//     if (!ChunkedSceneLoader::IsBusy()) break;
//   }
//
// Single-job model — only one chunked load in flight at a time.
//
// All operations log [ChunkedSceneLoader] lines for greppability.

#pragma once

#ifdef STANDALONE_EXE

namespace ChunkedSceneLoader {

// Run LoadSceneA1 (~10ms — small allocs + MenuScene.rsc load + 3
// AddGlobalObject calls) and capture the chunked PhaseB state. After this
// returns, IsBusy() is true; Tick() drives the rest. Safe to call multiple
// times — early-exits if already initialized for the same loop.
void Init(void* menuDltSelf);

// Process up to maxElements elements of the chunked PhaseB loop. When the
// chunk function reports done, runs LoadSceneA3 (particles, ~5ms) and clears
// the busy flag. Returns the number of elements processed this call.
int Tick(int maxElements);

// True while Init has run but Tick hasn't yet finished the iteration.
bool IsBusy();

} // namespace ChunkedSceneLoader

#endif // STANDALONE_EXE
