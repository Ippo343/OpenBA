#pragma once
// ============================================================================
// code/Game/Action/MoveTo.h — ENDGAME B.3.8 block ACT (B38_RULES.md D1-D5)
//
// `MoveTo`, an exe Action leaf, as a real C++ class with a compiler vtable.
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "MoveTo", parent "Action", ti 0x1401c5fa0
//   create 0x1400228c0: MemAlloc(0x480, 8) + the inlined ctor
//   createInPlace = the IMPORTED `Internal::CreateInPlaceObject` (returns null);
//     the tree's in-place entry builds the object and that is KEPT (B8-13).
//   table 0x14015fe40, 10 slots; every slot not declared below is Action's (= parent)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Action/Action.h"
#include "Object/exe_type.h"

class MoveTo final : public Action {
    DECLARE_EXE_TYPE(MoveTo)

public:
    MoveTo();                                                   // create 0x1400228c0
    MoveTo(void* entity, uint32_t pathType, int param4, float* destination, int param6, int param7,
           float speed);   // 0x1400c9170
    Object::Base* DeletingDtor(unsigned flags) override;       // [0] 0x140022900
    void Persist(void* stream) override;                       // [3] 0x1400c8ec0
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c8ab0
    int  Execute(void* actionStack, void* time) override;      // [5] 0x1400c8570
    void Enter(void* actionStack, void* time) override;        // [6] 0x1400c8970
    void Exit(void* actionStack) override;                     // [7] 0x1400c8520
    void CheckEntity() override;                               // [8] 0x1400c84d0
    bool GetTarget(void* param) override;                      // [9] 0x1400c8480

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    int32_t   targetId;          // +0x20 target id (-1 = invalid)
    uint32_t  _field24;          // +0x24 
    void*     _field28;          // +0x28 entity IBP
    void*     _field30;          // +0x30 CharacterDescription (walk speeds)
    void*     _field38;          // +0x38 destination x/y (floats)
    void*     _field40;          // +0x40 destination z + ...
    void*     _field48;          // +0x48 
    void*     _field50;          // +0x50 
    void*     _field58;          // +0x58 
    int32_t   _field60;          // +0x60 -1 sentinel
    uint32_t  _field64;          // +0x64 
    uint8_t   _pathData[0x40C];  // +0x68 path / waypoint block (0x468 pathType, 0x46c, 0x470)
    uint8_t   _field474;         // +0x474 
    uint8_t   _pad475[3];        // +0x475 padding
    float     speedMultiplier;   // +0x478 walk speed
    uint8_t   _pad47C[4];        // +0x47C padding

protected:
    ~MoveTo() = default;
};

static_assert(sizeof(MoveTo) == 0x480, "MoveTo is 0x480 (create 0x1400228c0 MemAlloc)");
static_assert(alignof(MoveTo) == 8, "MoveTo is 8-aligned");
static_assert(offsetof(MoveTo, targetId) == 0x20, "MoveTo +0x20");
static_assert(offsetof(MoveTo, _field24) == 0x24, "MoveTo +0x24");
static_assert(offsetof(MoveTo, _field28) == 0x28, "MoveTo +0x28");
static_assert(offsetof(MoveTo, _field30) == 0x30, "MoveTo +0x30");
static_assert(offsetof(MoveTo, _field38) == 0x38, "MoveTo +0x38");
static_assert(offsetof(MoveTo, _field40) == 0x40, "MoveTo +0x40");
static_assert(offsetof(MoveTo, _field48) == 0x48, "MoveTo +0x48");
static_assert(offsetof(MoveTo, _field50) == 0x50, "MoveTo +0x50");
static_assert(offsetof(MoveTo, _field58) == 0x58, "MoveTo +0x58");
static_assert(offsetof(MoveTo, _field60) == 0x60, "MoveTo +0x60");
static_assert(offsetof(MoveTo, _field64) == 0x64, "MoveTo +0x64");
static_assert(offsetof(MoveTo, _pathData) == 0x68, "MoveTo +0x68");
static_assert(offsetof(MoveTo, _field474) == 0x474, "MoveTo +0x474");
static_assert(offsetof(MoveTo, speedMultiplier) == 0x478, "MoveTo +0x478");
