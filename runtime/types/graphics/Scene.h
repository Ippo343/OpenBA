#pragma once
// ============================================================================
// runtime/types/graphics/Scene.h — Graphics::Scene binary layout
// ============================================================================
//
// Wave 6 lane D of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2
// wave 6, §6 step 3).
//
// `Graphics::Scene` is NOT polymorphic: the export directory of the shipped
// Runtime-x64.dll contains no `??_7Scene@Graphics@@6B@`, its ctor stores no
// vtable, and every one of its exported members is `QEAA`/`QEBA` (non-virtual)
// or `AEBA` (private non-virtual). It is a quadtree-style hash grid of
// `Graphics::Geometry*` with a separate doubly linked list of "global" objects
// that skip the grid and are always visited.
//
// The grid half of the object is shared with other classes: the ctor body,
// the insert, the unthread and the re-index are all `FUN_`-named internals that
// several containers call. This lane calls those at their retail addresses
// rather than re-deriving them, exactly as Wave 5 lane D did for
// `Picking::Scene` (runtime/src/picking/scene.cpp) — see the banner in
// runtime/src/graphics/scene.cpp for the argument.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"       // Collection::Array, Collection::LinkedList
#include "Geometry.h"

namespace RtGraphics {

// ============================================================
// Graphics::Scene::Node — 0x18 bytes, the grid's per-object record.
//
// Pinned by three bodies that agree term for term:
//   Scene::RemoveObject (0x180102600) indexes `nodes + id*0x18`, zeroes the
//     first two qwords and writes -1 at +0x10.
//   Scene::Clean (0x1801023f0) skips a node whose `*(int*)(node+0x10)` is
//     negative and otherwise reads the Geometry* at +0x00.
//   FUN_18005f090 (the grid unthread) uses `*(int*)(node+0x10)` as an index
//     into the cell-head array at Scene+0x30, and splices the node out of that
//     cell's chain through the two int32 links at +0x08 (next) and +0x0c (prev).
//
// +0x14 IS A DON'T-CARE, AND RETAIL WRITES GARBAGE INTO IT.
// Both RemoveObject and the grid insert FUN_18005e5e0 build the `{-1, ?}` pair
// on the stack as `CONCAT44(uStack_14, 0xffffffff)` and store it as ONE qword —
// where `uStack_14` is an uninitialised local. Read out of the disassembly at
// 0x180102639: `MOV dword ptr [RSP+0x30], 0xffffffff` then
// `MOV RAX, qword ptr [RSP+0x30]`, with nothing ever writing [RSP+0x34]. This is
// the same shape as the shipped-DLL uninitialised read Wave 5 found in
// Picking::Scene::RemoveMesh (reference_shadow_predicate_must_name_its_failing_term.md).
// Consequence for this lane: no oracle may compare +0x14, and our port must not
// pretend to a value retail does not have.
// ============================================================
struct SceneNode {
    Geometry* _object;    // +0x00
    int32_t   _next;      // +0x08  next node index in this cell's chain
    int32_t   _prev;      // +0x0c  previous node index
    int32_t   _cell;      // +0x10  cell index; < 0 means the slot is free
    int32_t   _pad14;     // +0x14  DON'T CARE — retail stores uninitialised stack here
};
static_assert(sizeof(SceneNode) == 0x18, "Graphics::Scene::Node must be 0x18 bytes");
static_assert(offsetof(SceneNode, _next) == 0x08, "SceneNode::_next @ +0x08");
static_assert(offsetof(SceneNode, _cell) == 0x10, "SceneNode::_cell @ +0x10");

// ============================================================
// Graphics::Scene — 0x78 bytes.
//
// Whole-object witness: ??0Scene@Graphics@@QEAA@XZ (0x180102730) is
// `FUN_18005e4a0(this, 2048.0f); _globalHead = 0; _globalTail = 0;
//  _globalCount = 0;` — and the grid initialiser FUN_18005e4a0 writes every
// field from +0x00 to +0x5f. The destructor (0x1801026c0) then names the five
// containers by freeing them in reverse: the global list at +0x60, then the
// Arrays at +0x50, +0x40, +0x30, +0x20, then the node Array at +0x10.
//
//   +0x00  _cellSize        FUN_18005e4a0's float argument, from
//                           DAT_180161730 = 0x45000000 = 2048.0f (bytes read out
//                           of the shipped DLL, not a Ghidra rendering).
//   +0x04  _halfCellX       both set to _cellSize * 0.5f, written as one qword
//   +0x08  _halfCellY       (`CONCAT44(p*0.5f, p*0.5f)`).
//   +0x0c  _levelCount      8. FUN_18005ee30 compares the object's chosen level
//                           against it before clamping.
//   +0x10  _nodes           Array<Node>: Clean reads the count at +0x18,
//                           RemoveObject indexes the buffer at +0x10.
//   +0x20  _freeIds         Array<uint32>: RemoveObject appends the freed id
//                           through FUN_180023830 (the stride-4 SetSize) and
//                           the insert FUN_18005e5e0 pops the last entry.
//   +0x30  _cellHeads       Array<int32>, 0x5555 entries, filled with -1 by the
//                           ctor. FUN_18005f090 indexes it by node->_cell.
//   +0x40  _levelOffsets    Array<int32>, 8 entries, seeded
//                           {0, 1, 5, 0x15, 0x55, 0x155, 0x555, 0x1555} —
//                           the quadtree level base indices (4^n prefix sums).
//   +0x50  _cellCounts      Array<int32>, 0x5555 entries, filled with 0.
//   +0x60  _globals         LinkedList<Geometry*>: head +0x60, tail +0x68,
//                           count +0x70. AddGlobalObject (0x180102540)
//                           allocates a 0x18-byte node {element, prev, next} and
//                           links it at the tail; RemoveGlobalObject
//                           (0x1801024c0) unlinks and frees it. That is exactly
//                           the Collection::LinkedList shape recovered in Wave 2
//                           (runtime/types/collection/Collection.h).
//   +0x74  padding, never touched (the LinkedList's own _pad14).
// ============================================================
struct Scene {
    float                          _cellSize;      // +0x00
    float                          _halfCellX;     // +0x04
    float                          _halfCellY;     // +0x08
    int32_t                        _levelCount;    // +0x0c
    Collection::Array<SceneNode>   _nodes;         // +0x10 {data, size@0x18, cap@0x1c}
    Collection::Array<uint32_t>    _freeIds;       // +0x20
    Collection::Array<int32_t>     _cellHeads;     // +0x30
    Collection::Array<int32_t>     _levelOffsets;  // +0x40
    Collection::Array<int32_t>     _cellCounts;    // +0x50
    Collection::LinkedList<Geometry*> _globals;    // +0x60 {head, tail@0x68, count@0x70}
};

static_assert(sizeof(Scene) == 0x78, "Graphics::Scene must be 0x78 bytes");
static_assert(offsetof(Scene, _levelCount)   == 0x0c, "Scene::_levelCount @ +0x0c");
static_assert(offsetof(Scene, _nodes)        == 0x10, "Scene::_nodes @ +0x10");
static_assert(offsetof(Scene, _freeIds)      == 0x20, "Scene::_freeIds @ +0x20");
static_assert(offsetof(Scene, _cellHeads)    == 0x30, "Scene::_cellHeads @ +0x30");
static_assert(offsetof(Scene, _levelOffsets) == 0x40, "Scene::_levelOffsets @ +0x40");
static_assert(offsetof(Scene, _cellCounts)   == 0x50, "Scene::_cellCounts @ +0x50");
static_assert(offsetof(Scene, _globals)      == 0x60, "Scene::_globals @ +0x60");

// The grid's cell-head / cell-count arrays are both this many entries. Written
// as a literal because the ctor's fill loop is bounded by 0x15554 bytes, i.e.
// 0x5555 int32s (FUN_18005e4a0's `while (lVar1 < 0x15554)`).
constexpr int kSceneCellCount = 0x5555;

// The node index type AddObject returns and RemoveObject takes.
using SceneObjectId = uint32_t;

}  // namespace RtGraphics
