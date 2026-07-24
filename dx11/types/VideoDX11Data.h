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

    void*     unk10;             // +0x10  COM interface (ID3D11Device or DeviceContext — TBD)
    void*     swapChain;         // +0x18  IDXGISwapChain* (SetFullscreenState(FALSE) before release)
    void*     unk20;             // +0x20  COM interface, released on shutdown
    void*     unk28;             // +0x28  COM interface, released on shutdown
    void*     unk30;             // +0x30  COM interface, released on shutdown
    void*     unk38;             // +0x38  COM interface, released on shutdown

    uint8_t   wasFullscreen;     // +0x40  mirrored into Application::_singleton+0x40 on shutdown
    uint8_t   pad41[7];          // +0x41

    void*     cursor;            // +0x48  HCURSOR
    void*     cursorImage;       // +0x50  Video::ImageBuffer* the cursor was built from
    int32_t   cursorHotspotX;    // +0x58
    int32_t   cursorHotspotY;    // +0x5c

    void*     interfaces;        // +0x60  Video::Device::GraphicsInterface[] (stride 0x18)
    int32_t   interfaceCount;    // +0x68
    int32_t   interfaceCapacity; // +0x6c
};
static_assert(sizeof(DeviceData) == 0x70, "DeviceData size (MemAlloc 0x70 @ 0x1800011a0)");
static_assert(offsetof(DeviceData, swapChain)      == 0x18, "DeviceData::swapChain");
static_assert(offsetof(DeviceData, wasFullscreen)  == 0x40, "DeviceData::wasFullscreen");
static_assert(offsetof(DeviceData, cursor)         == 0x48, "DeviceData::cursor");
static_assert(offsetof(DeviceData, cursorImage)    == 0x50, "DeviceData::cursorImage");
static_assert(offsetof(DeviceData, cursorHotspotX) == 0x58, "DeviceData::cursorHotspotX");
static_assert(offsetof(DeviceData, interfaces)     == 0x60, "DeviceData::interfaces");
static_assert(offsetof(DeviceData, interfaceCount) == 0x68, "DeviceData::interfaceCount");

// DeviceData vtable slot indices — see ../README.md status board.
enum DeviceDataSlot : int {
    kSlot_Dtor                    = 0,   // 0x180004270
    kSlot_EnumerateInterfaces     = 1,   // 0x180004690
    kSlot_CreateDevice            = 2,   // 0x180004e10
    kSlot_Shutdown                = 3,   // 0x1800051c0
    kSlot_Unknown4                = 4,   // 0x180001190
    kSlot_Unknown5                = 5,   // 0x180005430
    kSlot_Unknown6                = 6,   // 0x180005440
    kSlot_Unknown7                = 7,   // 0x180001060
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
// Only the sizes and the zero-init pattern from each factory are proven so
// far; the per-class vtables have not been walked. Fields stay raw until a
// vtable method is decompiled that pins them down.
// ============================================================

// 0x20, vtable 0x1800186f0, factory 0x180005650
struct ContextData {
    void*   vptr;    // +0x00
    void*   context; // +0x08  Video::Context*
    int32_t unk10;   // +0x10  zeroed by factory
    int32_t pad14;
    void*   unk18;   // +0x18  zeroed by factory
};
static_assert(sizeof(ContextData) == 0x20, "ContextData size");

// 0x38, vtable 0x180018790, factory 0x1800056c0
struct ImageBufferData {
    void* vptr;        // +0x00
    void* imageBuffer; // +0x08  Video::ImageBuffer*
    void* unk10;       // +0x10  (texture / SRV / sampler / staging — TBD)
    void* unk18;       // +0x18
    void* unk20;       // +0x20
    void* unk28;       // +0x28
    void* unk30;       // +0x30
};
static_assert(sizeof(ImageBufferData) == 0x38, "ImageBufferData size");

// 0x28, vtable 0x1800185d0, factory 0x180005730
struct ConstantBufferData {
    void*   vptr;          // +0x00
    void*   constantBuffer;// +0x08  Video::ConstantBuffer*
    int32_t unk10;         // +0x10  initialized to 0xFFFFFFFF (a slot/binding index?)
    int32_t pad14;
    void*   unk18;         // +0x18
    void*   unk20;         // +0x20
};
static_assert(sizeof(ConstantBufferData) == 0x28, "ConstantBufferData size");

// 0x20, vtable 0x1800187c8, factory 0x1800057a0
struct IndexBufferData {
    void*   vptr;        // +0x00
    void*   indexBuffer; // +0x08  Video::IndexBuffer*
    void*   unk10;       // +0x10  ID3D11Buffer*
    int32_t unk18;       // +0x18
    uint8_t unk1c;       // +0x1c  (factory writes a byte here, not a dword)
    uint8_t pad1d[3];
};
static_assert(sizeof(IndexBufferData) == 0x20, "IndexBufferData size");

// 0x20, vtable 0x180018830, factory 0x180005810 — same shape as IndexBufferData
struct VertexBufferData {
    void*   vptr;         // +0x00
    void*   vertexBuffer; // +0x08  Video::VertexBuffer*
    void*   unk10;        // +0x10  ID3D11Buffer*
    int32_t unk18;        // +0x18
    uint8_t unk1c;        // +0x1c
    uint8_t pad1d[3];
};
static_assert(sizeof(VertexBufferData) == 0x20, "VertexBufferData size");

// 0x18, vtable 0x180018878, factory 0x180005880
struct VertexLayoutData {
    void* vptr;         // +0x00
    void* vertexLayout; // +0x08  Video::VertexLayout*
    void* inputLayout;  // +0x10  ID3D11InputLayout* (uses the "TEXCOORD" semantic string)
};
static_assert(sizeof(VertexLayoutData) == 0x18, "VertexLayoutData size");

// 0x30, vtable 0x180018890, factory 0x1800058f0
struct VertexProgramData {
    void* vptr;          // +0x00
    void* vertexProgram; // +0x08  Video::VertexProgram*
    void* unk10;         // +0x10  ID3D11VertexShader* / bytecode blob
    void* unk18;         // +0x18
    void* unk20;         // +0x20
    void* unk28;         // +0x28
};
static_assert(sizeof(VertexProgramData) == 0x30, "VertexProgramData size");

// 0x30, vtable 0x180018800, factory 0x180005960 — same shape as VertexProgramData
struct PixelProgramData {
    void* vptr;         // +0x00
    void* pixelProgram; // +0x08  Video::PixelProgram*
    void* unk10;        // +0x10  ID3D11PixelShader* / bytecode blob
    void* unk18;        // +0x18
    void* unk20;        // +0x20
    void* unk28;        // +0x28
};
static_assert(sizeof(PixelProgramData) == 0x30, "PixelProgramData size");

// 0x28, vtable 0x180018818, factory 0x1800059d0
struct RenderStateData {
    void* vptr;        // +0x00
    void* renderState; // +0x08  Video::RenderState*
    void* unk10;       // +0x10  ID3D11RasterizerState* / BlendState* / DepthStencilState*
    void* unk18;       // +0x18
    void* unk20;       // +0x20
};
static_assert(sizeof(RenderStateData) == 0x28, "RenderStateData size");

// 0x10, vtable 0x1800185a8, factory 0x180005a40 — no DX11-private fields at all
struct VideoProgramData {
    void* vptr;         // +0x00
    void* videoProgram; // +0x08  Video::VideoProgram*
};
static_assert(sizeof(VideoProgramData) == 0x10, "VideoProgramData size");

// 0x10, vtable 0x180018588, factory 0x180005aa0 — no DX11-private fields at all
struct RenderBufferData {
    void* vptr;         // +0x00
    void* renderBuffer; // +0x08  Video::RenderBuffer*
};
static_assert(sizeof(RenderBufferData) == 0x10, "RenderBufferData size");

} // namespace VideoDX11
