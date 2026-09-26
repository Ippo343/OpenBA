#pragma once
// ============================================================================
// code/Game/Action/PickupRawMaterial.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `PickupRawMaterial`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PickupRawMaterial", parent "Action", ti 0x1401c6010
//   create 0x140022c60: MemAlloc(0x30, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015ff10, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class PickupRawMaterial final : public Action {
    DECLARE_EXE_TYPE(PickupRawMaterial)

public:
    PickupRawMaterial();                                                   // create 0x140022c60

    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140022cd0
    void Persist(void* stream) override;                       // [3] 0x1400c99e0
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c98d0
    bool GetTarget(void* param) override;                      // [9] 0x1400c9860

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    void*     _field20;          // +0x20 
    void*     _field28;          // +0x28 

protected:
    ~PickupRawMaterial() = default;
};

static_assert(sizeof(PickupRawMaterial) == 0x30, "PickupRawMaterial is 0x30 (create 0x140022c60 MemAlloc)");
static_assert(alignof(PickupRawMaterial) == 8, "PickupRawMaterial is 8-aligned");
static_assert(offsetof(PickupRawMaterial, _field20) == 0x20, "PickupRawMaterial +0x20");
static_assert(offsetof(PickupRawMaterial, _field28) == 0x28, "PickupRawMaterial +0x28");
