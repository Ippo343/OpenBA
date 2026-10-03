// ============================================================================
// tests/unit/video_program_abi_test.cpp — ENDGAME B.3.5 block B.
//
// The ABI claims block B makes about the Video program/shader family, and
// nothing else. Model: `tests/unit/video_data_abi_test.cpp` (block A) and
// `io_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// Block B turns five layouts into real C++ classes with THREE DIFFERENT PARENTS,
// and that is the whole risk:
//
//     Object::External           -> Video::VideoProgram
//     Object::CompiledExternal   -> Video::ShaderProgram -> Video::VertexProgram
//                                                        -> Video::PixelProgram
//                                -> Video::VertexLayout
//
// `VIDEO_GRAPHICS_INVENTORY.md` called the family "all External subclasses,
// mechanical after one" (B35_RULES 2.1's headline trap). PART A is the test that
// fails if someone ever re-parents `VideoProgram` onto `CompiledExternal` or
// `ShaderProgram`: `Video::VideoProgram` must be convertible to
// `Object::External` and must NOT be convertible to `Object::CompiledExternal`.
//
// WHY MIRROR CLASSES FOR THE SLOT ORDER. The same posture block A and
// `io_vtable_abi_test.cpp` take: the real classes' virtuals are registered doors
// in `runtime/src/video/`, which stand on the port registry, the address registry
// and `Video::Device::_singleton`, and cannot link into a host test; and all five
// real classes declare slot 0 `override = 0` (their slot-0 bodies are the
// `RT_DELDTOR` rows in `runtime/src/object/deleting_dtor.cpp`, registered by
// address), so none of them can be instantiated to have a vtable read back. The
// real classes' LAYOUT is checked DIRECTLY — `sizeof`/`offsetof` need neither a
// definition nor an instantiation — and the SLOT ABI is checked on classes
// declared here in the same shape, because what is under test is what GCC does
// with that shape.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "unit.h"
#include "slot_check.h"
#include "dtor_check.h"

#include "video/ShaderProgram.h"
#include "io/Stream.h"   // IO::MemoryStream: Serialize takes IO::Stream&
#include "video/VertexLayout.h"
#include "video/VideoProgram.h"

namespace {

// The one instruction every raw caller idiom compiles to.
// `byteOffset` is RETAIL's; T (the object's class, here the mirror) decides the compiler's.
template <typename Fn, class T>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* const* vt = *reinterpret_cast<const uint8_t* const* const*>(obj);
    return reinterpret_cast<Fn>(*reinterpret_cast<void* const*>(
        reinterpret_cast<const uint8_t*>(vt) + RetailByteOffset<T>(byteOffset)));
}

int  g_lastMark = -1;
void Note(int mark) { g_lastMark = mark; }
void Reset()        { g_lastMark = -1; }

}  // namespace

// ===========================================================================
// PART A — THE THREE PARENTS. The trap this block exists to avoid.
//
// A pointer conversion to a base is a compile-time fact about the class's
// inheritance, so `std::is_convertible` IS the test: it cannot pass for a class
// that was given the wrong parent. `Object::CompiledExternal` adds no BYTES over
// `Object::External` (both 0x20), which is exactly why the mistake would be
// invisible to every size and offset assertion in this file — the only thing it
// changes is which vtable a constructor stamps, and B35_RULES 2.1 says retail's
// `??0VideoProgram` stamps three tables where `??0ShaderProgram` stamps four.
// ===========================================================================
UNIT_TEST(video_program_parents) {
    // ShaderProgram and VertexLayout ARE CompiledExternals.
    CHECK((std::is_convertible<Video::ShaderProgram*, Object::CompiledExternal*>::value));
    CHECK((std::is_convertible<Video::VertexLayout*,  Object::CompiledExternal*>::value));

    // VertexProgram and PixelProgram are ShaderPrograms — and so, transitively,
    // CompiledExternals. They are NOT each other.
    CHECK((std::is_convertible<Video::VertexProgram*, Video::ShaderProgram*>::value));
    CHECK((std::is_convertible<Video::PixelProgram*,  Video::ShaderProgram*>::value));
    CHECK(!(std::is_convertible<Video::VertexProgram*, Video::PixelProgram*>::value));
    CHECK(!(std::is_convertible<Video::PixelProgram*,  Video::VertexProgram*>::value));

    // *** VideoProgram IS AN External AND IS NOT A CompiledExternal. ***
    CHECK((std::is_convertible<Video::VideoProgram*, Object::External*>::value));
    CHECK(!(std::is_convertible<Video::VideoProgram*, Object::CompiledExternal*>::value));
    CHECK(!(std::is_convertible<Video::VideoProgram*, Video::ShaderProgram*>::value));

    // Every one of the five reaches Object::Base, which is what makes the minted
    // `??_7` tables interchangeable with GCC's slot for slot.
    CHECK((std::is_convertible<Video::ShaderProgram*, Object::Base*>::value));
    CHECK((std::is_convertible<Video::VideoProgram*,  Object::Base*>::value));

    // The base subobject is at offset 0 in all of them — single inheritance, no
    // virtual bases — which is what licenses the `reinterpret_cast` bridges every
    // door in this family uses.
    CHECK_EQ((size_t)reinterpret_cast<const char*>(
                 static_cast<const Object::External*>(
                     reinterpret_cast<const Video::VideoProgram*>(0x1000))) -
             (size_t)0x1000,
             (size_t)0);
    CHECK_EQ((size_t)reinterpret_cast<const char*>(
                 static_cast<const Video::ShaderProgram*>(
                     reinterpret_cast<const Video::VertexProgram*>(0x1000))) -
             (size_t)0x1000,
             (size_t)0);
}

// ===========================================================================
// PART B — the five layouts, straight off the real classes.
//
// Sizes from the `MOV EDX,imm` in each class's own slot-0 vector deleting
// destructor, which is also what its `RT_DELDTOR` row carries; offsets from the
// exported getters and the constructors named in the headers.
// ===========================================================================
UNIT_TEST(video_program_layout) {
    CHECK_EQ(sizeof(Video::ShaderProgram), (size_t)0x50);
    CHECK_EQ(sizeof(Video::VertexProgram), (size_t)0x58);
    CHECK_EQ(sizeof(Video::PixelProgram),  (size_t)0x58);
    CHECK_EQ(sizeof(Video::VertexLayout),  (size_t)0x48);
    CHECK_EQ(sizeof(Video::VideoProgram),  (size_t)0x48);

    CHECK_EQ(alignof(Video::ShaderProgram), (size_t)8);
    CHECK_EQ(alignof(Video::VideoProgram),  (size_t)8);

    // ShaderProgram's three arrays, and the `_size` each accessor reads.
    CHECK_EQ(offsetof(Video::ShaderProgram, _textures),  (size_t)0x20);
    CHECK_EQ(offsetof(Video::ShaderProgram, _constants), (size_t)0x30);
    CHECK_EQ(offsetof(Video::ShaderProgram, _byteCode),  (size_t)0x40);
    CHECK_EQ(offsetof(Video::ShaderProgram, _byteCode) +
                 offsetof(Collection::Array<Collection::Array<uint8_t>>, _size),
             (size_t)0x48);

    // The one pointer each program subclass adds, and the two are at the SAME
    // offset — identical layouts, different vtables.
    CHECK_EQ(offsetof(Video::VertexProgram, _data), (size_t)0x50);
    CHECK_EQ(offsetof(Video::PixelProgram,  _data), (size_t)0x50);

    // VideoProgram's four references and its companion, each named by its own
    // four-instruction exported getter (0x18004ee10 .. 0x18004ee50).
    CHECK_EQ(offsetof(Video::VideoProgram, _vertexLayout),  (size_t)0x20);
    CHECK_EQ(offsetof(Video::VideoProgram, _vertexProgram), (size_t)0x28);
    CHECK_EQ(offsetof(Video::VideoProgram, _pixelProgram),  (size_t)0x30);
    CHECK_EQ(offsetof(Video::VideoProgram, _renderState),   (size_t)0x38);
    CHECK_EQ(offsetof(Video::VideoProgram, _data),          (size_t)0x40);
    // Each is exactly one pointer: the ctor's four zero stores and Serialize's
    // four per-T calls describe the same object only if this holds.
    CHECK_EQ(sizeof(Object::ExternalPtr<Video::VertexLayout>), (size_t)8);
}

// ===========================================================================
// PART C — `ShaderProgram::Parameter` MUST STAY TRIVIAL, and this is the test
// that catches the double release.
//
// `Parameter` is held by value inside `Collection::Array<Parameter>`. Retail
// exports five special members for it (??0 x3, ??4, ??1) and block B declares
// NONE of them, because declaring a destructor or a copy would flip
// `rtcoll::SpecFor<Parameter>::destroy` from null to `&TDestroy<Parameter>` —
// and then the compiler-emitted `~Array<Parameter>` would release every
// parameter NAME a second time, after the registered array destructor
// (0x18004fb00, `ReleaseStringsCounted`) already did. The spec is checked here
// directly, not just the type traits, because the spec is what the array
// actually consults.
// ===========================================================================
UNIT_TEST(video_shader_parameter_trivial) {
    CHECK_EQ(sizeof(Video::ShaderProgram::Parameter), (size_t)0x10);
    CHECK_EQ(offsetof(Video::ShaderProgram::Parameter, dest),     (size_t)0x08);
    CHECK_EQ(offsetof(Video::ShaderProgram::Parameter, internal), (size_t)0x0c);

    CHECK((std::is_trivially_destructible<Video::ShaderProgram::Parameter>::value));
    CHECK((std::is_trivially_copyable<Video::ShaderProgram::Parameter>::value));

    // The consequence, read off the array spec the real member consults.
    const rtcoll::ArraySpec* spec =
        Collection::Array<Video::ShaderProgram::Parameter>::Spec();
    CHECK(spec->destroy == nullptr);
    CHECK_EQ(spec->stride, (uint32_t)0x10);

    // `VertexLayout::Element` is the same arrangement one class over: its `name`
    // is a `void*` so that the ported `~Array<Element>` (0x18004fa30) stays the
    // only thing that releases the names.
    CHECK((std::is_trivially_destructible<Video::VertexLayout::Element>::value));
    CHECK(Collection::Array<Video::VertexLayout::Element>::Spec()->destroy == nullptr);
    CHECK_EQ(Collection::Array<Video::VertexLayout::Element>::Spec()->stride, (uint32_t)0x20);
}

// ===========================================================================
// PART D — slot order, on mirrors of the two shapes in this family: the 8-slot
// abstract base and the 9-slot leaf that introduces `Construct` at slot 8.
//
// RULE B5-16 is the claim: `Object::External`'s and `CompiledExternal`'s retail
// tables are EIGHT entries, so every Video/Graphics external introduces
// `Construct` itself, `virtual` and not `override`. If `Construct` were declared
// `override` of something, or declared before `Serialize`, the branch's slot 8
// would move and `Reconstruct`/`Destruct` would shift with it — and because all
// three return a pointer and take no arguments, a swap of 6 and 8 would be
// SILENT at runtime until the object was rebuilt.
//
// The mirrors give slot 0 a body where the real classes declare it `= 0`, for
// the reason block A's mirrors do: a probe has to be instantiable to have a
// vtable to read.
//
// B3X W1: retail slot 0 is `virtual ~T()` in the real family now — TWO GCC
// entries (D1 complete, D0 deleting) — so retail slot n >= 1 is compiler slot
// n + 1 and the SlotAt reads translate. The mirror follows the product, with a
// recording class-scope operator delete for the root's MemFree one.
// ===========================================================================
namespace {

// The eight slots of `Video::ShaderProgram`, in retail order. Slots 3, 6 and 7
// are `_purecall` in retail and pure here, exactly as `Object::External` leaves
// them; slot 4 (`Compile`) is inherited and not redeclared, so it still occupies
// +0x20.
class MirrorShaderProgram {
public:
    virtual ~MirrorShaderProgram()               { Note(0); }                        // [0] D1 + D0
    DTOR_CHECK_RECORDING_DELETE
    virtual const char* TypeName() const         { Note(1); return nullptr; }        // [1] +0x08
    virtual uint32_t    Type() const             { Note(2); return 0; }              // [2] +0x10
    virtual int32_t     GetFileVersion() const   { Note(3); return 0; }              // [3] +0x18
    virtual uint32_t    Compile(const void*)     { Note(4); return 1; }              // [4] +0x20
    virtual bool        Serialize(IO::Stream&)         { Note(5); return true; }           // [5] +0x28
    virtual void*       Reconstruct()            { Note(6); return this; }           // [6] +0x30
    virtual void        Destruct()               { Note(7); }                        // [7] +0x38
};

// The 9-slot leaf: the SAME eight, overridden, plus `Construct` introduced at 8.
class MirrorVertexProgram final : public MirrorShaderProgram {
public:
    const char* TypeName() const override       { Note(101); return nullptr; }
    uint32_t    Type() const override           { Note(102); return 0; }
    int32_t     GetFileVersion() const override { Note(103); return 5; }
    bool        Serialize(IO::Stream&) override       { Note(105); return true; }
    void*       Reconstruct() override          { Note(106); return this; }
    void        Destruct() override             { Note(107); }
    virtual void* Construct()                   { Note(108); return this; }          // [8] +0x40
    ~MirrorVertexProgram() = default;
};

}  // namespace

UNIT_TEST(video_program_slot_order) {
    MirrorVertexProgram vp;
    MirrorShaderProgram* p = &vp;

    // The base's eight slots, reached by BYTE OFFSET, dispatching to the leaf's
    // overrides — which is what a live object wearing the minted table does.
    // Retail slot 0: raw D1 on a placement-new'd leaf (D1 really destroys; `vp`
    // on the stack would be destroyed twice), D0 at compiler slot 1 frees
    // through the class operator delete.
    {
        alignas(MirrorVertexProgram) unsigned char buf[sizeof(MirrorVertexProgram)];
        MirrorVertexProgram* q = ::new (static_cast<void*>(buf)) MirrorVertexProgram();
        CHECK(dtor_check::CompilerEntry<const void*>(q, 0) != dtor_check::CompilerEntry<const void*>(q, 1));
        dtor_check::ResetFrees();
        Reset(); SlotAt<void (*)(void*), MirrorShaderProgram>(q, 0x00)(q);         CHECK_EQ(g_lastMark, 0);
        CHECK_EQ(dtor_check::g_frees, 0);
        MirrorVertexProgram* h = dtor_check::NewOnMalloc<MirrorVertexProgram>();
        Reset(); dtor_check::CallD0(h);                                            CHECK_EQ(g_lastMark, 0);
        CHECK_EQ(dtor_check::g_frees, 1);
        CHECK(dtor_check::g_lastFree == h);
    }
    Reset(); SlotAt<const char* (*)(void*), MirrorShaderProgram>(p, 0x08)(p);          CHECK_EQ(g_lastMark, 101);
    Reset(); SlotAt<uint32_t (*)(void*), MirrorShaderProgram>(p, 0x10)(p);             CHECK_EQ(g_lastMark, 102);
    Reset(); SlotAt<int32_t (*)(void*), MirrorShaderProgram>(p, 0x18)(p);              CHECK_EQ(g_lastMark, 103);
    Reset(); SlotAt<uint32_t (*)(void*, const void*), MirrorShaderProgram>(p, 0x20)(p, nullptr);
    CHECK_EQ(g_lastMark, 4);   // INHERITED: External::Compile, not overridden
    Reset(); SlotAt<bool (*)(void*, void*), MirrorShaderProgram>(p, 0x28)(p, nullptr); CHECK_EQ(g_lastMark, 105);
    Reset(); SlotAt<void* (*)(void*), MirrorShaderProgram>(p, 0x30)(p);                CHECK_EQ(g_lastMark, 106);
    Reset(); SlotAt<void (*)(void*), MirrorShaderProgram>(p, 0x38)(p);                 CHECK_EQ(g_lastMark, 107);

    // *** Construct is slot 8, at +0x40, and it is the LEAF's own. ***
    Reset(); SlotAt<void* (*)(void*), MirrorShaderProgram>(p, 0x40)(p);                CHECK_EQ(g_lastMark, 108);

    // And it really is the ninth entry: the base has exactly eight, so reading
    // +0x40 off a BASE object would be past the end of its table. That is not
    // testable by reading memory, but it is testable by the declaration — the
    // leaf's `Construct` is not an override of anything.
    CHECK(!(std::is_same<decltype(&MirrorVertexProgram::Construct),
                         decltype(&MirrorShaderProgram::Destruct)>::value));
}

// ===========================================================================
// PART E — the QUALIFIED non-virtual call, on THIS family's two shapes: a door
// delegating to a member, and a subclass's `Serialize` calling its base's.
//
// `Video::VertexProgram::Serialize` calls `this->ShaderProgram::Serialize(…)`,
// which is retail's `CALL 0x1800f5110`. Unqualified it would re-enter itself.
// Every door does the same one level out: a live program wears the MINTED table
// whose slot 5 IS the door, so an unqualified virtual call inside the door is
// infinite recursion, not delegation (B34_BRIEFS A.1).
// ===========================================================================
namespace {

int g_baseSerializeCalls = 0;
int g_leafSerializeCalls = 0;

class MirrorQualBase {
public:
    virtual bool Serialize(IO::Stream&) { ++g_baseSerializeCalls; return true; }
protected:
    ~MirrorQualBase() = default;
};

class MirrorQualLeaf final : public MirrorQualBase {
public:
    bool Serialize(IO::Stream& s) override {
        ++g_leafSerializeCalls;
        // Retail's CALL to the base body, spelled as the qualified call.
        return this->MirrorQualBase::Serialize(s);
    }
    ~MirrorQualLeaf() = default;
};

}  // namespace

UNIT_TEST(video_program_qualified_call) {
    MirrorQualLeaf leaf;
    MirrorQualBase* p = &leaf;

    g_baseSerializeCalls = g_leafSerializeCalls = 0;
    IO::MemoryStream stream;
    CHECK(p->Serialize(stream));
    CHECK_EQ(g_leafSerializeCalls, 1);
    CHECK_EQ(g_baseSerializeCalls, 1);   // exactly once, not recursion

    // A door's shape: the qualified call is a DIRECT call and does not read the
    // vptr, so stamping the object with a foreign table cannot redirect it back
    // into the door. That is the property that makes a door safe to register into
    // the minted table.
    g_baseSerializeCalls = g_leafSerializeCalls = 0;
    void* saved = *reinterpret_cast<void**>(p);
    *reinterpret_cast<void**>(p) = nullptr;
    CHECK(p->MirrorQualBase::Serialize(stream));
    CHECK_EQ(g_baseSerializeCalls, 1);
    CHECK_EQ(g_leafSerializeCalls, 0);
    *reinterpret_cast<void**>(p) = saved;
}

// ===========================================================================
// PART F — which of the five is constructible.
//
// Written when every live instance was built by a registered ctor DOOR that
// stamped a MINTED `??_7…@Video@@6B@`, and `protected = default` constructors
// plus a pure slot 0 kept a `Video::VertexProgram p;` from stamping GCC's table.
// ENDGAME B.3.9 VID made the four leaves concrete with retail's public
// constructors (the mint rows are retired); B3X W1 made the destructors public
// and virtual. So the abstract base alone is still not default-constructible.
// ===========================================================================
UNIT_TEST(video_program_not_constructible) {
    // B3X W1: `is_default_constructible` also asks for an accessible
    // destructor. Until the cutover every destructor here was protected, so
    // all five answered false; since ENDGAME B.3.9 VID the four concrete
    // classes have retail's PUBLIC constructor (0x1800f6100 VertexProgram,
    // 0x1800f3950 PixelProgram, VertexLayout, VideoProgram), and with the
    // public virtual destructor they are default-constructible. The abstract
    // base's constructor stays protected. (Test name kept.)
    CHECK(!(std::is_default_constructible<Video::ShaderProgram>::value));
    CHECK((std::is_default_constructible<Video::VertexProgram>::value));
    CHECK((std::is_default_constructible<Video::PixelProgram>::value));
    CHECK((std::is_default_constructible<Video::VertexLayout>::value));
    CHECK((std::is_default_constructible<Video::VideoProgram>::value));


    // Abstract, because slot 0 is `override = 0` — the `RT_DELDTOR` rows in
    // deleting_dtor.cpp are the bodies and B35_RULES 11 keeps them.
    CHECK((std::is_abstract<Video::ShaderProgram>::value));
    CHECK(!(std::is_abstract<Video::VertexProgram>::value));   // B.3.9 VID: concrete
    CHECK(!(std::is_abstract<Video::PixelProgram>::value));   // B.3.9 VID: concrete
    CHECK(!(std::is_abstract<Video::VertexLayout>::value));   // B.3.9 VID: concrete
    // ENDGAME B.3.9 VID: VideoProgram is CONCRETE (tests/unit/b39_vid_program_test.cpp).
    CHECK(!(std::is_abstract<Video::VideoProgram>::value));

    // B3X W1 retired ENDGAME B.1 decision 2: the whole family has a virtual
    // destructor (retail slot 0 = D1 + D0) and deletion is `delete p`.
    CHECK((std::has_virtual_destructor<Video::ShaderProgram>::value));
    CHECK((std::has_virtual_destructor<Video::VertexProgram>::value));
    CHECK((std::has_virtual_destructor<Video::PixelProgram>::value));
    CHECK((std::has_virtual_destructor<Video::VertexLayout>::value));
    CHECK((std::has_virtual_destructor<Video::VideoProgram>::value));

}
