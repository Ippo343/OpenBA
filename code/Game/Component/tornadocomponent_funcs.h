#pragma once
// Function declarations for the reimplemented TornadoComponent lane.

#include "engine/types/TornadoComponent.h"   // the forwarder: also carries TornadoDescriptionFields
#include <cstdint>

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
// 0x140058040 — countdown + the three strike sets (scatter cell, radius query,
// square sweep).
void  Tornado_DamageTick_58040(TornadoComponent* self, const int64_t* dt);

// Leaves owned by other classes that had no port yet:
// 0x14000cf40 — Vector3 dynamic-array grow+fill {data, count, capacity}.
// 0x140053720 — ParticleComponent: negate instance `index`'s playback rate.
void  Tornado_ParticleReverse_53720(void* particleComp, int index);
// 0x140042960 — AudioComponent: move every emitter to `newPos` and cache it.
void  Tornado_AudioMoveEmitters_42960(void* audioComp, void* newPos);
}
