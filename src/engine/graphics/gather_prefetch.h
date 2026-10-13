// ============================================================================
// src/engine/graphics/gather_prefetch.h -- GATHERPF (NOT RETAIL; faithful perf
// work, hints only): the prefetch-ahead switches of the faithful scene gather
// `Graphics::Scene::AddCulledDrawItems` (cull.cpp), and the gather's counters
// for the stress harness's COMPPROF `gather` row.
// ============================================================================
//
// docs/extensions/handins/GATHERPF.md has the cache-line table these arms are
// derived from, the A/B protocol and the predictions.
//
// Every arm issues `__builtin_prefetch` only (prefetcht0: no architectural
// effect, never faults). The only loads an arm adds are EARLIER copies of loads
// the faithful walk performs on the same object later in the same call (the
// next chain node's `_object` / `_next`, the next visible list entry, that
// entry's vptr, a MeshInstance's `_materials._data`); each is listed beside its
// arm in cull.cpp. No arm reorders, skips or guards a faithful load or store,
// so the visible list, its order and every draw item are unchanged: there is
// no shadow to run.
//
// The switches are `static constexpr`: `if constexpr` arms, so a `false` arm
// compiles to nothing. No flag, no Opt() field, no settings key. To A/B an arm
// flip its line and rebuild the perf exe (only cull.cpp recompiles); count the
// prefetch instructions with
//   objdump -d build/cmake-perf/CMakeFiles/reimpl-x64.dir/src/engine/graphics/cull.cpp.obj | grep -c prefetch
// against the hand-in's table before running.
//
// When ext.draw_gather is ON the faithful body is not reached at all (its seam
// returns at the top of AddCulledDrawItems), so none of this runs and the
// counters stay 0.
// ============================================================================
#pragma once

#include <cstdint>

namespace Graphics {
namespace GatherPf {

// ---- arm 1: the cell chain walk (default ON) --------------------------------
// While object i of a cell's ring is tested, load node i+S (`_object`, `_next`:
// the walk reads both on its next visit), prefetch that Geometry's cull lines
// (+0x28 `_detailId` .. +0x63 `_worldBounds.max.z`: one or two lines), and
// prefetch node i+S+1's SceneNode line. The ring gives the next node for free;
// S > 1 hops the ring S-1 more times (loads of nodes the walk visits anyway).
inline constexpr bool kChain = true;
// PREFETCH (the tick walk) measured stride 1 best and every farther stride worse
// (+3/+4/+8% at 2/4/8, PREFETCH.md section 9.2). Kept as a constant so a sweep is
// repeatable, not as a tuning knob.
inline constexpr int  kChainStride = 1;

// ---- arm 2: the visible-list dispatch (default ON) --------------------------
// The visible list is an array of Geometry pointers, so lookahead costs no
// pointer chase. Two stages, one dispatch apart:
//   i+2S  the vptr line (+0x00) and the line at +0xc0 (`geo + 1`, a
//         MeshInstance's _mesh / _materials / the head of _animation);
//   i+S   by dynamic type (the vptr, resident since i+2S, compared with the
//         compiler's MeshInstance vtable): +0x80 (_opacityPrev), +0xd8
//         (_animation), +0x11c (_sortBias).
inline constexpr bool kDispatch = true;
inline constexpr int  kDispatchStride = 1;
// Sub-arm of 2 (needs kDispatch): at i+S also load a MeshInstance's
// `_materials._data` (the line was prefetched at i+2S) and prefetch the first
// line of that per-instance array, which AddDrawItems indexes for every
// triangle list. A dependent line behind a pointer in the object: PREFETCHCOV's
// winning arm P had this shape.
inline constexpr bool kDispatchMaterials = true;

// ---- arm 3: the cell head slot (default OFF) --------------------------------
// After the faithful `_cellCounts[cell] >= 1` test and BEFORE the node-box
// frustum test, prefetch `&_cellHeads[cell]` (prefetch only; the head itself is
// read by the walk after the box test, so no new load of the array). OFF: the
// two 0x5555-entry arrays are 87 KB each and stay L2-resident across the ~4
// walks per frame, four sibling cells share one line, and the out-of-order core
// already issues the head load early whenever the box branch is predicted.
inline constexpr bool kCellHead = false;

// ---- not built (reasons in the hand-in) -------------------------------------
// * the draw-item array write side (Renderer::AddDrawItem / DrawItemArrayResize):
//   IBS_20261013 measured 0.78% / 0.05% L1 miss there: a sequential append the
//   hardware stream prefetcher already covers;
// * the cull stack (CullStackPushChildren): a 9 KB local array, always in L1.

}  // namespace GatherPf
}  // namespace Graphics

// ---- the counters (cull.cpp; COMPPROF >= 1 prints them as the `gather` row) --
// Main thread only, reset by the harness per window. Cells and chain objects are
// one `addq $1, mem` each in the walk (a local counter held a register across the
// walk and added 16 spill / reload instructions; the memory add adds none);
// `visible` is added once per call; draw items once per append in AddDrawItem.
extern uint64_t g_gatherCells;        // cells whose ring was walked
extern uint64_t g_gatherChainObjects; // ring objects visited (= Geometry headers read)
extern uint64_t g_gatherVisible;      // visible-list entries dispatched (slot 3 calls)
extern uint64_t g_gatherDrawItems;    // draw items appended to the current group's three
                                      // pass lists during the dispatch phase
