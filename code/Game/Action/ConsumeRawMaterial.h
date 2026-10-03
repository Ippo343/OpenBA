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
#include "object/Internal.h"   // Object::InternalPtr<T>

class ComponentDescription;   // Component/ComponentDescription.h
class ComponentEntity;        // Entity/ComponentEntity.h

class ConsumeRawMaterial final : public Action {
    DECLARE_EXE_TYPE(ConsumeRawMaterial)

public:
    // One element of the +0x28 consume list (stride 0x10; Persist 0x1400c6120 serializes
    // the ExternalPtr then the int; +0x0C is the reserved count the append zeroes).
    struct Entry {
        ComponentDescription* material;   // +0x00 ExternalPtr<ComponentDescription>
        int32_t               count;      // +0x08
        int32_t               reserved;   // +0x0C
    };

    ConsumeRawMaterial();                                                   // create 0x1400220c0
    ConsumeRawMaterial(ComponentEntity* const* storage, ComponentDescription* material,
                       bool reserve);                                       // 0x1400c63b0
    explicit ConsumeRawMaterial(ComponentEntity* const* storage);   // 0x1400c6440
    // 0x1400c64b0: append {material, count} (reserving it in the storage first when +0x38 is set).
    void AddMaterial(ComponentDescription* material, int count);
    // Retail 0x140022130: frees the +0x28 consume list, then Action's dtor.
    ~ConsumeRawMaterial() override;                            // [0] 0x140022130 (frees the +0x28 list)
    void Persist(IO::Stream& stream) override;                       // [3] 0x1400c6120
    int  AbstractImpl(void* actionStack) override;             // [4] 0x1400c5f50
    void CheckEntity() override;                               // [8] 0x1400c5e30
    bool GetTarget(ComponentEntity** param) override;                      // [9] 0x1400c5f10

    // ---- fields, retail order (PUBLIC: the slot bodies are C functions) ----
    Object::InternalPtr<ComponentEntity> _field20;   // +0x20 storage entity (Persist checks ComponentEntity)
    // +0x28 Array<Entry> (H5: a raw triple; the dtor frees the data as retail's does)
    Entry*    _field28;          // +0x28 consume list data (stride 0x10), freed by the dtor
    int32_t   _field30;          // +0x30 consume list count
    int32_t   _field34;          // +0x34 consume list capacity
    bool      _field38;          // +0x38 reserve flag (Persist: the bool overload)
    uint8_t   _pad39[7];         // +0x39 padding
};

static_assert(sizeof(ConsumeRawMaterial::Entry) == 0x10, "ConsumeRawMaterial consume-list stride 0x10");
static_assert(offsetof(ConsumeRawMaterial::Entry, count) == 0x08, "Entry +0x08");
static_assert(offsetof(ConsumeRawMaterial::Entry, reserved) == 0x0C, "Entry +0x0C");
static_assert(sizeof(ConsumeRawMaterial) == 0x40, "ConsumeRawMaterial is 0x40 (create 0x1400220c0 MemAlloc)");
static_assert(alignof(ConsumeRawMaterial) == 8, "ConsumeRawMaterial is 8-aligned");
static_assert(offsetof(ConsumeRawMaterial, _field20) == 0x20, "ConsumeRawMaterial +0x20");
static_assert(offsetof(ConsumeRawMaterial, _field28) == 0x28, "ConsumeRawMaterial +0x28");
static_assert(offsetof(ConsumeRawMaterial, _field30) == 0x30, "ConsumeRawMaterial +0x30");
static_assert(offsetof(ConsumeRawMaterial, _field34) == 0x34, "ConsumeRawMaterial +0x34");
static_assert(offsetof(ConsumeRawMaterial, _field38) == 0x38, "ConsumeRawMaterial +0x38");
