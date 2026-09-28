#pragma once
// ============================================================================
// code/Game/Component/TradeDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TradeDescription", parent "Description", ti 0x1401c4bb8
//   create 0x14000dbd0: MemAlloc(0x70, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000dc10.
//   table 0x14015ad60, 10 slots:
//     [0] 0x14000dc50 scalar deleting dtor   [1] 0x14000dbc0 TypeName   [2] 0x14000dbb0 Type
//     [3] 0x14005ca00 GetFileVersion = 7   [4] External::Compile (import, inherited)
//     [5] 0x14005c7f0 Serialize (`MOV AL,1`)
//     [6] 0x14000dc20 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000dc40 GetComponentType: TradeComponent's TypeId
//     [9] 0x14000dc10 Construct (folded with createInPlace)
//   sizeof 0x70.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class TradeDescription final : public Description {
    DECLARE_EXE_TYPE(TradeDescription)

public:
    TradeDescription();

    ~TradeDescription() override;                                          // [0] 0x14000dc50
    int32_t           GetFileVersion() const override { return 7; }    // [3] 0x14005ca00
    bool              Serialize(void* stream) override;                    // [5] 0x14005c7f0
    Object::External* Reconstruct() override;                              // [6] 0x14000dc20
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000dc40
    virtual Object::External* Construct();                                 // [9] 0x14000dc10

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _ref20;                               // +0x20 ExternalPtr (a raw pointer)
    void* _ref28;                               // +0x28 ExternalPtr (a raw pointer)
    void* _ref30;                               // +0x30 ExternalPtr (a raw pointer)
    void* _pairs;                               // +0x38 string-pair array data (0x18 stride: enum, String, String)
    int32_t _pairCount;                         // +0x40
    int32_t _pairCap;                           // +0x44
    void* _merchants;                           // +0x48 merchant array data (0x48 stride)
    int32_t _merchantCount;                     // +0x50
    int32_t _merchantCap;                       // +0x54
    void* _prices;                              // +0x58 price array data (0x14 stride)
    int32_t _priceCount;                        // +0x60
    int32_t _priceCap;                          // +0x64
    float _f68;                                 // +0x68 ~0.333
    int32_t _i6c;                               // +0x6c
};

static_assert(sizeof(TradeDescription) == 0x70, "TradeDescription is 0x70 (create 0x14000dbd0 MemAlloc(0x70))");
static_assert(alignof(TradeDescription) == 8, "TradeDescription alignment");
static_assert(offsetof(TradeDescription, _ref20) == 0x20, "TradeDescription::_ref20 @ +0x20");
static_assert(offsetof(TradeDescription, _ref28) == 0x28, "TradeDescription::_ref28 @ +0x28");
static_assert(offsetof(TradeDescription, _ref30) == 0x30, "TradeDescription::_ref30 @ +0x30");
static_assert(offsetof(TradeDescription, _pairs) == 0x38, "TradeDescription::_pairs @ +0x38");
static_assert(offsetof(TradeDescription, _pairCount) == 0x40, "TradeDescription::_pairCount @ +0x40");
static_assert(offsetof(TradeDescription, _pairCap) == 0x44, "TradeDescription::_pairCap @ +0x44");
static_assert(offsetof(TradeDescription, _merchants) == 0x48, "TradeDescription::_merchants @ +0x48");
static_assert(offsetof(TradeDescription, _merchantCount) == 0x50, "TradeDescription::_merchantCount @ +0x50");
static_assert(offsetof(TradeDescription, _merchantCap) == 0x54, "TradeDescription::_merchantCap @ +0x54");
static_assert(offsetof(TradeDescription, _prices) == 0x58, "TradeDescription::_prices @ +0x58");
static_assert(offsetof(TradeDescription, _priceCount) == 0x60, "TradeDescription::_priceCount @ +0x60");
static_assert(offsetof(TradeDescription, _priceCap) == 0x64, "TradeDescription::_priceCap @ +0x64");
static_assert(offsetof(TradeDescription, _f68) == 0x68, "TradeDescription::_f68 @ +0x68");
static_assert(offsetof(TradeDescription, _i6c) == 0x6c, "TradeDescription::_i6c @ +0x6c");
