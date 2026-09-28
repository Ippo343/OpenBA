#pragma once
// ============================================================================
// code/Game/Work/GatherRawMaterials.h — ENDGAME B.3.8 block WRK
//
// GatherRawMaterials — the "haul the materials a construction site needs" work:
// per-material amounts present (+0x48 array) and in flight (+0x58 array), and a
// BST of the citizens hauling (+0x68, count +0x70). A REAL class with a
// compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "GatherRawMaterials", parent "Work",
//   create 0x14002a3c0 (MemAlloc(0x78) + ctor 0x1400ec8a0); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161420, 13 slots:
//   [0] 0x14002a410 scalar deleting dtor (the dtor 0x1400ec650 + MemFree on &1)
//   [1] TypeName  [2] Type  [3] 0x1400ec560 Persist  [4] Execute
//   [5] = Work (0x1400f7540)  [6] OnComplete  [7] OnCancel  [8] Cleanup  [9] Validate
//   [10] 0x1400ec510 GetTarget  [11] 0x14002a400 GetPriority (`+0x70 > 0`)
//   [12] = Work (0x140006b10)
//   ctors: the default one (the tree built it with Work_GenericCtor: base init, leaf
//   fields zeroed — kept) and the site ctor 0x1400ec710 (entity-seeded, sizes the two
//   arrays from the site's BuildDescription; gatherrawmaterials_ctor.cpp), which
//   BuildComponent's state 1 (buildcomponent_state1.cpp) calls BY ADDRESS / name.
//   Nothing outside this block stamps 0x140161420.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

namespace GatherRawMaterialsAddr {
    constexpr uintptr_t Ctor         = 0x1400ec8a0;
    constexpr uintptr_t Factory      = 0x14002a3c0;
    constexpr uintptr_t Vtable       = 0x140161420;
    constexpr uintptr_t ScalarDtor   = 0x14002a410;
    constexpr uintptr_t BaseDtor     = 0x1400ec650;
    constexpr uintptr_t Serialize    = 0x1400ec560;
}

class GatherRawMaterials final : public Work {
    DECLARE_EXE_TYPE(GatherRawMaterials)

public:
    GatherRawMaterials();                                           // (Work_GenericCtor shape)
    explicit GatherRawMaterials(void* buildEntityIBP);              // 0x1400ec710 (site ctor)

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002a410
    void  Persist(void* stream) override;                           // [3]  0x1400ec560
    int   Execute(void* entityIBP, void* context) override;  // [4]
    int   OnComplete(void* param2, void* param3, int arg3) override;  // [6]
    int   OnCancel(void* param2, void* param3, int arg3) override;    // [7]
    int   Cleanup() override;                                       // [8]
    int   Validate() override;                          // [9]
    void* GetTarget(void* retbuf) override;                         // [10] 0x1400ec510
    int   GetPriority() override;                                   // [11] 0x14002a400

    // --- GatherRawMaterials fields (+0x48) ---
    void*    field_48;           // +0x48: int array (zeroed)
    int32_t  field_50;           // +0x50: count (zeroed)
    int32_t  field_54;           // +0x54: capacity of the +0x48 array
    void*    field_58;           // +0x58: int array (zeroed)
    int32_t  field_60;           // +0x60: count (zeroed)
    int32_t  field_64;           // +0x64: capacity of the +0x58 array
    void*    field_68;           // +0x68: tree node ptr (zeroed)
    int32_t  field_70;           // +0x70: count (zeroed)
    uint8_t  _pad74[4];          // +0x74: padding

protected:
    ~GatherRawMaterials();                                         // 0x1400ec650: frees the BST and both arrays
};

static_assert(sizeof(GatherRawMaterials) == 0x78, "GatherRawMaterials is 0x78 (retail create MemAlloc)");
static_assert(alignof(GatherRawMaterials) == 8, "GatherRawMaterials is 8-aligned");
static_assert(offsetof(GatherRawMaterials, field_48) == 0x48, "GatherRawMaterials +0x48");
static_assert(offsetof(GatherRawMaterials, field_50) == 0x50, "GatherRawMaterials +0x50");
static_assert(offsetof(GatherRawMaterials, field_54) == 0x54, "GatherRawMaterials +0x54");
static_assert(offsetof(GatherRawMaterials, field_58) == 0x58, "GatherRawMaterials +0x58");
static_assert(offsetof(GatherRawMaterials, field_60) == 0x60, "GatherRawMaterials +0x60");
static_assert(offsetof(GatherRawMaterials, field_64) == 0x64, "GatherRawMaterials +0x64");
static_assert(offsetof(GatherRawMaterials, field_68) == 0x68, "GatherRawMaterials +0x68");
static_assert(offsetof(GatherRawMaterials, field_70) == 0x70, "GatherRawMaterials +0x70");
