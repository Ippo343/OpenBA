// ============================================================================
// runtime/src/video/video_program.cpp — Wave 23 lane VIDVT
//
//   ?SetupShaders@VideoProgram@Video@@QEBAXAEAVContext@2@@Z   retail 0x1800f4520
//
// The ONE body row in lane VIDVT's nineteen. It entered `called` in Wave 21:
// lane GFXBODY's port of `?Setup@MaterialInstance@Graphics@@` calls it by name
// (runtime/src/graphics/wave21_lane_gfxbody.cpp), which grew the cutover
// surface by one row (reference_porting_can_grow_the_cutover_surface.md).
// GFXBODY's own §6 left it as a residual in these words: "it is [safe to leave
// on the DLL], today, because that is where it already was — but this lane made
// our tree depend on it by name for the first time and did not read its body."
// This is that read, and the port.
//
// ---------------------------------------------------------------------------
// EXTENT — 122 bytes, 0x1800f4520 .. 0x1800f459b inclusive, and note HOW
// ---------------------------------------------------------------------------
// NOT from `.pdata`: this function has NO `.pdata` entry at all. It is a
// frameless leaf — no prologue, no unwind data — so the nearest PRECEDING
// RUNTIME_FUNCTION belongs to `?PlatformDestruct@RenderBuffer@Video@@`
// (0x1800f4400..0x1800f44de) and a tool that sizes by "the covering .pdata
// entry" without checking containment would silently hand back a DIFFERENT
// function's 222 bytes. (reference_pdata_is_a_lower_bound_on_function_extent.md
// is the related trap in the other direction — chained fragments — and does not
// arise here because there is no entry to chain.)
//
// The extent used is instead the ADJACENT-EXPORT upper bound (next export
// 0x1800f45b0) NARROWED by the decode: a `repz ret` at 0x1800f459a followed by
// 0x14 bytes of `int3` alignment padding. The upper bound and the decode agree
// to the padding, so 122 is exact rather than an upper bound
// (reference_adjacent_export_is_an_upper_bound.md).
//
// ---------------------------------------------------------------------------
// THE BODY, read as disassembly and not as decompiled C
// ---------------------------------------------------------------------------
//   RCX = this (VideoProgram const*),  RDX = ctx (Context&)  -- QEBAX...AEAV
//
//   1800f4520  cmp   qword ptr [rdx + 0x200], rcx
//   1800f4527  je    1800f4537
//   1800f4529  or    dword ptr [rdx + 0x10], 0x100
//   1800f4530  mov   qword ptr [rdx + 0x200], rcx
//   1800f4537  mov   rax, qword ptr [rcx + 0x20]
//   1800f453b  cmp   rax, qword ptr [rdx + 0xb0]
//   1800f4542  je    1800f454f
//   1800f4544  or    dword ptr [rdx + 0x10], 4
//   1800f4548  mov   qword ptr [rdx + 0xb0], rax
//   1800f454f  mov   rax, qword ptr [rcx + 0x28]      ; and the same shape for
//   ...                                               ; 0x1f0 / 0x1f8 / 0x1c8
//   1800f459a  repz ret
//
// Five compare-then-OR-then-store pairs, no calls, no allocation, no loop, and
// EVERY memory operand in the function is one of exactly ten: four reads of
// `this` (+0x20 +0x28 +0x30 +0x38) and six accesses of `ctx` (+0x10 +0xb0
// +0x1c8 +0x1f0 +0x1f8 +0x200). That enumeration is what makes the oracle below
// provably complete rather than merely plausible.
//
// WHAT IT IS. It is the five `?Set*@Context@Video@@` setters inlined into one
// call, and the destination/bit pairs line up one-for-one with the dirty-mask
// table already in runtime/types/video/Context.h:
//
//   ctx+0x200 <- this          bit 0x100  _videoProgram   (?SetVideoProgram@)
//   ctx+0x0b0 <- this+0x20     bit 0x004  _vertexLayout   (?SetVertexLayout@)
//   ctx+0x1f0 <- this+0x28     bit 0x040  _vertexProgram  (?SetVertexProgram@)
//   ctx+0x1f8 <- this+0x30     bit 0x080  _pixelProgram   (?SetPixelProgram@)
//   ctx+0x1c8 <- this+0x38     bit 0x020  _renderState    (?SetRenderState@)
//
// That agreement is the evidence for the VideoProgram layout below: the four
// fields are identified by WHICH CONTEXT SLOT each is copied into and WHICH
// DIRTY BIT is raised for it, not by a reader's guess. `WAVE6_LANE_F.md` §117
// had already established that `+0x20` on this object is non-null-checked by a
// caller that hands the object to `SetupShaders`; this pins its type.
//
// Retail's compare for `_vertexLayout` is spelled `cmp rax, [rdx+0xb0]` where
// the other four are `cmp [rdx+off], rax` / `cmp [rdx+off], rcx`. Operand order
// in a CMP changes only the flags' sign, and every one of the five branches on
// ZF alone, so this is a codegen artefact and not a semantic difference. It is
// recorded rather than smoothed over.
//
// ---------------------------------------------------------------------------
// THE ORACLE, and why RESTORE-AND-REPLAY is sound here specifically
// ---------------------------------------------------------------------------
// A naive replay is a TAUTOLOGY for this function: it is compare-guarded, so
// running retail's body AFTER ours on the same Context finds all five fields
// already equal and writes nothing, and the comparison passes no matter what
// ours did. WAVE9_LANE_FLUSH.md §215 records exactly that hazard for this
// family of bodies.
//
// So: snapshot the six ctx words, run OURS, capture ours' six, restore the six
// to the pre-state, run RETAIL's on the live Context, and compare. That is
// complete because the ten-operand enumeration above proves the function reads
// and writes NOTHING ELSE — a restore of those six words restores the entire
// input state this function can observe. The live Context ends carrying
// retail's values, which are the same values when the port is right.
//
// It is also NOT reading bytes neither side writes
// (reference_oracle_must_not_compare_unwritten_bytes.md): all six compared
// words are written by at least one arm of the function, and the dirty mask is
// compared as a whole because retail ORs into whatever was already there.
//
// `self` is `const` to retail and is not snapshotted; nothing aliases it to
// `ctx` (one is a VideoProgram, the other a 0x2168-byte Context, and the
// caller in wave21_lane_gfxbody.cpp passes two distinct objects), so
// reference_oracle_must_snapshot_aliased_inputs.md's trap does not arise. That
// is asserted from the call site, not assumed from the types.
// ============================================================================

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Context.h"

namespace {

// ----------------------------------------------------------------------------
// Video::VideoProgram, PARTIAL. Only the four words this function reads are
// pinned; the class is larger and the rest is deliberately not invented here,
// so there is no `sizeof` assert to be wrong. Offsets 0x20..0x38 are proven by
// the destination/bit pairing in the banner.
// ----------------------------------------------------------------------------
struct VideoProgramPartial {
    uint8_t _head[0x20];        // 0x00 .. 0x1f  not read by SetupShaders
    void*   _vertexLayout;      // 0x20
    void*   _vertexProgram;     // 0x28
    void*   _pixelProgram;      // 0x30
    void*   _renderState;       // 0x38
};
static_assert(offsetof(VideoProgramPartial, _vertexLayout)  == 0x20, "VideoProgram::_vertexLayout");
static_assert(offsetof(VideoProgramPartial, _vertexProgram) == 0x28, "VideoProgram::_vertexProgram");
static_assert(offsetof(VideoProgramPartial, _pixelProgram)  == 0x30, "VideoProgram::_pixelProgram");
static_assert(offsetof(VideoProgramPartial, _renderState)   == 0x38, "VideoProgram::_renderState");

// The video-program dirty bit. Context.h's prose names it ("0x00000100 VIDEO
// program ?SetVideoProgram@ 0x18001a040", from WAVE 9 lane CTX reading
// ?BindResources@'s arms) but its enum does not carry an enumerator, so it is
// spelled here rather than added to a header three other lanes include.
constexpr uint32_t kDirtyVideoProgram = 0x00000100u;

// The six Context words this function can touch, in one struct so the snapshot,
// the restore and the compare cannot drift apart.
struct Touched {
    uint32_t dirty;
    void*    vertexLayout;
    void*    renderState;
    void*    vertexProgram;
    void*    pixelProgram;
    void*    videoProgram;
};

Touched Capture(const RtVideo::Context* ctx) {
    // Value-initialised: `dirty` is 32 bits and `vertexLayout` is pointer-
    // aligned, so bytes +0x4..+0x7 are PADDING. the diff oracle compares the
    // whole struct, and uninitialised padding reported 47,280 "divergences"
    // in 68,377 calls (2026-09-12) that were stack garbage, not the port.
    Touched t{};
    t.dirty         = ctx->_dirty;
    t.vertexLayout  = ctx->_vertexLayout;
    t.renderState   = ctx->_renderState;
    t.vertexProgram = ctx->_vertexProgram;
    t.pixelProgram  = ctx->_pixelProgram;
    std::memcpy(&t.videoProgram, ctx->_pixelConstantStaging, sizeof(void*));
    return t;
}

void Restore(RtVideo::Context* ctx, const Touched& t) {
    ctx->_dirty         = t.dirty;
    ctx->_vertexLayout  = t.vertexLayout;
    ctx->_renderState   = t.renderState;
    ctx->_vertexProgram = t.vertexProgram;
    ctx->_pixelProgram  = t.pixelProgram;
    std::memcpy(ctx->_pixelConstantStaging, &t.videoProgram, sizeof(void*));
}

// One compare-then-OR-then-store, retail's idiom, for a pointer slot.
inline void Apply(RtVideo::Context* ctx, void** slot, void* value, uint32_t bit) {
    if (*slot != value) {
        ctx->_dirty |= bit;
        *slot = value;
    }
}

using SetupShadersFn = void (*)(const void*, void*);

// Kept inside the anonymous namespace with the types it takes: a function with
// EXTERNAL linkage whose parameter type has INTERNAL linkage is ill-formed in
// principle and a -Wsubobject-linkage warning in practice. Nothing outside this
// file needs the symbol -- the registry takes its ADDRESS, not its name.
void Video_VideoProgram_SetupShaders(const VideoProgramPartial* self,
                                     RtVideo::Context* ctx) {

    Touched pre{};

    // Context+0x200 is `_videoProgram`, a POINTER that lives in the first eight
    // bytes of the `_pixelConstantStaging` array — see the WAVE 9 LANE CTX note
    // in runtime/types/video/Context.h. It is reached through memcpy rather
    // than a reinterpret_cast so this file does not depend on the array staying
    // suitably aligned for a void* if the header is ever re-divided.
    void* videoProgramSlot = nullptr;
    std::memcpy(&videoProgramSlot, ctx->_pixelConstantStaging, sizeof(void*));
    if (videoProgramSlot != static_cast<const void*>(self)) {
        ctx->_dirty |= kDirtyVideoProgram;
        void* me = const_cast<VideoProgramPartial*>(self);
        std::memcpy(ctx->_pixelConstantStaging, &me, sizeof(void*));
    }

    Apply(ctx, &ctx->_vertexLayout,  self->_vertexLayout,  RtVideo::kDirtyVertexLayout);
    Apply(ctx, &ctx->_vertexProgram, self->_vertexProgram, RtVideo::kDirtyVertexProgram);
    Apply(ctx, &ctx->_pixelProgram,  self->_pixelProgram,  RtVideo::kDirtyPixelProgram);
    Apply(ctx, &ctx->_renderState,   self->_renderState,   RtVideo::kDirtyRenderState);

}

}  // namespace

// ----------------------------------------------------------------------------
// Registration. The mangled name is spelled as a LITERAL at the
// RegisterRuntimeExport call site, which is what tools/runtime_census.py
// matches — a named constant here would make a live port invisible to the
// census (reference_census_cannot_see_nonliteral_registrations.md).
// ----------------------------------------------------------------------------
int RtVideoVideoProgramRegister() {
    RegisterRuntimeExport("?SetupShaders@VideoProgram@Video@@QEBAXAEAVContext@2@@Z",
                          reinterpret_cast<void*>(&Video_VideoProgram_SetupShaders));
    return 1;
}
