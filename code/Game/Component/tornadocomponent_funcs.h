#pragma once
// Function declarations for the reimplemented TornadoComponent lane.
//
// Slot indices below come from the vtable BYTES at 0x14015aa80. Ghidra's names for
// all three are off by one ("_Shutdown" / "_Serialize" / "_OnRemove"); the C++
// function names here keep Ghidra's spelling so grepping the address registry and
// the decompiler agree, but the COMMENTS say what each one really is.

#include "TornadoComponent.h"
#include <cstdint>

// vtable slot [3] (+0x18), 0x140058b80 — the real Serialize (save/load).
// Streams waypoint(+0x30), pathIndex(+0x50), pathCount(+0x48), the waypoints,
// Time(+0x28), position(+0x58).
void TornadoComponent_Shutdown(TornadoComponent* self, void* stream);

// vtable slot [4] (+0x20), 0x1400585a0 — the real Startup. Picks two map cells at
// least desc+0x24 apart, terrain-clamps them, fractal-fills 129 waypoints between
// them, posts event-log type 0x1e severity 3, and honours the pause-on-event setting.
void TornadoComponent_Serialize(TornadoComponent* self);

// vtable slot [8] (+0x40), 0x140057cd0 — the real per-frame Update. Slot 8, not 9:
// the ctor's 0x84 flags set bit 2, which is the full-rate EntityList update-tree walk.
// Particle-liveness gate -> path advance -> damage -> position integration ->
// geometry transform + audio emitters.
void TornadoComponent_OnRemove(TornadoComponent* self, void* time);

void TornadoComponent_InitFuncs();

// ---- ported leaves, exposed for the address-registry wave -------------------
extern "C" {
// 0x140058980 — recursive midpoint displacement over path[lo..hi]. Genuinely
// fractal; the `* 0.0` in the decompilation is an artifact (see the .cpp).
void  Tornado_PathFractal_58980(void* self, float* path, int lo, int hi);
// 0x1400a70c0 — first waypoint at least `radius` away, or -1 past the end.
int   Tornado_PathFindNext_a70c0(uint8_t* block, const float* pos, float radius);
// 0x1400a71e0 — latch the next waypoint once the current one is within
// `arriveRadius`. False only when the path is exhausted.
bool  Tornado_PathAdvance_a71e0(uint8_t* block, const float* pos,
                                float skipRadius, float arriveRadius);
// 0x1400583c0 — apply the tornado's effect to one entity (tree-knockdown /
// building-ignite / animal-remove / citizen-kill). The third arm is the ANIMAL arm:
// 0x140005500 matches AnimalComponent (type id 0x1401c6f98), not "loose objects".
// First parameter is dead in retail; kept.
void  Tornado_StrikeEntity_583c0(int64_t unusedFirstArg, void* entityIbp);
// 0x140058040 — countdown + the three strike sets (scatter cell, radius query,
// square sweep).
void  Tornado_DamageTick_58040(TornadoComponent* self, const int64_t* dt);

// Leaves owned by other classes that had no port yet:
// 0x14000cf40 — Vector3 dynamic-array grow+fill {data, count, capacity}.
void  Tornado_Vec3ArrayGrow_cf40(void* arrayBase, int newCount, const void* fill);
// 0x140053720 — ParticleComponent: negate instance `index`'s playback rate.
void  Tornado_ParticleReverse_53720(void* particleComp, int index);
// 0x140042960 — AudioComponent: move every emitter to `newPos` and cache it.
void  Tornado_AudioMoveEmitters_42960(void* audioComp, void* newPos);
}
