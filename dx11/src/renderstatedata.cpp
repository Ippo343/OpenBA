// VideoDX11 RenderStateData — all 3 vtable slots (vtable 0x180018818).
//
// This is the class that turns the engine's render-state *description*
// (Video::RenderState, a resource loaded from .rsc data) into the three
// immutable D3D11 state objects, and binds them.
//
//   slot 0  0x1800036e0  scalar deleting dtor  — Release x3, base dtor, MemFree
//   slot 1  0x180003760  Create                — the three CreateXxxState calls
//   slot 2  0x180003a90  Bind                  — OMSetBlendState / OMSetDepthStencilState / RSSetState
//
// Who calls what (verified in Runtime-x64.dll, so the slot roles are not guesses):
//
//   Video::RenderState::PlatformCreate   0x1800f4ea0  -> data->vtable[1]()
//   Video::RenderState::Bind(Context&)   0x1800f4ec0  -> data->vtable[2]()
//   Video::RenderState::PlatformDestruct 0x1800f4e80  -> data->vtable[0](data, 1)
//
// Note that Runtime's Bind takes a `Video::Context&` and passes it in RDX, but
// the DX11 slot 2 never reads RDX: it walks to the *global* immediate context
// instead (see below). Our signature therefore takes and ignores the argument.
//
// ---------------------------------------------------------------------------
// Reaching D3D11
// ---------------------------------------------------------------------------
// RenderStateData caches neither the device nor the context. Both slots 1 and 2
// walk the same chain on every call, exactly as every other resource Bind in
// this DLL does (0x180003a1c and 0x180003a9a in the disassembly):
//
//     mov rax, [0x1800134b8]      ; import ?_singleton@Device@Video@@0PEAV12@EA
//     mov rcx, [rax]              ; Video::Device*
//     mov rax, [rcx+0x100]        ; Video::DeviceData*   (the DX11 one)
//     mov rbx, [rax+0x20]         ; ID3D11Device*        (slot 1)
//     mov rbx, [rax+0x28]         ; ID3D11DeviceContext* (slot 2)
//
// 0x1800134b8 is VideoDX11's import slot for Runtime's private static
// `Video::Device* Video::Device::_singleton`, which lives at Runtime
// 0x1801fe7a8 — i.e. it is a Video::Device**, hence the two loads. DeviceData
// +0x20 / +0x28 are already pinned by dx11/types/VideoDX11Data.h.
//
// ---------------------------------------------------------------------------
// COM vtable offsets — every one verified against the decompilation
// ---------------------------------------------------------------------------
// We call through <d3d11.h>'s C++ interfaces rather than raw vtable indices.
// That is only safe if GCC lays those interfaces out at the offsets the
// original hard-codes, so each one was cross-checked: the index below is the
// position in mingw-w64's ID3D11DeviceVtbl / ID3D11DeviceContextVtbl (counted
// from QueryInterface = 0), and index*8 is the byte offset in the original.
//
//   ID3D11Device        20  CreateBlendState         -> 0xa0   call at 0x180003a3f
//   ID3D11Device        21  CreateDepthStencilState  -> 0xa8   call at 0x180003a68
//   ID3D11Device        22  CreateRasterizerState    -> 0xb0   call at 0x180003a53
//   ID3D11DeviceContext 35  OMSetBlendState          -> 0x118  call at 0x180003ad9
//   ID3D11DeviceContext 36  OMSetDepthStencilState   -> 0x120  call at 0x180003af2
//   ID3D11DeviceContext 43  RSSetState               -> 0x158  call at 0x180003b03
//   IUnknown             2  Release                  -> 0x10   calls at 0x180003713/722/731
//
// NOTE the task brief listed CreateDepthStencilState/BlendState/RasterizerState
// at 0x50/0x58/0x60. That is wrong — those are D3D11-only method indices 10/11/12
// counted past IUnknown, not full-vtable offsets, and it also has Blend and
// DepthStencil the wrong way round. The offsets above are the ones actually
// encoded in the DLL, and they are self-consistent: the object CreateBlendState
// writes to (+0x10) is the one OMSetBlendState later reads, and likewise for
// +0x18 / OMSetDepthStencilState and +0x20 / RSSetState.
//
// ---------------------------------------------------------------------------
// Deliberate deviations from the original (all unobservable to D3D11)
// ---------------------------------------------------------------------------
//  * The original leaves the two padding bytes of D3D11_DEPTH_STENCIL_DESC (the
//    hole after StencilWriteMask, offsets +0x12/+0x13) uninitialised — it stores
//    only a WORD 0xFFFF at +0x10. We value-initialise the whole struct. D3D11
//    does not read those bytes.
//  * The original never checks the HRESULT of the three Create calls and returns
//    `true` unconditionally (`mov al,1` at 0x180003a76). We do the same, so a
//    failed state creation leaves a null pointer that Bind will hand to D3D11 —
//    which is legal and means "default state". Reproduced deliberately.
//  * The original does not release a previously-created state object if Create
//    runs twice. Reproduced (Runtime pairs Create with PlatformDestruct).
//  * The original does not bounds-check any of the six enum lookups. Neither do
//    we: clamping would silently diverge on malformed .rsc data.

#include <cstring>
#include <cstdint>
#include <cstddef>

#include <d3d11.h>

#include "dx11_seam.h"
#include "VideoDX11Data.h"
#include "resolve.h"

// ENDGAME B.3.5 block C, RULE B5-3 — the real `Video::RenderStateData` abstract
// base (block A), so this file's destructor can call its base destructor as a
// qualified non-virtual member call instead of resolving a mangled name.
// B35_RULES 1.2: `dx11/` compiles with the shared include set
// (`-Iruntime/types -Iruntime/funcs`), so no Makefile change is needed. The
// twelve CONCRETE `*Data` classes and the 84-slot seam remain the declared leave
// of RULE B5-8; only the Runtime-side base is a class.
#include "video/CompanionData.h"

namespace VideoDX11 {
namespace {

// ============================================================================
// Video::RenderState — the description object at RenderStateData+0x08
//
// Layout proven by three independent Runtime functions rather than inferred
// from the DX11 reads alone:
//
//   Video::RenderState::RenderState()  0x1800f4b60   default values
//   Video::RenderState::Serialize()    0x1800f4c90   field order + widths
//   Video::RenderState::Stencil::Serialize() 0x1800f48e0
//   Video::RenderState::Depth::Serialize()   0x1800f4a00
//   Video::RenderState::AlphaBlend::Serialize() 0x1800f4aa0
//
// The public API is
//   RenderState::Initialize(const AlphaBlend&, const Depth&, const Stencil&,
//                           CullMode, FillMode, bool)
// (mangled name at Runtime 0x1801dfe97), which is exactly the five groups
// below, in order.
//
// Serializer virtual used per field tells the width: +0x78 = bool,
// +0x80 = float, +0x98 = int/enum, +0xb8 = byte.
// ============================================================================
struct VideoRenderState {
    // +0x00..+0x1f  Object::External base: vptr, System::Filename at +0x08,
    //               an array {ptr,count} at +0x10/+0x18. Not touched here.
    uint8_t  base[0x20];

    // --- AlphaBlend (RenderState::AlphaBlend) ---
    uint8_t  blendEnabled;          // +0x20  bool   default false
    uint8_t  pad21[3];
    int32_t  srcBlend;              // +0x24  enum   default 1 (One)
    int32_t  destBlend;             // +0x28  enum   default 0 (Zero)
    int32_t  blendOp;               // +0x2c  enum   default 0 (Add)
    int32_t  colorWriteMask;        // +0x30  int    default 0xF (RGBA)

    // --- Depth (RenderState::Depth) ---
    uint8_t  depthEnabled;          // +0x34  bool   default true
    uint8_t  depthWriteEnabled;     // +0x35  bool   default true
    uint8_t  pad36[2];
    int32_t  depthFunc;             // +0x38  enum   default 4 (LessEqual)
    float    depthBias;             // +0x3c  float  default 0.0f
    float    slopeScaledDepthBias;  // +0x40  float  default 0.0f

    // --- Stencil (RenderState::Stencil) ---
    uint8_t  stencilEnabled;        // +0x44  bool   default false
    uint8_t  stencilTwoSided;       // +0x45  bool   default false
    uint8_t  pad46[2];
    int32_t  stencilFunc;           // +0x48  enum   default 0 — shared by BOTH faces
    uint8_t  stencilRef;            // +0x4c  byte   default 0
    uint8_t  pad4d[3];
    int32_t  frontFailOp;           // +0x50  enum   default 0 (Keep)
    int32_t  frontDepthFailOp;      // +0x54  enum   default 0 (Keep)
    int32_t  frontPassOp;           // +0x58  enum   default 0 (Keep)
    int32_t  backFailOp;            // +0x5c  enum   default 0 (Keep)
    int32_t  backDepthFailOp;       // +0x60  enum   default 0 (Keep)
    int32_t  backPassOp;            // +0x64  enum   default 0 (Keep)

    // --- top-level ---
    int32_t  cullMode;              // +0x68  enum   default 1 (Back)
    int32_t  fillMode;              // +0x6c  enum   default 0 (Solid)
    uint8_t  scissorEnabled;        // +0x70  bool   default false
    uint8_t  pad71[7];
    void*    platformData;          // +0x78  Video::RenderStateData* (us)
};
static_assert(sizeof(VideoRenderState) == 0x80, "VideoRenderState size");
static_assert(offsetof(VideoRenderState, blendEnabled)         == 0x20, "RenderState::blendEnabled");
static_assert(offsetof(VideoRenderState, srcBlend)             == 0x24, "RenderState::srcBlend");
static_assert(offsetof(VideoRenderState, destBlend)            == 0x28, "RenderState::destBlend");
static_assert(offsetof(VideoRenderState, blendOp)              == 0x2c, "RenderState::blendOp");
static_assert(offsetof(VideoRenderState, colorWriteMask)       == 0x30, "RenderState::colorWriteMask");
static_assert(offsetof(VideoRenderState, depthEnabled)         == 0x34, "RenderState::depthEnabled");
static_assert(offsetof(VideoRenderState, depthWriteEnabled)    == 0x35, "RenderState::depthWriteEnabled");
static_assert(offsetof(VideoRenderState, depthFunc)            == 0x38, "RenderState::depthFunc");
static_assert(offsetof(VideoRenderState, depthBias)            == 0x3c, "RenderState::depthBias");
static_assert(offsetof(VideoRenderState, slopeScaledDepthBias) == 0x40, "RenderState::slopeScaledDepthBias");
static_assert(offsetof(VideoRenderState, stencilEnabled)       == 0x44, "RenderState::stencilEnabled");
static_assert(offsetof(VideoRenderState, stencilTwoSided)      == 0x45, "RenderState::stencilTwoSided");
static_assert(offsetof(VideoRenderState, stencilFunc)          == 0x48, "RenderState::stencilFunc");
static_assert(offsetof(VideoRenderState, stencilRef)           == 0x4c, "RenderState::stencilRef");
static_assert(offsetof(VideoRenderState, frontFailOp)          == 0x50, "RenderState::frontFailOp");
static_assert(offsetof(VideoRenderState, frontDepthFailOp)     == 0x54, "RenderState::frontDepthFailOp");
static_assert(offsetof(VideoRenderState, frontPassOp)          == 0x58, "RenderState::frontPassOp");
static_assert(offsetof(VideoRenderState, backFailOp)           == 0x5c, "RenderState::backFailOp");
static_assert(offsetof(VideoRenderState, backDepthFailOp)      == 0x60, "RenderState::backDepthFailOp");
static_assert(offsetof(VideoRenderState, backPassOp)           == 0x64, "RenderState::backPassOp");
static_assert(offsetof(VideoRenderState, cullMode)             == 0x68, "RenderState::cullMode");
static_assert(offsetof(VideoRenderState, fillMode)             == 0x6c, "RenderState::fillMode");
static_assert(offsetof(VideoRenderState, scissorEnabled)       == 0x70, "RenderState::scissorEnabled");
static_assert(offsetof(VideoRenderState, platformData)         == 0x78, "RenderState::platformData");

// ============================================================================
// The six engine-enum -> D3D11-enum translation tables.
//
// Each is a literal transcription of a stack table the original builds at the
// top of 0x180003760, cited by the instruction that writes it. The *values* are
// therefore exact; the enumerator *names* below are inferred from the D3D11
// constant each slot maps to (and corroborated by RenderState's ctor defaults
// and by Video::RenderState::IsWireframe, 0x18004b730, which is literally
// `return fillMode == 1`).
// ============================================================================

// T1 @ [RSP+0x20..0x44], written at 0x180003783-0x1800037cb. 10 entries.
// D3D11_BLEND. Engine order pairs each factor with its inverse.
//   0 Zero  1 One  2 SrcColor  3 InvSrcColor  4 SrcAlpha  5 InvSrcAlpha
//   6 DestColor  7 InvDestColor  8 DestAlpha  9 InvDestAlpha
static const D3D11_BLEND kBlendFactor[10] = {
    D3D11_BLEND_ZERO,            //  0 -> 1
    D3D11_BLEND_ONE,             //  1 -> 2
    D3D11_BLEND_SRC_COLOR,       //  2 -> 3
    D3D11_BLEND_INV_SRC_COLOR,   //  3 -> 4
    D3D11_BLEND_SRC_ALPHA,       //  4 -> 5
    D3D11_BLEND_INV_SRC_ALPHA,   //  5 -> 6
    D3D11_BLEND_DEST_COLOR,      //  6 -> 9
    D3D11_BLEND_INV_DEST_COLOR,  //  7 -> 10
    D3D11_BLEND_DEST_ALPHA,      //  8 -> 7
    D3D11_BLEND_INV_DEST_ALPHA,  //  9 -> 8
};

// T3b @ [RSP+0x78..0x80], written at 0x18000382c/0x180003834/0x18000383c.
// 3 entries. D3D11_BLEND_OP. Min/Max are not reachable from the engine enum.
static const D3D11_BLEND_OP kBlendOp[3] = {
    D3D11_BLEND_OP_ADD,           // 0 -> 1
    D3D11_BLEND_OP_SUBTRACT,      // 1 -> 2
    D3D11_BLEND_OP_REV_SUBTRACT,  // 2 -> 3
};

// T2 @ [RBP-0x78..-0x5c], written at 0x180003857-0x1800038ae. 8 entries.
// D3D11_COMPARISON_FUNC. Used for BOTH depthFunc and stencilFunc.
static const D3D11_COMPARISON_FUNC kCompareFunc[8] = {
    D3D11_COMPARISON_NEVER,          // 0 -> 1
    D3D11_COMPARISON_ALWAYS,         // 1 -> 8
    D3D11_COMPARISON_EQUAL,          // 2 -> 3
    D3D11_COMPARISON_LESS,           // 3 -> 2
    D3D11_COMPARISON_LESS_EQUAL,     // 4 -> 4   (RenderState ctor default)
    D3D11_COMPARISON_GREATER,        // 5 -> 5
    D3D11_COMPARISON_GREATER_EQUAL,  // 6 -> 7
    D3D11_COMPARISON_NOT_EQUAL,      // 7 -> 6
};

// T3 @ [RSP+0x58..0x74], written at 0x1800038b8-0x1800038f0. 8 entries.
// D3D11_STENCIL_OP. Note 2/3 are the *wrapping* INCR/DECR and 4/5 the
// saturating ones — easy to get backwards, so: 2->7 (INCR), 3->8 (DECR),
// 4->4 (INCR_SAT), 5->5 (DECR_SAT).
static const D3D11_STENCIL_OP kStencilOp[8] = {
    D3D11_STENCIL_OP_KEEP,      // 0 -> 1
    D3D11_STENCIL_OP_ZERO,      // 1 -> 2
    D3D11_STENCIL_OP_INCR,      // 2 -> 7
    D3D11_STENCIL_OP_DECR,      // 3 -> 8
    D3D11_STENCIL_OP_INCR_SAT,  // 4 -> 4
    D3D11_STENCIL_OP_DECR_SAT,  // 5 -> 5
    D3D11_STENCIL_OP_REPLACE,   // 6 -> 3
    D3D11_STENCIL_OP_INVERT,    // 7 -> 6
};

// T2b @ [RBP-0x58..-0x50], written at 0x1800037f7/0x180003808/0x18000380f.
// 3 entries. D3D11_CULL_MODE. The engine's ordering is Front/Back/None — NOT
// the more usual None-first — but the ctor default of 1 mapping to CULL_BACK
// corroborates it.
static const D3D11_CULL_MODE kCullMode[3] = {
    D3D11_CULL_FRONT,  // 0 -> 2
    D3D11_CULL_BACK,   // 1 -> 3   (RenderState ctor default)
    D3D11_CULL_NONE,   // 2 -> 1
};

// T4 @ [RBP+0x100..0x104] (the caller's shadow space!), written at
// 0x1800037dc/0x1800037e6. 2 entries. D3D11_FILL_MODE.
static const D3D11_FILL_MODE kFillMode[2] = {
    D3D11_FILL_SOLID,      // 0 -> 3   (RenderState ctor default)
    D3D11_FILL_WIREFRAME,  // 1 -> 2   (== Video::RenderState::IsWireframe)
};

// Depth-bias scale factors, read as float constants by the original:
//   0x180018c5c = 0x4B7FFFFF = 16777215.0f  (2^24 - 1: the D24 depth range,
//                                            matching the D24_UNORM_S8_UINT
//                                            depth buffer this backend creates)
//   0x180018c58 = 0x45800000 = 4096.0f
static const float kDepthBiasScale       = 16777215.0f;  // MULSS @ 0x180003843
static const float kSlopeScaledBiasScale = 4096.0f;      // MULSS @ 0x18000387d

// The D3D11 desc structs must be exactly the shapes the original hard-codes.
// The blend-desc size in particular is load-bearing: the original memsets
// 0x108 bytes at 0x180003821 (`mov r8d, 0x108`).
static_assert(sizeof(D3D11_BLEND_DESC)         == 0x108, "D3D11_BLEND_DESC (memset 0x108 @ 0x180003821)");
static_assert(sizeof(D3D11_RASTERIZER_DESC)    == 0x28,  "D3D11_RASTERIZER_DESC");
static_assert(sizeof(D3D11_DEPTH_STENCIL_DESC) == 0x34,  "D3D11_DEPTH_STENCIL_DESC");
static_assert(offsetof(D3D11_BLEND_DESC, RenderTarget)              == 0x08, "BLEND_DESC::RenderTarget");
static_assert(offsetof(D3D11_DEPTH_STENCIL_DESC, StencilReadMask)   == 0x10, "DS_DESC::StencilReadMask");
static_assert(offsetof(D3D11_DEPTH_STENCIL_DESC, FrontFace)         == 0x14, "DS_DESC::FrontFace");
static_assert(offsetof(D3D11_DEPTH_STENCIL_DESC, BackFace)          == 0x24, "DS_DESC::BackFace");
static_assert(offsetof(D3D11_RASTERIZER_DESC, DepthBias)            == 0x0c, "RS_DESC::DepthBias");
static_assert(offsetof(D3D11_RASTERIZER_DESC, SlopeScaledDepthBias) == 0x14, "RS_DESC::SlopeScaledDepthBias");
static_assert(offsetof(D3D11_RASTERIZER_DESC, MultisampleEnable)    == 0x20, "RS_DESC::MultisampleEnable");

// ============================================================================
// Runtime-side dependencies.
//
// Resolved here, in this lane's own translation unit, rather than in the shared
// engine/funcs/engine_funcs.h — six lanes are editing that file concurrently.
//
// The remaining one is a *by-name* import of the shipped DLL, read straight out
// of its import name table, so the mangled string is exact rather than
// reconstructed:
//   [0x1800134d0] -> ?MemFree@MemoryAllocator@System@@SAXPEAX@Z   (hint 0xc9b)
//
// ENDGAME B.3.5 block C, RULE B5-3: the second import,
// `[0x180013348] -> ??1RenderStateData@Video@@UEAA@XZ` (hint 0x295), is GONE. It
// was spelling 9 of the nine caller bindings (RULE B5-14) — a mangled name
// resolved into this struct and called indirectly, one of dx11's 76 literals. The
// Runtime base is now a real class, so the call is a qualified non-virtual member
// call; see the destructor below.
// ============================================================================
typedef void (*fn_MemFree_t)(void* p);

struct RuntimeDeps {
    void**                       deviceSingleton = nullptr;  // Video::Device**
    fn_MemFree_t                 memFree         = nullptr;
    bool                         tried           = false;
    bool                         ok              = false;
};

RuntimeDeps& Deps() {
    static RuntimeDeps d;
    if (!d.tried) {
        d.tried = true;

        // Runtime 0x1801fe7a8 == ?_singleton@Device@Video@@0PEAV12@EA, the
        // target of VideoDX11's import slot 0x1800134b8.
        d.deviceSingleton =
            reinterpret_cast<void**>(ResolveAddr(Module::Runtime, 0x1801fe7a8));

        d.memFree = reinterpret_cast<fn_MemFree_t>(
            ResolveExport("?MemFree@MemoryAllocator@System@@SAXPEAX@Z"));

        d.ok = (d.deviceSingleton != nullptr) && (d.memFree != nullptr);
    }
    return d;
}

// The chain every slot walks. Returns null if the device has not been created
// yet — the original would fault here, we forward to the original slot instead
// (which faults identically, but keeps the failure attributable to it).
DeviceData* ActiveDeviceData() {
    RuntimeDeps& d = Deps();
    if (!d.deviceSingleton) return nullptr;
    void* device = *d.deviceSingleton;               // Video::Device*
    if (!device) return nullptr;
    return *reinterpret_cast<DeviceData**>(
        reinterpret_cast<uint8_t*>(device) + 0x100); // Video::Device+0x100
}

inline VideoRenderState* OwnerState(RenderStateData* self) {
    return reinterpret_cast<VideoRenderState*>(self->renderState);
}

// ============================================================================
// slot 0 — 0x1800036e0
// `RenderStateData* ~RenderStateData(RenderStateData* this, unsigned flags)`
//
// The standard MSVC scalar deleting destructor. Exact original sequence:
//   1. store our vptr        (lea rax,[0x180018818]; mov [rcx],rax)
//   2. Release +0x10 if non-null   (blend)
//   3. Release +0x18 if non-null   (depth-stencil)
//   4. Release +0x20 if non-null   (rasterizer)
//   5. Video::RenderStateData::~RenderStateData(this)   — the Runtime base
//   6. if (flags & 1) MemFree(this)
//   7. return this
//
// Note the release order is blend, depth-stencil, rasterizer — i.e. field
// order, NOT the order the three were created in (blend, rasterizer,
// depth-stencil). Reproduced as-is. The fields are *not* nulled afterwards;
// Runtime always follows this with the object going away.
//
// Step 1 writes the *original* DLL's vtable address, not ours. That is
// deliberate and correct: the seam patches slots inside the shipped vtable, so
// 0x180018818 is still the live vtable for this class. Writing it is also
// almost dead code here (nothing dispatches through the object afterwards), but
// the base dtor at step 5 does overwrite it with the Runtime base vptr, so the
// store has to happen before the base dtor runs, exactly as it does.
// ============================================================================
RenderStateData* RenderStateData_ScalarDeletingDtor(RenderStateData* self, unsigned int flags) {
    RuntimeDeps& d = Deps();
    if (!d.ok) {
        // Cannot safely half-destroy. Hand the whole thing back to the
        // original before touching anything.
        typedef RenderStateData* (*orig_t)(RenderStateData*, unsigned int);
        orig_t orig = reinterpret_cast<orig_t>(
            Dx11OriginalSlot(Vt::RenderStateData, 0));
        return orig ? orig(self, flags) : self;
    }

    self->vptr = Dx11LiveVtable(Vt::RenderStateData);

    if (self->blendState) {
        static_cast<ID3D11BlendState*>(self->blendState)->Release();
    }
    if (self->depthStencilState) {
        static_cast<ID3D11DepthStencilState*>(self->depthStencilState)->Release();
    }
    if (self->rasterizerState) {
        static_cast<ID3D11RasterizerState*>(self->rasterizerState)->Release();
    }

    // ENDGAME B.3.5 block C, RULE B5-3. Was `d.baseDtor(self)`, an indirect call
    // through a resolved `??1RenderStateData@Video@@UEAA@XZ`.
    // `Video::RenderStateData` is now a real class, so this is the MSVC
    // derived-destructor idiom written as C++: a QUALIFIED non-virtual call on the
    // base subobject, reached through the bridge cast because
    // `VideoDX11::RenderStateData` is still the declared leave of RULE B5-8 and
    // does not inherit in C++.
    //
    // It must NOT be `DeletingDtor(0)`: that re-enters slot 0, i.e. this very
    // function. The base destructor is in no slot, which is exactly why the
    // qualification is safe and the virtual call is not.
    reinterpret_cast< ::Video::RenderStateData*>(self)
        -> ::Video::RenderStateData::~RenderStateData();   // B.3.5 bridge

    if (flags & 1u) {
        d.memFree(self);
    }
    return self;
}

// ============================================================================
// slot 1 — 0x180003760  "Create"  (Video::RenderState::PlatformCreate)
//
// Builds all three D3D11 state objects from the owner Video::RenderState.
// Returns true unconditionally (`mov al,1` @ 0x180003a76) — the three HRESULTs
// are discarded.
//
// Call order is fixed and reproduced: Blend, Rasterizer, DepthStencil.
// ============================================================================
bool RenderStateData_Create(RenderStateData* self) {
    DeviceData* dd = ActiveDeviceData();
    if (!dd || !dd->d3dDevice) {
        typedef bool (*orig_t)(RenderStateData*);
        orig_t orig = reinterpret_cast<orig_t>(
            Dx11OriginalSlot(Vt::RenderStateData, 1));
        return orig ? orig(self) : true;
    }
    ID3D11Device* dev = static_cast<ID3D11Device*>(dd->d3dDevice);
    const VideoRenderState* rs = OwnerState(self);

    // ---- D3D11_BLEND_DESC ------------------------------------------------
    // memset 0x108 (@0x180003821), then nine explicit fields. Everything the
    // original leaves zero stays zero here: AlphaToCoverageEnable,
    // IndependentBlendEnable, and RenderTarget[1..7] (which are unused because
    // IndependentBlendEnable is FALSE).
    D3D11_BLEND_DESC blendDesc;
    std::memset(&blendDesc, 0, sizeof(blendDesc));
    blendDesc.AlphaToCoverageEnable  = FALSE;                      // [RBP-0x20].lo, 0x180003905
    blendDesc.IndependentBlendEnable = FALSE;                      // [RBP-0x20].hi, 0x180003905
    blendDesc.RenderTarget[0].BlendEnable    = (rs->blendEnabled != 0);   // RS+0x20, 0x18000390b
    blendDesc.RenderTarget[0].SrcBlend       = kBlendFactor[rs->srcBlend];  // RS+0x24, 0x180003914
    blendDesc.RenderTarget[0].DestBlend      = kBlendFactor[rs->destBlend]; // RS+0x28, 0x18000391f
    blendDesc.RenderTarget[0].BlendOp        = kBlendOp[rs->blendOp];       // RS+0x2c, 0x18000392a
    // Alpha blending is NOT separately configurable: the alpha triplet reads
    // the very same three RenderState fields (0x180003935 / 0x180003940 /
    // 0x18000394b re-load RS+0x24, +0x28, +0x2c).
    blendDesc.RenderTarget[0].SrcBlendAlpha  = kBlendFactor[rs->srcBlend];
    blendDesc.RenderTarget[0].DestBlendAlpha = kBlendFactor[rs->destBlend];
    blendDesc.RenderTarget[0].BlendOpAlpha   = kBlendOp[rs->blendOp];
    // movzx eax,byte [rdi+0x30]; and al,0xf   @ 0x180003956 — low nibble only,
    // i.e. the four D3D11_COLOR_WRITE_ENABLE_{RED,GREEN,BLUE,ALPHA} bits.
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        static_cast<UINT8>(static_cast<uint8_t>(rs->colorWriteMask) & 0x0Fu);

    // ---- D3D11_RASTERIZER_DESC -------------------------------------------
    // Fully written, no memset in the original (all ten fields are assigned).
    D3D11_RASTERIZER_DESC rasterDesc;
    std::memset(&rasterDesc, 0, sizeof(rasterDesc));
    rasterDesc.FillMode              = kFillMode[rs->fillMode];   // RS+0x6c, 0x1800037d3
    rasterDesc.CullMode              = kCullMode[rs->cullMode];   // RS+0x68, 0x180003801
    rasterDesc.FrontCounterClockwise = FALSE;                     // hard 0,   0x180003805
    // cvttss2si of (RS+0x3c * 16777215.0f) — truncation toward zero, matching
    // the C float->int conversion the original compiled to.
    rasterDesc.DepthBias             =
        static_cast<INT>(rs->depthBias * kDepthBiasScale);        // 0x180003827-0x18000384b
    rasterDesc.DepthBiasClamp        = 0.0f;                      // xorps,    0x18000385e
    rasterDesc.SlopeScaledDepthBias  =
        rs->slopeScaledDepthBias * kSlopeScaledBiasScale;         // 0x180003863-0x180003885
    rasterDesc.DepthClipEnable       = TRUE;                      // hard 1,   0x180003868
    rasterDesc.ScissorEnable         = (rs->scissorEnabled != 0); // RS+0x70,  0x18000388a
    // `mov qword [RBP-0x28], 1` @ 0x1800038f8 covers both trailing BOOLs.
    rasterDesc.MultisampleEnable     = TRUE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    // ---- D3D11_DEPTH_STENCIL_DESC ----------------------------------------
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    std::memset(&dsDesc, 0, sizeof(dsDesc));   // see header note re: padding
    dsDesc.DepthEnable    = (rs->depthEnabled != 0);              // RS+0x34, 0x180003961
    // D3D11_DEPTH_WRITE_MASK_ZERO == 0, _ALL == 1, so the raw bool is the enum.
    dsDesc.DepthWriteMask = rs->depthWriteEnabled
                                ? D3D11_DEPTH_WRITE_MASK_ALL
                                : D3D11_DEPTH_WRITE_MASK_ZERO;    // RS+0x35, 0x18000396d
    dsDesc.DepthFunc      = kCompareFunc[rs->depthFunc];          // RS+0x38, 0x180003977
    dsDesc.StencilEnable  = (rs->stencilEnabled != 0);            // RS+0x44, 0x180003983
    // `mov word [RSP+0x30], 0xffff` @ 0x180003986 sets both masks at once.
    dsDesc.StencilReadMask  = 0xFF;
    dsDesc.StencilWriteMask = 0xFF;

    dsDesc.FrontFace.StencilFailOp      = kStencilOp[rs->frontFailOp];      // RS+0x50
    dsDesc.FrontFace.StencilDepthFailOp = kStencilOp[rs->frontDepthFailOp]; // RS+0x54
    dsDesc.FrontFace.StencilPassOp      = kStencilOp[rs->frontPassOp];      // RS+0x58
    dsDesc.FrontFace.StencilFunc        = kCompareFunc[rs->stencilFunc];    // RS+0x48

    if (!rs->stencilTwoSided) {
        // RS+0x45 == 0 -> the fixed back face at 0x1800039ca-0x1800039e2.
        dsDesc.BackFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;   // 1
        dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;   // 1
        dsDesc.BackFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;   // 1
        dsDesc.BackFace.StencilFunc        = D3D11_COMPARISON_ALWAYS; // 8
    } else {
        dsDesc.BackFace.StencilFailOp      = kStencilOp[rs->backFailOp];      // RS+0x5c
        dsDesc.BackFace.StencilDepthFailOp = kStencilOp[rs->backDepthFailOp]; // RS+0x60
        dsDesc.BackFace.StencilPassOp      = kStencilOp[rs->backPassOp];      // RS+0x64
        // NOT a typo and NOT a bug: the back face re-reads RS+0x48
        // (0x180003a10, the same field the front face used). Video::RenderState
        // simply has one stencil compare function — RenderState::Serialize
        // (0x1800f4c90 -> Stencil::Serialize 0x1800f48e0) shows a single int at
        // +0x48 for the whole Stencil sub-struct.
        dsDesc.BackFace.StencilFunc        = kCompareFunc[rs->stencilFunc];
    }

    // ---- create, in the original's order ---------------------------------
    dev->CreateBlendState(&blendDesc,
                          reinterpret_cast<ID3D11BlendState**>(&self->blendState));
    dev->CreateRasterizerState(&rasterDesc,
                          reinterpret_cast<ID3D11RasterizerState**>(&self->rasterizerState));
    dev->CreateDepthStencilState(&dsDesc,
                          reinterpret_cast<ID3D11DepthStencilState**>(&self->depthStencilState));
    return true;
}

// ============================================================================
// slot 2 — 0x180003a90  "Bind"  (Video::RenderState::Bind)
//
//   OMSetBlendState(blend, {0,0,0,0}, 0xFFFFFFFF)
//   OMSetDepthStencilState(depthStencil, (UINT)(BYTE)renderState[0x4c])
//   RSSetState(rasterizer)
//
// The blend factor is a zeroed float[4] built on the stack at [RSP+0x20]
// (movss of xorps'd XMM0 to +0x20, then a zero qword to +0x24 and a zero dword
// to +0x2c — sixteen zero bytes). The sample mask is `or r9d,-1`.
//
// The stencil reference is a *byte* load (`movzx r8d, byte [rax+0x4c]`), which
// agrees with Runtime serializing that field through the byte virtual (+0xb8 in
// Stencil::Serialize) rather than the int one.
//
// The Video::Context& Runtime passes in RDX is never read.
// ============================================================================
void RenderStateData_Bind(RenderStateData* self, void* /*videoContext — unused, as in the original*/) {
    DeviceData* dd = ActiveDeviceData();
    if (!dd || !dd->d3dContext) {
        typedef void (*orig_t)(RenderStateData*, void*);
        orig_t orig = reinterpret_cast<orig_t>(
            Dx11OriginalSlot(Vt::RenderStateData, 2));
        if (orig) orig(self, nullptr);
        return;
    }
    ID3D11DeviceContext* ctx = static_cast<ID3D11DeviceContext*>(dd->d3dContext);
    const VideoRenderState* rs = OwnerState(self);

    const FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx->OMSetBlendState(static_cast<ID3D11BlendState*>(self->blendState),
                         blendFactor, 0xFFFFFFFFu);
    ctx->OMSetDepthStencilState(
        static_cast<ID3D11DepthStencilState*>(self->depthStencilState),
        static_cast<UINT>(rs->stencilRef));
    ctx->RSSetState(static_cast<ID3D11RasterizerState*>(self->rasterizerState));
}

}  // namespace
}  // namespace VideoDX11

// All three live. Verified in-game and RenderDoc-diffed against the shipped
// DLL — see dx11/README.md §"Per-draw pipeline-state equality".

DX11_REGISTER_SLOT(RenderStateData, 0, &::VideoDX11::RenderStateData_ScalarDeletingDtor,
                   "RenderStateData::~RenderStateData");
DX11_REGISTER_SLOT(RenderStateData, 1, &::VideoDX11::RenderStateData_Create,
                   "RenderStateData::Create");
DX11_REGISTER_SLOT(RenderStateData, 2, &::VideoDX11::RenderStateData_Bind,
                   "RenderStateData::Bind");
