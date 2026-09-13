#pragma once
// Shared plumbing for the ported VideoDX11 resource classes.
//
// This header holds the two things every ported *Data slot needs and that are
// NOT class-specific:
//
//   1. The walk from the Video::Device singleton down to ID3D11Device /
//      ID3D11DeviceContext, reproduced exactly as the shipped DLL does it.
//   2. Partial, offset-pinned views of the *Runtime-side* owner objects
//      (Video::VertexLayout, Video::ShaderProgram, Video::RenderBuffer,
//      Video::ImageBuffer) that the ported slots read fields out of.
//
// It deliberately does NOT live in dx11/types/VideoDX11Data.h: that file is the
// home of the twelve VideoDX11-defined *Data layouts and is being edited
// concurrently by several porting lanes. Everything here describes Runtime-side
// types instead, which VideoDX11Data.h does not currently model at all. If
// another lane needs these, include this header rather than duplicating it.
//
// Every offset below is proven by a decompiled VideoDX11 function; the proof is
// cited inline. Nothing here is guessed. Unknown regions stay as `opaque`
// arrays so the layouts remain honest about what has actually been recovered.

#include <windows.h>
#include <d3d11.h>

#include <cstdint>
#include <cstddef>

#include "VideoDX11Data.h"
#include "dx11_seam.h"   // Vt / VtGhidraAddr, used by Dx11VtableAddress
#include "resolve.h"

namespace VideoDX11 {

// ============================================================
// Reaching D3D11
// ============================================================
//
// Every resource class in the DLL except ContextData refuses to cache the
// device or the context. It walks to them on every single call, through the
// one global the DLL has (VideoDX11 .rdata 0x1800134b8, an IAT slot holding the
// address of Runtime's Video::Device::_singleton at Runtime 0x1801fe7a8):
//
//     mov rax, [rip+...]      ; 0x1800134b8 -> Video::Device**
//     mov rdx, [rax]          ; Video::Device*
//     mov rax, [rdx+0x100]    ; active Video::DeviceData*   (our DeviceData)
//     mov rcx, [rax+0x28]     ; ID3D11DeviceContext*
//     mov rbx, [rax+0x20]     ; ID3D11Device*
//
// (See decomp/videodx11.md §"The one global". The +0x20 / +0x28 identification
// is from dx11/types/VideoDX11Data.h, itself proven by the CreateTexture2D
// calls in 0x1800052b0 / 0x180005350.)
//
// We resolve the singleton's *address* once and re-read its contents on every
// call, exactly like the original: Video::Device::_singleton can in principle
// be re-pointed, and DeviceData+0x20/+0x28 are definitely replaced across a
// device reset, so caching either would be a behavioural change.
//
// NOTE ON NULL CHECKS: there are none, in the original or here. The shipped
// code dereferences this chain unconditionally; a null anywhere in it means the
// renderer was never brought up, which is already unrecoverable. Adding guards
// would hide that instead of reporting it, and would be a divergence from the
// original for no gain.

// Offset of the active Video::DeviceData* on the Video::Device singleton.
// (decomp/videodx11.md §"How Runtime loads it": Device+0x100.)
constexpr uintptr_t kVideoDevice_ActiveDeviceData = 0x100;

// Ghidra address of Runtime's Video::Device::_singleton.
constexpr uintptr_t kRt_VideoDeviceSingleton = 0x1801fe7a8;

// Ghidra address of the VideoDX11 IAT slot the original reads it through.
// Retained as documentation of the shipped import; nothing reads it now.
constexpr uintptr_t kDx11Iat_VideoDeviceSingleton = 0x1800134b8;

inline void** Dx11VideoDeviceSingletonSlot() {
    static void** slot = reinterpret_cast<void**>(
        ResolveAddr(Module::Runtime, kRt_VideoDeviceSingleton));
    return slot;
}

// Video::Device* — the live singleton.
inline void* Dx11VideoDevice() {
    return *Dx11VideoDeviceSingletonSlot();
}

// The active Video::DeviceData*, i.e. the object whose vtable the seam patches.
inline DeviceData* Dx11DeviceData() {
    uint8_t* dev = static_cast<uint8_t*>(Dx11VideoDevice());
    return *reinterpret_cast<DeviceData**>(dev + kVideoDevice_ActiveDeviceData);
}

inline ID3D11Device* Dx11Device() {
    return static_cast<ID3D11Device*>(Dx11DeviceData()->d3dDevice);
}

inline ID3D11DeviceContext* Dx11Context() {
    return static_cast<ID3D11DeviceContext*>(Dx11DeviceData()->d3dContext);
}

// One-time sanity check that ResolveAddr(Module::Runtime, 0x1801fe7a8) really
// is the same variable the shipped DLL reads through its own IAT. Both are
// meant to be Runtime's Video::Device::_singleton; if they ever disagree, one
// of the two module deltas is wrong and every ported slot would be reading a
// different Device than the un-ported ones. Returns true when they agree (or
// when the IAT slot could not be resolved, which is not evidence of a problem).
bool Dx11VerifyDeviceSingleton();

// ============================================================
// Runtime-side container shape
// ============================================================
//
// Collection::Array<T>. The name comes from the Runtime export
//   ?GetByteCode@ShaderProgram@Video@@QEBAAEBV?$Array@E@Collection@@H@Z
//     -> const Collection::Array<unsigned char>& ShaderProgram::GetByteCode(int)
// and the layout from how VideoDX11 walks these arrays: a base pointer at +0x00
// and an int32 element count at +0x08, with the whole header 0x10 bytes (that
// last part proven by the 0x10 stride the shader-program loop at 0x1800041a0
// uses to step from one Array<unsigned char> to the next).
//
// +0x0C is inferred to be the capacity from the universal {data,count,capacity}
// shape used elsewhere in Runtime (e.g. DeviceData+0x60/+0x68/+0x6c). Nothing
// in this lane reads it.
struct RtArray {
    void*   data;      // +0x00
    int32_t length;    // +0x08
    int32_t capacity;  // +0x0C  (inferred, never read here)
};
static_assert(sizeof(RtArray) == 0x10, "Collection::Array<T> header size");
static_assert(offsetof(RtArray, data)     == 0x00, "RtArray::data");
static_assert(offsetof(RtArray, length)   == 0x08, "RtArray::length");
static_assert(offsetof(RtArray, capacity) == 0x0C, "RtArray::capacity");

// ============================================================
// Video::ShaderProgram — the owner of VertexProgramData / PixelProgramData
// ============================================================
//
// VertexProgramData+0x08 is a Video::VertexProgram* and PixelProgramData+0x08 a
// Video::PixelProgram* (Runtime ctor exports
// ??0VertexProgramData@Video@@QEAA@PEAVVertexProgram@1@@Z and
// ??0PixelProgramData@Video@@QEAA@PEAVPixelProgram@1@@Z). Both derive from
// Video::ShaderProgram, and 0x1800041a0 / 0x180003610 read *identical* offsets
// out of them — which is what makes the fields below base-class fields.
//
// Proven by 0x1800041a0 (VertexProgramData::PlatformCreate):
//     CMP  EBX, dword ptr [RAX + 0x48]     ; permutation count
//     MOV  RAX, qword ptr [RAX + 0x40]     ; base of the byte-code blobs
//     MOV  RDX, qword ptr [RAX + RDI]      ; blob[i].data     (RDI = i*0x10)
//     MOVSXD R8, dword ptr [RAX + RDI + 8] ; blob[i].length
//
// i.e. ShaderProgram+0x40 is a Collection::Array<Collection::Array<uint8>> —
// one compiled byte-code blob per shader permutation, at most 4 of which the
// DX11 backend ever creates.
struct ShaderProgramOwner {
    uint8_t opaque00[0x40];  // +0x00  parameters, constants, textures, ...
    RtArray byteCode;        // +0x40  Array<Array<uint8>>, one blob per permutation
};
static_assert(offsetof(ShaderProgramOwner, byteCode) == 0x40,
              "Video::ShaderProgram byte-code array @ +0x40 (0x1800041a0)");
// The count the loop compares against is byteCode.length at +0x48.
static_assert(offsetof(ShaderProgramOwner, byteCode) + offsetof(RtArray, length) == 0x48,
              "Video::ShaderProgram permutation count @ +0x48 (0x1800041a0)");

// The DX11 backend creates at most this many shader permutations, regardless of
// how many byte-code blobs the program actually carries: the loop in
// 0x1800041a0 / 0x180003610 is `for (i = 0; i < 4; ++i)`, and the variant array
// on VertexProgramData / PixelProgramData is exactly 4 pointers wide.
constexpr int kShaderVariantCount = 4;
static_assert(sizeof(VertexProgramData::variants) / sizeof(void*) == kShaderVariantCount,
              "VertexProgramData variant array width");
static_assert(sizeof(PixelProgramData::variants) / sizeof(void*) == kShaderVariantCount,
              "PixelProgramData variant array width");

// ============================================================
// Video::VertexLayout — the owner of VertexLayoutData
// ============================================================
//
// VertexLayoutData+0x08 is a Video::VertexLayout* (Runtime ctor export
// ??0VertexLayoutData@Video@@QEAA@PEAVVertexLayout@1@@Z). Runtime also exports
// ?GetElementCount@VertexLayout@Video@@QEBAHXZ and
// ?GetElement@VertexLayout@Video@@QEBAAEBUElement@12@H@Z, which is where the
// `Element` struct name comes from.
//
// Proven by 0x180003ff0 (VertexLayoutData::PlatformCreate):
//     CMP  dword ptr [R9 + 0x40], 0x0   ; guard: input-signature length
//     MOV  R8D, dword ptr [R9 + 0x28]   ; element count
//     MOV  RAX, qword ptr [R9 + 0x20]   ; element array base (stride 0x20)
//     MOV  R9,  qword ptr [R9 + 0x38]   ; input-signature byte code
//     MOVSXD RDX, dword ptr [R9 + 0x40] ; input-signature length
struct VertexLayoutOwner {
    uint8_t opaque00[0x20];  // +0x00
    RtArray elements;        // +0x20  Array<Element>, stride 0x20
    uint8_t opaque30[0x08];  // +0x30
    RtArray signature;       // +0x38  Array<uint8> — the vertex-shader input signature
};
static_assert(offsetof(VertexLayoutOwner, elements) == 0x20,
              "Video::VertexLayout element array @ +0x20 (0x180003ff0)");
static_assert(offsetof(VertexLayoutOwner, elements) + offsetof(RtArray, length) == 0x28,
              "Video::VertexLayout element count @ +0x28 (0x180003ff0)");
static_assert(offsetof(VertexLayoutOwner, signature) == 0x38,
              "Video::VertexLayout input signature @ +0x38 (0x180003ff0)");
static_assert(offsetof(VertexLayoutOwner, signature) + offsetof(RtArray, length) == 0x40,
              "Video::VertexLayout input-signature length @ +0x40 (0x180003ff0)");

// Video::VertexLayout::Element — 0x20 bytes.
//
// Runtime's ?Add@VertexLayout@Video@@QEAAXHHW4Usage@12@HW4Format@12@@Z is
//     void VertexLayout::Add(int, int, Usage, int, Format)
// so an Element carries at least {stream, offset, usage, usageIndex, format}.
// VideoDX11 reads exactly three of those:
//
//     MOVZX ECX, word ptr [RAX + RDX - 0x18]  ; +0x08, u16 -> InputSlot
//     MOVZX ECX, word ptr [RAX + RDX - 0x14]  ; +0x0C, u16 -> AlignedByteOffset
//     MOVSXD RCX, dword ptr [RAX + RDX - 0x8] ; +0x18, i32 -> Format enum
//
// (RDX walks in 0x20 steps and is pre-biased by +0x20, hence the negative
// displacements.) The `usage` / `usageIndex` fields are somewhere in the opaque
// regions; the DX11 backend never looks at them — see the semantic-name note in
// vertexlayoutdata.cpp.
struct VertexLayoutElement {
    uint8_t  opaque00[0x08];  // +0x00
    uint16_t stream;          // +0x08  -> D3D11_INPUT_ELEMENT_DESC::InputSlot
    uint8_t  opaque0a[0x02];  // +0x0A
    uint16_t byteOffset;      // +0x0C  -> D3D11_INPUT_ELEMENT_DESC::AlignedByteOffset
    uint8_t  opaque0e[0x0A];  // +0x0E  (Usage / usageIndex live in here)
    int32_t  format;          // +0x18  Video::VertexLayout::Format, 0..8
    uint8_t  opaque1c[0x04];  // +0x1C
};
static_assert(sizeof(VertexLayoutElement) == 0x20,
              "Video::VertexLayout::Element stride (0x180003ff0 walks in 0x20 steps)");
static_assert(offsetof(VertexLayoutElement, stream)     == 0x08, "Element::stream");
static_assert(offsetof(VertexLayoutElement, byteOffset) == 0x0C, "Element::byteOffset");
static_assert(offsetof(VertexLayoutElement, format)     == 0x18, "Element::format");

// ============================================================
// Video::RenderBuffer / Video::ImageBuffer — owners reached by RenderBufferData
// ============================================================
//
// RenderBufferData+0x08 is a Video::RenderBuffer* (Runtime ctor export
// ??0RenderBufferData@Video@@QEAA@PEAVRenderBuffer@1@@Z), and Runtime exports
//     ?ColorBuffer@RenderBuffer@Video@@QEBAPEBVImageBuffer@2@XZ
//     ?DepthBuffer@RenderBuffer@Video@@QEBAPEBVImageBuffer@2@XZ
//     ?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z
// i.e. a RenderBuffer is exactly a (color, depth) ImageBuffer pair. There is no
// ??_7RenderBuffer@Video@@6B@ export, so the class is non-polymorphic and can
// legitimately start with a data member at +0x00.
//
// Proven by 0x180001070 (RenderBufferData::Bind):
//     MOV RDX, qword ptr [RCX + 0x8]   ; this->renderBuffer
//     MOV RAX, qword ptr [RDX]         ; +0x00 -> color ImageBuffer*
//     MOV RAX, qword ptr [RDX + 0x8]   ; +0x08 -> depth ImageBuffer*
// ============================================================
// Constant-buffer binding registry (device-reset bookkeeping)
//
// DeviceData::Resize calls ID3D11DeviceContext::ClearState(), which unbinds
// every constant buffer from its VS/PS slot. ConstantBufferData::Bind only
// issues VS/PSSetConstantBuffers when its cached rung index changes, so after a
// ClearState nothing ever re-binds them: every shader then reads a null
// constant buffer, all transforms come back zero, no geometry rasterises, and
// the window is black for the rest of the run. See the note on
// Dx11InvalidateConstantBufferBindings in buffers.cpp.
//
// The registry is only touched on create/destroy and once per device reset,
// never per draw.
// ============================================================
void Dx11TrackConstantBuffer(ConstantBufferData* p);
void Dx11UntrackConstantBuffer(ConstantBufferData* p);
void Dx11InvalidateConstantBufferBindings();

struct RenderBufferOwner {
    void* color;   // +0x00  Video::ImageBuffer* (may be null)
    void* depth;   // +0x08  Video::ImageBuffer* (may be null)
};
static_assert(sizeof(RenderBufferOwner) == 0x10, "Video::RenderBuffer color/depth pair");
static_assert(offsetof(RenderBufferOwner, depth) == 0x08, "RenderBuffer::depth");

// Video::ImageBuffer+0x60 is the backend companion object, i.e. the
// VideoDX11::ImageBufferData whose layout is in dx11/types/VideoDX11Data.h.
//
// Proven by 0x180001070 / 0x1800010e0, which read
//     imageBuffer->[0x60]->[0x18]   the D3D texture
//     imageBuffer->[0x60]->[0x20]   the render-target view
//     imageBuffer->[0x60]->[0x28]   the depth-stencil view
// and cross-checked against ImageBufferData's own creator 0x180002870, which
// stores into exactly those three offsets of `this`:
//     CALL [dev+0x38] (index  7 = CreateShaderResourceView) -> this+0x30
//     CALL [dev+0x48] (index  9 = CreateRenderTargetView)   -> this+0x20
//     CALL [dev+0x50] (index 10 = CreateDepthStencilView)   -> this+0x28
// and 0x180002580, which stores the swap-chain colour texture into this+0x18.
struct ImageBufferOwner {
    uint8_t opaque00[0x60];  // +0x00
    void*   data;            // +0x60  VideoDX11::ImageBufferData*
};
static_assert(offsetof(ImageBufferOwner, data) == 0x60,
              "Video::ImageBuffer backend data @ +0x60 (0x180001070)");

// ImageBufferData fields this lane reads. L2 derived these offsets from
// RenderBufferData slots 2/3 (0x180001070, 0x1800010e0) while L3 derived them
// independently from ImageBufferData slots 1/2/3 — the two lanes agreed
// exactly, so +0x18/+0x20/+0x28 are texture/RTV/DSV on two separate proofs.
// L3 owns the struct and has now named them; these helpers just spell out which
// view each caller wants.
inline ID3D11Resource* Dx11ImageTexture(const void* imageBuffer) {
    const ImageBufferOwner* img = static_cast<const ImageBufferOwner*>(imageBuffer);
    return static_cast<ID3D11Resource*>(
        static_cast<ImageBufferData*>(img->data)->texture);
}
inline ID3D11RenderTargetView* Dx11ImageRtv(const void* imageBuffer) {
    const ImageBufferOwner* img = static_cast<const ImageBufferOwner*>(imageBuffer);
    return static_cast<ID3D11RenderTargetView*>(
        static_cast<ImageBufferData*>(img->data)->renderTargetView);
}
inline ID3D11DepthStencilView* Dx11ImageDsv(const void* imageBuffer) {
    const ImageBufferOwner* img = static_cast<const ImageBufferOwner*>(imageBuffer);
    return static_cast<ID3D11DepthStencilView*>(
        static_cast<ImageBufferData*>(img->data)->depthStencilView);
}

// ============================================================
// Runtime imports used by the ported destructors
// ============================================================
//
// Each ported slot-0 destructor has to call the same Runtime-side base
// destructor the original called, then the same allocator. We resolve those by
// MSVC mangled name (verified against the shipped Runtime-x64.dll export table
// with objdump -p), and fall back to reading VideoDX11's own IAT slot — the
// very pointer the original CALLs through — if the name lookup ever misses.
// The fallback is what makes this immune to a mangling mistake: the IAT entry
// is by construction the exact function the shipped code would have called.
//
// Resolution is lazy and cached, so there is no static-initialisation-order
// dependency on EngineFuncsInit() or ResolveInit(). The unsynchronised cache
// write is benign: both racers compute the same value.

typedef void (*Dx11BaseDtor_t)(void* self);
typedef void (*Dx11MemFree_t)(void* ptr);

// mangled: the Runtime-x64.dll export name
// iatGhidra: the VideoDX11 .rdata IAT slot the original calls through
void* Dx11ResolveRuntimeImport(const char* mangled, uintptr_t iatGhidra);

// L6's port of 0x180004340, the DeviceData swap-chain resize/recreate helper.
// Declared here because it is the one cross-lane call in the port: L5's
// ContextData::BeginCommands reaches it with a near CALL rather than a vtable
// dispatch, so the seam cannot redirect it and L5 must name our version.
// `deviceData` is a Video::DeviceData* (void* to keep this declaration free of
// L6's internal types).
int Dx11_DeviceData_Resize(void* deviceData);

// System::MemoryAllocator::MemFree(void*) — VideoDX11 IAT slot 0x1800134d0.
// Called by every *Data scalar-deleting destructor when (flags & 1).
inline void Dx11MemFree(void* p) {
    static Dx11MemFree_t fn = reinterpret_cast<Dx11MemFree_t>(
        Dx11ResolveRuntimeImport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z",
                                 0x1800134d0));
    if (fn) fn(p);
}

// The DX11 vtable a destructor must re-seat `this` on before running the base
// destructor (MSVC emits this vptr store in every derived destructor). Handing
// back the address of the *live* vtable is what keeps a partially-ported class
// consistent: the seam patches slots inside that same array.
inline void* Dx11VtableAddress(Vt v) {
    // Was the shipped .rdata array at VtGhidraAddr(v). Dx11LiveVtable()
    // returns our exe-owned array. Do not reintroduce the direct resolve:
    // VideoDX11-x64.dll is never mapped (ENDGAME.md A.1) and it would yield a
    // vptr into nothing.
    return Dx11LiveVtable(v);
}

// COM Release() through the IUnknown vtable (index 2, byte offset 0x10) —
// exactly the `MOV RAX,[RCX]; CALL [RAX+0x10]` the destructors emit.
inline void Dx11Release(void* comObject) {
    if (comObject) static_cast<IUnknown*>(comObject)->Release();
}

}  // namespace VideoDX11
