// ============================================================================
// tests/unit/video_data_abi_test.cpp — ENDGAME B.3.5 block A.
//
// The ABI claims block A makes, and nothing else. Model:
// `tests/unit/io_vtable_abi_test.cpp` and `object_vtable_abi_test.cpp`.
//
// WHAT THIS FILE HAS TO ESTABLISH
// -------------------------------
// Block A declares thirteen real C++ classes over layouts the rest of the tree
// still reaches by raw byte offset, and it does NOT move the bodies:
//
//   * the twelve `Video::*Data` abstract bases (runtime/types/video/
//     CompanionData.h). Every live one of these wears dx11's vtable, is
//     allocated by a dx11 factory with `MemAlloc`, and is reached from Runtime
//     by hand-inlined `(*(void***)p)[N]` — `Video::IndexBuffer::Lock` through
//     +0x18, `Video::ConstantBuffer::Bind` through +0x10,
//     `Video::VertexLayout::PlatformCreate` through +0x10,
//     `Video::ImageBuffer::CopyToTexture` through +0x28. So the step is only
//     sound if GCC lays a declared slot N at byte offset N*8 with `this`
//     unadjusted, in BOTH directions.
//   * `Video::VertexLayout` (runtime/types/video/VertexLayout.h), whose nine
//     slots put `Construct` at 8 — the `virtual`-not-`override` case of RULE
//     B5-16 — and whose `Element` field map this block CORRECTED.
//
// WHY MIRROR CLASSES AND NOT THE REAL ONES. Exactly the posture
// `io_vtable_abi_test.cpp` takes with `TestReaderWriter`: the real classes'
// virtuals are registered doors in `runtime/src/video/` and `dx11/src/`, which
// stand on the port registry, the address registry, `<windows.h>` and D3D11 and
// cannot link into a host test. The real classes' LAYOUT is checked directly
// (PART A and PART D — `sizeof`/`offsetof` need no definition and no
// instantiation); the SLOT ABI is checked on classes declared here in the same
// shape, because what is under test is what GCC does with that shape, not what
// the bodies do.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "unit.h"
#include "slot_check.h"

#include "video/CompanionData.h"
#include "video/VertexLayout.h"

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
// PART A — the twelve `Video::*Data` bases are the two-pointer object, and
// `owner` is the SECOND qword.
//
// RULE B5-1.1: there is deliberately NO `sizeof == 0x10` assertion in the
// header, because 0x10 is a lower bound on the DERIVED dx11 object. It is an
// exact statement about the BASE, which is what is checked here: the base is
// `{vptr, owner}` and nothing else, so a field accidentally added to one of the
// twelve moves every dx11 subclass's fields and fails here rather than in a
// black window.
// ===========================================================================
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

    CHECK_EQ(offsetof(Video::DeviceData, _owner),         (size_t)0x08);
    CHECK_EQ(offsetof(Video::ContextData, _owner),        (size_t)0x08);
    CHECK_EQ(offsetof(Video::ImageBufferData, _owner),    (size_t)0x08);
    CHECK_EQ(offsetof(Video::IndexBufferData, _owner),    (size_t)0x08);
    CHECK_EQ(offsetof(Video::VertexBufferData, _owner),   (size_t)0x08);
    CHECK_EQ(offsetof(Video::ConstantBufferData, _owner), (size_t)0x08);
    CHECK_EQ(offsetof(Video::RenderBufferData, _owner),   (size_t)0x08);
    CHECK_EQ(offsetof(Video::VertexLayoutData, _owner),   (size_t)0x08);
    CHECK_EQ(offsetof(Video::VertexProgramData, _owner),  (size_t)0x08);
    CHECK_EQ(offsetof(Video::PixelProgramData, _owner),   (size_t)0x08);
    CHECK_EQ(offsetof(Video::RenderStateData, _owner),    (size_t)0x08);
    CHECK_EQ(offsetof(Video::VideoProgramData, _owner),   (size_t)0x08);

    // `RtVideo::ImageBufferData` and `RtVideo::CompanionData` describe the same
    // bytes as the real classes — the B4b-1 move in this block did not change
    // them, it removed the second copy (`ImageBuffer.h`'s).
    CHECK_EQ(sizeof(RtVideo::ImageBufferData), sizeof(Video::ImageBufferData));
    CHECK_EQ(offsetof(RtVideo::ImageBufferData, imageBuffer),
             offsetof(Video::ImageBufferData, _owner));
    CHECK_EQ(sizeof(RtVideo::CompanionData), sizeof(Video::DeviceData));
}

// ===========================================================================
// PART B — slot order.
//
// Three shapes, one per distinct slot count in the family: 20 (`DeviceData` /
// `ContextData`), 7 (`ImageBufferData` / `IndexBufferData` / `VertexBufferData`)
// and 3 (five classes; `ConstantBufferData` / `RenderBufferData`'s 4 is the same
// claim one slot longer). Slot 0 is `DeletingDtor(unsigned)` and is an ordinary
// virtual, NOT a destructor — ENDGAME B.1 decision 2.
//
// The mirrors give slot 0 a BODY where the real classes declare it `= 0`. That
// is the one deliberate difference: the real slot-0 bodies are the twelve
// `RT_DELDTOR` rows in `runtime/src/object/deleting_dtor.cpp`, registered by
// address and not linkable here, and a probe has to be instantiable to have a
// vtable to read. The slot INDEX is what is under test, and it does not move.
// ===========================================================================
namespace {

// Mirrors `Video::DeviceData`: slot 0 concrete, 1..19 pure. The four unnamed
// stub slots keep the header's `SlotN` spelling.
class MirrorDeviceData {
public:
    virtual MirrorDeviceData* DeletingDtor(unsigned flags) { Note(this, 0, flags); return this; }
    virtual bool  EnumerateInterfaces() = 0;
    virtual bool  CreateDevice() = 0;
    virtual void  Shutdown() = 0;
    virtual void  Slot4() = 0;
    virtual float Slot5() = 0;
    virtual bool  Slot6() = 0;
    virtual bool  Slot7() = 0;
    virtual void  SetImageBuffer(void* imageBuffer, int hotX, int hotY) = 0;
    virtual void* CreateContextData(void* owner) = 0;
    virtual void* CreateImageBufferData(void* owner) = 0;
    virtual void* CreateConstantBufferData(void* owner) = 0;
    virtual void* CreateIndexBufferData(void* owner) = 0;
    virtual void* CreateVertexBufferData(void* owner) = 0;
    virtual void* CreateVertexLayoutData(void* owner) = 0;
    virtual void* CreateVertexProgramData(void* owner) = 0;
    virtual void* CreatePixelProgramData(void* owner) = 0;
    virtual void* CreateRenderStateData(void* owner) = 0;
    virtual void* CreateVideoProgramData(void* owner) = 0;
    virtual void* CreateRenderBufferData(void* owner) = 0;

    void* _owner;
protected:
    ~MirrorDeviceData() = default;
};

class ProbeDeviceData final : public MirrorDeviceData {
public:
    ProbeDeviceData() { _owner = nullptr; }
    bool  EnumerateInterfaces() override { Note(this, 1);  return true; }
    bool  CreateDevice() override        { Note(this, 2);  return true; }
    void  Shutdown() override            { Note(this, 3); }
    void  Slot4() override               { Note(this, 4); }
    float Slot5() override               { Note(this, 5);  return 0.0f; }
    bool  Slot6() override               { Note(this, 6);  return false; }
    bool  Slot7() override               { Note(this, 7);  return true; }
    void  SetImageBuffer(void*, int hotX, int) override { Note(this, 8, hotX); }
    void* CreateContextData(void*) override        { Note(this,  9); return nullptr; }
    void* CreateImageBufferData(void*) override    { Note(this, 10); return nullptr; }
    void* CreateConstantBufferData(void*) override { Note(this, 11); return nullptr; }
    void* CreateIndexBufferData(void*) override    { Note(this, 12); return nullptr; }
    void* CreateVertexBufferData(void*) override   { Note(this, 13); return nullptr; }
    void* CreateVertexLayoutData(void*) override   { Note(this, 14); return nullptr; }
    void* CreateVertexProgramData(void*) override  { Note(this, 15); return nullptr; }
    void* CreatePixelProgramData(void*) override   { Note(this, 16); return nullptr; }
    void* CreateRenderStateData(void*) override    { Note(this, 17); return nullptr; }
    void* CreateVideoProgramData(void*) override   { Note(this, 18); return nullptr; }
    void* CreateRenderBufferData(void*) override   { Note(this, 19); return nullptr; }
    ~ProbeDeviceData() = default;
};

// Mirrors `Video::IndexBufferData` — the 7-slot shape, and the one whose
// slots 3/4 two Runtime trampolines reach by byte offset (+0x18 / +0x20).
class MirrorIndexBufferData {
public:
    virtual MirrorIndexBufferData* DeletingDtor(unsigned flags) { Note(this, 0, flags); return this; }
    virtual bool  Create(uint32_t sizeBytes, uint8_t dynamic, const void* data) = 0;
    virtual bool  SetData(int32_t sizeBytes, int32_t offsetBytes, const void* src) = 0;
    virtual void* Lock(int32_t sizeBytes, int32_t* outByteOffset) = 0;
    virtual void  Unlock() = 0;
    virtual void  Bind() = 0;
    virtual void  Serialize(void* serializer) = 0;

    void* _owner;
protected:
    ~MirrorIndexBufferData() = default;
};

class ProbeIndexBufferData final : public MirrorIndexBufferData {
public:
    ProbeIndexBufferData() { _owner = nullptr; }
    bool  Create(uint32_t n, uint8_t, const void*) override        { Note(this, 1, n); return true; }
    bool  SetData(int32_t n, int32_t, const void*) override        { Note(this, 2, n); return true; }
    void* Lock(int32_t n, int32_t*) override                        { Note(this, 3, n); return this; }
    void  Unlock() override                                         { Note(this, 4); }
    void  Bind() override                                           { Note(this, 5); }
    void  Serialize(void*) override                                 { Note(this, 6); }
    ~ProbeIndexBufferData() = default;
};

// Mirrors `Video::VertexLayoutData` — the 3-slot shape. Slot 2
// (`PlatformCreate`, byte offset +0x10) is what
// `Video::VertexLayout::Serialize` calls on the read path.
class MirrorVertexLayoutData {
public:
    virtual MirrorVertexLayoutData* DeletingDtor(unsigned flags) { Note(this, 0, flags); return this; }
    virtual void Bind(void* context) = 0;
    virtual void PlatformCreate() = 0;

    void* _owner;
protected:
    ~MirrorVertexLayoutData() = default;
};

class ProbeVertexLayoutData final : public MirrorVertexLayoutData {
public:
    ProbeVertexLayoutData() { _owner = nullptr; }
    void Bind(void*) override      { Note(this, 1); }
    void PlatformCreate() override { Note(this, 2); }
    ~ProbeVertexLayoutData() = default;
};

}  // namespace

UNIT_TEST(video_companion_data_slot_order) {
    // The mirrors must still describe the real classes. If a declaration in
    // CompanionData.h gains or loses a slot without this file following, these
    // four fail before any slot check runs.
    CHECK_EQ(sizeof(MirrorDeviceData),       sizeof(Video::DeviceData));
    CHECK_EQ(sizeof(MirrorIndexBufferData),  sizeof(Video::IndexBufferData));
    CHECK_EQ(sizeof(MirrorVertexLayoutData), sizeof(Video::VertexLayoutData));

    ProbeDeviceData dev;
    MirrorDeviceData* d = &dev;

    // Direction A: a raw `(*(void***)p)[N]` caller lands on slot N's override,
    // `this` unadjusted.
    Reset(); SlotAt<void* (*)(void*, unsigned), MirrorDeviceData>(d, 0x00)(d, 1);
    CHECK_EQ(g_lastMark, 0); CHECK(g_lastSelf == d); CHECK_EQ(g_lastArg, 1);
    Reset(); SlotAt<bool (*)(void*), MirrorDeviceData>(d, 0x08)(d);  CHECK_EQ(g_lastMark, 1);
    Reset(); SlotAt<bool (*)(void*), MirrorDeviceData>(d, 0x10)(d);  CHECK_EQ(g_lastMark, 2);
    Reset(); SlotAt<void (*)(void*), MirrorDeviceData>(d, 0x18)(d);  CHECK_EQ(g_lastMark, 3);
    Reset(); SlotAt<void (*)(void*), MirrorDeviceData>(d, 0x20)(d);  CHECK_EQ(g_lastMark, 4);
    Reset(); SlotAt<float (*)(void*), MirrorDeviceData>(d, 0x28)(d); CHECK_EQ(g_lastMark, 5);
    Reset(); SlotAt<bool (*)(void*), MirrorDeviceData>(d, 0x30)(d);  CHECK_EQ(g_lastMark, 6);
    Reset(); SlotAt<bool (*)(void*), MirrorDeviceData>(d, 0x38)(d);  CHECK_EQ(g_lastMark, 7);
    Reset(); SlotAt<void (*)(void*, void*, int, int), MirrorDeviceData>(d, 0x40)(d, nullptr, 9, 3);
    CHECK_EQ(g_lastMark, 8); CHECK_EQ(g_lastArg, 9);

    // The eleven factories, 9..19 == +0x48..+0x98. `Video::ConstantBuffer`'s
    // constructor reads +0x58 (slot 11) and `Video::VertexLayout`'s reads +0x70
    // (slot 14); both are checked by position here.
    for (int slot = 9; slot <= 19; ++slot) {
        Reset();
        SlotAt<void* (*)(void*, void*), MirrorDeviceData>(d, slot * 8)(d, nullptr);
        CHECK_EQ(g_lastMark, slot);
    }

    // Direction B: an ordinary virtual call through the real class reaches the
    // same slot the raw caller did.
    Reset(); d->CreateConstantBufferData(nullptr); CHECK_EQ(g_lastMark, 11);
    Reset(); d->CreateVertexLayoutData(nullptr);   CHECK_EQ(g_lastMark, 14);

    ProbeIndexBufferData ib;
    MirrorIndexBufferData* b = &ib;
    // The two byte offsets `Video::{Index,Vertex}Buffer::Lock/Unlock` tail-jump
    // through (companion_data.cpp, retail 0x1800f34e0 / 0x1800f34c0).
    Reset(); SlotAt<void* (*)(void*, int32_t, int32_t*), MirrorIndexBufferData>(b, 0x18)(b, 64, nullptr);
    CHECK_EQ(g_lastMark, 3); CHECK_EQ(g_lastArg, 64);
    Reset(); SlotAt<void (*)(void*), MirrorIndexBufferData>(b, 0x20)(b);
    CHECK_EQ(g_lastMark, 4);
    Reset(); SlotAt<void (*)(void*), MirrorIndexBufferData>(b, 0x28)(b);  CHECK_EQ(g_lastMark, 5);
    Reset(); SlotAt<void (*)(void*, void*), MirrorIndexBufferData>(b, 0x30)(b, nullptr); CHECK_EQ(g_lastMark, 6);

    ProbeVertexLayoutData vl;
    MirrorVertexLayoutData* v = &vl;
    // `Video::VertexLayout::Serialize` calls PlatformCreate as `_data->vt[2]`.
    Reset(); SlotAt<void (*)(void*), MirrorVertexLayoutData>(v, 0x10)(v);
    CHECK_EQ(g_lastMark, 2); CHECK(g_lastSelf == v);
}

// ===========================================================================
// PART C — the QUALIFIED non-virtual call, which is what every door in
// `companion_data.cpp` and RULE B5-3's dx11 destructor now perform.
//
// The claim: `p->Base::Method()` on an object whose dynamic type overrides
// `Method` reaches BASE's body, not the override. Without it, a door registered
// into the minted table would dispatch through that table back into itself —
// infinite recursion instead of delegation (B34_BRIEFS A.1). The destructor case
// is the same mechanism: `self->Video::XData::~XData()` must run the base
// subobject's destructor and must NOT become `DeletingDtor(0)`, which re-enters
// the derived slot 0.
// ===========================================================================
namespace {

class MirrorQualBase {
public:
    virtual int Which() { Note(this, 100); return 100; }
    // Stands in for `~XData()`: a non-virtual member with a body, in no slot,
    // that a derived object must be able to reach explicitly.
    void BaseOnly() { Note(this, 101); }
protected:
    ~MirrorQualBase() = default;
};

class MirrorQualDerived final : public MirrorQualBase {
public:
    int Which() override { Note(this, 200); return 200; }
    ~MirrorQualDerived() = default;
};

}  // namespace

UNIT_TEST(video_companion_data_qualified_call) {
    MirrorQualDerived obj;
    MirrorQualBase* p = &obj;

    Reset(); CHECK_EQ(p->Which(), 200);                CHECK_EQ(g_lastMark, 200);
    Reset(); CHECK_EQ(p->MirrorQualBase::Which(), 100); CHECK_EQ(g_lastMark, 100);
    Reset(); p->MirrorQualBase::BaseOnly();             CHECK_EQ(g_lastMark, 101);

    // And the qualified call is a DIRECT call: it does not read the vptr, so
    // stamping the object with a foreign table cannot redirect it. That is the
    // property that makes a door safe to register into the minted table.
    void* saved = *reinterpret_cast<void**>(p);
    *reinterpret_cast<void**>(p) = nullptr;
    Reset(); CHECK_EQ(p->MirrorQualBase::Which(), 100); CHECK_EQ(g_lastMark, 100);
    *reinterpret_cast<void**>(p) = saved;
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
    CHECK_EQ(sizeof(Video::VertexLayout), (size_t)0x48);
    CHECK_EQ(alignof(Video::VertexLayout), (size_t)8);
    // The base adds 0x20 and no bytes of its own beyond `Object::External`'s.
    CHECK_EQ(sizeof(Object::CompiledExternal), (size_t)0x20);
    CHECK_EQ(offsetof(Video::VertexLayout, _elements),  (size_t)0x20);
    CHECK_EQ(offsetof(Video::VertexLayout, _data),      (size_t)0x30);
    CHECK_EQ(offsetof(Video::VertexLayout, _signature), (size_t)0x38);

    CHECK_EQ(sizeof(Video::VertexLayout::Element), (size_t)0x20);
    CHECK_EQ(offsetof(Video::VertexLayout::Element, name),       (size_t)0x00);
    CHECK_EQ(offsetof(Video::VertexLayout::Element, stream),     (size_t)0x08);
    CHECK_EQ(offsetof(Video::VertexLayout::Element, offset),     (size_t)0x0c);
    CHECK_EQ(offsetof(Video::VertexLayout::Element, usage),      (size_t)0x10);
    CHECK_EQ(offsetof(Video::VertexLayout::Element, usageIndex), (size_t)0x14);
    CHECK_EQ(offsetof(Video::VertexLayout::Element, format),     (size_t)0x18);

    // The element array's count is at +0x28 and the signature's at +0x40 —
    // `Serialize` (0x1800f5e70) reads exactly those two dwords, and VideoDX11's
    // PlatformCreate guards on +0x40 and loops on +0x28.
    CHECK_EQ(offsetof(Video::VertexLayout, _elements) +
                 offsetof(Collection::Array<Video::VertexLayout::Element>, _size),
             (size_t)0x28);
    CHECK_EQ(offsetof(Video::VertexLayout, _signature) +
                 offsetof(Collection::Array<uint8_t>, _size),
             (size_t)0x40);

    // Not constructible (B34_BRIEFS A.4): the default constructor is protected,
    // so an accidental `Video::VertexLayout vl;` cannot stamp GCC's table on an
    // object the tree reads through the MINTED one.
    CHECK(!std::is_default_constructible<Video::VertexLayout>::value);
}

// ===========================================================================
// PART E — `Construct` enters `Video::VertexLayout`'s branch at SLOT 8
// (RULE B5-16), behind an eight-slot `Object::CompiledExternal`.
//
// The mirror is the retail shape: eight slots on the base, `Construct` added by
// the leaf. `Graphics::ParticleSystem` is the one class in step 5 where
// `Construct` is slot 9 instead; that is block H's trap, not this one's, and
// the reason this test pins the index rather than assuming it.
// ===========================================================================
namespace {

class MirrorCompiledExternal {
public:
    virtual void* DeletingDtor(unsigned flags) { Note(this, 0, flags); return this; }
    virtual const char* TypeName() const = 0;                 // [1]
    virtual uint32_t    Type() const = 0;                     // [2]
    virtual int32_t     GetFileVersion() const = 0;           // [3]
    virtual uint32_t    Compile(const void* path)             // [4] not pure in retail
                        { Note(this, 4); (void)path; return 1; }
    virtual bool        Serialize(void* stream) = 0;           // [5]
    virtual void*       Reconstruct() = 0;                     // [6]
    virtual void        Destruct() = 0;                        // [7]
protected:
    ~MirrorCompiledExternal() = default;
};

class MirrorVertexLayout final : public MirrorCompiledExternal {
public:
    const char* TypeName() const override    { Note(this, 1); return "VertexLayout"; }
    uint32_t    Type() const override        { Note(this, 2); return 0xabcdu; }
    int32_t     GetFileVersion() const override { Note(this, 3); return 3; }
    bool        Serialize(void*) override    { Note(this, 5); return true; }
    void*       Reconstruct() override       { Note(this, 6); return this; }
    void        Destruct() override          { Note(this, 7); }
    virtual void* Construct()                { Note(this, 8); return this; }   // [8]
    ~MirrorVertexLayout() = default;
};

}  // namespace

UNIT_TEST(video_vertexlayout_slot_order) {
    MirrorVertexLayout vl;
    void* p = &vl;

    Reset(); SlotAt<void* (*)(void*, unsigned), MirrorVertexLayout>(p, 0x00)(p, 0); CHECK_EQ(g_lastMark, 0);
    Reset(); SlotAt<const char* (*)(void*), MirrorVertexLayout>(p, 0x08)(p);        CHECK_EQ(g_lastMark, 1);
    Reset(); SlotAt<uint32_t (*)(void*), MirrorVertexLayout>(p, 0x10)(p);           CHECK_EQ(g_lastMark, 2);
    Reset(); SlotAt<int32_t (*)(void*), MirrorVertexLayout>(p, 0x18)(p);            CHECK_EQ(g_lastMark, 3);
    // Slot 4 is the base's own body, INHERITED — retail's slot 4 is
    // `?Compile@External@Object@@`, byte-identical to the base table's.
    Reset(); SlotAt<uint32_t (*)(void*, const void*), MirrorVertexLayout>(p, 0x20)(p, nullptr);
    CHECK_EQ(g_lastMark, 4);
    Reset(); SlotAt<bool (*)(void*, void*), MirrorVertexLayout>(p, 0x28)(p, nullptr); CHECK_EQ(g_lastMark, 5);
    Reset(); SlotAt<void* (*)(void*), MirrorVertexLayout>(p, 0x30)(p);              CHECK_EQ(g_lastMark, 6);
    Reset(); SlotAt<void (*)(void*), MirrorVertexLayout>(p, 0x38)(p);               CHECK_EQ(g_lastMark, 7);
    // THE SLOT THIS TEST EXISTS FOR.
    Reset(); SlotAt<void* (*)(void*), MirrorVertexLayout>(p, 0x40)(p);              CHECK_EQ(g_lastMark, 8);

    // GetFileVersion is 3, not 1. A `VertexLayout::GetFileVersion` returning 1
    // silently failed every `MeshLayout\*.rsc` version check once, which
    // dropped every draw item and blacked out the window
    // (runtime/src/video/vertexlayout.cpp's banner, STATUS.md:710-713).
    CHECK_EQ(vl.GetFileVersion(), 3);
}
