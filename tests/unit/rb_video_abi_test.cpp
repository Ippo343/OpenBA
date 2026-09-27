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
// PART C — the doors reach the same bodies with a stand-in minted table.
//
// MARKERS: the class's virtuals, ~ImageBuffer and the two shared bodies live
// in runtime/src/video/buffers.cpp, which does not link here.
// ============================================================================
#include "unit.h"

#include "runtime/types/video/ImageBuffer.h"
#include "runtime/types/graphics/FrameCopyImageBuffer.h"
#include "runtime/types/graphics/GeneratedImageBuffer.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

extern const void* const g_rbIbVtGroup[] __asm__("_ZTVN5Video11ImageBufferE");
extern const void* const g_rbGiVtGroup[] __asm__("_ZTVN8Graphics20GeneratedImageBufferE");
extern const void* const g_rbFcVtGroup[] __asm__("_ZTVN8Graphics20FrameCopyImageBufferE");
extern const void* const g_rbRiVtGroup[] __asm__("_ZTVN8Graphics21ReflectionImageBufferE");

namespace {
const void* IbVt() { return &g_rbIbVtGroup[2]; }
const void* VptrOf(const void* p) { return *static_cast<const void* const*>(p); }
int g_init = 0, g_rel = 0;
const void* g_initVptr = nullptr;
const void* g_relVptr = nullptr;
const void* const kFakeMinted[4] = {};
}  // namespace

namespace Video {
const char*       ImageBuffer::TypeName() const { return "ImageBuffer"; }
uint32_t          ImageBuffer::Type() const { return 0; }
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

namespace {
int g_fcInit = 0, g_fcRel = 0;
const void* g_fcInitVptr = nullptr;
const void* g_fcRelVptr = nullptr;
int32_t g_genSeen = -1;
}  // namespace

namespace Graphics {
const char* GeneratedImageBuffer::TypeName() const { return "GeneratedImageBuffer"; }
uint32_t    GeneratedImageBuffer::Type() const { return 0; }
int32_t     GeneratedImageBuffer::GetFileVersion() const { return 7; }
bool        GeneratedImageBuffer::Serialize(void*) { return true; }

const char*       FrameCopyImageBuffer::TypeName() const { return "FrameCopyImageBuffer"; }
uint32_t          FrameCopyImageBuffer::Type() const { return 0; }
int32_t           FrameCopyImageBuffer::GetFileVersion() const { return 0x1007; }
bool              FrameCopyImageBuffer::Serialize(void*) { return true; }
Object::External* FrameCopyImageBuffer::Reconstruct() { return this; }
void              FrameCopyImageBuffer::Destruct() {}
Object::External* FrameCopyImageBuffer::Construct() { return this; }
bool FrameCopyImageBuffer::Create(ImageType, Usage, Format, uint32_t, int32_t, int32_t,
                                  FilterMode, AddressMode, AddressMode, AddressMode, LevelArray*) {
    return false;
}
void FrameCopyImageBuffer::Generate(void*, int32_t) const {}
void FrameCopyImageBuffer::Render(void*, const void*, const void*, int32_t) const {}
void FrameCopyImageBuffer::InitFields() {
    ++g_fcInit; g_fcInitVptr = VptrOf(this); g_genSeen = _generation;
    _backBufferImage = nullptr; _sourceTarget = nullptr; _destTarget = nullptr;
}
void FrameCopyImageBuffer::ReleaseFields() { ++g_fcRel; g_fcRelVptr = VptrOf(this); }
}  // namespace Graphics

// ENDGAME B.3.9 block GFX -- ReflectionImageBuffer's out-of-line members are
// MARKERS (imagebuffer_derived.cpp does not link here); ~ReflectionImageBuffer
// records each call, so the real inline DeletingDtor can be seen running it.
namespace {
int         g_riRel = 0;
const void* g_riRelVptr = nullptr;
const void* g_riRelOrder[4] = {};
}  // namespace
namespace Graphics {
const char*       ReflectionImageBuffer::TypeName() const { return "ReflectionImageBuffer"; }
uint32_t          ReflectionImageBuffer::Type() const { return 0; }
int32_t           ReflectionImageBuffer::GetFileVersion() const { return 0x1007; }
bool              ReflectionImageBuffer::Serialize(void*) { return true; }
Object::External* ReflectionImageBuffer::Reconstruct() { return this; }
void              ReflectionImageBuffer::Destruct() {}
Object::External* ReflectionImageBuffer::Construct() { return this; }
void ReflectionImageBuffer::Generate(void*, int32_t) const {}
void ReflectionImageBuffer::Render(void*, const void*, const void*, int32_t) const {}
ReflectionImageBuffer::~ReflectionImageBuffer() {
    if (g_riRel < 4) g_riRelOrder[g_riRel] = this;
    ++g_riRel; g_riRelVptr = VptrOf(this);
}
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

UNIT_TEST(rb_imagebuffer_doors_reach_the_same_bodies) {
    alignas(8) unsigned char raw[sizeof(Video::ImageBuffer)] = {};
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    g_init = g_rel = 0;
    Video::ImageBuffer::DoorBody::Init(reinterpret_cast<Video::ImageBuffer*>(raw));
    Video::ImageBuffer::DoorBody::Release(reinterpret_cast<Video::ImageBuffer*>(raw));
    CHECK_EQ(g_init, 1);
    CHECK_EQ(g_rel, 1);
    CHECK(g_initVptr == static_cast<const void*>(kFakeMinted));
    CHECK(g_relVptr == static_cast<const void*>(kFakeMinted));
}

UNIT_TEST(rb_imagebuffer_shape) {
    CHECK(!std::is_abstract<Video::ImageBuffer>::value);
    CHECK(!std::has_virtual_destructor<Video::ImageBuffer>::value);
    CHECK_EQ(sizeof(Video::ImageBuffer), (size_t)0x68);
}

// ===========================================================================
// PART D — FrameCopyImageBuffer (exe table copy 0x14015e370): the chain is
// ImageBuffer() -> GeneratedImageBuffer() (`_generation = 0`, REAL inline
// body of 0x1800a3e70) -> FrameCopy's shared body under FrameCopy's table;
// slot 0 is the exe scalar deleting dtor 0x140019d50, and teardown runs
// FrameCopy's body under its own table, then ImageBuffer's under its own.
// ===========================================================================
UNIT_TEST(rb_framecopyimagebuffer_chain_and_concrete_slot0) {
    g_init = g_rel = g_fcInit = g_fcRel = 0;
    g_genSeen = -1;
    void* mem = Object::detail::InternalMemAlloc(sizeof(Graphics::FrameCopyImageBuffer), 8);
    std::memset(mem, 0xCD, sizeof(Graphics::FrameCopyImageBuffer));
    Graphics::FrameCopyImageBuffer* f = ::new (mem) Graphics::FrameCopyImageBuffer();
    CHECK_EQ(g_init, 1);
    CHECK(g_initVptr == IbVt());                       // ImageBuffer's body first
    CHECK_EQ(g_fcInit, 1);
    CHECK(g_fcInitVptr == &g_rbFcVtGroup[2]);          // FrameCopy's body last
    CHECK_EQ(g_genSeen, 0);                            // GeneratedImageBuffer() ran between
    CHECK(VptrOf(f) == &g_rbFcVtGroup[2]);
    using Slot0 = Object::Base* (*)(void*, unsigned);
    Slot0 s0 = reinterpret_cast<Slot0>(const_cast<void*>(g_rbFcVtGroup[2]));
    CHECK(static_cast<void*>(s0(f, 1u)) == mem);
    CHECK_EQ(g_fcRel, 1);
    CHECK(g_fcRelVptr == &g_rbFcVtGroup[2]);
    CHECK_EQ(g_rel, 1);
    CHECK(g_relVptr == IbVt());
    CHECK(!std::is_abstract<Graphics::FrameCopyImageBuffer>::value);
    CHECK(std::is_abstract<Graphics::GeneratedImageBuffer>::value);

    // The doors reach the same bodies under a minted table.
    alignas(8) unsigned char raw[sizeof(Graphics::FrameCopyImageBuffer)] = {};
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    g_fcInit = g_fcRel = 0;
    Graphics::GeneratedImageBuffer::DoorBody::Init(reinterpret_cast<Graphics::GeneratedImageBuffer*>(raw));
    Graphics::FrameCopyImageBuffer::DoorBody::Init(reinterpret_cast<Graphics::FrameCopyImageBuffer*>(raw));
    Graphics::FrameCopyImageBuffer::DoorBody::Release(reinterpret_cast<Graphics::FrameCopyImageBuffer*>(raw));
    CHECK_EQ(g_fcInit, 1);
    CHECK_EQ(g_fcRel, 1);
    CHECK(g_fcInitVptr == static_cast<const void*>(kFakeMinted));
    CHECK_EQ(g_genSeen, 0);
}

// ===========================================================================
// PART E -- ENDGAME B.3.9 block GFX: ReflectionImageBuffer CONCRETE (D1).
// The ctor is ImageBuffer() -> GeneratedImageBuffer() -> retail 0x1800a7130's
// stores (+0x70 / +0x78 null, plane (0, 0, 1, 0)); slot 0 is the class's own
// VECTOR deleting dtor (0x180022600, `MOV EDX,0x90`), both arms.
// ===========================================================================
UNIT_TEST(b39_gfx_reflectionimagebuffer_ctor_stores_and_both_slot0_arms) {
    using R = Graphics::ReflectionImageBuffer;
    CHECK(!std::is_abstract<R>::value);
    CHECK(!std::is_destructible<R>::value);            // ~R protected
    CHECK(!std::has_virtual_destructor<R>::value);
    CHECK_EQ(sizeof(R), (size_t)0x90);

    g_init = g_rel = g_riRel = 0;
    void* mem = Object::detail::InternalMemAlloc(sizeof(R), 8);
    std::memset(mem, 0xCD, sizeof(R));
    R* r = ::new (mem) R();
    const uint8_t* b = static_cast<const uint8_t*>(mem);
    auto D = [&](size_t off) { uint32_t v; std::memcpy(&v, b + off, 4); return v; };
    auto Q = [&](size_t off) { uint64_t v; std::memcpy(&v, b + off, 8); return v; };
    CHECK_EQ(g_init, 1);
    CHECK(g_initVptr == IbVt());                       // ImageBuffer's body first
    CHECK(VptrOf(r) == &g_rbRiVtGroup[2]);             // the compiler's table
    CHECK_EQ(D(0x68), 0u);                             // GeneratedImageBuffer()
    CHECK_EQ(D(0x6c), 0xCDCDCDCDu);                    // never written
    CHECK_EQ(Q(0x70), (uint64_t)0);
    CHECK_EQ(Q(0x78), (uint64_t)0);
    CHECK_EQ(Q(0x80), (uint64_t)0);
    CHECK_EQ(Q(0x88), (uint64_t)0x3f800000ull);        // retail's one imm64 store

    // scalar arm, flags 1: ~R under R's table, then ~ImageBuffer, then free.
    CHECK(static_cast<void*>(r->DeletingDtor(1u)) == mem);
    CHECK_EQ(g_riRel, 1);
    CHECK(g_riRelVptr == &g_rbRiVtGroup[2]);
    CHECK_EQ(g_rel, 1);
    CHECK(g_relVptr == IbVt());

    // vector arm, flags 3: count at block-8, destroyed downward, block freed.
    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * 0x90, 8));
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    R* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (block + 8 + i * 0x90) R();
    g_riRel = 0; g_rel = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3u)) == block);
    CHECK_EQ(g_riRel, 3);
    CHECK_EQ(g_rel, 3);
    CHECK(g_riRelOrder[0] == e[2]);
    CHECK(g_riRelOrder[1] == e[1]);
    CHECK(g_riRelOrder[2] == e[0]);
}

}  // namespace
