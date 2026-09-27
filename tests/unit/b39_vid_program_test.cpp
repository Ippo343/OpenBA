// ============================================================================
// tests/unit/b39_vid_program_test.cpp -- ENDGAME B.3 step 9, block VID
// (B39_RULES.md D1/D2, rule B9-16): the Video resource classes made CONCRETE.
//
// For each converted class: not abstract, `!std::is_destructible` (the dtor is
// protected), `!std::has_virtual_destructor`, slot order read back through the
// Itanium PMF (adjustment 0), the real constructor's retail stores, and both
// arms of the inline VECTOR `DeletingDtor` with a recording destructor marker.
//
// No tree source links (the class bodies stand on the port registry, the
// Device singleton and the serializer), so the out-of-line members are defined
// HERE as markers -- which is also what makes GCC emit each class's vtable in
// this translation unit. The inline members under test (the constructor and
// slot 0) are the real ones from the headers.
// ============================================================================
#include "unit.h"

#include "runtime/types/video/RenderState.h"
#include "runtime/types/video/ShaderProgram.h"
#include "runtime/types/video/VertexLayout.h"
#include "runtime/types/video/VideoProgram.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <new>
#include <type_traits>

namespace {

int         g_dtorCount = 0;
const void* g_dtorOrder[8] = {};
int g_shaderBaseDtorCount = 0;   // ~ShaderProgram runs as the base step
void NoteDtor(const void* self) {
    if (g_dtorCount < 8) g_dtorOrder[g_dtorCount] = self;
    ++g_dtorCount;
}
// What the marker PlatformConstruct stores into the companion field, so the
// ctor test can see the call happened (retail: DeviceData slot N's result).
void* const kMarkerData = reinterpret_cast<void*>(uintptr_t(0x0DA7A0DA7A0ull));

template <typename PMF>
uintptr_t PmfSlot(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);          // adjustment must be 0
    return words[0] - 1;
}

// Both arms of a class's inline VECTOR deleting destructor: flags 1 destroys
// one and frees it; flags 3 reads the count at `this - 8`, destroys downward
// and frees `this - 8`.
template <class T, int kSize>
void CheckBothArms() {
    void* m = Object::detail::InternalMemAlloc(kSize, 8);
    CHECK(m != nullptr);
    T* p = ::new (m) T();
    g_dtorCount = 0;
    CHECK(static_cast<void*>(p->DeletingDtor(1)) == static_cast<void*>(p));
    CHECK_EQ(g_dtorCount, 1);
    CHECK(g_dtorOrder[0] == p);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * kSize, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    T* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * kSize) T();
    g_dtorCount = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_dtorCount, 3);
    CHECK(g_dtorOrder[0] == e[2]);
    CHECK(g_dtorOrder[1] == e[1]);
    CHECK(g_dtorOrder[2] == e[0]);
}

}  // namespace

// ---------------------------------------------------------------------------
// Marker bodies for the out-of-line members (videoprogram.cpp in the exe).
// ---------------------------------------------------------------------------
namespace Video {
const char*       VideoProgram::TypeName() const { return "VideoProgram"; }
uint32_t          VideoProgram::Type() const { return 0x7071u; }
uint32_t          VideoProgram::TypeId() { return 0x7071u; }
int32_t           VideoProgram::GetFileVersion() const { return 1; }
bool              VideoProgram::Serialize(void*) { return true; }
Object::External* VideoProgram::Reconstruct() { return this; }
void              VideoProgram::Destruct() {}
Object::External* VideoProgram::Construct() { return this; }
VideoProgram::~VideoProgram() { NoteDtor(this); }
void              VideoProgram::PlatformConstruct() { _data = kMarkerData; }
void              VideoProgram::PlatformDestruct() {}

const char*       VertexLayout::TypeName() const { return "VertexLayout"; }
uint32_t          VertexLayout::Type() const { return 0x7072u; }
uint32_t          VertexLayout::TypeId() { return 0x7072u; }
int32_t           VertexLayout::GetFileVersion() const { return 3; }
bool              VertexLayout::Serialize(void*) { return true; }
Object::External* VertexLayout::Reconstruct() { return this; }
void              VertexLayout::Destruct() {}
Object::External* VertexLayout::Construct() { return this; }
VertexLayout::~VertexLayout() { NoteDtor(this); }
void              VertexLayout::PlatformConstruct() { _data = kMarkerData; }
void              VertexLayout::PlatformDestruct() {}

const char*       RenderState::TypeName() const { return "RenderState"; }
uint32_t          RenderState::Type() const { return 0x7073u; }
uint32_t          RenderState::TypeId() { return 0x7073u; }
int32_t           RenderState::GetFileVersion() const { return 3; }
bool              RenderState::Serialize(void*) { return true; }
Object::External* RenderState::Reconstruct() { return this; }
void              RenderState::Destruct() {}
Object::External* RenderState::Construct() { return this; }
RenderState::~RenderState() { NoteDtor(this); }
void              RenderState::PlatformConstruct() { _data = static_cast<RenderStateData*>(kMarkerData); }
void              RenderState::PlatformDestruct() {}

const char*       ShaderProgram::TypeName() const { return "ShaderProgram"; }
uint32_t          ShaderProgram::Type() const { return 0x7074u; }
uint32_t          ShaderProgram::TypeId() { return 0x7074u; }
bool              ShaderProgram::Serialize(void*) { return true; }
ShaderProgram::~ShaderProgram() { ++g_shaderBaseDtorCount; }

#define B39_VID_SHADER_MARKERS(C, ID)                                                  const char*       C::TypeName() const { return #C; }                               uint32_t          C::Type() const { return ID; }                                   uint32_t          C::TypeId() { return ID; }                                       int32_t           C::GetFileVersion() const { return 5; }                          bool              C::Serialize(void*) { return true; }                             Object::External* C::Reconstruct() { return this; }                                void              C::Destruct() {}                                                 Object::External* C::Construct() { return this; }                                 C::~C() { NoteDtor(this); }                                                        void              C::PlatformConstruct() { _data = kMarkerData; }                  void              C::PlatformDestruct() {}
B39_VID_SHADER_MARKERS(VertexProgram, 0x7075u)
B39_VID_SHADER_MARKERS(PixelProgram,  0x7076u)
#undef B39_VID_SHADER_MARKERS
}  // namespace Video

// ===========================================================================
// Video::VideoProgram (row 148, RT_DELDTOR 0x18004efb0)
// ===========================================================================
UNIT_TEST(b39_vid_videoprogram_shape) {
    using V = Video::VideoProgram;
    CHECK(!std::is_abstract<V>::value);
    CHECK(!std::is_destructible<V>::value);          // protected ~VideoProgram
    CHECK(!std::has_virtual_destructor<V>::value);
    CHECK((std::is_base_of<Object::External, V>::value));
    CHECK(!(std::is_base_of<Object::CompiledExternal, V>::value));   // the famous trap
    CHECK_EQ((int)sizeof(V), 0x48);
}

UNIT_TEST(b39_vid_videoprogram_slot_order_by_pmf) {
    using V = Video::VideoProgram;
    CHECK_EQ(PmfSlot(&V::DeletingDtor),   (uintptr_t)0x00);   // 0x18004efb0
    CHECK_EQ(PmfSlot(&V::TypeName),       (uintptr_t)0x08);   // 0x18004ec50
    CHECK_EQ(PmfSlot(&V::Type),           (uintptr_t)0x10);   // 0x18004ec30
    CHECK_EQ(PmfSlot(&V::GetFileVersion), (uintptr_t)0x18);   // 0x1800f46a0
    CHECK_EQ(PmfSlot(&V::Compile),        (uintptr_t)0x20);   // External's 0x1800066e0
    CHECK_EQ(PmfSlot(&V::Serialize),      (uintptr_t)0x28);   // 0x1800f46b0
    CHECK_EQ(PmfSlot(&V::Reconstruct),    (uintptr_t)0x30);   // 0x18004edd0
    CHECK_EQ(PmfSlot(&V::Destruct),       (uintptr_t)0x38);   // 0x18004ee00
    CHECK_EQ(PmfSlot(&V::Construct),      (uintptr_t)0x40);   // 0x18004edf0
}

// ??0VideoProgram 0x1800f45b0: External's three zero qwords (+0x08..+0x1f), the
// four ExternalPtrs (+0x20..+0x3f), then PlatformConstruct's +0x40.
UNIT_TEST(b39_vid_videoprogram_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0x48];
    std::memset(buf, 0xCD, sizeof buf);
    Video::VideoProgram* v = ::new (buf) Video::VideoProgram();
    CHECK(static_cast<void*>(v) == static_cast<void*>(buf));
    uint64_t q = 0;
    for (int off = 0x08; off < 0x40; off += 8) {
        std::memcpy(&q, buf + off, sizeof q);
        CHECK_EQ(q, (uint64_t)0);
    }
    CHECK(v->_data == kMarkerData);
    g_dtorCount = 0;
    CHECK(static_cast<void*>(v->DeletingDtor(0)) == static_cast<void*>(v));
    CHECK_EQ(g_dtorCount, 1);
}

UNIT_TEST(b39_vid_videoprogram_deletingdtor_both_arms) {
    CheckBothArms<Video::VideoProgram, 0x48>();
}

// ===========================================================================
// Video::VertexLayout (row 144, RT_DELDTOR 0x18004e000)
// ===========================================================================
UNIT_TEST(b39_vid_vertexlayout_shape) {
    using L = Video::VertexLayout;
    CHECK(!std::is_abstract<L>::value);
    CHECK(!std::is_destructible<L>::value);          // protected ~VertexLayout
    CHECK(!std::has_virtual_destructor<L>::value);
    CHECK((std::is_base_of<Object::CompiledExternal, L>::value));
    CHECK_EQ((int)sizeof(L), 0x48);
}

UNIT_TEST(b39_vid_vertexlayout_slot_order_by_pmf) {
    using L = Video::VertexLayout;
    CHECK_EQ(PmfSlot(&L::DeletingDtor),   (uintptr_t)0x00);   // 0x18004e000
    CHECK_EQ(PmfSlot(&L::TypeName),       (uintptr_t)0x08);   // 0x18004dcd0
    CHECK_EQ(PmfSlot(&L::Type),           (uintptr_t)0x10);   // 0x18004dcb0
    CHECK_EQ(PmfSlot(&L::GetFileVersion), (uintptr_t)0x18);   // 0x1800f5e60
    CHECK_EQ(PmfSlot(&L::Compile),        (uintptr_t)0x20);   // External's 0x1800066e0
    CHECK_EQ(PmfSlot(&L::Serialize),      (uintptr_t)0x28);   // 0x1800f5e70
    CHECK_EQ(PmfSlot(&L::Reconstruct),    (uintptr_t)0x30);   // 0x18004de50
    CHECK_EQ(PmfSlot(&L::Destruct),       (uintptr_t)0x38);   // 0x18004de80
    CHECK_EQ(PmfSlot(&L::Construct),      (uintptr_t)0x40);   // 0x18004de70
}

// ??0VertexLayout 0x1800f5d50: External's three zero qwords, the two Array
// headers (+0x20..+0x2f, +0x38..+0x47) and PlatformConstruct's +0x30.
UNIT_TEST(b39_vid_vertexlayout_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0x48];
    std::memset(buf, 0xCD, sizeof buf);
    Video::VertexLayout* l = ::new (buf) Video::VertexLayout();
    uint64_t q = 0;
    for (int off : {0x08, 0x10, 0x18, 0x20, 0x28, 0x38, 0x40}) {
        std::memcpy(&q, buf + off, sizeof q);
        CHECK_EQ(q, (uint64_t)0);
    }
    CHECK(l->_data == kMarkerData);
    g_dtorCount = 0;
    CHECK(static_cast<void*>(l->DeletingDtor(0)) == static_cast<void*>(l));
    CHECK_EQ(g_dtorCount, 1);
}

UNIT_TEST(b39_vid_vertexlayout_deletingdtor_both_arms) {
    CheckBothArms<Video::VertexLayout, 0x48>();
}

// ===========================================================================
// Video::RenderState (row 112, RT_DELDTOR 0x18004b9d0)
// ===========================================================================
UNIT_TEST(b39_vid_renderstate_shape) {
    using R = Video::RenderState;
    CHECK(!std::is_abstract<R>::value);
    CHECK(!std::is_destructible<R>::value);          // protected ~RenderState
    CHECK(!std::has_virtual_destructor<R>::value);
    CHECK((std::is_base_of<Object::External, R>::value));
    CHECK(!(std::is_base_of<Object::CompiledExternal, R>::value));
    CHECK_EQ((int)sizeof(R), 0x80);
}

UNIT_TEST(b39_vid_renderstate_slot_order_by_pmf) {
    using R = Video::RenderState;
    CHECK_EQ(PmfSlot(&R::DeletingDtor),   (uintptr_t)0x00);   // 0x18004b9d0
    CHECK_EQ(PmfSlot(&R::TypeName),       (uintptr_t)0x08);   // 0x18004b570
    CHECK_EQ(PmfSlot(&R::Type),           (uintptr_t)0x10);   // 0x18004b550
    CHECK_EQ(PmfSlot(&R::GetFileVersion), (uintptr_t)0x18);   // 0x1800f4c80
    CHECK_EQ(PmfSlot(&R::Compile),        (uintptr_t)0x20);   // External's 0x1800066e0
    CHECK_EQ(PmfSlot(&R::Serialize),      (uintptr_t)0x28);   // 0x1800f4c90
    CHECK_EQ(PmfSlot(&R::Reconstruct),    (uintptr_t)0x30);   // 0x18004b6f0
    CHECK_EQ(PmfSlot(&R::Destruct),       (uintptr_t)0x38);   // 0x18004b720
    CHECK_EQ(PmfSlot(&R::Construct),      (uintptr_t)0x40);   // 0x18004b710
}

// ??0RenderState 0x1800f4b60: External's zeros, AlphaBlend {0,1,0,0,0xf},
// Depth {1,1,4,0,0}, Stencil all zero, cull 1, fill 0, flag 0, +0x71..+0x77
// untouched, PlatformConstruct's +0x78.
UNIT_TEST(b39_vid_renderstate_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0x80];
    std::memset(buf, 0xCD, sizeof buf);
    Video::RenderState* r = ::new (buf) Video::RenderState();
    uint64_t q = 0;
    for (int off = 0x08; off < 0x20; off += 8) {
        std::memcpy(&q, buf + off, sizeof q);
        CHECK_EQ(q, (uint64_t)0);
    }
    CHECK_EQ((int)r->_alphaBlend.enabled, 0);
    CHECK_EQ(r->_alphaBlend.srcBlend, 1u);
    CHECK_EQ(r->_alphaBlend.dstBlend, 0u);
    CHECK_EQ(r->_alphaBlend.blendOp, 0u);
    CHECK_EQ(r->_alphaBlend.writeMask, 0xfu);
    CHECK_EQ((int)r->_depth.test, 1);
    CHECK_EQ((int)r->_depth.write, 1);
    CHECK_EQ(r->_depth.func, 4u);
    for (int off = 0x44; off < 0x68; ++off) CHECK_EQ((int)buf[off], 0);
    CHECK_EQ(r->_cullMode, 1u);
    CHECK_EQ(r->_fillMode, 0u);
    CHECK_EQ((int)r->_flag70, 0);
    for (int off = 0x71; off < 0x78; ++off) CHECK_EQ((int)buf[off], 0xCD);
    CHECK(static_cast<void*>(r->_data) == kMarkerData);
    g_dtorCount = 0;
    CHECK(static_cast<void*>(r->DeletingDtor(0)) == static_cast<void*>(r));
    CHECK_EQ(g_dtorCount, 1);
}

UNIT_TEST(b39_vid_renderstate_deletingdtor_both_arms) {
    CheckBothArms<Video::RenderState, 0x80>();
}

// ===========================================================================
// Video::ShaderProgram (row 118, RT_DELDTOR 0x18004e360) -- the ABSTRACT base
// with a protected real ctor -- and its two concrete leaves, Video::VertexProgram
// (row 146, 0x18004e810) and Video::PixelProgram (row 102, 0x18004eb90).
// ===========================================================================
UNIT_TEST(b39_vid_shaderprogram_family_shape) {
    using S = Video::ShaderProgram;
    CHECK(std::is_abstract<S>::value);               // slots 3/6/7 are _purecall in retail too
    CHECK(!std::is_destructible<S>::value);
    CHECK(!std::has_virtual_destructor<S>::value);
    CHECK((std::is_base_of<Object::CompiledExternal, S>::value));
    CHECK_EQ((int)sizeof(S), 0x50);
    using V = Video::VertexProgram;
    using P = Video::PixelProgram;
    CHECK(!std::is_abstract<V>::value);
    CHECK(!std::is_abstract<P>::value);
    CHECK(!std::is_destructible<V>::value);
    CHECK(!std::is_destructible<P>::value);
    CHECK(!std::has_virtual_destructor<V>::value);
    CHECK(!std::has_virtual_destructor<P>::value);
    CHECK((std::is_base_of<S, V>::value));
    CHECK((std::is_base_of<S, P>::value));
    CHECK_EQ((int)sizeof(V), 0x58);
    CHECK_EQ((int)sizeof(P), 0x58);
}

UNIT_TEST(b39_vid_shaderprogram_family_slot_order_by_pmf) {
    using S = Video::ShaderProgram;
    CHECK_EQ(PmfSlot(&S::DeletingDtor),   (uintptr_t)0x00);   // 0x18004e360
    CHECK_EQ(PmfSlot(&S::TypeName),       (uintptr_t)0x08);   // 0x18004e0c0
    CHECK_EQ(PmfSlot(&S::Type),           (uintptr_t)0x10);   // 0x18004e0a0
    CHECK_EQ(PmfSlot(&S::Compile),        (uintptr_t)0x20);   // External's 0x1800066e0
    CHECK_EQ(PmfSlot(&S::Serialize),      (uintptr_t)0x28);   // 0x1800f5110
    using V = Video::VertexProgram;
    CHECK_EQ(PmfSlot(&V::DeletingDtor),   (uintptr_t)0x00);   // 0x18004e810
    CHECK_EQ(PmfSlot(&V::TypeName),       (uintptr_t)0x08);   // 0x18004e560
    CHECK_EQ(PmfSlot(&V::Type),           (uintptr_t)0x10);   // 0x18004e540
    CHECK_EQ(PmfSlot(&V::GetFileVersion), (uintptr_t)0x18);   // 0x1800f6230
    CHECK_EQ(PmfSlot(&V::Compile),        (uintptr_t)0x20);
    CHECK_EQ(PmfSlot(&V::Serialize),      (uintptr_t)0x28);   // 0x1800f6240
    CHECK_EQ(PmfSlot(&V::Reconstruct),    (uintptr_t)0x30);   // 0x18004e6e0
    CHECK_EQ(PmfSlot(&V::Destruct),       (uintptr_t)0x38);   // 0x18004e710
    CHECK_EQ(PmfSlot(&V::Construct),      (uintptr_t)0x40);   // 0x18004e700
    using P = Video::PixelProgram;
    CHECK_EQ(PmfSlot(&P::DeletingDtor),   (uintptr_t)0x00);   // 0x18004eb90
    CHECK_EQ(PmfSlot(&P::TypeName),       (uintptr_t)0x08);   // 0x18004e8e0
    CHECK_EQ(PmfSlot(&P::Type),           (uintptr_t)0x10);   // 0x18004e8c0
    CHECK_EQ(PmfSlot(&P::GetFileVersion), (uintptr_t)0x18);   // 0x1800f3a80
    CHECK_EQ(PmfSlot(&P::Compile),        (uintptr_t)0x20);
    CHECK_EQ(PmfSlot(&P::Serialize),      (uintptr_t)0x28);   // 0x1800f3a90
    CHECK_EQ(PmfSlot(&P::Reconstruct),    (uintptr_t)0x30);   // 0x18004ea60
    CHECK_EQ(PmfSlot(&P::Destruct),       (uintptr_t)0x38);   // 0x18004ea90
    CHECK_EQ(PmfSlot(&P::Construct),      (uintptr_t)0x40);   // 0x18004ea80
}

// ??0VertexProgram 0x1800f6100 / ??0PixelProgram 0x1800f3950: ??0ShaderProgram
// inlined (External's zeros + the three Array headers, +0x08..+0x4f), then
// PlatformConstruct's +0x50. The leaf's dtor runs ~ShaderProgram as its base step.
template <class T>
void CheckShaderLeafCtor() {
    alignas(8) uint8_t buf[0x58];
    std::memset(buf, 0xCD, sizeof buf);
    T* v = ::new (buf) T();
    uint64_t q = 0;
    for (int off = 0x08; off < 0x50; off += 8) {
        std::memcpy(&q, buf + off, sizeof q);
        CHECK_EQ(q, (uint64_t)0);
    }
    CHECK(v->_data == kMarkerData);
    g_dtorCount = 0;
    g_shaderBaseDtorCount = 0;
    CHECK(static_cast<void*>(v->DeletingDtor(0)) == static_cast<void*>(v));
    CHECK_EQ(g_dtorCount, 1);
    CHECK_EQ(g_shaderBaseDtorCount, 1);
}
UNIT_TEST(b39_vid_vertexprogram_ctor_makes_retails_stores) { CheckShaderLeafCtor<Video::VertexProgram>(); }
UNIT_TEST(b39_vid_pixelprogram_ctor_makes_retails_stores)  { CheckShaderLeafCtor<Video::PixelProgram>(); }

UNIT_TEST(b39_vid_vertexprogram_deletingdtor_both_arms) {
    CheckBothArms<Video::VertexProgram, 0x58>();
}
UNIT_TEST(b39_vid_pixelprogram_deletingdtor_both_arms) {
    CheckBothArms<Video::PixelProgram, 0x58>();
}
