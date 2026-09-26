#pragma once
// ============================================================================
// code/Game/Action/DropOffRawMaterial.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `DropOffRawMaterial`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "DropOffRawMaterial", parent "Action", ti 0x1401c5f30
//   create 0x140022600: MemAlloc(0x40, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015fd80, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class DropOffRawMaterial final : public Action {
    DECLARE_EXE_TYPE(DropOffRawMaterial)

public:
    DropOffRawMaterial();                                                   // create 0x140022600
    DropOffRawMaterial(uint64_t* storage, uint64_t* source, uint64_t resType, uint32_t count,
                       uint8_t flag);   // 0x1400c7cd0
    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140022670
    void Persist(void* stream) override;                       // [3] 0x1400c7b10
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c7300
    void CheckEntity() override;                               // [8] 0x1400c72f0 (JMP 0x1400c7570)
    bool GetTarget(void* param) override;                      // [9] 0x1400c7270

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    uint8_t   _field20;          // +0x20 bool flag
    uint8_t   _pad21[3];         // +0x21 padding
    int32_t   _field24;          // +0x24 count
    void*     _field28;          // +0x28 resource type
    void*     _field30;          // +0x30 target storage
    void*     _field38;          // +0x38 source entity

protected:
    ~DropOffRawMaterial() = default;
};

static_assert(sizeof(DropOffRawMaterial) == 0x40, "DropOffRawMaterial is 0x40 (create 0x140022600 MemAlloc)");
static_assert(alignof(DropOffRawMaterial) == 8, "DropOffRawMaterial is 8-aligned");
static_assert(offsetof(DropOffRawMaterial, _field20) == 0x20, "DropOffRawMaterial +0x20");
static_assert(offsetof(DropOffRawMaterial, _field24) == 0x24, "DropOffRawMaterial +0x24");
static_assert(offsetof(DropOffRawMaterial, _field28) == 0x28, "DropOffRawMaterial +0x28");
static_assert(offsetof(DropOffRawMaterial, _field30) == 0x30, "DropOffRawMaterial +0x30");
static_assert(offsetof(DropOffRawMaterial, _field38) == 0x38, "DropOffRawMaterial +0x38");
