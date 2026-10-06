// VideoDX11 VideoProgramData (vtable 0x1800185a8, 3 slots) and RenderBufferData
// (vtable 0x180018588, 4 slots) — the two classes with zero DX11-private
// fields. Both are exactly 0x10 bytes: the Runtime base's vptr + owner
// back-pointer and nothing else.
//
//   class             slot  address      role
//   ----------------  ----  -----------  ---------------------------------------
//   VideoProgramData    0   0x180001140  scalar deleting destructor
//   VideoProgramData    1   0x180001060  `mov al,1; ret`  -> true
//   VideoProgramData    2   0x180001190  `ret`            -> void
//   RenderBufferData    0   0x180001010  scalar deleting destructor
//   RenderBufferData    1   0x180001060  `mov al,1; ret`  -> true
//   RenderBufferData    2   0x180001070  Bind(Context&)      -> OMSetRenderTargets
//   RenderBufferData    3   0x1800010e0  Resolve(...)        -> ResolveSubresource
//
// B5 PILOT (the dx11 seam): both are real C++ subclasses of their abstract
// `Video::XData` bases (dx11/types/VideoDX11Data.h); every slot below is a
// member declared `override`, and the compiler builds both vtables (retail slot
// N >= 1 is compiler slot N + 1, which nothing indexes any more).
//
// VideoProgramData really is inert — the DX11 backend implements none of its
// interface beyond construction and destruction. RenderBufferData holds no
// state of its own either, but it is NOT inert: it reaches its two views
// through the Video::RenderBuffer it was built for, so slots 2 and 3 are real
// D3D11 work. The "0 private bytes -> cheapest to port" note in dx11/README.md
// is true of the class, not of the code.
//
// ---------------------------------------------------------------------------
// The shared one-liner stubs
// ---------------------------------------------------------------------------
// 0x180001060 (`b0 01 c3`) and 0x180001190 (`c2 00 00`) are not Ghidra
// functions — they are three-byte tails inside a neighbour's body, and each is
// a live slot in more than one vtable:
//
//     0x180001060  RenderBufferData[1], VideoProgramData[1], DeviceData[7]
//     0x180001190  VideoProgramData[2], DeviceData[4]
//
// The DeviceData copies are already ported in dx11/src/devicedata_stubs.cpp.
// That is not a conflict and not a dependency: a vtable holds a *slot*, not a
// *function*, so DeviceData's slots 4 and 7 and the slots below are
// independent entries that happen to have pointed at the same two addresses.
// Each is now its own tiny member of its own class.
//
// Each member carries its base slot's signature (the CALLER's dispatch shape)
// and ignores the arguments, as the three-byte originals do. Note also that
// 0x180001190 is `ret 0`, not `ret N` — it pops nothing, so there is no stack
// adjustment to reproduce either.
//
// Return registers match the originals exactly: `bool` returns in AL, like the
// `mov al,1`.

#include "dx11_backend_access.h"
#include "VideoDX11Data.h"

namespace VideoDX11 {

// ============================================================
// VideoProgramData
// ============================================================

// 0x180001140 — scalar deleting destructor. No owned COM objects, so it is just
// the vptr re-seat, the Runtime base destructor, and the conditional free.
//
//     18000115d  LEA  RAX,[0x1800185a8]
//                MOV  qword ptr [RCX],RAX
//     180001167  CALL [0x180013498]     ; Video::VideoProgramData::~VideoProgramData
//     180001175  CALL [0x1800134d0]     ; MemFree, only when (flags & 1)
//                returns `this`
//
// B5 PILOT: those three instructions are the compiler's now — the vptr re-seat
// and the base-destructor chain are what `~VideoProgramData()` does by itself,
// and the conditional MemFree is the compiler's D0 calling the base's
// class-scope `operator delete` (B5 W0, runtime/src/video/companion_operators.cpp).
// Nothing is left for the body.
VideoProgramData::~VideoProgramData() {
}

// 0x180001060 — `mov al,1; ret`
bool VideoProgramData::Slot1() {
    return true;
}

// 0x180001190 — `ret`
// ENDGAME B.3.5 block J: slot 2 is `Bind(Context*, int detail)` and takes two
// arguments this stub never reads. `?BindResources@Context@Video@@`'s
// video-program arm loads RDX with the Context and R8D with the shader-detail
// level (0x1800ef0e4 / 0x1800ef0e8) before dispatching here, which is the same
// three-argument shape Vertex/PixelProgramData::Bind has at the same index. The
// slot is NAMED in runtime/types/video/CompanionData.h now, corrected this pass
// from `Slot2()`; this backend really does implement no video-program binding.
void VideoProgramData::Bind(void* /*context*/, int /*detail*/) {
}

// ============================================================
// RenderBufferData
// ============================================================

// 0x180001010 — scalar deleting destructor. Same shape as VideoProgramData's.
//
//     18000102d  LEA  RAX,[0x180018588]
//                MOV  qword ptr [RCX],RAX
//     180001037  CALL [0x1800134a8]     ; Video::RenderBufferData::~RenderBufferData
//     180001045  CALL [0x1800134d0]     ; MemFree, only when (flags & 1)
//
// B5 PILOT: as above, the vptr re-seat, the base-destructor chain and the
// conditional MemFree are the compiler's (its D0 + the base's class-scope
// `operator delete`, B5 W0, companion_operators.cpp). The body is empty.
//
// Note what is absent: no Release of the render-target or depth-stencil view.
// RenderBufferData does not own them — they belong to the ImageBufferData of
// each Video::ImageBuffer the RenderBuffer points at, and are released by
// ~ImageBufferData (0x180002430). Reproduced by doing nothing here.
RenderBufferData::~RenderBufferData() {
}

// 0x180001060 — `mov al,1; ret` (same address as VideoProgramData slot 1;
// separate slot, separate member — see the header comment).
bool RenderBufferData::Slot1() {
    return true;
}

// 0x180001070 — RenderBuffer::Bind(Context&).
//
// Runtime names it: ?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z.
// The Context& arrives in RDX and is overwritten with the Video::Device
// singleton before it is ever read, so it is ignored — like every other Bind in
// this DLL.
//
//     180001074  MOV RDX,[RCX+0x8]         ; this->_owner (the RenderBuffer)
//     18000107b  MOV RAX,[RDX]             ; +0x00 colour ImageBuffer*
//     180001081  JNZ / else RTV = 0
//     180001088  MOV RAX,[RAX+0x60]        ;   -> ImageBufferData*
//     18000108c  MOV RAX,[RAX+0x20]        ;   -> ID3D11RenderTargetView*
//     180001090  MOV [RSP+0x30],RAX        ; a one-element RTV array on the stack
//     180001095  MOV RAX,[RDX+0x8]         ; +0x08 depth ImageBuffer*
//     18000109c  JZ  / else DSV = 0
//     18000109e  MOV RAX,[RAX+0x60]
//     1800010a2  MOV R9,[RAX+0x28]         ;   -> ID3D11DepthStencilView*
//     1800010b2  MOV EDX,0x1               ; NumViews = 1, ALWAYS
//     1800010c8  CALL [ctxvt+0x108]        ; index 33 = OMSetRenderTargets
//
// Two details preserved deliberately:
//   * NumViews is the literal 1 even when the colour buffer is absent — the
//     call then binds a single NULL render target, which is not the same as
//     binding zero targets. D3D11 treats them differently (zero unbinds
//     nothing beyond slot 0's contents on some drivers' debug output), so this
//     is not a cosmetic difference.
//   * The RTV array is a stack temporary whose address is passed regardless.
//
// ID3D11DeviceContext index 33 == byte offset 0x108 == OMSetRenderTargets; see
// the vtable-numbering note in shaderprograms.cpp.
void RenderBufferData::Bind(void* /*Video::Context& — ignored*/) {
    // `_owner` is the base's, typed `Video::RenderBuffer*` (B5 W0), so these are
    // its own field names.
    const RenderBufferOwner* rb = _owner;

    ID3D11RenderTargetView* rtv = nullptr;
    if (rb->colorBuffer) rtv = Dx11ImageRtv(rb->colorBuffer);

    ID3D11DepthStencilView* dsv = nullptr;
    if (rb->depthBuffer) dsv = Dx11ImageDsv(rb->depthBuffer);

    Dx11Context()->OMSetRenderTargets(1, &rtv, dsv);
}

// 0x1800010e0 — RenderBuffer::Resolve(...).
//
// Runtime names it:
//     ?Resolve@RenderBuffer@Video@@QEBAXAEAVContext@2@PEAV12@_N@Z
//       void RenderBuffer::Resolve(Context&, RenderBuffer*, bool) const
// which matches the registers exactly: RDX = Context& (ignored — clobbered at
// 1800010fb), R8 = the other RenderBuffer*, R9 = the bool (never read).
//
//     1800010eb  MOV dword ptr [RSP+0x28],0x57   ; Format = B8G8R8A8_UNORM
//     1800010f3  MOV dword ptr [RSP+0x20],0x0    ; SrcSubresource = 0
//     180001109  MOV RAX,[R8]                    ; other->colour ImageBuffer*
//     18000110c  XOR R8D,R8D                     ; DstSubresource = 0
//     18000110f  MOV R9,[RAX+0x60] / [R9+0x18]   ; -> pSrcResource (the texture)
//     180001113  MOV RAX,[RCX+0x8]               ; this->_owner (the RenderBuffer)
//     18000111a  MOV RDX,[RAX]                   ; this->colour ImageBuffer*
//     180001124  MOV RDX,[RDX+0x60] / [RDX+0x18] ; -> pDstResource
//     18000112c  CALL [ctxvt+0x1c8]              ; index 57 = ResolveSubresource
//
// So the direction is: resolve FROM the argument's colour texture INTO this
// one's. The depth buffers are not touched and the bool argument is ignored,
// which is presumably where a "resolve depth too" option went unimplemented on
// this backend — that is a gap in the shipped DLL, not in this port.
//
// There are no null checks anywhere on either chain: this->_owner->color,
// its ImageBufferData, and its texture are all dereferenced unconditionally, as
// are the argument's. Preserved.
//
// ID3D11DeviceContext index 57 == byte offset 0x1c8 == ResolveSubresource.
void RenderBufferData::Resolve(void* /*Video::Context& — ignored*/,
                               ::Video::RenderBuffer* other,
                               bool  /*flag — ignored by this backend*/) {
    const RenderBufferOwner* dst = _owner;
    // `other` is typed `Video::RenderBuffer*` from retail's owner-side mangling
    // (CompanionData.h, B5 CLOSE).
    const RenderBufferOwner* src = other;

    Dx11Context()->ResolveSubresource(Dx11ImageTexture(dst->colorBuffer), 0,
                                      Dx11ImageTexture(src->colorBuffer), 0,
                                      DXGI_FORMAT_B8G8R8A8_UNORM);
}
static_assert(static_cast<int>(DXGI_FORMAT_B8G8R8A8_UNORM) == 0x57,
              "the literal 0x57 stored at 1800010eb");

}  // namespace VideoDX11
