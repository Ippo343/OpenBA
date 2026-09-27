#pragma once
// ============================================================================
// code/Game/Player/dragtool.h — ENDGAME B.3.8 TOOL (B38_RULES.md §2.1, D1-D5)
//
// `DragTool`, the abstract base of the click-and-drag tools (Bridge, Road,
// Removal, Fenced, DragPlacement), as a REAL C++ class with a compiler vtable.
// (This header is the class's, named after it; the file system is case-
// insensitive, so it keeps its existing lowercase spelling.)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §1, bytes read 2026-09-26):
//   reflected "DragTool", parent "Tool", ti 0x1401c6da0, create = 0x140003730
//   ctor   inlined into every leaf ctor (e.g. 0x1400fb890): the Tool part, then
//          stamp 0x140161c10; `+0x28 word = 0` (dragState, singleClick);
//          `+0x2c qword = -1`; `+0x34 dword = -1` (start/end/last cells)
//   table  0x140161c10, 15 slots:
//   [0]  0x14002da20 scalar deleting dtor: stamp 0x140161c10, stamp
//        0x140161f10, `~Internal`, MemFree on &1 — ~DragTool has no body
//   [1]  0x14002da10 TypeName   [2] 0x14002da00 Type
//   [3]-[5], [7], [8] = Tool's (Internal::Persist / pure)
//   [6]  0x1400fc6b0 OnKey — the shared drag lifecycle (dragtool.cpp)
//   [9]  0x1400fc640 OnCancel — cancel an active drag, 1 = keep the tool
//   [10], [11] = Tool's
//   [12] 0x1400fc9d0 `MOV EAX,5; RET` (cursor)
//   [13], [14] _purecall — CalcPath (commit) / CalcCost (preview), introduced
//
// DOORS (B38 D5 (b) rows): reg_code_lane6.cpp / reg_player_drag.cpp register
// the names `DragTool_OnKey` (0x1400fc6b0) and `DragTool_CancelDrag_Impl`
// (0x1400fc640), so those two stay as one-line doors onto the members, which
// hold the bodies (ENDGAME B.3.8 callers CB4).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Player/Tool.h"

class DragTool : public Tool {
public:
    // ---- the 15 slots, in retail order (declared here: the ones DragTool fills)
    Object::Base* DeletingDtor(unsigned flags) override;               // [0]  0x14002da20
    const char*   TypeName() const override;                           // [1]  0x14002da10
    uint32_t      Type() const override;                               // [2]  0x14002da00
    char          OnKey(char active) override;                         // [6]  0x1400fc6b0
    char          OnCancel() override;                                 // [9]  0x1400fc640
    int           GetCursor() const override { return 5; }             // [12] 0x1400fc9d0
    // [13] commit the drag between two cells; AL = success.
    virtual bool  CalcPath(int startCell, int endCell) = 0;            // [13] pure
    // [14] evaluate the drag: returns the segment count and fills four
    // out-values (DragTool::OnKey reads them as path type, tile H, tile W and
    // an extra word; each leaf names them after its own use).
    virtual int   CalcCost(int startCell, int endCell, uint32_t* out0,
                           int* out1, int* out2, int* out3) = 0;       // [14] pure

    // ---- the 0x10 bytes DragTool adds, retail order -------------------------
    uint8_t  dragState;     // +0x28 0 = idle, 1 = dragging
    uint8_t  singleClick;   // +0x29 1 = released on the start cell (confirm on next click)
    uint8_t  _pad2A[2];     // +0x2a never written by retail (zeroed here)
    int32_t  startCell;     // +0x2c -1 = invalid
    int32_t  endCell;       // +0x30 -1 = invalid
    int32_t  lastEndCell;   // +0x34 change detection for the tick sound

protected:
    DragTool() : dragState(0), singleClick(0), _pad2A{0, 0},
                 startCell(-1), endCell(-1), lastEndCell(-1) {}
    ~DragTool() = default;  // no body in retail
};

namespace DragToolAddr {
    constexpr uintptr_t Vtable = 0x140161c10;   // retail table (documentation)
}

static_assert(sizeof(DragTool) == 0x38, "DragTool is 0x38 bytes");
static_assert(alignof(DragTool) == 8, "DragTool is 8-aligned");
static_assert(offsetof(DragTool, dragState)   == 0x28, "DragTool +0x28 dragState (word store 0x1400fb8cb)");
static_assert(offsetof(DragTool, singleClick) == 0x29, "DragTool +0x29 singleClick");
static_assert(offsetof(DragTool, startCell)   == 0x2c, "DragTool +0x2c startCell (qword -1 at 0x1400fb8cf)");
static_assert(offsetof(DragTool, endCell)     == 0x30, "DragTool +0x30 endCell");
static_assert(offsetof(DragTool, lastEndCell) == 0x34, "DragTool +0x34 lastEndCell (dword -1 at 0x1400fb8d7)");

// The by-name doors onto the members (dragtool.cpp). OnKey (0x1400fc6b0) returns
// 1 always (event consumed); CancelDrag (0x1400fc640) returns 1 when it
// cancelled a drag (the tool stays), 0 when idle.
uint64_t DragTool_OnKey(DragTool* self, char key);
extern "C" int DragTool_CancelDrag_Impl(void* self);   // self = DragTool*
