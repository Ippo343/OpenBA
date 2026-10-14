// ============================================================================
// src/ext/render_pipe/render_pipe.h -- ext.render_pipe (class A, default OFF)
// ============================================================================
// NOT RETAIL. docs/extensions/handins/RENDERPIPE0.md; THREADRESEARCH.md section 1.5.
//
// The frame's emit (Renderer::Draw pass 2, the post-scene draws, the post-draw
// callbacks, EndCommands, the frame index, Present) runs one frame behind the
// gather (GameUpdateLoop::Draw steps 1-6 and pass 1). RENDERPIPE-0 runs it on
// the main thread in that PIPELINED ORDER, with no thread: pass 1 of frame N+1,
// then the held packet of frame N. The sim is unchanged: before any fixed step
// the held packet is emitted (MainLoop::_extFrame, kExtBeforeSteps).
//
// Selected through ONE engine seam, Engine::MainLoop::_extFrame (MainLoop.h),
// set by Sync() at boot and on every Extensions-dialog toggle. Switching off
// takes effect at the next Frame tail (drain, then the seam is removed).
// ============================================================================
#pragma once

#include <cstdint>

namespace RenderPipe {

// Install the seam when ext.render_pipe is on (the switch-off is the hook's own,
// at the next Frame tail). Called by Ext::InitFromSettings and Ext::SetAndRecord.
void Sync();
bool Installed();

// Per CSV window, for the [Stress::pipe] line (src/diag/stress_harness.cpp).
struct Counters {
    uint64_t pipelined   = 0;   // frames whose first half ran pipelined (a packet was held)
    uint64_t sequential  = 0;   // frames run unpipelined while installed (the faithful tail)
    uint64_t emits       = 0;   // packets emitted
    uint64_t drainStep   = 0;   // emitted before a fixed step (the fence)
    uint64_t drainOther  = 0;   // emitted at a tail that then ran unpipelined
    uint64_t discarded   = 0;   // new packets dropped (device lost / BeginCommands failed in the emit)
    uint64_t idleWrites  = 0;   // frames that ran SunLight::Set / UpdateCamera (nothing pending)
    uint64_t idleSkips   = 0;   // frames that skipped them (fixes 4 and 5)
};
Counters TakeWindow();

}  // namespace RenderPipe
