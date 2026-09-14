// ============================================================================
// runtime/src/picking/closure_batch2b.cpp — cutover closure batch 2b
// ============================================================================
//
//   0x18007aef0  Picking_Scene_InitGrid   (unexported, 14 stores + one call)
//
// The shared grid initialiser `??0Scene@Picking@@QEAA@XZ` (0x1801258e0) calls
// with a cell size of 16.0f. Our port of that constructor
// (runtime/src/picking/scene.cpp:494, `RtPicking_Scene_Ctor`) INLINES it, which
// was the right call there — a forwarder would have had nothing to diff — but
// it left the shared body itself unported, so the closure still reaches
// 0x18007aef0 and, at cutover, ResolveAddr would return nothing for it.
//
// This is that body, registered at its own address. It is the SAME fourteen
// stores scene.cpp transcribes; the two are kept in step by both writing
// through `RtPicking::Scene`'s named fields rather than raw offsets, so a
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

#include "../../funcs/rt_port.h"
#include "../../types/picking/Scene.h"
#include "../collection/array_internal.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

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
extern "C" RtPicking::Scene* RtPicking_Scene_InitGrid(RtPicking::Scene* self,
                                                      float cellSize) {
    self->_cellSize    = cellSize;
    self->_invCellSize = kOne / cellSize;

    self->_cellHeads._data     = nullptr;
    self->_cellHeads._size     = 0;
    self->_cellHeads._capacity = 0;

    self->_cells._data     = nullptr;
    self->_cells._size     = 0;
    self->_cells._capacity = 0;
    self->_freeCell        = -1;

    self->_links._data     = nullptr;
    self->_links._size     = 0;
    self->_links._capacity = 0;
    self->_freeLink        = -1;

    self->_entries._data     = nullptr;
    self->_entries._size     = 0;
    self->_entries._capacity = 0;
    self->_freeEntry         = -1;

    self->_queryStamp = 0;

    // ENDGAME B.3.2 step 2 (callers) — retail 0x18007b960, `Array<int32>::
    // SetSize(10000)` with every new slot filled -1 (both constants folded
    // into the body). NOT trait-equivalent (Collection.h's Array banner names
    // this exact row as one of the twelve a trait cannot express: a fill of
    // -1 is not what `SpecFor<int32_t>()` would leave the new tail holding),
    // so `_cellHeads` (a real `Collection::Array<int32_t>` member,
    // runtime/types/picking/Scene.h) is bridged through the hand-derived row
    // rather than trusted to its own (default) spec.
    rtcoll::AsArray<&rtcoll::kSpec_I32Neg1>(&self->_cellHeads)->SetSize(10000);
    return self;   // MOV RAX,<this>
}

int RtClosure2bPickingRegister() {
    RegisterRuntimeAddr(0x18007aef0, (void*)&RtPicking_Scene_InitGrid);
    return 1;
}
