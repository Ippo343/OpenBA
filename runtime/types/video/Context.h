#pragma once
// ============================================================================
// runtime/types/video/Context.h — Video::Context, binary-compatible layout
// ============================================================================
//
// Wave 6 lane A. `Video::Context` is the drawing state machine: a big, flat,
// NON-POLYMORPHIC state block that the game fills in with setters and then
// hands to the backend (`Video::ContextData*` at +0x2160, which today is OUR
// code — dx11/src/contextdata.cpp).
//
// SIZE IS PROVEN, NOT INFERRED. `Engine::MainLoop::CreateVideoContext`
// (Runtime 0x1800dd500, ordinal 2139) allocates the object itself:
//
//     pCtx = MemoryAllocator::Allocate(_singleton, 0x2168, 8);
//     Video::Context::Context(pCtx, Video::Device::_singleton,
//                             0x800000, 0x400000, 0x100000, nullptr);
//
// so sizeof(Video::Context) == 0x2168 and its alignment is 8.
//
// THERE IS NO VTABLE. The ctor's first store is `*(Device**)this = device`
// (0x1800f1820), not a vtable stamp, and no Context method is virtual. That
// matters for PLAN §4.3 risk 3: this class carries no vtable ownership
// question at all, which is the single biggest reason a Wave 6 Context port
// can coexist with the DLL's own un-ported `Graphics::Renderer` on one live
// object.
//
// ---------------------------------------------------------------------------
// EVIDENCE FOR EVERY OFFSET
// ---------------------------------------------------------------------------
// The Context has an unusually complete set of one-line accessors, and a
// one-line accessor is the strongest kind of evidence there is: it gives the
// offset AND the field's real name, straight out of the shipped export table.
// Every named field below cites either such an accessor or the setter that
// owns the field. Retail addresses and ordinals are from the export directory
// of C:\GOG Games\Banished\Runtime-x64.dll (read with pefile, not guessed).
//
//   off     field                     proven by                          retail
//   ------  ------------------------  ---------------------------------  ---------
//   0x0000  _device                   ctor's first store                 0x1800f1820
//   0x0008  _frameIndex               ?GetFrameIndex@                    0x18001ae50
//   0x000c  _shaderDetail             ?GetShaderDetail@                  0x18001ae30
//   0x0010  _dirty                    every setter ORs into it
//   0x0018  _imageBufferDirty         ?SetImageBuffer@ (bit per slot)    0x180019ed0
//   0x001c  _vertexBufferDirty        ?SetVertexBuffer@ (bit per slot)   0x180019f20
//   0x0020  _transformCount           ?SetTransform@ high-water mark     0x18001a6b0
//   0x0028  _renderBuffer             ?GetRenderBuffer@                  0x180019ec0
//   0x0030  _imageBuffers[16]         ?SetImageBuffer@                   0x180019ed0
//   0x00b0  _vertexLayout             ?SetVertexLayout@                  0x180019f00
//   0x00b8  _vertexBuffers[16]        ?SetVertexBuffer@ / ?GetVertexBuffer@
//   0x0138  _vertexOffsets[16]        ?GetVertexBufferOffset@            0x180019fa0
//   0x0178  _vertexStrides[16]        ?GetVertexBufferStride@            0x180019f90
//   0x01b8  _indexBuffer              ?SetIndexBuffer@                   0x180019fb0
//   0x01c0  _indexOffset              ?SetIndexBuffer@ (2nd arg)         0x180019fb0
//   0x01c8  _renderState              ?SetRenderState@                   0x180019fe0
//   0x01d0  _viewport                 ?GetViewport@ / ?SetViewport@      0x18001a0c0
//   0x01e0  _scissor                  ?SetScissor@                       0x18001a0f0
//   0x01f0  _vertexProgram            ?SetVertexProgram@                 0x18001a000
//   0x01f8  _pixelProgram             ?SetPixelProgram@                  0x18001a020
//   0x0258  _fogColorPixel            ?SetFog@ 2nd copy of the colour    0x18001a360
//   0x0298  _materialConstants        ?GetMaterialConstantBuffer@        0x18001aef0
//   0x02e0  _pixelConstants           ?GetPixelConstantBuffer@           0x18001af00
//   0x0328  _viewProjection           ?SetCamera@ (invView * proj)       0x18001a150
//   0x0368  _inverseView              ?SetCamera@ (Matrix::Inverse)      0x18001a150
//   0x03a8  _cameraPosition           ?SetCamera@ (view row 3)           0x18001a150
//   0x03c8  _time                     ?SetTime@                          0x18001a330
//   0x03d8  _fogParams                ?SetFog@                           0x18001a360
//   0x03e8  _fogColor                 ?SetFog@                           0x18001a360
//   0x03f8  _inverseScale             ?SetInverseScale@                  0x18001a880
//   0x0408  _color                    ?SetColor@                         0x18001a8e0
//   0x10d8  _transforms[52]           ?SetTransform@ (this + i*0x40 + 0x10d8)
//   0x1f78  _globalConstants          ?GetGlobalConstantBuffer@          0x18001aea0
//   0x1fc0  _instanceConstants        ?GetInstanceConstantBuffer@        0x18001aed0
//   0x2008  _boneConstants            ?GetBoneConstantBuffer@            0x18001aec0
//   0x2050  _transformConstants       ?GetTransformConstantBuffer@       0x18001aeb0
//   0x2098  _lightConstants           ?GetLightConstantBuffer@           0x18001aee0
//   0x20e0  _dynamicVertexCursor      ?AddVertex@                        0x18001ad40
//   0x20e8  _dynamicVertexStride      ?AddVertex@                        0x18001ad40
//   0x20f8  _dynamicVertexBuffer      ctor (param 2 > 0 branch)          0x1800f1820
//   0x2110  _instanceCount            dx11/src/contextdata.cpp slot 16/18
//   0x2118  _dynamicIndexBuffer       ctor (param 3 > 0 branch)          0x1800f1820
//   0x2130  _dynamicInstanceBuffer    ctor (param 4 > 0 branch)          0x1800f1820
//   0x2138  _defaultRenderBuffer      ?GetDefaultRenderBuffer@           0x18001ae40
//   0x2140  _defaultColorImage        ctor (param 5 == nullptr branch)   0x1800f1820
//   0x2148  _defaultDepthImage        ctor (param 5 == nullptr branch)   0x1800f1820
//   0x2150  _trianglesSubmitted       ?GetTrianglesSubmitted@            0x18001ae60
//   0x2154  _verticesSubmitted        ?GetVerticesSubmitted@             0x18001ae70
//   0x2158  _drawsSubmitted           ?GetDrawsSubmitted@                0x18001ae80
//   0x215c  _objectsSubmitted         ?GetObjectsSubmitted@              0x18001ae90
//   0x2160  _contextData              every backend thunk's first load
//
// The sizes of the seven `ConstantBuffer` members are pinned by the ctor and
// by the getters together: the ctor initialises the pair at +0x298 and +0x2e0
// with the same field-by-field sequence, and 0x2e0 - 0x298 == 0x48; the same
// stride separates the five at +0x1f78 / +0x1fc0 / +0x2008 / +0x2050 / +0x2098.
// So sizeof(Video::ConstantBuffer) == 0x48. This header does NOT model its
// interior: nothing in lane A reads a field of one, and inventing a layout we
// have not proven is how a shared struct silently corrupts (PLAN §4.3 risk 1).
//
// CROSS-CHECK. dx11/src/contextdata.cpp recovered +0x0028, +0x01d0, +0x2110,
// +0x2138, +0x2150..+0x215c, +0x20e0, +0x20e8 and +0x2160 INDEPENDENTLY, from
// the DX11 side, before this lane existed. Every one agrees.
//
// ---------------------------------------------------------------------------
// THE DIRTY MASK (+0x0010)
// ---------------------------------------------------------------------------
// Each bit is named by the setter that sets it. Bits 9..16 (0x1fe00) are the
// constant-buffer group, which is exactly the value `Reset` writes wholesale.
//
//   0x00000001  render buffer      ?SetRenderBuffer@         0x1800f2100
//   0x00000002  image buffers      ?SetImageBuffer@          0x180019ed0
//   0x00000004  vertex layout      ?SetVertexLayout@         0x180019f00
//   0x00000008  vertex buffers     ?SetVertexBuffer@         0x180019f20
//   0x00000010  index buffer       ?SetIndexBuffer@          0x180019fb0
//   0x00000020  render state       ?SetRenderState@          0x180019fe0
//   0x00000040  vertex program     ?SetVertexProgram@        0x18001a000
//   0x00000080  pixel program      ?SetPixelProgram@         0x18001a020
//   0x00000200  viewport           ?SetViewport@             0x18001a060
//   0x00000400  scissor            ?SetScissor@              0x18001a0f0
//   0x00000800  global constants   ?SetCamera@ / ?SetTime@
//   0x00001000  material constants ?SetColor@ / ?SetInverseScale@
//   0x00004000  transform consts   ?SetTransform@
//   0x00010000  pixel constants    ?SetFog@ (which ORs 0x10800)
//
// 0x100 and 0x2000 and 0x8000 are inside `Reset`'s 0x1fe00 but no lane-A
// function names them; they are NOT guessed here.
//
// ---------------------------------------------------------------------------
// WAVE 9 LANE CTX — the three unnamed bits, from the CONSUMER
// ---------------------------------------------------------------------------
// The setters could not name 0x100 / 0x2000 / 0x8000 because a setter only ORs
// a bit; the bit's MEANING lives in the one function that consumes the whole
// mask, `?BindResources@Context@Video@@AEAAXXZ` (0x1800ef060). Reading its arms
// closes all three, and corrects a fourth entry above:
//
//   0x00000100  VIDEO program      ?SetVideoProgram@         0x18001a040
//               and Context+0x200 is that program POINTER, not staging — see
//               the note on `_pixelConstantStaging` below.
//   0x00002000  LIGHT constants    the buffer at +0x2098, uploading +0x1dd8
//                                  len 0x1a0, which is `?SetLight@`'s block.
//   0x00008000  MATERIAL constants the buffer at +0x298, uploading +0x208
//                                  len 0x40.
//   0x00001000  is listed above as "material constants". By the buffer it
//               actually feeds it is the INSTANCE constants: BindResources
//               uploads +0x3f8 len 0x20 (`_inverseScale` and `_color`) into
//               the buffer at +0x1fc0, which this header already calls
//               `_instanceConstants`. The enum name `kDirtyMaterialConstants`
//               is left as it is — three lanes include this header — but it is
//               MISLEADING and runtime/src/video/context_bind.cpp does not use
//               it. `kDirtyMaterialConstants` is 0x8000, if it is ever renamed.
//
// BindResources also settles what +0x20 and +0x24 are for: its transform arm
// fires on `(dirty & 0x4000) || (int32)+0x20 > 0 || (int32)+0x24 > 0`, and its
// final clear is a QWORD store over BOTH. So `_unknown0024` is a live counter
// with the same sign convention as `_transformCount`, not padding.
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace RtVideo {

// Video::Viewport — {x, y, width, height}, four ints. Proven by ?SetViewport@
// (0x18001a060) and ?SetScissor@ (0x18001a0f0), which both compare and copy
// exactly four dwords, and by ?GetViewport@ (0x18001a0c0) returning 16 bytes.
struct Viewport {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
};
static_assert(sizeof(Viewport) == 0x10, "Video::Viewport is 16 bytes");

// Math::Vector4 / Math::Matrix as lane A needs them: raw float storage. The
// engine's own types live in runtime/types/math/, but this header must not
// depend on them to describe a byte layout, and a Matrix here is only ever
// memcpy'd or handed to Math::Matrix::Inverse / operator* by address.
struct Vector4Raw {
    float x, y, z, w;
};
static_assert(sizeof(Vector4Raw) == 0x10, "Math::Vector4 is 16 bytes");

struct MatrixRaw {
    float m[16];
};
static_assert(sizeof(MatrixRaw) == 0x40, "Math::Matrix is 64 bytes");

// Video::RenderBuffer, only as far as Reset reads it: Reset (0x1800f1e50)
// loads *(int*)(rb + 0x10) and *(int*)(rb + 0x14) as the width/height it
// resets the viewport and scissor to. Lane B owns this class; this is the
// minimum prefix lane A needs and deliberately not a full layout.
struct RenderBufferPrefix {
    uint8_t _head[0x10];
    int32_t width;    // +0x10
    int32_t height;   // +0x14
};
static_assert(offsetof(RenderBufferPrefix, width)  == 0x10, "RenderBuffer::width");
static_assert(offsetof(RenderBufferPrefix, height) == 0x14, "RenderBuffer::height");

// Video::ConstantBuffer — 0x48 bytes, interior deliberately opaque (see above).
struct ConstantBufferOpaque {
    uint8_t _bytes[0x48];
};
static_assert(sizeof(ConstantBufferOpaque) == 0x48, "Video::ConstantBuffer is 0x48 bytes");

// The number of transform slots is 0xd00 / 0x40 = 52, from the byte count
// Reset (0x1800f1e50) memsets over the array at +0x10d8.
constexpr int kTransformSlots  = 52;
// Sixteen texture and sixteen vertex-stream slots: ClearResources (0x1800f2090)
// and Reset both walk exactly 0x10 iterations over each array.
constexpr int kImageBufferSlots  = 16;
constexpr int kVertexBufferSlots = 16;

// ---- Dirty-mask bits (named by their setters; see the table above) ----------
enum : uint32_t {
    kDirtyRenderBuffer      = 0x00000001u,
    kDirtyImageBuffers      = 0x00000002u,
    kDirtyVertexLayout      = 0x00000004u,
    kDirtyVertexBuffers     = 0x00000008u,
    kDirtyIndexBuffer       = 0x00000010u,
    kDirtyRenderState       = 0x00000020u,
    kDirtyVertexProgram     = 0x00000040u,
    kDirtyPixelProgram      = 0x00000080u,
    kDirtyViewport          = 0x00000200u,
    kDirtyScissor           = 0x00000400u,
    kDirtyGlobalConstants   = 0x00000800u,
    kDirtyMaterialConstants = 0x00001000u,
    kDirtyTransformConstants= 0x00004000u,
    kDirtyPixelConstants    = 0x00010000u,
    // The literal Reset (0x1800f1e50) stores into +0x10 before it starts
    // re-dirtying individual bindings. Bits 9..16 inclusive.
    kDirtyAllConstants      = 0x0001fe00u,
};

// No #pragma pack: every member below is already at its natural alignment, so
// the compiler inserts no padding and sizeof lands on 0x2168 on its own. The
// static_asserts underneath are what actually prove that, on this compiler.
struct Context {
    void*    _device;                                   // 0x0000
    int32_t  _frameIndex;                               // 0x0008
    int32_t  _shaderDetail;                             // 0x000c
    uint32_t _dirty;                                    // 0x0010
    uint32_t _unknown0014;                              // 0x0014
    uint32_t _imageBufferDirty;                         // 0x0018
    uint32_t _vertexBufferDirty;                        // 0x001c
    int32_t  _transformCount;                           // 0x0020
    int32_t  _unknown0024;                              // 0x0024
    void*    _renderBuffer;                             // 0x0028
    void*    _imageBuffers[kImageBufferSlots];          // 0x0030
    void*    _vertexLayout;                             // 0x00b0
    void*    _vertexBuffers[kVertexBufferSlots];        // 0x00b8
    int32_t  _vertexOffsets[kVertexBufferSlots];        // 0x0138
    int32_t  _vertexStrides[kVertexBufferSlots];        // 0x0178
    void*    _indexBuffer;                              // 0x01b8
    int32_t  _indexOffset;                              // 0x01c0
    int32_t  _unknown01c4;                              // 0x01c4
    void*    _renderState;                              // 0x01c8
    Viewport _viewport;                                 // 0x01d0
    Viewport _scissor;                                  // 0x01e0
    void*    _vertexProgram;                            // 0x01f0
    void*    _pixelProgram;                             // 0x01f8

    // 0x0200 .. 0x0297. Reset memsets 0x208..0x247 and 0x248..0x297 in two
    // calls; ?SetFog@ writes a second copy of the fog colour at +0x258, and
    // ?SetLight@ / ?SetMaterialParameter@ / ?SetUserPosition@ (all outside
    // lane A) own the rest. Not decomposed here because lane A cannot prove it.
    //
    // *** WAVE 9 LANE CTX: THE FIRST EIGHT BYTES OF THIS ARRAY ARE A POINTER.
    // *** Context+0x200 is `_videoProgram` — `?SetVideoProgram@Context@Video@@`
    // (0x18001a040) compares it against its argument, ORs 0x100 into the dirty
    // mask and stores it, and `?BindResources@` dereferences it twice. The
    // STAGING is 0x208..0x297, which is exactly the two ranges Reset memsets
    // and exactly the two ranges BindResources uploads. The member is left
    // undivided so no other lane's offsets move; anything reading +0x200
    // through this array is reading a pointer, not constant data.
    uint8_t  _pixelConstantStaging[0x98];               // 0x0200

    ConstantBufferOpaque _materialConstants;            // 0x0298
    ConstantBufferOpaque _pixelConstants;               // 0x02e0

    MatrixRaw   _viewProjection;                        // 0x0328
    MatrixRaw   _inverseView;                           // 0x0368
    Vector4Raw  _cameraPosition;                        // 0x03a8
    Vector4Raw  _unknown03b8;                           // 0x03b8
    Vector4Raw  _time;                                  // 0x03c8
    Vector4Raw  _fogParams;                             // 0x03d8
    Vector4Raw  _fogColor;                              // 0x03e8
    Vector4Raw  _inverseScale;                          // 0x03f8
    Vector4Raw  _color;                                 // 0x0408

    // 0x0418 .. 0x10d7, one 0xcc0-byte block Reset clears in a single memset.
    // ?SetBones@ (0x18001a780), ?SetShadowProjection@ (0x18001ac20) and
    // ?SetTextureProjection@ (0x18001ab80) write in here; none is lane A's.
    uint8_t  _boneAndProjectionStaging[0xcc0];          // 0x0418

    MatrixRaw _transforms[kTransformSlots];             // 0x10d8

    // 0x1dd8 .. 0x1f77, one 0x1a0-byte block Reset clears in a single memset.
    // ?SetLight@ (0x18001a950) writes here.
    uint8_t  _lightStaging[0x1a0];                      // 0x1dd8

    ConstantBufferOpaque _globalConstants;              // 0x1f78
    ConstantBufferOpaque _instanceConstants;            // 0x1fc0
    ConstantBufferOpaque _boneConstants;                // 0x2008
    ConstantBufferOpaque _transformConstants;           // 0x2050
    ConstantBufferOpaque _lightConstants;               // 0x2098

    uint8_t* _dynamicVertexCursor;                      // 0x20e0
    int32_t  _dynamicVertexStride;                      // 0x20e8
    int32_t  _unknown20ec;                              // 0x20ec
    uint64_t _unknown20f0;                              // 0x20f0
    void*    _dynamicVertexBuffer;                      // 0x20f8
    uint64_t _unknown2100;                              // 0x2100
    uint64_t _unknown2108;                              // 0x2108
    int32_t  _instanceCount;                            // 0x2110
    int32_t  _unknown2114;                              // 0x2114
    void*    _dynamicIndexBuffer;                       // 0x2118
    uint64_t _unknown2120;                              // 0x2120
    uint64_t _unknown2128;                              // 0x2128
    void*    _dynamicInstanceBuffer;                    // 0x2130
    void*    _defaultRenderBuffer;                      // 0x2138
    void*    _defaultColorImage;                        // 0x2140
    void*    _defaultDepthImage;                        // 0x2148
    int32_t  _trianglesSubmitted;                       // 0x2150
    int32_t  _verticesSubmitted;                        // 0x2154
    int32_t  _drawsSubmitted;                           // 0x2158
    int32_t  _objectsSubmitted;                         // 0x215c
    void*    _contextData;                              // 0x2160
};

// Every offset lane A touches, asserted. A mismatch here is a compile error
// rather than a wild write into a live render context.
static_assert(offsetof(Context, _device)                == 0x0000, "Context::_device");
static_assert(offsetof(Context, _frameIndex)            == 0x0008, "Context::_frameIndex");
static_assert(offsetof(Context, _shaderDetail)          == 0x000c, "Context::_shaderDetail");
static_assert(offsetof(Context, _dirty)                 == 0x0010, "Context::_dirty");
static_assert(offsetof(Context, _imageBufferDirty)      == 0x0018, "Context::_imageBufferDirty");
static_assert(offsetof(Context, _vertexBufferDirty)     == 0x001c, "Context::_vertexBufferDirty");
static_assert(offsetof(Context, _transformCount)        == 0x0020, "Context::_transformCount");
static_assert(offsetof(Context, _renderBuffer)          == 0x0028, "Context::_renderBuffer");
static_assert(offsetof(Context, _imageBuffers)          == 0x0030, "Context::_imageBuffers");
static_assert(offsetof(Context, _vertexLayout)          == 0x00b0, "Context::_vertexLayout");
static_assert(offsetof(Context, _vertexBuffers)         == 0x00b8, "Context::_vertexBuffers");
static_assert(offsetof(Context, _vertexOffsets)         == 0x0138, "Context::_vertexOffsets");
static_assert(offsetof(Context, _vertexStrides)         == 0x0178, "Context::_vertexStrides");
static_assert(offsetof(Context, _indexBuffer)           == 0x01b8, "Context::_indexBuffer");
static_assert(offsetof(Context, _indexOffset)           == 0x01c0, "Context::_indexOffset");
static_assert(offsetof(Context, _renderState)           == 0x01c8, "Context::_renderState");
static_assert(offsetof(Context, _viewport)              == 0x01d0, "Context::_viewport");
static_assert(offsetof(Context, _scissor)               == 0x01e0, "Context::_scissor");
static_assert(offsetof(Context, _vertexProgram)         == 0x01f0, "Context::_vertexProgram");
static_assert(offsetof(Context, _pixelProgram)          == 0x01f8, "Context::_pixelProgram");
static_assert(offsetof(Context, _pixelConstantStaging)  == 0x0200, "Context::_pixelConstantStaging");
static_assert(offsetof(Context, _materialConstants)     == 0x0298, "Context::_materialConstants");
static_assert(offsetof(Context, _pixelConstants)        == 0x02e0, "Context::_pixelConstants");
static_assert(offsetof(Context, _viewProjection)        == 0x0328, "Context::_viewProjection");
static_assert(offsetof(Context, _inverseView)           == 0x0368, "Context::_inverseView");
static_assert(offsetof(Context, _cameraPosition)        == 0x03a8, "Context::_cameraPosition");
static_assert(offsetof(Context, _time)                  == 0x03c8, "Context::_time");
static_assert(offsetof(Context, _fogParams)             == 0x03d8, "Context::_fogParams");
static_assert(offsetof(Context, _fogColor)              == 0x03e8, "Context::_fogColor");
static_assert(offsetof(Context, _inverseScale)          == 0x03f8, "Context::_inverseScale");
static_assert(offsetof(Context, _color)                 == 0x0408, "Context::_color");
static_assert(offsetof(Context, _boneAndProjectionStaging) == 0x0418, "Context::_boneAndProjectionStaging");
static_assert(offsetof(Context, _transforms)            == 0x10d8, "Context::_transforms");
static_assert(offsetof(Context, _lightStaging)          == 0x1dd8, "Context::_lightStaging");
static_assert(offsetof(Context, _globalConstants)       == 0x1f78, "Context::_globalConstants");
static_assert(offsetof(Context, _instanceConstants)     == 0x1fc0, "Context::_instanceConstants");
static_assert(offsetof(Context, _boneConstants)         == 0x2008, "Context::_boneConstants");
static_assert(offsetof(Context, _transformConstants)    == 0x2050, "Context::_transformConstants");
static_assert(offsetof(Context, _lightConstants)        == 0x2098, "Context::_lightConstants");
static_assert(offsetof(Context, _dynamicVertexCursor)   == 0x20e0, "Context::_dynamicVertexCursor");
static_assert(offsetof(Context, _dynamicVertexStride)   == 0x20e8, "Context::_dynamicVertexStride");
static_assert(offsetof(Context, _dynamicVertexBuffer)   == 0x20f8, "Context::_dynamicVertexBuffer");
static_assert(offsetof(Context, _instanceCount)         == 0x2110, "Context::_instanceCount");
static_assert(offsetof(Context, _dynamicIndexBuffer)    == 0x2118, "Context::_dynamicIndexBuffer");
static_assert(offsetof(Context, _dynamicInstanceBuffer) == 0x2130, "Context::_dynamicInstanceBuffer");
static_assert(offsetof(Context, _defaultRenderBuffer)   == 0x2138, "Context::_defaultRenderBuffer");
static_assert(offsetof(Context, _defaultColorImage)     == 0x2140, "Context::_defaultColorImage");
static_assert(offsetof(Context, _defaultDepthImage)     == 0x2148, "Context::_defaultDepthImage");
static_assert(offsetof(Context, _trianglesSubmitted)    == 0x2150, "Context::_trianglesSubmitted");
static_assert(offsetof(Context, _verticesSubmitted)     == 0x2154, "Context::_verticesSubmitted");
static_assert(offsetof(Context, _drawsSubmitted)        == 0x2158, "Context::_drawsSubmitted");
static_assert(offsetof(Context, _objectsSubmitted)      == 0x215c, "Context::_objectsSubmitted");
static_assert(offsetof(Context, _contextData)           == 0x2160, "Context::_contextData");

// The allocation size in Engine::MainLoop::CreateVideoContext (0x1800dd500).
static_assert(sizeof(Context) == 0x2168, "sizeof(Video::Context) == 0x2168");

constexpr size_t kContextSize = sizeof(Context);

// ---------------------------------------------------------------------------
// Video::ContextData vtable byte offsets
// ---------------------------------------------------------------------------
// Nine Context methods are pure tail-jumps into the backend object at +0x2160:
//
//     MOV RCX,[RCX+0x2160] / MOV RAX,[RCX] / JMP [RAX + <offset>]
//
// The offsets below are read out of those three-instruction bodies. They agree
// exactly with the twenty-slot table dx11/src/contextdata.cpp recovered from
// the VideoDX11 side, and runtime/src/video/context.cpp re-verifies each one
// against the shipped bytes at registration time rather than trusting either.
enum : unsigned {
    kSlotClear                         = 0x20,
    kSlotDrawIndexed                   = 0x60,
    kSlotGetMaxDynamicInstances        = 0x68,
    kSlotBeginVertices                 = 0x70,
    kSlotEndVertices                   = 0x78,
    kSlotBeginIndexedVertices          = 0x80,
    kSlotEndIndexedVertices            = 0x88,
    kSlotBeginInstancedIndexedVertices = 0x90,
    kSlotEndInstancedIndexedVertices   = 0x98,
};

} // namespace RtVideo
