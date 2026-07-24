// VideoDX11 ContextData — vtable 0x1800186f0, all 20 slots (lane L5).
//
// The largest vtable in the DLL and the last resource class. It is also the one
// class that CACHES the D3D11 immediate context (ContextData+0x18) instead of
// re-walking Video::Device::_singleton -> [+0x100] -> [+0x28] on every call.
//
// ---------------------------------------------------------------------------
// The slot names are recovered, not invented
// ---------------------------------------------------------------------------
// Runtime's Video::Context keeps its Video::ContextData* at Context+0x2160, and
// every public Context method that reaches the backend is a two-load tail-jump
// through it:
//
//     Video::Context::BindTransforms   0x1800f22a0
//         MOV RCX,[RCX+0x2160] / MOV RAX,[RCX] / JMP [RAX+0x40]
//
// So the mapping from Runtime method name to vtable byte offset is read
// straight out of the thunk, with no inference at all. All twenty were checked
// individually in Runtime-x64.dll:
//
//   slot off   Runtime thunk   exported signature (from the Runtime export table)
//   ---- ----  --------------  -----------------------------------------------
//    0   0x00  0x1800f2240 PlatformDestruct  -> vtable[0](this, 1)
//    1   0x08  0x1800f2340   ?BeginCommands@Context@Video@@QEAA_NXZ
//    2   0x10  0x1800f2360   ?EndCommands@Context@Video@@QEAAXXZ
//    3   0x18  0x1800f2380   ?Finish@Context@Video@@QEAAXXZ
//    4   0x20  0x1800f23f0   ?Clear@Context@Video@@QEAAXIIMH@Z
//    5   0x28  0x1800f2280   ?BindRenderBuffer@Context@Video@@AEAAXXZ
//    6   0x30  0x1800f2300   ?BindViewport@Context@Video@@AEAAXXZ
//    7   0x38  0x1800f2320   ?BindScissor@Context@Video@@AEAAXXZ
//    8   0x40  0x1800f22a0   ?BindTransforms@Context@Video@@AEAAXXZ
//    9   0x48  0x1800f2260   ?BindTextureNone@Context@Video@@AEAAXH@Z
//   10   0x50  0x1800f22e0   ?BindIndexNone@Context@Video@@AEAAXXZ
//   11   0x58  0x1800f22c0   ?BindVertexNone@Context@Video@@AEAAXH@Z
//   12   0x60  0x1800f2410   ?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z
//   13   0x68  0x1800f21d0   ?GetMaxDynamicInstances@Context@Video@@QEBAHHHH@Z
//   14   0x70  0x1800f21b0   ?BeginVertices@Context@Video@@QEAA_NHH@Z
//   15   0x78  0x1800f2190   ?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z
//   16   0x80  0x1800f2170   ?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z
//   17   0x88  0x1800f2150   ?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z
//   18   0x90  0x1800f2130   ?BeginInstancedIndexedVertices@Context@Video@@QEAA_NHHHH@Z
//   19   0x98  0x1800f2110   ?EndInstancedIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z
//
// The Runtime thunks pass `this` = the ContextData and forward RDX/R8/R9 and
// the stack arguments untouched, so each slot's parameter list is exactly the
// Context method's parameter list. That is where the signatures below come
// from — none of them is guessed from register usage alone.
//
// ---------------------------------------------------------------------------
// Slot 17 and slot 19 are the same function
// ---------------------------------------------------------------------------
// 0x180002020 is five bytes: `e9 2b 01 00 00` = `jmp 0x180002150`. So
// EndIndexedVertices and EndInstancedIndexedVertices share one body. That is
// consistent because the instance count is carried on the Context
// (Context+0x2110), and the non-instanced entry point (slot 16) sets it to 1
// before locking. Both slots are registered here with the same implementation.
//
// Three slots are not Ghidra functions at all — they are tails inside a
// neighbour's body. All three were decoded by hand from `read_memory`:
//
//   0x180001d10  48 8b 49 18            MOV RCX,[RCX+0x18]      this->d3dContext
//                45 33 c9               XOR R9D,R9D             Offset = 0
//                33 d2                  XOR EDX,EDX             pIndexBuffer = NULL
//                48 8b 01               MOV RAX,[RCX]
//                45 8d 41 39            LEA R8D,[R9+0x39]       Format = 57 = R16_UINT
//                48 ff a0 98 00 00 00   JMP [RAX+0x98]          IASetIndexBuffer
//   0x180001ef0  b8 34 00 00 00 / c3    MOV EAX,0x34; RET       -> 52
//   0x180002020  e9 2b 01 00 00         JMP 0x180002150
//
// (dx11/README.md already listed all three; each is re-verified here rather
// than trusted.)
//
// ---------------------------------------------------------------------------
// Video::Context field map used by this file
// ---------------------------------------------------------------------------
// Every offset is proven either by a VideoDX11 instruction cited at its use
// site below, or by a one-line Runtime accessor, which is the stronger proof
// because it also gives the field its name:
//
//   Runtime accessor                              offset  field
//   --------------------------------------------  ------  ---------------------
//   ?GetRenderBuffer@         0x180019ec0          +0x0028 current RenderBuffer*
//   ?GetViewport@             0x18001a0c0          +0x01d0 Viewport{x,y,w,h}
//   ?GetVertexBufferStride@   0x180019f90          (array) dynamic stride kin
//   ?GetDefaultRenderBuffer@  0x18001ae40          +0x2138 default RenderBuffer*
//   ?GetTrianglesSubmitted@   0x18001ae60          +0x2150 int
//   ?GetVerticesSubmitted@    0x18001ae70          +0x2154 int
//   ?GetDrawsSubmitted@       0x18001ae80          +0x2158 int
//   ?GetObjectsSubmitted@     0x18001ae90          +0x215c int
//   ?AddVertex@               0x18001ad40          +0x20e0 write cursor,
//                                                  +0x20e8 stride
//   (Context+0x2160 = the ContextData*, from every thunk above.)
//
// ---------------------------------------------------------------------------
// COM vtable indices
// ---------------------------------------------------------------------------
// Index = position counted from QueryInterface = 0, i.e. INCLUDING every
// inherited method (ID3D11DeviceContext derives ID3D11DeviceChild, which
// contributes GetDevice/GetPrivateData/SetPrivateData/SetPrivateDataInterface
// at 3..6). Byte offset = index * 8. Each one below was checked against the
// disassembly of the call site, not assumed:
//
//   interface               idx  byte   method                     call site
//   ----------------------  ---  -----  -------------------------  -----------
//   IUnknown                  1  0x008  AddRef                     0x180001747
//   IUnknown                  2  0x010  Release                    0x180001794
//   IDXGISwapChain            8  0x040  Present                    0x1800017fc
//   IDXGISwapChain            9  0x048  GetBuffer                  0x180001763
//   ID3D11Device             39  0x138  GetDeviceRemovedReason     0x180001828
//   ID3D11DeviceContext       8  0x040  PSSetShaderResources       0x180001cf4
//   ID3D11DeviceContext      10  0x050  PSSetSamplers              0x180001cdd
//   ID3D11DeviceContext      13  0x068  Draw                       0x180001ff0
//   ID3D11DeviceContext      12  0x060  DrawIndexed                0x180001e2a
//   ID3D11DeviceContext      18  0x090  IASetVertexBuffers         0x180001d69
//   ID3D11DeviceContext      19  0x098  IASetIndexBuffer           0x180001d1d
//   ID3D11DeviceContext      20  0x0a0  DrawIndexedInstanced       0x180001e51
//   ID3D11DeviceContext      24  0x0c0  IASetPrimitiveTopology     0x180001dfd
//   ID3D11DeviceContext      44  0x160  RSSetViewports             0x180001bd4
//   ID3D11DeviceContext      45  0x168  RSSetScissorRects          0x1800019bc
//   ID3D11DeviceContext      50  0x190  ClearRenderTargetView      0x180001a5e
//   ID3D11DeviceContext      53  0x1a8  ClearDepthStencilView      0x180001aa9
//   ID3D11DeviceContext      57  0x1c8  ResolveSubresource         0x180001788
//
// These agree with the indices L3 and L4 recovered independently for
// CreateTexture2D (5), CreateShaderResourceView (7), CreateRenderTargetView
// (9), CreateDepthStencilView (10), CreateBlendState (20), CreateSamplerState
// (23), OMSetBlendState (35), OMSetDepthStencilState (36) and RSSetState (43).
//
// ---------------------------------------------------------------------------
// What this lane deliberately does NOT do
// ---------------------------------------------------------------------------
// Slot 1 calls 0x180004340, the DeviceData swap-chain resize/recreate helper.
// That function belongs to lane L6 and is NOT ported here — it is reached
// through ResolveAddr(Module::VideoDX11, 0x180004340), i.e. the shipped code,
// exactly as the original slot 1 reaches it.
//
// Slot 3 calls DeviceData vtable slots 3 (shutdown) and 2 (create device). It
// calls them THROUGH the live vtable, so it picks up whatever the seam has
// installed there — the original today, L6's port later. No DeviceData slot is
// registered by this file.

#include "dx11_seam.h"
#include "dx11_backend_access.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace VideoDX11 {
namespace {

// ============================================================
// Runtime-x64.dll imports
//
// Resolved lazily in this translation unit only — deliberately not added to
// engine/funcs/engine_funcs.h. Every mangled string below was read out of
// VideoDX11-x64.dll's own import-name table (`objdump -p`), so it is exactly
// the string the shipped DLL resolves, and the IAT slot given as the fallback
// is the pointer the shipped code CALLs through.
//
// The IAT slot addresses were derived by walking the import table in order from
// two anchors (?_singleton@Device@Video@@ at 0x1800134b8 and ?MemFree@ at
// 0x1800134d0) and then cross-checked: all fourteen below land on a call site
// Ghidra had already labelled with that exact name. Two independent proofs per
// entry.
//
// Note the two `AEAA` (private) members: BindResources and BindTextures really
// are private and really are exported by name; a `QEAA` guess would silently
// resolve to nullptr.
// ============================================================
struct Imports {
    void  (*ContextDataBaseDtor)(void*);              // ??1ContextData@Video@@UEAA@XZ           0x180013470

    void  (*DeviceLock)(void*);                       // ?Lock@Device@Video@@QEAAXXZ             0x180013408
    void  (*DeviceUnlock)(void*);                     // ?Unlock@Device@Video@@QEAAXXZ           0x180013410
    void  (*DeviceReleaseDynamic)(void*);             // ?ReleaseDynamic@Device@Video@@QEAAXXZ   0x180013230
    void  (*DeviceRecreateDynamic)(void*);            // ?RecreateDynamic@Device@Video@@QEAAXXZ  0x180013240

    void  (*CtxSetViewport)(void*, const void*);      // ?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z          0x180013400
    void  (*CtxSetRenderBuffer)(void*, const void*);  // ?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z  0x180013468
    void  (*CtxSetImageBuffer)(void*, const void*, int);           // ?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z   0x1800133f0
    void  (*CtxSetVertexBuffer)(void*, const void*, int, int, int);// ?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z 0x1800133f8
    void  (*CtxBindTextures)(void*);                  // ?BindTextures@Context@Video@@AEAAXXZ    0x180013450
    void  (*CtxBindResources)(void*);                 // ?BindResources@Context@Video@@AEAAXXZ   0x180013438

    void  (*RenderBufferBind)(const void*, void*);    // ?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z             0x180013448
    void  (*ConstantBufferBind)(void*, void*, void*, int, int);    // ?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z     0x180013440

    void* (*VertexBufferLock)(void*, int, int*);      // ?Lock@VertexBuffer@Video@@QEAAPEAXHAEAH@Z   0x180013430
    void  (*VertexBufferUnlock)(void*);               // ?Unlock@VertexBuffer@Video@@QEAAXXZ         0x180013428
    void* (*IndexBufferLock)(void*, int, int*);       // ?Lock@IndexBuffer@Video@@QEAAPEAXHAEAH@Z    0x180013420
    void  (*IndexBufferUnlock)(void*);                // ?Unlock@IndexBuffer@Video@@QEAAXXZ          0x180013418

    void  (*ErrorSetText)(const char*, bool);         // ?SetText@Error@System@@SAXPEBD_N@Z      0x180013460
    void  (*ErrorDisplay)();                          // ?Display@Error@System@@SAXXZ            0x180013458

    void** configSingleton;                           // ?_singleton@Configuration@Video@@0PEAV12@EA 0x180013480

    // NOT a Runtime import: VideoDX11's own DeviceData swap-chain resize helper
    // at 0x180004340, owned by lane L6. Slot 1 calls it directly (0x1800016a3
    // is a near CALL, not a vtable dispatch), so it is resolved as a plain
    // VideoDX11 address rather than through the seam.
    int   (*DeviceDataResize)(void*);
};

Imports g_im;
bool    g_imResolved = false;
bool    g_imOk       = false;

// Returns nullptr when any import is missing; the affected slots then forward
// to the shipped implementation instead of dereferencing a null. This is the
// same policy lane L3 used.
const Imports* GetImports() {
    if (!g_imResolved) {
        g_imResolved = true;
        std::memset(&g_im, 0, sizeof(g_im));

        struct Entry { void** dst; const char* mangled; uintptr_t iat; };
        const Entry table[] = {
            { (void**)&g_im.ContextDataBaseDtor,   "??1ContextData@Video@@UEAA@XZ",                                 0x180013470 },
            { (void**)&g_im.DeviceLock,            "?Lock@Device@Video@@QEAAXXZ",                                   0x180013408 },
            { (void**)&g_im.DeviceUnlock,          "?Unlock@Device@Video@@QEAAXXZ",                                 0x180013410 },
            { (void**)&g_im.DeviceReleaseDynamic,  "?ReleaseDynamic@Device@Video@@QEAAXXZ",                         0x180013230 },
            { (void**)&g_im.DeviceRecreateDynamic, "?RecreateDynamic@Device@Video@@QEAAXXZ",                        0x180013240 },
            { (void**)&g_im.CtxSetViewport,        "?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z",            0x180013400 },
            { (void**)&g_im.CtxSetRenderBuffer,    "?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z",    0x180013468 },
            { (void**)&g_im.CtxSetImageBuffer,     "?SetImageBuffer@Context@Video@@QEAAXPEBVImageBuffer@2@H@Z",     0x1800133f0 },
            { (void**)&g_im.CtxSetVertexBuffer,    "?SetVertexBuffer@Context@Video@@QEAAXPEBVVertexBuffer@2@HHH@Z", 0x1800133f8 },
            { (void**)&g_im.CtxBindTextures,       "?BindTextures@Context@Video@@AEAAXXZ",                          0x180013450 },
            { (void**)&g_im.CtxBindResources,      "?BindResources@Context@Video@@AEAAXXZ",                         0x180013438 },
            { (void**)&g_im.RenderBufferBind,      "?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z",               0x180013448 },
            { (void**)&g_im.ConstantBufferBind,    "?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z",       0x180013440 },
            { (void**)&g_im.VertexBufferLock,      "?Lock@VertexBuffer@Video@@QEAAPEAXHAEAH@Z",                     0x180013430 },
            { (void**)&g_im.VertexBufferUnlock,    "?Unlock@VertexBuffer@Video@@QEAAXXZ",                           0x180013428 },
            { (void**)&g_im.IndexBufferLock,       "?Lock@IndexBuffer@Video@@QEAAPEAXHAEAH@Z",                      0x180013420 },
            { (void**)&g_im.IndexBufferUnlock,     "?Unlock@IndexBuffer@Video@@QEAAXXZ",                            0x180013418 },
            { (void**)&g_im.ErrorSetText,          "?SetText@Error@System@@SAXPEBD_N@Z",                            0x180013460 },
            { (void**)&g_im.ErrorDisplay,          "?Display@Error@System@@SAXXZ",                                  0x180013458 },
            { (void**)&g_im.configSingleton,       "?_singleton@Configuration@Video@@0PEAV12@EA",                   0x180013480 },
        };
        for (const Entry& e : table)
            *e.dst = Dx11ResolveRuntimeImport(e.mangled, e.iat);

        g_im.DeviceDataResize = reinterpret_cast<int (*)(void*)>(
            ResolveAddr(Module::VideoDX11, 0x180004340));

        const void* const* p = reinterpret_cast<const void* const*>(&g_im);
        g_imOk = true;
        for (size_t i = 0; i < sizeof(Imports) / sizeof(void*); ++i)
            if (p[i] == nullptr) g_imOk = false;
    }
    return g_imOk ? &g_im : nullptr;
}

// ============================================================
// Video::Context field access
//
// Raw byte offsets rather than an invented 0x2168-byte struct: only ~25 of
// those bytes are actually understood, and a struct would have to fabricate
// twenty opaque padding runs to express that. Each constant carries its proof.
// ============================================================
namespace Ctx {
// +0x0000  Video::Device* — the Context's device. Slot 1 loads it with
//          `MOV RAX,[RCX+8] / MOV RDI,[RAX]` (0x18000168c) and hands it
//          straight to Video::Device::Lock, so Context+0x00 is a Device*, not
//          a vptr. (Video::Context is therefore non-polymorphic here.)
constexpr size_t kDevice              = 0x0000;

// +0x0010  dirty-flag word. Bits this DLL touches:
//            0x0001 render buffer   (slot 4 -> BindRenderBuffer)
//            0x0002 textures        (slot 5 -> Video::Context::BindTextures)
//            0x0010 index buffer    (set by slot 19)
//            0x0200 viewport        (slot 4 -> BindViewport)
//            0x0400 scissor         (slot 4 -> RSSetScissorRects)
//            0x4000 transforms      (slot 8)
//          Slot 4 clears 0x0601 in one AND (0x1800019c6, mask 0xfffff9fe).
constexpr size_t kDirtyFlags          = 0x0010;

// +0x0020 / +0x0024  element counts for the two embedded constant buffers.
// Slot 8: transform payload size is `[0x20] << 6` (0x180001c67, i.e. 64-byte
// matrices) and the second buffer is bound only when `[0x24] > 0`.
constexpr size_t kTransformCount      = 0x0020;
constexpr size_t kBoneCount           = 0x0024;

// +0x0028  current Video::RenderBuffer*. Named by Runtime's
// ?GetRenderBuffer@Context@Video@@ (0x180019ec0 = `MOV RAX,[RCX+0x28]; RET`).
constexpr size_t kRenderBuffer        = 0x0028;

// +0x0030  16 bound Video::ImageBuffer* texture slots. Slot 5 walks
// i in [0,0x10) with a stride of 8 starting at 0x30 (0x180001ae4 seeds RDI
// with 0x30, 0x180001b18 bounds the loop at 0x10).
constexpr size_t kTextures            = 0x0030;
constexpr int    kTextureSlotCount    = 16;

// +0x0418  bone matrix payload, exactly 0xcc0 bytes (slot 8 passes the literal
// 0xcc0 at 0x180001c92) — and 0x0418 + 0xcc0 == 0x10d8, the transform payload,
// so the two regions are adjacent and the size is corroborated.
constexpr size_t kBoneData            = 0x0418;
constexpr int    kBoneDataBytes       = 0x0cc0;
constexpr size_t kTransformData       = 0x10d8;

// +0x01b8 / +0x01c0  last index buffer + byte offset bound through the normal
// (non-dynamic) path; slot 19 compares against them and raises dirty bit 0x10
// when the dynamic index buffer differs (0x1800021d4-0x1800021f0).
constexpr size_t kBoundIndexBuffer    = 0x01b8;
constexpr size_t kBoundIndexOffset    = 0x01c0;

// +0x01d0  Viewport {x, y, w, h} as int32. Named and pinned by Runtime's
// ?GetViewport@Context@Video@@ (0x18001a0c0), which copies exactly these four
// dwords into the 16-byte return value.
constexpr size_t kViewport            = 0x01d0;

// +0x01e0  Scissor {x, y, w, h} as int32 — same shape, read by slots 4 and 7
// to build a D3D11_RECT of {x, y, x+w, y+h}.
constexpr size_t kScissor             = 0x01e0;

// +0x2008 / +0x2050  two Video::ConstantBuffer objects EMBEDDED in the Context
// (slot 8 takes their addresses with LEA, it does not load a pointer).
// +0x2050 carries the transform payload, +0x2008 the bone payload.
constexpr size_t kBoneConstantBuffer      = 0x2008;
constexpr size_t kTransformConstantBuffer = 0x2050;

// The dynamic-geometry scratch block. Names come from Runtime's
// ?AddVertex@Context@Video@@ (0x18001ad40), which memcpy's into [0x20e0] and
// advances it by [0x20e8].
constexpr size_t kDynVertexCursor     = 0x20e0;  // void*  moving write pointer
constexpr size_t kDynVertexStride     = 0x20e8;  // int    bytes per vertex
constexpr size_t kDynIndexCursor      = 0x20f0;  // void*  moving write pointer
constexpr size_t kDynVertexBuffer     = 0x20f8;  // Video::VertexBuffer*
constexpr size_t kDynVertexOffset     = 0x2100;  // int    byte offset from Lock
constexpr size_t kDynVertexCount      = 0x2104;  // int
constexpr size_t kDynVertexBase       = 0x2108;  // void*  copy of the Lock result
constexpr size_t kDynInstanceCount    = 0x2110;  // int
constexpr size_t kDynIndexBuffer      = 0x2118;  // Video::IndexBuffer*
constexpr size_t kDynIndexOffset      = 0x2120;  // int    byte offset from Lock
constexpr size_t kDynIndexCount       = 0x2124;  // int
constexpr size_t kDynIndexBase        = 0x2128;  // void*  copy of the Lock result

// +0x2138  default Video::RenderBuffer*, named by ?GetDefaultRenderBuffer@
// (0x18001ae40 = `MOV RAX,[RCX+0x2138]; RET`).
constexpr size_t kDefaultRenderBuffer = 0x2138;

// The four per-frame counters, each named by its one-instruction Runtime getter.
constexpr size_t kTrianglesSubmitted  = 0x2150;  // ?GetTrianglesSubmitted@ 0x18001ae60
constexpr size_t kVerticesSubmitted   = 0x2154;  // ?GetVerticesSubmitted@  0x18001ae70
constexpr size_t kDrawsSubmitted      = 0x2158;  // ?GetDrawsSubmitted@     0x18001ae80
constexpr size_t kObjectsSubmitted    = 0x215c;  // ?GetObjectsSubmitted@   0x18001ae90
}  // namespace Ctx

template <typename T>
inline T& Field(void* base, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);
}
template <typename T>
inline T Field(const void* base, size_t off) {
    return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + off);
}
inline void* FieldPtr(void* base, size_t off) {
    return static_cast<uint8_t*>(base) + off;
}

// ============================================================
// Video::Device fields this file reads
//
// Only five, all proven by a VideoDX11 instruction:
//   +0x04 / +0x08  backbuffer width / height. Slot 1 builds the full-screen
//                  viewport from them (0x1800016d4 / 0x1800016df); the same two
//                  fields fill DXGI_SWAP_CHAIN_DESC.BufferDesc in 0x180004590
//                  (see decomp/videodx11.md), which is an independent proof.
//   +0x50 / +0x58  two `void(*)()` notification hooks, called by slot 3 either
//                  side of a device reset (0x18000183c / 0x18000187d). Both are
//                  null-checked by the original.
//   +0x9c          a byte flag; when non-zero slot 1 refuses to begin the frame
//                  and runs the swap-chain recreate helper instead
//                  (0x180001693). Best read as "the backbuffer no longer
//                  matches the window". NOT the same field as Device+0x98,
//                  which decomp/videodx11.md documents as the caps word.
//   +0x100         the active Video::DeviceData*.
// ============================================================
namespace Dev {
constexpr size_t kWidth            = 0x004;
constexpr size_t kHeight           = 0x008;
constexpr size_t kOnDeviceLost     = 0x050;
constexpr size_t kOnDeviceRestored = 0x058;
constexpr size_t kResetPending     = 0x09c;
constexpr size_t kActiveDeviceData = 0x100;
}  // namespace Dev

// Video::Configuration+0x41 — a byte, used as Present's SyncInterval
// (`MOVZX EAX,byte [RDX+0x41]` at 0x1800017e7, then `SETNZ DL`). Read through
// ?_singleton@Configuration@Video@@, which is the OTHER of the two globals
// Ghidra renders as `_singleton_exref`; conflating it with
// ?_singleton@Device@Video@@ (0x1800134b8) would read the wrong object.
constexpr size_t kConfigVSync = 0x41;

typedef void (*DeviceNotifyFn)();

// ContextData never uses the Video::Device singleton: it walks
// this->context->[0x00] instead. Both routes reach the same object, but the
// original only ever takes this one, so the port takes it too.
inline void* OwnerDevice(ContextData* self) {
    return Field<void*>(self->context, Ctx::kDevice);
}
inline DeviceData* OwnerDeviceData(ContextData* self) {
    return Field<DeviceData*>(OwnerDevice(self), Dev::kActiveDeviceData);
}
inline ID3D11DeviceContext* Ctx3D(ContextData* self) {
    return static_cast<ID3D11DeviceContext*>(self->d3dContext);
}

// Video::Viewport — 16 bytes, four int32. Shape proven by Runtime's
// ?GetViewport@ (0x18001a0c0) copying Context+0x1d0..0x1dc into it, and used
// here to call ?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z.
struct Viewport {
    int32_t x, y, w, h;
};
static_assert(sizeof(Viewport) == 16, "Video::Viewport is four int32");

// IID_ID3D11Texture2D, transcribed from the 16 bytes at VideoDX11 0x1800186d8
// that slot 2 passes to IDXGISwapChain::GetBuffer:
//   f2 aa 15 6f  08 d2  89 4e  9a b4 48 95 35 d3 4f 9c
// Spelled out here rather than using IID_ID3D11Texture2D so this file does not
// depend on dxguid.lib being on the link line, and so the bytes in the shipped
// DLL are visible next to the call that uses them.
const GUID kIID_ID3D11Texture2D =
    { 0x6f15aaf2, 0xd208, 0x4e89, { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c } };

// ============================================================
// Forwarding to the shipped implementation
//
// Used only when a Runtime import failed to resolve. The seam keeps every
// original target, so a missing import degrades to "the shipped DLL runs this
// slot" instead of a null dereference.
// ============================================================
template <typename Fn>
inline Fn Orig(int slot) {
    return reinterpret_cast<Fn>(Dx11OriginalSlot(Vt::ContextData, slot));
}

// ============================================================
// Slot 0 — scalar deleting destructor  (0x180001600)
//
//     18000161d  LEA  RAX,[0x1800186f0] / MOV [RCX],RAX
//     180001627  CALL [0x180013470]   ; Video::ContextData::~ContextData
//     180001635  CALL [0x1800134d0]   ; MemFree, only when (flags & 1)
//                returns `this`
//
// Note what is NOT here: no Release of this->d3dContext. The cached context is
// borrowed — slot 1 copies it out of the DeviceData without an AddRef, so
// releasing it here would be an over-release. Preserved by doing nothing.
// ============================================================
void* ContextData_Dtor(ContextData* self, unsigned flags) {
    self->vptr = Dx11VtableAddress(Vt::ContextData);

    static Dx11BaseDtor_t base = reinterpret_cast<Dx11BaseDtor_t>(
        Dx11ResolveRuntimeImport("??1ContextData@Video@@UEAA@XZ", 0x180013470));
    if (base) base(self);

    if (flags & 1) Dx11MemFree(self);
    return self;
}

// ============================================================
// Slot 1 — bool BeginCommands()  (0x180001650)
//
// Start-of-frame. Zero the four statistics counters, bail out if the device
// wants a swap-chain rebuild, otherwise take the device lock, reset the render
// target to the default one and the viewport to the whole backbuffer, and
// refresh the cached ID3D11DeviceContext.
//
// The counters are zeroed BEFORE the reset check, so a frame that is skipped
// still reports zeros. Reproduced.
//
// The original re-loads `this->context` before every one of those four stores
// (0x180001668, 0x18000166e, ...). Using one local is equivalent: nothing on
// the path writes ContextData+0x08.
// ============================================================
bool ContextData_BeginCommands(ContextData* self) {
    const Imports* im = GetImports();
    if (!im) return Orig<bool (*)(ContextData*)>(1)(self);

    void* ctx = self->context;
    Field<int32_t>(ctx, Ctx::kTrianglesSubmitted) = 0;
    Field<int32_t>(ctx, Ctx::kVerticesSubmitted)  = 0;
    Field<int32_t>(ctx, Ctx::kDrawsSubmitted)     = 0;
    Field<int32_t>(ctx, Ctx::kObjectsSubmitted)   = 0;

    void* device = Field<void*>(ctx, Ctx::kDevice);
    if (Field<uint8_t>(device, Dev::kResetPending) != 0) {
        // 0x180004340 — DeviceData swap-chain resize/recreate. Lane L6's
        // function; called through the shipped code, and its result is
        // discarded exactly as the original discards it (`XOR AL,AL`).
        im->DeviceDataResize(Field<void*>(device, Dev::kActiveDeviceData));
        return false;
    }

    im->DeviceLock(device);
    im->CtxSetRenderBuffer(ctx, Field<void*>(ctx, Ctx::kDefaultRenderBuffer));

    Viewport vp;
    vp.x = 0;
    vp.y = 0;
    vp.w = Field<int32_t>(device, Dev::kWidth);
    vp.h = Field<int32_t>(device, Dev::kHeight);
    im->CtxSetViewport(ctx, &vp);

    self->d3dContext = OwnerDeviceData(self)->d3dContext;
    return true;
}

// ============================================================
// Slot 2 — void EndCommands()  (0x180001720)
//
// Resolve the (possibly multisampled) colour buffer into the swap chain's back
// buffer. Nothing is presented here; slot 3 does that.
//
//     180001747  AddRef(deviceData->colorBuffer)        [+0x08]
//     180001763  swapChain->GetBuffer(0, IID_Texture2D, &backBuffer)  [+0x48]
//     180001788  ResolveSubresource(backBuffer, 0, colorBuffer, 0, 0x57)
//     180001794  Release(colorBuffer)
//     18000179f  Release(backBuffer)
//
// The AddRef/Release pair around the colour buffer is a no-op in net terms and
// is kept because it is observable in a PIX/RenderDoc reference-count view.
//
// The original never checks GetBuffer's HRESULT and never initialises the
// out-pointer — `LEA R9,[RSP+0x40]` points at its own argument home slot. It
// then Releases whatever came back unconditionally. This port initialises the
// local to nullptr (reading an uninitialised object would be UB in C++) and
// still Releases unconditionally, so a GetBuffer failure faults here just as it
// faults in the shipped DLL rather than being silently swallowed.
// ============================================================
void ContextData_EndCommands(ContextData* self) {
    DeviceData* dd = OwnerDeviceData(self);

    IUnknown* color = static_cast<IUnknown*>(dd->colorBuffer);
    color->AddRef();

    ID3D11Resource* backBuffer = nullptr;
    static_cast<IDXGISwapChain*>(dd->swapChain)->GetBuffer(
        0, kIID_ID3D11Texture2D, reinterpret_cast<void**>(&backBuffer));

    Ctx3D(self)->ResolveSubresource(backBuffer, 0,
                                    static_cast<ID3D11Resource*>(dd->colorBuffer), 0,
                                    DXGI_FORMAT_B8G8R8A8_UNORM);

    color->Release();
    backBuffer->Release();
}
static_assert(static_cast<int>(DXGI_FORMAT_B8G8R8A8_UNORM) == 0x57,
              "the literal 0x57 stored at 0x180001778");

// ============================================================
// Slot 3 — void Finish()  (0x1800017c0)
//
// Present, then the whole device-lost recovery path.
//
//   1. Present(Configuration+0x41 ? 1 : 0, 0) and drop the device lock.
//   2. Success -> done.
//   3. If the failure is not already DEVICE_REMOVED / DEVICE_RESET, ask
//      ID3D11Device::GetDeviceRemovedReason() for the real one. The original
//      writes this as `(hr + 0x7785fffb) & 0xfffffffd`, which is zero for
//      exactly hr == 0x887a0005 and hr == 0x887a0007 and nothing else; the two
//      explicit comparisons below are the same predicate.
//   4. A zero reason means the Present error was transient -> done.
//   5. Otherwise: notify (Device+0x50), Video::Device::ReleaseDynamic, NULL the
//      ID3D11Device, run DeviceData slot 3 (shutdown) and slot 2 (create).
//      On success: RecreateDynamic + notify (Device+0x58), tail-called.
//      On failure: System::Error::SetText(msg, true) then ::Display().
//
// The `dd->d3dDevice = nullptr` at step 5 happens BEFORE shutdown
// (0x180001857), i.e. it deliberately stops the shutdown path from Releasing a
// device that is already gone. That ordering is load-bearing, not incidental.
//
// The DeviceData slots are invoked through the LIVE vtable, so if lane L6 later
// registers them this code picks up the port automatically — the same thing the
// original does, because the original also dispatches virtually.
// ============================================================
void ContextData_Finish(ContextData* self) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(ContextData*)>(3)(self); return; }

    void*       device = OwnerDevice(self);
    DeviceData* dd     = Field<DeviceData*>(device, Dev::kActiveDeviceData);

    const void* config = *im->configSingleton;
    const UINT  sync   = Field<uint8_t>(config, kConfigVSync) != 0 ? 1u : 0u;

    HRESULT hr = static_cast<IDXGISwapChain*>(dd->swapChain)->Present(sync, 0);
    im->DeviceUnlock(device);
    if (hr == 0) return;

    if (hr != DXGI_ERROR_DEVICE_REMOVED && hr != DXGI_ERROR_DEVICE_RESET)
        hr = static_cast<ID3D11Device*>(dd->d3dDevice)->GetDeviceRemovedReason();
    if (hr == 0) return;

    void* owner = dd->device;   // DeviceData+0x08, the Video::Device*

    DeviceNotifyFn onLost = Field<DeviceNotifyFn>(owner, Dev::kOnDeviceLost);
    if (onLost) onLost();

    im->DeviceReleaseDynamic(owner);

    dd->d3dDevice = nullptr;

    void** ddVtable = static_cast<void**>(dd->vptr);
    reinterpret_cast<void (*)(DeviceData*)>(ddVtable[kSlot_Shutdown])(dd);
    const bool created =
        reinterpret_cast<bool (*)(DeviceData*)>(ddVtable[kSlot_CreateDevice])(dd);

    if (!created) {
        // Same five strings, same fallback, same order of tests as
        // 0x18000189a-0x1800018f6. They are our own literals rather than the
        // .rdata copies in the shipped DLL; System::Error::SetText copies the
        // text, so the observable result is identical.
        const char* msg;
        if      (hr == DXGI_ERROR_INVALID_CALL)           msg = "D3D11 Error: DXGI_ERROR_INVALID_CALL!";
        else if (hr == DXGI_ERROR_DEVICE_REMOVED)         msg = "D3D11 Error: DXGI_ERROR_DEVICE_REMOVED!";
        else if (hr == DXGI_ERROR_DEVICE_HUNG)            msg = "D3D11 Error: DXGI_ERROR_DEVICE_HUNG!";
        else if (hr == DXGI_ERROR_DEVICE_RESET)           msg = "D3D11 Error: DXGI_ERROR_DEVICE_RESET!";
        else if (hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)  msg = "D3D11 Error: DXGI_ERROR_DRIVER_INTERNAL_ERROR!";
        else                                              msg = "D3D11 Error: UNKNOWN!";
        im->ErrorSetText(msg, true);
        im->ErrorDisplay();
        return;
    }

    im->DeviceRecreateDynamic(owner);

    DeviceNotifyFn onRestored = Field<DeviceNotifyFn>(owner, Dev::kOnDeviceRestored);
    if (onRestored) onRestored();
}
static_assert(static_cast<unsigned>(DXGI_ERROR_DEVICE_REMOVED) == 0x887a0005u, "hr constant");
static_assert(static_cast<unsigned>(DXGI_ERROR_DEVICE_RESET)   == 0x887a0007u, "hr constant");
static_assert(static_cast<unsigned>(DXGI_ERROR_INVALID_CALL)   == 0x887a0001u, "hr constant");
static_assert(static_cast<unsigned>(DXGI_ERROR_DEVICE_HUNG)    == 0x887a0006u, "hr constant");
static_assert(static_cast<unsigned>(DXGI_ERROR_DRIVER_INTERNAL_ERROR) == 0x887a0020u, "hr constant");

// Forward declarations: slot 4 calls slots 5 and 6 directly (near CALLs at
// 0x18000194e and 0x180001963), not through the vtable. The port keeps that —
// a vtable dispatch would let a half-enabled seam mix ported and un-ported
// halves of one Clear.
void ContextData_BindRenderBuffer(ContextData* self);
void ContextData_BindViewport(ContextData* self);

// Build the D3D11_RECT slots 4 and 7 both use: {x, y, x + w, y + h} from
// Context+0x1e0..0x1ec. The shipped DLL has two copies of this (inlined into
// 0x180001920 and standing alone at 0x180001be0); one helper emits the same
// RSSetScissorRects call from both.
inline void SetScissorFromContext(ContextData* self, void* ctx) {
    D3D11_RECT r;
    r.left   = Field<int32_t>(ctx, Ctx::kScissor + 0x0);
    r.top    = Field<int32_t>(ctx, Ctx::kScissor + 0x4);
    r.right  = Field<int32_t>(ctx, Ctx::kScissor + 0x8) +
               Field<int32_t>(ctx, Ctx::kScissor + 0x0);
    r.bottom = Field<int32_t>(ctx, Ctx::kScissor + 0xc) +
               Field<int32_t>(ctx, Ctx::kScissor + 0x4);
    Ctx3D(self)->RSSetScissorRects(1, &r);
}

// ============================================================
// Slot 4 — void Clear(unsigned flags, unsigned color, float depth, int stencil)
//          (0x180001920)
//
// Signature is Runtime's: ?Clear@Context@Video@@QEAAXIIMH@Z. The register
// assignment matches exactly — EDX flags, R8D colour, XMM3 depth, and the
// stencil in the fifth argument slot, which the backend reads as a BYTE
// (`MOVZX EAX, byte ptr [RSP+0x80]` at 0x180001a99).
//
// Clearing forces the three lazily-bound pieces of state to be flushed first,
// because a clear targets whatever is currently bound:
//     flags & 0x0001 -> BindRenderBuffer()
//     flags & 0x0200 -> BindViewport()
//     flags & 0x0400 -> RSSetScissorRects(...)
// then all three dirty bits are cleared in one AND.
//
// The colour word is 0xAARRGGBB: the four components are extracted as
// (c>>16)&0xff, (c>>8)&0xff, c&0xff, c>>24 and each is DIVIDED by 255.0f
// (0x180018c54 = 0x437f0000). Division, not multiplication by a reciprocal —
// the two do not give bit-identical results, and this value ends up in a clear
// colour that a RenderDoc capture shows to the last bit.
//
// The clear flags are the engine's own bit 1 = depth, bit 2 = stencil, mapped
// onto D3D11_CLEAR_DEPTH / D3D11_CLEAR_STENCIL; ClearDepthStencilView is
// skipped entirely when neither is asked for.
// ============================================================
void ContextData_Clear(ContextData* self, unsigned flags, unsigned color,
                       float depth, int stencil) {
    void* ctx = self->context;

    if (Field<uint8_t>(ctx, Ctx::kDirtyFlags) & 0x01)
        ContextData_BindRenderBuffer(self);

    if (Field<uint32_t>(self->context, Ctx::kDirtyFlags) & 0x200)
        ContextData_BindViewport(self);

    ctx = self->context;
    if (Field<uint32_t>(ctx, Ctx::kDirtyFlags) & 0x400)
        SetScissorFromContext(self, ctx);

    Field<uint32_t>(self->context, Ctx::kDirtyFlags) &= 0xfffff9feu;

    if (flags & 1) {
        const float rgba[4] = {
            static_cast<float>((color >> 16) & 0xff) / 255.0f,
            static_cast<float>((color >>  8) & 0xff) / 255.0f,
            static_cast<float>( color        & 0xff) / 255.0f,
            static_cast<float>( color >> 24)         / 255.0f,
        };
        const void* rb = Field<void*>(self->context, Ctx::kRenderBuffer);
        Ctx3D(self)->ClearRenderTargetView(
            Dx11ImageRtv(static_cast<const RenderBufferOwner*>(rb)->color), rgba);
    }

    UINT clearFlags = (flags & 2) ? D3D11_CLEAR_DEPTH : 0;
    if (flags & 4) clearFlags |= D3D11_CLEAR_STENCIL;

    if (clearFlags) {
        const void* rb = Field<void*>(self->context, Ctx::kRenderBuffer);
        Ctx3D(self)->ClearDepthStencilView(
            Dx11ImageDsv(static_cast<const RenderBufferOwner*>(rb)->depth),
            clearFlags, depth, static_cast<UINT8>(stencil));
    }
}
static_assert(D3D11_CLEAR_DEPTH == 1 && D3D11_CLEAR_STENCIL == 2,
              "the literals 1 and 2 at 0x180001a6c / 0x180001a76");

// ============================================================
// Slot 5 — void BindRenderBuffer()  (0x180001ad0)
//
// Break any read-after-write hazard, flush the texture bindings if they are
// dirty, then let Runtime bind the render buffer (which reaches
// RenderBufferData::Bind -> OMSetRenderTargets, lane L2's slot).
//
// The hazard loop walks all 16 texture slots and unbinds any that currently
// hold the colour or depth ImageBuffer of the render buffer about to be bound
// as a target. It calls Video::Context::SetImageBuffer(nullptr, i) rather than
// touching D3D11 directly, so the unbind is recorded on the Context and flushed
// with the rest.
//
// The original re-reads `this->context` on every iteration (0x180001af0 is
// inside the loop) and re-reads Context+0x28 each time too. Kept, because
// SetImageBuffer is a Runtime call and this file cannot prove it leaves those
// two fields alone.
//
// This is a near CALL from slot 4 as well as a vtable slot; both paths run the
// identical body.
// ============================================================
void ContextData_BindRenderBuffer(ContextData* self) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(ContextData*)>(5)(self); return; }

    for (int i = 0; i < Ctx::kTextureSlotCount; ++i) {
        void* ctx = self->context;
        void* bound = Field<void*>(ctx, Ctx::kTextures + static_cast<size_t>(i) * 8);
        const RenderBufferOwner* rb =
            Field<const RenderBufferOwner*>(ctx, Ctx::kRenderBuffer);
        if (bound == rb->color || bound == rb->depth)
            im->CtxSetImageBuffer(ctx, nullptr, i);
    }

    void* ctx = self->context;
    if (Field<uint8_t>(ctx, Ctx::kDirtyFlags) & 0x02) {
        im->CtxBindTextures(ctx);
        Field<uint32_t>(self->context, Ctx::kDirtyFlags) &= 0xfffffffdu;
    }

    ctx = self->context;
    im->RenderBufferBind(Field<void*>(ctx, Ctx::kRenderBuffer), ctx);
}

// ============================================================
// Slot 6 — void BindViewport()  (0x180001b60)
//
// RSSetViewports(1, {(float)x, (float)y, (float)w, (float)h, 0.0f, 1.0f}) from
// the int32 viewport at Context+0x1d0. MaxDepth comes from the 1.0f constant at
// 0x180018c50; MinDepth is an `xorps`'d zero.
// ============================================================
void ContextData_BindViewport(ContextData* self) {
    void* ctx = self->context;

    D3D11_VIEWPORT vp;
    vp.TopLeftX = static_cast<float>(Field<int32_t>(ctx, Ctx::kViewport + 0x0));
    vp.TopLeftY = static_cast<float>(Field<int32_t>(ctx, Ctx::kViewport + 0x4));
    vp.Width    = static_cast<float>(Field<int32_t>(ctx, Ctx::kViewport + 0x8));
    vp.Height   = static_cast<float>(Field<int32_t>(ctx, Ctx::kViewport + 0xc));
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    Ctx3D(self)->RSSetViewports(1, &vp);
}

// ============================================================
// Slot 7 — void BindScissor()  (0x180001be0)
// ============================================================
void ContextData_BindScissor(ContextData* self) {
    SetScissorFromContext(self, self->context);
}

// ============================================================
// Slot 8 — void BindTransforms()  (0x180001c40)
//
// Upload the two constant buffers that live INSIDE the Video::Context object:
//
//   Context+0x2050  transforms  <- Context+0x10d8, (Context+0x20 << 6) bytes
//                                  guarded by dirty bit 0x4000
//   Context+0x2008  bones       <- Context+0x0418, exactly 0xcc0 bytes
//                                  guarded by Context+0x24 > 0
//
// The <<6 says a transform is a 64-byte 4x4 matrix, and 0x0418 + 0xcc0 ==
// 0x10d8 says the bone block ends exactly where the transform block starts.
//
// Two asymmetries worth noting, both preserved: the transform upload is gated
// on a dirty bit while the bone upload is gated on a count, and the bone size
// is a constant rather than derived from that count.
//
// Signature of the callee (from the import table):
//     ?Bind@ConstantBuffer@Video@@QEAAXAEAVContext@2@PEAXHH@Z
//       void ConstantBuffer::Bind(Context&, void* data, int offset, int size)
// The `offset` argument is the literal 0 in both calls.
// ============================================================
void ContextData_BindTransforms(ContextData* self) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(ContextData*)>(8)(self); return; }

    void* ctx = self->context;
    if (Field<uint32_t>(ctx, Ctx::kDirtyFlags) & 0x4000) {
        im->ConstantBufferBind(FieldPtr(ctx, Ctx::kTransformConstantBuffer),
                               ctx,
                               FieldPtr(ctx, Ctx::kTransformData),
                               0,
                               Field<int32_t>(ctx, Ctx::kTransformCount) << 6);
    }

    ctx = self->context;
    if (Field<int32_t>(ctx, Ctx::kBoneCount) > 0) {
        im->ConstantBufferBind(FieldPtr(ctx, Ctx::kBoneConstantBuffer),
                               ctx,
                               FieldPtr(ctx, Ctx::kBoneData),
                               0,
                               Ctx::kBoneDataBytes);
    }
}
static_assert(Ctx::kBoneData + Ctx::kBoneDataBytes == Ctx::kTransformData,
              "bone block is adjacent to the transform block");

// ============================================================
// Slot 9 — void BindTextureNone(int slot)  (0x180001cb0)
//
// PSSetSamplers(slot, 1, {NULL}) then PSSetShaderResources(slot, 1, {NULL}).
// That order is the original's (index 10 first, then index 8) and is kept: the
// two calls appear in that sequence in any capture.
//
// Pixel stage only — the vertex stage's sampler/SRV slots are never unbound.
// ============================================================
void ContextData_BindTextureNone(ContextData* self, int slot) {
    ID3D11SamplerState*       nullSampler = nullptr;
    ID3D11ShaderResourceView* nullSrv     = nullptr;

    Ctx3D(self)->PSSetSamplers(static_cast<UINT>(slot), 1, &nullSampler);
    Ctx3D(self)->PSSetShaderResources(static_cast<UINT>(slot), 1, &nullSrv);
}

// ============================================================
// Slot 10 — void BindIndexNone()  (0x180001d10, a 22-byte tail)
//
// IASetIndexBuffer(NULL, DXGI_FORMAT_R16_UINT, 0). The format is the literal
// 0x39 built with `LEA R8D,[R9+0x39]` off an already-zeroed R9 — a null index
// buffer still gets a format, and it is the same 16-bit format
// IndexBufferData::Bind uses (lane L1, 0x180003380).
//
// This slot is the proof that ContextData+0x18 is the cached context: it loads
// [RCX+0x18] and calls straight through it with no singleton walk at all.
// ============================================================
void ContextData_BindIndexNone(ContextData* self) {
    Ctx3D(self)->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
}
static_assert(static_cast<int>(DXGI_FORMAT_R16_UINT) == 0x39,
              "the literal 0x39 encoded at 0x180001d19");

// ============================================================
// Slot 11 — void BindVertexNone(int slot)  (0x180001d30)
//
// IASetVertexBuffers(slot, 1, {NULL}, {0}, {0}). The stride and offset arrays
// are separate zeroed stack dwords in the original; both are passed by address.
// ============================================================
void ContextData_BindVertexNone(ContextData* self, int slot) {
    ID3D11Buffer* nullBuffer = nullptr;
    UINT          stride     = 0;
    UINT          offset     = 0;

    Ctx3D(self)->IASetVertexBuffers(static_cast<UINT>(slot), 1,
                                    &nullBuffer, &stride, &offset);
}

// ============================================================
// Slot 12 — void DrawIndexed(PrimitiveType, int baseVertex, int vertexCount,
//                            int startIndex, int indexCount, int instanceCount)
//          (0x180001d80)
//
// Signature from ?DrawIndexed@Context@Video@@QEAAXW4PrimitiveType@12@HHHHH@Z.
// The parameter roles come from where each register lands in the D3D11 call:
//
//     DrawIndexed(indexCount, startIndex, baseVertex)              instanceCount == 1
//     DrawIndexedInstanced(indexCount, instanceCount,
//                          startIndex, baseVertex, 0)              otherwise
//
// `vertexCount` never reaches D3D11 — it exists only to feed the
// verticesSubmitted counter.
//
// *** THE PrimitiveType ARGUMENT IS IGNORED. *** This slot unconditionally
// forces D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, whatever it was handed
// (0x180001de4 compares this->topology against the literal 4 and never looks at
// RDX; RDX is in fact destroyed by the div-by-3 IMUL at 0x180001d9c before it
// could be read). Slots 15/19 by contrast do honour their PrimitiveType. So on
// this backend a non-triangle DrawIndexed silently draws triangles. That is a
// bug in the shipped DLL; reproducing it is the whole point of the exercise.
//
// The triangle counter uses `indexCount / 3` — the assembly is the canonical
// signed divide-by-3 (magic 0x55555556, `MOV EAX,EDX; SHR EAX,31; ADD EDX,EAX`),
// which is exactly C's truncating `/ 3`, negatives included.
// ============================================================
void ContextData_DrawIndexed(ContextData* self, int primitiveType,
                             int baseVertex, int vertexCount,
                             int startIndex, int indexCount, int instanceCount) {
    const Imports* im = GetImports();
    if (!im) {
        Orig<void (*)(ContextData*, int, int, int, int, int, int)>(12)(
            self, primitiveType, baseVertex, vertexCount,
            startIndex, indexCount, instanceCount);
        return;
    }
    (void)primitiveType;   // deliberately unused past this point — see above

    void* ctx = self->context;
    Field<int32_t>(ctx, Ctx::kTrianglesSubmitted) += (indexCount / 3) * instanceCount;
    Field<int32_t>(self->context, Ctx::kVerticesSubmitted) += vertexCount * instanceCount;
    Field<int32_t>(self->context, Ctx::kDrawsSubmitted)    += 1;
    Field<int32_t>(self->context, Ctx::kObjectsSubmitted)  += instanceCount;

    im->CtxBindResources(self->context);

    if (self->topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) {
        self->topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        Ctx3D(self)->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    if (instanceCount == 1) {
        Ctx3D(self)->DrawIndexed(static_cast<UINT>(indexCount),
                                 static_cast<UINT>(startIndex),
                                 baseVertex);
    } else {
        Ctx3D(self)->DrawIndexedInstanced(static_cast<UINT>(indexCount),
                                          static_cast<UINT>(instanceCount),
                                          static_cast<UINT>(startIndex),
                                          baseVertex,
                                          0);
    }
}
static_assert(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST == 4, "topology literal 4");
static_assert(D3D11_PRIMITIVE_TOPOLOGY_LINELIST == 2, "topology literal 2");

// ============================================================
// Slot 13 — int GetMaxDynamicInstances(int, int, int) const
//          (0x180001ef0: `b8 34 00 00 00 / c3`)
//
// Returns 52 and ignores all three arguments. What the 52 *means* is not
// determinable from this DLL: it is a bare immediate with no relationship to
// any other constant here. It is not the bone-block capacity (0xcc0 / 64 = 51)
// and not the transform-block size. Left as the literal.
// ============================================================
int ContextData_GetMaxDynamicInstances(ContextData* /*self*/,
                                       int /*a*/, int /*b*/, int /*c*/) {
    return 0x34;
}

// ============================================================
// Slot 14 — bool BeginVertices(int vertexCount, int vertexStride)
//          (0x180001e60)
//
// Sub-allocate `vertexCount * vertexStride` bytes out of the Context's shared
// dynamic vertex buffer (Context+0x20f8) and leave the mapped pointer at
// Context+0x20e0 for Video::Context::AddVertex to fill.
//
// The capacity test reads the FIRST DWORD of the Video::VertexBuffer
// (`MOV ECX,[RAX]`) and is a strict `<`: a request equal to the capacity is
// rejected. VertexBuffer+0x00 is not otherwise identified by this DLL; the
// context makes it a byte capacity.
//
// Which argument is which is settled by slot 15, which passes Context+0x2104 to
// ID3D11DeviceContext::Draw as the vertex count, and by Runtime's AddVertex
// advancing the cursor by Context+0x20e8. So EDX is the count and R8D the
// stride, matching ?BeginVertices@Context@Video@@QEAA_NHH@Z.
//
// Note the return is `Context+0x20e0 != 0` re-read from memory, not the local
// the Lock returned. Same value; kept literal.
// ============================================================
bool ContextData_BeginVertices(ContextData* self, int vertexCount, int vertexStride) {
    const Imports* im = GetImports();
    if (!im) return Orig<bool (*)(ContextData*, int, int)>(14)(self, vertexCount, vertexStride);

    void* ctx = self->context;
    void* vb  = Field<void*>(ctx, Ctx::kDynVertexBuffer);
    if (*static_cast<const int32_t*>(vb) <= vertexCount * vertexStride)
        return false;

    Field<int32_t>(self->context, Ctx::kDynVertexStride) = vertexStride;
    Field<int32_t>(self->context, Ctx::kDynVertexCount)  = vertexCount;

    ctx = self->context;
    void* mapped = im->VertexBufferLock(
        Field<void*>(ctx, Ctx::kDynVertexBuffer),
        vertexCount * vertexStride,
        &Field<int32_t>(ctx, Ctx::kDynVertexOffset));

    Field<void*>(self->context, Ctx::kDynVertexCursor) = mapped;
    Field<void*>(self->context, Ctx::kDynVertexBase) =
        Field<void*>(self->context, Ctx::kDynVertexCursor);

    return Field<void*>(self->context, Ctx::kDynVertexCursor) != nullptr;
}

// Topology filter shared by slots 15, 17 and 19 (identical code in each). The
// engine's PrimitiveType 0 maps to TRIANGLELIST and 2 to LINELIST; every other
// value does NOTHING AT ALL — no call, and no update of the cache — so the
// draw inherits whatever topology was last set. Preserved verbatim: the values
// 1 and 3 are presumably strips that this backend never implemented.
inline void ApplyPrimitiveType(ContextData* self, int primitiveType) {
    int topology;
    if (primitiveType == 0) {
        if (self->topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST) return;
        topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    } else if (primitiveType == 2) {
        if (self->topology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST) return;
        topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    } else {
        return;
    }
    self->topology = topology;
    Ctx3D(self)->IASetPrimitiveTopology(
        static_cast<D3D11_PRIMITIVE_TOPOLOGY>(topology));
}

// ============================================================
// Slot 15 — void EndVertices(PrimitiveType)  (0x180001f00)
//
// Unlock, rebind the dynamic vertex buffer at the offset the Lock handed out,
// account, flush the resource bindings, apply the topology, and draw.
//
// The cursor is nulled BEFORE the Unlock (0x180001f13), which is the reverse of
// the obvious order and is kept.
//
// Statistics here count one object and one draw; the triangle count is
// `vertexCount / 3` (no index buffer involved).
//
// The final Draw is a tail jump: Draw(Context+0x2104, 0).
// ============================================================
void ContextData_EndVertices(ContextData* self, int primitiveType) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(ContextData*, int)>(15)(self, primitiveType); return; }

    Field<void*>(self->context, Ctx::kDynVertexCursor) = nullptr;
    im->VertexBufferUnlock(Field<void*>(self->context, Ctx::kDynVertexBuffer));

    void* ctx = self->context;
    im->CtxSetVertexBuffer(ctx,
                           Field<void*>(ctx, Ctx::kDynVertexBuffer),
                           0,
                           Field<int32_t>(ctx, Ctx::kDynVertexOffset),
                           Field<int32_t>(ctx, Ctx::kDynVertexStride));

    ctx = self->context;
    const int vertexCount = Field<int32_t>(ctx, Ctx::kDynVertexCount);
    Field<int32_t>(ctx, Ctx::kTrianglesSubmitted) += vertexCount / 3;
    Field<int32_t>(self->context, Ctx::kVerticesSubmitted) +=
        Field<int32_t>(self->context, Ctx::kDynVertexCount);
    Field<int32_t>(self->context, Ctx::kDrawsSubmitted)   += 1;
    Field<int32_t>(self->context, Ctx::kObjectsSubmitted) += 1;

    im->CtxBindResources(self->context);

    ApplyPrimitiveType(self, primitiveType);

    Ctx3D(self)->Draw(
        static_cast<UINT>(Field<int32_t>(self->context, Ctx::kDynVertexCount)), 0);
}

// Forward declaration: slot 16 is a two-instruction shim over slot 18.
bool ContextData_BeginInstancedIndexedVertices(ContextData* self, int vertexCount,
                                               int vertexStride, int indexCount,
                                               int instanceCount);

// ============================================================
// Slot 16 — bool BeginIndexedVertices(int vertexCount, int vertexStride,
//                                     int indexCount)  (0x180002000)
//
//     180002004  MOV dword ptr [RSP+0x20],0x1   ; instanceCount = 1
//     18000200c  CALL 0x180002030               ; slot 18
//
// A literal near CALL, so the port calls the same function directly.
// ============================================================
bool ContextData_BeginIndexedVertices(ContextData* self, int vertexCount,
                                      int vertexStride, int indexCount) {
    return ContextData_BeginInstancedIndexedVertices(self, vertexCount,
                                                     vertexStride, indexCount, 1);
}

// ============================================================
// Slot 18 — bool BeginInstancedIndexedVertices(int vertexCount,
//               int vertexStride, int indexCount, int instanceCount)
//          (0x180002030)
//
// Same sub-allocation as slot 14, plus an index allocation out of
// Context+0x2118. Indices are 16-bit, hence `indexCount * 2` bytes.
//
// *** ORIGINAL BUG, REPRODUCED. *** The second capacity test compares the index
// byte count against the VERTEX buffer's capacity:
//
//     18000204e  MOV ECX,[RAX]        ; RAX = Context+0x20f8, the VertexBuffer
//     18000206a  LEA EDI,[RSI+RSI]    ; indexCount * 2
//     18000206d  CMP EDI,ECX
//     18000206f  JGE fail
//
// ECX is never reloaded from the index buffer at Context+0x2118. Whether that
// is harmless depends on the two buffers' relative sizes, which this DLL does
// not reveal. Fixing it would change which draws are rejected, so it stands.
//
// Note also the two tests use different comparisons: the vertex test rejects
// `capacity <= bytes` and the index test rejects `bytes >= capacity`. Those are
// the same predicate, written differently in the original; no asymmetry.
// ============================================================
bool ContextData_BeginInstancedIndexedVertices(ContextData* self, int vertexCount,
                                               int vertexStride, int indexCount,
                                               int instanceCount) {
    const Imports* im = GetImports();
    if (!im) {
        return Orig<bool (*)(ContextData*, int, int, int, int)>(18)(
            self, vertexCount, vertexStride, indexCount, instanceCount);
    }

    void* ctx = self->context;
    void* vb  = Field<void*>(ctx, Ctx::kDynVertexBuffer);
    const int32_t vertexCapacity = *static_cast<const int32_t*>(vb);

    if (vertexCapacity <= vertexCount * vertexStride) return false;
    if (indexCount * 2 >= vertexCapacity) return false;   // sic — see above

    Field<int32_t>(ctx, Ctx::kDynVertexStride) = vertexStride;
    Field<int32_t>(self->context, Ctx::kDynVertexCount)    = vertexCount;
    Field<int32_t>(self->context, Ctx::kDynInstanceCount)  = instanceCount;

    ctx = self->context;
    void* mappedVertices = im->VertexBufferLock(
        Field<void*>(ctx, Ctx::kDynVertexBuffer),
        vertexCount * vertexStride,
        &Field<int32_t>(ctx, Ctx::kDynVertexOffset));

    Field<void*>(self->context, Ctx::kDynVertexCursor) = mappedVertices;
    Field<void*>(self->context, Ctx::kDynVertexBase) =
        Field<void*>(self->context, Ctx::kDynVertexCursor);

    if (Field<void*>(self->context, Ctx::kDynVertexCursor) == nullptr) return false;

    Field<int32_t>(self->context, Ctx::kDynIndexCount) = indexCount;

    ctx = self->context;
    void* mappedIndices = im->IndexBufferLock(
        Field<void*>(ctx, Ctx::kDynIndexBuffer),
        indexCount * 2,
        &Field<int32_t>(ctx, Ctx::kDynIndexOffset));

    Field<void*>(self->context, Ctx::kDynIndexCursor) = mappedIndices;
    Field<void*>(self->context, Ctx::kDynIndexBase) =
        Field<void*>(self->context, Ctx::kDynIndexCursor);

    return Field<void*>(self->context, Ctx::kDynIndexCursor) != nullptr;
}

// ============================================================
// Slots 17 and 19 — void EndIndexedVertices(PrimitiveType) /
//                   void EndInstancedIndexedVertices(PrimitiveType)
//          (0x180002020 -> 0x180002150; one body, two slots)
//
// Unlock both dynamic buffers, rebind the vertex stream, mark the index binding
// dirty on the Context if it moved, account, flush, apply the topology and draw.
//
// The index-buffer dirty check is the one place this slot writes a Context
// dirty bit (0x10). It compares BOTH the buffer pointer and the byte offset
// against Context+0x1b8 / +0x1c0, which is what the ordinary (non-dynamic)
// SetIndexBuffer path records.
//
// StartIndexLocation is `Context+0x2120 >> 1`: a byte offset converted to a
// 16-bit index count. The original sign-extends the dword to 64 bits and then
// shifts LOGICALLY right (`MOVSXD R8,...` + `SHR R8,1`), so a negative offset
// would produce an enormous index — an impossible input in practice, but the
// cast chain below reproduces it exactly rather than writing `/ 2`.
//
// Statistics use the INSTANCE count as a multiplier on both triangles and
// vertices, and add the instance count (not 1) to objectsSubmitted.
// ============================================================
void ContextData_EndInstancedIndexedVertices(ContextData* self, int primitiveType) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(ContextData*, int)>(19)(self, primitiveType); return; }

    Field<void*>(self->context, Ctx::kDynVertexCursor) = nullptr;
    im->VertexBufferUnlock(Field<void*>(self->context, Ctx::kDynVertexBuffer));

    Field<void*>(self->context, Ctx::kDynIndexCursor) = nullptr;
    im->IndexBufferUnlock(Field<void*>(self->context, Ctx::kDynIndexBuffer));

    void* ctx = self->context;
    im->CtxSetVertexBuffer(ctx,
                           Field<void*>(ctx, Ctx::kDynVertexBuffer),
                           0,
                           Field<int32_t>(ctx, Ctx::kDynVertexOffset),
                           Field<int32_t>(ctx, Ctx::kDynVertexStride));

    ctx = self->context;
    void* const   dynIb     = Field<void*>(ctx, Ctx::kDynIndexBuffer);
    const int32_t dynOffset = Field<int32_t>(ctx, Ctx::kDynIndexOffset);
    if (dynIb     != Field<void*>(ctx, Ctx::kBoundIndexBuffer) ||
        dynOffset != Field<int32_t>(ctx, Ctx::kBoundIndexOffset)) {
        Field<uint32_t>(ctx, Ctx::kDirtyFlags) |= 0x10u;
        Field<void*>(ctx, Ctx::kBoundIndexBuffer) = dynIb;
        Field<int32_t>(ctx, Ctx::kBoundIndexOffset) = dynOffset;
    }

    ctx = self->context;
    const int indexCount    = Field<int32_t>(ctx, Ctx::kDynIndexCount);
    const int instanceCount = Field<int32_t>(ctx, Ctx::kDynInstanceCount);
    Field<int32_t>(ctx, Ctx::kTrianglesSubmitted) += (indexCount / 3) * instanceCount;

    ctx = self->context;
    Field<int32_t>(ctx, Ctx::kVerticesSubmitted) +=
        Field<int32_t>(ctx, Ctx::kDynInstanceCount) *
        Field<int32_t>(ctx, Ctx::kDynVertexCount);
    Field<int32_t>(self->context, Ctx::kDrawsSubmitted) += 1;
    Field<int32_t>(self->context, Ctx::kObjectsSubmitted) +=
        Field<int32_t>(self->context, Ctx::kDynInstanceCount);

    im->CtxBindResources(self->context);

    ApplyPrimitiveType(self, primitiveType);

    ctx = self->context;
    const UINT startIndex = static_cast<UINT>(
        static_cast<uint64_t>(static_cast<int64_t>(
            Field<int32_t>(ctx, Ctx::kDynIndexOffset))) >> 1);

    if (Field<int32_t>(ctx, Ctx::kDynInstanceCount) == 1) {
        Ctx3D(self)->DrawIndexed(
            static_cast<UINT>(Field<int32_t>(ctx, Ctx::kDynIndexCount)),
            startIndex, 0);
    } else {
        Ctx3D(self)->DrawIndexedInstanced(
            static_cast<UINT>(Field<int32_t>(ctx, Ctx::kDynIndexCount)),
            static_cast<UINT>(Field<int32_t>(ctx, Ctx::kDynInstanceCount)),
            startIndex, 0, 0);
    }
}

}  // namespace
}  // namespace VideoDX11

// ---------------------------------------------------------------------------
// Registration
//
// Unproven — every slot is opt-in behind BANISHED_DX11_CONTEXT with
// defaultOn=false, per the kill-switch convention in dx11/funcs/dx11_seam.h.
// Graduation to defaultOn=true happens only after an in-game run and a
// RenderDoc A/B pass, which this lane does not perform.
//
// Slots 17 and 19 share one implementation because 0x180002020 is a `jmp` to
// 0x180002150 — see the comment on the function.
// ---------------------------------------------------------------------------
#define DX11_CONTEXT_FLAG "BANISHED_DX11_CONTEXT"

DX11_REGISTER_SLOT(ContextData,  0, &::VideoDX11::ContextData_Dtor,
                   DX11_CONTEXT_FLAG, false, "ContextData::~ (scalar deleting)");
DX11_REGISTER_SLOT(ContextData,  1, &::VideoDX11::ContextData_BeginCommands,
                   DX11_CONTEXT_FLAG, false, "ContextData::BeginCommands");
DX11_REGISTER_SLOT(ContextData,  2, &::VideoDX11::ContextData_EndCommands,
                   DX11_CONTEXT_FLAG, false, "ContextData::EndCommands");
DX11_REGISTER_SLOT(ContextData,  3, &::VideoDX11::ContextData_Finish,
                   DX11_CONTEXT_FLAG, false, "ContextData::Finish");
DX11_REGISTER_SLOT(ContextData,  4, &::VideoDX11::ContextData_Clear,
                   DX11_CONTEXT_FLAG, false, "ContextData::Clear");
DX11_REGISTER_SLOT(ContextData,  5, &::VideoDX11::ContextData_BindRenderBuffer,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindRenderBuffer");
DX11_REGISTER_SLOT(ContextData,  6, &::VideoDX11::ContextData_BindViewport,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindViewport");
DX11_REGISTER_SLOT(ContextData,  7, &::VideoDX11::ContextData_BindScissor,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindScissor");
DX11_REGISTER_SLOT(ContextData,  8, &::VideoDX11::ContextData_BindTransforms,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindTransforms");
DX11_REGISTER_SLOT(ContextData,  9, &::VideoDX11::ContextData_BindTextureNone,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindTextureNone");
DX11_REGISTER_SLOT(ContextData, 10, &::VideoDX11::ContextData_BindIndexNone,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindIndexNone");
DX11_REGISTER_SLOT(ContextData, 11, &::VideoDX11::ContextData_BindVertexNone,
                   DX11_CONTEXT_FLAG, false, "ContextData::BindVertexNone");
DX11_REGISTER_SLOT(ContextData, 12, &::VideoDX11::ContextData_DrawIndexed,
                   DX11_CONTEXT_FLAG, false, "ContextData::DrawIndexed");
DX11_REGISTER_SLOT(ContextData, 13, &::VideoDX11::ContextData_GetMaxDynamicInstances,
                   DX11_CONTEXT_FLAG, false, "ContextData::GetMaxDynamicInstances (52)");
DX11_REGISTER_SLOT(ContextData, 14, &::VideoDX11::ContextData_BeginVertices,
                   DX11_CONTEXT_FLAG, false, "ContextData::BeginVertices");
DX11_REGISTER_SLOT(ContextData, 15, &::VideoDX11::ContextData_EndVertices,
                   DX11_CONTEXT_FLAG, false, "ContextData::EndVertices");
DX11_REGISTER_SLOT(ContextData, 16, &::VideoDX11::ContextData_BeginIndexedVertices,
                   DX11_CONTEXT_FLAG, false, "ContextData::BeginIndexedVertices");
DX11_REGISTER_SLOT(ContextData, 17, &::VideoDX11::ContextData_EndInstancedIndexedVertices,
                   DX11_CONTEXT_FLAG, false, "ContextData::EndIndexedVertices (= slot 19)");
DX11_REGISTER_SLOT(ContextData, 18, &::VideoDX11::ContextData_BeginInstancedIndexedVertices,
                   DX11_CONTEXT_FLAG, false, "ContextData::BeginInstancedIndexedVertices");
DX11_REGISTER_SLOT(ContextData, 19, &::VideoDX11::ContextData_EndInstancedIndexedVertices,
                   DX11_CONTEXT_FLAG, false, "ContextData::EndInstancedIndexedVertices");
