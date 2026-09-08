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

// The Scene's `_globals` LinkedList<Geometry*> node, the only thing
// FreeGlobals (0x18005f1f0) walks. Layout is Collection::LinkedList's, and the
// only field that body READS is `next` at +0x10 — `prev` is never touched by
// it, which is why the walk is single-linked even though the list is not.
struct GlobalNode {
    void*       element;   // +0x00
    GlobalNode* prev;      // +0x08  never read by 0x18005f1f0
    GlobalNode* next;      // +0x10
};
static_assert(sizeof(GlobalNode) == 0x18, "LinkedList::Node(T=8) is 0x18 bytes");

// The list header the same body clears: {head, tail, count}.
struct GlobalList {
    GlobalNode* head;    // +0x00
    GlobalNode* tail;    // +0x08
    int32_t     count;   // +0x10
    int32_t     pad14;   // +0x14  never read or written
};
static_assert(sizeof(GlobalList) == 0x18, "Collection::LinkedList is 0x18 bytes");

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

// 0x18005f1f0 — walk the global list freeing every node under the allocator
// lock, then clear {head, tail, count}. Scene::~Scene's first call.
void FreeGlobals(GlobalList* list);

// ---------------------------------------------------------------------------
// Probe scheduling — the same contract array_internal.h and picking's grid use,
// and for the same reason: the probe ALLOCATES, and a probe that allocates
// cannot run from a *PortInit (WAVE9.md, lane PKG2's `null+0x640` crash — the
// allocator's critical section taken before the allocator exists). So the init
// only ARMS this flag and the first call into any registered body runs it, at
// a moment that is provably safe because the caller is itself building a Scene.
// ---------------------------------------------------------------------------
extern bool g_probePending;

}  // namespace rtgfxgrid

// Runs the lockstep synthetic-subject oracle. Driven by g_probePending.
void RtGraphicsGridProbe();
