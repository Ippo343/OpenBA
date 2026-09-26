// ============================================================================
// runtime/src/graphics/wave23_lane_gfxbody.cpp — Wave 23 lane GFXBODY
// ============================================================================
//
// Five of the ten Graphics function rows this lane was dealt. The triage for
// all ten — including the five NOT ported here and why — is
// docs/runtime_port/WAVE23_LANE_GFXBODY.md. Read §5 before adding a sixth.
//
//   | Export                                        | Retail      | Ord  | Bytes |
//   |-----------------------------------------------|-------------|------|-------|
//   | ??0FrameCopyImageBuffer@Graphics@@QEAA@XZ     | 0x1800a3d10 |  198 |   304 |
//   | ??1FrameCopyImageBuffer@Graphics@@UEAA@XZ     | 0x1800a3c80 |  597 |   138 |
//   | ?Create@FrameCopyImageBuffer@Graphics@@UEAA_N…| 0x1800a3b70 | 1877 |   197 |
//   | ?Generate@FrameCopyImageBuffer@Graphics@@UEBAX…| 0x1800a3b20 | 2395 |   60 |
//   | ?Update@AnimationManager@Graphics@@QEAAX…     | 0x1800fc240 | 4473 |   430 |
//
// Every name and address above is `pefile`'s reading of the export directory of
// the shipped C:\GOG Games\Banished\Runtime-x64.dll, run before a line of this
// file was written (reference_resolveexport_phantom_names.md). That check also
// settled the dtor's spelling, which the lane brief flagged as a trap: the
// `UEAA` form IS in the directory at 0x1800a3c80 and the non-virtual `QEAA`
// form is ***ABSENT*** — a `??1FrameCopyImageBuffer@Graphics@@QEAA@XZ`
// registration would have resolved NULL and registered nothing, silently
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
//
// EVERY BODY HERE WAS TRANSCRIBED FROM THE DISASSEMBLY, with Ghidra's C read
// afterwards as a second, differently-biased opinion. That order mattered twice
// in this file:
//
//   * the ctor's decompile invents `extraout_RAX` for the inlined operator-new
//     result and makes the null-allocation path look like it stores an
//     uninitialised value. The bytes say `MOV RAX,RBP` (RBP == 0) — see §1.
//   * `AnimationManager::Update`'s decompile is not consulted at all for the
//     argument slots; the nine-argument callee is spelled out in full by its
//     own exported mangled name, which is a stronger source than either.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/graphics/AnimationManager.h"   // ENDGAME B.3.5 block GC
#include "../../types/graphics/AnimationState.h"     // ditto
#include "../../types/graphics/FrameCopyImageBuffer.h"
#include "../../types/math/Math.h"                   // Math::Transform / Vector3
#include "../../types/system/Value.h"                // System::Time
#include "../../types/video/Device.h"                // ENDGAME B.3.5: Video::RenderBuffer
#include "../../types/video/ImageBuffer.h"           // ENDGAME B.3.5: Video::ImageBuffer

// ---------------------------------------------------------------------------
// THE NEGATIVE-CONTROL SEAM IS GONE -- ENDGAME B.3.5 block GC.
//
// This file used to carry a `GFXBODY23_PROBE` / `RT_GFX23_CORRUPT(n)` seam that
// wrapped seven sites inside `AnimationManager::Update` so a probe binary could
// corrupt one of them at a time. THE PROBE IT WAS WRITTEN FOR DOES NOT EXIST:
// a repo-wide search for `GFXBODY23_PROBE`, `gfxbody23` and
// `GfxbodyProbeCorrupt` finds nothing outside this file except one stale
// sentence in AnimationManager.h -- no `tools/gfxbody23_probe`, no Makefile
// target. So every guarded site expanded to `(false)` in every build this tree
// can produce, and the seam documented a measurement nobody can re-run.
//
// It is deleted with the body's move onto `Graphics::AnimationManager::Update`,
// which is the block brief's "may be deleted if and only if nothing references
// it" arm; the seven sites are now their live arms, written plainly. THE
// ATTENDANCE COUNTERS ARE A DIFFERENT THING AND THEY STAY: `g_animUpdCalls` and
// `g_animUpdStates` have a live reader in `RtGraphicsWave23GfxbodySummary()`.
// ---------------------------------------------------------------------------

using RtGraphics::FrameCopyImageBuffer;

// ----------------------------------------------------------------------------
// Declared here rather than pulled in from standalone/resolve.h, for the reason
// wave21_lane_gfxbody.cpp:47 gives at length: resolve.h drags in <windows.h>,
// whose <wingdi.h> declares a FUNCTION called `Rectangle`. These must stay
// byte-identical to resolve.h's declarations; the linker checks them.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// ATTENDANCE. Same shape and the same reasoning as wave21_lane_gfxbody.cpp:125:
// a port with no attendance counter cannot be told apart from an absent one,
// and `calls=0` with `worked>0` is a different finding from `never entered`
// (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md). Printed
// at 1, 10, 1000 and then every 100000, so a log tail carries the evidence.
//
// `g_generateCopies` is deliberately NOT an entry counter: Generate's whole
// body is behind one equality test, so an entry count of N says nothing about
// whether the copy ever happened. It counts the times the latch DIFFERED and
// the blit was actually issued — the branch, not the door.
// ---------------------------------------------------------------------------
void Attend(const char* what, unsigned long long& n) {
    ++n;
    if (n == 1 || n == 10 || n == 1000 || (n % 100000) == 0)
        printf("[GFXBODY23] %s calls=%llu\n", what, n);
}

unsigned long long g_ctorCalls      = 0;
unsigned long long g_dtorCalls      = 0;
unsigned long long g_createCalls    = 0;
unsigned long long g_generateCalls  = 0;
unsigned long long g_generateCopies = 0;   // the BRANCH, not the door
unsigned long long g_animUpdCalls   = 0;
unsigned long long g_animUpdStates  = 0;   // inner-loop iterations; 0 == "ran, did nothing"

// ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z, retail 0x1800c7a50.
//
// The ctor and Create do NOT call operator new; they INLINE it — the same five
// instructions (`cs = *0x1801fc718 + 0x640; Enter; Allocate(singleton, n, 8);
// Leave`) that wave21_lane_gfxbody.cpp:184 disassembled and showed to be
// instruction-for-instruction `MemAlloc(n, 8)`, with the alignment arriving in
// EDX instead of an immediate. Routing through MemAlloc is therefore not an
// approximation of the inlined sequence, it is that sequence; and it keeps the
// arena identical whether or not lane ALLOC's port is armed.
//
// Resolved by ADDRESS through ResolveAddr, which consults the port registry
// FIRST (reference_resolveaddr_consults_port_registry_first.md) — the door
// runtime/src/video/context_ctor.cpp:105 and wave21_lane_gfxbody.cpp:143
// already use. Never the raw DLL door: that is GetProcAddress
// (reference_rtoriginal_is_getprocaddress.md).
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

// ---------------------------------------------------------------------------
// The seven named dependencies. ALL SEVEN ARE ALREADY `called` AND `ported=1`
// in today's census, so this file adds NOTHING to the cutover surface on the
// FrameCopyImageBuffer side (reference_porting_can_grow_the_cutover_surface.md
// is the hazard; here it is measured and it is zero). The one place this lane
// DOES grow the surface is AnimationState::Update — see its own note below.
//
// RT_EXPORT_ONCE, not the raw DLL door: these calls are the WORK, so they must
// reach our ports where ports exist.
// ---------------------------------------------------------------------------
using VoidThisFn  = void (*)(void*);

void ImageBufferCtor(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??0ImageBuffer@Video@@QEAA@XZ")(p);
}
void ImageBufferDtor(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??1ImageBuffer@Video@@UEAA@XZ")(p);
}
// ENDGAME B.3.5 caller batch: `PlatformConstruct`, `PlatformDestruct` and
// `Create` are DEFINED members of the real `Video::RenderBuffer` today, so the
// three by-name doors become member calls and `RbCreateFn` goes with them. The
// `??3` operator delete below stays a door (B4b-2 / B35_RULES section 11).
void RenderBufferPlatformConstruct(void* p) {
    reinterpret_cast<Video::RenderBuffer*>(p)->PlatformConstruct();   // B.3.5 bridge
}
void RenderBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::RenderBuffer*>(p)->PlatformDestruct();    // B.3.5 bridge
}
void RenderBufferOperatorDelete(void* p) {
    RT_EXPORT_ONCE(VoidThisFn, "??3RenderBuffer@Video@@SAXPEAX@Z")(p);
}
bool RenderBufferCreate(void* rb, const void* colour, const void* depth) {
    return reinterpret_cast<Video::RenderBuffer*>(rb)->Create(
        reinterpret_cast<const Video::ImageBuffer*>(colour),
        reinterpret_cast<const Video::ImageBuffer*>(depth));          // B.3.5 bridge
}

// The two vtable addresses, rebased. `ResolveAddr(Module::Runtime, …)` on a
// DATA address is just the rebase; it consults the address registry first, and
// NOTHING in code/ engine/ runtime/ standalone/ dx11/ registers either of these
// two addresses today (checked by grep for `180154f00` and `180154f60` — the
// only hits are comments). If a later lane MINTS
// `??_7FrameCopyImageBuffer@Graphics@@6B@`, this call starts stamping the
// minted table instead of the DLL's, which is the right answer for a promoted
// row and the wrong one for an unpromoted mint — ADDRESSEE: Wave 23 lane GFXVT,
// which owns the eight Graphics VTABLE rows; if you mint this table, read this
// comment first.
void* VtFrameCopy() {
    static void* v = nullptr;
    if (!v) v = reinterpret_cast<void*>(
        ResolveAddr(Module::Runtime, RtGraphics::kVtFrameCopyImageBuffer));
    return v;
}
void* VtGenerated() {
    static void* v = nullptr;
    if (!v) v = reinterpret_cast<void*>(
        ResolveAddr(Module::Runtime, RtGraphics::kVtGeneratedImageBuffer));
    return v;
}

}  // namespace

// ============================================================================
// §1.  ??0FrameCopyImageBuffer@Graphics@@QEAA@XZ   retail 0x1800a3d10, ord 198
// ============================================================================
// 304 bytes / 80 instructions. Constructs the ImageBuffer base, then builds the
// two members the class owns unconditionally: a second ImageBuffer aliased onto
// the device back buffer, and a RenderBuffer pointing at it.
//
// *** THE ONE PLACE THE DECOMPILER WOULD HAVE MISLED A REWRITE. ***
// Ghidra renders the inlined operator new as
//
//     pRVar1 = this_01;                       // this_01 == 0
//     if (this_00 != 0) { ImageBuffer::ImageBuffer(this_00); pRVar1 = extraout_RAX; }
//     *(RenderBuffer **)(this + 0x70) = pRVar1;
//
// which reads as "store the constructor's return value". The bytes at
// 0x1800a3d9f-0x1800a3dae are
//
//     JZ  0x1800a3dab                          ; allocation failed
//     MOV RCX,RDI ; CALL 0x1800f2580 ; JMP 0x1800a3dae
//   0x1800a3dab:
//     MOV RAX,RBP                              ; RBP is 0 for the whole body
//   0x1800a3dae:
//     MOV qword ptr [RSI + 0x70],RAX
//
// so RAX is the ALLOCATION on both paths — `this` returned in RAX by MSVC
// convention on one, and the null RBP on the other. We store `image`, never a
// constructor's return, which is the rule
// runtime/src/save/saveload_draw.cpp:427 and
// runtime/src/graphics/sunlight.cpp:230 both paid for: this tree's ported
// constructors are not uniform about returning `this`, and one of them
// (`Video_ImageBuffer_Ctor`, buffers.cpp:387) is declared `void`. Using the
// pointer we already hold is byte-faithful to what retail computes AND
// independent of how any port spells its signature.
//
// THE UNGUARDED DEREF IS DELIBERATE AND FAITHFUL. If MemAlloc returns null,
// retail walks straight into `MOV RCX,[RAX+0x60]` and dies. Per CLAUDE.md
// ("don't fix a faithful crash with a null check") that is reproduced, not
// guarded: the deref is load-bearing — retail relies on the allocation being
// there, and skipping the work would not restore the behaviour.
//
// Return type: retail ends `MOV RAX,RSI; RET`, i.e. it returns `this`. Spelled
// `void*` and returning `self`, so a caller that DOES read RAX (the exe's
// 0x140095da0 does: `CALL <ctor>; MOV R12,RAX`) gets the right value.
// ============================================================================
extern "C" void* RtGraphics_FrameCopyImageBuffer_Ctor(void* self) {
    Attend("FrameCopyImageBuffer::ctor", g_ctorCalls);

    FrameCopyImageBuffer* const p = static_cast<FrameCopyImageBuffer*>(self);

    ImageBufferCtor(p);                              // 0x1800a3d2d
    p->base.vtable   = VtGenerated();                // 0x1800a3d3a — transient, see below
    // ENDGAME B.3.8 block RB (RULE B8-1): `_generation = 0` (0x1800a3d3f) is
    // GeneratedImageBuffer's shared body, the rest after the FrameCopy stamp
    // is FrameCopyImageBuffer's (`InitFields`, below).
    ::Graphics::GeneratedImageBuffer::DoorBody::Init(
        reinterpret_cast<::Graphics::GeneratedImageBuffer*>(self));   // B.3.8 bridge
    p->base.vtable   = VtFrameCopy();                // 0x1800a3d49
    // The first vptr store is overwritten seven instructions later with no
    // virtual dispatch in between, so it is observationally dead. It is kept
    // because it is what the bytes do, and because a future lane that inserts
    // anything between the two wants to see retail's own ordering rather than
    // this lane's opinion of it.
    ::Graphics::FrameCopyImageBuffer::DoorBody::Init(
        reinterpret_cast<::Graphics::FrameCopyImageBuffer*>(self));   // B.3.8 bridge

    return self;                                     // MOV RAX,RSI
}

// ============================================================================
// §2.  ??1FrameCopyImageBuffer@Graphics@@UEAA@XZ   retail 0x1800a3c80, ord 597
// ============================================================================
// 138 bytes / 37 instructions. Mirror of the ctor, plus the base-class handback.
//
// The `UEAA` spelling is the one that exists (see banner). This is the SCALAR
// destructor; vtable slot 0 is the separate deleting destructor (0x18001f260),
// which is not this row and is not ported here.
//
// The member at +0x70 is destroyed through ITS OWN vtable slot 0 with EDX=1 —
// a DELETING destructor call, so it both destructs and frees. The two
// RenderBuffers are destroyed the long way (`PlatformDestruct` then
// `operator delete`) because RenderBuffer has no virtual destructor to reach.
// Those are not the same idiom and swapping one for the other would leak or
// double-free (reference_a_written_recipe_can_be_wrong.md is the general form).
// ============================================================================
extern "C" void RtGraphics_FrameCopyImageBuffer_Dtor(void* self) {
    Attend("FrameCopyImageBuffer::dtor", g_dtorCalls);

    FrameCopyImageBuffer* const p = static_cast<FrameCopyImageBuffer*>(self);

    p->base.vtable = VtFrameCopy();                  // 0x1800a3c9b

    // ENDGAME B.3.8 block RB (RULE B8-1): ONE body with `~FrameCopyImageBuffer()`.
    ::Graphics::FrameCopyImageBuffer::DoorBody::Release(
        reinterpret_cast<::Graphics::FrameCopyImageBuffer*>(self));   // B.3.8 bridge

    p->base.vtable = VtGenerated();                  // 0x1800a3cee
    ImageBufferDtor(p);                              // tail JMP 0x1800f2610
}

// ============================================================================
// Graphics::FrameCopyImageBuffer::InitFields / ReleaseFields (ENDGAME B.3.8
// block RB, RULE B8-1): 0x1800a3d10 after the FrameCopy stamp, and 0x1800a3c80
// between its restamp and the Generated restamp. Shared by the real ctor/dtor
// (FrameCopyImageBuffer.h) and the two doors above; MOVED VERBATIM through the
// field-identical `RtGraphics::FrameCopyImageBuffer` POD view.
// ============================================================================
void Graphics::FrameCopyImageBuffer::InitFields() {
    RtGraphics::FrameCopyImageBuffer* const p =
        reinterpret_cast<RtGraphics::FrameCopyImageBuffer*>(this);   // B.3.8 bridge (the POD view)
    p->_backBufferImage = nullptr;                   // 0x1800a3d4c
    p->_sourceTarget    = nullptr;                   // 0x1800a3d50
    p->_destTarget      = nullptr;                   // 0x1800a3d54

    // --- the back-buffer alias ---------------------------------------------
    void* const image = MemAlloc(0x68, 8);           // LEA EDX,[RBP+0x68] / LEA R8D,[RBP+8]
    if (image != nullptr) ImageBufferCtor(image);
    p->_backBufferImage = image;                     // never the ctor's return; see banner

    // `MOV RCX,[RAX+0x60]; MOV RAX,[RCX]; CALL [RAX+8]` — slot 1 of the
    // ImageBuffer's platform companion, `this` and nothing else (retail sets no
    // other argument register). dx11/src/imagebufferdata.cpp:11 names slot 1
    // "alias the DeviceData color buffer, build an RTV", which is what makes
    // this member the back buffer rather than a fresh texture.
    {
        void* const data = *reinterpret_cast<void**>(
            reinterpret_cast<uint8_t*>(image) + 0x60);      // faithful: unguarded
        void** const vt = *reinterpret_cast<void***>(data);
        reinterpret_cast<void (*)(void*)>(vt[1])(data);
    }

    // --- the RenderBuffer over it -------------------------------------------
    void* const rb = MemAlloc(0x30, 8);              // MOV EDX,0x30 / LEA R8D,[RDX-0x28]
    if (rb != nullptr) {
        // Three qword zero stores INSIDE the success arm, then PlatformConstruct
        // — the same shape saveload_draw.cpp:497 transcribed for the standalone
        // RenderBuffer it builds.
        reinterpret_cast<uint64_t*>(rb)[0] = 0;
        reinterpret_cast<uint64_t*>(rb)[1] = 0;
        reinterpret_cast<uint64_t*>(rb)[2] = 0;
        RenderBufferPlatformConstruct(rb);
    }
    p->_sourceTarget = rb;

    RenderBufferCreate(rb, p->_backBufferImage, nullptr);   // XOR R8D,R8D => depth = null
}

void Graphics::FrameCopyImageBuffer::ReleaseFields() {
    RtGraphics::FrameCopyImageBuffer* const p =
        reinterpret_cast<RtGraphics::FrameCopyImageBuffer*>(this);   // B.3.8 bridge (the POD view)

    if (p->_sourceTarget != nullptr) {               // +0x78
        RenderBufferPlatformDestruct(p->_sourceTarget);
        RenderBufferOperatorDelete(p->_sourceTarget);
    }
    if (p->_destTarget != nullptr) {                 // +0x80
        RenderBufferPlatformDestruct(p->_destTarget);
        RenderBufferOperatorDelete(p->_destTarget);
    }
    if (p->_backBufferImage != nullptr) {            // +0x70
        void** const vt = *reinterpret_cast<void***>(p->_backBufferImage);
        // EDX = 1 -> the deleting destructor frees as well as destructs.
        reinterpret_cast<void (*)(void*, int)>(vt[0])(p->_backBufferImage, 1);
    }
}

// ============================================================================
// §3.  ?Create@FrameCopyImageBuffer@Graphics@@UEAA_N…  retail 0x1800a3b70, 1877
// ============================================================================
// 197 bytes / 54 instructions. ELEVEN parameters plus `this`, and the order is
// read from the export table's own mangled name, not guessed from the call
// site — `_N Create(ImageType, Usage, Format, unsigned int, int, int,
// FilterMode, AddressMode, AddressMode, AddressMode,
// Collection::Array<Collection::Array<uchar>>*)`. saveload_draw.cpp:463 typed
// its vtable call from the same reading, independently.
//
// *** THE STORES ARE NOT IN PARAMETER ORDER. *** Retail writes
// +0x28/+0x2c/+0x30 from the three register arguments, then +0x20 and +0x24
// from the FIFTH and SIXTH stack arguments, then +0x34 from the FOURTH. A
// transcription that assumed "stack args land in ascending field order" would
// put the flags word into `width`. The mapping below is instruction-by-
// instruction from 0x1800a3b87-0x1800a3bc9, cross-checked against the field
// names runtime/types/video/ImageBuffer.h recovered from the DX11 side.
// ============================================================================
// ENDGAME B.3.5 block GC: THE BODY MOVED onto
// `Graphics::FrameCopyImageBuffer::Create` -- vtable SLOT 9, which this class
// OVERRIDES (the header's correction to B35_RULES 2.3 item 5). The door keeps its
// name, its registration and its entry counter.
extern "C" bool RtGraphics_FrameCopyImageBuffer_Create(
        void* self,
        int32_t imageType, int32_t usage, int32_t format,
        uint32_t flags, int32_t width, int32_t height,
        int32_t filterMode,
        int32_t addressU, int32_t addressV, int32_t addressW,
        void* levels) {
    Attend("FrameCopyImageBuffer::Create", g_createCalls);
    return reinterpret_cast<Graphics::FrameCopyImageBuffer*>(self)       // B.3.5 bridge
        ->Graphics::FrameCopyImageBuffer::Create(
            static_cast<Video::ImageBuffer::ImageType>(imageType),
            static_cast<Video::ImageBuffer::Usage>(usage),
            static_cast<Video::ImageBuffer::Format>(format),
            flags, width, height,
            static_cast<Video::ImageBuffer::FilterMode>(filterMode),
            static_cast<Video::ImageBuffer::AddressMode>(addressU),
            static_cast<Video::ImageBuffer::AddressMode>(addressV),
            static_cast<Video::ImageBuffer::AddressMode>(addressW),
            static_cast<Video::ImageBuffer::LevelArray*>(levels));
}

// ============================================================================
// §4.  ?Generate@FrameCopyImageBuffer@Graphics@@UEBAX…  retail 0x1800a3b20, 2395
// ============================================================================
// 60 bytes / 18 instructions, and `UEBA` — const. The whole body is behind one
// integer comparison, which is why the counter below counts the BRANCH and not
// the entry (reference_calls_zero_can_mean_always_took_the_uncovered_branch.md:
// an entry count here is exactly the number that cannot tell you whether the
// copy happened).
//
// THREE arguments at the indirect call, not two. `MOV R9B,0x1` at 0x1800a3b41
// sets a fourth register before `CALL [RAX+0x18]`, so the companion's slot 3
// takes (this, Context*, RenderBuffer*, bool). saveload_draw.cpp:75 records the
// same class of defect on the call INTO this function, found the same way.
// ============================================================================
// ENDGAME B.3.5 block GC: THE BODY MOVED onto
// `Graphics::FrameCopyImageBuffer::Generate` -- vtable slot 10. The door keeps its
// name, its registration and the ENTRY counter; the BLIT counter is a branch
// counter and stays inside the member, where the branch is.
extern "C" void RtGraphics_FrameCopyImageBuffer_Generate(
        void* self, void* context, int32_t generation) {
    Attend("FrameCopyImageBuffer::Generate", g_generateCalls);
    reinterpret_cast<const Graphics::FrameCopyImageBuffer*>(self)        // B.3.5 bridge
        ->Graphics::FrameCopyImageBuffer::Generate(context, generation);
}

// ============================================================================
// §5.  ?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z
//      retail 0x1800fc240, ordinal 4473. 430 bytes / 104 instructions, ONE call.
// ============================================================================
// THE INHERITED HOLDBACK NAMED THE WRONG HAZARD, AND THIS LANE CHECKED BOTH
// HALVES OF IT (rule 3). runtime/src/graphics/animation.cpp:70-79 held this
// back on "a wrong argument-slot mapping in the 9-argument
// AnimationState::Update call … through a nested, UNEXPORTED AnimationState
// record". Wave 21 already showed the callee IS exported; this lane confirms it
// with pefile — 0x1800fdb20, ordinal 4474 — and its mangled name spells all
// eight parameters and their types, so the named risk is answerable from the
// export table with no reversing at all. The residual risk is the ITERATION
// LAYOUT, which is what §5.1 pins.
//
// The SECOND half of that holdback ("it runs behind a default-on non-retail
// batching path, standalone/perf_experimental.cpp ANIM_BATCH_ALL") was checked
// too and is NOT a blocker on this body: that path decides which MeshInstances
// REGISTER a group with the manager, i.e. it changes this function's INPUT, not
// its contract. A manager with no groups makes this body a single compare and a
// return — which is exactly why `g_animUpdStates` exists.
//
// THIS ROW GROWS THE CUTOVER SURFACE BY ONE, and it is the only place in this
// file that does. `?Update@AnimationState@Graphics@@…` appears NOWHERE in the
// census today — not in `called`, not in `ported`. Porting this body puts it
// there, unported (reference_porting_can_grow_the_cutover_surface.md). Said out
// loud because the census prints the new row without saying who added it.
//
// ---------------------------------------------------------------------------
// §5.1  THE TWO RECORDS, AND A CORRECTION TO runtime/types/graphics/Animation.h
// ---------------------------------------------------------------------------
// `AnimationManager` +0x00/+0x08/+0x0c are a `Collection::Array<Group>`
// (_data / _size / _capacity), NOT the CRITICAL_SECTION-shaped triple that
// header currently describes. Proof, from this body alone: 0x1800fc268 reads
// `*(int*)(this+8)` as a LOOP BOUND and 0x1800fc2a0 reads `*(void**)this` as
// the base of an array of 0x30-byte records. A LockCount is not a loop bound.
// The header's own dtor note gives the second half of the proof once read the
// other way round: it says ~AnimationManager calls FUN_18005bf60 "as if `this`
// were an LPCRITICAL_SECTION (it reads ->DebugInfo, ->LockCount,
// ->RecursionCount at exactly +0x00/+0x08/+0x0c)". Those three offsets are
// _data / _size / _capacity, and FUN_18005bf60 destroys each element, frees
// _data, then zeroes all three — it is `Collection::Array<Group>::~Array`.
// Ghidra typed its parameter LPCRITICAL_SECTION only because it frees under the
// allocator's own critical section. The header is corrected in this branch; the
// ported ctor and dtor in animation.cpp needed no behavioural change, because
// they already delegate to the real helper at its retail address.
//
//   Group, stride 0x30      +0x00  pointer, null => skip the whole group
//                           +0x10  Collection::Array<GraphicsMesh::Node>, passed BY REFERENCE
//                           +0x20  AnimationState array base
//                           +0x28  int state count
//
//   AnimationState, stride 0x48
//                           +0x18  Math::Vector3   (written by the callee; read back here)
//                           +0x28  InterpolatedState<Transform>*   (6th arg, and the write target)
//                           +0x38  InterpolatedState<Transform>*   (5th arg)
//
// Wave 21 §4.3 enumerated the same offsets from Ghidra's C and explicitly
// recorded that they had NOT been checked against the disassembly. They now
// have been, and they hold — but the check was worth doing, because the SIGN of
// such an agreement is not informative until someone runs it
// (reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// §5.2  THE IDENTITY TRANSFORM IS A CONSTANT, NOT A COMPUTATION
// ---------------------------------------------------------------------------
// The 4th argument is a stack `Math::Transform` rebuilt every iteration as
// {0,0,0, 1.0f, 0,0,0}: quaternion identity then a zero Vector3. The 1.0f comes
// from `MOVSS XMM7,[0x18016169c]`, and the four bytes there were READ
// (00 00 80 3f) rather than assumed — a float constant lifted by eye is how
// reference_crt_coefficients_must_be_lifted_not_retyped.md got written.
//
// ---------------------------------------------------------------------------
// §5.3  WHAT RETAIL DOES AFTER THE CALL, AND WHY IT IS AN INT COPY
// ---------------------------------------------------------------------------
// It writes a Transform into `*state->_interp28`: identity quaternion, then the
// Vector3 the callee just left at `state+0x18`, moved as THREE DWORDS
// (`MOV ECX,[RBX]` / `MOV EDX,[RBX+4]` / `MOV R8D,[RBX+8]` then three
// `MOV dword`). Copied as uint32 here for the same reason
// wave21_lane_gfxbody.cpp gives for its four constants: an integer move of a
// float is bit-exact where a float assignment is only value-exact, and the two
// differ on a signalling NaN or a negative zero.
//
// ---------------------------------------------------------------------------
// §5.4  THE BOUNDS ARE RE-READ EVERY ITERATION, AND THAT IS TRANSCRIBED
// ---------------------------------------------------------------------------
// Both loops reload their limit from memory at the bottom (`CMP R12D,
// [RSI+RBP+0x28]`, `CMP R14D,[R15+8]`) and the outer loop reloads `_data` at
// the top. The callee could in principle grow either array, so this is not
// merely stylistic — it is written unhoisted because that is what the bytes do.
// ============================================================================
namespace {

struct AnimGroup {          // stride 0x30
    void*    _f00;          // +0x00  null => skip
    uint64_t _f08;
    uint8_t  _nodes[0x10];  // +0x10  Collection::Array<GraphicsMesh::Node>, passed by reference
    void*    _states;       // +0x20
    int32_t  _stateCount;   // +0x28
    int32_t  _f2c;
};
static_assert(sizeof(AnimGroup) == 0x30, "AnimationManager group stride is 0x30");
static_assert(offsetof(AnimGroup, _nodes) == 0x10, "group node array @ +0x10");
static_assert(offsetof(AnimGroup, _states) == 0x20, "group state array @ +0x20");
static_assert(offsetof(AnimGroup, _stateCount) == 0x28, "group state count @ +0x28");

struct AnimTransform {      // Math::Transform — Quaternion then Vector3, 0x1c bytes
    float q[4];
    float t[3];
};
static_assert(sizeof(AnimTransform) == 0x1c, "Math::Transform is 28 bytes");

// The nine-argument `RT_EXPORT_ONCE` forwarder onto
// `?Update@AnimationState@Graphics@@` that used to sit here is GONE: block GC
// made that a real member of `Graphics::AnimationState`, so the call site is a
// QUALIFIED member call. The mangled name -- which is what settled the argument
// mapping, and is a stronger source than any prologue read -- is kept at the
// call site.

}  // namespace

// ENDGAME B.3.5 block GC: THE BODY MOVED to
// `Graphics::AnimationManager::Update`, in this file's `namespace Graphics` block
// at the bottom. The door keeps its name, its registration and BOTH counters --
// the entry count here and the per-state count inside the loop, which the file's
// summary line prints and which measure different things.
extern "C" void RtGraphics_AnimationManager_Update(void* self, const void* time, bool rootOnly) {
    Attend("AnimationManager::Update", g_animUpdCalls);
    reinterpret_cast<Graphics::AnimationManager*>(self)                 // B.3.5 bridge
        ->Graphics::AnimationManager::Update(
            *reinterpret_cast<const System::Time*>(time), rootOnly);    // B.3.5 bridge
}

// ============================================================================
// Registration.
//
// NAMES ONLY, AND EVERY ADDRESS WAS CHECKED FIRST rather than skipped.
// wave21_lane_gfxbody.cpp:311 explains why that check is not ceremony: this
// same namespace contains `?AddDrawItems@MeshInstance@`, where a
// RegisterRuntimeAddr is ACTIVE HARM because standalone/scene_watch.cpp:400
// compares that retail address against a live vtable slot as an IDENTITY TOKEN.
//
//   0x1800a3d10, 0x1800a3c80, 0x1800a3b70  — occur ONLY in the holdback table
//                                            at runtime/src/graphics/
//                                            animation.cpp:27-29 (comments).
//   0x1800a3b20                            — animation.cpp:30 and
//                                            saveload_draw.cpp:83, both comments.
//   0x1800fc240                            — animation.cpp:20 and
//                                            standalone/address_registry.cpp:1522,
//                                            both comments; the registration on
//                                            the line below :1522 is a
//                                            RegisterDispatchSlot keyed on the
//                                            EXE slot 0x140152050 and filled by
//                                            ResolveExport(<name>), so it is a
//                                            NAME door and this registration
//                                            reaches it.
//
// No hit is an identity token and none is a call door a name would miss, so an
// address registration would be dead code (reference_count_the_doors_not_the_calls.md).
//
// THE DOORS THESE FIVE ACTUALLY HAVE, stated so nobody has to re-derive them:
//   ctor / dtor / Create / Generate  <- code/Game/Game/w2_takescreenshot.cpp:148-154
//                                       resolves ALL FOUR by ResolveExport name.
//   ctor                             <- runtime/src/save/saveload_draw.cpp:453
//                                       (RT_EXPORT_ONCE, a name door). Create and
//                                       Generate are reached there through the
//                                       object's DLL vtable instead, which is NOT
//                                       a door while ??_7FrameCopyImageBuffer is
//                                       unpromoted.
//   AnimationManager::Update         <- standalone/address_registry.cpp:1523,
//                                       exe dispatch slot 0x140152050.
// RuntimePortInit() runs BEFORE AddressRegistryInit() (standalone/main.cpp), so
// that dispatch slot is filled AFTER this registration and picks it up.
//
// Each mangled name is its own STRING LITERAL, never a named constant:
// tools/runtime_census.py matches `RegisterRuntimeExport("<literal>"` and cannot
// see through a constant
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
int RtGraphicsWave23GfxbodyRegister() {
    int count = 0;

    RegisterRuntimeExport("??0FrameCopyImageBuffer@Graphics@@QEAA@XZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Ctor); ++count;
    RegisterRuntimeExport("??1FrameCopyImageBuffer@Graphics@@UEAA@XZ",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Dtor); ++count;
    RegisterRuntimeExport("?Create@FrameCopyImageBuffer@Graphics@@UEAA_NW4ImageType@ImageBuffer@Video@@"
                          "W4Usage@45@W4Format@45@IHHW4FilterMode@45@W4AddressMode@45@44PEAV?$Array@"
                          "V?$Array@E@Collection@@@Collection@@@Z",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Create); ++count;
    RegisterRuntimeExport("?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z",
                          (void*)&RtGraphics_FrameCopyImageBuffer_Generate); ++count;
    RegisterRuntimeExport("?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z",
                          (void*)&RtGraphics_AnimationManager_Update); ++count;

    return count;   // 5
}

// One line beside the other wave summaries, so a run that entered none of these
// says so rather than staying silent.
void RtGraphicsWave23GfxbodySummary() {
    printf("[GFXBODY23] ctor=%llu dtor=%llu Create=%llu Generate=%llu (blits=%llu) "
           "AnimUpdate=%llu (states=%llu)\n",
           g_ctorCalls, g_dtorCalls, g_createCalls, g_generateCalls, g_generateCopies,
           g_animUpdCalls, g_animUpdStates);
}

// ############################################################################
// ENDGAME B.3.5 step 5, block GC -- `Graphics::AnimationManager::Update`'s body,
// whose live door is the `extern "C"` function above.
//
// Its own `namespace Graphics { }` block, because a member definition has to
// appear in a namespace ENCLOSING the class and `Graphics` / `RtGraphics` are
// siblings (B34_BRIEFS A.2). This file's helpers are in the GLOBAL unnamed
// namespace and are spelled `::` here, because inside `namespace Graphics` a
// bare `AnimGroup` or `AnimTransform` would look inside `Graphics` first.
// ############################################################################

namespace Graphics {

// ---------------------------------------------------------------------------
// ?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z 0x1800fc240
// ENDGAME B.3.5 block GC. The group/state offsets, the identity-transform
// constant, the integer copy and the re-read bounds are all in the banner beside
// the door above.
//
// `_instances` is the `Array<Instance>` at +0x00, so retail's `[RCX]` and
// `[RCX+8]` are `_instances._data` and `_instances._size` -- the same two loads,
// now held by AnimationManager.h's own `static_assert`s. The GROUP record is
// still this file's `AnimGroup` POD: it is field-for-field
// `Graphics::AnimationManager::Instance` and its four asserts stay, because the
// POD has live users outside this file and B4b-1 keeps it until the last one
// converts.
//
// THE CALLEE EDGE IS NO LONGER A RESOLVER HIT (block GC item 6). The nine-argument
// `RT_EXPORT_ONCE` onto `?Update@AnimationState@Graphics@@` is a QUALIFIED
// non-virtual member call on `Graphics::AnimationState`, whose body block GC also
// moved onto its member (`wave27_lane_gfxbig.cpp`). Its registered door is
// untouched and still serves every caller outside this file. The nine-argument
// mangled name that settled the argument mapping is kept in the comment: that is
// what the banner's "a stronger source than either" sentence was about.
// ---------------------------------------------------------------------------
void AnimationManager::Update(const System::Time& time, bool rootOnly) {
    int32_t groupIndex = 0;
    if (_instances._size <= 0) return;             // [RCX+8], Array::_size

    for (;;) {
        uint8_t* const groupBase = _instances._data;    // [RCX], reloaded each pass
        ::AnimGroup* const g = reinterpret_cast<::AnimGroup*>(groupBase + groupIndex * 0x30);

        if (g->_f00 != nullptr && g->_stateCount > 0) {
            // Control 5 hoists this bound. It is the EFFECTIVENESS control and
            // is EXPECTED TO REPORT NOT CAUGHT: nothing reachable here grows the
            // array, so the hoist is genuinely unobservable and a control set in
            // which every control fires proves only that the controls were
            // picked to fire (reference_probe_needs_an_effectiveness_control.md).
            for (int32_t i = 0; i < g->_stateCount; ++i) {
                uint8_t* const state =
                    static_cast<uint8_t*>(g->_states)
                    + static_cast<int64_t>(i) * 0x48;

                // Rebuilt inside the loop, exactly as retail does (the callee
                // takes it by const reference and could keep the address).
                ::AnimTransform base;
                base.q[0] = 0.0f; base.q[1] = 0.0f; base.q[2] = 0.0f;
                base.q[3] = 1.0f;                      // [0x18016169c] == 00 00 80 3f
                base.t[0] = 0.0f; base.t[1] = 0.0f; base.t[2] = 0.0f;

                void* interp38 = *reinterpret_cast<void**>(state + 0x38);  // 5th arg
                void* interp28 = *reinterpret_cast<void**>(state + 0x28);  // 6th arg
                void* const outVec3 = state + 0x18;                        // 7th arg

                // ?Update@AnimationState@Graphics@@QEAA_NAEBVTime@System@@AEBV?$Array@
                // UNode@GraphicsMesh@Graphics@@@Collection@@AEBVTransform@Math@@PEAV?$
                // InterpolatedState@VTransform@Math@@@2@PEAV78@AEAVVector3@8@_N6@Z
                // retail 0x1800fdb20 -- eight parameters plus `this`, in the order
                // that mangled name gives them. The 9th is retail's literal 0
                // (MOV byte [RSP+0x40],0), NOT the bool this function was given.
                reinterpret_cast<Graphics::AnimationState*>(state)      // B.3.5 bridge
                    ->Graphics::AnimationState::Update(
                        time,
                        *reinterpret_cast<const Collection::Array<uint8_t>*>(&g->_nodes),
                        *reinterpret_cast<const Math::Transform*>(&base),
                        interp38, interp28,
                        *reinterpret_cast<Math::Vector3*>(outVec3),
                        rootOnly, false);

                // Write the identity-rotation Transform carrying the Vector3 the
                // callee produced into *state->_interp28. `_interp28` is RE-READ
                // (`MOV RAX,[RDI+0x28]` at 0x1800fc33b) rather than reused.
                uint8_t* const dst = *reinterpret_cast<uint8_t**>(state + 0x28);
                const uint32_t* const src =
                    reinterpret_cast<const uint32_t*>(state + 0x18);
                float* const dq = reinterpret_cast<float*>(dst);
                dq[0] = 0.0f; dq[1] = 0.0f; dq[2] = 0.0f;
                dq[3] = 1.0f;
                uint32_t* const dt = reinterpret_cast<uint32_t*>(dst + 0x10);
                dt[0] = src[0]; dt[1] = src[1]; dt[2] = src[2];   // integer moves, see §5.3

                ++::g_animUpdStates;
            }
        }

        ++groupIndex;
        if (groupIndex >= _instances._size) break;     // the bound is RE-READ
    }
}

// ---------------------------------------------------------------------------
// ?Create@FrameCopyImageBuffer@Graphics@@  0x1800a3b70 -- SLOT 9, ENDGAME B.3.5
// block GC. The argument-to-offset map, the two-argument platform slot 3 call and
// the "RDX is `this`, NOT `_backBufferImage`" note are in the door's banner.
//
// Ten raw `base.*` POD writes became the real `Video::ImageBuffer` fields, which
// is what the inheritance chain is for; `self + 0x60` is `_data`.
// ---------------------------------------------------------------------------
bool FrameCopyImageBuffer::Create(ImageType imageType, Usage usage, Format format,
                                  uint32_t flags, int32_t width, int32_t height,
                                  FilterMode filterMode,
                                  AddressMode addressU, AddressMode addressV,
                                  AddressMode addressW, LevelArray* levels) {
    _dimension = imageType;            // +0x28  <- EDX      (0x1800a3b87)
    _kind      = usage;                    // +0x2c  <- R8D      (0x1800a3b8a)
    _format    = format;            // +0x30 <- R9D (0x1800a3b8e)
    _width     = width;                    // +0x20  <- arg5     (0x1800a3b92)
    _height    = height;                  // +0x24  <- arg6     (0x1800a3b99)
    _flags     = flags;                    // +0x34  <- arg4     (0x1800a3ba0)
    _filter    = filterMode;          // +0x44  <- arg7     (0x1800a3ba7)
    _addressU  = addressU;              // +0x38  <- arg8     (0x1800a3bae)
    _addressV  = addressV;              // +0x3c  <- arg9     (0x1800a3bb8)
    _addressW  = addressW;              // +0x40  <- arg10    (0x1800a3bc2)

    // `MOV RCX,[RCX+0x60]; MOV RAX,[RCX]; MOV RDX,<arg11>; CALL [RAX+0x18]` —
    // slot 3 of the platform companion, "create the texture and all its views"
    // (dx11/src/imagebufferdata.cpp:13), taking the level-descriptor array.
    // TWO arguments: retail sets RCX and RDX and nothing else.
    void* const data = _data;                          // +0x60
    void** const vt = *reinterpret_cast<void***>(data);
    const bool ok = reinterpret_cast<bool (*)(void*, void*)>(vt[3])(data, levels);
    if (!ok) return false;                              // TEST AL,AL / JZ -> return AL

    // The second RenderBuffer, this time with `this` as the colour attachment.
    // Here retail calls `??2RenderBuffer@Video@@SAPEAX_K@Z` (0x18001f2e0) as a
    // REAL CALL rather than inlining it — `MOV ECX,0x30; CALL 0x18001f2e0` at
    // 0x1800a3be2. Same five instructions as the inlined form in the ctor, and
    // MemAlloc(0x30, 8) is that body (see the MemAlloc note above), so this is
    // spelled the same way for one arena and one reader.
    void* const rb = MemAlloc(0x30, 8);
    if (rb != nullptr) {
        reinterpret_cast<uint64_t*>(rb)[0] = 0;
        reinterpret_cast<uint64_t*>(rb)[1] = 0;
        reinterpret_cast<uint64_t*>(rb)[2] = 0;
        RenderBufferPlatformConstruct(rb);
    }
    _destTarget = rb;                                   // +0x80

    // `XOR R8D,R8D; MOV RDX,RDI; MOV RCX,RBX` — colour is THIS, depth is null.
    // Note RDX is `this`, NOT `this->_backBufferImage`: that is the difference
    // between the ctor's RenderBuffer and this one, and it is the whole point
    // of the class (copy FROM the back-buffer alias INTO our own texture).
    RenderBufferCreate(rb, this, nullptr);

    return true;                                        // MOV AL,0x1
}

// ---------------------------------------------------------------------------
// ?Generate@FrameCopyImageBuffer@Graphics@@  0x1800a3b20 -- slot 10, block GC.
// `UEBA` is CONST and the body still writes `_generation` through it; the
// `const_cast` is at the site rather than hidden in a `mutable`, which is
// GeneratedImageBuffer.h's own rule for exactly these two slots. The
// four-argument platform slot-3 call (`MOV R9B,0x1`) is unchanged.
// ---------------------------------------------------------------------------
void FrameCopyImageBuffer::Generate(void* context, int32_t generation) const {
    if (_generation == generation) return;              // CMP [RCX+0x68],R8D / JZ

    Attend("FrameCopyImageBuffer::Generate BLIT", g_generateCopies);

    // rax = _destTarget (+0x80); rcx = rax->platformData (+0x28); r8 = _sourceTarget.
    void* const data = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(_destTarget) + 0x28);   // faithful: unguarded
    void** const vt = *reinterpret_cast<void***>(data);
    reinterpret_cast<void (*)(void*, void*, void*, bool)>(vt[3])(
        data, context, _sourceTarget, true);

    const_cast<FrameCopyImageBuffer*>(this)->_generation = generation;  // [RBX+0x68]
}

}  // namespace Graphics
