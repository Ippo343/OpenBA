#pragma once
// ============================================================================
// runtime/src/graphics/scene_grid.h — WAVE 9 LANE GFX3
// ============================================================================
//
// Shared between scene_grid.cpp (the six ported bodies) and
// scene_grid_probe.cpp (the synthetic-subject oracle), for exactly the reason
// array_internal.h and picking/scene_grid.h exist: a probe that re-derives the
// body it is probing is testing a COPY of the port, not the port.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>

#include "../../types/graphics/Scene.h"

namespace rtgfxgrid {

using RtGraphics::Scene;
using RtGraphics::SceneNode;
using RtGraphics::Geometry;

// ---------------------------------------------------------------------------
// The six bodies. Signatures are the retail raw ABI, argument for argument.
// ---------------------------------------------------------------------------

// 0x18005e4a0 — the grid initialiser Scene::Scene calls with 2048.0f.
// Returns `self` in RAX, exactly as retail does (`MOV RAX,RBP` before the
// epilogue), because the caller is free to use it.
Scene* GridInit(Scene* self, float cellSize);

// 0x18005e5e0 — the grid insert. `geo` is a POINTER TO a Geometry* (retail
// reads `MOV RAX,[RSI]`), which is why Scene::AddObject spills its argument to
// a local before calling. Returns the node id.
uint32_t GridAdd(Scene* self, Geometry* const* geo);

// 0x18005ee30 — recompute node `id`'s quadtree cell from the geometry's
// CURRENT world box and re-thread it if the cell changed.
void GridReindex(Scene* self, int32_t id);

// 0x18005f090 — splice node `id` out of its cell's CIRCULAR chain and
// decrement the cell's count and every ancestor's.
void GridUnthread(Scene* self, int32_t id);

// 0x18005f4b0 — the matching thread-in. Not on any census list; it is reached
// only from GridReindex, and porting GridReindex without it would have traded
// one unregistered door for another.
void GridThread(Scene* self, int32_t id);

// 0x18005f1f0 (walk the global list freeing every node, then clear {head,
// tail, count}) IS `Collection::LinkedList<T>::Clear()`, which Scene::~Scene
// calls on `_globals`. B.4: its uncalled bridge and the GlobalList / GlobalNode
// mirrors it took are gone.

}  // namespace rtgfxgrid
