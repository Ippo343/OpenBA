#pragma once
// ============================================================================
// code/Game/Work/PlantArea.h — ENDGAME B.3.8 block WRK
//
// PlantArea — the crop-field planting work: a BST of the cells being planted
// (+0x48, count +0x50), a per-cell validity byte array (+0x58 / +0x60) and the
// crop description (ExternalPtr +0x68). A REAL class with a compiler-generated
// vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "PlantArea", parent "Work", ti 0x1401c6c18,
//   create 0x14002c470 (MemAlloc(0x70) + ctor 0x1400f4770); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161890, 13 slots:
//   [0] 0x14002c4b0 scalar deleting dtor (the dtor 0x1400f4600 + MemFree on &1)
//   [1] TypeName  [2] Type  [3] 0x1400f4540 Persist  [4] 0x1400f4010 Execute
//   [5] = Work (0x1400f7540)  [6] 0x1400f3f60 OnComplete  [7] 0x1400f3ea0 OnCancel
//   [8] 0x1400f4460 Cleanup  [9] 0x1400f3c70 Validate  [10] 0x1400f44e0 GetTarget
//   [11] 0x140029860 GetPriority (`+0x50 > 0`, shared by 13 tables)
//   [12] = Work (0x140006b10)
//   CropFieldComponent's state machine (cropfieldcomponent_states.cpp, block MI-B)
//   builds a PlantArea BY HAND and stamps 0x140161890; that registration serves
//   PlantArea::Vtable() (B38 D5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

class ComponentDescription;

class PlantArea final : public Work {
    DECLARE_EXE_TYPE(PlantArea)

public:
    PlantArea();                                                    // 0x1400f4770

    // 0x1400f4600: frees the validity array and the cell BST
    ~PlantArea() override;                                          // [0]  0x14002c4b0
    void  Persist(void* stream) override;                           // [3]  0x1400f4540
    int   Execute(CitizenComponent** entityIBP, System::Time* context) override;  // [4]  0x1400f4010
    int   OnComplete(CitizenComponent** param2, ComponentEntity** param3, int arg3) override;  // [6]  0x1400f3f60
    int   OnCancel(CitizenComponent** param2, ComponentEntity** param3, int arg3) override;    // [7]  0x1400f3ea0
    int   Cleanup() override;                                       // [8]  0x1400f4460
    int   Validate() override;                          // [9]  0x1400f3c70
    Profession** GetTarget(Profession** outPtr) override;                         // [10] 0x1400f44e0
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- PlantArea fields (+0x48) ---
    WorkCellSetNode* field_48;   // +0x48: the cell -> citizen set (0x140029a40 / 0x14001ca60 / 0x140029de0)
    int32_t  field_50;           // +0x50: its count (0x140029860 `CMP [RCX+0x50]; SETG`: signed)
    uint8_t  _pad54[4];          // +0x54: padding
    // +0x58 / +0x60 / +0x64 are one byte Collection::Array {data, count, capacity}
    // (SetSize 0x14000a590; Persist streams it through the bool-array overload
    // 0x140151458): one byte per CropFieldComponent slot, 1 = still to plant.
    bool*    field_58;           // +0x58: the array's data (bool: the overload Persist streams it with)
    int32_t  field_60;           // +0x60: its count (= the field's slot count)
    uint8_t  _pad64[4];          // +0x64: its capacity (written by SetSize; kept raw bytes for
                                 //        the crop-field state that zeroes it bytewise)
    ComponentDescription* field_68;   // +0x68: ExternalPtr<ComponentDescription> (0x140002340):
                                      //        the crop the field plants
};

static_assert(sizeof(PlantArea) == 0x70, "PlantArea is 0x70 (retail create MemAlloc)");
static_assert(alignof(PlantArea) == 8, "PlantArea is 8-aligned");
static_assert(offsetof(PlantArea, field_48) == 0x48, "PlantArea +0x48");
static_assert(offsetof(PlantArea, field_50) == 0x50, "PlantArea +0x50");
static_assert(offsetof(PlantArea, field_58) == 0x58, "PlantArea +0x58");
static_assert(offsetof(PlantArea, field_60) == 0x60, "PlantArea +0x60");
static_assert(offsetof(PlantArea, field_68) == 0x68, "PlantArea +0x68");
