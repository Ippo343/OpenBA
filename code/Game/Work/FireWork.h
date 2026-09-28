#pragma once
// ============================================================================
// code/Game/Work/FireWork.h — ENDGAME B.3.8 block WRK
//
// FireWork — the firefighting work a burning building posts: a BST of the
// citizens carrying water (+0x50), their count (+0x58) and the capacity (+0x48).
// A REAL class with a compiler-generated vtable. FireWork extends Work directly,
// NOT AutoPickupWork.
//
// RETAIL (B38_SLOTS.md §6): reflected "FireWork", parent "Work", ti 0x1401c6978,
//   create 0x14002a1e0 (MemAlloc(0x60) + ctor 0x1400e9b50); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x1401612a0, 13 slots:
//   [0] 0x14002a230 scalar deleting dtor (the dtor 0x1400e9ac0 + MemFree on &1)
//   [1] 0x14002a1d0 TypeName  [2] 0x14002a1c0 Type  [3] 0x1400e9a50 Persist
//   [4] 0x1400e9620 Execute   [5] = Work (0x1400f7540)  [6] 0x1400e9600 OnComplete
//   [7] 0x1400e95f0 OnCancel = `ADD RCX,0x50; JMP 0x140025520` (a member forwarder)
//   [8] 0x1400e99a0 Cleanup   [9] 0x1400e93c0 Validate  [10] 0x1400e9a30 GetTarget
//   [11] 0x14002a220 GetPriority (`+0x58 > 0`)  [12] = Work (0x140006b10)
//   The real producer is the 3-argument ctor 0x1400e9bd0, built BY HAND in
//   firecomponent_ignite.cpp (block CIT), which stamps 0x1401612a0; that
//   registration serves FireWork::Vtable() (B38 D5). The default ctor here builds
//   what the tree's factory built (Work_GenericCtor: the Work base init, the leaf
//   fields zeroed).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

namespace FireWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400e9b50;
    constexpr uintptr_t Factory = 0x14002a1e0;
    constexpr uintptr_t Vtable  = 0x1401612a0;
}

class FireWork final : public Work {
    DECLARE_EXE_TYPE(FireWork)

public:
    FireWork();                                                     // (Work_GenericCtor shape)
    // 0x1400e9bd0, the 3-argument ctor (FireComponent::Ignite's, the only
    // producer): the entity-seeded Work base ctor 0x1400f72e0, the table, a
    // DWORD store of `capacity` at +0x48 (the upper dword is not written),
    // +0x50 = 0 (qword), +0x58 = 0 (dword). ENDGAME B.3.9 OBJ: replaces the
    // `??0Internal` door + stamp in FireWork_CtorFull_e9bd0.
    FireWork(void* pEntityIBP, uint32_t capacity);

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002a230
    void  Persist(void* stream) override;                           // [3]  0x1400e9a50
    int   Execute(void* citizenIBP, void* time) override;  // [4]  0x1400e9620
    int   OnComplete(void* citizenIBP, void* arg2, int arg3) override;    // [6]  0x1400e9600
    int   OnCancel(void* citizenIBP, void* arg2, int arg3) override;      // [7]  0x1400e95f0
    int   Cleanup() override;                                       // [8]  0x1400e99a0
    int   Validate() override;                      // [9]  0x1400e93c0
    void* GetTarget(void* retbuf) override;                         // [10] 0x1400e9a30
    int   GetPriority() override;                                   // [11] 0x14002a220

    // --- FireWork fields (+0x48) ---
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    uint32_t _field58;           // +0x58: zeroed
    uint8_t  _pad5C[4];          // +0x5C: padding

protected:
    // PORT GAP (retail 0x1400e9ac0) — B38 D9: retail's dtor (reached from slot 0
    // 0x14002a230) is its own body; the tree's slot 0 was Work_GenericScalarDtor
    // (stamp Work, ~Internal), which is what this empty dtor + the compiler chain
    // still does. Whether retail frees the +0x50 citizen BST here is not read.
    ~FireWork() = default;
};

static_assert(sizeof(FireWork) == 0x60, "FireWork is 0x60 (retail create MemAlloc)");
static_assert(alignof(FireWork) == 8, "FireWork is 8-aligned");
static_assert(offsetof(FireWork, _field48) == 0x48, "FireWork +0x48");
static_assert(offsetof(FireWork, _field50) == 0x50, "FireWork +0x50");
static_assert(offsetof(FireWork, _field58) == 0x58, "FireWork +0x58");
