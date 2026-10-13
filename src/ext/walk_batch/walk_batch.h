#pragma once
// ============================================================================
// src/ext/walk_batch/walk_batch.h -- NOT RETAIL. ext.walk_batch (class A, a live toggle, no
// save impact): OFFSCREEN design D2, the walk batch at speed N.
// docs/extensions/handins/WALKBATCH.md is the design and the proof.
//
// At speed N retail runs N sim iterations per Update and draws only the last one. On the
// N-1 intermediate iterations (MeshInstance::_rootOnlyUpdates set), a character whose move
// tick is exactly MoveTo::Execute's walking branch followed by the tail's step gets a short
// tick instead of CharacterComponent::Tick: the same Execute arithmetic (the speed of the
// cell, the waypoint advance with the real MoveTo::UpdateFacing), the same root-motion step
// and heading recurrence, written straight into the live fields. Skipped: the executor's
// loop and virtual dispatch, the trig, the rotation store and the Geometry push.
// CharacterGrid::Place is still made, at the point the faithful tail makes it. The last
// iteration always runs the faithful walk, which writes the rotation of the caught-up yaw.
//
// Why class A: position, yaw, speed, facing, path index and the grid are retail's every tick;
// only the walker's model rotation (and its render transform) lags inside the frame, and no
// sim read of either was found (WALKBATCH.md sections 11 and 13). Between frames the state is
// retail's, so the toggle is live.
//
// Selection (DESIGN.md hook rules 2 and 4): GameUpdateLoop::Update picks the EntityList update
// once per iteration from Select() -- the toggle read once per frame and the iteration's
// root-only flag. The faithful walk is the existing body, with no test in its loop.
// ============================================================================
#include <cstdint>

class CharacterComponent;                // Component/CharacterComponent.h
namespace System { class Time; }         // src/engine/system/Value.h

namespace WalkBatch {

// Batched ticks taken (or, under BANISHED_STRESS_COMPPROF=4, the ticks the shadow checked).
// Main thread only; read by the [COMPPROF] /char walkbatch row.
extern long long g_batchedTicks;
// COMPPROF >= 2: the RDTSC ticks the batched ticks charged (entitylist.cpp's bracket) and
// their count; reset with the report window.
extern long long g_profTicks;
extern long long g_profCalls;

// GameUpdateLoop::Update, once per frame beside PerfX::BeginFrame: g_frameOn = the extension's
// live value (Ext::Ext(); class A, so a toggle takes effect at the next frame) and the BANISHED_MOVETICK_TRACE
// diagnostic off (its log counts every tail). The only reader of the toggle.
void BeginFrame();
extern bool g_frameOn;

// EntityList::Update, once per sim iteration, before the walk: true = walk with the batched
// tick. Plain loads (no call), so the faithful EntityList::Update keeps its code: false on
// the last iteration of a frame (and at speed 1, where every iteration is the last) and
// while the extension is off.
inline bool Select(bool rootOnlyIteration) { return g_frameOn && rootOnlyIteration; }

// The batched decision and step for one CharacterComponent tick. Returns true when the tick
// was taken here (the batched step, or under COMPPROF=4 the shadow, which runs the real
// tick itself); false = the caller runs CharacterComponent::Tick.
bool Tick(CharacterComponent* self, const System::Time* time);

// The batched walk's slot-8 tick of a CharacterComponent (EntityList::UpdateWalkBatch): Tick,
// else CharacterComponent::Tick. At BANISHED_STRESS_COMPPROF >= 2 it brackets the tick as the
// faithful walk's TickOneComponent does (the /tree and /char rows) and charges a batched tick
// to g_profTicks / g_profCalls. Defined here, out of line, so the walk's translation unit
// compiles its faithful instantiation as before.
void TickCharacter(CharacterComponent* self, const System::Time* time);

}  // namespace WalkBatch
