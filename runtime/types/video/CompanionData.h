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
#include <type_traits>   // std::has_virtual_destructor (B3X X1 pins)

// ENDGAME B.3.5 block D1: the real `Video::ConstantBuffer`. The partial view
// that used to sit in this file (head[0x38] + device + platformData) is gone —
// see the alias below.
#include "ConstantBuffer.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree (the slot-0 bodies)

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
// Video::ConstantBuffer — MOVED, ENDGAME B.3.5 block D1 (B4b-1 / RULE B5-2).
//
// This file used to carry a partial view of the class: `head[0x38]` plus
// `device` @+0x38 and `platformData` @+0x40, the two fields `Bind` reads, with
// no `sizeof` assert because the ctor's last write is at +0x40. It was one of
// FOUR views of the same 0x48 bytes (the others: `ConstantBufferOpaque` in
// Context.h, ten `constexpr` offsets in constant_buffer.cpp, and dx11's
// `ConstantBufferOwner`). The real class now lives in
// `runtime/types/video/ConstantBuffer.h`, with its interior read out of retail's
// one-line accessors — which closes B35_RULES section 13 item 4 — and this is an
// ALIAS so that `runtime/src/video/companion_data.cpp` (block A's file) keeps
// compiling unchanged. There is now ONE definition instead of two.
//
// The ctor transcription that used to be here is in ConstantBuffer.h's banner,
// alongside the accessor that names each field:
//
//   1800eee08  MOV [RCX],RAX        \  RAX = 0   -> _prefix
//   1800eee0b  MOV [RCX+0x08],RAX    |           -> _typeName
//   1800eee0f  MOV [RCX+0x10],-1     |  a QWORD: _ordinalIndex AND _uploadIndex
//   1800eee17  MOV [RCX+0x18],AL     |           -> _bindToVertex (a BYTE)
//   1800eee1a  MOV [RCX+0x1c],RAX    |  unaligned: _registerCount + _bankStride
//   1800eee1e  MOV [RCX+0x28],RAX    |           -> _constantData
//   1800eee22  MOV [RCX+0x30],RAX   /            -> _constantCount/_capacity
//   1800eee26  MOV [RCX+0x38],RDX      device
//   1800eee3c  CALL [RAX+0x58]         Device::CreateConstantBufferData(this)
//   1800eee3f  MOV [RBX+0x40],RAX      platformData
// ----------------------------------------------------------------------------
using ConstantBuffer = ::Video::ConstantBuffer;

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
// ENDGAME B.3 step 9, block VID (B39_RULES.md D1, section 5 item 4): CONCRETE.
// Each class has a REAL public constructor (retail's `??0XData(owner)`: the
// owner at +0x08, the vptr -- now the compiler's), its OWN slot 0 with the
// VECTOR arms of the retired RT_DELDTOR row (element 0x10, over ~XData), and a
// body in every other slot that is retail's: `_purecall` (0x18007ec28), spelled
// `detail::CompanionPureCall()`. The twelve minted `??_7XData@Video@@6B@` rows,
// their RT_DELDTOR rows and the `??1XData` doors are retired
// (tools/rt_mint_retired/vid.txt).
//
// NOTHING DISPATCHES ON THESE TABLES. A live companion is built by dx11's
// factory: `MemAlloc`, the `??0XData` DOOR (companion_data.cpp -- which STAYS,
// reached through dx11's import struct, B39_RULES D1.4 / B9-6, now one line:
// `::new (self) XData(owner)`), then dx11 stores ITS OWN derived table
// (`Dx11LiveVtable`, assembled from DX11_REGISTER_SLOT registrations at
// `Dx11SeamInstall`, never from a Runtime table). Teardown: dx11's slot 0
// re-seats its own table, calls `~XData()` (the base table is stored and never
// read), then frees. So the base table is only ever the vptr between two
// stores; which table it is changes nothing.
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
// non-virtual, which is the one place these classes differ from the B.3.9
// recipe's protected destructor: RULE B5-3 requires each dx11 derived destructor
// to call `self->Video::XData::~XData()` as a QUALIFIED non-virtual call, and the
// dx11 concrete classes are a DECLARED LEAVE (RULE B5-8) that does not actually
// derive in C++ -- so the call site is not a derived class and a `protected`
// destructor would be unreachable to it. It must NOT become `DeletingDtor(0)`:
// that re-enters the derived slot 0.
//
// B.4 (R-VIDEO): each slot 0 returns `void*` -- what MSVC's `??_E` returns (the
// block it freed or `this`) -- and its vector arm reads the element count out of
// the vector-new cookie in the qword before element 0 through ONE marked cast
// (marked with the `dx11_seam` category: the arm exists only because these classes keep the
// hand slot 0 instead of a compiler virtual destructor, and it goes with the
// seam, plan §4.1). The elements are `this[i]`: `sizeof(XData) == 0x10` is the
// retail element stride and RT_VIDEO_COMPANION_PINS below asserts it.

extern "C" void _purecall();   // the CRT's (the exe's standalone/main.cpp logs it)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"   // the _purecall bodies below

namespace Video {

// The owners and the companions the factory slots below name (B.4 R-VIDEO, H4).
class Context;
class ImageBuffer;
class IndexBuffer;
class VertexBuffer;
class VertexLayout;
class VertexProgram;
class PixelProgram;
class RenderState;
class VideoProgram;
class RenderBuffer;
class ContextData;
class ImageBufferData;
class ConstantBufferData;
class IndexBufferData;
class VertexBufferData;
class VertexLayoutData;
class VertexProgramData;
class PixelProgramData;
class RenderStateData;
class VideoProgramData;
class RenderBufferData;

namespace detail {
// Retail's body in every Video::*Data slot but 0: `_purecall` (0x18007ec28).
// Never reached -- a live companion wears dx11's table -- and never returns.
[[noreturn]] inline void CompanionPureCall() { _purecall(); __builtin_trap(); }
}  // namespace detail


// ---- 20 slots -------------------------------------------------------------
class DeviceData {
public:
    // ??0DeviceData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit DeviceData(void* owner) : _owner(owner) {}

    // [0] 0x180058b10 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~DeviceData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~DeviceData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~DeviceData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool EnumerateInterfaces() { detail::CompanionPureCall(); }                      // [ 1]
    virtual bool CreateDevice() { detail::CompanionPureCall(); }                             // [ 2]
    virtual void Shutdown() { detail::CompanionPureCall(); }                                 // [ 3]
    // [ 4] name unknown. Retail's one caller, Engine::Loader::Run 0x1800dbfc0,
    // passes a bool in DL: 0x1800dbfe7 MOV DL,1 before the load, 0x1800dc028
    // XOR EDX,EDX before the closing tail JMP [RAX+0x20]. dx11's body
    // (0x180001190) is a bare `ret`.
    virtual void  Slot4(bool loading) { (void)loading; detail::CompanionPureCall(); }          // [ 4] dx11: void stub
    virtual float Slot5() { detail::CompanionPureCall(); }                                   // [ 5] dx11: returns 0.0f
    virtual bool  Slot6() { detail::CompanionPureCall(); }                                   // [ 6] dx11: returns false
    virtual bool  Slot7() { detail::CompanionPureCall(); }                                   // [ 7] dx11: returns true
    // HCURSOR-shaped: (imageBuffer, hotspotX, hotspotY).
    virtual void SetImageBuffer(void* imageBuffer, int hotX, int hotY) { detail::CompanionPureCall(); }  // [ 8]
    // [ 9]..[19] — the eleven companion factories, in retail slot order. Each
    // takes the owning Video-side object and returns the new `*Data`; B.4
    // (R-VIDEO) types both, as `Video::Device`'s eleven `Create*Data` forwarders'
    // mangled names spell them (`?CreateImageBufferData@Device@Video@@QEAAPEAVImageBufferData@2@PEAVImageBuffer@2@@Z`
    // and its ten siblings). Layout-neutral: pointer parameters and returns, and
    // dx11's table is filled by address (DX11_REGISTER_SLOT), not by signature.
    virtual ContextData*        CreateContextData(Context* owner) { detail::CompanionPureCall(); }                   // [ 9] +0x48
    virtual ImageBufferData*    CreateImageBufferData(ImageBuffer* owner) { detail::CompanionPureCall(); }           // [10] +0x50
    virtual ConstantBufferData* CreateConstantBufferData(ConstantBuffer* owner) { detail::CompanionPureCall(); }     // [11] +0x58
    virtual IndexBufferData*    CreateIndexBufferData(IndexBuffer* owner) { detail::CompanionPureCall(); }           // [12] +0x60
    virtual VertexBufferData*   CreateVertexBufferData(VertexBuffer* owner) { detail::CompanionPureCall(); }         // [13] +0x68
    virtual VertexLayoutData*   CreateVertexLayoutData(VertexLayout* owner) { detail::CompanionPureCall(); }         // [14] +0x70
    virtual VertexProgramData*  CreateVertexProgramData(VertexProgram* owner) { detail::CompanionPureCall(); }       // [15] +0x78
    virtual PixelProgramData*   CreatePixelProgramData(PixelProgram* owner) { detail::CompanionPureCall(); }         // [16] +0x80
    virtual RenderStateData*    CreateRenderStateData(RenderState* owner) { detail::CompanionPureCall(); }           // [17] +0x88
    virtual VideoProgramData*   CreateVideoProgramData(VideoProgram* owner) { detail::CompanionPureCall(); }         // [18] +0x90
    virtual RenderBufferData*   CreateRenderBufferData(RenderBuffer* owner) { detail::CompanionPureCall(); }         // [19] +0x98

    ~DeviceData();          // ??1DeviceData@Video@@UEAA@XZ  0x1800f8cf0

    void* _owner;           // +0x08  Video::Device*
};

// ---- 20 slots -------------------------------------------------------------
class ContextData {
public:
    // ??0ContextData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit ContextData(void* owner) : _owner(owner) {}

    // [0] 0x180058a90 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~ContextData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~ContextData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~ContextData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool BeginCommands() { detail::CompanionPureCall(); }                            // [ 1]
    virtual void EndCommands() { detail::CompanionPureCall(); }                              // [ 2]
    virtual void Finish() { detail::CompanionPureCall(); }                                   // [ 3]
    virtual void Clear(unsigned flags, unsigned color,
                       float depth, int stencil) { detail::CompanionPureCall(); }            // [ 4]
    virtual void BindRenderBuffer() { detail::CompanionPureCall(); }                         // [ 5]
    virtual void BindViewport() { detail::CompanionPureCall(); }                             // [ 6]
    virtual void BindScissor() { detail::CompanionPureCall(); }                              // [ 7]
    virtual void BindTransforms() { detail::CompanionPureCall(); }                           // [ 8]
    virtual void BindTextureNone(int slot) { detail::CompanionPureCall(); }                  // [ 9]
    virtual void BindIndexNone() { detail::CompanionPureCall(); }                            // [10]
    virtual void BindVertexNone(int slot) { detail::CompanionPureCall(); }                   // [11]
    virtual void DrawIndexed(int primitiveType, int baseVertex, int vertexCount,
                             int startIndex, int indexCount,
                             int instanceCount) { detail::CompanionPureCall(); }             // [12]
    virtual int  GetMaxDynamicInstances(int a, int b, int c) { detail::CompanionPureCall(); } // [13]
    virtual bool BeginVertices(int vertexCount, int vertexStride) { detail::CompanionPureCall(); }        // [14]
    virtual void EndVertices(int primitiveType) { detail::CompanionPureCall(); }                          // [15]
    virtual bool BeginIndexedVertices(int vertexCount, int vertexStride,
                                      int indexCount) { detail::CompanionPureCall(); }                    // [16]
    virtual void EndIndexedVertices(int primitiveType) { detail::CompanionPureCall(); }                   // [17]
    virtual bool BeginInstancedIndexedVertices(int vertexCount, int vertexStride,
                                               int indexCount,
                                               int instanceCount) { detail::CompanionPureCall(); }        // [18]
    virtual void EndInstancedIndexedVertices(int primitiveType) { detail::CompanionPureCall(); }          // [19]

    ~ContextData();         // ??1ContextData@Video@@UEAA@XZ  0x1800f8e30

    void* _owner;           // +0x08  Video::Context*
};

// ---- 7 slots --------------------------------------------------------------
class ImageBufferData {
public:
    // ??0ImageBufferData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit ImageBufferData(void* owner) : _owner(owner) {}

    // [0] 0x180058a10 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~ImageBufferData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~ImageBufferData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~ImageBufferData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual void AttachColorBuffer() { detail::CompanionPureCall(); }                        // [1]
    virtual void AttachDepthBuffer() { detail::CompanionPureCall(); }                        // [2]
    // `levels` is a `Collection::Array<ImageLevel>`-shaped header; dx11 spells
    // it `ImageLevelArray`.
    virtual bool Create(const void* levels) { detail::CompanionPureCall(); }                 // [3]
    // ENDGAME B.3.5 block J, SIGNATURE CORRECTED. `unknownArg` was not unknown:
    // it is the `Video::Context*`. Read as DISASSEMBLY this pass from the only
    // caller, `?BindTextures@Context@Video@@` 0x1800ef330 --
    //   1800ef360  MOV RCX,[RCX+0x60]   ; img->_data, the companion
    //   1800ef364  MOV R8D,EDI          ; the slot index
    //   1800ef367  MOV RDX,RSI          ; `this`, the Context
    //   1800ef36d  CALL [RAX+0x20]      ; slot 4
    // -- so RDX carries the Context, exactly as it does on the six other Bind
    // slots. VideoDX11's body ignores it (dx11/src/imagebufferdata.cpp says so at
    // the parameter); the CALLER still sets it, and the name should say what the
    // register holds rather than that one implementation does not read it.
    virtual void Bind(void* context, unsigned slot) { detail::CompanionPureCall(); }         // [4]
    virtual bool Update(void* data, int width, int height) { detail::CompanionPureCall(); }  // [5]
    virtual bool Read(void* dst, int width, int height) { detail::CompanionPureCall(); }     // [6]

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
    // ??0IndexBufferData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit IndexBufferData(void* owner) : _owner(owner) {}

    // [0] 0x180058910 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~IndexBufferData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~IndexBufferData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~IndexBufferData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool Create(uint32_t sizeBytes, uint8_t dynamic,
                        const void* data) { detail::CompanionPureCall(); }                   // [1]
    virtual bool SetData(int32_t sizeBytes, int32_t offsetBytes,
                         const void* src) { detail::CompanionPureCall(); }                   // [2]
    // `Video::IndexBuffer::Lock`/`Unlock` (companion_data.cpp) are three-
    // instruction tail-call trampolines into these two, +0x18 and +0x20.
    virtual void* Lock(int32_t sizeBytes, int32_t* outByteOffset) { detail::CompanionPureCall(); }  // [3] +0x18
    virtual void  Unlock() { detail::CompanionPureCall(); }                                  // [4] +0x20
    // ENDGAME B.3.5 block J, SIGNATURE CORRECTED -- this slot takes the Context.
    // It was declared `Bind()` because VideoDX11's body (0x180003380) reads only
    // `this`. But the ABSTRACT slot is what the CALLER dispatches through, and
    // `?BindResources@Context@Video@@`'s index-buffer arm sets RDX, read as
    // disassembly this pass --
    //   1800ef1de  MOV RCX,[RCX+0x8]    ; ib->platformData
    //   1800ef1e2  MOV RDX,RBX          ; `this`, the Context
    //   1800ef1e8  CALL [RAX+0x28]      ; slot 5
    // -- so the argument exists and one implementation declining to read it is not
    // the slot's arity. RULE B5-14 / the caller brief: THE SLOT DECIDES, and the
    // arity comes from retail. (VertexBufferData's slot 5, below, takes four; that
    // is still the one slot where the two shapes differ.)
    virtual void  Bind(void* context) { detail::CompanionPureCall(); }                       // [5]
    virtual void  Serialize(void* serializer) { detail::CompanionPureCall(); }               // [6]

    ~IndexBufferData();     // ??1IndexBufferData@Video@@UEAA@XZ  0x1800f8df0

    void* _owner;           // +0x08  Video::IndexBuffer*
};

// ---- 7 slots --------------------------------------------------------------
class VertexBufferData {
public:
    // ??0VertexBufferData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit VertexBufferData(void* owner) : _owner(owner) {}

    // [0] 0x180058890 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~VertexBufferData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~VertexBufferData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~VertexBufferData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool Create(uint32_t sizeBytes, uint8_t dynamic,
                        const void* data) { detail::CompanionPureCall(); }                   // [1]
    virtual bool SetData(int32_t sizeBytes, int32_t offsetBytes,
                         const void* src) { detail::CompanionPureCall(); }                   // [2]
    virtual void* Lock(int32_t sizeBytes, int32_t* outByteOffset) { detail::CompanionPureCall(); }  // [3] +0x18
    virtual void  Unlock() { detail::CompanionPureCall(); }                                  // [4] +0x20
    // The one slot where the vertex and index shapes differ.
    virtual void  Bind(void* context, uint32_t startSlot, uint32_t offsetBytes,
                       uint32_t strideBytes) { detail::CompanionPureCall(); }                // [5]
    virtual void  Serialize(void* serializer) { detail::CompanionPureCall(); }               // [6]

    ~VertexBufferData();    // ??1VertexBufferData@Video@@UEAA@XZ  0x1800f8d50

    void* _owner;           // +0x08  Video::VertexBuffer*
};

// ---- 4 slots --------------------------------------------------------------
class ConstantBufferData {
public:
    // ??0ConstantBufferData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit ConstantBufferData(void* owner) : _owner(owner) {}

    // [0] 0x180058990 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~ConstantBufferData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~ConstantBufferData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~ConstantBufferData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool Create() { detail::CompanionPureCall(); }                                   // [1]
    // `Video::ConstantBuffer::Bind` (0x1800eef70) tail-jumps into this one
    // through +0x10, off the owner's +0x40.
    // ENDGAME B.3.5 block J, SIGNATURE CORRECTED IN TWO PLACES. `unused` is the
    // `Video::Context*`, and `unused2` is NOT A POINTER -- it is a 32-bit int.
    // Two independent proofs:
    //   * retail's mangling of the owner-side trampoline,
    //     `?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z`
    //     = (Context&, void*, int, int), which tail-jumps straight into this slot;
    //   * the five call sites in `?BindResources@Context@Video@@`, read as
    //     disassembly this pass (0x1800ef249, ef274, ef29f, ef2ca, ef2f5), each of
    //     which is
    //       MOV  RCX,[RBX+<buf>+0x40]   ; the companion
    //       LEA  R8,[RBX+<staging>]     ; the payload
    //       XOR  R9D,R9D                ; <-- a DWORD zero, i.e. an int 0
    //       MOV  RDX,RBX                ; `this`, the Context
    //       MOV  dword [RSP+0x20],<len>
    //       CALL [RAX+0x10]             ; slot 2
    // A `void*` fourth parameter would have been `XOR R9,R9`. Declaring it as one
    // is the kind of quiet type error a caller conversion has to pass an ugly cast
    // through, which is how it was found. VideoDX11 reads neither argument.
    virtual void Bind(void* context, const void* srcData, int32_t first,
                      int32_t sizeBytes) { detail::CompanionPureCall(); }                    // [2] +0x10
    virtual int32_t SlotIndexPlusOne() { detail::CompanionPureCall(); }                      // [3]

    ~ConstantBufferData();  // ??1ConstantBufferData@Video@@UEAA@XZ  0x1800f8e50

    void* _owner;           // +0x08  Video::ConstantBuffer*
};

// ---- 4 slots --------------------------------------------------------------
class RenderBufferData {
public:
    // ??0RenderBufferData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit RenderBufferData(void* owner) : _owner(owner) {}

    // [0] 0x180058590 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~RenderBufferData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~RenderBufferData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~RenderBufferData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool Slot1() { detail::CompanionPureCall(); }                                    // [1] dx11: returns true
    virtual void Bind(void* context) { detail::CompanionPureCall(); }                        // [2]
    virtual void Resolve(void* context, void* other, bool flag) { detail::CompanionPureCall(); }  // [3]

    ~RenderBufferData();    // ??1RenderBufferData@Video@@UEAA@XZ  0x1800f8db0

    void* _owner;           // +0x08  Video::RenderBuffer*
};

// ---- 3 slots --------------------------------------------------------------
class VertexLayoutData {
public:
    // ??0VertexLayoutData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit VertexLayoutData(void* owner) : _owner(owner) {}

    // [0] 0x180058810 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~VertexLayoutData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~VertexLayoutData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~VertexLayoutData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual void Bind(void* context) { detail::CompanionPureCall(); }                        // [1]
    // void, unlike VertexProgramData's and PixelProgramData's bool — VideoDX11's
    // 0x180003ff0 falls off its end with no return value set.
    virtual void PlatformCreate() { detail::CompanionPureCall(); }                           // [2] +0x10

    ~VertexLayoutData();    // ??1VertexLayoutData@Video@@UEAA@XZ  0x1800f8d30

    void* _owner;           // +0x08  Video::VertexLayout*
};

// ---- 3 slots --------------------------------------------------------------
class VertexProgramData {
public:
    // ??0VertexProgramData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit VertexProgramData(void* owner) : _owner(owner) {}

    // [0] 0x180058790 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~VertexProgramData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~VertexProgramData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~VertexProgramData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool PlatformCreate() { detail::CompanionPureCall(); }                           // [1]
    virtual void Bind(void* context, int permutation) { detail::CompanionPureCall(); }        // [2]

    ~VertexProgramData();   // ??1VertexProgramData@Video@@UEAA@XZ  0x1800f8d10

    void* _owner;           // +0x08  Video::VertexProgram*
};

// ---- 3 slots --------------------------------------------------------------
class PixelProgramData {
public:
    // ??0PixelProgramData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit PixelProgramData(void* owner) : _owner(owner) {}

    // [0] 0x180058710 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~PixelProgramData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~PixelProgramData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~PixelProgramData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool PlatformCreate() { detail::CompanionPureCall(); }                           // [1]
    virtual void Bind(void* context, int permutation) { detail::CompanionPureCall(); }        // [2]

    ~PixelProgramData();    // ??1PixelProgramData@Video@@UEAA@XZ  0x1800f8dd0

    void* _owner;           // +0x08  Video::PixelProgram*
};

// ---- 3 slots --------------------------------------------------------------
class RenderStateData {
public:
    // ??0RenderStateData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit RenderStateData(void* owner) : _owner(owner) {}

    // [0] 0x180058690 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~RenderStateData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~RenderStateData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~RenderStateData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool Create() { detail::CompanionPureCall(); }                                   // [1]
    virtual void Bind(void* context) { detail::CompanionPureCall(); }                        // [2]

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
    // ??0VideoProgramData(owner) -- the REAL constructor: the owner at +0x08 (retail
    // stores it first, then the vptr; nothing can observe the order).
    explicit VideoProgramData(void* owner) : _owner(owner) {}

    // [0] 0x180058610 -- MSVC's VECTOR deleting destructor (element 0x10, over
    // ~VideoProgramData), both arms as the retired RT_DELDTOR row ran them. Reached only on
    // an object wearing THIS table; a live companion wears dx11's.
    virtual void* DeletingDtor(unsigned flags) {
        if (flags & 2u) {
            void* const block = this;   // element 0 of a vector-new block
            int32_t* const cookie = static_cast<int32_t*>(block) - 2;  // CAST-OK: dx11_seam
            for (int32_t i = *cookie; --i >= 0;)
                this[i].~VideoProgramData();
            if (flags & 1u) Object::detail::InternalMemFree(cookie);
            return cookie;
        }
        this->~VideoProgramData();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    virtual bool Slot1() { detail::CompanionPureCall(); }                                    // [1] dx11: returns true
    // ENDGAME B.3.5 block J, SLOT NAMED AND SIGNATURE CORRECTED. It was `Slot2()`
    // because VideoDX11 implements it as a bare `RET` (0x180001190), so the label
    // block A copied from the seam said nothing. The CALLER says what it is:
    // `?BindResources@Context@Video@@`'s video-program arm, read as disassembly
    // this pass --
    //   1800ef0dd  MOV RAX,[RBX+0x200]  ; the Video::VideoProgram
    //   1800ef0e4  MOV R8D,[RBX+0xc]    ; the Context's shader-detail level
    //   1800ef0e8  MOV RDX,RBX          ; `this`, the Context
    //   1800ef0eb  MOV RCX,[RAX+0x40]   ; the companion -- +0x40, NOT the
    //                                   ; +0x50 a Vertex/PixelProgram uses
    //   1800ef0f2  CALL [RAX+0x10]      ; slot 2
    // -- which is the SAME three-argument shape as `VertexProgramData::Bind` and
    // `PixelProgramData::Bind` at the same slot index. So slot 2 is `Bind`, and
    // this backend simply implements no video-program binding (dx11/src/
    // smallclasses.cpp: "VideoProgramData really is inert").
    virtual void Bind(void* context, int detail) { detail::CompanionPureCall(); }             // [2] dx11: inert stub

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

// ENDGAME B3X (the normal-virtual cutover), B3X_RULES X1: the twelve `*Data`
// classes are EXEMPT BY DECISION. Retail's slot 0 IS a deleting destructor, but
// every live object wears dx11's hand table built in RETAIL slot order
// (dx11_seam.cpp), so these stay on `DeletingDtor(unsigned)` and must never
// gain a virtual destructor (GCC would spend two slots on it and shift every
// slot the seam fills). They flip when dx11's types become real subclasses.
static_assert(!std::has_virtual_destructor<DeviceData>::value,
              "Video::DeviceData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<ContextData>::value,
              "Video::ContextData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<ImageBufferData>::value,
              "Video::ImageBufferData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<IndexBufferData>::value,
              "Video::IndexBufferData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<VertexBufferData>::value,
              "Video::VertexBufferData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<ConstantBufferData>::value,
              "Video::ConstantBufferData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<RenderBufferData>::value,
              "Video::RenderBufferData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<VertexLayoutData>::value,
              "Video::VertexLayoutData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<VertexProgramData>::value,
              "Video::VertexProgramData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<PixelProgramData>::value,
              "Video::PixelProgramData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<RenderStateData>::value,
              "Video::RenderStateData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");
static_assert(!std::has_virtual_destructor<VideoProgramData>::value,
              "Video::VideoProgramData is exempt from the normal-virtual cutover -- the live "
              "object wears dx11's table built in retail slot order (B3X_RULES X1)");

}  // namespace Video

#pragma GCC diagnostic pop
