#pragma once
// ============================================================================
// src/ext/walk_batch/walk_batch.h -- NOT RETAIL. ext.walk_batch (class B, locked per game,
// pseudo-mod ext:walk_batch, formatVersion 1): OFFSCREEN design D2, the walk batch at speed N.
// docs/extensions/handins/WALKBATCH.md is the design and the proof.
//
// At speed N retail runs N sim iterations per Update and draws only the last one. On the
// N-1 intermediate iterations (MeshInstance::_rootOnlyUpdates set), a character whose move
// tick is exactly MoveTo::Execute's walking branch followed by the tail's step gets a short
// tick instead of CharacterComponent::Tick: the same Execute arithmetic (the speed of the
// cell, the waypoint advance with the real MoveTo::UpdateFacing), the same root-motion step
// and heading recurrence, written straight into the live fields. Skipped: the executor's
// loop and virtual dispatch, the trig, the rotation store, the Geometry push and
// CharacterGrid::Place. The last iteration always runs the faithful walk, which writes the
// rotation of the yaw and places the character in the grid.
//
// Why class B: on the intermediate iterations the character grid holds a walker where its
// last real tick placed it (the frame start, or its last declined tick), so another
// character's QueryNearestCharacter (MoveTo::Enter's follow flag, hunts, FindBestTarget)
// can see it up to N-1 ticks behind; the model rotation also lags inside the frame.
// Position, yaw, speed, facing and path index are retail's every tick. The compile-time
// variant kPlaceEveryTick (walk_batch_rules.h) also places the walker every tick; then only
// the rotation and the render transform lag (WALKBATCH.md section 11).
//
// Selection (DESIGN.md hook rule 3): EntityList::Update picks the walk instantiation once per
// iteration from Select() -- the extension's per-game value under the class B lock and the
// iteration's root-only flag. The faithful walk is the existing body, with no test in its loop.
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
// value for this game (Ext::Ext(), held by the class B lock) and the BANISHED_MOVETICK_TRACE
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
