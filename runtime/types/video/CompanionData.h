#pragma once
// ============================================================================
// runtime/types/video/CompanionData.h — Wave 9 lane B2
//
// The twelve `Video::*Data` companion base classes, and the two small owner
// classes whose only unported members dispatch through them.
//
// SHAPE, FROM THE BYTES. Runtime declares one abstract base per renderer
// resource (`DeviceData`, `ContextData`, `VertexBufferData`, ...). The plugin
// — VideoDX11, which in this project is `dx11/` and therefore already ours —
// derives from each one. Every base is the same two-pointer object and every
// ctor/dtor pair is the same three or four instructions, read at the twelve
// addresses listed in runtime/src/video/companion_data.cpp:
//
//     ??0XData(XData* this, X* owner)   this->vtable = ??_7XData; this->owner = owner; return this;
//     ??1XData(XData* this)             this->vtable = ??_7XData;
//
// so ONE struct covers all twelve. That is not an assumption carried over
// from one class to the others: all 24 bodies were disassembled individually
// and each writes its OWN class's exported vtable — the per-class vtable
// addresses are tabulated in companion_data.cpp and each was checked against
// the LEA operand in that class's own ctor and dtor.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtVideo {

// ----------------------------------------------------------------------------
// The shared layout of all twelve `Video::*Data` bases — 0x10 bytes.
//
// `owner` is typed `void*` deliberately: it is a `Device*` for `DeviceData`, a
// `VertexBuffer*` for `VertexBufferData` and so on, and none of the ported
// bodies dereference it. They store it and nothing else.
//
// There is no `sizeof` assertion here because 0x10 is a LOWER bound, not a
// measured total: the ctor writes +0x00 and +0x08 and the derived dx11 object
// continues past that (dx11/types/VideoDX11Data.h owns those layouts). The two
// offsets the ported bodies actually touch are asserted.
// ----------------------------------------------------------------------------
struct CompanionData {
    void* vtable;  // +0x00  ??_7<Class>@Video@@6B@, stamped by both ctor and dtor
    void* owner;   // +0x08  the Video-side object this platform record belongs to
};
static_assert(offsetof(CompanionData, vtable) == 0x00, "CompanionData::vtable at +0x00");
static_assert(offsetof(CompanionData, owner) == 0x08, "CompanionData::owner at +0x08");

// ----------------------------------------------------------------------------
// Video::ConstantBuffer — the fields ??0ConstantBuffer@Video@@QEAA@PEAVDevice@1@@Z
// (0x1800eedf0) writes. Only two of them are read by this lane's one ported
// member (`Bind`), but the whole ctor is transcribed so the two offsets are
// anchored in something checkable rather than asserted in isolation:
//
//   1800eee08  MOV [RCX],RAX        \  RAX = 0
//   1800eee0b  MOV [RCX+0x08],RAX    |
//   1800eee0f  MOV [RCX+0x10],-1     |  ctor constants
//   1800eee17  MOV [RCX+0x18],AL     |
//   1800eee1a  MOV [RCX+0x1c],RAX    |  (deliberately unaligned in retail)
//   1800eee1e  MOV [RCX+0x28],RAX    |
//   1800eee22  MOV [RCX+0x30],RAX   /
//   1800eee26  MOV [RCX+0x38],RDX      device
//   1800eee3c  CALL [RAX+0x58]         Device::CreateConstantBufferData(this)
//   1800eee3f  MOV [RBX+0x40],RAX      platformData
//
// No `sizeof` assertion: the ctor's last write is at +0x40, so 0x48 is a lower
// bound and nothing in the shipped binary pins the total. Asserting a guessed
// size would be a static_assert that cannot fail for the right reason.
// ----------------------------------------------------------------------------
struct ConstantBuffer {
    uint8_t  head[0x38];    // +0x00..+0x37  name/prefix Strings, indices, flags — not read here
    void*    device;        // +0x38  Video::Device* (ctor's second parameter)
    void*    platformData;  // +0x40  dx11::ConstantBufferData*, from Device vtable +0x58
};
static_assert(offsetof(ConstantBuffer, device) == 0x38, "ConstantBuffer::device at +0x38");
static_assert(offsetof(ConstantBuffer, platformData) == 0x40, "ConstantBuffer::platformData at +0x40");

// ----------------------------------------------------------------------------
// `Video::ImageBufferData`'s POD view, MOVED HERE from
// runtime/types/video/ImageBuffer.h by ENDGAME B.3.5 block A.
//
// RULE B4b-1: the retail name now belongs to the real abstract class
// `Video::ImageBufferData` further down this file, so the POD moves into the
// `Rt*` namespace. `ImageBuffer.h` keeps a global-scope alias to this
// declaration so its one consumer — `runtime/src/video/buffers.cpp`, which
// writes `pd->vtable` by hand in three trampolines and in the ported
// `??1ImageBufferData` door — compiles unchanged. There is now exactly ONE
// definition of these two fields instead of two.
//
// It is NOT collapsed into `CompanionData` above even though the bytes are
// identical: `buffers.cpp` names the second field `imageBuffer`, and the whole
// point of B4b-1 is to stop the same layout being spelled twice, not to rename
// a live field in a file this block does not own (block C owns `ImageBuffer.h`
// and `buffers.cpp` after this).
//
// Decompiled from (Wave 6 lane C):
//   Ctor(ImageBuffer*):  0x1800f8c90 (??0ImageBufferData@Video@@QEAA@PEAVImageBuffer@1@@Z)
//   Copy ctor:           0x18007dd80 (??0ImageBufferData@Video@@QEAA@AEBV01@@Z)
//   Dtor:                0x1800f8e10 (??1ImageBufferData@Video@@UEAA@XZ)
// ----------------------------------------------------------------------------
struct ImageBufferData {
    void* vtable;       // +0x00
    void* imageBuffer;  // +0x08  owning Video::ImageBuffer* (set by the ctor, untouched by the dtor)
};
static_assert(sizeof(ImageBufferData) == 0x10, "ImageBufferData must be 0x10 bytes");
static_assert(offsetof(ImageBufferData, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(ImageBufferData, imageBuffer) == 0x08, "imageBuffer at +0x08");

}  // namespace RtVideo

// ############################################################################
// THE TWELVE `Video::*Data` ABSTRACT BASES, as real C++ classes.
// ENDGAME B.3.5 block A, RULE B5-1.
// ############################################################################
//
// THE BOUNDARY, AND WHICH HALF THIS IS. Runtime declares one ABSTRACT base per
// renderer resource; VideoDX11 — `dx11/`, ours — derives the concrete override
// set from it. Confirmed byte-wise for all twelve (B35_RULES 1.1): the Runtime
// vtable has a CONCRETE slot 0 (a deleting dtor in Runtime's
// 0x180058590..0x180058b10 band) and `_purecall` (0x18007ec28) in EVERY other
// slot, and the object is `{vptr, owner}`.
//
//   class                 ??_7 (Runtime)  slots   dx11 concrete vtable
//   DeviceData            0x180158c30      20     Vt::DeviceData
//   ContextData           0x180158b90      20     Vt::ContextData
//   ImageBufferData       0x180158b50       7     Vt::ImageBufferData
//   IndexBufferData       0x180158af8       7     Vt::IndexBufferData
//   VertexBufferData      0x180158ac0       7     Vt::VertexBufferData
//   ConstantBufferData    0x180158b30       4     Vt::ConstantBufferData
//   RenderBufferData      0x180158a28       4     Vt::RenderBufferData
//   VertexLayoutData      0x180158aa8       3     Vt::VertexLayoutData
//   VertexProgramData     0x180158a90       3     Vt::VertexProgramData
//   PixelProgramData      0x180158a78       3     Vt::PixelProgramData
//   RenderStateData       0x180158a60       3     Vt::RenderStateData
//   VideoProgramData      0x180158a48       3     Vt::VideoProgramData
//
// Two independent instruments agree on all twelve slot counts: the Runtime
// `.rdata` read (B35_SLOTS.md) and `dx11/funcs/dx11_seam.h`'s 84-slot table,
// read out of the shipped plugin in an earlier wave.
//
// NO `sizeof` ASSERT, DELIBERATELY (RULE B5-1.1). 0x10 is a LOWER bound, not a
// measured total: the ctor writes +0x00 and +0x08 and the DERIVED dx11 object
// continues past that (`dx11/types/VideoDX11Data.h` owns those layouts and
// keeps its own size asserts — `DeviceData` is 0x70). The two offsets the
// ported bodies actually touch are what is asserted, once, on
// `RtVideo::CompanionData` above; `offsetof` on these polymorphic classes would
// add nothing it does not already say, so each class below asserts only that it
// is still the two-pointer object.
//
// SLOT 0 IS CONCRETE IN RETAIL AND IS DECLARED `= 0` HERE, which is B.3.4's
// settled shape (B34_BRIEFS A.4: "slot 0 is left `override = 0` because its body
// lives in iobase_vtables.cpp, registered by address"). Retail's slot 0 IS a
// real body per class — the twelve addresses in the 0x180058590..0x180058b10
// band, which is what makes these classes abstract-with-a-concrete-slot-0 rather
// than fully abstract — and ours is already registered BY ADDRESS in
// `runtime/src/object/deleting_dtor.cpp` (e.g. row 52 for `ImageBufferData`,
// 0x180058a10). Declaring it `virtual XData* DeletingDtor(unsigned)` without
// `= 0` is what does NOT work, and the reason is worth recording: defining
// `~XData()` below makes GCC emit a vptr store at the top of the destructor,
// which needs `vtable for Video::XData`, which needs slot 0's body — twelve
// `undefined reference to 'vtable for Video::XData'` at link (seen, not
// predicted). Pure makes every entry known (`__cxa_pure_virtual`) and the
// vtable emittable, and that vtable is immediately overwritten by the explicit
// store of the MINTED address anyway (see companion_data.cpp).
//
// ENDGAME B.2.5 / B35_RULES 11 keep the `RT_DELDTOR` rows and the twelve
// `??_7*Data` mint rows exactly where they are — the mint row is what FILLS slot
// 0 of the minted table.
//
// SLOT NAMES. Retail gives none: every slot but 0 is `_purecall`, and a pure
// slot's name can only be read off an override. The overrides are ours —
// `dx11/`'s 84 `DX11_REGISTER_SLOT` registrations, whose labels were recovered
// from the shipped VideoDX11 vtables lane by lane — so the names below are
// COPIED from those labels and the signatures from those functions' parameter
// lists. Where a lane could not recover a name (four `DeviceData` stubs, one
// `RenderBufferData` and two `VideoProgramData` slots, all of which the shipped
// plugin implements as a constant return) the slot is spelled `SlotN` rather
// than given an invented name.
//
// NO VIRTUAL DESTRUCTOR (ENDGAME B.1 decision 2). `~XData()` is PUBLIC and
// non-virtual, which is the one place these classes differ from the abstract
// bases B.3.4 produced: RULE B5-3 requires each dx11 derived destructor to call
// `self->Video::XData::~XData()` as a QUALIFIED non-virtual call, and the dx11
// concrete classes are a DECLARED LEAVE (RULE B5-8) that does not actually
// derive in C++ — so the call site is not a derived class and a `protected`
// destructor would be unreachable to it. It must NOT become `DeletingDtor(0)`:
// that re-enters the derived slot 0.
//
// NO REAL CONSTRUCTOR (B4b-2). All twelve `??0XData` bodies are a pure write of
// two fields, `owner` first and then the vtable; they stay registered ctor
// DOORS in `runtime/src/video/companion_data.cpp`, over the
// `RtVideo::CompanionData` view, because the first of the two fields is the
// vptr and no constructor can write it to the MINTED address.

namespace Video {

// ---- 20 slots -------------------------------------------------------------
class DeviceData {
public:
    virtual DeviceData* DeletingDtor(unsigned flags) = 0;            // [ 0] 0x180058b10
    virtual bool EnumerateInterfaces() = 0;                      // [ 1]
    virtual bool CreateDevice() = 0;                             // [ 2]
    virtual void Shutdown() = 0;                                 // [ 3]
    virtual void  Slot4() = 0;                                   // [ 4] dx11: void stub
    virtual float Slot5() = 0;                                   // [ 5] dx11: returns 0.0f
    virtual bool  Slot6() = 0;                                   // [ 6] dx11: returns false
    virtual bool  Slot7() = 0;                                   // [ 7] dx11: returns true
    // HCURSOR-shaped: (imageBuffer, hotspotX, hotspotY).
    virtual void SetImageBuffer(void* imageBuffer, int hotX, int hotY) = 0;  // [ 8]
    // [ 9]..[19] — the eleven companion factories, in retail slot order. Each
    // takes the owning Video-side object and returns the new `*Data`. The
    // Runtime-side callers reach these by BYTE OFFSET: `Video::VertexLayout`'s
    // constructor uses +0x70 (slot 14) and `Video::ConstantBuffer`'s +0x58
    // (slot 11).
    virtual void* CreateContextData(void* owner) = 0;            // [ 9] +0x48
    virtual void* CreateImageBufferData(void* owner) = 0;        // [10] +0x50
    virtual void* CreateConstantBufferData(void* owner) = 0;     // [11] +0x58
    virtual void* CreateIndexBufferData(void* owner) = 0;        // [12] +0x60
    virtual void* CreateVertexBufferData(void* owner) = 0;       // [13] +0x68
    virtual void* CreateVertexLayoutData(void* owner) = 0;       // [14] +0x70
    virtual void* CreateVertexProgramData(void* owner) = 0;      // [15] +0x78
    virtual void* CreatePixelProgramData(void* owner) = 0;       // [16] +0x80
    virtual void* CreateRenderStateData(void* owner) = 0;        // [17] +0x88
    virtual void* CreateVideoProgramData(void* owner) = 0;       // [18] +0x90
    virtual void* CreateRenderBufferData(void* owner) = 0;       // [19] +0x98

    ~DeviceData();          // ??1DeviceData@Video@@UEAA@XZ  0x1800f8cf0

    void* _owner;           // +0x08  Video::Device*
};

// ---- 20 slots -------------------------------------------------------------
class ContextData {
public:
    virtual ContextData* DeletingDtor(unsigned flags) = 0;           // [ 0] 0x180058a90
    virtual bool BeginCommands() = 0;                            // [ 1]
    virtual void EndCommands() = 0;                              // [ 2]
    virtual void Finish() = 0;                                   // [ 3]
    virtual void Clear(unsigned flags, unsigned color,
                       float depth, int stencil) = 0;            // [ 4]
    virtual void BindRenderBuffer() = 0;                         // [ 5]
    virtual void BindViewport() = 0;                             // [ 6]
    virtual void BindScissor() = 0;                              // [ 7]
    virtual void BindTransforms() = 0;                           // [ 8]
    virtual void BindTextureNone(int slot) = 0;                  // [ 9]
    virtual void BindIndexNone() = 0;                            // [10]
    virtual void BindVertexNone(int slot) = 0;                   // [11]
    virtual void DrawIndexed(int primitiveType, int baseVertex, int vertexCount,
                             int startIndex, int indexCount,
                             int instanceCount) = 0;             // [12]
    virtual int  GetMaxDynamicInstances(int a, int b, int c) = 0; // [13]
    virtual bool BeginVertices(int vertexCount, int vertexStride) = 0;        // [14]
    virtual void EndVertices(int primitiveType) = 0;                          // [15]
    virtual bool BeginIndexedVertices(int vertexCount, int vertexStride,
                                      int indexCount) = 0;                    // [16]
    virtual void EndIndexedVertices(int primitiveType) = 0;                   // [17]
    virtual bool BeginInstancedIndexedVertices(int vertexCount, int vertexStride,
                                               int indexCount,
                                               int instanceCount) = 0;        // [18]
    virtual void EndInstancedIndexedVertices(int primitiveType) = 0;          // [19]

    ~ContextData();         // ??1ContextData@Video@@UEAA@XZ  0x1800f8e30

    void* _owner;           // +0x08  Video::Context*
};

// ---- 7 slots --------------------------------------------------------------
class ImageBufferData {
public:
    virtual ImageBufferData* DeletingDtor(unsigned flags) = 0;       // [0] 0x180058a10
    virtual void AttachColorBuffer() = 0;                        // [1]
    virtual void AttachDepthBuffer() = 0;                        // [2]
    // `levels` is a `Collection::Array<ImageLevel>`-shaped header; dx11 spells
    // it `ImageLevelArray`.
    virtual bool Create(const void* levels) = 0;                 // [3]
    virtual void Bind(void* unknownArg, unsigned slot) = 0;      // [4]
    virtual bool Update(void* data, int width, int height) = 0;  // [5]
    virtual bool Read(void* dst, int width, int height) = 0;     // [6]

    // ??1ImageBufferData@Video@@UEAA@XZ  0x1800f8e10. Its registered DOOR is
    // `Video_ImageBufferData_Dtor` in `runtime/src/video/buffers.cpp` (Wave 6
    // lane C), not in companion_data.cpp; the member body lives with the other
    // eleven and that door converts to a delegation when block C takes
    // `buffers.cpp`.
    ~ImageBufferData();

    void* _owner;           // +0x08  Video::ImageBuffer*
};

// ---- 7 slots --------------------------------------------------------------
class IndexBufferData {
public:
    virtual IndexBufferData* DeletingDtor(unsigned flags) = 0;        // [0] 0x180058910
    virtual bool Create(uint32_t sizeBytes, uint8_t dynamic,
                        const void* data) = 0;                   // [1]
    virtual bool SetData(int32_t sizeBytes, int32_t offsetBytes,
                         const void* src) = 0;                   // [2]
    // `Video::IndexBuffer::Lock`/`Unlock` (companion_data.cpp) are three-
    // instruction tail-call trampolines into these two, +0x18 and +0x20.
    virtual void* Lock(int32_t sizeBytes, int32_t* outByteOffset) = 0;  // [3] +0x18
    virtual void  Unlock() = 0;                                  // [4] +0x20
    virtual void  Bind() = 0;                                    // [5]
    virtual void  Serialize(void* serializer) = 0;               // [6]

    ~IndexBufferData();     // ??1IndexBufferData@Video@@UEAA@XZ  0x1800f8df0

    void* _owner;           // +0x08  Video::IndexBuffer*
};

// ---- 7 slots --------------------------------------------------------------
class VertexBufferData {
public:
    virtual VertexBufferData* DeletingDtor(unsigned flags) = 0;       // [0] 0x180058890
    virtual bool Create(uint32_t sizeBytes, uint8_t dynamic,
                        const void* data) = 0;                   // [1]
    virtual bool SetData(int32_t sizeBytes, int32_t offsetBytes,
                         const void* src) = 0;                   // [2]
    virtual void* Lock(int32_t sizeBytes, int32_t* outByteOffset) = 0;  // [3] +0x18
    virtual void  Unlock() = 0;                                  // [4] +0x20
    // The one slot where the vertex and index shapes differ.
    virtual void  Bind(void* context, uint32_t startSlot, uint32_t offsetBytes,
                       uint32_t strideBytes) = 0;                // [5]
    virtual void  Serialize(void* serializer) = 0;               // [6]

    ~VertexBufferData();    // ??1VertexBufferData@Video@@UEAA@XZ  0x1800f8d50

    void* _owner;           // +0x08  Video::VertexBuffer*
};

// ---- 4 slots --------------------------------------------------------------
class ConstantBufferData {
public:
    virtual ConstantBufferData* DeletingDtor(unsigned flags) = 0;     // [0] 0x180058990
    virtual bool Create() = 0;                                   // [1]
    // `Video::ConstantBuffer::Bind` (0x1800eef70) tail-jumps into this one
    // through +0x10, off the owner's +0x40.
    virtual void Bind(void* unused, const void* srcData, void* unused2,
                      int32_t sizeBytes) = 0;                    // [2] +0x10
    virtual int32_t SlotIndexPlusOne() = 0;                      // [3]

    ~ConstantBufferData();  // ??1ConstantBufferData@Video@@UEAA@XZ  0x1800f8e50

    void* _owner;           // +0x08  Video::ConstantBuffer*
};

// ---- 4 slots --------------------------------------------------------------
class RenderBufferData {
public:
    virtual RenderBufferData* DeletingDtor(unsigned flags) = 0;       // [0] 0x180058590
    virtual bool Slot1() = 0;                                    // [1] dx11: returns true
    virtual void Bind(void* context) = 0;                        // [2]
    virtual void Resolve(void* context, void* other, bool flag) = 0;  // [3]

    ~RenderBufferData();    // ??1RenderBufferData@Video@@UEAA@XZ  0x1800f8db0

    void* _owner;           // +0x08  Video::RenderBuffer*
};

// ---- 3 slots --------------------------------------------------------------
class VertexLayoutData {
public:
    virtual VertexLayoutData* DeletingDtor(unsigned flags) = 0;       // [0] 0x180058810
    virtual void Bind(void* context) = 0;                        // [1]
    // void, unlike VertexProgramData's and PixelProgramData's bool — VideoDX11's
    // 0x180003ff0 falls off its end with no return value set.
    virtual void PlatformCreate() = 0;                           // [2] +0x10

    ~VertexLayoutData();    // ??1VertexLayoutData@Video@@UEAA@XZ  0x1800f8d30

    void* _owner;           // +0x08  Video::VertexLayout*
};

// ---- 3 slots --------------------------------------------------------------
class VertexProgramData {
public:
    virtual VertexProgramData* DeletingDtor(unsigned flags) = 0;      // [0] 0x180058790
    virtual bool PlatformCreate() = 0;                           // [1]
    virtual void Bind(void* context, int permutation) = 0;        // [2]

    ~VertexProgramData();   // ??1VertexProgramData@Video@@UEAA@XZ  0x1800f8d10

    void* _owner;           // +0x08  Video::VertexProgram*
};

// ---- 3 slots --------------------------------------------------------------
class PixelProgramData {
public:
    virtual PixelProgramData* DeletingDtor(unsigned flags) = 0;       // [0] 0x180058710
    virtual bool PlatformCreate() = 0;                           // [1]
    virtual void Bind(void* context, int permutation) = 0;        // [2]

    ~PixelProgramData();    // ??1PixelProgramData@Video@@UEAA@XZ  0x1800f8dd0

    void* _owner;           // +0x08  Video::PixelProgram*
};

// ---- 3 slots --------------------------------------------------------------
class RenderStateData {
public:
    virtual RenderStateData* DeletingDtor(unsigned flags) = 0;        // [0] 0x180058690
    virtual bool Create() = 0;                                   // [1]
    virtual void Bind(void* context) = 0;                        // [2]

    // ??1RenderStateData@Video@@UEAA@XZ. Its registered DOOR is in
    // `runtime/src/video/device.cpp` (Wave 6 lane B), not in
    // companion_data.cpp — see the banner there. Same arrangement as
    // `ImageBufferData`'s above.
    ~RenderStateData();

    void* _owner;           // +0x08  Video::RenderState*
};

// ---- 3 slots --------------------------------------------------------------
class VideoProgramData {
public:
    virtual VideoProgramData* DeletingDtor(unsigned flags) = 0;       // [0] 0x180058610
    virtual bool Slot1() = 0;                                    // [1] dx11: returns true
    virtual void Slot2() = 0;                                     // [2] dx11: void stub

    ~VideoProgramData();    // ??1VideoProgramData@Video@@UEAA@XZ  0x1800f8d90

    void* _owner;           // +0x08  Video::VideoProgram*
};

// The two offsets, once per class, as the two things every one of the twelve
// really claims: the vptr is the first qword and `owner` is the second. A
// `sizeof` assert would be the false witness RULE B5-1.1 names.
#define RT_VIDEO_COMPANION_PINS(CLASS)                                          \
    static_assert(sizeof(CLASS) == sizeof(RtVideo::CompanionData),              \
                  #CLASS " is the two-pointer object CompanionData pins");      \
    static_assert(alignof(CLASS) == alignof(void*), #CLASS " is qword-aligned");\
    static_assert(offsetof(CLASS, _owner) == 0x08, #CLASS "::_owner at +0x08")

RT_VIDEO_COMPANION_PINS(DeviceData);
RT_VIDEO_COMPANION_PINS(ContextData);
RT_VIDEO_COMPANION_PINS(ImageBufferData);
RT_VIDEO_COMPANION_PINS(IndexBufferData);
RT_VIDEO_COMPANION_PINS(VertexBufferData);
RT_VIDEO_COMPANION_PINS(ConstantBufferData);
RT_VIDEO_COMPANION_PINS(RenderBufferData);
RT_VIDEO_COMPANION_PINS(VertexLayoutData);
RT_VIDEO_COMPANION_PINS(VertexProgramData);
RT_VIDEO_COMPANION_PINS(PixelProgramData);
RT_VIDEO_COMPANION_PINS(RenderStateData);
RT_VIDEO_COMPANION_PINS(VideoProgramData);

#undef RT_VIDEO_COMPANION_PINS

}  // namespace Video
