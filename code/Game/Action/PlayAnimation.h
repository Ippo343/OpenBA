#pragma once
// ============================================================================
// code/Game/Action/PlayAnimation.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `PlayAnimation`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PlayAnimation", parent "Action", ti 0x1401c3df0
//   create 0x1400011f0: MemAlloc(0x40, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x140158950, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class PlayAnimation final : public Action {
    DECLARE_EXE_TYPE(PlayAnimation)

public:
    PlayAnimation();                                                   // create 0x1400011f0

    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140001260
    void Persist(void* stream) override;                       // [3] 0x140042480
    int  AbstractImpl(void* actionStack) override;             // [4] 0x140042420
    int  Execute(void* actionStack, void* time) override;      // [5] 0x140042400
    void CheckEntity() override;                               // [8] 0x1400423f0 own `RET 0` (same bytes as the base body)
    bool GetTarget(void* param) override;                      // [9] 0x1400423b0

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    void*     _field20;          // +0x20 
    uint8_t   _pad28[8];         // +0x28 gap
    void*     _field30;          // +0x30 
    void*     _field38;          // +0x38 

protected:
    ~PlayAnimation() = default;
};

static_assert(sizeof(PlayAnimation) == 0x40, "PlayAnimation is 0x40 (create 0x1400011f0 MemAlloc)");
static_assert(alignof(PlayAnimation) == 8, "PlayAnimation is 8-aligned");
static_assert(offsetof(PlayAnimation, _field20) == 0x20, "PlayAnimation +0x20");
static_assert(offsetof(PlayAnimation, _field30) == 0x30, "PlayAnimation +0x30");
static_assert(offsetof(PlayAnimation, _field38) == 0x38, "PlayAnimation +0x38");
