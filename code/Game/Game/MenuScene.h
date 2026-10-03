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
// references, an embedded camera controller at +0x88, atmosphere/lighting colors
// and fog parameters (same defaults as Weather), two tree/map structures.
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
//
// B.4 GAME-b: the members are typed. The name String, the four arrays and the
// camera controller are real members: the tree's ctor stored the same zeros /
// ran the same 0x140098800 body on them, and its dtor freed the arrays (zeroing
// data and the count|capacity qword) and ran ~String in the compiler's member
// order. The element layouts are the menu loop's reads (MenuDialogLoopT::Load
// 0x14009a930 and its helpers 0x1400a3b60 / 0x1400a3ca0) and the serializers'.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"
#include "collection/Collection.h"
#include "system/String.h"
#include "system/Value.h"            // System::Color
#include "math/Math.h"               // Math::Vector3 / Vector4
#include "Game/PlayerController.h"   // +0x88

class WorldGeometry;
class Weather;
namespace Graphics { class GraphicsMesh; class Particle; }

// One static mesh of the scene (+0x30, stride 0x20; 0x1400a3b60 places it).
struct MenuSceneMesh {
    Math::Vector3 position;     // +0x00 FromYawPitchOffset's offset
    float         color[3];     // +0x0C copied to MeshInstance::_color[0..2] (+0x10c..+0x114)
    float         yaw;          // +0x18
    int16_t       meshIndex;    // +0x1C into MenuScene::meshResources
    int16_t       skinIndex;    // +0x1E GraphicsMesh::GetMaterialList's argument
};
static_assert(sizeof(MenuSceneMesh) == 0x20, "MenuSceneMesh stride 0x20 (0x14001baf0)");
static_assert(offsetof(MenuSceneMesh, color) == 0x0C, "MenuSceneMesh::color @ +0x0C");
static_assert(offsetof(MenuSceneMesh, yaw) == 0x18, "MenuSceneMesh::yaw @ +0x18");
static_assert(offsetof(MenuSceneMesh, meshIndex) == 0x1C, "MenuSceneMesh::meshIndex @ +0x1C");
static_assert(offsetof(MenuSceneMesh, skinIndex) == 0x1E, "MenuSceneMesh::skinIndex @ +0x1E");

// One particle emitter of the scene (+0x50, stride 0x14; 0x1400a3ca0 places it).
struct MenuSceneParticle {
    Math::Vector3 position;       // +0x00
    float         yaw;            // +0x0C
    int32_t       particleIndex;  // +0x10 into MenuScene::particleResources
};
static_assert(sizeof(MenuSceneParticle) == 0x14, "MenuSceneParticle stride 0x14 (0x14001bf10)");
static_assert(offsetof(MenuSceneParticle, yaw) == 0x0C, "MenuSceneParticle::yaw @ +0x0C");
static_assert(offsetof(MenuSceneParticle, particleIndex) == 0x10, "MenuSceneParticle::particleIndex @ +0x10");

class MenuScene final : public Object::External {
    DECLARE_EXE_TYPE(MenuScene)

public:
    MenuScene();                                                  // 0x1400a3d70

    // body 0x1400a3970
    ~MenuScene() override;                                        // [0] 0x14001b830
    int32_t GetFileVersion() const override { return 1; }         // [3] 0x1400a3960
    CompileResult Compile(const System::Filename&) override { return 1; }   // [4] 0x1400a3620
    bool Serialize(IO::Stream& stream) override;                        // [5] 0x1400a3630
    Object::External* Reconstruct() override;                     // [6] 0x14001b810
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x14001b800

public:
    System::String   name;              // +0x20: the scene name, serialized first
    WorldGeometry*   geometry;          // +0x28: the terrain (scalar-deleted in the dtor)

    Collection::Array<MenuSceneMesh>           meshes;             // +0x30 (0x14001bbf0 serializes it)
    Collection::Array<Graphics::GraphicsMesh*> meshResources;      // +0x40 ExternalPtr<GraphicsMesh> (0x14001bcc0)
    Collection::Array<MenuSceneParticle>       particles;          // +0x50
    Collection::Array<Graphics::Particle*>     particleResources;  // +0x60 ExternalPtr<Particle> (0x14001bff0)

    Graphics::GraphicsMesh* terrainMesh;   // +0x70: ExternalPtr<GraphicsMesh> (0x14001b8e0); the menu loop's main mesh
    float    _field78;           // +0x78: float (version >= 2)
    uint8_t  _pad7C[4];          // +0x7C: padding
    Weather* weather;            // +0x80: ExternalPtr<Weather> (0x1400196a0)

    PlayerController transform;  // +0x88: the camera controller (0x140098800; serialized as 0x1400988c0 does)

    // --- Atmosphere/lighting colors (same defaults as Weather) ---
    System::Color ambientColor;  // +0x194: 0xff595965
    System::Color sunColor;      // +0x198: 0xffcbcbbc
    System::Color skyColor;      // +0x19C: 0xffbfbff2
    float    fogStart;           // +0x1A0: 90.0f
    float    fogEnd;             // +0x1A4: 225.0f
    float    fogDensity;         // +0x1A8: 0.95f
    float    sunIntensity;       // +0x1AC: 120.0f (the menu loop reads it as the sun's yaw)
    float    ambientIntensity;   // +0x1B0: 65.0f (the menu loop reads it as the sun's pitch)
    Math::Vector4 field_0x1B4;   // +0x1B4: zero; serialized; copied to Video::Context +0x3b8 / +0x288
    uint8_t  _pad1C4[4];         // +0x1C4: padding

    // Tree/map structure 1 at +0x1C8 ({Array, root, count}; Game_MenuSceneArrayDtor)
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
static_assert(offsetof(MenuScene, name) == 0x20, "+0x20");
static_assert(offsetof(MenuScene, geometry) == 0x28, "+0x28");
static_assert(offsetof(MenuScene, meshes) == 0x30, "+0x30");
static_assert(offsetof(MenuScene, meshResources) == 0x40, "+0x40");
static_assert(offsetof(MenuScene, particles) == 0x50, "+0x50");
static_assert(offsetof(MenuScene, particleResources) == 0x60, "+0x60");
static_assert(offsetof(MenuScene, terrainMesh) == 0x70, "+0x70");
static_assert(offsetof(MenuScene, _field78) == 0x78, "+0x78");
static_assert(offsetof(MenuScene, weather) == 0x80, "+0x80");
static_assert(offsetof(MenuScene, transform) == 0x88, "+0x88");
static_assert(offsetof(MenuScene, ambientColor) == 0x194, "+0x194");
static_assert(offsetof(MenuScene, sunColor) == 0x198, "+0x198");
static_assert(offsetof(MenuScene, skyColor) == 0x19C, "+0x19C");
static_assert(offsetof(MenuScene, fogStart) == 0x1A0, "+0x1A0");
static_assert(offsetof(MenuScene, fogEnd) == 0x1A4, "+0x1A4");
static_assert(offsetof(MenuScene, fogDensity) == 0x1A8, "+0x1A8");
static_assert(offsetof(MenuScene, sunIntensity) == 0x1AC, "+0x1AC");
static_assert(offsetof(MenuScene, ambientIntensity) == 0x1B0, "+0x1B0");
static_assert(offsetof(MenuScene, field_0x1B4) == 0x1B4, "+0x1B4");
static_assert(offsetof(MenuScene, _pad1C4) == 0x1C4, "+0x1C4");
static_assert(offsetof(MenuScene, _field1C8) == 0x1C8, "+0x1C8");
static_assert(offsetof(MenuScene, _field1E8) == 0x1E8, "+0x1E8");
static_assert(offsetof(MenuScene, _field200) == 0x200, "+0x200");
