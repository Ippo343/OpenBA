// ============================================================================
// tests/unit/rb_video_abi_test.cpp — ENDGAME B.3.8 block RB, Video::ImageBuffer
//
// The exe instantiates `Video::ImageBuffer` itself (its table copy
// 0x14015c8c0: ModDialog, NoiseUI, TradeWork), so B38_RULES D1/D2 make it
// CONCRETE with a PUBLIC real constructor (0x1800f2580) whose body is ONE body
// with the ctor door (`InitFields`), a destructor sharing `ReleaseFields` with
// the dtor door, and slot 0 = the exe's scalar deleting dtor 0x1400129d0
// (~ImageBuffer; MemFree on flags & 1).
//
// PART A — construction runs the shared body under ImageBuffer's compiler
//   table (CompiledExternal() and the stamp first, External's three zero
//   stores before it), and the object ends on the subclass's table.
// PART B — slot 0 destroys (ReleaseFields under ImageBuffer's table) and frees.
// (PART C, the doors reaching the same bodies, went with the doors and the
// minted row at ENDGAME B.3.9 VS.) PART D (b39_vs_*) — B9-16: the retired
// row's shape (not abstract, protected non-virtual dtor) and the ctor's
// retail stores.
//
// MARKERS: the class's virtuals, ~ImageBuffer and the two shared bodies live
// in runtime/src/video/buffers.cpp, which does not link here.
// ============================================================================
#include "unit.h"

#include "runtime/types/video/ImageBuffer.h"
#include "runtime/types/graphics/GeneratedImageBuffer.h"   // B.3.9 GFX: FrameCopy's part moved to rb_graphics_abi_test.cpp

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

extern const void* const g_rbIbVtGroup[] __asm__("_ZTVN5Video11ImageBufferE");
extern const void* const g_rbGiVtGroup[] __asm__("_ZTVN8Graphics20GeneratedImageBufferE");

namespace {
const void* IbVt() { return &g_rbIbVtGroup[2]; }
const void* VptrOf(const void* p) { return *static_cast<const void* const*>(p); }
int g_init = 0, g_rel = 0;
const void* g_initVptr = nullptr;
const void* g_relVptr = nullptr;
}  // namespace

namespace Video {
int32_t           ImageBuffer::GetFileVersion() const { return 7; }
bool              ImageBuffer::Serialize(void*) { return true; }
Object::External* ImageBuffer::Reconstruct() { return this; }
void              ImageBuffer::Destruct() {}
Object::External* ImageBuffer::Construct() { return this; }
bool ImageBuffer::Create(ImageType, Usage, Format, uint32_t, int32_t, int32_t,
                         FilterMode, AddressMode, AddressMode, AddressMode, LevelArray*) {
    return false;
}
ImageBuffer::~ImageBuffer() { ReleaseFields(); }   // buffers.cpp's shape
void ImageBuffer::InitFields() {
    ++g_init; g_initVptr = VptrOf(this);
    _kind = 2; _filter = 1; _sourceName = nullptr;
}
void ImageBuffer::ReleaseFields() { ++g_rel; g_relVptr = VptrOf(this); }
}  // namespace Video

namespace Graphics {
int32_t     GeneratedImageBuffer::GetFileVersion() const { return 7; }
bool        GeneratedImageBuffer::Serialize(void*) { return true; }
}  // namespace Graphics

namespace {

struct RbModImage final : Video::ImageBuffer {};   // a stand-in subclass

UNIT_TEST(rb_imagebuffer_ctor_runs_shared_body_under_its_table) {
    g_init = g_rel = 0;
    alignas(8) unsigned char buf[sizeof(RbModImage)];
    std::memset(buf, 0xCD, sizeof buf);
    RbModImage* ib = ::new (static_cast<void*>(buf)) RbModImage;
    CHECK_EQ(g_init, 1);
    CHECK(g_initVptr == IbVt());
    CHECK(VptrOf(ib) != IbVt());
    uint64_t q;
    for (size_t off = 0x08; off < 0x20; off += 8) {   // External's zero stores
        std::memcpy(&q, buf + off, 8);
        CHECK_EQ(q, (uint64_t)0);
    }
    ib->~RbModImage();
    CHECK_EQ(g_rel, 1);
    CHECK(g_relVptr == IbVt());
}

UNIT_TEST(rb_imagebuffer_slot0_is_the_exe_scalar_deleting_dtor) {
    g_init = g_rel = 0;
    void* mem = Object::detail::InternalMemAlloc(sizeof(Video::ImageBuffer), 8);
    Video::ImageBuffer* ib = ::new (mem) Video::ImageBuffer();
    CHECK(VptrOf(ib) == IbVt());
    using Slot0 = Object::Base* (*)(void*, unsigned);
    Slot0 s0 = reinterpret_cast<Slot0>(const_cast<void*>(g_rbIbVtGroup[2]));
    CHECK(static_cast<void*>(s0(ib, 1u)) == mem);
    CHECK_EQ(g_rel, 1);
    CHECK(g_relVptr == IbVt());
}

UNIT_TEST(b39_vs_imagebuffer_is_concrete_with_a_protected_dtor) {
    CHECK(!std::is_abstract<Video::ImageBuffer>::value);
    CHECK(!std::is_destructible<Video::ImageBuffer>::value);      // D1.3: protected
    CHECK(!std::has_virtual_destructor<Video::ImageBuffer>::value);
    CHECK((std::is_base_of<Object::CompiledExternal, Video::ImageBuffer>::value));
}

// The retail stores of 0x1800f2580 the stand-in InitFields above does NOT make:
// External's three qwords (+0x08/+0x10/+0x18) are the compiler's base ctors, and
// the vptr is the class's own table from the first byte the body sees.
UNIT_TEST(b39_vs_imagebuffer_ctor_stores_and_table) {
    g_init = g_rel = 0;
    void* mem = Object::detail::InternalMemAlloc(sizeof(Video::ImageBuffer), 8);
    std::memset(mem, 0xCD, sizeof(Video::ImageBuffer));
    Video::ImageBuffer* ib = ::new (mem) Video::ImageBuffer();
    CHECK_EQ(g_init, 1);
    CHECK(g_initVptr == IbVt());
    CHECK(VptrOf(ib) == IbVt());
    uint64_t q;
    for (size_t off = 0x08; off < 0x20; off += 8) {
        std::memcpy(&q, static_cast<unsigned char*>(mem) + off, 8);
        CHECK_EQ(q, (uint64_t)0);
    }
    CHECK_EQ(ib->_kind, 2);
    CHECK_EQ(ib->_filter, 1);
    CHECK(ib->_sourceName == nullptr);
    // slot 0 with flags 0: destroy in place, keep the storage (Reconstruct's arm)
    CHECK(static_cast<void*>(ib->DeletingDtor(0u)) == mem);
    CHECK_EQ(g_rel, 1);
    Object::detail::InternalMemFree(mem);
}

UNIT_TEST(rb_imagebuffer_shape) {
    CHECK(!std::is_abstract<Video::ImageBuffer>::value);
    CHECK(!std::has_virtual_destructor<Video::ImageBuffer>::value);
    CHECK_EQ(sizeof(Video::ImageBuffer), (size_t)0x68);
}

}  // namespace
