#pragma once
// ============================================================================
// code/Game/Game/Minimap.h — ENDGAME B.4 GAME-a
//
// The 0x38-byte minimap image at GameUpdateLoop+0x148 (m_imageCapture): two
// map-sized 32-bit pixel arrays and the two GPU images they are copied into.
//
// CLASS NAME UNCONFIRMED: retail gives it no reflected name or vtable;
// "Minimap" is descriptive (the tree's Minimap_Serialize / Minimap_PaintTerrain).
// No vtable: a plain struct.
//
// RETAIL (Application-x64.exe):
//   Load 0x140096b00: MemAlloc(0x38, 4); byte +0x00 = 0, dword +0x04 = 0,
//     qwords +0x08..+0x30 = 0; the new-game branch sets +0x04 = map size and
//     sizes the +0x28 / +0x18 arrays to size^2 (0x1400021d0), zeroing +0x28
//   Serialize 0x1400a57f0: count + Serialize(n, uint*) of +0x28, then of
//     +0x18, then int +0x04; on load: 0x1400a4e80, then
//     ImageBuffer::CopyToTexture(+0x08, +0x18, size, size) and
//     (+0x10, +0x28, size, size)
//   Update 0x140096380: +0x08 non-null and +0x00 set ->
//     CopyToTexture(+0x10, +0x28, size, size), +0x00 = 0
//   Draw 0x140096010: +0x08 null -> paint the terrain (0x1400a5140)
//   dtor 0x1400a4fc0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "collection/Collection.h"   // Collection::Array (the pixel arrays)

namespace Video { class ImageBuffer; class Context; }
namespace Graphics { class Geometry; }
namespace IO { class Stream; }

class Minimap {
public:
    uint8_t             overlayDirty;          // +0x00 Update uploads overlayPixels when set, then clears it
    uint8_t             _pad01[3];             // +0x01
    int32_t             size;                  // +0x04 image edge = the map size (Serialize int)
    Video::ImageBuffer* terrainImage;          // +0x08 terrainPixels' texture (null until painted)
    Video::ImageBuffer* overlayImage;          // +0x10 overlayPixels' texture
    Collection::Array<uint32_t> terrainPixels; // +0x18 Array<uint> (size^2; count +0x20, capacity +0x24)
    Collection::Array<uint32_t> overlayPixels; // +0x28 Array<uint> (size^2, zeroed on a new game; count +0x30, capacity +0x34)

    // Load 0x140097276..0x140097291 (inlined): byte +0x00 = 0, dword +0x04 = 0, the two
    // image pointers, then the two Arrays' {data, size + capacity} qwords; +0x01..+0x03
    // are not written.
    Minimap() : overlayDirty(0), size(0), terrainImage(nullptr), overlayImage(nullptr) {}
    // 0x1400a4fc0: the two images through their virtual scalar-deleting dtor (slot 0, flag
    // 1); then ~overlayPixels (+0x28) and ~terrainPixels (+0x18): MemFree, data = 0, the
    // size + capacity qword = 0 (reverse declaration order).
    ~Minimap();

    // ---- members (B4 GAME-a; bodies in gameupdateloop_serialize.cpp) ----
    void Serialize(IO::Stream& stream);   // 0x1400a57f0
    // B4 GAME-b (minimap_paint.cpp / minimap_upload.cpp):
    void PaintTerrain(Video::Context& context, Graphics::Geometry* worldGeom,
                      Graphics::Geometry* sunLight);   // 0x1400a5140 (ImageCapture::DrawCheck)
    void CreateAndUpload();   // 0x1400a4e80's two ImageBuffers, then both CopyToTexture uploads
    void ClearTerrain(int value, int x, int y, int w, int h);   // 0x1400a4d30 (Component/mapcomponent.cpp)
};

static_assert(sizeof(Minimap) == 0x38, "Minimap is 0x38 (Load MemAlloc(0x38, 4))");
static_assert(offsetof(Minimap, overlayDirty) == 0x00, "Minimap::overlayDirty @ +0x00");
static_assert(offsetof(Minimap, _pad01) == 0x01, "Minimap +0x01");
static_assert(offsetof(Minimap, size) == 0x04, "Minimap::size @ +0x04");
static_assert(offsetof(Minimap, terrainImage) == 0x08, "Minimap::terrainImage @ +0x08");
static_assert(offsetof(Minimap, overlayImage) == 0x10, "Minimap::overlayImage @ +0x10");
static_assert(offsetof(Minimap, terrainPixels) == 0x18, "Minimap::terrainPixels @ +0x18");
static_assert(offsetof(Minimap, terrainPixels) + offsetof(Collection::Array<uint32_t>, _size) == 0x20, "Minimap terrain count @ +0x20");
static_assert(offsetof(Minimap, terrainPixels) + offsetof(Collection::Array<uint32_t>, _capacity) == 0x24, "Minimap terrain capacity @ +0x24");
static_assert(offsetof(Minimap, overlayPixels) == 0x28, "Minimap::overlayPixels @ +0x28");
static_assert(offsetof(Minimap, overlayPixels) + offsetof(Collection::Array<uint32_t>, _size) == 0x30, "Minimap overlay count @ +0x30");
static_assert(offsetof(Minimap, overlayPixels) + offsetof(Collection::Array<uint32_t>, _capacity) == 0x34, "Minimap overlay capacity @ +0x34");
