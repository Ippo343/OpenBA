#pragma once
// Description types — binary-compatible struct layouts for tool/visual descriptions.
// Application-x64.exe, build 170910.
//
// All Description types inherit from Object::External (0x20 bytes).
// Base Description vtable at 0x1401629a0 — shared intermediate base.
// Each concrete description overwrites with its own vtable.
//
// Two categories:
//   1. Tool descriptions (CreateXxxDescription) — describe how to create game objects
//   2. UI element descriptions (FrustumOutline, CaptureBox) — extend UI::ElementDescription (0x68 base)

#include <cstdint>
#include <cstddef>

namespace DescriptionAddr {
    constexpr uintptr_t BaseVtable = 0x1401629a0;
}

// -------------------------------------------------------
// CreatePathDescription — path/trail creation. Size = 0x20.
// Decompiled from: CreatePathDescription_Factory (0x140004c30)
// Smallest description — just the base External with vtable override.
// -------------------------------------------------------
namespace CreatePathDescriptionAddr {
    constexpr uintptr_t Factory = 0x140004c30;
    constexpr uintptr_t Vtable  = 0x140159210;
}

struct CreatePathDescription {
    void*    vtable;             // +0x00: -> 0x140159210
    uint8_t  _extBase[0x18];     // +0x08: External base
};
static_assert(sizeof(CreatePathDescription) == 0x20, "CreatePathDescription size mismatch");

// -------------------------------------------------------
// CreateRoadDescription — road creation. Size = 0x30.
// Decompiled from: CreateRoadDescription_Factory (0x140032d30, inline ctor)
// -------------------------------------------------------
namespace CreateRoadDescriptionAddr {
    constexpr uintptr_t Factory = 0x140032d30;
    constexpr uintptr_t Vtable  = 0x140162df0;
}

struct CreateRoadDescription {
    void*    vtable;             // +0x00: -> 0x140162df0
    uint8_t  _extBase[0x18];     // +0x08: External base
    uint8_t  _field20;           // +0x20: zeroed (bool?)
    uint8_t  _pad21[7];          // +0x21: padding
    void*    _field28;           // +0x28: zeroed
};
static_assert(sizeof(CreateRoadDescription) == 0x30, "CreateRoadDescription size mismatch");

// -------------------------------------------------------
// RadiusDescription — radius overlay display. Size = 0x30.
// Decompiled from: RadiusDescription_Factory (0x14000b9e0, inline ctor)
// -------------------------------------------------------
namespace RadiusDescriptionAddr {
    constexpr uintptr_t Factory = 0x14000b9e0;
    constexpr uintptr_t Vtable  = 0x14015a580;
}

struct RadiusDescription {
    void*    vtable;             // +0x00: -> 0x14015a580
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    int32_t  radius;             // +0x28: 30 (0x1e) — default radius
    uint8_t  _pad2C[4];          // +0x2C: padding
};
static_assert(sizeof(RadiusDescription) == 0x30, "RadiusDescription size mismatch");

// -------------------------------------------------------
// CreateDragDescription — drag-placement creation params. Size = 0x40.
// Decompiled from: CreateDragDescription_Ctor (0x140119860)
// -------------------------------------------------------
namespace CreateDragDescriptionAddr {
    constexpr uintptr_t Ctor    = 0x140119860;
    constexpr uintptr_t Factory = 0x140032970;
    constexpr uintptr_t Vtable  = 0x140162ca0;
}

struct CreateDragDescription {
    void*    vtable;             // +0x00: -> 0x140162ca0
    uint8_t  _extBase[0x18];     // +0x08: External base
    uint32_t _field20;           // +0x20: zeroed
    uint8_t  _field24;           // +0x24: zeroed (bool?)
    uint8_t  _pad25[3];          // +0x25: padding
    int32_t  minWidth;           // +0x28: 1
    int32_t  minHeight;          // +0x2C: 1
    int32_t  maxWidth;           // +0x30: 1
    int32_t  maxHeight;          // +0x34: 1
    void*    _field38;           // +0x38: zeroed
};
static_assert(sizeof(CreateDragDescription) == 0x40, "CreateDragDescription size mismatch");

// -------------------------------------------------------
// CreateRemovalDescription — removal tool params. Size = 0x48.
// Decompiled from: CreateRemovalDescription_Factory (0x140032b70, inline ctor)
// -------------------------------------------------------
namespace CreateRemovalDescriptionAddr {
    constexpr uintptr_t Factory = 0x140032b70;
    constexpr uintptr_t Vtable  = 0x140162d80;
}

struct CreateRemovalDescription {
    void*    vtable;             // +0x00: -> 0x140162d80
    uint8_t  _extBase[0x18];     // +0x08: External base
    uint8_t  _field20;           // +0x20: zeroed
    uint8_t  _pad21[7];          // +0x21: padding
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    uint8_t  _pad44[4];          // +0x44: padding
};
static_assert(sizeof(CreateRemovalDescription) == 0x48, "CreateRemovalDescription size mismatch");

// -------------------------------------------------------
// SelectDescription — selection display params. Size = 0x48.
// Decompiled from: SelectDescription_Factory (0x14000bb90, inline ctor)
// -------------------------------------------------------
namespace SelectDescriptionAddr {
    constexpr uintptr_t Factory = 0x14000bb90;
    constexpr uintptr_t Vtable  = 0x14015a5f0;
}

struct SelectDescription {
    void*    vtable;             // +0x00: -> 0x14015a5f0
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    uint8_t  _pad44[4];          // +0x44: padding
};
static_assert(sizeof(SelectDescription) == 0x48, "SelectDescription size mismatch");

// -------------------------------------------------------
// ValueDescription — value overlay display params. Size = 0x50.
// Decompiled from: ValueDescription_Factory (0x14000faa0, inline ctor)
// -------------------------------------------------------
namespace ValueDescriptionAddr {
    constexpr uintptr_t Factory = 0x14000faa0;
    constexpr uintptr_t Vtable  = 0x14015add0;
}

struct ValueDescription {
    void*    vtable;             // +0x00: -> 0x14015add0
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    int64_t  defaultValue;       // +0x44: 100 (packed with alignment)
    uint8_t  _pad4C[4];          // +0x4C: padding
};
static_assert(sizeof(ValueDescription) == 0x50, "ValueDescription size mismatch");

// -------------------------------------------------------
// CreatePlacedDescription — placed building params. Size = 0x58.
// Decompiled from: CreatePlacedDescription_Ctor (0x140119a60)
// -------------------------------------------------------
namespace CreatePlacedDescriptionAddr {
    constexpr uintptr_t Ctor    = 0x140119a60;
    constexpr uintptr_t Factory = 0x140032a60;
    constexpr uintptr_t Vtable  = 0x140162cf0;
}

struct CreatePlacedDescription {
    void*    vtable;             // +0x00: -> 0x140162cf0
    uint8_t  _extBase[0x18];     // +0x08: External base
    uint32_t _field20;           // +0x20: zeroed
    uint8_t  _pad24[4];          // +0x24: padding
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    int32_t  _field44;           // +0x44: 1 (grid alignment?)
    int32_t  _field48;           // +0x48: 1 (snap to grid?)
    uint8_t  _pad4C[4];          // +0x4C: padding
    void*    _field50;           // +0x50: zeroed
};
static_assert(sizeof(CreatePlacedDescription) == 0x58, "CreatePlacedDescription size mismatch");

// -------------------------------------------------------
// CreateBridgeDescription — bridge creation params. Size = 0x60.
// Decompiled from: CreateBridgeDescription_Ctor (0x1401196f0)
// -------------------------------------------------------
namespace CreateBridgeDescriptionAddr {
    constexpr uintptr_t Ctor    = 0x1401196f0;
    constexpr uintptr_t Factory = 0x140032880;
    constexpr uintptr_t Vtable  = 0x140162c30;
}

struct CreateBridgeDescription {
    void*    vtable;             // +0x00: -> 0x140162c30
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
};
static_assert(sizeof(CreateBridgeDescription) == 0x60, "CreateBridgeDescription size mismatch");

// -------------------------------------------------------
// ToolbarDescription — toolbar layout description. Size = 0x68.
// Decompiled from: ToolbarDescription_Ctor (0x14012dc30)
// -------------------------------------------------------
namespace ToolbarDescriptionAddr {
    constexpr uintptr_t Ctor          = 0x14012dc30;
    constexpr uintptr_t Factory       = 0x140039490;
    constexpr uintptr_t Vtable        = 0x140164980;
    constexpr uintptr_t CreateInPlace = 0x1400394d0;
}

struct ToolbarDescription {
    void*    vtable;             // +0x00: -> 0x140164980
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
    int32_t  _field60;           // +0x60: -1 sentinel (selected index?)
    uint32_t _field64;           // +0x64: zeroed
};
static_assert(sizeof(ToolbarDescription) == 0x68, "ToolbarDescription size mismatch");

// -------------------------------------------------------
// CreateFencedDescription — fenced area creation params. Size = 0x78.
// Decompiled from: CreateFencedDescription_Ctor (0x140047a10)
// -------------------------------------------------------
namespace CreateFencedDescriptionAddr {
    constexpr uintptr_t Ctor    = 0x140047a10;
    constexpr uintptr_t Factory = 0x1400048e0;
    constexpr uintptr_t Vtable  = 0x1401591a0;
}

struct CreateFencedDescription {
    void*    vtable;             // +0x00: -> 0x1401591a0
    uint8_t  _extBase[0x18];     // +0x08: External base
    uint32_t _field20;           // +0x20: zeroed
    uint8_t  _pad24[4];          // +0x24: padding
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    uint8_t  _pad58[0x10];       // +0x58: gap
    uint8_t  _field68;           // +0x68: zeroed (bool?)
    uint8_t  _pad69[7];          // +0x69: padding
    void*    _field70;           // +0x70: zeroed
};
static_assert(sizeof(CreateFencedDescription) == 0x78, "CreateFencedDescription size mismatch");

// -------------------------------------------------------
// EnvironmentDescription — environment/lighting description. Size = 0x1B8.
// Decompiled from: EnvironmentDescription_Ctor (0x1400941c0)
// Contains 12 repeating atmosphere blocks (0x20 bytes each) at +0x38.
// Each block has: ambient ARGB, sun ARGB, sky ARGB, fogStart, fogEnd,
//   fogDensity, sunIntensity, ambientIntensity.
// -------------------------------------------------------
namespace EnvironmentDescriptionAddr {
    constexpr uintptr_t Ctor    = 0x1400941c0;
    constexpr uintptr_t Factory = 0x140019120;
    constexpr uintptr_t Vtable  = 0x14015e1f0;
}

struct AtmosphereBlock {
    uint32_t ambientColor;       // +0x00: 0xff595965
    uint32_t sunColor;           // +0x04: 0xffcbcbbc
    uint32_t skyColor;           // +0x08: 0xffbfbff2
    float    fogStart;           // +0x0C: 90.0f
    float    fogEnd;             // +0x10: 225.0f
    float    fogDensity;         // +0x14: 0.95f
    float    sunIntensity;       // +0x18: 120.0f
    float    ambientIntensity;   // +0x1C: 65.0f
};
static_assert(sizeof(AtmosphereBlock) == 0x20, "AtmosphereBlock size mismatch");

struct EnvironmentDescription {
    void*    vtable;             // +0x00: -> 0x14015e1f0
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    AtmosphereBlock atmosphere[12]; // +0x38: 12 atmosphere configs (0x180 bytes)
                                    // Loop runs 12 times (iVar2=0xb, counts down to >=0)
};
static_assert(sizeof(EnvironmentDescription) == 0x1B8, "EnvironmentDescription size mismatch");

// -------------------------------------------------------
// UI Element Descriptions (extend UI::ElementDescription, 0x68 base)
// -------------------------------------------------------

// CaptureBoxDescription — capture/screenshot box. Size = 0x78.
// Decompiled from: CaptureBoxDescription_Factory (0x140013d90, inline ctor)
namespace CaptureBoxDescriptionAddr {
    constexpr uintptr_t Factory = 0x140013d90;
    constexpr uintptr_t Vtable  = 0x14015d4a0;
}

struct CaptureBoxDescription {
    void*    vtable;             // +0x00: -> 0x14015d4a0
    uint8_t  _elemBase[0x60];    // +0x08: UI::ElementDescription base (0x68 total)
    void*    _field68;           // +0x68: zeroed
    void*    _field70;           // +0x70: zeroed
};
static_assert(sizeof(CaptureBoxDescription) == 0x78, "CaptureBoxDescription size mismatch");

// FrustumOutlineDescription — frustum outline display. Size = 0x80.
// Decompiled from: FrustumOutlineDescription_Factory (0x1400122c0, inline ctor)
namespace FrustumOutlineDescriptionAddr {
    constexpr uintptr_t Factory = 0x1400122c0;
    constexpr uintptr_t Vtable  = 0x14015c300;
}

struct FrustumOutlineDescription {
    void*    vtable;             // +0x00: -> 0x14015c300
    uint8_t  _elemBase[0x60];    // +0x08: UI::ElementDescription base (0x68 total)
    void*    _field68;           // +0x68: zeroed
    void*    _field70;           // +0x70: zeroed
    int64_t  _field78;           // +0x78: -1 (all 0xFF bytes)
};
static_assert(sizeof(FrustumOutlineDescription) == 0x80, "FrustumOutlineDescription size mismatch");
