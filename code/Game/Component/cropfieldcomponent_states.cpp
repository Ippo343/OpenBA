// code/Game/Component/cropfieldcomponent_states.cpp
// CropFieldComponent controller state machine.
//
// Structurally the Orchard's twin: a ControllerComponent (0x68) driving a drag-created
// area through plant / tend / harvest, with its own private state table. Where the
// orchard has six states (2/9/10/12/11/20) the crop field has FOUR bodies (2/9/10/11)
// plus two transition-only targets (3 and 21).
//
// ── How this table was recovered ──────────────────────────────────────────────────
// CropFieldDescription_Setup (0x14014edb0) tail-calls the populator. Ghidra decompiles
// that call as `TownHallUI_Ctor()` and names the whole helper family `TownHallUI_*`,
// which is WRONG — the disassembly is unambiguous:
//     14014edb4: CALL 0x14011b150
//     14014edb9: MOV dword ptr [0x1401d7ad8],EAX
// So the real populator is 0x14011b150, the state array is 0x1401d6b50 and the count is
// 0x1401d7ad0. (0x1401d7ad8, the thing Setup writes, is a separate "registered ok" flag
// — do not mistake it for the count.) Every handler address below was read from that
// function's DISASSEMBLY operands, not from Ghidra's symbol names.
//
// ── Register-helper argument order (0x1400330d0), verified from its own disassembly ──
// It stages the new entry in a stack buffer at RSP+0x30 and memcpy-style installs it:
//     [RSP+0x30] = ECX          -> +0x00 id
//     [RSP+0x38] = RDX          -> +0x08 enter
//     [RSP+0x40] = 0            -> +0x10 trans30   (always zero for this controller)
//     [RSP+0x48] = [RSP+0x170]  -> +0x18 trans38   (the 5th/stack argument)
//     [RSP+0x50] = R8           -> +0x20 update
// i.e. the shape is sub_330d0(id, enter, UPDATE, _, TRANS38) — update and trans38 are
// NOT in source order, and reading them the other way round would silently swap the
// per-frame handler with the update handler. Companions: 0x1400331f0 adds a transition,
// 0x1400332a0 adds an event.
//
// ── The recovered table ───────────────────────────────────────────────────────────
//   state 2   enter 0x14011a920  trans38 0x14011a860   -> 3, 9        event 3 -> 0x14011a7b0
//   state 9   enter 0x14011a310  trans38 0x14011a1e0   -> 3, 10
//   state 10  enter 0x14011a100  trans38 0x140119fd0
//                                update  0x140119f70   -> 3, 21, 11   event 4 -> 0x140119f00
//   state 11  enter 0x140119e60  trans38 0x140119d80   -> 3, 21
// States 3 and 21 are transition TARGETS with no registered body — exactly as the
// orchard table leaves its state 3 — so they are deliberately absent here too.
//
// ⚠ FIRST PASS: the handler BODIES are not ported yet, so every handler is registered
// as nullptr. ControllerLive::Dispatcher treats a null handler as a graceful no-op, so
// the state machine still runs the REAL structure — Step returns real indices and
// transitions are validated against the real graph — it simply performs no per-state
// work. This is the same staged approach the orchard table went through, and it makes
// the component observable (BANISHED_FOOD_DIAG reports its state) before any behaviour
// is added. Do NOT read "crop field does nothing" as a defect until the bodies land.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace { struct CropFieldCtrlTag {}; }
using CropFieldDispatch = ::ControllerLive::Dispatcher<CropFieldCtrlTag>;

extern "C" void** CropFieldCtrl_NodeVtable() { return CropFieldDispatch::Vtable(); }

// Replica of the populator at 0x14011b150, in registration order. Order matters: it
// fixes the state INDEX that StateMachine::Tick carries in its +0x09/+0x0a/+0x0b bytes.
extern "C" void CropFieldCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = CropFieldDispatch::table;
    if (t.count != 0) return;                       // idempotent — runs per instance

    // ── state 2 — DEFAULT / idle ──────────────────────────────────────────────────
    // Slot 16 (GetDefaultState) is the shared 0x1400026c0 = `MOV EAX,2; RET`, so this
    // is where a new field starts. enter 0x14011a920 / trans38 0x14011a860; the event-3
    // body 0x14011a7b0 is the panel's plant command, mirroring the orchard's event 3.
    TableRegisterState(t, 2, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 9,  nullptr);
    TableAddEvent(t, 3, nullptr);

    // ── state 9 — PLANT ───────────────────────────────────────────────────────────
    // enter 0x14011a310 / trans38 0x14011a1e0.
    TableRegisterState(t, 9, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 10, nullptr);

    // ── state 10 — TEND / GROW ────────────────────────────────────────────────────
    // The only state with an UPDATE handler (0x140119f70) as well as a per-frame
    // trans38 (0x140119fd0); enter 0x14011a100. Event 4 (0x140119f00) is the harvest
    // command, and 21 is the clear/cut target.
    TableRegisterState(t, 10, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 21, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddEvent(t, 4, nullptr);

    // ── state 11 — HARVEST ────────────────────────────────────────────────────────
    // enter 0x140119e60 / trans38 0x140119d80.
    TableRegisterState(t, 11, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 21, nullptr);
}
