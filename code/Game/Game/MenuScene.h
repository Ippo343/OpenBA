#pragma once
// ============================================================================
// code/Game/Game/MenuScene.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `MenuScene` (0x208): the main-menu 3D scene (Game/MenuScene/SceneN.rsc), a
// real `Object::External` subclass. Layout from ctor 0x1400a3d70, dtor body
// 0x1400a3970 and Serialize 0x1400a3630.
//
// Contains the scene name string, the scene Geometry, dynamic arrays of scene
// objects (0x20-byte, ExternalPtr, 0x14-byte, ExternalPtr elements), external
// references, an embedded Transform at +0x88, atmosphere/lighting colors and
// fog parameters (same defaults as Weather), two tree/map structures.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "MenuScene", parent "Object::External", table 0x14015ede0,
//   9 slots, sizeof 0x208.
//   [0] 0x14001b830 scalar deleting dtor (body 0x1400a3970)
//   [1] 0x14001b7b0 TypeName               [2] 0x14001b7a0 Type
//   [3] 0x1400a3960 GetFileVersion `MOV EAX,1`
//   [4] 0x1400a3620 Compile — OWN `MOV EAX,1` (not the import)
//   [5] 0x1400a3630 Serialize              [6] 0x14001b810 Reconstruct
//   [7] 0x140001640 Destruct               [8] 0x14001b800 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class MenuScene final : public Object::External {
    DECLARE_EXE_TYPE(MenuScene)

public:
    MenuScene();                                                  // 0x1400a3d70

    // body 0x1400a3970
    ~MenuScene() override;                                        // [0] 0x14001b830
    int32_t GetFileVersion() const override { return 1; }         // [3] 0x1400a3960
    CompileResult Compile(const System::Filename&) override { return 1; }   // [4] 0x1400a3620
    bool Serialize(void* stream) override;                        // [5] 0x1400a3630
    Object::External* Reconstruct() override;                     // [6] 0x14001b810
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x14001b800

public:
    void*    _field20;           // +0x20: System::String (scene name), serialized
    void*    _field28;           // +0x28: Geometry* (scene object, scalar-deleted in dtor)

    // Dynamic array 1: 0x20-byte elements (serialized in FUN_14001bbf0)
    void*    _field30;           // +0x30: array data pointer
    void*    _field38;           // +0x38: count (low 32b) | capacity (high 32b)
    // Dynamic array 2: ExternalPtr elements (serialized in FUN_14001bcc0)
    void*    _field40;           // +0x40: array data pointer
    void*    _field48;           // +0x48: count | capacity
    // Dynamic array 3: 0x14-byte elements (4 floats + 1 int)
    void*    _field50;           // +0x50: array data pointer
    void*    _field58;           // +0x58: count | capacity
    // Dynamic array 4: ExternalPtr elements (serialized in FUN_14001bff0)
    void*    _field60;           // +0x60: array data pointer
    void*    _field68;           // +0x68: count | capacity

    void*    _field70;           // +0x70: ExternalPtr (GraphicsMesh)
    float    _field78;           // +0x78: float (version >= 2)
    uint8_t  _pad7C[4];          // +0x7C: padding
    void*    _field80;           // +0x80: ExternalPtr (Weather)

    uint8_t  transform[0x10C];   // +0x88: Transform block (initialized by FUN_140098800)

    // --- Atmosphere/lighting colors (same defaults as Weather) ---
    uint32_t ambientColor;       // +0x194: 0xff595965
    uint32_t sunColor;           // +0x198: 0xffcbcbbc
    uint32_t skyColor;           // +0x19C: 0xffbfbff2
    float    fogStart;           // +0x1A0: 90.0f
    float    fogEnd;             // +0x1A4: 225.0f
    float    fogDensity;         // +0x1A8: 0.95f
    float    sunIntensity;       // +0x1AC: 120.0f
    float    ambientIntensity;   // +0x1B0: 65.0f
    uint8_t  _pad1B4[4];         // +0x1B4: zero (the Math::Vector4 serialized at +0x1B4 starts here)

    void*    _field1B8;          // +0x1B8: rest of the +0x1B4 Vector4
    uint32_t _field1C0;          // +0x1C0: zeroed
    uint8_t  _pad1C4[4];         // +0x1C4: padding

    // Tree/map structure 1 at +0x1C8
    void*    _field1C8;          // +0x1C8
    void*    _field1D0;          // +0x1D0
    void*    _field1D8;          // +0x1D8: root
    uint32_t _field1E0;          // +0x1E0: count
    uint8_t  _pad1E4[4];         // +0x1E4
    // Tree/map structure 2 at +0x1E8
    void*    _field1E8;          // +0x1E8
    void*    _field1F0;          // +0x1F0
    void*    _field1F8;          // +0x1F8: root
    uint32_t _field200;          // +0x200: count
    uint8_t  _pad204[4];         // +0x204
};

static_assert(sizeof(MenuScene) == 0x208, "MenuScene is 0x208");
static_assert(alignof(MenuScene) == 8, "qword-aligned");
static_assert(offsetof(MenuScene, _field20) == 0x20, "+0x20");
static_assert(offsetof(MenuScene, _field28) == 0x28, "+0x28");
static_assert(offsetof(MenuScene, _field30) == 0x30, "+0x30");
static_assert(offsetof(MenuScene, _field40) == 0x40, "+0x40");
static_assert(offsetof(MenuScene, _field50) == 0x50, "+0x50");
static_assert(offsetof(MenuScene, _field60) == 0x60, "+0x60");
static_assert(offsetof(MenuScene, _field70) == 0x70, "+0x70");
static_assert(offsetof(MenuScene, _field78) == 0x78, "+0x78");
static_assert(offsetof(MenuScene, _field80) == 0x80, "+0x80");
static_assert(offsetof(MenuScene, transform) == 0x88, "+0x88");
static_assert(offsetof(MenuScene, ambientColor) == 0x194, "+0x194");
static_assert(offsetof(MenuScene, sunColor) == 0x198, "+0x198");
static_assert(offsetof(MenuScene, skyColor) == 0x19C, "+0x19C");
static_assert(offsetof(MenuScene, fogStart) == 0x1A0, "+0x1A0");
static_assert(offsetof(MenuScene, fogEnd) == 0x1A4, "+0x1A4");
static_assert(offsetof(MenuScene, fogDensity) == 0x1A8, "+0x1A8");
static_assert(offsetof(MenuScene, sunIntensity) == 0x1AC, "+0x1AC");
static_assert(offsetof(MenuScene, ambientIntensity) == 0x1B0, "+0x1B0");
static_assert(offsetof(MenuScene, _field1B8) == 0x1B8, "+0x1B8");
static_assert(offsetof(MenuScene, _field1C0) == 0x1C0, "+0x1C0");
static_assert(offsetof(MenuScene, _field1C8) == 0x1C8, "+0x1C8");
static_assert(offsetof(MenuScene, _field1E8) == 0x1E8, "+0x1E8");
static_assert(offsetof(MenuScene, _field200) == 0x200, "+0x200");
