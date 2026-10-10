// ============================================================================
// tests/unit/b39_vid_data_test.cpp -- the twelve Video::*Data companion bases.
//
// ENDGAME B.3 step 9, block VID (B39_RULES.md D1, rule B9-16) made them CONCRETE
// (a real constructor storing the owner at +0x08, `_purecall` bodies in every
// slot but 0). ENDGAME B5 (the dx11 seam, 2026-10-05, B5_RULES.md D1-D3) gave
// each `virtual ~T()` and made dx11's twelve `VideoDX11::*Data` real subclasses
// with compiler vtables; the old `DeletingDtor(unsigned)` slot-0 member and its
// vector arm are gone. The dx11 subclasses cannot link here (D3D11), so the BASE
// shape is what is tested, plus the shape the DX9 placeholder
// (src/render/dx11/dx9_placeholder.cpp) puts on it. Slots 1..N are pinned by RETAIL
// number through slot_check.h (which shifts for the two destructor slots).
//
// The out-of-line destructors live in src/engine/video/companion_data.cpp
// (which cannot link into the host test), so they are defined HERE as markers.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "src/engine/video/CompanionData.h"

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

// B5 W0: the owner is typed per class (`T::Owner`); the test's fake owner is the
// same bit pattern through that type.
constexpr uintptr_t kOwnerBits = 0x0E0E0E0E0ull;

// B5 (the dx11 seam, B5_RULES D1): a base whose dx11 subclass is a real C++
// subclass declares `virtual ~T()` -- the compiler's D1/D0 pair is retail slot 0
// and its class-scope operator delete (companion_operators.cpp, host arm
// std::free) is slot 0's MemFree. Still concrete (the `_purecall` bodies).
template <class T>
void CheckDataB5() {
    typename T::Owner* const kOwner = reinterpret_cast<typename T::Owner*>(kOwnerBits);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK(std::is_destructible<T>::value);
    CHECK(!std::is_default_constructible<T>::value); // the owner is required
    CHECK_EQ(sizeof(T), (size_t)0x10);
    CHECK_DTOR_SLOT(T);

    // The ctor's one data store: owner at +0x08.
    alignas(8) uint8_t buf[0x10];
    std::memset(buf, 0xCD, sizeof buf);
    T* d = ::new (buf) T(kOwner);
    CHECK(static_cast<void*>(d) == static_cast<void*>(buf));
    CHECK(d->_owner == kOwner);
    void* ownerAt8 = nullptr;
    std::memcpy(&ownerAt8, buf + 0x08, sizeof ownerAt8);
    CHECK(ownerAt8 == static_cast<void*>(kOwner));
    g_dtorCount = 0;
    d->~T();
    CHECK_EQ(g_dtorCount, 1);

    // retail's slot 0 with flags 1: `delete` runs the destructor once and frees
    // through the class's own operator delete.
    T* p = new T(kOwner);
    CHECK(p->_owner == kOwner);
    g_dtorCount = 0;
    delete p;
    CHECK_EQ(g_dtorCount, 1);
    CHECK(g_dtorOrder[0] == static_cast<const void*>(p));
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

UNIT_TEST(b39_vid_devicedata_concrete)         { CheckDataB5<Video::DeviceData>(); }
UNIT_TEST(b39_vid_contextdata_concrete)        { CheckDataB5<Video::ContextData>(); }
UNIT_TEST(b39_vid_imagebufferdata_concrete)    { CheckDataB5<Video::ImageBufferData>(); }
UNIT_TEST(b39_vid_indexbufferdata_concrete)    { CheckDataB5<Video::IndexBufferData>(); }
UNIT_TEST(b39_vid_vertexbufferdata_concrete)   { CheckDataB5<Video::VertexBufferData>(); }
UNIT_TEST(b39_vid_constantbufferdata_concrete) { CheckDataB5<Video::ConstantBufferData>(); }
UNIT_TEST(b39_vid_renderbufferdata_concrete)   { CheckDataB5<Video::RenderBufferData>(); }
UNIT_TEST(b39_vid_vertexlayoutdata_concrete)   { CheckDataB5<Video::VertexLayoutData>(); }
UNIT_TEST(b39_vid_vertexprogramdata_concrete)  { CheckDataB5<Video::VertexProgramData>(); }
UNIT_TEST(b39_vid_pixelprogramdata_concrete)   { CheckDataB5<Video::PixelProgramData>(); }
UNIT_TEST(b39_vid_renderstatedata_concrete)    { CheckDataB5<Video::RenderStateData>(); }
UNIT_TEST(b39_vid_videoprogramdata_concrete)   { CheckDataB5<Video::VideoProgramData>(); }

// B5 PILOT: the two flipped classes' slots in RETAIL numbers (slot_check.h
// shifts them past the destructor pair).
UNIT_TEST(b5_vid_flipped_companion_slots_by_pmf) {
    using R = Video::RenderBufferData;
    CHECK_SLOT(R, Slot1,   1);
    CHECK_SLOT(R, Bind,    2);
    CHECK_SLOT(R, Resolve, 3);
    using V = Video::VideoProgramData;
    CHECK_SLOT(V, Slot1, 1);
    CHECK_SLOT(V, Bind,  2);
    // B5 CTX: all twenty ContextData slots, retail numbers.
    using C = Video::ContextData;
    CHECK_SLOT(C, BeginCommands,                  1);
    CHECK_SLOT(C, EndCommands,                    2);
    CHECK_SLOT(C, Finish,                         3);
    CHECK_SLOT(C, Clear,                          4);
    CHECK_SLOT(C, BindRenderBuffer,               5);
    CHECK_SLOT(C, BindViewport,                   6);
    CHECK_SLOT(C, BindScissor,                    7);
    CHECK_SLOT(C, BindTransforms,                 8);
    CHECK_SLOT(C, BindTextureNone,                9);
    CHECK_SLOT(C, BindIndexNone,                 10);
    CHECK_SLOT(C, BindVertexNone,                11);
    CHECK_SLOT(C, DrawIndexed,                   12);
    CHECK_SLOT(C, GetMaxDynamicInstances,        13);
    CHECK_SLOT(C, BeginVertices,                 14);
    CHECK_SLOT(C, EndVertices,                   15);
    CHECK_SLOT(C, BeginIndexedVertices,          16);
    CHECK_SLOT(C, EndIndexedVertices,            17);
    CHECK_SLOT(C, BeginInstancedIndexedVertices, 18);
    CHECK_SLOT(C, EndInstancedIndexedVertices,   19);
}

// B5 DEV-A (B5_RULES D5, C3): the DX9 placeholder's shape -- a `final` subclass
// of `Video::DeviceData` overriding slots 1-3 and INHERITING the other sixteen
// -- built through the base's operator new and destroyed through a BASE pointer,
// as Video::Device::EnumerateVideoModes does (create, EnumerateInterfaces,
// `delete`). Stand-in: the real one calls dx11's DXGI walk, which cannot link.
namespace {
int g_placeholderDtors = 0;
class PlaceholderShape final : public Video::DeviceData {
public:
    explicit PlaceholderShape(Video::Device* owner) : Video::DeviceData(owner) {}
    ~PlaceholderShape() override { ++g_placeholderDtors; }
    bool EnumerateInterfaces() override { return true; }
    bool CreateDevice() override { return false; }
    void Shutdown() override {}
};
}  // namespace

UNIT_TEST(b5_vid_dx9_placeholder_shape) {
    CHECK_EQ(sizeof(PlaceholderShape), (size_t)0x10);
    CHECK(std::has_virtual_destructor<PlaceholderShape>::value);
    // Slots 1-3 are the subclass's; slot 8 and the factories stay the base's
    // `_purecall` bodies (inherited, so the PMF names the base's slot).
    CHECK_SLOT(PlaceholderShape, EnumerateInterfaces, 1);
    CHECK_SLOT(PlaceholderShape, CreateDevice,        2);
    CHECK_SLOT(PlaceholderShape, Shutdown,            3);

    Video::Device* const owner = reinterpret_cast<Video::Device*>(kOwnerBits);
    Video::DeviceData* data = new PlaceholderShape(owner);   // the slot's factory
    CHECK(data->_owner == owner);
    CHECK(data->EnumerateInterfaces());
    CHECK(!data->CreateDevice());
    g_placeholderDtors = 0;
    g_dtorCount = 0;
    delete data;                                              // slot 0, flags 1
    CHECK_EQ(g_placeholderDtors, 1);   // the derived destructor ran ...
    CHECK_EQ(g_dtorCount, 1);          // ... then the base's
}

// B5 BUF: the three buffer companions, retail numbers. Slots 3 / 4 of Index and
// Vertex are the two `Video::{Index,Vertex}Buffer::Lock/Unlock` trampolines'
// retail byte offsets +0x18 / +0x20; ConstantBufferData's Bind is +0x10.
UNIT_TEST(b5_buf_flipped_companion_slots_by_pmf) {
    using I = Video::IndexBufferData;
    CHECK_SLOT(I, Create,    1);
    CHECK_SLOT(I, SetData,   2);
    CHECK_SLOT(I, Lock,      0x18/8);
    CHECK_SLOT(I, Unlock,    0x20/8);
    CHECK_SLOT(I, Bind,      5);
    CHECK_SLOT(I, Serialize, 6);
    using V = Video::VertexBufferData;
    CHECK_SLOT(V, Create,    1);
    CHECK_SLOT(V, SetData,   2);
    CHECK_SLOT(V, Lock,      0x18/8);
    CHECK_SLOT(V, Unlock,    0x20/8);
    CHECK_SLOT(V, Bind,      5);
    CHECK_SLOT(V, Serialize, 6);
    using C = Video::ConstantBufferData;
    CHECK_SLOT(C, Create,           1);
    CHECK_SLOT(C, Bind,             0x10/8);
    CHECK_SLOT(C, SlotIndexPlusOne, 3);
}

// B5 RES: the block's flipped classes, in RETAIL numbers. `Video::VertexLayout::
// Serialize` reaches PlatformCreate as retail slot 2 (+0x10).
UNIT_TEST(b5_res_flipped_companion_slots_by_pmf) {
    using L = Video::VertexLayoutData;
    CHECK_SLOT(L, Bind,           1);
    CHECK_SLOT(L, PlatformCreate, 2);
    using VP = Video::VertexProgramData;
    CHECK_SLOT(VP, PlatformCreate, 1);
    CHECK_SLOT(VP, Bind,           2);
    using PP = Video::PixelProgramData;
    CHECK_SLOT(PP, PlatformCreate, 1);
    CHECK_SLOT(PP, Bind,           2);
    // Video::RenderState::PlatformCreate 0x1800f4ea0 -> slot 1, ::Bind 0x1800f4ec0 -> slot 2.
    using RS = Video::RenderStateData;
    CHECK_SLOT(RS, Create, 1);
    CHECK_SLOT(RS, Bind,   2);
    // `?BindTextures@Context@Video@@` 0x1800ef36d calls slot 4 as `[RAX+0x20]`;
    // `?Create@ImageBuffer@Video@@` reaches slot 3 (+0x18).
    using IB = Video::ImageBufferData;
    CHECK_SLOT(IB, AttachColorBuffer, 1);
    CHECK_SLOT(IB, AttachDepthBuffer, 2);
    CHECK_SLOT(IB, Create,            3);
    CHECK_SLOT(IB, Bind,              4);
    CHECK_SLOT(IB, Update,            5);
    CHECK_SLOT(IB, Read,              6);
}

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
