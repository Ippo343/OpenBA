#pragma once
// ============================================================================
// code/Game/Component/StorageComponent.h — ENDGAME B.3.8 block VIS
//
// `StorageComponent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D4). The engine/types POD is a forwarder to this file.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StorageComponent", parent "Component", ti 0x1401c7e78
//   create 0x140038b50: MemAlloc(0x40, 8) + ctor 0x14012c8d0
//   ctor   0x14012c8d0: Component ctor, stamp 0x140164720, +0x28 = +0x30 = 0,
//                       flags |= 0x81
//   dtor   0x14012c870: MemFree(+0x28), +0x28 = +0x30 = 0, restamp, ~Internal
//   table  0x140164720, 16 slots:
//     [0] 0x140038d60 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x14012c730 Persist                [4] 0x14012c720 own `RET`
//     [6] 0x14012c640 OnAdded (Ghidra "Draw": capacity + template slots)
//     every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

class StorageComponent final : public Component {
    DECLARE_EXE_TYPE(StorageComponent)

public:
    StorageComponent();                                           // 0x14012c8d0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140038d60
    void Persist(void* stream) override;                          // [3] 0x14012c730
    void Startup() override;                                      // [4] 0x14012c720 own RET
    void OnAdded() override;                                      // [6] 0x14012c640

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    storageSlots; // +0x28 slot array data (freed by the retail dtor)
    int32_t  slotCount;    // +0x30 number of storage slots
    uint8_t  _pad34[4];    // +0x34 (capacity of the slot array)
    void*    field38;      // +0x38 capacity (+0x38) / default type (+0x3c)

protected:
    // PORT GAP (retail 0x14012c870) — B38 D2: retail frees the +0x28 slot array;
    // the tree never did (Inert_ScalarDtor). Kept as the tree's.
    ~StorageComponent() = default;
};

static_assert(sizeof(StorageComponent) == 0x40, "StorageComponent is 0x40 (create 0x140038b50)");
static_assert(alignof(StorageComponent) == 8, "StorageComponent alignment");
static_assert(offsetof(StorageComponent, storageSlots) == 0x28, "");
static_assert(offsetof(StorageComponent, slotCount) == 0x30, "");
static_assert(offsetof(StorageComponent, field38) == 0x38, "");
