#pragma once
// ============================================================================
// code/Game/Action/HuntAnimal.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `HuntAnimal`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HuntAnimal", parent "Action", ti 0x1401c5f68
//   create 0x140022700: MemAlloc(0x40, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015fde0, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class HuntAnimal final : public Action {
    DECLARE_EXE_TYPE(HuntAnimal)

public:
    HuntAnimal();                                                   // create 0x140022700
    HuntAnimal(uint64_t* herd, uint64_t* animal);   // 0x1400c8400
    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140022770
    void Persist(void* stream) override;                       // [3] 0x1400c8260
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c8250
    int  Execute(void* actionStack, void* time) override;      // [5] 0x1400c8240
    void Enter() override;                                     // [6] 0x1400c7de0
    bool GetTarget(void* param) override;                      // [9] 0x1400c7d60

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    void*     _field20;          // +0x20 animal entity
    void*     _field28;          // +0x28 herd entity
    void*     _field30;          // +0x30 
    uint8_t   _field38;          // +0x38 hunt started flag
    uint8_t   _pad39[7];         // +0x39 padding

protected:
    ~HuntAnimal() = default;
};

static_assert(sizeof(HuntAnimal) == 0x40, "HuntAnimal is 0x40 (create 0x140022700 MemAlloc)");
static_assert(alignof(HuntAnimal) == 8, "HuntAnimal is 8-aligned");
static_assert(offsetof(HuntAnimal, _field20) == 0x20, "HuntAnimal +0x20");
static_assert(offsetof(HuntAnimal, _field28) == 0x28, "HuntAnimal +0x28");
static_assert(offsetof(HuntAnimal, _field30) == 0x30, "HuntAnimal +0x30");
static_assert(offsetof(HuntAnimal, _field38) == 0x38, "HuntAnimal +0x38");
