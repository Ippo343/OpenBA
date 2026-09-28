// VideoDX11 VertexProgramData (vtable 0x180018890) and PixelProgramData
// (vtable 0x180018800) — all six slots.
//
// The two classes are the same class twice. Byte for byte, the only differences
// between the vertex and pixel versions of each of the three functions are the
// vtable address stored by the destructor, the Runtime base destructor it
// chains to, and which of ID3D11Device::CreateVertexShader /
// ID3D11DeviceContext::VSSetShader it calls versus their pixel counterparts.
// They are ported together here so the shared reasoning is written once.
//
//   slot  vertex       pixel        role
//   ----  -----------  -----------  ------------------------------------------
//     0   0x180004110  0x180003580  scalar deleting destructor
//     1   0x1800041a0  0x180003610  PlatformCreate — compile the permutations
//     2   0x180004230  0x1800036a0  Bind(Context&, int permutation)
//
// Slot names come from the Runtime export table, which names the facade methods
// these virtuals implement:
//     ?PlatformCreate@VertexProgram@Video@@AEAA_NXZ        -> bool, no args
//     ?PlatformCreate@PixelProgram@Video@@AEAA_NXZ         -> bool, no args
//     ?Bind@VertexProgram@Video@@QEBAXAEAVContext@2@H@Z    -> (Context&, int)
//     ?Bind@PixelProgram@Video@@QEBAXAEAVContext@2@H@Z     -> (Context&, int)
//
// The shape of Bind is worth stating explicitly because it decides the ABI: the
// Context& lands in RDX and the permutation index in R8D. Both 0x180004230 and
// 0x1800036a0 clobber RDX with the Video::Device singleton before ever reading
// it, so the DX11 backend genuinely ignores the context it is handed and uses
// the global instead. Our signatures keep the parameter (it must be consumed
// off the register) and mark it unused.
//
// ---------------------------------------------------------------------------
// The permutation array
// ---------------------------------------------------------------------------
// Both classes are 0x30 bytes: the 0x10-byte Runtime base plus a 4-entry array
// of shader objects at +0x10. The owner (Video::VertexProgram /
// Video::PixelProgram, both deriving from Video::ShaderProgram) carries a
// Collection::Array<Collection::Array<unsigned char>> of compiled byte-code
// blobs at +0x40 — one per permutation. PlatformCreate creates a D3D11 shader
// object for each blob the program actually has, up to a hard limit of 4;
// entries past the program's own count stay null.
//
// Bind then does the fallback that makes the whole scheme work:
//
//     i = variants[requested] ? requested : 0;
//     VSSetShader(variants[i], nullptr, 0);
//
// i.e. permutation 0 is the "always compiled" one, and asking for a permutation
// that was never built silently falls back to it rather than binding null.
//
// There is NO bounds check on `requested` in the original — the index is
// sign-extended from int32 and used directly. We reproduce that. Clamping it
// would hide a caller bug rather than surface it, and would be a behavioural
// divergence in exactly the case that matters.

#include "dx11_seam.h"
#include "dx11_backend_access.h"

// ENDGAME B.3.5 block B, RULE B5-3 — the real `Video::VertexProgramData` /
// `PixelProgramData` abstract bases (block A), so the two destructors below can
// call their base destructor as a qualified non-virtual member call instead of
// resolving a mangled name. The twelve CONCRETE `*Data` classes in this
// directory and the 84-slot seam remain the declared leave of RULE B5-8.
#include "video/CompanionData.h"

namespace VideoDX11 {

// ============================================================
// VertexProgramData
// ============================================================

// 0x180004110 — scalar deleting destructor.
//
//     180004137  LEA  RAX,[0x180018890]      ; DX11 vtable
//     18000413e  MOV  qword ptr [RCX],RAX    ; re-seat vptr FIRST
//     180004150  loop x4: if (variants[i]) variants[i]->Release()
//     18000416a  CALL [0x180013318]          ; Video::VertexProgramData::~VertexProgramData
//     180004179  CALL [0x1800134d0]          ; MemFree, only when (flags & 1)
//     18000417f  MOV  RAX,RSI                ; returns `this`
//
// Order matters and is preserved: vptr store, then the four Releases, then the
// Runtime base destructor, then the free. The original does not null the
// variant pointers after releasing them; neither do we — the storage is about
// to be freed or the object re-created, and writing them would be an extra
// store the original never made.
static void* VertexProgramData_Dtor(VertexProgramData* self, unsigned flags) {
    self->vptr = Dx11VtableAddress(Vt::VertexProgramData);

    for (int i = 0; i < kShaderVariantCount; ++i)
        Dx11Release(self->variants[i]);

    // ENDGAME B.3.5 block B, RULE B5-3. Was a `Dx11ResolveRuntimeImport
    // ("??1VertexProgramData@Video@@UEAA@XZ", 0x180013318)` + indirect call —
    // spelling 9 of the nine caller bindings (RULE B5-14), one of dx11's 76
    // mangled-name literals. `Video::VertexProgramData` is a real class now, so
    // this is the MSVC derived-destructor idiom written as C++: a QUALIFIED
    // non-virtual call on the base subobject, reached through the bridge cast
    // because `VideoDX11::VertexProgramData` is the declared leave of RULE B5-8
    // and does not inherit in C++.
    //
    // It must NOT be `DeletingDtor(0)`: that re-enters slot 0, i.e. this
    // function. The base destructor is in no slot, which is why the
    // qualification is safe and the virtual call is not.
    reinterpret_cast< ::Video::VertexProgramData*>(self)
        -> ::Video::VertexProgramData::~VertexProgramData();   // B.3.5 bridge

    if (flags & 1) System::MemoryAllocator::MemFree(self);
    return self;
}

// 0x1800041a0 — VertexProgram::PlatformCreate. Returns bool.
//
//     1800041cd  MOV R14,[deviceData+0x20]        ; ID3D11Device*, fetched ONCE
//     1800041d5  CMP EBX,[owner+0x48]             ; i < permutation count?
//     1800041d8  JGE  skip                        ;   no -> leave variants[i] null
//     1800041da  MOV RAX,[owner+0x40]             ; blob array base
//     1800041e4  MOVSXD R8,[RAX+RDI+8]            ; blob[i].length (int32, sign-extended)
//     1800041e9  MOV RDX,[RAX+RDI]                ; blob[i].data
//     1800041e1  XOR R9D,R9D                      ; pClassLinkage = nullptr
//     1800041f4  LEA RCX,[RSI+(i+2)*8]            ; &this->variants[i]  (this+0x10+i*8)
//     180004200  CALL [R14vt+0x60]                ; index 12 = CreateVertexShader
//     180004204  TEST EAX,EAX / JNZ fail          ; ANY non-zero HRESULT aborts
//     180004213  MOV AL,1 / RET                   ; all four handled -> true
//
// Three details that are easy to get wrong and are deliberate here:
//
//  * The loop always runs 4 iterations. Permutations the program does not have
//    are skipped, not break-ed out of, so a sparse byte-code array would still
//    fill the later slots. (In practice `length` is the prefix count, but the
//    original's control flow is a skip, and we match it.)
//  * The failure test is `hr != 0`, not FAILED(hr). A positive non-S_OK
//    HRESULT would abort in the original, so it aborts here.
//  * On failure it returns false immediately, leaving the already-created
//    variants in place and un-released. The destructor cleans them up.
//
// ID3D11Device vtable index 12 == byte offset 0x60 == CreateVertexShader:
// ID3D11Device inherits IUnknown (0..2) then declares CreateBuffer(3),
// CreateTexture1D/2D/3D(4,5,6), CreateShaderResourceView(7),
// CreateUnorderedAccessView(8), CreateRenderTargetView(9),
// CreateDepthStencilView(10), CreateInputLayout(11), CreateVertexShader(12).
// Independently corroborated inside this DLL: 0x1800052b0/0x180005350 call
// [device+0x28] == index 5 == CreateTexture2D, and 0x180002870 calls
// [device+0x48] == index 9 == CreateRenderTargetView and [device+0x50] ==
// index 10 == CreateDepthStencilView, both of which match this numbering.
static bool VertexProgramData_PlatformCreate(VertexProgramData* self) {
    ID3D11Device* device = Dx11Device();

    for (int i = 0; i < kShaderVariantCount; ++i) {
        // ENDGAME B.3.5 block B: was `const ShaderProgramOwner*`, a partial
        // view in dx11_backend_access.h. `Video::ShaderProgram` is a real class
        // now and this is the same two offsets. The owner is a
        // `Video::VertexProgram`; the fields read here are its ShaderProgram
        // base's, which is what made the view's offsets base-class offsets.
        const Video::ShaderProgram* owner =
            static_cast<const Video::ShaderProgram*>(self->vertexProgram);
        if (i >= owner->_byteCode.GetSize()) continue;

        // ENDGAME B.3.2 step 2 — `byteCode` is the real
        // `Collection::Array<Collection::Array<uint8_t>>`; the element read that
        // needed a cast off a `void*` is now `operator[]`.
        const RtByteCodeBlob* blobs = owner->_byteCode.GetData();
        HRESULT hr = device->CreateVertexShader(
            blobs[i].GetData(),
            static_cast<SIZE_T>(blobs[i].GetSize()),
            nullptr,
            reinterpret_cast<ID3D11VertexShader**>(&self->variants[i]));
        if (hr != 0) return false;
    }
    return true;
}

// 0x180004230 — VertexProgram::Bind(Context&, int permutation).
//
//     4c8bd1        MOV   R10,RCX                    ; this
//     4963c0        MOVSXD RAX,R8D                   ; permutation
//     33c9          XOR   ECX,ECX
//     49394cc210    CMP   [R10+RAX*8+0x10],RCX       ; variants[permutation] == 0 ?
//     440f44c1      CMOVZ R8D,ECX                    ;   yes -> fall back to 0
//     4533c9        XOR   R9D,R9D                    ; NumClassInstances = 0
//     4533c0        XOR   R8D,R8D                    ; ppClassInstances  = nullptr
//     498b54d210    MOV   RDX,[R10+RDX*8+0x10]       ; variants[i]
//     48ff6058      JMP   [ctxvt+0x58]               ; index 11 = VSSetShader
//
// ID3D11DeviceContext vtable index 11 == byte offset 0x58 == VSSetShader:
// ID3D11DeviceChild contributes 0..6 (IUnknown 0..2, GetDevice 3,
// Get/Set/SetInterface PrivateData 4..6), then VSSetConstantBuffers(7),
// PSSetShaderResources(8), PSSetShader(9), PSSetSamplers(10), VSSetShader(11).
// Corroborated within this DLL: IASetInputLayout is index 17 == 0x88 in
// 0x180003fc0, IASetIndexBuffer index 19 == 0x98 in 0x180003380,
// OMSetRenderTargets index 33 == 0x108 in 0x180001070 — all consistent.
static void VertexProgramData_Bind(VertexProgramData* self,
                                   void* /*Video::Context& — ignored, see header*/,
                                   int permutation) {
    ID3D11DeviceContext* ctx = Dx11Context();
    const int i = self->variants[permutation] ? permutation : 0;
    ctx->VSSetShader(static_cast<ID3D11VertexShader*>(self->variants[i]),
                     nullptr, 0);
}

// ============================================================
// PixelProgramData — identical logic, pixel-stage calls
// ============================================================

// 0x180003580 — scalar deleting destructor.
//   1800035a7 LEA RAX,[0x180018800] / 1800035da CALL [0x180013358]
//   (Video::PixelProgramData::~PixelProgramData) / 1800035e9 CALL [0x1800134d0]
static void* PixelProgramData_Dtor(PixelProgramData* self, unsigned flags) {
    self->vptr = Dx11VtableAddress(Vt::PixelProgramData);

    for (int i = 0; i < kShaderVariantCount; ++i)
        Dx11Release(self->variants[i]);

    // RULE B5-3, as in VertexProgramData_Dtor above: was
    // `Dx11ResolveRuntimeImport("??1PixelProgramData@Video@@UEAA@XZ",
    // 0x180013358)` + an indirect call.
    reinterpret_cast< ::Video::PixelProgramData*>(self)
        -> ::Video::PixelProgramData::~PixelProgramData();      // B.3.5 bridge

    if (flags & 1) System::MemoryAllocator::MemFree(self);
    return self;
}

// 0x180003610 — PixelProgram::PlatformCreate. Byte-identical to 0x1800041a0
// except for the device vtable offset: 0x78 == index 15 == CreatePixelShader
// (CreateVertexShader 12, CreateGeometryShader 13,
// CreateGeometryShaderWithStreamOutput 14, CreatePixelShader 15).
static bool PixelProgramData_PlatformCreate(PixelProgramData* self) {
    ID3D11Device* device = Dx11Device();

    for (int i = 0; i < kShaderVariantCount; ++i) {
        // B.3.5 block B, as in VertexProgramData_PlatformCreate above.
        const Video::ShaderProgram* owner =
            static_cast<const Video::ShaderProgram*>(self->pixelProgram);
        if (i >= owner->_byteCode.GetSize()) continue;

        // ENDGAME B.3.2 step 2 — `byteCode` is the real
        // `Collection::Array<Collection::Array<uint8_t>>`; the element read that
        // needed a cast off a `void*` is now `operator[]`.
        const RtByteCodeBlob* blobs = owner->_byteCode.GetData();
        HRESULT hr = device->CreatePixelShader(
            blobs[i].GetData(),
            static_cast<SIZE_T>(blobs[i].GetSize()),
            nullptr,
            reinterpret_cast<ID3D11PixelShader**>(&self->variants[i]));
        if (hr != 0) return false;
    }
    return true;
}

// 0x1800036a0 — PixelProgram::Bind(Context&, int permutation).
// Same instruction sequence as 0x180004230 with the tail-jmp through
// [ctxvt+0x48] == index 9 == PSSetShader.
static void PixelProgramData_Bind(PixelProgramData* self,
                                  void* /*Video::Context& — ignored*/,
                                  int permutation) {
    ID3D11DeviceContext* ctx = Dx11Context();
    const int i = self->variants[permutation] ? permutation : 0;
    ctx->PSSetShader(static_cast<ID3D11PixelShader*>(self->variants[i]),
                     nullptr, 0);
}

}  // namespace VideoDX11

// All live. Verified in-game and RenderDoc-diffed against the shipped DLL —
// see dx11/README.md §"Per-draw pipeline-state equality".

DX11_REGISTER_SLOT(VertexProgramData, 0, &::VideoDX11::VertexProgramData_Dtor,
                   "VertexProgramData::~ (scalar deleting)");
DX11_REGISTER_SLOT(VertexProgramData, 1, &::VideoDX11::VertexProgramData_PlatformCreate,
                   "VertexProgramData::PlatformCreate");
DX11_REGISTER_SLOT(VertexProgramData, 2, &::VideoDX11::VertexProgramData_Bind,
                   "VertexProgramData::Bind");

DX11_REGISTER_SLOT(PixelProgramData, 0, &::VideoDX11::PixelProgramData_Dtor,
                   "PixelProgramData::~ (scalar deleting)");
DX11_REGISTER_SLOT(PixelProgramData, 1, &::VideoDX11::PixelProgramData_PlatformCreate,
                   "PixelProgramData::PlatformCreate");
DX11_REGISTER_SLOT(PixelProgramData, 2, &::VideoDX11::PixelProgramData_Bind,
                   "PixelProgramData::Bind");
