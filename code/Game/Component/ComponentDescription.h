#pragma once
// ============================================================================
// code/Game/Component/ComponentDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md
// D1-D5, §2.2)
//
// `ComponentDescription`, the entity-template container (the top-level type of
// every Template\*.rsc), as a real C++ class. It derives Object::External
// DIRECTLY — it is not a `Description` (no GetComponentType slot).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ComponentDescription", parent "Object::External", ti 0x1401c7318
//   create 0x140031f30: MemAlloc(0x38, 8), `??0External@Object@@`, the stamp,
//   the defaults. createInPlace 0x140031f90.
//   table 0x140162a00, 9 slots:
//     [0] 0x140032030 scalar deleting dtor   [1] 0x140031f20 TypeName
//     [2] 0x140031f10 Type                   [3] 0x140118b90 GetFileVersion = 2
//     [4] External::Compile (import, inherited)
//     [5] 0x140118af0 Serialize              [6] 0x140031fe0 Reconstruct
//     [7] 0x140001640 Destruct (shared)      [8] 0x140031f90 Construct
//         (the leaf-introduced last slot, folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

#include "Component/Description.h"   // FindDescription<T> reads each sub-description

class ComponentDescription final : public Object::External {
    DECLARE_EXE_TYPE(ComponentDescription)

public:
    ComponentDescription();

    ~ComponentDescription() override;                                      // [0] 0x140032030
    int32_t           GetFileVersion() const override { return 2; }        // [3] 0x140118b90
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x140118af0
    Object::External* Reconstruct() override;                              // [6] 0x140031fe0
    void              Destruct() override;                                 // [7] 0x140001640
    virtual Object::External* Construct();                                 // [8] 0x140031f90

    // ---- fields (PUBLIC: ComponentEntity and the finders read them at raw offsets) ----
    uint32_t _category;          // +0x20 enum, default 6 (Serialize 0x140118af0: the uint door)
    uint8_t  _pad24[4];          // +0x24 never written by the ctor
    Description** _subDescs;     // +0x28 array of sub-descriptions (stride 8; each answers
                                 //       Description slot 8 GetComponentType, ComponentEntity ctor 0x1400c3720)
    int32_t  _subDescCount;      // +0x30
    int32_t  _subDescCap;        // +0x34

    // ---- FindDescription<T> (the template retail instantiates per description class)
    // One shape at every instance read (0x140006330 RawMaterialDescription, 0x14000c3c0
    // GrowthDescription, 0x140005160 ToolbarDescription, 0x140026990 MapDescription):
    //   for (i = 0; i < [this+0x30]; ++i)                 // count re-read per pass
    //       if ([this+0x28][i]->Type() == T's cell)        // slot 2, [vtbl+0x10]
    //           return IsDerivedFrom([this+0x28][i], T's cell) ? it : 0;   // IAT 0x140152a98
    //   return 0;
    // stored into the caller's slot (0x1400063b7 `MOV [R15],R14`): retail returns the
    // ExternalPtr<T> by value, the same eight bytes, so the member returns the pointer
    // TYPED (as ComponentEntity::FindComponent<T>). No CheckPtr, no null test on the
    // description or on a slot: retail has none.
    template <class T>
    T* FindDescription() const {
        for (int32_t i = 0; i < _subDescCount; ++i) {
            if (_subDescs[i]->Type() == T::TypeId())
                return _subDescs[i]->IsDerivedFrom(T::TypeId()) ? static_cast<T*>(_subDescs[i]) : nullptr;
        }
        return nullptr;
    }
};

static_assert(sizeof(ComponentDescription) == 0x38, "ComponentDescription is 0x38 (create 0x140031f30 MemAlloc(0x38))");
static_assert(offsetof(ComponentDescription, _pad24) == 0x24, "ComponentDescription::_pad24");
static_assert(alignof(ComponentDescription) == 8, "ComponentDescription alignment");
static_assert(offsetof(ComponentDescription, _category)     == 0x20, "ComponentDescription::_category @ +0x20");
static_assert(offsetof(ComponentDescription, _subDescs)     == 0x28, "ComponentDescription::_subDescs @ +0x28");
static_assert(offsetof(ComponentDescription, _subDescCount) == 0x30, "ComponentDescription::_subDescCount @ +0x30");
static_assert(offsetof(ComponentDescription, _subDescCap)   == 0x34, "ComponentDescription::_subDescCap @ +0x34");
