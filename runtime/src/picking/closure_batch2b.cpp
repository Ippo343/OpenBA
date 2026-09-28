// ============================================================================
// runtime/src/picking/closure_batch2b.cpp — cutover closure batch 2b
// ============================================================================
//
//   0x18007aef0  Picking_Scene_InitGrid   (unexported, 14 stores + one call)
//
// The shared grid initialiser `??0Scene@Picking@@QEAA@XZ` (0x1801258e0) calls
// with a cell size of 16.0f. Our port of that constructor
// (runtime/src/picking/scene.cpp, `RtPicking_Scene_Ctor`) used to INLINE it (it calls this now), which
// was the right call there — a forwarder would have had nothing to diff — but
// it left the shared body itself unported, so the closure still reaches
// 0x18007aef0 and, at cutover, ResolveAddr would return nothing for it.
//
// This is that body, registered at its own address. It is the SAME fourteen
// stores scene.cpp transcribes; the two are kept in step by both writing
// through `Picking::Scene`'s named fields rather than raw offsets, so a
// layout change breaks the header's static_asserts and not one of the two
// copies silently.
//
// THE BRIEF CALLED THIS "a Frustum/Camera-shaped ctor" AND ASKED FOR A NEW TYPE
// HEADER. It is neither: `runtime/types/picking/Scene.h` already pins every one
// of these fields, whole-object, from this very body (its "+0x2c and +0x44 are
// never written by the ctor" note is read off the same fourteen stores). No new
// header is needed and writing one would be a second, competing witness.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../../types/picking/Scene.h"
#include "../collection/array_internal.h"

namespace {

// `DAT_18016169c` — the numerator of the reciprocal. 1.0f, the same constant
// scene.cpp's `kOne` names.
const float kOne = 1.0f;

}  // namespace

// ============================================================================
// Store order, exactly as retail writes it:
//
//   +0x00  _cellSize    = cellSize          (the argument, in XMM1)
//   +0x04  _invCellSize = 1.0f / cellSize   (a real DIVSS, not a folded
//                                            reciprocal — a caller passing 0
//                                            gets +inf here, faithfully)
//   +0x08  _cellHeads   {0, 0, 0}
//   +0x18  _cells       {0, 0, 0}
//   +0x28  _freeCell    = -1
//   +0x2c               NOT WRITTEN
//   +0x30  _links       {0, 0, 0}
//   +0x40  _freeLink    = -1
//   +0x44               NOT WRITTEN
//   +0x48  _entries     {0, 0, 0}
//   +0x58  _freeEntry   = -1
//   +0x5c  _queryStamp  = 0
//   then CellHeadsInit(&_cellHeads) — which allocates the 10,000-entry bucket
//   table, so this function ALLOCATES and must not be called before the
//   MemoryAllocator is started.
//
// The two unwritten dwords are recorded, not zeroed: a `= {}` would be a
// behavioural difference from retail, and scene.cpp's oracle already excludes
// exactly those two offsets from its byte comparison.
// ============================================================================
// ENDGAME B.3.7 block B6: the body is `Picking::Scene::InitGrid` now, and the
// ctor door (scene.cpp) CALLS it with 16.0f exactly as retail's 0x1801258e0
// does — the second, inlined copy of these fourteen stores that scene.cpp used
// to carry is gone (B7-6). The registered door is a qualified delegation.
namespace Picking {

Scene* Scene::InitGrid(float cellSize) {
    _cellSize    = cellSize;
    _invCellSize = kOne / cellSize;

    _cellHeads._data     = nullptr;
    _cellHeads._size     = 0;
    _cellHeads._capacity = 0;

    _cells._data     = nullptr;
    _cells._size     = 0;
    _cells._capacity = 0;
    _freeCell        = -1;

    _links._data     = nullptr;
    _links._size     = 0;
    _links._capacity = 0;
    _freeLink        = -1;

    _entries._data     = nullptr;
    _entries._size     = 0;
    _entries._capacity = 0;
    _freeEntry         = -1;

    _queryStamp = 0;

    // ENDGAME B.3.2 step 2 (callers) — retail 0x18007b960, `Array<int32>::
    // SetSize(10000)` with every new slot filled -1 (both constants folded
    // into the body). NOT trait-equivalent (Collection.h's Array banner names
    // this exact row as one of the twelve a trait cannot express: a fill of
    // -1 is not what `SpecFor<int32_t>()` would leave the new tail holding),
    // so `_cellHeads` is bridged through the hand-derived row rather than
    // trusted to its own (default) spec.
    rtcoll::AsArray<&rtcoll::kSpec_I32Neg1>(&_cellHeads)->SetSize(10000);
    return this;   // MOV RAX,<this>
}

}  // namespace Picking

extern "C" Picking::Scene* RtPicking_Scene_InitGrid(void* selfv, float cellSize) {
    return reinterpret_cast<Picking::Scene*>(selfv)->Picking::Scene::InitGrid(cellSize);   // B.3.7 bridge
}
