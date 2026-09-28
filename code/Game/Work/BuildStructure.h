#pragma once
// ============================================================================
// code/Game/Work/BuildStructure.h — ENDGAME B.3.8 block WRK
//
// BuildStructure — the construction-labour work of a building site: one byte per
// build position (free / claimed) and a BST of the citizens holding one. A REAL
// class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "BuildStructure", parent "Work", ti 0x1401c6898,
//   create 0x140029630 (MemAlloc(0x70) + ctor 0x1400e5a90); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x1401610a0, 13 slots:
//   [0] 0x140029680 scalar deleting dtor (the dtor 0x1400e5920 + MemFree on &1)
//   [1] 0x140029620 TypeName  [2] 0x140029610 Type  [3] 0x1400e5850 Persist
//   [4] 0x1400e4fc0 Execute   [5] = Work (0x1400f7540)  [6] 0x1400e4f80 OnComplete
//   [7] 0x1400e4f40 OnCancel  [8] 0x1400e57a0 Cleanup   [9] 0x1400e4be0 Validate
//   [10] 0x1400e5830 GetTarget  [11] 0x140029670 GetPriority  [12] = Work (0x140006b10)
//   ctors: 0x1400e5a90 (default) and 0x1400e59c0 (entity + alt-anim flag; the one
//   BuildComponent's state-8 enter uses).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"


class BuildStructure final : public Work {
    DECLARE_EXE_TYPE(BuildStructure)

public:
    BuildStructure();                                               // 0x1400e5a90
    BuildStructure(void* entityIBP, uint8_t altAnim);               // 0x1400e59c0

    // 0x1400e5920: frees the tree and the array
    ~BuildStructure() override;                                     // [0]  0x140029680
    void  Persist(void* stream) override;                           // [3]  0x1400e5850
    int   Execute(void* citizenPtr, void* context) override;  // [4]  0x1400e4fc0
    int   OnComplete(void* citizenIBP, void* unused, int slotIndex) override;  // [6]  0x1400e4f80
    int   OnCancel(void* citizenIBP, void* unused, int slotIndex) override;    // [7]  0x1400e4f40
    int   Cleanup() override;                                       // [8]  0x1400e57a0
    int   Validate() override;                      // [9]  0x1400e4be0
    void* GetTarget(void* retBuf) override;                         // [10] 0x1400e5830
    int   GetPriority() override;                                   // [11] 0x140029670

    // --- BuildStructure fields (+0x48) ---
    // Note: ctor doesn't init +0x48/+0x4C (set by Serialize or caller)
    uint8_t  field_48;           // +0x48: bool — serialized flag (has initial setup?)
    uint8_t  _pad49[3];          // +0x49: padding
    int32_t  field_4c;           // +0x4C: current slot index (round-robin for assignment)
    void*    field_50;           // +0x50: slot availability array (byte per build point, 0=free 1=busy)
    int32_t  field_58;           // +0x58: slot count (number of build points)
    int32_t  field_5c;           // +0x5C: capacity of the +0x50 array (the ctors zero it)
    void*    field_60;           // +0x60: binary tree root (citizen assignment tree, nodes 0x20 bytes)
    int32_t  field_68;           // +0x68: priority counter (GetPriority returns field_68 > 0)
    uint8_t  _pad6c[4];          // +0x6C: padding
};

static_assert(sizeof(BuildStructure) == 0x70, "BuildStructure is 0x70 (retail create MemAlloc)");
static_assert(alignof(BuildStructure) == 8, "BuildStructure is 8-aligned");
static_assert(offsetof(BuildStructure, field_48) == 0x48, "BuildStructure +0x48");
static_assert(offsetof(BuildStructure, field_4c) == 0x4c, "BuildStructure +0x4c");
static_assert(offsetof(BuildStructure, field_50) == 0x50, "BuildStructure +0x50");
static_assert(offsetof(BuildStructure, field_58) == 0x58, "BuildStructure +0x58");
static_assert(offsetof(BuildStructure, field_5c) == 0x5c, "BuildStructure +0x5c");
static_assert(offsetof(BuildStructure, field_60) == 0x60, "BuildStructure +0x60");
static_assert(offsetof(BuildStructure, field_68) == 0x68, "BuildStructure +0x68");
