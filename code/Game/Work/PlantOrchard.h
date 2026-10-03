#pragma once
// ============================================================================
// code/Game/Work/PlantOrchard.h — ENDGAME B.3.8 block WRK
//
// PlantOrchard — the orchard planting work: a BST of the slots being planted
// (+0x48, count +0x50), a per-orchard-slot byte array (+0x58 / +0x60 / +0x64) and
// the tree type chosen in the orchard UI (ExternalPtr +0x68). A REAL class with a
// compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "PlantOrchard", parent "Work", ti 0x1401c6c50,
//   create 0x14002c500 (MemAlloc(0x70) + ctor 0x1400f50b0); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161910, 13 slots:
//   [0] 0x14002c540 scalar deleting dtor (the dtor 0x1400f5010 + MemFree on &1)
//   [1] TypeName  [2] Type  [3] 0x1400f4f50 Persist  [4] 0x1400f4af0 Execute
//   [5] = Work (0x1400f7540)  [6] 0x1400f4a90 OnComplete  [7] 0x1400f4a20 OnCancel
//   [8] 0x1400f4e70 Cleanup  [9] 0x1400f47f0 Validate  [10] 0x1400f4ef0 GetTarget
//   [11] 0x140029860 GetPriority (`+0x50 > 0`, shared by 13 tables)
//   [12] = Work (0x140006b10)
//   ctors: the default one 0x1400f50b0 (deserialize/factory, +0x68 null) and the
//   from-entity one 0x1400f5140 (plantorchard_ctor.cpp) that OrchardComponent's
//   states 9 and 12 (orchardcomponent_states.cpp, block MI-B) reach through the door
//   PlantOrchard_CtorFromEntity. Nothing outside this block reads 0x140161910 back.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

class ComponentDescription;


class PlantOrchard final : public Work {
    DECLARE_EXE_TYPE(PlantOrchard)

public:
    PlantOrchard();                                                 // 0x1400f50b0
    PlantOrchard(void** entityIBP, ComponentDescription* treeType);   // 0x1400f5140

    // 0x1400f5010: frees the slot array and the BST
    ~PlantOrchard() override;                                       // [0]  0x14002c540
    void  Persist(IO::Stream& stream) override;                           // [3]  0x1400f4f50
    int   Execute(void* entityIBP, void* context) override;  // [4]  0x1400f4af0
    int   OnComplete(void* param2, void* param3, int param4) override;  // [6]  0x1400f4a90
    int   OnCancel(void* param2, void* param3, int param4) override;    // [7]  0x1400f4a20
    int   Cleanup() override;                                       // [8]  0x1400f4e70
    int   Validate() override;                          // [9]  0x1400f47f0
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400f4ef0
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- PlantOrchard fields (+0x48) ---
    WorkCellSetNode* field_48;   // +0x48: the cell -> citizen set (0x140029a40 / 0x14001ca60 / 0x140029de0)
    int32_t  field_50;           // +0x50: its count (0x140029860 `CMP [RCX+0x50]; SETG`: signed)
    uint8_t  _pad54[4];          // +0x54: padding
    // +0x58/+0x60/+0x64 are ONE dynamic-array triple {base, count, capacity} — the same
    // shape as OrchardComponent's +0x50/+0x58/+0x5c. 0x14000a590 (the resizer the ctor
    // calls) reads capacity at +0x0c of the triple, so +0x64 is NOT padding. Persist
    // streams it through the bool-array overload (0x140151458): 1 = planted.
    bool*    field_58;           // +0x58: validity array base (one bool per orchard slot)
    int32_t  field_60;           // +0x60: element count (= OrchardComponent+0x58 slot count)
    int32_t  field_64;           // +0x64: capacity (was mislabelled padding)
    ComponentDescription* field_68;   // +0x68: ExternalPtr<ComponentDescription> (0x140002340): the
                                      //        TREE TYPE template chosen in OrchardUI's combo (copied
                                      //        from OrchardComponent+0x60). NOT the OrchardDescription.
};

static_assert(sizeof(PlantOrchard) == 0x70, "PlantOrchard is 0x70 (retail create MemAlloc)");
static_assert(alignof(PlantOrchard) == 8, "PlantOrchard is 8-aligned");
static_assert(offsetof(PlantOrchard, field_48) == 0x48, "PlantOrchard +0x48");
static_assert(offsetof(PlantOrchard, field_50) == 0x50, "PlantOrchard +0x50");
static_assert(offsetof(PlantOrchard, field_58) == 0x58, "PlantOrchard +0x58");
static_assert(offsetof(PlantOrchard, field_60) == 0x60, "PlantOrchard +0x60");
static_assert(offsetof(PlantOrchard, field_64) == 0x64, "PlantOrchard +0x64");
static_assert(offsetof(PlantOrchard, field_68) == 0x68, "PlantOrchard +0x68");
