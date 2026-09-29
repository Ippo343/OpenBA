#pragma once
// ============================================================================
// code/Game/Game/WorkScheduler.h — ENDGAME B.4 GAME-a
//
// The 0x138-byte work scheduler at GameUpdateLoop+0x118 (the member the tree
// calls m_tutorial): every posted Work item by id, the per-profession work
// trees the citizens' job search walks, and the scheduler clock the Work
// slots are handed (+0x130). NOT the Tutorial resource (Tutorial/Tutorial.h).
//
// CLASS NAME UNCONFIRMED: retail gives it no reflected name or vtable;
// "WorkScheduler" is descriptive (Work.h calls it the scheduler).
// No vtable: a plain struct.
//
// RETAIL (Application-x64.exe):
//   ctor 0x1400f9180: +0x00 = 0 (qword), +0x08 = 1, +0x10 = 0, +0x18 = 0,
//     +0x20 = 0, +0x28 = 0; the vector ctor over 15 x 0x10 trees at +0x30
//     (element ctor MainMenuDialog_helper_cab0); +0x120 = 0, +0x128 = 0, +0x130 = 0
//   Serialize 0x1400f9240 (version 3): the 14 / 15 trees at +0x30
//     (TradeDescription_impl_d060), the tree at +0x120 (TradeWork_impl_d5f0), then int +0x00, int
//     +0x04, uint +0x08; on load rebuilds the +0x10 tree (count +0x18) and
//     reads the +0x30 trees by key
//   CitizenAI's work walk 0x1400f7b60 / assign 0x1400f8620 hand the Work
//     slots &+0x130
//   dtor 0x1400f8ff0
//
// The first three dwords: the ctor's qword store at +0x00 zeroes +0x00 and
// +0x04, then +0x08 = 1 (0x1400f9180). The tree's Update increments +0x08 each
// simulation step and adds dt to +0x130; those two are named from that, the
// rest keep typed guess names.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/Value.h"   // System::Time (8 bytes, trivial)

// One {root, count} tree header of the +0x30 array (the vector ctor's element,
// 0x10 bytes).
struct WorkSchedulerTree {
    void*   root;          // +0x00 tree root (nodes: int key at +0x00)
    int32_t count;         // +0x08
    uint8_t _pad0C[4];     // +0x0C
};

static_assert(sizeof(WorkSchedulerTree) == 0x10, "WorkSchedulerTree is 0x10 (the vector ctor's stride)");
static_assert(offsetof(WorkSchedulerTree, root) == 0x00, "WorkSchedulerTree::root @ +0x00");
static_assert(offsetof(WorkSchedulerTree, count) == 0x08, "WorkSchedulerTree::count @ +0x08");
static_assert(offsetof(WorkSchedulerTree, _pad0C) == 0x0C, "WorkSchedulerTree +0x0C");

class WorkScheduler {
public:
    int32_t           field_0x00;       // +0x00 (Serialize int)
    int32_t           field_0x04;       // +0x04 (Serialize int)
    uint32_t          stepCount;        // +0x08 ctor 1 (Serialize uint); +1 per simulation step
    uint8_t           _pad0C[4];        // +0x0C
    void*             field_0x10;       // +0x10 tree root (int key; rebuilt on load)
    int32_t           field_0x18;       // +0x18 tree count
    uint8_t           _pad1C[4];        // +0x1C
    void*             field_0x20;       // +0x20 ctor 0
    int32_t           field_0x28;       // +0x28 ctor 0
    uint8_t           _pad2C[4];        // +0x2C
    WorkSchedulerTree trees[15];        // +0x30 (TradeDescription_impl_d060: 14 before version 3)
    void*             field_0x120;      // +0x120 tree root (TradeWork_impl_d5f0)
    int32_t           field_0x128;      // +0x128 tree count
    uint8_t           _pad12C[4];       // +0x12C
    System::Time      time;             // +0x130 the scheduler clock (+= dt per step)
};

static_assert(sizeof(WorkScheduler) == 0x138, "WorkScheduler is 0x138 (Load MemAlloc(0x138, 4))");
static_assert(offsetof(WorkScheduler, field_0x00) == 0x00, "WorkScheduler +0x00");
static_assert(offsetof(WorkScheduler, field_0x04) == 0x04, "WorkScheduler +0x04");
static_assert(offsetof(WorkScheduler, stepCount) == 0x08, "WorkScheduler::stepCount @ +0x08");
static_assert(offsetof(WorkScheduler, _pad0C) == 0x0C, "WorkScheduler +0x0C");
static_assert(offsetof(WorkScheduler, field_0x10) == 0x10, "WorkScheduler +0x10");
static_assert(offsetof(WorkScheduler, field_0x18) == 0x18, "WorkScheduler +0x18");
static_assert(offsetof(WorkScheduler, _pad1C) == 0x1C, "WorkScheduler +0x1C");
static_assert(offsetof(WorkScheduler, field_0x20) == 0x20, "WorkScheduler +0x20");
static_assert(offsetof(WorkScheduler, field_0x28) == 0x28, "WorkScheduler +0x28");
static_assert(offsetof(WorkScheduler, _pad2C) == 0x2C, "WorkScheduler +0x2C");
static_assert(offsetof(WorkScheduler, trees) == 0x30, "WorkScheduler::trees @ +0x30");
static_assert(offsetof(WorkScheduler, field_0x120) == 0x120, "WorkScheduler +0x120");
static_assert(offsetof(WorkScheduler, field_0x128) == 0x128, "WorkScheduler +0x128");
static_assert(offsetof(WorkScheduler, _pad12C) == 0x12C, "WorkScheduler +0x12C");
static_assert(offsetof(WorkScheduler, time) == 0x130, "WorkScheduler::time @ +0x130");
