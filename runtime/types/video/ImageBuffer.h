#pragma once
// ============================================================================
// runtime/types/video/ImageBuffer.h — `Video::ImageBuffer` as a real C++ class,
// plus the `RtVideo::ImageBuffer` POD its unconverted callers still use.
//
// ENDGAME B.3.5 block C. RULES B5-2.1 (this header is what `dx11/`'s deleted
// `ImageBufferOwner` view now points at), B5-15 (the two type-name literals are
// COPIED from B35_SLOTS, not derived from the C++ base name), B5-16 (`Construct`
// is introduced by this branch at slot 8).
// ============================================================================
//
// Runtime-x64.dll class, 0x68 bytes, `??_7ImageBuffer@Video@@6B@` @ 0x180154fc0,
// TEN slots (B35_SLOTS.md), `sizeof` confirmed by the `MOV EDX,0x68` in the
// slot-0 vector deleting destructor at 0x18001ed20.
//
// Inheritance: Object::Base -> Object::External -> Object::CompiledExternal ->
// Video::ImageBuffer. `?_ParentTypeName@ImageBuffer@Video@@` (0x18001e8d0)
// returns "Object::CompiledExternal", read byte-wise.
//
// Decompiled from (Wave 6 lane C):
//   Ctor:            0x1800f2580 (??0ImageBuffer@Video@@QEAA@XZ)
//   Copy ctor:       0x18001eb80 (??0ImageBuffer@Video@@QEAA@AEBV01@@Z)
//   Dtor:            0x1800f2610 (??1ImageBuffer@Video@@UEAA@XZ)
//   Width:           0x1800f2530 (?Width@ImageBuffer@Video@@QEBAHXZ)
//   Height:          0x1800f24e0 (?Height@ImageBuffer@Video@@QEBAHXZ)
//   GetLevelCount:   0x1800f2420 (?GetLevelCount@ImageBuffer@Video@@QEBAHXZ)
//   IsCompressed:    0x1800f2480 (?IsCompressed@ImageBuffer@Video@@QEBA_NXZ)
//   IsDepthBuffer:   0x1800f24c0 (?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ)
//   IsDynamic:       0x1800f24d0 (?IsDynamic@ImageBuffer@Video@@QEBA_NXZ)
//   IsGenerated:     0x1800f24a0 (?IsGenerated@ImageBuffer@Video@@QEBA_NXZ)
//   IsRenderBuffer:  0x1800f2460 (?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ)
//   IsRendered:      0x1800f2490 (?IsRendered@ImageBuffer@Video@@QEBA_NXZ)
//   CopyToTexture:   0x1800f3380 (?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)
//   CopyFromTexture: 0x1800f33a0 (?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)
//
// STALE-COMMENT CORRECTION (Wave 6 lane C): the +0x28/+0x2C fields were
// previously "unknown". The disassembly of the ctor (0x1800f25d8-0x1800f25db,
// one dword store then one qword store covering both) and the Is*() family
// (all of which read *(int*)(this+0x2c)) show they are `dimension` (2D vs
// cube) and `kind` (an enum selecting bind flags: 0/4/5 = render target,
// 1 = depth, 2 = default/none, 3 = dynamic). This matches
// dx11/src/imagebufferdata.cpp's independently-recovered field map exactly
// (that file reads the same offsets from the DX11 side). +0x38/+0x3C/+0x40
// were also "unknown"; dx11/src/imagebufferdata.cpp's sampler code (and the
// ctor's default value 0) identify them as the address-mode triple
// (U/V/W), and +0x44 (init value 1) as the filter index.
//
// Width()/Height() were PREVIOUSLY WRONG as inlines: the old `return width;` /
// `return height;` only reproduced the common case. The real accessors
// (0x1800f2530 / 0x1800f24e0) special-case two flag bits that make the
// buffer track the live device resolution (bit 8 = full device size, bit 9 =
// half device size) and a third (bit 6) that right-shifts the stored value
// by a device-configured downsample amount.
// ============================================================================

#include <cstdint>
#include <cstddef>

// ENDGAME B.3.5 block A — `Video::ImageBufferData` and its POD view live here
// now, with the other eleven companion bases (RULE B5-1 / B4b-1).
#include "CompanionData.h"
// `Object::CompiledExternal`, the proved parent of the real class below.
#include "../object/External.h"
#include "../object/Internal.h"   // ENDGAME B.3.8 RB: InternalMemFree (the concrete slot 0)

// `runtime/src/video/`'s door helper over `Video::ImageBuffer`'s protected
// surface. Four of its members are PRIVATE in retail (`AEAA` — the three
// platform halves and `PlatformSerialize`) and `~ImageBuffer` is non-virtual and
// protected, yet all five are exported by name and keep registered `extern "C"`
// doors, which cannot name a protected member; `Serialize` also has to call
// `PlatformSerialize`. B34_BRIEFS A.4's answer is to FRIEND the holder rather
// than widen the access, so the header keeps saying what retail says. A DERIVED
// helper class would not work: deriving forces GCC to reference
// `vtable for Video::ImageBuffer`, whose slot 0 (`DeletingDtor`) is a table row
// in `runtime/src/object/deleting_dtor.cpp` and is defined in no translation
// unit, so the link fails (seen on `Video::RenderState`, not predicted).
struct ImageBufferDoorAccess;

// `Collection::Array<T>` (three template parameters, two defaulted) and
// `System::Filename` both arrive with `../object/External.h` above, which
// includes them; they are NOT forward-declared here, because a hand-written
// `template <class T> class Array;` would be a different template.
namespace IO { class Serializer; }

// ############################################################################
// THE POD, MOVED INTO `RtVideo` BY B4b-1.
// ############################################################################
//
// The retail name now belongs to the real class `Video::ImageBuffer` below, so
// the POD moves into the `Rt*` namespace — and it STAYS, because it still has
// five unconverted users, none of them this block's:
//
//   runtime/src/video/buffers.cpp          (13 members; block D2)
//   runtime/src/video/dynamic_resources.cpp (2;          block D2)
//   runtime/src/video/video_resources.cpp   (1;          block D2)
//   runtime/src/save/saveload_draw.cpp / saveload_saveinfo.cpp (the owner's)
//   runtime/src/graphics/wave23_lane_gfxbody.cpp, through the two
//     `RtGraphics::*ImageBuffer` PODs that embed it (block H1)
//
// A global-scope `using` is left behind so not one of those files has to move
// in this block; they all spell it `ImageBuffer` with no namespace, which is
// the fourth namespace spelling B35_FILEMAP (d) records for this header.
// ############################################################################

namespace RtVideo {

// ============================================================
// RtVideo::ImageBuffer (0x68 bytes) — the POD view
// ============================================================
struct ImageBuffer {
    // --- CompiledExternal base (0x20 bytes) ---
    void*       vtable;             // +0x00  vtable pointer (set through Base->External->CompiledExternal->ImageBuffer chain)
    uint64_t    externalData08;     // +0x08  External base field (zeroed in ctor) — External::_path
    uint64_t    externalData10;     // +0x10  External base field (zeroed in ctor) — External::_refs
    uint64_t    externalData18;     // +0x18  External base field (zeroed in ctor)

    // --- ImageBuffer subclass fields ---
    int32_t     width;              // +0x20  image width in pixels (raw field; use Width() for the accessor)
    int32_t     height;             // +0x24  image height in pixels (raw field; use Height() for the accessor)
    int32_t     dimension;          // +0x28  0 = 2D, non-zero (1) = cube map (zeroed in ctor)
    int32_t     kind;               // +0x2C  bind-flag selector: 0/4/5 render target, 1 depth, 2 default/none, 3 dynamic (init 2 in ctor)
    uint32_t    format;             // +0x30  pixel format: index into an 8-entry DXGI table, < 3 = block-compressed
    uint32_t    flags;              // +0x34  bit0 mipmapped, bit4/5 device-reset-tracked (set by DX11 color/depth attach),
                                     //        bit6 comparison sampler + downsample-shift enable, bit8 = device width/height,
                                     //        bit9 = half device width/height (bits 8/9 read only by Width()/Height())
    int32_t     addressU;           // +0x38  address mode index (0=WRAP, 1=CLAMP), zeroed in ctor
    int32_t     addressV;           // +0x3C  address mode index, zeroed in ctor
    int32_t     addressW;           // +0x40  address mode index, zeroed in ctor
    int32_t     filter;             // +0x44  filter mode index (0=POINT,1=LINEAR,2=ANISOTROPIC), init 1 in ctor
    // +0x48 — NAME CORRECTION, Wave 18 lane VDATA. This field is a
    // **System::String**, not a pixel buffer. `?Serialize@ImageBuffer@Video@@`
    // (0x1800f280d) hands `this+0x48` straight to
    // `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z`, which only
    // accepts a String. The two descriptions cover the same eight bytes — a
    // System::String IS one pointer to a refcounted payload whose count sits at
    // -4, which is why the destructor in buffers.cpp is correct as written and
    // needs no change — but the NAME sends a reader looking for pixels in what
    // is really the source path/name. Left spelled `pixelData` HERE so no other
    // block's file has to move; the real class below calls it `_sourceName`.
    void*       pixelData;          // +0x48  System::String (refcount at ptr-4, frees at 0)
    uint64_t    field50;            // +0x50  unknown (copied in copy ctor; not touched by any Lane C export)
    uint64_t    field58;            // +0x58  unknown (copied in copy ctor; not touched by any Lane C export)
    void*       platformData;       // +0x60  Video::ImageBufferData* (platform-specific, set by PlatformConstruct)

    // --- Inline accessors matching the real (non-trivial) originals ---
    bool IsCompressed()   const { return format < 3; }
    bool HasMipMaps()     const { return (flags & 1u) != 0; }
    bool IsDepthBuffer()  const { return kind == 1; }
    bool IsDynamic()      const { return kind == 3; }
    bool IsGenerated()    const { return kind == 4; }
    bool IsRendered()     const { return kind == 5; }
    // kind & ~0b101 == 0  <=>  kind in {0,1,4,5}
    bool IsRenderBuffer() const { return (static_cast<uint32_t>(kind) & 0xfffffffau) == 0; }

    int GetLevelCount() const {
        int count = 1;
        if (HasMipMaps()) {
            int w = width;
            int h = height;
            while (w > 1 || h > 1) {
                h >>= 1; if (h == 0) h = 1;
                w >>= 1; if (w == 0) w = 1;
                count++;
            }
        }
        return count;
    }
};

static_assert(sizeof(ImageBuffer) == 0x68, "ImageBuffer must be 0x68 bytes");
static_assert(offsetof(ImageBuffer, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(ImageBuffer, externalData08) == 0x08, "externalData08 at +0x08");
static_assert(offsetof(ImageBuffer, externalData10) == 0x10, "externalData10 at +0x10");
static_assert(offsetof(ImageBuffer, externalData18) == 0x18, "externalData18 at +0x18");
static_assert(offsetof(ImageBuffer, width) == 0x20, "width at +0x20");
static_assert(offsetof(ImageBuffer, height) == 0x24, "height at +0x24");
static_assert(offsetof(ImageBuffer, dimension) == 0x28, "dimension at +0x28");
static_assert(offsetof(ImageBuffer, kind) == 0x2C, "kind at +0x2C");
static_assert(offsetof(ImageBuffer, format) == 0x30, "format at +0x30");
static_assert(offsetof(ImageBuffer, flags) == 0x34, "flags at +0x34");
static_assert(offsetof(ImageBuffer, addressU) == 0x38, "addressU at +0x38");
static_assert(offsetof(ImageBuffer, addressV) == 0x3C, "addressV at +0x3C");
static_assert(offsetof(ImageBuffer, addressW) == 0x40, "addressW at +0x40");
static_assert(offsetof(ImageBuffer, filter) == 0x44, "filter at +0x44");
static_assert(offsetof(ImageBuffer, pixelData) == 0x48, "pixelData at +0x48");
static_assert(offsetof(ImageBuffer, field50) == 0x50, "field50 at +0x50");
static_assert(offsetof(ImageBuffer, field58) == 0x58, "field58 at +0x58");
static_assert(offsetof(ImageBuffer, platformData) == 0x60, "platformData at +0x60");

}  // namespace RtVideo

// The global spelling the five unconverted consumers use. See the banner above.
using RtVideo::ImageBuffer;
// And `Video::ImageBufferData`'s POD, whose global alias ENDGAME B.3.5 block A
// left here when it moved the declaration into `CompanionData.h`:
// `runtime/src/video/buffers.cpp` writes `pd->vtable` by hand in three
// trampolines and in the ported `??1ImageBufferData` door. Block D2's when
// `buffers.cpp` converts.
using RtVideo::ImageBufferData;

// ############################################################################
// THE REAL CLASS.
// ############################################################################

namespace Video {

// ============================================================
// Video::ImageBuffer — one image/texture resource. 0x68 bytes,
// `Object::CompiledExternal` base (which adds no bytes over `External`'s 0x20).
//
// ENDGAME B.3.9 VS: every live `ImageBuffer` is built by the real public
// constructor (`::new (p) Video::ImageBuffer()` at every builder, Runtime and
// exe, and by the class's own `CreateNewObject` / `CreateInPlaceObject`) and
// wears the compiler's table. The `??0` / `??1` doors, the FACTREST row, the
// `RT_DELDTOR` row and the minted `??_7ImageBuffer@Video@@6B@` are retired.
//
// NO VIRTUAL DESTRUCTOR (ENDGAME B.1 decision 2): slot 0 is
// `DeletingDtor(unsigned)`. `~ImageBuffer()` is non-virtual and has no slot.
// ============================================================
class ImageBuffer : public Object::CompiledExternal {
public:
    // The five 4-byte enums `?Create@` names, all NESTED in this class
    // (`W4ImageType@12@`, `W4Usage@12@`, `W4Format@12@`, `W4FilterMode@12@`,
    // `W4AddressMode@12@`). THE ENUMERATOR NAMES ARE NOT RECOVERABLE from the
    // binary — only the values the ported bodies and the DX11 backend read — so,
    // exactly as `Object::External::CompileResult` and
    // `Video::VertexLayout::Usage` do for the same reason, they are spelled as
    // the 32-bit integers they are rather than as enums with invented names.
    //
    // What IS known, from the bodies:
    //   ImageType    +0x28 `_dimension`: 0 = 2D, 1 = cube
    //   Usage        +0x2c `_kind`: 1 = depth, 2 = default, 3 = dynamic,
    //                4 = generated, 5 = rendered, and {0,1,4,5} = render buffer
    //   Format       +0x30, an index into an 8-entry DXGI table; < 3 is
    //                block-compressed
    //   FilterMode   +0x44: 0 = POINT, 1 = LINEAR, 2 = ANISOTROPIC
    //   AddressMode  +0x38/+0x3c/+0x40: 0 = WRAP, 1 = CLAMP
    using ImageType   = int32_t;
    using Usage       = int32_t;
    using Format      = int32_t;
    using FilterMode  = int32_t;
    using AddressMode = int32_t;

    // The `Collection::Array<Collection::Array<uint8_t>>` of per-mip-level pixel
    // blobs that `Create` and `PlatformCreate` take
    // (`PEAV?$Array@V?$Array@E@Collection@@@Collection@@` in the mangling).
    using LevelArray = Collection::Array<Collection::Array<uint8_t> >;

    // ---- the ten slots, in measured order -------------------------------
    // Slot 4 is `Object::External::Compile`, INHERITED (retail's slot 4 is
    // 0x1800066e0, External's own body), so it is not redeclared here.
    //
    // SLOT 0 IS CONCRETE IN RETAIL (0x18001ed20) AND IS DECLARED `= 0` HERE,
    // which is B.3.4's settled shape (B34_BRIEFS A.4) and the same call
    // `CompanionData.h` makes for the twelve `*Data` bases: our slot 0 is a table
    // ROW in `runtime/src/object/deleting_dtor.cpp`, registered by address, and
    // is defined in no translation unit. Leaving it non-pure makes GCC demand
    // `vtable for Video::ImageBuffer` from whichever translation unit defines a
    // constructor or destructor of the class, which then fails to link. Pure
    // makes every entry known (`__cxa_pure_virtual`) and the vtable emittable —
    // and no live object ever wears it, because the ctor door stamps the minted
    // address. The class is therefore ABSTRACT in C++ while being "concrete" in
    // B35_RULES 2.2's sense (retail instantiates it); the two statements are
    // about different things.
    //
    // ENDGAME B.3.8 block RB (B38_RULES D1/D2): the exe instantiates the class
    // ITSELF (its table copy 0x14015c8c0; ModDialog, NoiseUI, TradeWork), so
    // slot 0 is now CONCRETE — the exe copy's own slot 0 0x1400129d0, the
    // scalar deleting dtor (`~ImageBuffer`; MemFree on flags & 1). Inline, so
    // the key function (TypeName) and the vtable's TU do not move. ENDGAME
    // B.3.9 VS: the mint's RT_DELDTOR row (the DLL table's VECTOR slot 0
    // 0x18001ed20) is retired; every object wears this table, and the two
    // arms differ only on flags & 2, which nothing passes (B39_RULES D1.2).
    Object::Base* DeletingDtor(unsigned flags) override {             // [0]
        this->~ImageBuffer();
        if (flags & 1) Object::detail::InternalMemFree(this);
        return this;
    }
    // [1] 0x18001e900 TypeName / [2] 0x18001e8e0 Type, TypeId 0x18001e8f0, `_TypeName` / `_ParentTypeName` and the factory pair:
    // retail's macro (ENDGAME B.3.9 TI-REST, B39_RULES D6); the record is
    // `DEFINE_TYPE`d in runtime/src/video/video_type_records.cpp.
    DECLARE_TYPE(ImageBuffer)
public:
    int32_t           GetFileVersion() const override;        // [3] 0x1800f26e0 — `MOV EAX,7`
    bool              Serialize(void* stream) override;       // [5] 0x1800f26f0
    Object::External* Reconstruct() override;                 // [6] 0x18001ea80
    void              Destruct() override;                    // [7] 0x18001eab0
    // [8] 0x18001eaa0. `virtual`, NOT `override`: `Object::External`'s and
    // `CompiledExternal`'s retail tables are eight entries, so this branch
    // introduces `Construct` itself (RULE B5-16).
    virtual Object::External* Construct();
    // [9] 0x1800f2680. Introduced by this branch too. `Graphics::
    // GeneratedImageBuffer` INHERITS it, `Graphics::FrameCopyImageBuffer`
    // OVERRIDES it, and `Graphics::ReflectionImageBuffer` inherits it again
    // (B35_RULES 2.3 item 5, read from the three slot lists).
    //
    // NO LONGER PURE — ENDGAME B.3.5 block VC (the fragment pass). Block C wrote
    // `= 0` here for one reason only: the body was the registered door
    // `Video_ImageBuffer_Create` in `runtime/src/video/buffers.cpp` and was
    // defined in no translation unit AS A MEMBER, so a non-pure declaration put
    // an undefined symbol in this class's emitted vtable and the exe did not
    // link. `buffers.cpp` has now moved the body onto this member, so the symbol
    // exists and the `= 0` comes off exactly as that banner said it would.
    //
    // SLOT 0 STAYS PURE and so the class stays ABSTRACT in C++: its body is a
    // table row in `runtime/src/object/deleting_dtor.cpp`, which B35_RULES 11
    // keeps for the whole of step 5. Nothing is therefore instantiable by
    // accident, and no live object wears GCC's table — the ctor door stamps the
    // minted `??_7ImageBuffer@Video@@6B@`.
    virtual bool Create(ImageType type, Usage usage, Format format, uint32_t flags,
                        int32_t width, int32_t height, FilterMode filter,
                        AddressMode addressU, AddressMode addressV,
                        AddressMode addressW, LevelArray* levels);

    // ---- the exported non-virtual members --------------------------------
    // Every `?…@ImageBuffer@Video@@` in the shipped export table is declared
    // here, ported or not, so no later block has to edit this header
    // (BLOCK_BRIEF "the shape").
    //
    // ENDGAME B.3.5 block VC (the fragment pass) MOVED THE PORTED BODIES ONTO
    // THESE MEMBERS. Block C had to leave them declared-undefined because the
    // bodies lived in `runtime/src/video/buffers.cpp`, `dynamic_resources.cpp`
    // and `video_resources.cpp` — block D2's file set. Those three files are
    // converted now and the doors are one-line QUALIFIED delegations. The
    // members marked `not ported` below still have no body anywhere in the tree
    // and nothing calls them; that is unchanged.
    int32_t Width() const;                 // 0x1800f2530 — buffers.cpp; live device size for bits 8/9
    int32_t Height() const;                // 0x1800f24e0 — buffers.cpp; ditto
    int32_t GetLevelCount() const;         // 0x1800f2420 — buffers.cpp
    // ========================================================================
    // ADDRESS CORRECTION, ENDGAME B.3.5 block VC — 23 of the addresses in this
    // section and below were WRONG, and the instrument that found them is the
    // shipped export directory itself (`tools/pe_exports.py` over
    // `C:/GOG Games/Banished/Runtime-x64.dll`, every `*ImageBuffer@Video@@`
    // symbol dumped with its RVA and re-based to 0x180000000). Every address
    // that was wrong sat in the 0x1800f23e0..0x1800f2570 band or the
    // 0x1800f2ba0..0x1800f3100 band, and NOT ONE address in either band is an
    // ImageBuffer export at all — so the old comments named nothing. The eleven
    // that were right are exactly the eleven that a registration in
    // `buffers.cpp` / `dynamic_resources.cpp` / `video_resources.cpp` also
    // spells, i.e. the ones a pefile check had already been run against
    // (`reference_resolveexport_phantom_names.md` is the rule those three files
    // follow and this header did not).
    //
    // NOTHING BEHAVIOURAL CHANGED with the fix: every one of the corrected rows
    // is an unported member, so no door and no registration named the bad
    // address. It is a documentation defect, and the reason it is worth this
    // banner is that the next block to port one of these would have registered
    // `RegisterRuntimeAddr` on a number out of a comment.
    // ========================================================================
    Format  GetFormat() const;             // 0x18001eb50 — `?AW4`, an ENUM by value: EAX, NOT a retslot
    bool    HasMipMaps() const;            // 0x18001eac0 — buffers.cpp (GetLevelCount's gate)
    bool    Is2D() const;                  // 0x18001eb30 — not ported
    bool    IsCube() const;                // 0x18001eb40 — not ported
    bool    IsCompressed() const;          // 0x1800f2480 — buffers.cpp
    bool    IsColorBuffer() const;         // 0x1800f24b0 — not ported
    bool    IsDepthBuffer() const;         // 0x1800f24c0 — buffers.cpp
    bool    IsDynamic() const;             // 0x1800f24d0 — buffers.cpp
    bool    IsGenerated() const;           // 0x1800f24a0 — buffers.cpp
    bool    IsRenderBuffer() const;        // 0x1800f2460 — buffers.cpp
    bool    IsRendered() const;            // 0x1800f2490 — buffers.cpp
    bool    IsShadowMap() const;           // 0x18001eb20 — not ported
    bool    IsAliasedColor() const;        // 0x18001eb00 — not ported
    bool    IsAliasedDepth() const;        // 0x18001eb10 — not ported
    bool    CanDownsample() const;         // 0x18001ead0 — not ported
    bool    ClearEdgeAlpha() const;        // 0x18001eae0 — not ported
    bool    ClearEdgeColor() const;        // 0x18001eaf0 — not ported
    void    AliasDefaultColorBuffer();     // 0x1800f3470 — not ported
    void    AliasDefaultDepthBuffer();     // 0x1800f3340 — not ported
    void    Bind(void* context, int32_t slot) const;   // 0x1800f3360 — not ported
    bool    CopyToTexture(void* dst, int32_t width, int32_t height);   // 0x1800f3380 — buffers.cpp
    bool    CopyFromTexture(void* src, int32_t width, int32_t height); // 0x1800f33a0 — buffers.cpp
    void    Recreate();                    // 0x1800f33f0 — video_resources.cpp
    void    Release();                     // 0x1800f33c0 — not ported
    // `?GetData@ImageBuffer@Video@@` IS NOT IN THE SHIPPED EXPORT TABLE, under
    // either mangling, and the two addresses previously claimed here
    // (0x1800f2560 / 0x1800f2570) are not exports either. The pair is kept
    // DECLARED because `dx11/` reads `_data` at +0x60 and a later block may want
    // the accessor, but the "exported member" claim is withdrawn: they are
    // either inlined everywhere or they do not exist.
    ImageBufferData*       GetData();      // not exported — the non-const overload
    const ImageBufferData* GetData() const;// not exported — `QEBA`, the const one

    // ?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z
    //   0x1800f2840. STATIC — it takes no `this`; the body is
    //   `runtime/src/video/save_image_to_file.cpp`.
    static bool SaveImageToFile(const System::Filename& path, int32_t width,
                                int32_t height, uint32_t* pixels);
    // ?ReleaseDynamic@ / ?RecreateDynamic@ — STATIC, they walk the
    // `?_firstSurface@ImageBuffer@Video@@` intrusive list. Bodies in
    // `dynamic_resources.cpp`, moved onto these members by block VC.
    // Addresses corrected — see the banner above; `dynamic_resources.cpp` had
    // the right pair all along (ordinals 3537 / 3523).
    static void ReleaseDynamic();          // 0x1800f2e80
    static void RecreateDynamic();         // 0x1800f2d90

    // ?_firstSurface@ImageBuffer@Video@@0PEAV12@EA (0x1801fe780) — the head of
    // the live-surface list PlatformConstruct / PlatformDestruct link into and the
    // two sweeps walk (device-lost recovery). The class static since ENDGAME
    // B.3.9 VID (B39_RULES D7), defined in dynamic_resources.cpp; private in
    // retail, PUBLIC here because the walkers are free functions.
    static ImageBuffer* _firstSurface;

    // `??0ImageBuffer@Video@@QEAA@AEBV01@@Z` (0x18001eb80) and
    // `??4ImageBuffer@Video@@QEAAAEAV01@AEBV01@@Z` (0x18001ec50) are REAL,
    // EXPORTED bodies, not compiler defaults — the situation RULE B5-10 records
    // for `Graphics::Geometry`. Declared, and deliberately left UNDEFINED: a
    // `= default` here would silently substitute a bitwise copy (which would
    // alias `_data`, a singly-owned `ImageBufferData*`, and share `_sourceName`
    // without bumping its refcount) for whatever those two do.
    ImageBuffer(const ImageBuffer& other);              // 0x18001eb80 — not ported
    ImageBuffer& operator=(const ImageBuffer& other);   // 0x18001ec30 — not ported
                                                        //   (was 0x18001ec50; the
                                                        //   export table says ec30)

    // The static reflection companions retail emits are DECLARE_TYPE's (the
    // factory pair 0x18001e9f0 / 0x18001ea70 is in buffers.cpp).
    // ??2ImageBuffer@Video@@SAPEAX_K@Z 0x18001e910 — the body moved onto this
    // member by block VC; the door is `runtime/src/video/context_ctor.cpp`'s
    // `RtVideo_ImageBuffer_OperatorNew`, which keeps its name and both rows.
    static void*         operator new(size_t size);
    // ??2ImageBuffer@Video@@SAPEAX_KPEAX@Z 0x18001e980 — not ported.
    static void*         operator new(size_t size, void* storage);
    // ??3ImageBuffer@Video@@SAXPEAX@Z 0x18001e990 — NOT PORTED, and the previous
    // attribution to `closure_batch2b.cpp` was wrong: that file holds
    // `Video::ShaderProgram`'s blob-array helper and a `Volume::Frustum` leaf,
    // and no `??3ImageBuffer@Video@@` body or registration exists anywhere in
    // the tree. (0x180006a10 — the address `context_lifecycle.cpp` registers —
    // is NOT this: it is the shared unexported guarded free that IO, Save,
    // Input, Object and Engine all reach by address.)
    static void          operator delete(void* p);

    // ---- PUBLIC BY NECESSITY, ENDGAME B.3.5 block VC ----------------------
    // `~ImageBuffer` is non-virtual and PROTECTED in retail, and the two platform
    // halves are PRIVATE (`AEAA`) — yet all three are exported by name and keep
    // registered `extern "C"`-style doors in `runtime/src/video/buffers.cpp`, and
    // a door is a free function that cannot name a protected member. They are
    // therefore public, exactly as `Video::VertexLayout` made the same three
    // public in b35-B and for the same reason. The CONSTRUCTOR stays protected.
    //
    void PlatformConstruct();                  // 0x1800f2f40 (was 0x1800f2ba0) — buffers.cpp
    void PlatformDestruct();                   // 0x1800f3020 (was 0x1800f2c40) — buffers.cpp

    // ??0ImageBuffer@Video@@QEAA@XZ 0x1800f2580 — the REAL constructor
    // (ENDGAME B.3.8 block RB), PUBLIC: the class is concrete. CompiledExternal()
    // and the stamp are the compiler's; the field stores and `PlatformConstruct`
    // are `InitFields`. ENDGAME B.3.9 VS: the ctor door is gone.
    ImageBuffer() { InitFields(); }

protected:
    // ??1ImageBuffer@Video@@UEAA@XZ (0x1800f2610) — NON-virtual, in no slot,
    // PROTECTED (B39_RULES D1.3; retail's is protected too): reached by slot 0
    // (`DeletingDtor`, so `Destruct` / `Reconstruct` too) and by the three
    // Graphics subclasses' destructor chains. Its door is gone (ENDGAME B.3.9 VS).
    ~ImageBuffer();

private:
    // The ctor / dtor bodies minus the compiler's stores, out of line (buffers.cpp)
    // only so the unit tests can link recording stand-ins (no Video::Device there).
    void InitFields();      // 0x1800f2580 minus the External stores + stamp
    void ReleaseFields();   // 0x1800f2610 minus the stamps + ~External

protected:
    // The registered doors over the protected surface. See the banner on the
    // forward declaration above `namespace RtVideo`.
    friend struct ::ImageBufferDoorAccess;

    // ?PlatformCreate@ (private in retail, `AEAA`) and ?PlatformSerialize@ stay
    // behind the friend holder: `PlatformSerialize` has a registered door and one
    // in-class caller, both of which reach it through `ImageBufferDoorAccess`
    // (B34_BRIEFS A.4 — friend the holder rather than widen the access, so the
    // header keeps saying what retail says).
    // Addresses corrected from the export table — see the banner above.
    bool PlatformCreate(LevelArray* levels);   // 0x1800f3110 (was 0x1800f2c90) — not ported
    void PlatformSerialize(IO::Serializer& serializer);   // 0x1800f3130

    // RULE B5-15: both literals are COPIED from B35_SLOTS section 3 into the
    // DEFINE_TYPE (video_type_records.cpp):
    //   0x18001e8c0  ?_TypeName@ImageBuffer@Video@@       -> "ImageBuffer"
    //   0x18001e8d0  ?_ParentTypeName@ImageBuffer@Video@@ -> "Object::CompiledExternal"

public:
    // ---- the 0x48 bytes ImageBuffer adds ---------------------------------
    // PUBLIC for the reason `Object::External`'s `_path`/`_refs` are: the ported
    // bodies, the three `Graphics::*ImageBuffer` subclasses and `dx11/` read
    // these at exactly these offsets. Field for field the same bytes as
    // `RtVideo::ImageBuffer` above, cross-asserted below.
    int32_t     _width;        // +0x20  raw; `Width()` is NOT this field
    int32_t     _height;       // +0x24  raw; `Height()` is NOT this field
    ImageType   _dimension;    // +0x28  0 = 2D, 1 = cube
    Usage       _kind;         // +0x2C  bind-flag selector; init 2
    Format      _format;       // +0x30
    uint32_t    _flags;        // +0x34  see the POD's comment for the bit map
    AddressMode _addressU;     // +0x38
    AddressMode _addressV;     // +0x3C
    AddressMode _addressW;     // +0x40
    FilterMode  _filter;       // +0x44  init 1
    // +0x48 is a `System::String`, NOT a pixel buffer — proved by
    // `?Serialize@ImageBuffer@Video@@` handing `this+0x48` to
    // `?Serialize@String@System@@`. It is spelled `void*` rather than
    // `System::String` for the reason `Object::External::_path` is spelled as
    // the POD `RtSystem::Filename`: a real `System::String` member would give
    // this class a compiler-emitted release that runs IN ADDITION to the by-hand
    // release retail's `~ImageBuffer` performs (buffers.cpp), which is the
    // double-free shape B31_RULES records. The POD calls the same eight bytes
    // `pixelData`, which is the name Wave 18 proved wrong; the real class uses
    // the proved one.
    void*       _sourceName;   // +0x48  System::String (refcount at ptr-4)
    uint64_t    _field50;      // +0x50  copied by the copy ctor; no export reads it
    uint64_t    _field58;      // +0x58  copied by the copy ctor; no export reads it
    // The `Video::ImageBufferData*` the Device factory hands back (`DeviceData`
    // slot 10, +0x50 — `Dx11_CreateImageBufferData`). THIS is the field
    // `dx11/src/dx11_backend_access.h`'s deleted `ImageBufferOwner` view pinned
    // at +0x60 (RULE B5-2.1).
    ImageBufferData* _data;    // +0x60
};

static_assert(sizeof(ImageBuffer) == 0x68,
              "Video::ImageBuffer is 0x68 bytes (MOV EDX,0x68 in the slot-0 "
              "vector deleting dtor at 0x18001ed20)");
static_assert(alignof(ImageBuffer) == 8, "Video::ImageBuffer is qword-aligned");
static_assert(sizeof(ImageBuffer) == sizeof(RtVideo::ImageBuffer),
              "the real class and the POD describe the same bytes");

#define RT_VIDEO_IMAGEBUFFER_PIN(MEMBER, POD_MEMBER, OFFSET)                   \
    static_assert(offsetof(ImageBuffer, MEMBER) == (OFFSET),                   \
                  "Video::ImageBuffer::" #MEMBER " @ +" #OFFSET);              \
    static_assert(offsetof(ImageBuffer, MEMBER)                                \
                      == offsetof(RtVideo::ImageBuffer, POD_MEMBER),           \
                  "alias " #MEMBER)

RT_VIDEO_IMAGEBUFFER_PIN(_width,      width,        0x20);
RT_VIDEO_IMAGEBUFFER_PIN(_height,     height,       0x24);
RT_VIDEO_IMAGEBUFFER_PIN(_dimension,  dimension,    0x28);
RT_VIDEO_IMAGEBUFFER_PIN(_kind,       kind,         0x2C);
RT_VIDEO_IMAGEBUFFER_PIN(_format,     format,       0x30);
RT_VIDEO_IMAGEBUFFER_PIN(_flags,      flags,        0x34);
RT_VIDEO_IMAGEBUFFER_PIN(_addressU,   addressU,     0x38);
RT_VIDEO_IMAGEBUFFER_PIN(_addressV,   addressV,     0x3C);
RT_VIDEO_IMAGEBUFFER_PIN(_addressW,   addressW,     0x40);
RT_VIDEO_IMAGEBUFFER_PIN(_filter,     filter,       0x44);
RT_VIDEO_IMAGEBUFFER_PIN(_sourceName, pixelData,    0x48);
RT_VIDEO_IMAGEBUFFER_PIN(_field50,    field50,      0x50);
RT_VIDEO_IMAGEBUFFER_PIN(_field58,    field58,      0x58);
RT_VIDEO_IMAGEBUFFER_PIN(_data,       platformData, 0x60);

#undef RT_VIDEO_IMAGEBUFFER_PIN

}  // namespace Video
