// ============================================================================
// tests/unit/video_data_abi_test.cpp — ENDGAME B.3.5 block A.
//
// The layout relations the headers leave unsaid for the twelve `Video::*Data`
// companion bases (src/engine/video/CompanionData.h) and `Video::VertexLayout`
// (src/engine/video/VertexLayout.h). Sizes and offsets are the headers' own
// static_asserts. The mirror classes that used to restate the slot order are
// gone: a mirror declared in the test can only ever test itself; the real
// classes' slot order is pinned by pointer-to-member in
// tests/unit/video_device_abi_test.cpp and the b39_* tests.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "unit.h"

#include "video/CompanionData.h"
#include "video/VertexLayout.h"

UNIT_TEST(video_companion_data_layout) {
    // vptr + owner, and the compiler is free to choose neither the order nor
    // the size — retail chose both.
    CHECK_EQ(sizeof(Video::DeviceData),         sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::ContextData),        sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::ImageBufferData),    sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::IndexBufferData),    sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::VertexBufferData),   sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::ConstantBufferData), sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::RenderBufferData),   sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::VertexLayoutData),   sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::VertexProgramData),  sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::PixelProgramData),   sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::RenderStateData),    sizeof(void*) * 2);
    CHECK_EQ(sizeof(Video::VideoProgramData),   sizeof(void*) * 2);

    // `RtVideo::ImageBufferData` and `RtVideo::CompanionData` describe the same
    // bytes as the real classes — the B4b-1 move in this block did not change
    // them, it removed the second copy (`ImageBuffer.h`'s).
    CHECK_EQ(sizeof(RtVideo::CompanionData), sizeof(Video::DeviceData));
}

// ===========================================================================
// PART D — `Video::VertexLayout`, the header block A had to AUTHOR.
//
// 0x48 bytes, `Object::CompiledExternal` base, nine slots, and an `Element`
// whose field map this block corrected against
// `?Add@VertexLayout@Video@@QEAAXHHW4Usage@12@HW4Format@12@@Z` (0x1800f5a90),
// `?GetStreamCount@` (0x1800f5a30) and VideoDX11's `PlatformCreate`
// (0x180003ff0). The header carries the same pins as `static_assert`; these are
// the runtime form, so a failure NAMES the field instead of stopping the build
// at a line number.
// ===========================================================================
UNIT_TEST(video_vertexlayout_layout) {
    CHECK_EQ(alignof(Video::VertexLayout), (size_t)8);
    // The base adds 0x20 and no bytes of its own beyond `Object::External`'s.

    // The element array's count is at +0x28 and the signature's at +0x40 —
    // `Serialize` (0x1800f5e70) reads exactly those two dwords, and VideoDX11's
    // PlatformCreate guards on +0x40 and loops on +0x28.
    CHECK_EQ(offsetof(Video::VertexLayout, _elements) +
                 offsetof(Collection::Array<Video::VertexLayout::Element>, _size),
             (size_t)0x28);
    CHECK_EQ(offsetof(Video::VertexLayout, _signature) +
                 offsetof(Collection::Array<uint8_t>, _size),
             (size_t)0x40);

    // Was "not constructible" (B34_BRIEFS A.4: a protected default constructor
    // kept `Video::VertexLayout vl;` from stamping GCC's table on an object read
    // through the MINTED one). ENDGAME B.3.9 VID made the constructor retail's
    // public one (the mint row is retired) and B3X W1 made the destructor
    // public and virtual, so the class is default-constructible now.
    CHECK(std::is_default_constructible<Video::VertexLayout>::value);
}
