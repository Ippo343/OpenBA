#pragma once
// Binary-compatible layouts for the 12 Video::*Data subclasses defined by
// VideoDX11-x64.dll.
//
// Every one of these derives from a Video::*Data base class that lives in
// Runtime-x64.dll. The base contributes the first 0x10 bytes:
//
//     +0x00  void*  vptr        (overwritten by VideoDX11 after the base ctor)
//     +0x08  void*  owner       (back-pointer to the Video::* object)
//
// Sizes below are exact: each comes from the MemAlloc(size, 4) in the
// corresponding DeviceData factory slot. See ../README.md for the vtable map
// and ../../decomp/videodx11.md for the derivation.
//
// Fields past the base are named only where a decompiled function proves the
// meaning; everything else is a raw slot so the layout stays honest. Rename
// them as functions get ported — do not guess.

#include <cstdint>
#include <cstddef>

namespace VideoDX11 {

// ============================================================
// Common Runtime-side base (first 0x10 bytes of every *Data)
// ============================================================
struct DataBase {
    void* vptr;    // +0x00
    void* owner;   // +0x08  Video::Device* / Video::Context* / Video::ImageBuffer* / ...
};
static_assert(sizeof(DataBase) == 0x10, "DataBase size");
static_assert(offsetof(DataBase, owner) == 0x08, "DataBase::owner");

// ============================================================
// DeviceData — 0x70 bytes, vtable 0x180018bb0 (20 slots)
// Created by the CreateDeviceData export (0x1800011a0).
//
// Field meanings recovered from:
//   slot 0  dtor      0x180004270  (frees +0x60 array)
//   slot 3  shutdown  0x1800051c0  (releases +0x18/+0x20/+0x28/+0x30/+0x38,
//                                   DestroyCursor(+0x48), reads +0x40)
//   slot 8  cursor    0x180005450  (writes +0x48/+0x50/+0x58/+0x5c)
// ============================================================
struct DeviceData {
    void*     vptr;              // +0x00
    void*     device;            // +0x08  Video::Device* owner

    void*     unk10;             // +0x10  COM interface released on shutdown — IDXGIFactory1* (TBD)
    void*     swapChain;         // +0x18  IDXGISwapChain* (SetFullscreenState(FALSE) before release)
    void*     d3dDevice;         // +0x20  ID3D11Device*        (CreateTexture2D callee in 0x1800052b0)
    void*     d3dContext;        // +0x28  ID3D11DeviceContext* (every resource Bind reads this)
    void*     depthBuffer;       // +0x30  ID3D11Texture2D* D24_UNORM_S8_UINT, BIND_DEPTH_STENCIL
    void*     colorBuffer;       // +0x38  ID3D11Texture2D* B8G8R8A8_UNORM,    BIND_RENDER_TARGET

    uint8_t   wasFullscreen;     // +0x40  mirrored into Application::_singleton+0x40 on shutdown
    uint8_t   pad41[7];          // +0x41

    void*     cursor;            // +0x48  HCURSOR
    void*     cursorImage;       // +0x50  Video::ImageBuffer* the cursor was built from
    int32_t   cursorHotspotX;    // +0x58
    int32_t   cursorHotspotY;    // +0x5c

    // Registry of the Video::ImageBuffer*s that must be released and recreated
    // across a device / swap-chain reset. Membership test (from ~ImageBufferData):
    //   IsGenerated() || (flags & 0x70) || IsRendered()
    // Appended by 0x180002300 (add-if-absent), swap-removed by ~ImageBufferData.
    // NOT the Device::GraphicsInterface array — the older note had that wrong;
    // the stride here is 8, not 0x18.
    void*     trackedImages;     // +0x60  Video::ImageBuffer*[]
    int32_t   trackedCount;      // +0x68
    int32_t   trackedCapacity;   // +0x6c
};
static_assert(sizeof(DeviceData) == 0x70, "DeviceData size (MemAlloc 0x70 @ 0x1800011a0)");
static_assert(offsetof(DeviceData, swapChain)       == 0x18, "DeviceData::swapChain");
static_assert(offsetof(DeviceData, d3dDevice)       == 0x20, "DeviceData::d3dDevice");
static_assert(offsetof(DeviceData, d3dContext)      == 0x28, "DeviceData::d3dContext");
static_assert(offsetof(DeviceData, depthBuffer)     == 0x30, "DeviceData::depthBuffer");
static_assert(offsetof(DeviceData, colorBuffer)     == 0x38, "DeviceData::colorBuffer");
static_assert(offsetof(DeviceData, wasFullscreen)   == 0x40, "DeviceData::wasFullscreen");
static_assert(offsetof(DeviceData, cursor)          == 0x48, "DeviceData::cursor");
static_assert(offsetof(DeviceData, cursorImage)     == 0x50, "DeviceData::cursorImage");
static_assert(offsetof(DeviceData, cursorHotspotX)  == 0x58, "DeviceData::cursorHotspotX");
static_assert(offsetof(DeviceData, trackedImages)   == 0x60, "DeviceData::trackedImages");
static_assert(offsetof(DeviceData, trackedCount)    == 0x68, "DeviceData::trackedCount");
static_assert(offsetof(DeviceData, trackedCapacity) == 0x6c, "DeviceData::trackedCapacity");

// DeviceData vtable slot indices — see ../README.md status board.
enum DeviceDataSlot : int {
    kSlot_Dtor                    = 0,   // 0x180004270
    kSlot_EnumerateInterfaces     = 1,   // 0x180004690
    kSlot_CreateDevice            = 2,   // 0x180004e10
    kSlot_Shutdown                = 3,   // 0x1800051c0
    // Slots 4-7 are one-liner stubs; their *names* are still unknown (they are
    // pure virtuals on the Runtime-side Video::DeviceData that DX11 declines to
    // implement), but their bodies are fully decoded.
    kSlot_Stub4_Void              = 4,   // 0x180001190  `ret`
    kSlot_Stub5_ZeroFloat         = 5,   // 0x180005430  `xorps xmm0,xmm0; ret`
    kSlot_Stub6_False             = 6,   // 0x180005440  `xor al,al; ret`
    kSlot_Stub7_True              = 7,   // 0x180001060  `mov al,1; ret`
    kSlot_SetImageBuffer          = 8,   // 0x180005450  (HCURSOR from ImageBuffer)
    kSlot_CreateContextData       = 9,   // 0x180005650
    kSlot_CreateImageBufferData   = 10,  // 0x1800056c0
    kSlot_CreateConstantBufferData= 11,  // 0x180005730
    kSlot_CreateIndexBufferData   = 12,  // 0x1800057a0
    kSlot_CreateVertexBufferData  = 13,  // 0x180005810
    kSlot_CreateVertexLayoutData  = 14,  // 0x180005880
    kSlot_CreateVertexProgramData = 15,  // 0x1800058f0
    kSlot_CreatePixelProgramData  = 16,  // 0x180005960
    kSlot_CreateRenderStateData   = 17,  // 0x1800059d0
    kSlot_CreateVideoProgramData  = 18,  // 0x180005a40
    kSlot_CreateRenderBufferData  = 19,  // 0x180005aa0
    kDeviceDataSlotCount          = 20
};

// ============================================================
// Resource *Data subclasses.
//
// All twelve vtables were walked 2026-07-24, so each class's slot count is
// exact (see kSlots below and the status board in ../README.md). Fields named
// here are proven by a decompiled slot; the rest stay raw. Do not guess.
//
// Every resource class except ContextData reaches D3D11 the same way — it does
// NOT cache the device or context, it walks to them on each call:
//
//     g_Device (0x1800134b8) -> Video::Device* -> [+0x100] DeviceData
//                            -> [+0x28] ID3D11DeviceContext*
//
// ContextData is the exception: it caches the context at +0x18.
// ============================================================

// 0x20, vtable 0x1800186f0 (20 slots), factory 0x180005650
struct ContextData {
    void*   vptr;       // +0x00
    void*   context;    // +0x08  Video::Context*
    int32_t unk10;      // +0x10  zeroed by factory
    int32_t pad14;
    void*   d3dContext; // +0x18  ID3D11DeviceContext* (slot 10 calls IASetIndexBuffer on it)
};
static_assert(sizeof(ContextData) == 0x20, "ContextData size");
static_assert(offsetof(ContextData, d3dContext) == 0x18, "ContextData::d3dContext");

// 0x38, vtable 0x180018790, factory 0x1800056c0
//
// All five COM interfaces identified 2026-07-24 from the seven slots (lane L3).
// Fields are void* rather than the D3D11 types so this shared header does not
// force <d3d11.h> on the other lanes; dx11/src/imagebufferdata.cpp casts.
//
//   +0x10 ID3D11SamplerState*       created by 0x180002720 via CreateSamplerState
//                                   (ID3D11Device vtable +0xb8 = index 23)
//   +0x18 ID3D11Texture2D*          created by slot 3 via CreateTexture2D (+0x28,
//                                   index 5); slots 1/2 instead AddRef the
//                                   DeviceData color/depth buffer into it
//   +0x20 ID3D11RenderTargetView*   CreateRenderTargetView (+0x48, index 9)
//   +0x28 ID3D11DepthStencilView*   CreateDepthStencilView (+0x50, index 10)
//   +0x30 ID3D11ShaderResourceView* CreateShaderResourceView (+0x38, index 7)
//
// The dtor (0x180002430) and slot 3 (0x180002870) both release these in the
// order +0x28, +0x20, +0x30, +0x10, +0x18 and then null all five. That order is
// reproduced verbatim in the port — it is not the declaration order.
struct ImageBufferData {
    void* vptr;               // +0x00
    void* imageBuffer;        // +0x08  Video::ImageBuffer*
    void* sampler;            // +0x10  ID3D11SamplerState*
    void* texture;            // +0x18  ID3D11Texture2D*
    void* renderTargetView;   // +0x20  ID3D11RenderTargetView*
    void* depthStencilView;   // +0x28  ID3D11DepthStencilView*
    void* shaderResourceView; // +0x30  ID3D11ShaderResourceView*
};
static_assert(sizeof(ImageBufferData) == 0x38, "ImageBufferData size");
static_assert(offsetof(ImageBufferData, imageBuffer)        == 0x08, "ImageBufferData::imageBuffer");
static_assert(offsetof(ImageBufferData, sampler)            == 0x10, "ImageBufferData::sampler");
static_assert(offsetof(ImageBufferData, texture)            == 0x18, "ImageBufferData::texture");
static_assert(offsetof(ImageBufferData, renderTargetView)   == 0x20, "ImageBufferData::renderTargetView");
static_assert(offsetof(ImageBufferData, depthStencilView)   == 0x28, "ImageBufferData::depthStencilView");
static_assert(offsetof(ImageBufferData, shaderResourceView) == 0x30, "ImageBufferData::shaderResourceView");

// ImageBufferData vtable slot indices (0x180018790, 7 slots) — recovered by
// lane L3. Names describe what the code does; the Runtime-side Video::ImageBufferData
// interface names are not recoverable from this DLL alone.
enum ImageBufferDataSlot : int {
    kImg_Dtor              = 0,  // 0x180002400  scalar deleting dtor -> 0x180002430
    kImg_AttachColorBuffer = 1,  // 0x180002580  alias DeviceData::colorBuffer + RTV
    kImg_AttachDepthBuffer = 2,  // 0x180002650  alias DeviceData::depthBuffer + DSV
    kImg_Create            = 3,  // 0x180002870  CreateTexture2D + SRV + RTV/DSV + sampler
    kImg_Bind              = 4,  // 0x180002da0  PSSetSamplers + PSSetShaderResources
    kImg_Update            = 5,  // 0x180002e10  Map/WRITE_DISCARD or UpdateSubresource
    kImg_Read              = 6,  // 0x180002ef0  staging copy + Map/READ readback
    kImageBufferDataSlotCount = 7
};

// 0x28, vtable 0x1800185d0 (4 slots), factory 0x180005730
//
// +0x18 is a growable array of ID3D11Buffer* — the dtor (0x180001270) walks
// [0, count) calling Release (COM vtable +0x10) on each, then MemFrees the
// block; slot 1 (0x180001300) sizes it through the generic Array::SetLength at
// 0x180001550. Slot 3 (0x180001450) returns owner->[0x14] + 1.
//
// The array is a LADDER when the owner is "banked" (Video::ConstantBuffer+0x19):
// buffers[i] is created with ByteWidth = owner->bankStride * (i + 1), so slot 2
// can bind the smallest buffer that still holds the payload. +0x10 is the index
// of the rung currently bound; slot 2 only re-issues VS/PSSetConstantBuffers
// when it changes. (Proven by 0x180001300 and 0x180001460 — lane L1.)
struct ConstantBufferData {
    void*   vptr;           // +0x00
    void*   constantBuffer; // +0x08  Video::ConstantBuffer*
    int32_t activeIndex;    // +0x10  index into buffers[] currently bound; factory inits to -1
    int32_t pad14;
    void*   buffers;        // +0x18  ID3D11Buffer*[]
    int32_t bufferCount;    // +0x20
    int32_t bufferCapacity; // +0x24
};
static_assert(sizeof(ConstantBufferData) == 0x28, "ConstantBufferData size");
static_assert(offsetof(ConstantBufferData, activeIndex)    == 0x10, "ConstantBufferData::activeIndex");
static_assert(offsetof(ConstantBufferData, buffers)        == 0x18, "ConstantBufferData::buffers");
static_assert(offsetof(ConstantBufferData, bufferCount)    == 0x20, "ConstantBufferData::bufferCount");
static_assert(offsetof(ConstantBufferData, bufferCapacity) == 0x24, "ConstantBufferData::bufferCapacity");

// 0x20, vtable 0x1800187c8 (7 slots), factory 0x1800057a0
//
// Slot 5 (0x180003380) is IASetIndexBuffer(this->buffer, DXGI_FORMAT_R16_UINT,
// 0) — so indices are always 16-bit, and +0x10 is the ID3D11Buffer.
// Slots 3 and 4 are shared verbatim with VertexBufferData (0x180003290 /
// 0x180003340): the two classes have identical Lock/Unlock code.
//
// +0x18 / +0x1c recovered from the shared Lock/Unlock (lane L1):
//   +0x18  byte offset the NEXT Lock will hand out. Lock maps
//          WRITE_NO_OVERWRITE and appends while it fits, WRITE_DISCARD and
//          rewinds to 0 when it does not — a classic dynamic ring buffer. The
//          cursor advances by the request rounded up to an even byte count.
//   +0x1c  bool, 1 between Lock and Unlock. Nothing in the DLL reads it; it
//          exists for the Runtime-side base class / debug assertions.
struct IndexBufferData {
    void*   vptr;        // +0x00
    void*   indexBuffer; // +0x08  Video::IndexBuffer*
    void*   buffer;      // +0x10  ID3D11Buffer*
    int32_t writeCursor; // +0x18  next Lock offset in bytes
    uint8_t locked;      // +0x1c  (factory writes a byte here, not a dword)
    uint8_t pad1d[3];
};
static_assert(sizeof(IndexBufferData) == 0x20, "IndexBufferData size");
static_assert(offsetof(IndexBufferData, buffer)      == 0x10, "IndexBufferData::buffer");
static_assert(offsetof(IndexBufferData, writeCursor) == 0x18, "IndexBufferData::writeCursor");
static_assert(offsetof(IndexBufferData, locked)      == 0x1c, "IndexBufferData::locked");

// 0x20, vtable 0x180018830 (7 slots), factory 0x180005810
// Same shape as IndexBufferData, and slots 3/4 are literally the same code.
struct VertexBufferData {
    void*   vptr;         // +0x00
    void*   vertexBuffer; // +0x08  Video::VertexBuffer*
    void*   buffer;       // +0x10  ID3D11Buffer*
    int32_t writeCursor;  // +0x18  next Lock offset in bytes
    uint8_t locked;       // +0x1c
    uint8_t pad1d[3];
};
static_assert(sizeof(VertexBufferData) == 0x20, "VertexBufferData size");
static_assert(offsetof(VertexBufferData, buffer)      == 0x10, "VertexBufferData::buffer");
static_assert(offsetof(VertexBufferData, writeCursor) == 0x18, "VertexBufferData::writeCursor");
static_assert(offsetof(VertexBufferData, locked)      == 0x1c, "VertexBufferData::locked");

// Slots 3 (0x180003290, Lock) and 4 (0x180003340, Unlock) are the SAME two
// addresses in both vtables — the shipped DLL has exactly one copy of the code
// and both classes dispatch to it. dx11/src/buffers.cpp likewise has exactly
// one implementation of each, typed on IndexBufferData and registered for both
// vtables. These asserts are what makes that legal: if a future edit moves a
// field in one struct and not the other, the build breaks here instead of
// corrupting vertex buffers at runtime.
static_assert(sizeof(VertexBufferData) == sizeof(IndexBufferData),
              "VertexBufferData/IndexBufferData must stay layout-identical (shared slots 3/4)");
static_assert(offsetof(VertexBufferData, buffer) == offsetof(IndexBufferData, buffer),
              "shared slot 3/4: buffer offset must match");
static_assert(offsetof(VertexBufferData, writeCursor) == offsetof(IndexBufferData, writeCursor),
              "shared slot 3/4: writeCursor offset must match");
static_assert(offsetof(VertexBufferData, locked) == offsetof(IndexBufferData, locked),
              "shared slot 3/4: locked offset must match");

// 0x18, vtable 0x180018878 (3 slots), factory 0x180005880
// Slot 1 (0x180003fc0) is IASetInputLayout(this->inputLayout).
struct VertexLayoutData {
    void* vptr;         // +0x00
    void* vertexLayout; // +0x08  Video::VertexLayout*
    void* inputLayout;  // +0x10  ID3D11InputLayout* (built with the "TEXCOORD" semantic string)
};
static_assert(sizeof(VertexLayoutData) == 0x18, "VertexLayoutData size");
static_assert(offsetof(VertexLayoutData, inputLayout) == 0x10, "VertexLayoutData::inputLayout");

// 0x30, vtable 0x180018890 (3 slots), factory 0x1800058f0
//
// Slot 2 (0x180004230) is the bind:
//     VSSetShader(this->variants[i] ? this->variants[i] : this->variants[0],
//                 nullptr, 0)
// i.e. a 4-entry array of shader-permutation objects at +0x10, with index 0 as
// the fallback when the requested permutation was never compiled.
struct VertexProgramData {
    void* vptr;          // +0x00
    void* vertexProgram; // +0x08  Video::VertexProgram*
    void* variants[4];   // +0x10  ID3D11VertexShader*[4]
};
static_assert(sizeof(VertexProgramData) == 0x30, "VertexProgramData size");
static_assert(offsetof(VertexProgramData, variants) == 0x10, "VertexProgramData::variants");

// 0x30, vtable 0x180018800 (3 slots), factory 0x180005960
// Mirror of VertexProgramData; slot 2 (0x1800036a0) calls PSSetShader.
struct PixelProgramData {
    void* vptr;         // +0x00
    void* pixelProgram; // +0x08  Video::PixelProgram*
    void* variants[4];  // +0x10  ID3D11PixelShader*[4]
};
static_assert(sizeof(PixelProgramData) == 0x30, "PixelProgramData size");
static_assert(offsetof(PixelProgramData, variants) == 0x10, "PixelProgramData::variants");

// 0x28, vtable 0x180018818 (3 slots), factory 0x1800059d0
//
// The three COM slots are pinned in both directions, so there is no ambiguity
// about which is which:
//   slot 1 "Create" (0x180003760) writes them via
//     CreateBlendState        -> &this[0x10]   (device vtable +0xa0, @0x180003a3f)
//     CreateRasterizerState   -> &this[0x20]   (device vtable +0xb0, @0x180003a53)
//     CreateDepthStencilState -> &this[0x18]   (device vtable +0xa8, @0x180003a68)
//   slot 2 "Bind"   (0x180003a90) reads them back through the matching setters
//     OMSetBlendState(this[0x10], {0,0,0,0}, 0xFFFFFFFF)   (ctx +0x118)
//     OMSetDepthStencilState(this[0x18], renderState[0x4c])(ctx +0x120)
//     RSSetState(this[0x20])                               (ctx +0x158)
//
// slot 0 (0x1800036e0) releases them in *field* order — blend, depth-stencil,
// rasterizer — which is deliberately not the creation order.
//
// The +0x08 owner is a Video::RenderState; its field map (the source of every
// state-desc value) is documented in dx11/src/renderstatedata.cpp and in the
// "RenderState -> D3D11 state objects" section of ../../decomp/videodx11.md.
struct RenderStateData {
    void* vptr;              // +0x00
    void* renderState;       // +0x08  Video::RenderState*
    void* blendState;        // +0x10  ID3D11BlendState*
    void* depthStencilState; // +0x18  ID3D11DepthStencilState*
    void* rasterizerState;   // +0x20  ID3D11RasterizerState*
};
static_assert(sizeof(RenderStateData) == 0x28, "RenderStateData size");
static_assert(offsetof(RenderStateData, renderState)       == 0x08, "RenderStateData::renderState");
static_assert(offsetof(RenderStateData, blendState)        == 0x10, "RenderStateData::blendState");
static_assert(offsetof(RenderStateData, depthStencilState) == 0x18, "RenderStateData::depthStencilState");
static_assert(offsetof(RenderStateData, rasterizerState)   == 0x20, "RenderStateData::rasterizerState");

// RenderStateData vtable slot indices (vtable 0x180018818).
// Roles confirmed from the Runtime side, not inferred:
//   Video::RenderState::PlatformDestruct 0x1800f4e80 -> vtable[0](this, 1)
//   Video::RenderState::PlatformCreate   0x1800f4ea0 -> vtable[1]()
//   Video::RenderState::Bind(Context&)   0x1800f4ec0 -> vtable[2]()
enum RenderStateDataSlot : int {
    kRS_Dtor   = 0,  // 0x1800036e0  scalar deleting dtor
    kRS_Create = 1,  // 0x180003760  build the three D3D11 state objects
    kRS_Bind   = 2,  // 0x180003a90  OMSetBlendState / OMSetDepthStencilState / RSSetState
    kRenderStateDataSlotCount = 3
};

// 0x10, vtable 0x1800185a8 (3 slots), factory 0x180005a40 — no private fields.
// Slots 1 and 2 are the shared `return true` / `ret` stubs, so only the dtor is
// real: this class exists purely to satisfy the Runtime-side interface.
struct VideoProgramData {
    void* vptr;         // +0x00
    void* videoProgram; // +0x08  Video::VideoProgram*
};
static_assert(sizeof(VideoProgramData) == 0x10, "VideoProgramData size");

// 0x10, vtable 0x180018588 (4 slots), factory 0x180005aa0 — no private fields.
struct RenderBufferData {
    void* vptr;         // +0x00
    void* renderBuffer; // +0x08  Video::RenderBuffer*
};
static_assert(sizeof(RenderBufferData) == 0x10, "RenderBufferData size");

// ============================================================
// Vtable slot counts, walked from .rdata 0x180018580-0x180018c60 on
// 2026-07-24. The runtime seam (dx11/funcs/dx11_seam.cpp) re-verifies every
// one of these 84 slots against the live DLL at install time.
// ============================================================
namespace kSlots {
constexpr int kDeviceData         = 20;
constexpr int kContextData        = 20;
constexpr int kImageBufferData    = 7;
constexpr int kConstantBufferData = 4;
constexpr int kIndexBufferData    = 7;
constexpr int kVertexBufferData   = 7;
constexpr int kVertexLayoutData   = 3;
constexpr int kVertexProgramData  = 3;
constexpr int kPixelProgramData   = 3;
constexpr int kRenderStateData    = 3;
constexpr int kVideoProgramData   = 3;
constexpr int kRenderBufferData   = 4;
constexpr int kTotal = kDeviceData + kContextData + kImageBufferData +
                       kConstantBufferData + kIndexBufferData +
                       kVertexBufferData + kVertexLayoutData +
                       kVertexProgramData + kPixelProgramData +
                       kRenderStateData + kVideoProgramData +
                       kRenderBufferData;
static_assert(kTotal == 84, "84 vtable slots across the 12 classes");
}  // namespace kSlots

} // namespace VideoDX11
