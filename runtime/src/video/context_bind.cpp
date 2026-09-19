// ============================================================================
// runtime/src/video/context_bind.cpp — Wave 9 lane CTX
//   ?BindResources@Context@Video@@AEAAXXZ   retail 0x1800ef060, ordinal 1664
//   ?BindTextures@Context@Video@@AEAAXXZ    retail 0x1800ef330, ordinal 1667
//
// These are the two `Video::Context` methods the FRAME LOOP actually calls, and
// until this file they were the last unported ones on that path.
// ============================================================================
//
// HOW "THE FRAME LOOP HITS THIS FIRST" WAS ESTABLISHED — from disassembly and
// from our own dx11/, not from a guess
// ---------------------------------------------------------------------------
// Lane VID2 predicted the next wall as "?PlatformStartup@Context@Video@@ or the
// first Context method the frame loop calls". **There is no
// ?PlatformStartup@Context@Video@@** — the shipped export directory has
// ?PlatformConstruct@ (0x1800f21f0) and ?PlatformDestruct@ (0x1800f2240) and
// nothing else with that shape. The second half of the prediction is the live
// one, and the answer is not in Engine::MainLoop::Draw at all:
//
//   ?Draw@MainLoop@Engine@@QEAAXM@Z  (0x1800dcd40) reaches exactly two Context
//   methods — `?Reset@` on the failure path (tail JMP 0x1800dcd82) and
//   `?ClearResources@` at 0x1800dcd9f — and BOTH are already ported (Wave 6
//   lane A and Wave 9 lane VID2 respectively). Everything else it does is a
//   virtual call into `Context+0x2160`, which is OUR dx11/ ContextData.
//
// So the frame's Context traffic comes from the OTHER side: our own DX11
// backend calls back INTO Video::Context, by name, on the hot path.
// dx11/src/contextdata.cpp resolves `?BindResources@` and `?BindTextures@`
// through `Dx11ResolveRuntimeImport` and calls them at four sites:
//
//   contextdata.cpp:776   BindTextures  — slot 5, ContextData_BindRenderBuffer
//   contextdata.cpp:953   BindResources — the indexed/instanced draw
//   contextdata.cpp:1092  BindResources — the dynamic-vertex draw
//   contextdata.cpp:1253  BindResources — the dynamic indexed draw
//
// `BindResources` therefore runs ONCE PER DRAW CALL. It is the most-executed
// `Video::Context` method in the process, and before this file every one of
// those calls left our own renderer and entered the shipped DLL.
//
// It is also, unusually, a method with NO Runtime-internal caller at all.
// get_xrefs_to(0x1800ef060) names four DATA references (the export tables) and
// nothing else; get_xrefs_to(0x1800ef330) adds exactly one CALL, from
// BindResources itself at 0x1800ef093. Everything that calls either of these
// lives in the video plugin — which is us. **There is no split-ownership
// hazard here at all**: after this registration our backend is the only caller
// in the process and it reaches only our body.
//
// TRANSITIVE CLOSURE, COUNTED BEFORE ANYTHING WAS WRITTEN
// -------------------------------------------------------
// BindResources contains exactly one direct CALL (`CALL 0x1800ef330`, which is
// BindTextures); every other call it makes is `CALL qword ptr [reg+imm]`, i.e.
// a virtual dispatch through a pointer the Context already holds. BindTextures
// contains no direct call at all. **Closure = these two functions, and ZERO new
// resolve doors** — the port adds no name to `unsatisfied_export_scan.py`'s
// denominator, which is the property lane PICK's five-rows-cost-fourteen note
// asks to be measured up front rather than discovered afterwards.
//
// THE ORACLE, CHOSEN BEFORE THE FIRST BODY, AND WHAT WAS REJECTED
// ---------------------------------------------------------------
// Neither function can be REPLAYED on a live Context the way Wave 6 lane A
// replayed the setters. A setter is a pure state transformation, so running it
// twice on two copies is free; these two ISSUE COMMANDS — every arm ends in a
// virtual call into the backend, and running them twice would bind every
// resource to D3D twice and (via the dirty-bit clears) make the second pass see
// a different input from the first.
//
// REJECTED — a live `RT_VIDCTXBIND` A/B. It can only report "did it
// boot and did the picture look right", which is exactly the
// executed-but-undiffed state reference_uncalled_port_is_not_verified.md warns
// about one door over; it cannot tell me that arm 0x1000 uploaded 0x20 bytes
// from +0x3f8 rather than 0x40 bytes from +0x208.
//
// REJECTED — comparing only the Context bytes afterwards. Both bodies write
// just three fields (+0x10, +0x18, +0x1c and the qword at +0x20), so a
// byte-only comparison is blind to the entire point of the function: WHICH
// backend entry points were called, in WHICH ORDER, with WHICH ARGUMENTS. That
// is the "lossy transform" trap — the comparison point cannot see the work.
//
// CHOSEN — a synthetic subject plus a CALL LOG. Two `.bss` Contexts, one set of
// fake sub-objects SHARED by both sides (so every recorded pointer argument
// except the Context itself compares directly), and a recorder on every vtable
// slot. Ours runs on one copy, the shipped body on the other, and the probe
// compares the full 0x2168 bytes AND the two call sequences element by element
// — tag, slot index, and all five arguments, with pointer arguments that land
// inside the subject normalised to an OFFSET from that side's own base.
//
// That is lockstep, not end-state-only: a defect that sets a wrong dirty bit
// and then clears it compares equal at the end and differs in the log.
//
// THE FAKE TABLES ARE 64 SLOTS WIDE AND EVERY ENTRY IS IMPLEMENTED, and the
// probe PRINTS THE OBSERVED SET rather than the predicted one. Lane VID2 guessed
// a slot set three times and was wrong three times; the derivation below says
// slots 4,5,6,7,8,9,10,11 and 1 and 2, but the report is what was actually
// called, on both sides, as a bitmask.
//
// NOTHING IS EXCLUDED FROM THE BYTE COMPARISON. Both subjects are memcpy'd from
// one master image before each case, so every byte either side reads was
// written by the probe; there is no 0xCD region for the two to disagree about
// and therefore no tolerance to state.
//
// ---------------------------------------------------------------------------
// THE DIRTY MASK, COMPLETED
// ---------------------------------------------------------------------------
// BindResources is the only function that consumes the whole of Context+0x10,
// so reading it fills four gaps runtime/types/video/Context.h left open, and
// corrects one name. Derived from the disassembly, arm by arm, in RETAIL ORDER
// (which is NOT bit order — note 0x8 before 0x4):
//
//   bit      arm at        does
//   0x00001  0x1800ef079   companion slot 5  (BindRenderBuffer)
//   0x00002  0x1800ef08a   CALL BindTextures (0x1800ef330)
//   0x00040  0x1800ef098   vertexProgram(+0x1f0)->+0x50 ->vt[2](that, ctx, detail)
//   0x00080  0x1800ef0b6   pixelProgram (+0x1f8)->+0x50 ->vt[2](that, ctx, detail)
//   0x00100  0x1800ef0d4   videoProgram (+0x200)->+0x40 ->vt[2](that, ctx, detail)
//   0x04000  0x1800ef0f5   companion slot 8  (BindTransforms)  ** see below **
//   0x00008  0x1800ef119   the sixteen vertex streams
//   0x00004  0x1800ef1b2   vertexLayout (+0xb0)->+0x30 ->vt[1](that, ctx)
//   0x00010  0x1800ef1cc   indexBuffer  (+0x1b8)->+0x08 ->vt[5](that, ctx)
//                          or companion slot 10 (BindIndexNone) when null
//   0x00020  0x1800ef1fa   renderState  (+0x1c8)->+0x78 ->vt[2](that, ctx)
//   0x00200  0x1800ef214   companion slot 6  (BindViewport)
//   0x00400  0x1800ef22a   companion slot 7  (BindScissor)
//   0x00800  0x1800ef240   globalConstants  (+0x1f78) upload +0x328  len 0xd0
//   0x01000  0x1800ef26b   instanceConstants(+0x1fc0) upload +0x3f8  len 0x20
//   0x02000  0x1800ef296   lightConstants   (+0x2098) upload +0x1dd8 len 0x1a0
//   0x08000  0x1800ef2c1   materialConstants(+0x0298) upload +0x208  len 0x40
//   0x10000  0x1800ef2ec   pixelConstants   (+0x02e0) upload +0x248  len 0x50
//
// FOUR CORRECTIONS TO Context.h, each derived rather than assumed:
//
//  * **+0x200 IS A POINTER, NOT STAGING.** Context.h models 0x200..0x297 as one
//    `_pixelConstantStaging[0x98]` blob. Arm 0x100 loads `[RBX+0x200]` and
//    dereferences it twice. `?SetVideoProgram@Context@Video@@` (0x18001a040) is
//    four instructions and settles it: it compares `[RCX+0x200]` against its
//    argument, ORs 0x100 into the dirty mask and stores the pointer. So +0x200
//    is `_videoProgram` and bit 0x100 is `kDirtyVideoProgram`. The staging blob
//    is 0x208..0x297, which is also exactly what `?Reset@` memsets (0x208 len
//    0x40, then 0x248 len 0x50) and exactly what arms 0x8000 and 0x10000 upload.
//  * bit **0x2000** is the LIGHT constants (upload of +0x1dd8, the block
//    `?SetLight@` writes), not "unknown".
//  * bit **0x8000** is the MATERIAL constants (+0x208 -> the buffer at +0x298).
//  * bit **0x1000** uploads +0x3f8 len 0x20 — `_inverseScale` and `_color` —
//    into the buffer at +0x1fc0, which Context.h calls `_instanceConstants`.
//    Context.h's enum calls this bit `kDirtyMaterialConstants`; by the buffer it
//    actually feeds it is the INSTANCE constants. The enum name is left alone
//    (renaming it is a change to a file three other lanes include) but the
//    disagreement is recorded here, and this file does not use that name.
//
// ** The 0x4000 arm is NOT a plain bit test. ** 0x1800ef0f5 reads
//
//     if ((dirty & 0x4000) || transformCount > 0 || _unknown0024 > 0)
//
// — a bit OR two SIGNED comparisons on the qword at +0x20, and the second
// comparison is on a field Context.h calls `_unknown0024`. That is why the
// function's final store is `MOV qword ptr [RBX+0x20],RBP`: a QWORD write
// clearing BOTH +0x20 and +0x24 at once. Clearing only the dword at +0x20
// would leave a live transform binding that never re-binds again, and it is one
// of this file's negative controls.
//
// AND THE WHOLE FUNCTION IS WRAPPED IN `if (dirty != 0) { ... }`. When the mask
// is zero nothing happens — not the transform check, not the two clears. A
// Context with `transformCount > 0` and `dirty == 0` binds NOTHING. Faithful.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Context.h"

// ENDGAME B.3.5 block D1. Both bodies MOVED onto `Video::Context`; the two
// registered doors at the bottom kept their names, their mangled-name rows and
// their address rows and became one-line QUALIFIED non-virtual delegations.
//
// THE RAW-OFFSET READS ARE NOW REAL FIELD ACCESSES, except in the three places
// the disassembly reads bytes the struct cannot name:
//   * +0x200, which is `_videoProgram` living inside `_pixelConstantStaging`
//     (see that member's banner in Context.h — the array is left undivided so no
//     other file's offsets move);
//   * the QWORD clear over +0x20..+0x27, which covers `_transformCount` AND
//     `_unknown0024` in ONE store (0x1800ef317) and is one of this file's
//     negative controls;
//   * the five constant-buffer STAGING sources at +0x208 / +0x248 / +0x328 /
//     +0x3f8 / +0x1dd8, two of which start mid-array.
// Everything else — including the five constant buffers' backend companions,
// which used to be `Field<void*>(self, bufOffset + 0x40)` — now goes through the
// real `Video::ConstantBuffer::_platformData` (runtime/types/video/ConstantBuffer.h).
using Video::Context;
using RtVideo::kContextSize;

namespace {

template <typename T> inline T& Field(void* p, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}
inline void* At(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }

// ---- Context offsets this file uses, by number ------------------------------
// Deliberately raw offsets rather than the struct members: two of them
// (+0x200 and +0x24) name fields the struct does not model the way the
// disassembly reads them, and mixing the two spellings in one body is how an
// offset silently drifts.
enum : size_t {
    kDirty            = 0x0010,
    kImageDirty       = 0x0018,
    kVertexDirty      = 0x001c,
    kTransformCount   = 0x0020,   // the QWORD at +0x20 covers +0x24 too
    kImageBuffers     = 0x0030,   // [16]
    kVertexLayout     = 0x00b0,
    kVertexBuffers    = 0x00b8,   // [16]
    kVertexOffsets    = 0x0138,   // int32[16]
    kVertexStrides    = 0x0178,   // int32[16]
    kIndexBuffer      = 0x01b8,
    kRenderState      = 0x01c8,
    kVertexProgram    = 0x01f0,
    kPixelProgram     = 0x01f8,
    kVideoProgram     = 0x0200,
    kShaderDetail     = 0x000c,
    kContextData      = 0x2160,

    kMaterialBuffer   = 0x0298,
    kPixelBuffer      = 0x02e0,
    kGlobalBuffer     = 0x1f78,
    kInstanceBuffer   = 0x1fc0,
    kLightBuffer      = 0x2098,

    kMaterialStaging  = 0x0208,
    kPixelStaging     = 0x0248,
    kGlobalStaging    = 0x0328,
    kInstanceStaging  = 0x03f8,
    kLightStaging     = 0x1dd8,

    // A Video::ConstantBuffer's backend companion. Same +0x40 lane VID2's ctor
    // mints through `factory->vtbl[11]`; retail reaches it as an absolute
    // offset from the Context (`[RBX+0x1fb8]` == 0x1f78 + 0x40).
    kBufCompanion     = 0x40,
};

// ---- Upload sizes, as retail's immediates ----------------------------------
enum : int32_t {
    kGlobalBytes   = 0xd0,
    kInstanceBytes = 0x20,
    kLightBytes    = 0x1a0,
    kMaterialBytes = 0x40,
    kPixelBytes    = 0x50,
};

// ---- Backend companion vtable slots (Context+0x2160) ------------------------
// Every one of these is confirmed twice: once as the offset BindResources
// dispatches through, and once as the offset the corresponding one-line
// exported thunk jumps through. The thunks are three instructions each
// (`MOV RCX,[RCX+0x2160] / MOV RAX,[RCX] / JMP [RAX+off]`), so the pairing is
// not an inference.
enum : size_t {
    kSlotBindRenderBuffer = 0x28,  // ?BindRenderBuffer@ 0x1800f2280
    kSlotBindViewport     = 0x30,  // ?BindViewport@     0x1800f2300
    kSlotBindScissor      = 0x38,  // ?BindScissor@      0x1800f2320
    kSlotBindTransforms   = 0x40,  // ?BindTransforms@   0x1800f22a0
    kSlotBindTextureNone  = 0x48,  // ?BindTextureNone@  0x1800f2260
    kSlotBindIndexNone    = 0x50,  // ?BindIndexNone@    0x1800f22e0
    kSlotBindVertexNone   = 0x58,  // ?BindVertexNone@   0x1800f22c0
};

// ---- Sub-object companion offsets and slots --------------------------------
enum : size_t {
    kImageCompanion   = 0x60, kImageBindSlot   = 0x20,  // (that, ctx, i)
    kProgCompanion50  = 0x50, kProgBindSlot    = 0x10,  // (that, ctx, detail)
    kVideoCompanion40 = 0x40,                            // same slot, other offset
    kVBufCompanion    = 0x08, kVBufBindSlot    = 0x28,  // (that,ctx,i,off,stride)
    kLayoutCompanion  = 0x30, kLayoutBindSlot  = 0x08,  // (that, ctx)
    kIBufCompanion    = 0x08, kIBufBindSlot    = 0x28,  // (that, ctx)
    kStateCompanion   = 0x78, kStateBindSlot   = 0x10,  // (that, ctx)
    kCBufBindSlot     = 0x10,                            // (that,ctx,src,0,len)
};

using VoidVoidFn   = void (*)(void*);
using VoidIntFn    = void (*)(void*, int32_t);
using BindCtxFn    = void (*)(void*, void*);
using BindCtxIntFn = void (*)(void*, void*, int32_t);
using BindVBufFn   = void (*)(void*, void*, int32_t, int32_t, int32_t);
using BindCBufFn   = void (*)(void*, void*, const void*, int32_t, int32_t);

inline void* VSlot(void* obj, size_t byteOff) {
    void* vt = Field<void*>(obj, 0);
    return Field<void*>(vt, byteOff);
}

// ---- Negative-control injection --------------------------------------------
// Set only by the probe, and only for one case at a time. A control that lives
// outside the function it is testing measures the harness rather than the port
// (reference_probe_needs_an_effectiveness_control.md), so these sit on the real
// branches inside the real bodies.
int g_ctxBindDefect = 0;   // 0 = none; see the probe's table

}  // namespace

// ============================================================================
// ?BindTextures@Context@Video@@AEAAXXZ                      retail 0x1800ef330
// ============================================================================
// Sixteen slots. `EBX` is a rotating one-bit mask (`ROL EBX,1` at 0x1800ef381)
// tested against the WORD IN MEMORY at +0x18 on every iteration — re-read, not
// cached, which is why clearing +0x18 before the loop instead of after would
// make the whole loop bind nothing. `EDI` is the index and `R14` walks the
// pointer array. The mask and the index are two separate registers even though
// one could be derived from the other; kept separate here for the same reason
// lane VID2 kept ClearResources' two masks separate.
void Video::Context::BindTextures() {

    uint32_t mask = 1;
    for (int32_t i = 0; i < 16; ++i, mask = (mask << 1) | (mask >> 31)) {
        if ((_imageBufferDirty & mask) == 0) continue;

        void* img = _imageBuffers[i];
        if (img) {
            // 0x1800ef358: RCX = img->+0x60, RDX = ctx, R8D = i, vt[+0x20].
            void* companion = Field<void*>(img, kImageCompanion);
            reinterpret_cast<BindCtxIntFn>(VSlot(companion, kImageBindSlot))(
                companion, this, i);
        } else {
            // 0x1800ef372: companion->BindTextureNone(i).
            void* cd = _contextData;
            reinterpret_cast<VoidIntFn>(VSlot(cd, kSlotBindTextureNone))(cd, i);
        }
    }
    // 0x1800ef398 — AFTER the loop, unconditionally, even when no bit was set.
    _imageBufferDirty = 0;
}

// ============================================================================
// ?BindResources@Context@Video@@AEAAXXZ                     retail 0x1800ef060
// ============================================================================
// The arms are in RETAIL ORDER, which is not bit order: the sixteen vertex
// streams (0x8) are bound BEFORE the vertex layout (0x4), and the transform
// check sits between the shader programs and the streams. Reordering them is
// invisible in the final Context bytes and visible to any backend that cares
// what was set when — which is why the probe compares the call SEQUENCE.
void Video::Context::BindResources() {

    // 0x1800ef066: the whole body is inside `if (dirty != 0)`.
    if (_dirty == 0) return;

    void* cd = _contextData;

    // --- 0x00001 render buffer ---------------------------------------------
    // TWO DELIBERATE DEPARTURES FROM THE LITERAL BYTES, both stated rather than
    // hidden, and both covered by the 33-case oracle:
    //
    //   (a) 0x1800ef079 tests `AL` of the value loaded ONCE at entry
    //       (0x1800ef066); every later arm re-reads `[RBX+0x10]`. This port
    //       re-reads for the 0x1 arm too. The only instruction between the
    //       entry load and that test is a register spill, so nothing can change
    //       +0x10 in between and the two spellings cannot differ.
    //   (b) retail reloads `[RBX+0x2160]` at each companion arm; `cd` is loaded
    //       once here. No arm writes +0x2160 — the field is set by the ctor and
    //       by ?PlatformConstruct@, neither of which this function reaches.
    //
    // Both are equivalences, not normalisations of observable behaviour: if
    // either were wrong the call log would differ, because a stale `cd` would
    // dispatch through the wrong table.
    if (_dirty & 0x1u)
        reinterpret_cast<VoidVoidFn>(VSlot(cd, kSlotBindRenderBuffer))(cd);

    // --- 0x00002 textures ---------------------------------------------------
    // A direct member call now, not the resolved door: retail's `CALL
    // 0x1800ef330` at 0x1800ef08a is a near call to its own sibling body, and
    // this is that same edge (B.3.5, runtime-internal caller inside the block).
    if (_dirty & 0x2u)
        this->Video::Context::BindTextures();

    const int32_t detail = _shaderDetail;

    // --- 0x00040 vertex program --------------------------------------------
    if (_dirty & 0x40u) {
        void* p = _vertexProgram;
        void* c = Field<void*>(p, kProgCompanion50);
        reinterpret_cast<BindCtxIntFn>(VSlot(c, kProgBindSlot))(c, this, detail);
    }

    // --- 0x00080 pixel program ---------------------------------------------
    if (_dirty & 0x80u) {
        void* p = _pixelProgram;
        void* c = Field<void*>(p, kProgCompanion50);
        reinterpret_cast<BindCtxIntFn>(VSlot(c, kProgBindSlot))(c, this, detail);
    }

    // --- 0x00100 video program ---------------------------------------------
    // NOTE THE COMPANION OFFSET: +0x40, not +0x50. A VideoProgram's backend
    // companion sits one qword earlier than a Vertex/PixelProgram's. Copying
    // the line above would be a silent wrong-pointer read.
    if (_dirty & 0x100u) {
        // +0x200 by raw offset: `_videoProgram` is the first qword of
        // `_pixelConstantStaging`, which Context.h keeps undivided on purpose.
        void* p = Field<void*>(this, kVideoProgram);
        void* c = Field<void*>(p, kVideoCompanion40);
        reinterpret_cast<BindCtxIntFn>(VSlot(c, kProgBindSlot))(c, this, detail);
    }

    // --- 0x04000 transforms, OR a live transform count ----------------------
    if ((_dirty & 0x4000u) || _transformCount > 0 || _unknown0024 > 0)
        reinterpret_cast<VoidVoidFn>(VSlot(cd, kSlotBindTransforms))(cd);

    // Negative control 1 hoists the vertex-layout arm to HERE, ahead of the
    // streams. Nothing in the Context bytes can tell the two orders apart, so
    // this control fails unless the oracle really is comparing the call
    // SEQUENCE and not just the end state.
    if (g_ctxBindDefect == 1 && (_dirty & 0x4u)) {
        void* c = Field<void*>(_vertexLayout, kLayoutCompanion);
        reinterpret_cast<BindCtxFn>(VSlot(c, kLayoutBindSlot))(c, this);
    }

    // --- 0x00008 the sixteen vertex streams ---------------------------------
    if (_dirty & 0x8u) {
        uint32_t mask = 1;
        for (int32_t i = 0; i < 16; ++i, mask = (mask << 1) | (mask >> 31)) {
            if ((_vertexBufferDirty & mask) == 0) continue;

            void* vb = _vertexBuffers[i];
            if (vb) {
                // 0x1800ef15d: RCX = vb->+0x8, RDX = ctx, R8D = i,
                // R9D = offsets[i] (+0x138), [RSP+0x20] = strides[i] (+0x178,
                // reached as R15+0x40 off the offsets cursor).
                void* c = Field<void*>(vb, kVBufCompanion);
                reinterpret_cast<BindVBufFn>(VSlot(c, kVBufBindSlot))(
                    c, this, i, _vertexOffsets[i], _vertexStrides[i]);
            } else {
                reinterpret_cast<VoidIntFn>(VSlot(cd, kSlotBindVertexNone))(cd, i);
            }
        }
        // 0x1800ef1af — inside the 0x8 arm only. A stream marked dirty while
        // bit 0x8 is clear stays marked.
        _vertexBufferDirty = 0;
    }

    // --- 0x00004 vertex layout ----------------------------------------------
    // Retail binds the layout AFTER the streams. Negative control 1 swaps
    // these two arms; nothing in the Context bytes can tell the difference.
    if (g_ctxBindDefect != 1) {
        if (_dirty & 0x4u) {
            void* c = Field<void*>(_vertexLayout, kLayoutCompanion);
            reinterpret_cast<BindCtxFn>(VSlot(c, kLayoutBindSlot))(c, this);
        }
    }

    // --- 0x00010 index buffer -----------------------------------------------
    if (_dirty & 0x10u) {
        void* ib = _indexBuffer;
        if (ib) {
            void* c = Field<void*>(ib, kIBufCompanion);
            reinterpret_cast<BindCtxFn>(VSlot(c, kIBufBindSlot))(c, this);
        } else {
            reinterpret_cast<VoidVoidFn>(VSlot(cd, kSlotBindIndexNone))(cd);
        }
    }

    // --- 0x00020 render state -----------------------------------------------
    if (_dirty & 0x20u) {
        void* c = Field<void*>(_renderState, kStateCompanion);
        reinterpret_cast<BindCtxFn>(VSlot(c, kStateBindSlot))(c, this);
    }

    // --- 0x00200 viewport / 0x00400 scissor ---------------------------------
    if (_dirty & 0x200u)
        reinterpret_cast<VoidVoidFn>(VSlot(cd, kSlotBindViewport))(cd);
    if (_dirty & 0x400u)
        reinterpret_cast<VoidVoidFn>(VSlot(cd, kSlotBindScissor))(cd);

    // --- the five constant-buffer uploads -----------------------------------
    // Each is `buf->_platformData->vt[+0x10](platformData, ctx, staging, 0,
    // len)`. The fourth argument is an `XOR R9D,R9D` in all five; the fifth is
    // the size immediate and is what negative control 3 corrupts.
    //
    // B.3.5: the companion used to be `Field<void*>(self, bufOffset + 0x40)`.
    // `Video::ConstantBuffer` is a real class now, so the buffer is named and
    // the +0x40 is `_platformData`. The STAGING source stays a raw offset —
    // +0x208 and +0x248 start inside `_pixelConstantStaging`.
    struct Upload { uint32_t bit; Video::ConstantBuffer* buf; size_t src; int32_t len; };
    const Upload kUploads[5] = {
        { 0x00800u, &_globalConstants,   kGlobalStaging,   kGlobalBytes   },
        { 0x01000u, &_instanceConstants, kInstanceStaging, kInstanceBytes },
        { 0x02000u, &_lightConstants,    kLightStaging,    kLightBytes    },
        { 0x08000u, &_materialConstants, kMaterialStaging, kMaterialBytes },
        { 0x10000u, &_pixelConstants,    kPixelStaging,    kPixelBytes    },
    };
    for (const Upload& u : kUploads) {
        if ((_dirty & u.bit) == 0) continue;
        void* c = u.buf->_platformData;
        int32_t len = u.len;
        if (g_ctxBindDefect == 3 && u.bit == 0x800u) len = kInstanceBytes;
        reinterpret_cast<BindCBufFn>(VSlot(c, kCBufBindSlot))(
            c, this, At(this, u.src), 0, len);
    }

    // 0x1800ef317 / 0x1800ef31b — a QWORD zero over +0x20..+0x27, then the
    // dirty mask. That single store covers `_transformCount` AND `_unknown0024`,
    // which is why it stays a raw write: clearing only the dword would leave a
    // live transform binding that never re-binds again. Negative control 2 does
    // exactly that.
    if (g_ctxBindDefect == 2)
        _transformCount = 0;
    else
        Field<uint64_t>(this, kTransformCount) = 0;
    _dirty = 0;
}

// ============================================================================
// THE TWO DOORS
// ============================================================================
// Unchanged names, unchanged registrations (see the bottom of this file), now
// one-line QUALIFIED non-virtual delegations. The parameter stays `void*`
// because `dx11/` reaches both through `Dx11ResolveRuntimeImport`, which hands
// back a `void(*)(void*)`, and because the probe below drives them through a
// `BindFn` of that shape over a raw `uint8_t[kContextSize]` subject.
extern "C" void RtVideo_Context_BindTextures(void* self) {
    static_cast<Video::Context*>(self)->Video::Context::BindTextures();
}

extern "C" void RtVideo_Context_BindResources(void* self) {
    static_cast<Video::Context*>(self)->Video::Context::BindResources();
}


// ============================================================================
// THE PROBE
// ============================================================================
namespace {

// ---- the recorder ----------------------------------------------------------
// One entry per backend call. `slot` is the vtable BYTE offset, which is what
// the disassembly shows and therefore what a mismatch should name; `tag`
// separates the eight different call shapes, so a slot collision between two
// object kinds cannot read as agreement.
//
// ARITY IS PART OF THE RECORD, AND IT IS AN EXCLUSION DERIVED FROM THE
// DISASSEMBLY, NOT A TOLERANCE. Retail's call sites load only as many argument
// registers as the callee takes: `?BindViewport@`'s dispatch loads RCX alone,
// the image bind loads RCX/RDX/R8, the vertex-stream bind loads RCX/RDX/R8/R9
// and one stack slot. A trampoline with one wide signature therefore READS
// REGISTERS THE CALLER NEVER SET for the narrow shapes, and their contents are
// whatever the two implementations happened to leave there — different on the
// two sides for a reason with nothing to do with the port. Comparing them would
// be comparing bytes neither side wrote
// (reference_oracle_must_not_compare_unwritten_bytes.md). So each shape's arity
// is fixed here from the call site, and arguments past it are zeroed before the
// record is stored. What this cannot see, stated plainly: a port that passed a
// SIXTH argument where retail passes five would go unnoticed. No shape in
// either body has more than five.
struct CallRec {
    int32_t  tag;
    int32_t  slot;      // vtable byte offset
    int32_t  nargs;
    int32_t  pad;
    int64_t  a[5];      // this, then up to four more; normalised
};

enum : int32_t {
    kTagCompanion = 1, kTagImage = 2, kTagProgram = 3,
    kTagVBuf = 4, kTagLayout = 5, kTagIBuf = 6, kTagState = 7, kTagCBuf = 8,
};

// Derived from the call sites listed in this file's banner.
int ArityOf(int32_t tag, size_t slotByte) {
    switch (tag) {
        case kTagCompanion:
            // slots 5,6,7,8,10 take (this); slots 9 and 11 take (this, int).
            return (slotByte == kSlotBindTextureNone ||
                    slotByte == kSlotBindVertexNone) ? 2 : 1;
        case kTagImage:   return 3;   // (this, ctx, i)
        case kTagProgram: return 3;   // (this, ctx, shaderDetail)
        case kTagVBuf:    return 5;   // (this, ctx, i, offset, stride)
        case kTagLayout:  return 2;   // (this, ctx)
        case kTagIBuf:    return 2;
        case kTagState:   return 2;
        case kTagCBuf:    return 5;   // (this, ctx, src, 0, len)
        default:          return 1;
    }
}

constexpr int kMaxRecs = 512;
CallRec  g_rec[kMaxRecs];
int      g_recN       = 0;
void*    g_recSubject = nullptr;    // the Context THIS side is running on
uint64_t g_slotsSeen  = 0;          // bitmask of vtable slot INDEXES observed

// A pointer argument that points inside the subject is recorded as its offset;
// anything else is recorded raw. Without this the five constant uploads would
// differ on 100% of calls for a reason with nothing to do with the port — each
// side passes a pointer into its OWN Context. Every OTHER pointer the two
// bodies pass is one of the SHARED fakes, so it compares directly.
int64_t Norm(const void* p) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    const uint8_t* s = static_cast<const uint8_t*>(g_recSubject);
    if (s && b >= s && b < s + kContextSize)
        return static_cast<int64_t>(b - s) | static_cast<int64_t>(0x1000000000000000ll);
    return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p));
}

void Rec(int32_t tag, size_t slotByte,
         int64_t a0, int64_t a1, int64_t a2, int64_t a3, int64_t a4) {
    g_slotsSeen |= (1ull << (slotByte / 8));
    if (g_recN >= kMaxRecs) return;
    CallRec& r = g_rec[g_recN++];
    std::memset(&r, 0, sizeof(r));
    r.tag = tag;
    r.slot = static_cast<int32_t>(slotByte);
    r.nargs = ArityOf(tag, slotByte);
    const int64_t all[5] = { a0, a1, a2, a3, a4 };
    for (int i = 0; i < r.nargs; ++i) r.a[i] = all[i];
}

// ---- the fake vtables ------------------------------------------------------
// SIXTY-FOUR SLOTS, EVERY ONE IMPLEMENTED, on all eight tables. Lane VID2
// shipped a probe three times that implemented only the slots it predicted; the
// first died EXEC-of-0 and the second read PAST the end of the array. The
// derivation in this file's banner predicts slots 1,2,4,5,6,7,8,9,10,11 — the
// probe reports which were ACTUALLY reached, on both sides, so the prediction
// and the measurement can disagree out loud.
constexpr int kFakeSlots = 64;

// The widest shape any of these is called with. A Microsoft-x64 callee may
// always ignore arguments it was not passed; the arity mask above is what keeps
// the unset ones out of the comparison.
template <int32_t TAG, int N>
void FakeSlot(void* self, void* a1, void* a2, int64_t a3, int64_t a4) {
    Rec(TAG, static_cast<size_t>(N) * 8,
        Norm(self), Norm(a1), Norm(a2),
        static_cast<int64_t>(static_cast<int32_t>(a3)),
        static_cast<int64_t>(static_cast<int32_t>(a4)));
}

template <int32_t TAG> struct Vt { static void* table[kFakeSlots]; };
template <int32_t TAG> void* Vt<TAG>::table[kFakeSlots] = {};

template <int32_t TAG, int N> struct Fill {
    static void Do() {
        Vt<TAG>::table[N] = reinterpret_cast<void*>(&FakeSlot<TAG, N>);
        Fill<TAG, N - 1>::Do();
    }
};
template <int32_t TAG> struct Fill<TAG, -1> { static void Do() {} };

// A fake object needs room for a companion pointer at up to +0x78 (the render
// state's), so it is 0x80 bytes, not two qwords. All of them are SHARED between
// the two sides, which is what lets every recorded pointer argument except the
// subject compare directly rather than merely by presence.
struct FakeObj { uint8_t bytes[0x80]; };

FakeObj g_companion;
FakeObj g_imgCompanion,   g_images[16];
FakeObj g_progCompanion,  g_vProg, g_pProg, g_videoProg;
FakeObj g_vbufCompanion,  g_vbufs[16];
FakeObj g_layoutCompanion, g_layout;
FakeObj g_ibufCompanion,   g_ibuf;
FakeObj g_stateCompanion,  g_state;
FakeObj g_cbufCompanion[5];

void InstallFakes() {
    Fill<kTagCompanion, kFakeSlots - 1>::Do();
    Fill<kTagImage,     kFakeSlots - 1>::Do();
    Fill<kTagProgram,   kFakeSlots - 1>::Do();
    Fill<kTagVBuf,      kFakeSlots - 1>::Do();
    Fill<kTagLayout,    kFakeSlots - 1>::Do();
    Fill<kTagIBuf,      kFakeSlots - 1>::Do();
    Fill<kTagState,     kFakeSlots - 1>::Do();
    Fill<kTagCBuf,      kFakeSlots - 1>::Do();

    std::memset(&g_companion, 0, sizeof(g_companion));
    Field<void*>(&g_companion, 0) = Vt<kTagCompanion>::table;

    // Each companion carries its own table at +0. The OWNING objects carry a
    // pointer to their companion at the offset the retail body loads it from —
    // and those offsets differ per kind, which is the whole point of recording
    // them separately.
    struct Wire { FakeObj* companion; void** table; };
    const Wire kW[] = {
        { &g_imgCompanion,    Vt<kTagImage>::table   },
        { &g_progCompanion,   Vt<kTagProgram>::table },
        { &g_vbufCompanion,   Vt<kTagVBuf>::table    },
        { &g_layoutCompanion, Vt<kTagLayout>::table  },
        { &g_ibufCompanion,   Vt<kTagIBuf>::table    },
        { &g_stateCompanion,  Vt<kTagState>::table   },
    };
    for (const Wire& w : kW) {
        std::memset(w.companion, 0, sizeof(FakeObj));
        Field<void*>(w.companion, 0) = w.table;
    }
    for (int i = 0; i < 5; ++i) {
        std::memset(&g_cbufCompanion[i], 0, sizeof(FakeObj));
        Field<void*>(&g_cbufCompanion[i], 0) = Vt<kTagCBuf>::table;
    }

    for (int i = 0; i < 16; ++i) {
        std::memset(&g_images[i], 0, sizeof(FakeObj));
        std::memset(&g_vbufs[i],  0, sizeof(FakeObj));
        Field<void*>(&g_images[i], kImageCompanion) = &g_imgCompanion;
        Field<void*>(&g_vbufs[i],  kVBufCompanion)  = &g_vbufCompanion;
    }
    std::memset(&g_vProg, 0, sizeof(FakeObj));
    std::memset(&g_pProg, 0, sizeof(FakeObj));
    std::memset(&g_videoProg, 0, sizeof(FakeObj));
    std::memset(&g_layout, 0, sizeof(FakeObj));
    std::memset(&g_ibuf,  0, sizeof(FakeObj));
    std::memset(&g_state, 0, sizeof(FakeObj));
    Field<void*>(&g_vProg,     kProgCompanion50)  = &g_progCompanion;
    Field<void*>(&g_pProg,     kProgCompanion50)  = &g_progCompanion;
    // +0x40, NOT +0x50 — a VideoProgram's companion sits one qword earlier.
    Field<void*>(&g_videoProg, kVideoCompanion40) = &g_progCompanion;
    Field<void*>(&g_layout,    kLayoutCompanion)  = &g_layoutCompanion;
    Field<void*>(&g_ibuf,      kIBufCompanion)    = &g_ibufCompanion;
    Field<void*>(&g_state,     kStateCompanion)   = &g_stateCompanion;
}

// ---- the subject ------------------------------------------------------------
alignas(16) uint8_t g_master[kContextSize];
alignas(16) uint8_t g_ours  [kContextSize];
alignas(16) uint8_t g_theirs[kContextSize];

struct Case {
    const char* name;
    uint32_t dirty;
    uint32_t imageDirty;
    uint32_t vertexDirty;
    int32_t  transformCount;
    int32_t  field24;
    bool     imagesNull;
    bool     vbufsNull;
    bool     ibufNull;
};

// Build the master image. Every byte is written here, so nothing either body
// reads is uninitialised and the full 0x2168-byte comparison needs NO
// exclusions at all.
void BuildMaster(const Case& c) {
    // A deterministic non-zero fill, so a wrong offset lands on a value that
    // differs from its neighbours instead of on a sea of zeroes
    // (reference_uniform_population_is_not_a_verification.md).
    for (size_t i = 0; i < kContextSize; ++i)
        g_master[i] = static_cast<uint8_t>((i * 31u + 7u) & 0xffu);

    void* m = g_master;
    Field<uint32_t>(m, kDirty)              = c.dirty;
    Field<uint32_t>(m, 0x14)                = 0;
    Field<uint32_t>(m, kImageDirty)         = c.imageDirty;
    Field<uint32_t>(m, kVertexDirty)        = c.vertexDirty;
    Field<int32_t> (m, kTransformCount)     = c.transformCount;
    Field<int32_t> (m, kTransformCount + 4) = c.field24;
    Field<int32_t> (m, kShaderDetail)       = 0x5a;

    Field<void*>(m, kContextData) = &g_companion;

    for (int i = 0; i < 16; ++i) {
        Field<void*>(m, kImageBuffers + static_cast<size_t>(i) * 8) =
            c.imagesNull ? nullptr : &g_images[i];
        Field<void*>(m, kVertexBuffers + static_cast<size_t>(i) * 8) =
            c.vbufsNull ? nullptr : &g_vbufs[i];
        Field<int32_t>(m, kVertexOffsets + static_cast<size_t>(i) * 4) = 0x1000 + i;
        Field<int32_t>(m, kVertexStrides + static_cast<size_t>(i) * 4) = 0x20 + i;
    }
    Field<void*>(m, kVertexLayout)  = &g_layout;
    Field<void*>(m, kIndexBuffer)   = c.ibufNull ? nullptr : &g_ibuf;
    Field<void*>(m, kRenderState)   = &g_state;
    Field<void*>(m, kVertexProgram) = &g_vProg;
    Field<void*>(m, kPixelProgram)  = &g_pProg;
    Field<void*>(m, kVideoProgram)  = &g_videoProg;

    static const size_t kBufs[5] = { kGlobalBuffer, kInstanceBuffer, kLightBuffer,
                                     kMaterialBuffer, kPixelBuffer };
    for (int i = 0; i < 5; ++i)
        Field<void*>(m, kBufs[i] + kBufCompanion) = &g_cbufCompanion[i];
}

using BindFn = void (*)(void*);

// Run one case through ours and theirs and compare. `quiet` suppresses the
// per-difference lines for the negative controls, which are SUPPOSED to differ.
int RunCase(const Case& c, BindFn ours, BindFn theirs, int defect, bool quiet,
            int* shippedCalls, uint64_t* slotsOurs, uint64_t* slotsTheirs) {
    BuildMaster(c);
    std::memcpy(g_ours,   g_master, kContextSize);
    std::memcpy(g_theirs, g_master, kContextSize);

    static CallRec ourRec[kMaxRecs];

    g_recN = 0; g_slotsSeen = 0; g_recSubject = g_ours;
    g_ctxBindDefect = defect;
    ours(g_ours);
    g_ctxBindDefect = 0;
    const int      ourN     = g_recN;
    const uint64_t ourSlots = g_slotsSeen;
    std::memcpy(ourRec, g_rec, sizeof(CallRec) * static_cast<size_t>(ourN));

    g_recN = 0; g_slotsSeen = 0; g_recSubject = g_theirs;
    theirs(g_theirs);
    const int      theirN     = g_recN;
    const uint64_t theirSlots = g_slotsSeen;

    if (shippedCalls) *shippedCalls = theirN;
    if (slotsOurs)    *slotsOurs    = ourSlots;
    if (slotsTheirs)  *slotsTheirs  = theirSlots;

    int bad = 0;

    // (a) the whole object, every byte, no exclusions.
    if (std::memcmp(g_ours, g_theirs, kContextSize) != 0) {
        ++bad;
        if (!quiet) {
            size_t off = 0;
            while (off < kContextSize && g_ours[off] == g_theirs[off]) ++off;
            printf("[CtxBind] *** %s: state differs first at +0x%04zx "
                        "ours=0x%02x theirs=0x%02x ***\n",
                        c.name, off, g_ours[off], g_theirs[off]);
        }
    }

    // (b) the call SEQUENCE, element by element.
    if (ourN != theirN) {
        ++bad;
        if (!quiet)
            printf("[CtxBind] *** %s: call COUNT ours=%d theirs=%d ***\n",
                        c.name, ourN, theirN);
    } else {
        for (int i = 0; i < ourN; ++i) {
            if (std::memcmp(&ourRec[i], &g_rec[i], sizeof(CallRec)) == 0) continue;
            ++bad;
            if (!quiet) {
                const CallRec& a = ourRec[i];
                const CallRec& b = g_rec[i];
                printf("[CtxBind] *** %s: call %d differs -- "
                            "ours tag=%d slot=+0x%02x n=%d a=%llx,%llx,%llx,%llx,%llx | "
                            "theirs tag=%d slot=+0x%02x n=%d a=%llx,%llx,%llx,%llx,%llx ***\n",
                            c.name, i,
                            a.tag, a.slot, a.nargs,
                            (unsigned long long)a.a[0], (unsigned long long)a.a[1],
                            (unsigned long long)a.a[2], (unsigned long long)a.a[3],
                            (unsigned long long)a.a[4],
                            b.tag, b.slot, b.nargs,
                            (unsigned long long)b.a[0], (unsigned long long)b.a[1],
                            (unsigned long long)b.a[2], (unsigned long long)b.a[3],
                            (unsigned long long)b.a[4]);
            }
            break;
        }
    }
    return bad;
}

// One row per arm, plus the shapes that make an arm take its OTHER branch.
const Case kCases[] = {
    // name                        dirty     imgD     vtxD     tc  f24  iN     vN     ibN
    { "dirty=0",                   0x00000u, 0xffffu, 0xffffu,  5,  5, false, false, false },
    { "dirty=0 tc>0",              0x00000u, 0u,      0u,       9,  0, false, false, false },
    { "renderBuffer",              0x00001u, 0u,      0u,       0,  0, false, false, false },
    { "textures all",              0x00002u, 0xffffu, 0u,       0,  0, false, false, false },
    { "textures alternating",      0x00002u, 0xa5a5u, 0u,       0,  0, false, false, false },
    { "textures null objects",     0x00002u, 0xffffu, 0u,       0,  0, true,  false, false },
    { "textures mask 0",           0x00002u, 0u,      0u,       0,  0, false, false, false },
    { "vertexLayout",              0x00004u, 0u,      0u,       0,  0, false, false, false },
    { "vertexBuffers all",         0x00008u, 0u,      0xffffu,  0,  0, false, false, false },
    { "vertexBuffers alternating", 0x00008u, 0u,      0x5a5au,  0,  0, false, false, false },
    { "vertexBuffers null",        0x00008u, 0u,      0xffffu,  0,  0, false, true,  false },
    { "vertexBuffers mask 0",      0x00008u, 0u,      0u,       0,  0, false, false, false },
    { "indexBuffer",               0x00010u, 0u,      0u,       0,  0, false, false, false },
    { "indexBuffer null",          0x00010u, 0u,      0u,       0,  0, false, false, true  },
    { "renderState",               0x00020u, 0u,      0u,       0,  0, false, false, false },
    { "vertexProgram",             0x00040u, 0u,      0u,       0,  0, false, false, false },
    { "pixelProgram",              0x00080u, 0u,      0u,       0,  0, false, false, false },
    { "videoProgram",              0x00100u, 0u,      0u,       0,  0, false, false, false },
    { "viewport",                  0x00200u, 0u,      0u,       0,  0, false, false, false },
    { "scissor",                   0x00400u, 0u,      0u,       0,  0, false, false, false },
    { "globalConstants",           0x00800u, 0u,      0u,       0,  0, false, false, false },
    { "instanceConstants",         0x01000u, 0u,      0u,       0,  0, false, false, false },
    { "lightConstants",            0x02000u, 0u,      0u,       0,  0, false, false, false },
    { "transforms bit",            0x04000u, 0u,      0u,       0,  0, false, false, false },
    { "transforms via count",      0x00001u, 0u,      0u,       3,  0, false, false, false },
    { "transforms via +0x24",      0x00001u, 0u,      0u,       0,  7, false, false, false },
    { "transforms negative both",  0x00001u, 0u,      0u,      -4, -4, false, false, false },
    { "materialConstants",         0x08000u, 0u,      0u,       0,  0, false, false, false },
    { "pixelConstants",            0x10000u, 0u,      0u,       0,  0, false, false, false },
    { "vertexDirty w/o bit 0x8",   0x00001u, 0u,      0xffffu,  0,  0, false, false, false },
    { "imageDirty w/o bit 0x2",    0x00001u, 0xffffu, 0u,       0,  0, false, false, false },
    { "ALL bits, objects present", 0x1ffffu, 0xffffu, 0xffffu,  2,  2, false, false, false },
    { "ALL bits, objects null",    0x1ffffu, 0xffffu, 0xffffu,  2,  2, true,  true,  true  },
};
constexpr int kNCases = static_cast<int>(sizeof(kCases) / sizeof(kCases[0]));

}  // namespace


// ============================================================================
// Registration — BOTH DOORS for both symbols.
// ============================================================================
// Lane VID2 §R5: `??3Context@Video@@` sat address-bound-only for three phases
// because nothing had yet asked for it by name. An exported symbol with a port
// gets the NAME door AND the ADDRESS door, not whichever one its first caller
// happened to use. Our dx11/ backend reaches these two through
// `Dx11ResolveRuntimeImport`, which tries `ResolveExport` (the name) first and
// only then falls back to VideoDX11's own IAT slot, so the name door is the one
// that carries today; the address door is registered so a future by-address
// caller cannot walk past the port (reference_bypassed_port_needs_a_source_checker.md).
// Neither address had any other reader in the tree before this file — checked
// by grepping 0x1800ef060 and 0x1800ef330 across code/, engine/, runtime/,
// standalone/ and dx11/, which returned nothing.
//
// Both mangled names were read out of the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE this file was
// written: `?BindResources@Context@Video@@AEAAXXZ` at 0x1800ef060 ordinal 1664,
// `?BindTextures@Context@Video@@AEAAXXZ` at 0x1800ef330 ordinal 1667. Note the
// `AEAA` — both are PRIVATE members, and a `QEAA` guess would resolve NULL in
// silence forever (reference_resolveexport_phantom_names.md). dx11/ already
// spells them `AEAA`, for the same reason and with the same note.
int RtVideoContextBindRegister() {
    int n = 0;
    RegisterRuntimeExport("?BindResources@Context@Video@@AEAAXXZ",
                          (void*)&RtVideo_Context_BindResources); ++n;
    RegisterRuntimeExport("?BindTextures@Context@Video@@AEAAXXZ",
                          (void*)&RtVideo_Context_BindTextures);  ++n;
    RegisterRuntimeAddr(0x1800ef060, (void*)&RtVideo_Context_BindResources);
    RegisterRuntimeAddr(0x1800ef330, (void*)&RtVideo_Context_BindTextures);
    return n;
}
