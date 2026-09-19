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
// VideoProgramData really is inert — the DX11 backend implements none of its
// interface beyond construction and destruction. RenderBufferData holds no
// state of its own either, but it is NOT inert: it reaches its two views
// through the Video::RenderBuffer it was built for, so slots 2 and 3 are real
// D3D11 work. The "0 private bytes -> cheapest to port" note in dx11/README.md
// is true of the struct, not of the code.
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
// That is not a conflict and not a dependency: the seam patches a *vtable
// slot*, not a *function*, so DeviceData's slots 4 and 7 and the two slots
// below are four independent pointers that happen to have pointed at the same
// two addresses. Each file writes its own implementation into its own slots.
//
// Argument lists are unknown for both and deliberately not guessed. Under the
// Microsoft x64 ABI a callee may ignore whatever it was handed: integer
// arguments arrive in RCX/RDX/R8/R9 or in caller-allocated stack space that the
// *caller* cleans up, so a one-parameter signature is safe no matter what the
// real one is. Note also that 0x180001190 is `ret 0`, not `ret N` — it pops
// nothing, so there is no stack adjustment to reproduce either.
//
// Return registers match the originals exactly: `bool` returns in AL, like the
// `mov al,1`.

#include "dx11_seam.h"
#include "dx11_backend_access.h"
// ENDGAME B.3.5, RULE B5-3: the real `Video::RenderBufferData` abstract base,
// whose destructor the scalar deleting dtor below calls by qualified name.
#include "video/CompanionData.h"

// ENDGAME B.3.5 block B, RULE B5-3 — the real `Video::VideoProgramData` abstract
// base (block A), so VideoProgramData_Dtor below can call its base destructor as a
// qualified non-virtual member call instead of resolving a mangled name. The
// CONCRETE `*Data` classes in this directory and the 84-slot seam remain the
// declared leave of RULE B5-8.
#include "video/CompanionData.h"

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
static void* VideoProgramData_Dtor(VideoProgramData* self, unsigned flags) {
    self->vptr = Dx11VtableAddress(Vt::VideoProgramData);

    // ENDGAME B.3.5 block B, RULE B5-3. Was a `Dx11ResolveRuntimeImport
    // ("??1VideoProgramData@Video@@UEAA@XZ", 0x180013498)` + indirect call —
    // spelling 9 of the nine caller bindings (RULE B5-14), one of dx11's 76
    // mangled-name literals. `Video::VideoProgramData` is a real class (block A),
    // so this is the MSVC derived-destructor idiom written as C++: a QUALIFIED
    // non-virtual call on the base subobject, reached through the bridge cast
    // because `VideoDX11::VideoProgramData` is the declared leave of RULE B5-8 and
    // does not inherit in C++.
    //
    // It must NOT be `DeletingDtor(0)`: that re-enters slot 0, i.e. this function.
    //
    // RenderBufferData_Dtor below is UNCHANGED and is block D2's — this commit
    // touches only the VideoProgramData half of this file.
    reinterpret_cast< ::Video::VideoProgramData*>(self)
        -> ::Video::VideoProgramData::~VideoProgramData();   // B.3.5 bridge

    if (flags & 1) Dx11MemFree(self);
    return self;
}

// 0x180001060 — `mov al,1; ret`
static bool VideoProgramData_StubTrue(void* /*self*/) {
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
static void VideoProgramData_StubVoid(void* /*self*/, void* /*context*/,
                                      int /*detail*/) {
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
// Note what is absent: no Release of the render-target or depth-stencil view.
// RenderBufferData does not own them — they belong to the ImageBufferData of
// each Video::ImageBuffer the RenderBuffer points at, and are released by
// ~ImageBufferData (0x180002430). Reproduced by doing nothing here.
static void* RenderBufferData_Dtor(RenderBufferData* self, unsigned flags) {
    self->vptr = Dx11VtableAddress(Vt::RenderBufferData);

    // ENDGAME B.3.5 block D2, RULE B5-3: `Video::RenderBufferData` is a real
    // abstract class (block A), so the base-dtor call is now a QUALIFIED
    // NON-VIRTUAL member call instead of a resolved import. It must NOT become
    // `DeletingDtor(0)` — that would re-enter this very slot. The
    // `reinterpret_cast` is the bridge the declared leave (RULE B5-8) requires:
    // `VideoDX11::RenderBufferData` does not derive from the Runtime base in C++,
    // it mirrors its first two fields.
    reinterpret_cast<Video::RenderBufferData*>(self)
        ->Video::RenderBufferData::~RenderBufferData();   // B.3.5 bridge

    if (flags & 1) Dx11MemFree(self);
    return self;
}

// 0x180001060 — `mov al,1; ret` (same address as VideoProgramData slot 1;
// separate slot, separate implementation — see the header comment).
static bool RenderBufferData_StubTrue(void* /*self*/) {
    return true;
}

// 0x180001070 — RenderBuffer::Bind(Context&).
//
// Runtime names it: ?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z.
// The Context& arrives in RDX and is overwritten with the Video::Device
// singleton before it is ever read, so it is ignored — like every other Bind in
// this DLL.
//
//     180001074  MOV RDX,[RCX+0x8]         ; this->renderBuffer
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
static void RenderBufferData_Bind(RenderBufferData* self,
                                  void* /*Video::Context& — ignored*/) {
    // ENDGAME B.3.5 block D2, RULE B5-2.3: `RenderBufferOwner` is now the real
    // `Video::RenderBuffer`, so these are its own field names.
    const RenderBufferOwner* rb =
        static_cast<const RenderBufferOwner*>(self->renderBuffer);

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
//     180001113  MOV RAX,[RCX+0x8]               ; this->renderBuffer
//     18000111a  MOV RDX,[RAX]                   ; this->colour ImageBuffer*
//     180001124  MOV RDX,[RDX+0x60] / [RDX+0x18] ; -> pDstResource
//     18000112c  CALL [ctxvt+0x1c8]              ; index 57 = ResolveSubresource
//
// So the direction is: resolve FROM the argument's colour texture INTO this
// one's. The depth buffers are not touched and the bool argument is ignored,
// which is presumably where a "resolve depth too" option went unimplemented on
// this backend — that is a gap in the shipped DLL, not in this port.
//
// There are no null checks anywhere on either chain: this->renderBuffer->color,
// its ImageBufferData, and its texture are all dereferenced unconditionally, as
// are the argument's. Preserved.
//
// ID3D11DeviceContext index 57 == byte offset 0x1c8 == ResolveSubresource.
static void RenderBufferData_Resolve(RenderBufferData* self,
                                     void* /*Video::Context& — ignored*/,
                                     void* other,
                                     bool  /*flag — ignored by this backend*/) {
    const RenderBufferOwner* dst =
        static_cast<const RenderBufferOwner*>(self->renderBuffer);
    const RenderBufferOwner* src =
        static_cast<const RenderBufferOwner*>(other);

    Dx11Context()->ResolveSubresource(Dx11ImageTexture(dst->colorBuffer), 0,
                                      Dx11ImageTexture(src->colorBuffer), 0,
                                      DXGI_FORMAT_B8G8R8A8_UNORM);
}
static_assert(static_cast<int>(DXGI_FORMAT_B8G8R8A8_UNORM) == 0x57,
              "the literal 0x57 stored at 1800010eb");

}  // namespace VideoDX11

// Unproven until run in-game — opt-in, shared with the rest of lane L2.

DX11_REGISTER_SLOT(VideoProgramData, 0, &::VideoDX11::VideoProgramData_Dtor,
                   "VideoProgramData::~ (scalar deleting)");
DX11_REGISTER_SLOT(VideoProgramData, 1, &::VideoDX11::VideoProgramData_StubTrue,
                   "VideoProgramData::Stub1 (true)");
DX11_REGISTER_SLOT(VideoProgramData, 2, &::VideoDX11::VideoProgramData_StubVoid,
                   "VideoProgramData::Stub2 (void)");

DX11_REGISTER_SLOT(RenderBufferData, 0, &::VideoDX11::RenderBufferData_Dtor,
                   "RenderBufferData::~ (scalar deleting)");
DX11_REGISTER_SLOT(RenderBufferData, 1, &::VideoDX11::RenderBufferData_StubTrue,
                   "RenderBufferData::Stub1 (true)");
DX11_REGISTER_SLOT(RenderBufferData, 2, &::VideoDX11::RenderBufferData_Bind,
                   "RenderBufferData::Bind");
DX11_REGISTER_SLOT(RenderBufferData, 3, &::VideoDX11::RenderBufferData_Resolve,
                   "RenderBufferData::Resolve");
