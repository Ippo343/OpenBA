// ============================================================================
// tests/unit/video_imagebuffer_abi_test.cpp — ENDGAME B.3.5 block C.
//
// The ABI claims block C makes, and nothing else. Model:
// `tests/unit/video_data_abi_test.cpp` (block A), `io_vtable_abi_test.cpp`,
// `object_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// Block C declares five real C++ classes over layouts the rest of the tree still
// reaches by raw byte offset:
//
//   * `Video::ImageBuffer` (0x68, 10 slots, `Object::CompiledExternal`), whose
//     slot 9 `Create` is reached from `imagebuffer_platform_serialize.cpp` and
//     `saveload_draw.cpp` (:455) as a virtual by name, and whose
//     `_data` at +0x60 is what `dx11/src/dx11_backend_access.h` reads now that
//     its `ImageBufferOwner` view is gone;
//   * the CHAIN across the namespace boundary — `Graphics::GeneratedImageBuffer`
//     (0x70, 12 slots, abstract, adds `Generate` 10 and `Render` 11 as pure) and
//     its two concretes `FrameCopyImageBuffer` (0x88) and
//     `ReflectionImageBuffer` (0x90), whose retail slot +0x50 `Generate`
//     `saveload_draw.cpp:468` calls by name;
//   * `Video::RenderState` (0x80, 9 slots, `Object::External` DIRECTLY) and its
//     three nested value types.
//
// So the step is only sound if GCC lays a declared slot N at byte offset N*8 with
// `this` unadjusted, ACROSS A THREE-LEVEL CHAIN THAT CROSSES A NAMESPACE, and if
// the two namespace branches do not perturb the layout.
//
// WHY MIRROR CLASSES AND NOT THE REAL ONES for the slot part. Exactly the posture
// block A and `io_vtable_abi_test.cpp` take: the real classes' virtuals are
// registered doors in `runtime/src/video/` and `runtime/src/graphics/` that stand
// on the port registry, the address registry and `Collection::Array`'s allocator
// seam, and cannot link into a host test. The real classes' LAYOUT is checked
// directly (PARTS A, D, E — `sizeof`/`offsetof` need no definition and no
// instantiation); the SLOT ABI is checked on classes declared here in the same
// shape, because what is under test is what GCC does with that shape, not what
// the bodies do.
//
// No tree source links (see the block in tests/unit/sources.mk):
// `runtime/src/object/external_class.cpp` is already on that list and supplies
// `Object::Base` / `External` / `CompiledExternal`'s linkable half, which is all
// the layout asserts in these headers need.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "unit.h"
#include "slot_check.h"
#include "dtor_check.h"

#include "video/ImageBuffer.h"
#include "video/RenderState.h"
#include "graphics/GeneratedImageBuffer.h"
#include "graphics/FrameCopyImageBuffer.h"

namespace {

// The one instruction every raw caller idiom compiles to.
// `byteOffset` is RETAIL's; T (the object's class, here the mirror) decides the compiler's.
template <typename Fn, class T>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(*reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(vt) + RetailByteOffset<T>(byteOffset)));
}

const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
long long   g_lastArg  = 0;

void Note(const void* self, int mark, long long arg = 0) {
    g_lastSelf = self;
    g_lastMark = mark;
    g_lastArg  = arg;
}
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; g_lastArg = 0; }

}  // namespace

// ===========================================================================
// PART A — `Video::ImageBuffer`'s layout, and its agreement with the POD.
//
// B4b-1 moved the POD to `RtVideo::ImageBuffer` and left a global alias; the two
// must keep describing the same bytes for as long as block D2's five files read
// the POD while this block's four read the class. A field accidentally added to
// either would move `_data` at +0x60, which `dx11/` reads, and fail HERE rather
// than in a black window.
// ===========================================================================
UNIT_TEST(video_imagebuffer_layout) {
    CHECK_EQ(sizeof(Video::ImageBuffer), (size_t)0x68);
    CHECK_EQ(alignof(Video::ImageBuffer), (size_t)8);
    CHECK_EQ(sizeof(Video::ImageBuffer), sizeof(RtVideo::ImageBuffer));

    // The `Object::CompiledExternal` base adds no bytes over `External`'s 0x20,
    // which is what puts `_width` at +0x20 rather than anywhere else.
    CHECK_EQ(sizeof(Object::CompiledExternal), (size_t)0x20);
    CHECK_EQ(sizeof(Object::External), sizeof(Object::CompiledExternal));

    CHECK_EQ(offsetof(Video::ImageBuffer, _width),      (size_t)0x20);
    CHECK_EQ(offsetof(Video::ImageBuffer, _height),     (size_t)0x24);
    CHECK_EQ(offsetof(Video::ImageBuffer, _dimension),  (size_t)0x28);
    CHECK_EQ(offsetof(Video::ImageBuffer, _kind),       (size_t)0x2C);
    CHECK_EQ(offsetof(Video::ImageBuffer, _format),     (size_t)0x30);
    CHECK_EQ(offsetof(Video::ImageBuffer, _flags),      (size_t)0x34);
    CHECK_EQ(offsetof(Video::ImageBuffer, _addressU),   (size_t)0x38);
    CHECK_EQ(offsetof(Video::ImageBuffer, _addressV),   (size_t)0x3C);
    CHECK_EQ(offsetof(Video::ImageBuffer, _addressW),   (size_t)0x40);
    CHECK_EQ(offsetof(Video::ImageBuffer, _filter),     (size_t)0x44);
    CHECK_EQ(offsetof(Video::ImageBuffer, _sourceName), (size_t)0x48);
    CHECK_EQ(offsetof(Video::ImageBuffer, _field50),    (size_t)0x50);
    CHECK_EQ(offsetof(Video::ImageBuffer, _field58),    (size_t)0x58);
    // THE ONE dx11 READS. `dx11/src/dx11_backend_access.h`'s deleted
    // `ImageBufferOwner` pinned exactly this offset (RULE B5-2.1).
    CHECK_EQ(offsetof(Video::ImageBuffer, _data),       (size_t)0x60);

    // Class and POD, field for field on the fourteen that matter.
    CHECK_EQ(offsetof(Video::ImageBuffer, _width),
             offsetof(RtVideo::ImageBuffer, width));
    CHECK_EQ(offsetof(Video::ImageBuffer, _kind),
             offsetof(RtVideo::ImageBuffer, kind));
    CHECK_EQ(offsetof(Video::ImageBuffer, _sourceName),
             offsetof(RtVideo::ImageBuffer, pixelData));
    CHECK_EQ(offsetof(Video::ImageBuffer, _data),
             offsetof(RtVideo::ImageBuffer, platformData));

    // `_data` is the real abstract base block A declared, not a `void*`.
    CHECK(  (std::is_same<decltype(Video::ImageBuffer::_data),
                          Video::ImageBufferData*>::value) );
}

// ===========================================================================
// PART B — slot order across the whole chain, on mirrors.
//
// Retail slot 0 is the deleting destructor. Since B3X W1 the real chain
// declares `virtual ~T()` there, which GCC lays out as TWO entries (D1
// complete, D0 deleting): retail slot n >= 1 is compiler slot n + 1, and the
// SlotAt reads translate (RetailByteOffset). The mirrors follow the product,
// with a recording class-scope operator delete for the root's MemFree one.
// The mirrors give slot 9, `Compile` and
// `Generate` a BODY where the real classes declare them `= 0`; that is the one
// deliberate difference, and the reason is the same as block A's — the real
// bodies are `deleting_dtor.cpp` table rows and `buffers.cpp` /
// `wave23_lane_gfxbody.cpp` doors, not linkable here, and a probe has to be
// instantiable to have a vtable to read. The slot INDEX is what is under test,
// and it does not move.
//
// The shape mirrored is the one B35_SLOTS.md read from bytes:
//   Video::ImageBuffer                10 slots, 0..9   (Create introduced at 9)
//   Graphics::GeneratedImageBuffer     12 slots        (Generate 10, Render 11)
//   Graphics::FrameCopy / Reflection   12 slots
// ===========================================================================
namespace {

// The three `Object::Base`/`External`/`CompiledExternal` slots the chain
// inherits, flattened into the mirror so the indices line up with retail's.
class MirrorImageBuffer {
public:
    virtual ~MirrorImageBuffer() { Note(this, 0); }                // [0] D1 + D0
    DTOR_CHECK_RECORDING_DELETE
    virtual const char* TypeName() const = 0;                      // [1]
    virtual uint32_t    Type() const = 0;                          // [2]
    virtual int32_t     GetFileVersion() const = 0;                // [3]
    // [4] External's own, NOT pure in retail (`?Compile@External@Object@@`);
    // given a body here so PART C has a concrete base member to call qualified.
    virtual uint32_t    Compile(const void* path) {
        Note(this, 104, static_cast<long long>(reinterpret_cast<uintptr_t>(path))); return 1u;
    }
    virtual bool        Serialize(void* stream) = 0;               // [5]
    virtual void*       Reconstruct() = 0;                         // [6]
    virtual void        Destruct() = 0;                            // [7]
    virtual void*       Construct() = 0;                           // [8]
    virtual bool        Create(int32_t a, int32_t b, int32_t c, uint32_t d,
                               int32_t w, int32_t h, int32_t f,
                               int32_t u, int32_t v, int32_t ww, void* lv) = 0;  // [9]
};

// The intermediate base: inherits 6, 7, 8 AND 9 and adds two PURE slots.
class MirrorGenerated : public MirrorImageBuffer {
public:
    virtual void Generate(void* context, int32_t generation) const = 0;   // [10]
    virtual void Render(void* renderer, const void* scene, const void* camera,
                        int32_t generation) const = 0;                    // [11]
};

// `ReflectionImageBuffer`'s override set: 1,2,3,5,6,7,8,10,11 — and slot 9
// INHERITED, which is what makes `ProbeGenerated`'s own `Create` the one the
// vtable carries at +0x48.
class ProbeGenerated final : public MirrorGenerated {
public:
    const char* TypeName() const override { Note(this, 1); return "probe"; }
    uint32_t    Type() const override     { Note(this, 2); return 0u; }
    int32_t     GetFileVersion() const override { Note(this, 3); return 0x1007; }
    uint32_t    Compile(const void*) override   { Note(this, 4); return 1u; }
    bool        Serialize(void*) override       { Note(this, 5); return true; }
    void*       Reconstruct() override          { Note(this, 6); return this; }
    void        Destruct() override             { Note(this, 7); }
    void*       Construct() override            { Note(this, 8); return this; }
    bool        Create(int32_t, int32_t usage, int32_t, uint32_t, int32_t, int32_t,
                       int32_t, int32_t, int32_t, int32_t, void*) override {
        Note(this, 9, usage); return true;
    }
    void Generate(void*, int32_t generation) const override { Note(this, 10, generation); }
    void Render(void*, const void*, const void*, int32_t g) const override { Note(this, 11, g); }
    ~ProbeGenerated() = default;
};

}  // namespace

UNIT_TEST(video_imagebuffer_chain_slot_order) {
    ProbeGenerated probe;
    MirrorImageBuffer* base = &probe;

    // `this` is UNADJUSTED across all three levels of single inheritance: a
    // Runtime body that casts a `Graphics::ReflectionImageBuffer*` to a
    // `Video::ImageBuffer*` and calls through the vtable must hand the same
    // pointer over, because the minted table's bodies take `this` verbatim.
    CHECK_EQ((const void*)base, (const void*)&probe);
    CHECK_EQ((const void*)static_cast<MirrorGenerated*>(&probe), (const void*)&probe);

    using DtorFn      = void (*)(void*);
    using TypeNameFn  = const char* (*)(const void*);
    using TypeFn      = uint32_t (*)(const void*);
    using VersionFn   = int32_t (*)(const void*);
    using CompileFn   = uint32_t (*)(void*, const void*);
    using SerializeFn = bool (*)(void*, void*);
    using PtrFn       = void* (*)(void*);
    using VoidFn      = void (*)(void*);
    using CreateFn    = bool (*)(void*, int32_t, int32_t, int32_t, uint32_t, int32_t,
                                 int32_t, int32_t, int32_t, int32_t, int32_t, void*);
    using GenerateFn  = void (*)(const void*, void*, int32_t);
    using RenderFn    = void (*)(const void*, void*, const void*, const void*, int32_t);

    // Retail slot 0: raw D1 (really destroys, so on a placement-new'd probe
    // never destroyed again — `probe` on the stack would run twice) and D0 at
    // compiler slot 1, which frees through the class operator delete.
    {
        alignas(ProbeGenerated) unsigned char buf[sizeof(ProbeGenerated)];
        ProbeGenerated* q = ::new (static_cast<void*>(buf)) ProbeGenerated();
        CHECK(dtor_check::CompilerEntry<const void*>(q, 0) != dtor_check::CompilerEntry<const void*>(q, 1));
        Reset(); dtor_check::ResetFrees();
        SlotAt<DtorFn, ProbeGenerated>(q, 0x00)(q);
        CHECK_EQ(g_lastMark, 0);
        CHECK_EQ((const void*)g_lastSelf, (const void*)q);
        CHECK_EQ(dtor_check::g_frees, 0);
        ProbeGenerated* h = dtor_check::NewOnMalloc<ProbeGenerated>();
        Reset(); dtor_check::ResetFrees();
        dtor_check::CallD0(h);
        CHECK_EQ(g_lastMark, 0);
        CHECK_EQ((const void*)g_lastSelf, (const void*)h);
        CHECK_EQ(dtor_check::g_frees, 1);
        CHECK(dtor_check::g_lastFree == h);
    }

    Reset(); (SlotAt<TypeNameFn, ProbeGenerated>(&probe, 0x08)(&probe));  CHECK_EQ(g_lastMark, 1);
    Reset(); (SlotAt<TypeFn, ProbeGenerated>(&probe, 0x10)(&probe));      CHECK_EQ(g_lastMark, 2);
    Reset(); (SlotAt<VersionFn, ProbeGenerated>(&probe, 0x18)(&probe));   CHECK_EQ(g_lastMark, 3);
    Reset(); (SlotAt<CompileFn, ProbeGenerated>(&probe, 0x20)(&probe, nullptr)); CHECK_EQ(g_lastMark, 4);
    Reset(); (SlotAt<SerializeFn, ProbeGenerated>(&probe, 0x28)(&probe, nullptr)); CHECK_EQ(g_lastMark, 5);
    Reset(); (SlotAt<PtrFn, ProbeGenerated>(&probe, 0x30)(&probe));      CHECK_EQ(g_lastMark, 6);
    Reset(); (SlotAt<VoidFn, ProbeGenerated>(&probe, 0x38)(&probe));     CHECK_EQ(g_lastMark, 7);

    // SLOT 8 IS `Construct`, INTRODUCED BY THIS BRANCH (RULE B5-16).
    // `Object::External`'s and `CompiledExternal`'s retail tables are EIGHT
    // entries each, so `Construct` is not inherited — it is `virtual`, not
    // `override`, on `Video::ImageBuffer`.
    Reset(); (SlotAt<PtrFn, ProbeGenerated>(&probe, 0x40)(&probe));      CHECK_EQ(g_lastMark, 8);

    // SLOT 9 IS `Create`, +0x48 — the offset
    // `imagebuffer_platform_serialize.cpp` used to hand-index and
    // `runtime/src/graphics/imagebuffer_derived.cpp`'s reflection rebuild used to
    // spell as `vt[9]`. `usage = 1` is what makes a reflection's depth member a
    // depth buffer, so it is the argument checked.
    Reset(); SlotAt<CreateFn, ProbeGenerated>(&probe, 0x48)(&probe, 0, 1, 7, 0u, 0, 0, 0, 1, 1, 1, nullptr);
    CHECK_EQ(g_lastMark, 9); CHECK_EQ(g_lastArg, 1LL);

    // SLOTS 10 AND 11 ARE `Generate` AND `Render`, INTRODUCED BY
    // `Graphics::GeneratedImageBuffer` — one level past the namespace boundary.
    // +0x50 is retail's offset of `?Generate@FrameCopyImageBuffer@Graphics@@`
    // (the export walk in `runtime/src/save/saveload_draw.cpp`'s banner, :85),
    // the independent corroboration of this index; that file's call (:468) is
    // `->Generate(ctx, 1)` by name.
    Reset(); SlotAt<GenerateFn, ProbeGenerated>(&probe, 0x50)(&probe, nullptr, 42);
    CHECK_EQ(g_lastMark, 10); CHECK_EQ(g_lastArg, 42LL);
    Reset(); SlotAt<RenderFn, ProbeGenerated>(&probe, 0x58)(&probe, nullptr, nullptr, nullptr, 7);
    CHECK_EQ(g_lastMark, 11); CHECK_EQ(g_lastArg, 7LL);

    // TWELVE retail slots, THIRTEEN compiler entries: B3X W1 gave the chain a
    // virtual destructor, and Itanium spends TWO slots on it, so `Generate` and
    // `Render` (retail +0x50 / +0x58) sit at compiler +0x58 / +0x60 — which is
    // why every caller (`saveload_draw.cpp:468` included) goes by name and a
    // test goes through RetailByteOffset.
    CHECK(!std::is_polymorphic<int>::value);   // sanity: the trait is live
    CHECK(std::has_virtual_destructor<ProbeGenerated>::value == true);
    CHECK_EQ(RetailByteOffset<ProbeGenerated>(0x50), (uintptr_t)0x58);
}

// ===========================================================================
// PART C — the qualified-call rule.
//
// Every converted door in this block is
// `reinterpret_cast<Ns::C*>(self)->Ns::C::Method(args)`. The point of the
// QUALIFICATION is that it is NON-VIRTUAL: it must reach the named class's body
// even when the object is a subclass whose slot overrides it. Three of block C's
// bodies depend on this directly — `GeneratedImageBuffer::Serialize`,
// `FrameCopyImageBuffer::Serialize` and `ReflectionImageBuffer::Serialize` all
// call `Video::ImageBuffer::Serialize(stream)` as their base-class arm, and an
// unqualified call there would be infinite recursion into their own slot.
// ===========================================================================
UNIT_TEST(video_imagebuffer_qualified_call_is_not_virtual) {
    ProbeGenerated probe;
    MirrorGenerated* p = &probe;

    Reset();
    p->Serialize(nullptr);                       // virtual -> the override
    CHECK_EQ(g_lastMark, 5);

    // The base-class arm: qualified, so it must NOT re-enter the override.
    // `MirrorImageBuffer::Serialize` is pure, so the closest thing this mirror
    // can state is that the qualified form of a CONCRETE base member resolves
    // statically, argument and all. B3X W1: this used to be the base's slot-0
    // body with a flags argument; a destructor takes none, so it is checked on
    // the base's own `Compile` (retail slot 4, External's body), which the probe
    // overrides.
    Reset();
    int marker = 0;
    p->MirrorImageBuffer::Compile(&marker);      // the BASE body, not the probe's
    CHECK_EQ(g_lastMark, 104);
    CHECK_EQ(g_lastArg, static_cast<long long>(reinterpret_cast<uintptr_t>(&marker)));
    Reset();
    p->Compile(&marker);                         // virtual -> the override
    CHECK_EQ(g_lastMark, 4);
}

// ===========================================================================
// PART D — the chain's three derived layouts.
//
// `GeneratedImageBuffer` adds exactly ONE field (the int latch at +0x68) plus the
// four bytes MSVC leaves before the 8-aligned tail of the two concretes, and the
// two concretes' owned pointers start at +0x70. Each size is its own deleting
// destructor's `MOV EDX,imm`, and each is cross-checked against the POD the
// unconverted `wave23_lane_gfxbody.cpp` still uses.
// ===========================================================================
UNIT_TEST(graphics_imagebuffer_chain_layout) {
    CHECK_EQ(sizeof(Graphics::GeneratedImageBuffer),  (size_t)0x70);
    CHECK_EQ(sizeof(Graphics::FrameCopyImageBuffer),  (size_t)0x88);
    CHECK_EQ(sizeof(Graphics::ReflectionImageBuffer), (size_t)0x90);

    CHECK_EQ(sizeof(Graphics::GeneratedImageBuffer),
             sizeof(RtGraphics::GeneratedImageBuffer));
    CHECK_EQ(sizeof(Graphics::FrameCopyImageBuffer),
             sizeof(RtGraphics::FrameCopyImageBuffer));
    CHECK_EQ(sizeof(Graphics::ReflectionImageBuffer),
             sizeof(RtGraphics::ReflectionImageBuffer));

    // The latch, inherited by both concretes at the SAME offset — which is why
    // `Graphics::ReflectionImageBuffer` has a field there doing the same job and
    // why it is `GeneratedImageBuffer`'s member, not each concrete's own.
    CHECK_EQ(offsetof(Graphics::GeneratedImageBuffer,  _generation), (size_t)0x68);
    CHECK_EQ(offsetof(Graphics::FrameCopyImageBuffer,  _generation), (size_t)0x68);
    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _generation), (size_t)0x68);

    CHECK_EQ(offsetof(Graphics::FrameCopyImageBuffer, _backBufferImage), (size_t)0x70);
    CHECK_EQ(offsetof(Graphics::FrameCopyImageBuffer, _sourceTarget),    (size_t)0x78);
    CHECK_EQ(offsetof(Graphics::FrameCopyImageBuffer, _destTarget),      (size_t)0x80);

    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _depth),  (size_t)0x70);
    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _target), (size_t)0x78);
    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _planeX), (size_t)0x80);
    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _planeY), (size_t)0x84);
    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _planeZ), (size_t)0x88);
    CHECK_EQ(offsetof(Graphics::ReflectionImageBuffer, _planeD), (size_t)0x8c);

    // The chain really is a chain in C++, not three siblings over one POD: the
    // namespace boundary between `Video::ImageBuffer` and
    // `Graphics::GeneratedImageBuffer` is the one B35_RULES section 4 calls out,
    // and single inheritance across it is what keeps `this` unadjusted.
    CHECK( (std::is_base_of<Video::ImageBuffer, Graphics::GeneratedImageBuffer>::value) );
    CHECK( (std::is_base_of<Graphics::GeneratedImageBuffer,
                            Graphics::FrameCopyImageBuffer>::value) );
    CHECK( (std::is_base_of<Graphics::GeneratedImageBuffer,
                            Graphics::ReflectionImageBuffer>::value) );
    // And `FrameCopy` is NOT a base of `Reflection` — they are siblings, which is
    // the shape `?_ParentTypeName@` gives both ("GeneratedImageBuffer").
    CHECK(!(std::is_base_of<Graphics::FrameCopyImageBuffer,
                            Graphics::ReflectionImageBuffer>::value) );
}

// ===========================================================================
// PART E — `Video::RenderState` and its three nested value types.
//
// The parent is `Object::External` DIRECTLY, and that is the claim worth a test
// rather than a comment: every other Video external in the tree goes through
// `Object::CompiledExternal`, which adds no bytes, so a wrong base would NOT
// change `sizeof` and NOT fail any offset assert — it would only change which
// vtable the object stamps and which literal `?_ParentTypeName@` returns, i.e. it
// would change the type-id CRC and stop saves loading. `is_base_of` is true for
// both, so the test is the NEGATIVE one.
// ===========================================================================
UNIT_TEST(video_renderstate_layout) {
    CHECK_EQ(sizeof(Video::RenderState), (size_t)0x80);
    CHECK_EQ(alignof(Video::RenderState), (size_t)8);

    CHECK( (std::is_base_of<Object::External, Video::RenderState>::value) );
    // THE ONE THAT MATTERS: NOT through CompiledExternal.
    CHECK(!(std::is_base_of<Object::CompiledExternal, Video::RenderState>::value) );
    // ...while `Video::ImageBuffer` IS, which is what makes the two branches
    // different and the distinction worth encoding in the type system.
    CHECK( (std::is_base_of<Object::CompiledExternal, Video::ImageBuffer>::value) );

    CHECK_EQ(offsetof(Video::RenderState, _alphaBlend), (size_t)0x20);
    CHECK_EQ(offsetof(Video::RenderState, _depth),      (size_t)0x34);
    CHECK_EQ(offsetof(Video::RenderState, _stencil),    (size_t)0x44);
    CHECK_EQ(offsetof(Video::RenderState, _cullMode),   (size_t)0x68);
    CHECK_EQ(offsetof(Video::RenderState, _fillMode),   (size_t)0x6c);
    CHECK_EQ(offsetof(Video::RenderState, _flag70),     (size_t)0x70);
    CHECK_EQ(offsetof(Video::RenderState, _data),       (size_t)0x78);

    // `_data` is block A's real abstract base, not a `void*`.
    CHECK(  (std::is_same<decltype(Video::RenderState::_data),
                          Video::RenderStateData*>::value) );

    // The three nested value types. `?Initialize@` copies each of them dword by
    // dword INCLUDING the padding, which the member expresses as a struct
    // assignment — sound only while all three stay trivially copyable, which the
    // real constructors below do not break.
    CHECK_EQ(sizeof(Video::RenderState::AlphaBlend), (size_t)0x14);
    CHECK_EQ(sizeof(Video::RenderState::Depth),      (size_t)0x10);
    CHECK_EQ(sizeof(Video::RenderState::Stencil),    (size_t)0x24);
    CHECK(std::is_trivially_copyable<Video::RenderState::AlphaBlend>::value);
    CHECK(std::is_trivially_copyable<Video::RenderState::Depth>::value);
    CHECK(std::is_trivially_copyable<Video::RenderState::Stencil>::value);

    // `Stencil::writeMask` is serialized as a UChar among eight UInts
    // (0x1800f48e0). A UInt there would move the next eight fields by three
    // bytes in every saved file, so the one-byte width is pinned.
    CHECK_EQ(sizeof(Video::RenderState::Stencil::writeMask), (size_t)1);
    CHECK_EQ(offsetof(Video::RenderState::Stencil, writeMask),  (size_t)0x08);
    CHECK_EQ(offsetof(Video::RenderState::Stencil, reference),  (size_t)0x20);
    CHECK_EQ(offsetof(Video::RenderState::AlphaBlend, writeMask), (size_t)0x10);
    CHECK_EQ(offsetof(Video::RenderState::Depth, func),           (size_t)0x04);

    // B3X W1: `RenderState` and the `ImageBuffer` chain DID acquire a virtual
    // destructor (retail slot 0 = D1 + D0 under GCC), all five together; every
    // raw reader of these tables went by name first (B3X W0).
    CHECK(std::has_virtual_destructor<Video::RenderState>::value == true);
    CHECK(std::has_virtual_destructor<Video::ImageBuffer>::value == true);
    CHECK(std::has_virtual_destructor<Graphics::GeneratedImageBuffer>::value == true);
    CHECK(std::has_virtual_destructor<Graphics::FrameCopyImageBuffer>::value == true);
    CHECK(std::has_virtual_destructor<Graphics::ReflectionImageBuffer>::value == true);

}

// ===========================================================================
// PART F — the explicit padding in the three nested value types.
//
// NOT a behavioural test, and the reason is worth stating rather than leaving as
// a gap: the three exported default constructors (0x1800f4b30 / 0x1800f4a80 /
// 0x1800f49d0) are DEFINED in `runtime/src/video/renderstate.cpp`, which stands
// on `RT_DATA_SLOT` and the port registry and cannot link into a host test, so
// nothing here may construct one — a `Video::RenderState::AlphaBlend ab;` would
// be an undefined reference. Their constants are verified by the real run (the
// `RenderState` ctor door writes the same values and the resave gate reads them
// back out of a save).
//
// What IS checkable here, and matters: retail writes a BYTE at `AlphaBlend+0x00`
// and never touches +0x01..+0x03, and a WORD at `Depth+0x00` and never +0x02.
// Those runs are DECLARED padding, so each struct's size comes from the
// declaration rather than from the compiler's choice of alignment — which is what
// stops `?Initialize@`'s struct assignment (a whole-extent copy, including the
// padding, exactly as retail's dword-by-dword copy does) from moving.
// `sizeof(Class::member)` needs no object.
// ===========================================================================
UNIT_TEST(video_renderstate_nested_padding_is_declared) {
    CHECK_EQ(sizeof(Video::RenderState::AlphaBlend::_pad01), (size_t)3);
    CHECK_EQ(sizeof(Video::RenderState::Depth::_pad02),      (size_t)2);
    CHECK_EQ(sizeof(Video::RenderState::Stencil::_pad02),    (size_t)2);
    CHECK_EQ(sizeof(Video::RenderState::Stencil::_pad09),    (size_t)3);
    // And `RenderState`'s own seven unwritten bytes after `_flag70`, which retail
    // leaves alone and which the class must therefore not initialise.
    CHECK_EQ(sizeof(Video::RenderState::_pad71), (size_t)7);
    // `GeneratedImageBuffer::_pad6c` is NEVER WRITTEN by its ctor or dtor; it is
    // the four bytes MSVC leaves before the 8-aligned tail of the two concretes.
    CHECK_EQ(sizeof(Graphics::GeneratedImageBuffer::_pad6c), (size_t)4);
}
