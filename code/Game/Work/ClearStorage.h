#pragma once
// ============================================================================
// code/Game/Work/ClearStorage.h — ENDGAME B.3.8 block WRK
//
// ClearStorage — the "haul everything out of this storage" work (a demolished or
// disabled stockpile): a BST of the citizens assigned (+0x58) and the count of
// items left (+0x60). A REAL class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "ClearStorage", parent "Work", ti 0x1401c6908,
//   create 0x14002a0b0 (MemAlloc(0x68) + ctor); createInPlace = the imported
//   Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x1401611a0, 13 slots:
//   [0] 0x14002a100 scalar deleting dtor (the dtor 0x1400e73b0 + MemFree on &1)
//   [1] 0x14002a0a0 TypeName  [2] 0x14002a090 Type  [3] 0x1400e7350 Persist
//   [4] 0x1400e6c50 Execute   [5] = Work (0x1400f7540)  [6] 0x1400e6c30 OnComplete
//   [7] 0x1400e6c20 OnCancel = `ADD RCX,0x58; JMP 0x140025520` (a member forwarder:
//       the citizen tree's find-remove, B38_SLOTS head fact 2)
//   [8] 0x1400e72a0 Cleanup   [9] 0x1400e69f0 Validate  [10] 0x1400e7330 GetTarget
//   [11] 0x14002a0f0 GetPriority (`+0x60 > 0`, shared with ConsumeProduce)
//   [12] = Work (0x140006b10)
//   ctors 0x1400e7450 / 0x1400e74b0 (NOT transcribed — the tree built the object with
//   Work_GenericCtor: the Work base init and the leaf fields zeroed; that is kept).
//   destroycomponent_states.cpp (block MI-A) builds a ClearStorage BY HAND and stamps
//   0x1401611a0; that registration serves ClearStorage::Vtable() (B38 D5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

namespace ClearStorageAddr {
    constexpr uintptr_t Ctor    = 0x1400e74b0;
    constexpr uintptr_t Vtable  = 0x1401611a0;
}

class ClearStorage final : public Work {
    DECLARE_EXE_TYPE(ClearStorage)

public:
    ClearStorage();                                                 // (Work_GenericCtor shape, see above)

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002a100
    void  Persist(void* stream) override;                           // [3]  0x1400e7350
    int   Execute(void* citizenAI, void* time, uint64_t arg3) override;  // [4]  0x1400e6c50
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400e6c30
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400e6c20
    int   Cleanup() override;                                       // [8]  0x1400e72a0
    int   Validate(void* citizenAI) override;                       // [9]  0x1400e69f0
    void* GetTarget(void* retbuf) override;                         // [10] 0x1400e7330
    int   GetPriority() override;                                   // [11] 0x14002a0f0

    // --- ClearStorage fields (+0x48) ---
    void*    _field48;           // +0x48: auxiliary data ptr (freed in dtor)
    void*    _field50;           // +0x50: auxiliary data ptr (zeroed in dtor)
    void*    _field58;           // +0x58: BST root — tree of citizen InternalBasePtrs
    uint32_t _field60;           // +0x60: item count remaining to clear
    uint8_t  _pad64[4];         // +0x64: padding

protected:
    ~ClearStorage();                                               // 0x1400e73b0: frees the tree and +0x48
};

static_assert(sizeof(ClearStorage) == 0x68, "ClearStorage is 0x68 (retail create MemAlloc)");
static_assert(alignof(ClearStorage) == 8, "ClearStorage is 8-aligned");
static_assert(offsetof(ClearStorage, _field48) == 0x48, "ClearStorage +0x48");
static_assert(offsetof(ClearStorage, _field50) == 0x50, "ClearStorage +0x50");
static_assert(offsetof(ClearStorage, _field58) == 0x58, "ClearStorage +0x58");
static_assert(offsetof(ClearStorage, _field60) == 0x60, "ClearStorage +0x60");
