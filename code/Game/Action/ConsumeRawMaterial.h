#pragma once
// ============================================================================
// code/Game/Action/ConsumeRawMaterial.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `ConsumeRawMaterial`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ConsumeRawMaterial", parent "Action", ti 0x1401c5e88
//   create 0x1400220c0: MemAlloc(0x40, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015fc20, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class ConsumeRawMaterial final : public Action {
    DECLARE_EXE_TYPE(ConsumeRawMaterial)

public:
    ConsumeRawMaterial();                                                   // create 0x1400220c0
    ConsumeRawMaterial(uint64_t* storage, int64_t resType, uint8_t reserve);   // 0x1400c63b0
    explicit ConsumeRawMaterial(uint64_t* storage);                         // 0x1400c6440
    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140022130
    void Persist(void* stream) override;                       // [3] 0x1400c6120
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c5f50
    void CheckEntity() override;                               // [8] 0x1400c5e30
    bool GetTarget(void* param) override;                      // [9] 0x1400c5f10

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    void*     _field20;          // +0x20 storage entity IBP
    void*     _field28;          // +0x28 consume list data (stride 0x10), freed by the dtor
    void*     _field30;          // +0x30 consume list count (+0x30) / capacity (+0x34)
    uint8_t   _field38;          // +0x38 reserve flag
    uint8_t   _pad39[7];         // +0x39 padding

protected:
    // PORT GAP (retail 0x140022130 frees the +0x28 consume list) — B38 D2, next commit.
    ~ConsumeRawMaterial() = default;
};

static_assert(sizeof(ConsumeRawMaterial) == 0x40, "ConsumeRawMaterial is 0x40 (create 0x1400220c0 MemAlloc)");
static_assert(alignof(ConsumeRawMaterial) == 8, "ConsumeRawMaterial is 8-aligned");
static_assert(offsetof(ConsumeRawMaterial, _field20) == 0x20, "ConsumeRawMaterial +0x20");
static_assert(offsetof(ConsumeRawMaterial, _field28) == 0x28, "ConsumeRawMaterial +0x28");
static_assert(offsetof(ConsumeRawMaterial, _field30) == 0x30, "ConsumeRawMaterial +0x30");
static_assert(offsetof(ConsumeRawMaterial, _field38) == 0x38, "ConsumeRawMaterial +0x38");
