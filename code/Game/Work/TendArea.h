#pragma once
// ============================================================================
// code/Game/Work/TendArea.h — ENDGAME B.3.8 block WRK
//
// TendArea — the "tend the field / orchard" work: a BST of the citizens assigned
// (+0x48, count +0x50; simple 0x20-byte nodes: key, parent, left, right) and the
// tend mode (+0x58: 0 crop/area, nonzero orchard). A REAL class with a
// compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "TendArea", parent "Work", ti 0x1401c6c88,
//   create 0x14002c590 (MemAlloc(0x60) + ctor 0x1400f5a00); createInPlace = the
//   imported Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161990, 13 slots:
//   [0] 0x14002c5d0 scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400f5900 Persist  [4] 0x1400f5490 Execute  [5] = Work (0x1400f7540)
//   [6] 0x1400f5470 OnComplete  [7] 0x1400f5460 OnCancel = `ADD RCX,0x48; JMP
//   0x140025520` (a member forwarder)  [8] 0x1400f5830 Cleanup  [9] 0x1400f5230
//   Validate  [10] 0x1400f58a0 GetTarget  [11] 0x140029860 GetPriority (`+0x50 > 0`,
//   shared by 13 tables)  [12] = Work (0x140006b10)
//   ctors: the default one (the tree built it with Work_GenericCtor: base init, leaf
//   fields zeroed — kept) and the from-entity one 0x1400f5a80 (tendarea_ctor.cpp) that
//   OrchardComponent's state 10 (orchardcomponent_states.cpp, block MI-B) reaches
//   through the door TendArea_CtorFromEntity. CropFieldComponent's state machine
//   (cropfieldcomponent_states.cpp, block MI-B) builds a TendArea BY HAND and stamps
//   0x140161990; that registration serves TendArea::Vtable() (B38 D5).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/Work.h"
#include "Object/exe_type.h"

namespace TendAreaAddr {
    constexpr uintptr_t Ctor    = 0x1400f5a00;
    constexpr uintptr_t Vtable  = 0x140161990;
}

class TendArea final : public Work {
    DECLARE_EXE_TYPE(TendArea)

public:
    TendArea();                                                     // (Work_GenericCtor shape)
    TendArea(void** entityIBP, int kind);                           // 0x1400f5a80

    Object::Base* DeletingDtor(unsigned flags) override;            // [0]  0x14002c5d0
    void  Persist(void* stream) override;                           // [3]  0x1400f5900
    int   Execute(void* citizenIBP, void* context) override;  // [4]  0x1400f5490
    int   OnComplete(void* param2, void* arg2, int arg3) override;  // [6]  0x1400f5470
    int   OnCancel(void* param2, void* arg2, int arg3) override;    // [7]  0x1400f5460
    int   Cleanup() override;                                       // [8]  0x1400f5830
    int   Validate() override;                          // [9]  0x1400f5230
    void* GetTarget(void* outPtr) override;                         // [10] 0x1400f58a0
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- TendArea fields (+0x48) ---
    void*    bstRoot;            // +0x48: BST root for assigned workers
    uint32_t workerCount;        // +0x50: number of workers in BST
    uint8_t  _pad54[4];          // +0x54: padding
    uint32_t tendMode;           // +0x58: 0=crop/area, nonzero=orchard mode (serialized int)

protected:
    ~TendArea();                                               // frees the +0x48 citizen BST
};

static_assert(sizeof(TendArea) == 0x60, "TendArea is 0x60 (retail create MemAlloc)");
static_assert(alignof(TendArea) == 8, "TendArea is 8-aligned");
static_assert(offsetof(TendArea, bstRoot) == 0x48, "TendArea +0x48");
static_assert(offsetof(TendArea, workerCount) == 0x50, "TendArea +0x50");
static_assert(offsetof(TendArea, tendMode) == 0x58, "TendArea +0x58");
