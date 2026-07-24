// VideoDX11 `ImageBufferData` — all seven vtable slots (0x180018790).
//
// Lane L3. This is the largest of the eleven resource classes (0x38 bytes, five
// COM interfaces) and the only one that touches texture creation, formats, mip
// levels, cube maps, compressed data, render targets and depth buffers.
//
// Contents
// --------
//   0x180002400  slot 0  scalar deleting dtor
//   0x180002430  (private) ~ImageBufferData — untrack + release five interfaces
//   0x180002580  slot 1  alias the DeviceData color buffer, build an RTV
//   0x180002650  slot 2  alias the DeviceData depth buffer, build a DSV
//   0x180002870  slot 3  create the texture and all its views
//   0x180002720  (private) create the sampler state — called by slot 3
//   0x180002da0  slot 4  bind sampler + SRV to a pixel-shader slot
//   0x180002e10  slot 5  upload pixels (dynamic Map, or UpdateSubresource)
//   0x180002ef0  slot 6  read pixels back through a STAGING texture
//   0x180002300  (shared with DeviceData) TrackImageBuffer — see below
//
// 0x180002300 is DeviceData's, not ours — but it lives here
// ---------------------------------------------------------
// `DeviceData::TrackImageBuffer(Video::ImageBuffer*)` appends (add-if-absent)
// to the array at DeviceData+0x60 `{void* data; int count; int capacity}`,
// stride 8, x3 growth. That array is the registry of Video::ImageBuffers that
// must be released and recreated across a device / swap-chain reset.
//
// It is called from ImageBufferData slots 1, 2 and 3, and the matching *remove*
// lives inside ~ImageBufferData (0x180002430) — i.e. three of the four call
// sites are in this file. So it is implemented here to keep these slots
// self-contained. **Lane L6 owns DeviceData and will need exactly this helper**
// (DeviceData slot 0 frees the array at teardown); when L6 lands, hoist
// TrackImageBuffer into the DeviceData translation unit and have this file call
// it instead. Until then there is one copy and it is here.
//
// Note that the *remove* in ~ImageBufferData is a **stable shift-erase**, not a
// swap-remove — dx11/README.md and decomp/videodx11.md both say "swap-remove",
// which is wrong. 0x1800024c8-0x1800024ee shifts every later element down by one
// (`MOV RCX,[R8+8]; MOV [R8],RCX`) and only then decrements the count. Corrected
// here; the two docs still carry the old wording.
//
// Membership differs between the two sites and that asymmetry is real:
//   ~ImageBufferData (remove):  IsGenerated() || (flags & 0x70) || IsRendered()
//   slot 3           (add):     IsGenerated() || IsRendered() || (flags & 0x40)
// The dtor also honours bits 4 and 5 (set by slots 1 and 2, which add
// unconditionally); slot 3 only honours bit 6. Reproduced verbatim.
//
// Reaching D3D11
// --------------
// ImageBufferData never caches the device. Every slot walks:
//
//     Video::Device::_singleton  ->  Video::Device*
//        [+0x100]                ->  active Video::DeviceData*
//        [+0x20]                 ->  ID3D11Device*
//        [+0x28]                 ->  ID3D11DeviceContext*
//
// The sampler helper additionally reads `Video::Configuration::_singleton`
// (a *different* import — 0x180013480 vs 0x1800134b8; Ghidra renders both as
// `_singleton_exref`, which is how the anisotropy field got misattributed at
// first glance). Configuration+0x28 is the requested anisotropy, Device+0x98 is
// the hardware cap.
//
// COM vtable indices
// ------------------
// Every call below goes through the mingw-w64 <d3d11.h> C++ interface rather
// than a hand-rolled offset, and every one was checked against the byte offset
// in the shipped code:
//
//   ID3D11Device        +0x28 = index  5 = CreateTexture2D          (0x180002c46)
//                       +0x38 = index  7 = CreateShaderResourceView (0x180002cf6)
//                       +0x48 = index  9 = CreateRenderTargetView   (0x18000262a)
//                       +0x50 = index 10 = CreateDepthStencilView   (0x180002d47)
//                       +0xb8 = index 23 = CreateSamplerState       (0x18000285b)
//   ID3D11DeviceContext +0x40 = index  8 = PSSetShaderResources     (0x180002e00)
//                       +0x50 = index 10 = PSSetSamplers            (0x180002ddc)
//                       +0x70 = index 14 = Map                      (0x180002e79)
//                       +0x78 = index 15 = Unmap                    (0x180002ea1)
//                       +0x178= index 47 = CopyResource             (0x180002f9a)
//                       +0x180= index 48 = UpdateSubresource        (0x180002ec7)
//   IUnknown            +0x08 = index  1 = AddRef                   (0x180002612)
//                       +0x10 = index  2 = Release                  (0x180002513)
//
// MinGW GCC lays out a single-inheritance pure-abstract COM interface exactly
// as MSVC does on Win64, so `dev->CreateTexture2D(...)` emits `call [rax+0x28]`.
//
// Video::ImageBuffer field map (read directly, offsets from the shipped code)
// --------------------------------------------------------------------------
//   +0x20 int    width          (also the mip-chain restart width in slot 3)
//   +0x24 int    height
//   +0x28 int    dimension      0 = 2D, non-zero (1) = cube  -> ArraySize 6,
//                               MiscFlags TEXTURECUBE, SRV dimension TEXTURECUBE
//   +0x2c int    kind           drives BindFlags / Usage / CPUAccessFlags:
//                                 1     -> BIND_DEPTH_STENCIL
//                                 0,4,5 -> BIND_RENDER_TARGET
//                                 3     -> USAGE_DYNAMIC + CPU_ACCESS_WRITE
//                                 2 (and anything with a bit outside 0x5 set)
//                                       -> no extra bind flag
//                               The names of these five values are NOT
//                               recoverable from this DLL — the arithmetic is
//                               reproduced literally rather than guessed at.
//   +0x30 int    format         index into the two 8-entry tables below
//   +0x34 uint   flags          bit0 = mipmapped, bit4/bit5 = set by slots 1/2,
//                               bit6 = comparison/shadow sampler
//   +0x38 int    addressU       index into {WRAP, CLAMP}
//   +0x3c int    addressV
//   +0x40 int    addressW
//   +0x44 int    filter         index into {POINT, LINEAR, ANISOTROPIC}
//
// Engine format enum -> DXGI_FORMAT (0x1800029e0..0x180002a11, and again at
// 0x180002c89 for the cube SRV; both tables are byte-identical), with the
// companion bytes-per-pixel-or-block table at 0x180002adc..0x180002b07:
//
//   idx | DXGI                                    | bytes/px-or-block
//   ----+-----------------------------------------+------------------
//    0  | 0x47 = 71 DXGI_FORMAT_BC1_UNORM         |  8   (block)
//    1  | 0x4d = 77 DXGI_FORMAT_BC3_UNORM         | 16   (block)
//    2  | 0x4d = 77 DXGI_FORMAT_BC3_UNORM         | 16   (block)
//    3  | 0x41 = 65 DXGI_FORMAT_A8_UNORM          |  1
//    4  | 0x3d = 61 DXGI_FORMAT_R8_UNORM          |  1
//    5  | 0x57 = 87 DXGI_FORMAT_B8G8R8A8_UNORM    |  4
//    6  | 0x29 = 41 DXGI_FORMAT_R32_TYPELESS      |  4
//    7  | 0x2c = 44 DXGI_FORMAT_R24G8_TYPELESS    |  4
//
// Index 5 is the backbuffer format (slot 1 stamps format=5) and index 7 is the
// depth format (slot 2 stamps format=7) — those two are confirmed against the
// DeviceData buffer creators. Indices 1 and 2 BOTH map to BC3; index 1 was very
// likely DXT3/BC2 in the engine enum and got folded onto BC3 in this backend.
// That is an observation about the shipped table, not a guess: the two entries
// really are the same immediate. Reproduced as-is.
//
// Standard: observable-accurate
// ----------------------------
// Identical D3D11/DXGI calls, identical arguments, identical order, identical
// struct layout. Where the original omits an error check, so does this. Where
// it releases in an odd order, so does this. Two things are deliberately NOT
// invented and are called out at their use sites:
//   * the second parameter of slot 4 (never read by the original),
//   * the meaning of the 16-byte level descriptors slot 3 receives (only the
//     leading pointer is read).
//
// See ../README.md for the seam, and ../types/VideoDX11Data.h for the layout.

#include "dx11_seam.h"
#include "VideoDX11Data.h"
#include "resolve.h"

#include <windows.h>
#include <d3d11.h>

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace VideoDX11 {
namespace {

// ============================================================
// Runtime-x64.dll imports.
//
// Resolved lazily in this translation unit only — deliberately NOT added to
// engine/funcs/engine_funcs.h, which the other five lanes are also editing.
// Mangled names read out of VideoDX11's own import-name table (RVAs 0x1c106+,
// IAT 0x180013398..0x1800134d0), so they are exactly the strings the shipped
// DLL resolves.
// ============================================================
struct Imports {
    int   (*Width)(void*);           // ?Width@ImageBuffer@Video@@QEBAHXZ
    int   (*Height)(void*);          // ?Height@ImageBuffer@Video@@QEBAHXZ
    bool  (*IsDynamic)(void*);       // ?IsDynamic@ImageBuffer@Video@@QEBA_NXZ
    bool  (*IsCompressed)(void*);    // ?IsCompressed@ImageBuffer@Video@@QEBA_NXZ
    bool  (*IsRenderBuffer)(void*);  // ?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ
    bool  (*IsDepthBuffer)(void*);   // ?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ
    int   (*GetLevelCount)(void*);   // ?GetLevelCount@ImageBuffer@Video@@QEBAHXZ
    bool  (*IsRendered)(void*);      // ?IsRendered@ImageBuffer@Video@@QEBA_NXZ
    bool  (*IsGenerated)(void*);     // ?IsGenerated@ImageBuffer@Video@@QEBA_NXZ
    void  (*BaseDtor)(void*);        // ??1ImageBufferData@Video@@UEAA@XZ
    void* (*MemAlloc)(uint64_t, int);// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z
    void  (*MemFree)(void*);         // ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
    void** deviceSingleton;          // ?_singleton@Device@Video@@0PEAV12@EA
    void** configSingleton;          // ?_singleton@Configuration@Video@@0PEAV12@EA
};

Imports  g_im;
bool     g_imResolved = false;
bool     g_imOk       = false;

// Returns nullptr when any import is missing. Every slot then forwards to the
// original code rather than dereferencing a null — the seam keeps the shipped
// implementation available for exactly this.
const Imports* GetImports() {
    if (!g_imResolved) {
        g_imResolved = true;
        std::memset(&g_im, 0, sizeof(g_im));

        g_im.Width          = (int  (*)(void*))ResolveExport("?Width@ImageBuffer@Video@@QEBAHXZ");
        g_im.Height         = (int  (*)(void*))ResolveExport("?Height@ImageBuffer@Video@@QEBAHXZ");
        g_im.IsDynamic      = (bool (*)(void*))ResolveExport("?IsDynamic@ImageBuffer@Video@@QEBA_NXZ");
        g_im.IsCompressed   = (bool (*)(void*))ResolveExport("?IsCompressed@ImageBuffer@Video@@QEBA_NXZ");
        g_im.IsRenderBuffer = (bool (*)(void*))ResolveExport("?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ");
        g_im.IsDepthBuffer  = (bool (*)(void*))ResolveExport("?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ");
        g_im.GetLevelCount  = (int  (*)(void*))ResolveExport("?GetLevelCount@ImageBuffer@Video@@QEBAHXZ");
        g_im.IsRendered     = (bool (*)(void*))ResolveExport("?IsRendered@ImageBuffer@Video@@QEBA_NXZ");
        g_im.IsGenerated    = (bool (*)(void*))ResolveExport("?IsGenerated@ImageBuffer@Video@@QEBA_NXZ");
        g_im.BaseDtor       = (void (*)(void*))ResolveExport("??1ImageBufferData@Video@@UEAA@XZ");
        g_im.MemAlloc       = (void* (*)(uint64_t, int))ResolveExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
        g_im.MemFree        = (void  (*)(void*))ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
        g_im.deviceSingleton = (void**)ResolveExport("?_singleton@Device@Video@@0PEAV12@EA");
        g_im.configSingleton = (void**)ResolveExport("?_singleton@Configuration@Video@@0PEAV12@EA");

        const void* const* p = reinterpret_cast<const void* const*>(&g_im);
        g_imOk = true;
        for (size_t i = 0; i < sizeof(Imports) / sizeof(void*); ++i)
            if (p[i] == nullptr) g_imOk = false;
    }
    return g_imOk ? &g_im : nullptr;
}

// ============================================================
// Field accessors — raw offsets, no invented struct
// ============================================================
inline int32_t  IbI32(void* ib, size_t off) { return *reinterpret_cast<int32_t*> (static_cast<uint8_t*>(ib) + off); }
inline uint32_t IbU32(void* ib, size_t off) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ib) + off); }
inline void     IbSet(void* ib, size_t off, int32_t v) { *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ib) + off) = v; }

// Video::Device* (never null once the video interface exists — the shipped code
// does not check either).
inline void* VideoDevice(const Imports* im) { return *im->deviceSingleton; }

// Active Video::DeviceData* at Video::Device+0x100.
inline DeviceData* ActiveDeviceData(void* device) {
    return *reinterpret_cast<DeviceData**>(static_cast<uint8_t*>(device) + 0x100);
}

inline ID3D11Device*        D3DDevice (DeviceData* dd) { return static_cast<ID3D11Device*>(dd->d3dDevice); }
inline ID3D11DeviceContext* D3DContext(DeviceData* dd) { return static_cast<ID3D11DeviceContext*>(dd->d3dContext); }

// Engine format -> DXGI_FORMAT. See the header comment for the derivation.
const int kFormatToDxgi[8] = { 0x47, 0x4d, 0x4d, 0x41, 0x3d, 0x57, 0x29, 0x2c };

// Engine format -> bytes per pixel (uncompressed) or per 4x4 block (BC).
const int kFormatBytes[8]  = {    8,   16,   16,    1,    1,    4,    4,    4 };

// ============================================================
// 0x180002300 — DeviceData::TrackImageBuffer(Video::ImageBuffer*)
//
// Add-if-absent into DeviceData+0x60 {data, count, capacity}, stride 8, x3
// growth. NOTE FOR LANE L6: this belongs to DeviceData; hoist it when
// DeviceData lands (see the header comment).
// ============================================================
void TrackImageBuffer(DeviceData* dd, void* imageBuffer, const Imports* im) {
    const int   oldCount = dd->trackedCount;
    void* const oldData  = dd->trackedImages;

    // Linear scan; present -> nothing to do. (The original also has an
    // `if (index > -1)` guard on the hit, which is unconditionally true.)
    if (oldCount > 0) {
        void** p = static_cast<void**>(oldData);
        for (int i = 0; i < oldCount; ++i)
            if (p[i] == imageBuffer) return;
    }

    // Array::SetLength(oldCount + 1), inlined by the original with stride 8.
    const int newCount = oldCount + 1;
    dd->trackedCount = newCount;
    if (dd->trackedCapacity < newCount) {
        dd->trackedCapacity = newCount;
        if (oldCount != 0) dd->trackedCapacity = newCount * 3;
        dd->trackedImages = im->MemAlloc(static_cast<uint64_t>(dd->trackedCapacity) * 8, 8);
    }

    // If the block was reused, only the tail needs filling; if it moved, the
    // whole range is copied across and the new element lands at the end.
    const int start = (oldData == dd->trackedImages) ? oldCount : 0;
    for (int i = start; i < newCount; ++i) {
        void** dst = static_cast<void**>(dd->trackedImages) + i;
        if (dst != nullptr)
            *dst = (i < oldCount) ? static_cast<void**>(oldData)[i] : imageBuffer;
    }

    if (oldData != dd->trackedImages) im->MemFree(oldData);
}

// The matching stable shift-erase, inlined into ~ImageBufferData at
// 0x1800024b2-0x180002505. Removes every occurrence (there is at most one).
void UntrackImageBuffer(DeviceData* dd, void* imageBuffer) {
    for (int i = 0; i < dd->trackedCount; ++i) {
        if (static_cast<void**>(dd->trackedImages)[i] != imageBuffer) continue;
        for (int j = i + 1; j < dd->trackedCount; ++j) {
            void** slot = static_cast<void**>(dd->trackedImages) + (j - 1);
            if (slot != nullptr) *slot = slot[1];
        }
        dd->trackedCount -= 1;
        --i;
    }
}

// ============================================================
// Release the five interfaces in the shipped order and null them.
// Shared verbatim by ~ImageBufferData (0x180002507) and slot 3 (0x18000292a).
// The order is +0x28, +0x20, +0x30, +0x10, +0x18 — NOT declaration order.
// ============================================================
void ReleaseInterfaces(ImageBufferData* self) {
    if (self->depthStencilView)   static_cast<IUnknown*>(self->depthStencilView)->Release();
    if (self->renderTargetView)   static_cast<IUnknown*>(self->renderTargetView)->Release();
    if (self->shaderResourceView) static_cast<IUnknown*>(self->shaderResourceView)->Release();
    if (self->sampler)            static_cast<IUnknown*>(self->sampler)->Release();
    if (self->texture)            static_cast<IUnknown*>(self->texture)->Release();

    self->depthStencilView   = nullptr;
    self->renderTargetView   = nullptr;
    self->shaderResourceView = nullptr;
    self->sampler            = nullptr;
    self->texture            = nullptr;
}

// ============================================================
// 0x180002720 — create the sampler state (private; called by slot 3 only,
// including on the CreateTexture2D failure path).
// ============================================================
void CreateSamplerState(ImageBufferData* self, const Imports* im) {
    void* const ib = self->imageBuffer;

    // Both tables are stack locals in the original and are *patched* before
    // being indexed, which is why they are locals here too.
    int addressTable[2] = { D3D11_TEXTURE_ADDRESS_WRAP,       // 1
                            D3D11_TEXTURE_ADDRESS_CLAMP };    // 3
    int filterTable[3]  = { D3D11_FILTER_MIN_MAG_MIP_POINT,   // 0x00
                            D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, // 0x14
                            D3D11_FILTER_ANISOTROPIC };       // 0x55

    int aniso = *reinterpret_cast<int*>(static_cast<uint8_t*>(*im->configSingleton) + 0x28);
    const uint32_t flags = IbU32(ib, 0x34);
    const bool comparison = ((flags >> 6) & 1) != 0;

    if (comparison) {
        // Shadow sampler: the LINEAR entry becomes the comparison filter and
        // anisotropy is forced off. The ANISOTROPIC entry is left at 0x55.
        filterTable[1] = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // 0x95
        aniso = 0;
    } else if (aniso == -1) {
        filterTable[2] = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;      // 0x14
        aniso = 0;
    } else if (aniso == 0) {
        filterTable[2] = D3D11_FILTER_MIN_MAG_MIP_LINEAR;            // 0x15
        aniso = 0;
    } else if (aniso < 0) {
        aniso = 0;                       // aniso < -1: nothing patched, just off
    }
    // aniso > 0: table stays ANISOTROPIC and the value is kept.

    void* const device = VideoDevice(im);
    const int maxAniso = *reinterpret_cast<int*>(static_cast<uint8_t*>(device) + 0x98);
    if (aniso > maxAniso) aniso = maxAniso;

    D3D11_SAMPLER_DESC desc;
    desc.Filter         = static_cast<D3D11_FILTER>(filterTable[IbI32(ib, 0x44)]);
    desc.AddressU       = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(addressTable[IbI32(ib, 0x38)]);
    desc.AddressV       = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(addressTable[IbI32(ib, 0x3c)]);
    desc.AddressW       = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(addressTable[IbI32(ib, 0x40)]);
    desc.MipLODBias     = 0.0f;
    desc.MaxAnisotropy  = static_cast<UINT>(aniso);
    // D3D11_COMPARISON_NEVER (1) normally, D3D11_COMPARISON_LESS (2) for shadow.
    desc.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>((comparison ? 1 : 0) + 1);
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD         = 0.0f;
    // 0x180018c50 = 1.0f (verified: bytes 00 00 80 3f).
    desc.MaxLOD         = (flags & 1) ? (static_cast<float>(im->GetLevelCount(ib)) - 1.0f) : 0.0f;

    D3DDevice(ActiveDeviceData(device))
        ->CreateSamplerState(&desc, reinterpret_cast<ID3D11SamplerState**>(&self->sampler));
}

// ============================================================
// 0x180002430 — ~ImageBufferData (private; tail-called by slot 0)
// ============================================================
void Destructor(ImageBufferData* self, const Imports* im) {
    // MSVC's "restore this class's vptr before running the dtor body". The
    // vtable is the live one in the shipped DLL — i.e. the seam's table, so a
    // later virtual call still reaches whichever slots we have installed.
    self->vptr = reinterpret_cast<void*>(ResolveAddr(Module::VideoDX11, 0x180018790));

    void* const ib = self->imageBuffer;

    // Untrack. Note bits 4|5|6 = 0x70 here, unlike slot 3 which only tests bit 6.
    if (im->IsGenerated(ib) || (IbU32(ib, 0x34) & 0x70) != 0 || im->IsRendered(ib))
        UntrackImageBuffer(ActiveDeviceData(VideoDevice(im)), ib);

    ReleaseInterfaces(self);

    im->BaseDtor(self);   // tail call in the original (JMP [0x1800133e0])
}

// ============================================================
// slot 0 — 0x180002400 — scalar deleting destructor
// ============================================================
void* ImageBufferData_ScalarDeletingDtor(ImageBufferData* self, unsigned int flags) {
    const Imports* im = GetImports();
    if (!im) {
        typedef void* (*orig_t)(ImageBufferData*, unsigned int);
        return reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 0))(self, flags);
    }
    Destructor(self, im);
    if (flags & 1) im->MemFree(self);
    return self;
}

// ============================================================
// slot 1 — 0x180002580 — alias the DeviceData color buffer
//
// Re-points the owning Video::ImageBuffer at the swap chain's render target:
// stamps the ImageBuffer's descriptor from the Video::Device resolution,
// AddRefs DeviceData::colorBuffer into +0x18, builds a default RTV, and joins
// the reset registry. No sampler and no SRV are created.
// ============================================================
void ImageBufferData_AttachColorBuffer(ImageBufferData* self) {
    const Imports* im = GetImports();
    if (!im) {
        typedef void (*orig_t)(ImageBufferData*);
        reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 1))(self);
        return;
    }

    void* const device = VideoDevice(im);
    DeviceData* const dd = ActiveDeviceData(device);
    void* const ib = self->imageBuffer;

    // Field write order copied exactly (0x1800025ab..0x180002600) — note 0x40
    // before 0x3c before 0x38, and flags (0x34) last.
    IbSet(ib, 0x20, *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x04)); // width
    IbSet(ib, 0x24, *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x08)); // height
    IbSet(ib, 0x28, 0);      // dimension: 2D
    IbSet(ib, 0x2c, 0);      // kind: render target
    IbSet(ib, 0x30, 5);      // format 5 = DXGI_FORMAT_B8G8R8A8_UNORM (the backbuffer)
    IbSet(ib, 0x40, 1);      // addressW = CLAMP
    IbSet(ib, 0x3c, 1);      // addressV = CLAMP
    IbSet(ib, 0x38, 1);      // addressU = CLAMP
    IbSet(ib, 0x44, 0);      // filter   = POINT
    IbSet(ib, 0x34, 0x10);   // flags: bit4 -> tracked for device reset

    ID3D11Device* const dev = D3DDevice(dd);
    static_cast<IUnknown*>(dd->colorBuffer)->AddRef();
    self->texture = dd->colorBuffer;
    dev->CreateRenderTargetView(static_cast<ID3D11Resource*>(self->texture), nullptr,
                                reinterpret_cast<ID3D11RenderTargetView**>(&self->renderTargetView));

    TrackImageBuffer(dd, ib, im);   // tail call in the original
}

// ============================================================
// slot 2 — 0x180002650 — alias the DeviceData depth buffer
// Mirror of slot 1 with the depth format, kind 1, flag bit 5, and a DSV.
// ============================================================
void ImageBufferData_AttachDepthBuffer(ImageBufferData* self) {
    const Imports* im = GetImports();
    if (!im) {
        typedef void (*orig_t)(ImageBufferData*);
        reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 2))(self);
        return;
    }

    void* const device = VideoDevice(im);
    DeviceData* const dd = ActiveDeviceData(device);
    void* const ib = self->imageBuffer;

    IbSet(ib, 0x20, *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x04));
    IbSet(ib, 0x24, *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(device) + 0x08));
    IbSet(ib, 0x28, 0);      // dimension: 2D
    IbSet(ib, 0x2c, 1);      // kind: depth buffer
    IbSet(ib, 0x30, 7);      // format 7 = DXGI_FORMAT_R24G8_TYPELESS (D24S8)
    IbSet(ib, 0x40, 1);
    IbSet(ib, 0x3c, 1);
    IbSet(ib, 0x38, 1);
    IbSet(ib, 0x44, 0);
    IbSet(ib, 0x34, 0x20);   // flags: bit5 -> tracked for device reset

    ID3D11Device* const dev = D3DDevice(dd);
    static_cast<IUnknown*>(dd->depthBuffer)->AddRef();
    self->texture = dd->depthBuffer;
    // Null desc: the DSV inherits the resource format. The resource here is the
    // DeviceData depth buffer, created as D24_UNORM_S8_UINT (typed, not
    // typeless), so a default view is legal.
    dev->CreateDepthStencilView(static_cast<ID3D11Resource*>(self->texture), nullptr,
                                reinterpret_cast<ID3D11DepthStencilView**>(&self->depthStencilView));

    TrackImageBuffer(dd, ib, im);
}

// ============================================================
// slot 3 — 0x180002870 — create the texture and all its views
// ============================================================

// The level descriptors slot 3 is handed. The array header is the engine's
// usual {data, count, capacity}; the elements are 0x10 bytes each and ONLY the
// leading pointer is ever read (0x180002ab4). The other 8 bytes are not touched
// by this DLL, so their meaning is undetermined and deliberately unnamed.
struct ImageLevel {
    void*    bits;    // +0x00  pixel data for this face/mip
    uint32_t unk08;   // +0x08  never read here
    uint32_t unk0c;   // +0x0c  never read here
};
static_assert(sizeof(ImageLevel) == 0x10, "ImageLevel stride (0x10 from the loop's ADD RCX,0x10)");

struct ImageLevelArray {
    void*   data;
    int32_t count;
    int32_t capacity;
};

// D3D11_SUBRESOURCE_DATA is the 0x10-byte element the original builds; the
// growable array it builds them in uses the same Array::SetLength shape.
static_assert(sizeof(D3D11_SUBRESOURCE_DATA) == 0x10, "D3D11_SUBRESOURCE_DATA stride");

struct SubresArray {
    D3D11_SUBRESOURCE_DATA* data;
    int32_t                 count;
    int32_t                 capacity;
};

// Array::SetLength(count+1) then fill, inlined by the original at
// 0x180002b18-0x180002bbe with stride 0x10. x3 growth, first alloc exactly 1.
void PushSubresource(SubresArray* arr, const D3D11_SUBRESOURCE_DATA& value, const Imports* im) {
    D3D11_SUBRESOURCE_DATA* const oldData = arr->data;
    const int oldCount = arr->count;
    const int newCount = oldCount + 1;

    arr->count = newCount;
    if (arr->capacity < newCount) {
        arr->capacity = newCount;
        if (oldCount != 0) arr->capacity = newCount * 3;
        arr->data = static_cast<D3D11_SUBRESOURCE_DATA*>(
            im->MemAlloc(static_cast<uint64_t>(arr->capacity) * 0x10, 8));
    }

    const int start = (oldData == arr->data) ? oldCount : 0;
    for (int i = start; i < newCount; ++i) {
        D3D11_SUBRESOURCE_DATA* dst = arr->data + i;
        if (dst != nullptr)
            *dst = (i < oldCount) ? oldData[i] : value;
    }

    if (oldData != arr->data) im->MemFree(oldData);
}

bool ImageBufferData_Create(ImageBufferData* self, const ImageLevelArray* levels) {
    const Imports* im = GetImports();
    if (!im) {
        typedef bool (*orig_t)(ImageBufferData*, const ImageLevelArray*);
        return reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 3))(self, levels);
    }

    void* const ib = self->imageBuffer;
    const int width  = im->Width(ib);
    const int height = im->Height(ib);

    // Degenerate, non-generated, non-rendered buffer: succeed and do nothing.
    if (!im->IsGenerated(ib) && !im->IsRendered(ib) && (width == 0 || height == 0))
        return true;

    DeviceData* const dd = ActiveDeviceData(VideoDevice(im));

    // Add to the reset registry. Only bit 6 here (the dtor also honours 4 and 5).
    if (im->IsGenerated(ib) || im->IsRendered(ib) || ((IbU32(ib, 0x34) >> 6) & 1) != 0)
        TrackImageBuffer(dd, ib, im);

    ReleaseInterfaces(self);

    // --- D3D11_TEXTURE2D_DESC ---------------------------------------------
    const uint32_t kind = IbU32(ib, 0x2c);

    // 0x180002992-0x1800029b6, reproduced literally. `kind & ~5` selects the
    // {0,1,4,5} group; 1 is then split off as the depth case.
    UINT bindFlags = 0;
    if ((kind & 0xfffffffau) == 0 && kind != 1) bindFlags = D3D11_BIND_RENDER_TARGET;  // 0x20
    if (kind == 1)                              bindFlags |= D3D11_BIND_DEPTH_STENCIL; // 0x40

    D3D11_TEXTURE2D_DESC desc;
    desc.Width  = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    // flags bit0 set (mipmapped) -> 0 = "generate the full chain".
    desc.MipLevels = static_cast<UINT>((~static_cast<uint8_t>(IbU32(ib, 0x34))) & 1);
    desc.ArraySize = (IbI32(ib, 0x28) == 0) ? 1 : 6;   // 6 = cube faces
    desc.Format = static_cast<DXGI_FORMAT>(kFormatToDxgi[IbI32(ib, 0x30)]);
    desc.SampleDesc.Count   = 1;   // textures are never multisampled here
    desc.SampleDesc.Quality = 0;
    desc.Usage = (kind == 3) ? D3D11_USAGE_DYNAMIC
                             : (levels != nullptr ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT);
    bindFlags |= D3D11_BIND_SHADER_RESOURCE;  // 0x8, always
    desc.BindFlags      = bindFlags;
    desc.CPUAccessFlags = (kind == 3) ? D3D11_CPU_ACCESS_WRITE : 0;   // 0x10000
    desc.MiscFlags      = (IbI32(ib, 0x28) != 0) ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;  // 0x4

    // --- initial data ------------------------------------------------------
    SubresArray subres = { nullptr, 0, 0 };
    if (levels != nullptr) {
        int levelWidth = IbI32(ib, 0x20);   // NOT the Width() accessor — the raw field
        int levelIndex = 0;

        for (int i = 0; i < levels->count; ++i) {
            const ImageLevel* src =
                reinterpret_cast<const ImageLevel*>(static_cast<uint8_t*>(levels->data) + i * 0x10);

            D3D11_SUBRESOURCE_DATA entry;
            entry.pSysMem = src->bits;

            // Row length in pixels, or in 4x4 blocks for a BC format. The
            // original is `(w + (w<0 ? 3 : 0)) >> 2`, i.e. C truncating /4.
            int rowUnits = levelWidth;
            if (im->IsCompressed(ib)) rowUnits = levelWidth / 4;
            entry.SysMemPitch      = static_cast<UINT>(kFormatBytes[IbI32(ib, 0x30)] * rowUnits);
            entry.SysMemSlicePitch = 0;   // hoisted out of the loop by the original

            PushSubresource(&subres, entry, im);

            // Next mip; BC formats floor at a 4-pixel (one-block) row. When the
            // mip chain is exhausted, restart it — that is how the six faces of
            // a cube map are walked with one flat counter.
            levelWidth >>= 1;
            if (im->IsCompressed(ib) && levelWidth < 4) levelWidth = 4;
            if (++levelIndex == im->GetLevelCount(ib)) {
                levelWidth = IbI32(ib, 0x20);
                levelIndex = 0;
            }
        }
    }

    HRESULT hr = D3DDevice(dd)->CreateTexture2D(
        &desc, subres.data, reinterpret_cast<ID3D11Texture2D**>(&self->texture));

    if (hr == 0) {
        // --- shader resource view -----------------------------------------
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        D3D11_SHADER_RESOURCE_VIEW_DESC* srvDescPtr = nullptr;

        if (im->IsDepthBuffer(ib)) {
            // Hard-coded: a depth texture is always read as R24_UNORM_X8.
            // GAP: format index 6 (R32_TYPELESS) would need R32_FLOAT here, so
            // either index 6 is unused or IsDepthBuffer implies index 7. The
            // shipped code makes no distinction and neither does this.
            srvDesc.Format                    = static_cast<DXGI_FORMAT>(0x2e); // R24_UNORM_X8_TYPELESS
            srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;  // 4
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels       = 1;
            srvDescPtr = &srvDesc;
        } else if (IbI32(ib, 0x28) == 1) {
            srvDesc.Format                      = static_cast<DXGI_FORMAT>(kFormatToDxgi[IbI32(ib, 0x30)]);
            srvDesc.ViewDimension               = D3D11_SRV_DIMENSION_TEXTURECUBE; // 9
            srvDesc.TextureCube.MipLevels       = static_cast<UINT>(im->GetLevelCount(ib));
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDescPtr = &srvDesc;
        }
        // else: null desc — the default view over the whole 2D texture.

        hr = D3DDevice(dd)->CreateShaderResourceView(
            static_cast<ID3D11Resource*>(self->texture), srvDescPtr,
            reinterpret_cast<ID3D11ShaderResourceView**>(&self->shaderResourceView));

        // --- render-target / depth-stencil view ----------------------------
        // Only for non-cube render buffers.
        if (hr == 0 && IbI32(ib, 0x28) == 0 && im->IsRenderBuffer(ib)) {
            if (im->IsDepthBuffer(ib)) {
                D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
                dsvDesc.Format             = static_cast<DXGI_FORMAT>(0x2d); // D24_UNORM_S8_UINT
                dsvDesc.ViewDimension      = D3D11_DSV_DIMENSION_TEXTURE2D;  // 3
                dsvDesc.Flags              = 0;
                dsvDesc.Texture2D.MipSlice = 0;
                hr = D3DDevice(dd)->CreateDepthStencilView(
                    static_cast<ID3D11Resource*>(self->texture), &dsvDesc,
                    reinterpret_cast<ID3D11DepthStencilView**>(&self->depthStencilView));
            } else {
                hr = D3DDevice(dd)->CreateRenderTargetView(
                    static_cast<ID3D11Resource*>(self->texture), nullptr,
                    reinterpret_cast<ID3D11RenderTargetView**>(&self->renderTargetView));
            }
        }
    }

    // Reached from every path, including a failed CreateTexture2D: the sampler
    // is created regardless, and the scratch array is always freed (the
    // original calls MemFree even when the pointer is null).
    CreateSamplerState(self, im);
    im->MemFree(subres.data);
    return hr == 0;
}

// ============================================================
// slot 4 — 0x180002da0 — bind to a pixel-shader slot
//
// The second parameter (RDX) is never read by the original. Its type is
// therefore undetermined; it is declared void* so the register is preserved and
// nothing is invented. Sampler first, then SRV — that order is observable.
// ============================================================
void ImageBufferData_Bind(ImageBufferData* self, void* unknownArg, unsigned int slot) {
    const Imports* im = GetImports();
    if (!im) {
        typedef void (*orig_t)(ImageBufferData*, void*, unsigned int);
        reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 4))(self, unknownArg, slot);
        return;
    }
    (void)unknownArg;   // never read by the original either

    ID3D11DeviceContext* const ctx = D3DContext(ActiveDeviceData(VideoDevice(im)));
    ctx->PSSetSamplers(slot, 1, reinterpret_cast<ID3D11SamplerState**>(&self->sampler));
    ctx->PSSetShaderResources(slot, 1, reinterpret_cast<ID3D11ShaderResourceView**>(&self->shaderResourceView));
}

// ============================================================
// slot 5 — 0x180002e10 — upload pixels
//
// Always returns true; the original ignores both HRESULTs. Both paths assume
// 4 bytes per pixel and a tightly packed source, so this is the 32-bit upload
// path only (dynamic UI/terrain textures), not the compressed one.
// ============================================================
bool ImageBufferData_Update(ImageBufferData* self, void* data, int width, int height) {
    const Imports* im = GetImports();
    if (!im) {
        typedef bool (*orig_t)(ImageBufferData*, void*, int, int);
        return reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 5))(self, data, width, height);
    }

    ID3D11DeviceContext* const ctx = D3DContext(ActiveDeviceData(VideoDevice(im)));

    if (im->IsDynamic(self->imageBuffer)) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(static_cast<ID3D11Resource*>(self->texture), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        // Note: copies width*height*4 into the mapped block *without* honouring
        // mapped.RowPitch. That is what the shipped code does.
        std::memcpy(mapped.pData, data, static_cast<size_t>(width * height) * 4);
        ctx->Unmap(static_cast<ID3D11Resource*>(self->texture), 0);
    } else {
        ctx->UpdateSubresource(static_cast<ID3D11Resource*>(self->texture), 0, nullptr,
                               data, static_cast<UINT>(width * 4), 0);
    }
    return true;
}

// ============================================================
// slot 6 — 0x180002ef0 — read pixels back
//
// Creates a STAGING B8G8R8A8 texture of the requested size, CopyResources into
// it, maps it for read and de-pitches into the caller's buffer. No HRESULT is
// checked anywhere in the original — if CreateTexture2D fails, `staging` stays
// null and CopyResource is called with it. Reproduced rather than "fixed":
// silently diverging here would hide a real failure during A/B capture.
// ============================================================
bool ImageBufferData_Read(ImageBufferData* self, void* dst, int width, int height) {
    const Imports* im = GetImports();
    if (!im) {
        typedef bool (*orig_t)(ImageBufferData*, void*, int, int);
        return reinterpret_cast<orig_t>(Dx11OriginalSlot(Vt::ImageBufferData, 6))(self, dst, width, height);
    }

    D3D11_TEXTURE2D_DESC desc;
    desc.Width              = static_cast<UINT>(width);
    desc.Height             = static_cast<UINT>(height);
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = static_cast<DXGI_FORMAT>(0x57);   // B8G8R8A8_UNORM
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_STAGING;              // 3
    desc.BindFlags          = 0;
    desc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;  // 0x30000
    desc.MiscFlags          = 0;

    DeviceData* const dd = ActiveDeviceData(VideoDevice(im));
    ID3D11DeviceContext* const ctx = D3DContext(dd);

    ID3D11Texture2D* staging = nullptr;
    D3DDevice(dd)->CreateTexture2D(&desc, nullptr, &staging);

    ctx->CopyResource(staging, static_cast<ID3D11Resource*>(self->texture));

    // MapType is 3 = D3D11_MAP_READ_WRITE, not READ (`LEA R9D,[RBX+3]` with
    // RBX zeroed, 0x180002fb0) — which is why the staging texture above asks
    // for CPU_ACCESS_READ **and** WRITE. A read-only map would be rejected
    // against that description, so this is load-bearing, not incidental.
    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(staging, 0, D3D11_MAP_READ_WRITE, 0, &mapped);

    const int dstPitch = width * 4;
    int srcOffset = 0;
    int dstOffset = 0;
    for (int y = 0; y < height; ++y) {
        std::memcpy(static_cast<uint8_t*>(dst) + dstOffset,
                    static_cast<uint8_t*>(mapped.pData) + srcOffset,
                    static_cast<size_t>(dstPitch));
        srcOffset += static_cast<int>(mapped.RowPitch);
        dstOffset += dstPitch;
    }

    ctx->Unmap(staging, 0);
    staging->Release();
    return true;
}

}  // namespace
}  // namespace VideoDX11

// Unproven: never run in-game as of this commit. All seven register opt-in
// behind one env var so a single BANISHED_DX11_IMAGES=1 run flips the whole
// class, and the seam report shows "7 slot(s) ours". Per project policy
// (kDefaultOnFlags in component_factory_base.cpp) these graduate to
// defaultOn=true once verified, after which the var is a =0 kill switch.
#define DX11_IMAGES_FLAG "BANISHED_DX11_IMAGES"

DX11_REGISTER_SLOT(ImageBufferData, 0, &::VideoDX11::ImageBufferData_ScalarDeletingDtor,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::~ImageBufferData (deleting)");
DX11_REGISTER_SLOT(ImageBufferData, 1, &::VideoDX11::ImageBufferData_AttachColorBuffer,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::AttachColorBuffer");
DX11_REGISTER_SLOT(ImageBufferData, 2, &::VideoDX11::ImageBufferData_AttachDepthBuffer,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::AttachDepthBuffer");
DX11_REGISTER_SLOT(ImageBufferData, 3, &::VideoDX11::ImageBufferData_Create,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::Create");
DX11_REGISTER_SLOT(ImageBufferData, 4, &::VideoDX11::ImageBufferData_Bind,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::Bind");
DX11_REGISTER_SLOT(ImageBufferData, 5, &::VideoDX11::ImageBufferData_Update,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::Update");
DX11_REGISTER_SLOT(ImageBufferData, 6, &::VideoDX11::ImageBufferData_Read,
                   DX11_IMAGES_FLAG, false, "ImageBufferData::Read");
