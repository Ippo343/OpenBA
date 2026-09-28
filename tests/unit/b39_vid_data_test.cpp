// ============================================================================
// tests/unit/b39_vid_data_test.cpp -- ENDGAME B.3 step 9, block VID
// (B39_RULES.md D1, rule B9-16): the twelve Video::*Data companion bases made
// CONCRETE.
//
// Per class: not abstract, no virtual destructor, the destructor PUBLIC (the one
// deliberate difference from the recipe -- RULE B5-3: dx11's derived destructors,
// which do not inherit in C++, call `self->Video::XData::~XData()`), slot 0 at
// byte 0 by PMF, the real constructor's one store (the owner at +0x08) and both
// arms of the inline VECTOR `DeletingDtor` (element 0x10). Slots 1..N are
// retail's `_purecall` and are never called here.
//
// The out-of-line destructors live in runtime/src/video/companion_data.cpp
// (which cannot link into the host test), so they are defined HERE as markers.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "runtime/types/video/CompanionData.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace {

int         g_dtorCount = 0;
const void* g_dtorOrder[8] = {};
void NoteDtor(const void* self) {
    if (g_dtorCount < 8) g_dtorOrder[g_dtorCount] = self;
    ++g_dtorCount;
}

void* const kOwner = reinterpret_cast<void*>(uintptr_t(0x0E0E0E0E0ull));

template <class T>
void CheckData() {
    CHECK(!std::is_abstract<T>::value);
    CHECK(!std::has_virtual_destructor<T>::value);
    CHECK(std::is_destructible<T>::value);          // PUBLIC: RULE B5-3
    CHECK(!std::is_default_constructible<T>::value); // the owner is required
    CHECK_DTOR_SLOT(T);

    // The ctor's one data store: owner at +0x08.
    alignas(8) uint8_t buf[0x10];
    std::memset(buf, 0xCD, sizeof buf);
    T* d = ::new (buf) T(kOwner);
    CHECK(static_cast<void*>(d) == static_cast<void*>(buf));
    CHECK(d->_owner == kOwner);
    g_dtorCount = 0;
    CHECK(static_cast<void*>(d->DeletingDtor(0)) == static_cast<void*>(d));
    CHECK_EQ(g_dtorCount, 1);

    // flags 1: destroy one and free it.
    void* m = Object::detail::InternalMemAlloc(0x10, 8);
    CHECK(m != nullptr);
    T* p = ::new (m) T(kOwner);
    g_dtorCount = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(1)) == static_cast<void*>(p));
    CHECK_EQ(g_dtorCount, 1);

    // flags 3: the count at this-8, destroyed downward, this-8 freed.
    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0x10, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    T* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * 0x10) T(kOwner);
    g_dtorCount = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_dtorCount, 3);
    CHECK(g_dtorOrder[0] == e[2]);
    CHECK(g_dtorOrder[1] == e[1]);
    CHECK(g_dtorOrder[2] == e[0]);
}

}  // namespace

namespace Video {
DeviceData::~DeviceData()                 { NoteDtor(this); }
ContextData::~ContextData()               { NoteDtor(this); }
ImageBufferData::~ImageBufferData()       { NoteDtor(this); }
IndexBufferData::~IndexBufferData()       { NoteDtor(this); }
VertexBufferData::~VertexBufferData()     { NoteDtor(this); }
ConstantBufferData::~ConstantBufferData() { NoteDtor(this); }
RenderBufferData::~RenderBufferData()     { NoteDtor(this); }
VertexLayoutData::~VertexLayoutData()     { NoteDtor(this); }
VertexProgramData::~VertexProgramData()   { NoteDtor(this); }
PixelProgramData::~PixelProgramData()     { NoteDtor(this); }
RenderStateData::~RenderStateData()       { NoteDtor(this); }
VideoProgramData::~VideoProgramData()     { NoteDtor(this); }
}  // namespace Video

UNIT_TEST(b39_vid_devicedata_concrete)         { CheckData<Video::DeviceData>(); }
UNIT_TEST(b39_vid_contextdata_concrete)        { CheckData<Video::ContextData>(); }
UNIT_TEST(b39_vid_imagebufferdata_concrete)    { CheckData<Video::ImageBufferData>(); }
UNIT_TEST(b39_vid_indexbufferdata_concrete)    { CheckData<Video::IndexBufferData>(); }
UNIT_TEST(b39_vid_vertexbufferdata_concrete)   { CheckData<Video::VertexBufferData>(); }
UNIT_TEST(b39_vid_constantbufferdata_concrete) { CheckData<Video::ConstantBufferData>(); }
UNIT_TEST(b39_vid_renderbufferdata_concrete)   { CheckData<Video::RenderBufferData>(); }
UNIT_TEST(b39_vid_vertexlayoutdata_concrete)   { CheckData<Video::VertexLayoutData>(); }
UNIT_TEST(b39_vid_vertexprogramdata_concrete)  { CheckData<Video::VertexProgramData>(); }
UNIT_TEST(b39_vid_pixelprogramdata_concrete)   { CheckData<Video::PixelProgramData>(); }
UNIT_TEST(b39_vid_renderstatedata_concrete)    { CheckData<Video::RenderStateData>(); }
UNIT_TEST(b39_vid_videoprogramdata_concrete)   { CheckData<Video::VideoProgramData>(); }

// The slot ORDER of the two 20-slot tables, by PMF: the Device-side callers
// reach the factories by BYTE OFFSET (VertexLayout +0x70, ConstantBuffer +0x58,
// VideoProgram +0x90), so these are the offsets that must not move.
UNIT_TEST(b39_vid_devicedata_factory_slots_by_pmf) {
    using D = Video::DeviceData;
    CHECK_SLOT(D, EnumerateInterfaces,      0x08/8);
    CHECK_SLOT(D, SetImageBuffer,           0x40/8);
    CHECK_SLOT(D, CreateContextData,        0x48/8);
    CHECK_SLOT(D, CreateConstantBufferData, 0x58/8);
    CHECK_SLOT(D, CreateVertexLayoutData,   0x70/8);
    CHECK_SLOT(D, CreateVertexProgramData,  0x78/8);
    CHECK_SLOT(D, CreatePixelProgramData,   0x80/8);
    CHECK_SLOT(D, CreateRenderStateData,    0x88/8);
    CHECK_SLOT(D, CreateVideoProgramData,   0x90/8);
    CHECK_SLOT(D, CreateRenderBufferData,   0x98/8);
    using C = Video::ContextData;
    CHECK_SLOT(C, BeginCommands,               0x08/8);
    CHECK_SLOT(C, DrawIndexed,                 0x60/8);
    CHECK_SLOT(C, EndInstancedIndexedVertices, 0x98/8);
}
