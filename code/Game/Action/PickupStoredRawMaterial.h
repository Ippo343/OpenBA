#pragma once
// ============================================================================
// code/Game/Action/PickupStoredRawMaterial.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `PickupStoredRawMaterial`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PickupStoredRawMaterial", parent "Action", ti 0x1401c6048
//   create 0x140022d60: MemAlloc(0x48, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015ff80, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class PickupStoredRawMaterial final : public Action {
    DECLARE_EXE_TYPE(PickupStoredRawMaterial)

public:
    PickupStoredRawMaterial();                                                   // create 0x140022d60
    PickupStoredRawMaterial(uint64_t* source, uint64_t* storage, int64_t resType, int requested,
                            uint8_t flag);   // 0x1400ca520
    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140022dd0 (frees the +0x30 list)
    void Persist(void* stream) override;                       // [3] 0x1400ca0d0
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c9d40
    void CheckEntity() override;                               // [8] 0x1400c9c70
    bool GetTarget(void* param) override;                      // [9] 0x1400c9bf0

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    void*     _field20;          // +0x20 source entity IBP
    void*     _field28;          // +0x28 storage entity IBP
    void*     _field30;          // +0x30 carrier list data (stride 0x10), freed by the dtor
    void*     _field38;          // +0x38 carrier list count (+0x38) / capacity (+0x3c)
    uint8_t   _field40;          // +0x40 flag
    uint8_t   _pad41[7];         // +0x41 padding

protected:
    // Retail 0x140022dd0: frees the +0x30 carrier list, then Action's dtor.
    ~PickupStoredRawMaterial();
};

static_assert(sizeof(PickupStoredRawMaterial) == 0x48, "PickupStoredRawMaterial is 0x48 (create 0x140022d60 MemAlloc)");
static_assert(alignof(PickupStoredRawMaterial) == 8, "PickupStoredRawMaterial is 8-aligned");
static_assert(offsetof(PickupStoredRawMaterial, _field20) == 0x20, "PickupStoredRawMaterial +0x20");
static_assert(offsetof(PickupStoredRawMaterial, _field28) == 0x28, "PickupStoredRawMaterial +0x28");
static_assert(offsetof(PickupStoredRawMaterial, _field30) == 0x30, "PickupStoredRawMaterial +0x30");
static_assert(offsetof(PickupStoredRawMaterial, _field38) == 0x38, "PickupStoredRawMaterial +0x38");
static_assert(offsetof(PickupStoredRawMaterial, _field40) == 0x40, "PickupStoredRawMaterial +0x40");
