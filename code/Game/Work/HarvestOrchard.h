#pragma once
// ============================================================================
// code/Game/Work/HarvestOrchard.h — ENDGAME B.3.8 block WRK
//
// HarvestOrchard — the orchard harvest work: a BST of the trees being picked
// (+0x48, count +0x50) and a per-orchard-slot byte array (+0x58 / +0x60: 1 = a
// tree to pick). A REAL class with a compiler-generated vtable.
//
// RETAIL (B38_SLOTS.md §6): reflected "HarvestOrchard", parent "AutoPickupWork",
//   create 0x14002a520 (MemAlloc(0x68) + ctor); createInPlace = the imported
//   Internal::CreateInPlaceObject (B8-13: the tree's in-place build kept)
//   table 0x140161520, 13 slots: [0] scalar deleting dtor  [1] TypeName  [2] Type
//   [3] 0x1400edea0 Persist  [4] Execute  [5] = Work (0x1400f7540)  [6] OnComplete
//   [7] OnCancel  [8] Cleanup  [9] Validate  [10] GetTarget  [11] 0x140029860
//   GetPriority (`+0x50 > 0`, shared by 13 tables)  [12] = Work (0x140006b10)
//   ctors: the default one (the tree built it with Work_GenericCtor: base init, leaf
//   fields zeroed — kept) and the from-entity ctor 0x1400ee060 (harvestorchard_ctor.cpp),
//   which OrchardComponent's state 11 (orchardcomponent_states.cpp, block MI-B) calls
//   through the door HarvestOrchard_CtorFromEntity. Nothing outside this block stamps
//   0x140161520.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Work/AutoPickupWork.h"
#include "Object/exe_type.h"
#include "collection/Collection.h"   // Collection::Set<T>::Node

class CitizenComponent;


class HarvestOrchard final : public AutoPickupWork {
    DECLARE_EXE_TYPE(HarvestOrchard)

public:
    HarvestOrchard();                                               // (Work_GenericCtor shape)
    explicit HarvestOrchard(void** entityIBP);                      // 0x1400ee060

    // frees the slot array and the BST
    ~HarvestOrchard() override;                                     // [0]
    void  Persist(void* stream) override;                           // [3]  0x1400edea0
    int   Execute(CitizenComponent** entityIBP, System::Time* context) override;  // [4]
    int   OnComplete(CitizenComponent** param2, ComponentEntity** param3, int param4) override;  // [6]
    int   OnCancel(CitizenComponent** param2, ComponentEntity** param3, int param4) override;    // [7]
    int   Cleanup() override;                                       // [8]
    int   Validate() override;                          // [9]
    Profession** GetTarget(Profession** outPtr) override;                         // [10]
    int   GetPriority() override;                                   // [11] 0x140029860

    // --- HarvestOrchard fields (+0x48) ---
    // The element of the claim tree (0x10): the orchard slot index (the int key) and the
    // claiming citizen (the cell-BST helpers 0x140029a40 / 0x14001ca60 / 0x140029de0).
    struct Claim {
        int32_t           slot;      // +0x00
        int32_t           _pad04;    // +0x04 written 0 with the key
        CitizenComponent* citizen;   // +0x08 InternalPtr<CitizenComponent> (Cleanup reads its +0x278)
    };
    // Its 0x30-byte node: parent +0x10, left +0x18, right +0x20.
    using ClaimNode = Collection::Set<Claim>::Node;

    // +0x48 / +0x50: Collection::Set<Claim> {root, count}; the container stays raw (H5).
    ClaimNode* _field48;         // +0x48: zeroed
    int32_t  _field50;           // +0x50: claim count (0x140029860 `CMP [RCX+0x50],EAX; SETG`)
    uint8_t  _pad54[4];          // +0x54: padding
    // +0x58 / +0x60 / +0x64: Collection::Array<bool> {data, count, capacity}, one flag per
    // orchard slot (Persist 0x1400edea0 serializes it through the bool-array door
    // 0x140151458 with the int count at +0x60; the ctor 0x1400ee060 and the dtor clear
    // +0x60/+0x64 with one qword store). The container stays raw (H5).
    bool*    _field58;           // +0x58: zeroed
    int32_t  _field60;           // +0x60: count
    int32_t  _field64;           // +0x64: capacity
};

static_assert(sizeof(HarvestOrchard::Claim) == 0x10, "HarvestOrchard claim element 0x10");
static_assert(sizeof(HarvestOrchard::ClaimNode) == 0x30, "HarvestOrchard claim node 0x30");
static_assert(offsetof(HarvestOrchard::ClaimNode, parent) == 0x10, "claim node parent @ +0x10");
static_assert(offsetof(HarvestOrchard::ClaimNode, left) == 0x18, "claim node left @ +0x18");
static_assert(offsetof(HarvestOrchard::ClaimNode, right) == 0x20, "claim node right @ +0x20");

static_assert(sizeof(HarvestOrchard) == 0x68, "HarvestOrchard is 0x68 (retail create MemAlloc)");
static_assert(alignof(HarvestOrchard) == 8, "HarvestOrchard is 8-aligned");
static_assert(offsetof(HarvestOrchard, _field48) == 0x48, "HarvestOrchard +0x48");
static_assert(offsetof(HarvestOrchard, _field50) == 0x50, "HarvestOrchard +0x50");
static_assert(offsetof(HarvestOrchard, _field58) == 0x58, "HarvestOrchard +0x58");
static_assert(offsetof(HarvestOrchard, _field60) == 0x60, "HarvestOrchard +0x60");
static_assert(offsetof(HarvestOrchard, _field64) == 0x64, "HarvestOrchard +0x64");
