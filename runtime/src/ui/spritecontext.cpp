// ============================================================================
// runtime/src/ui/spritecontext.cpp — WAVE 9 LANE UIF — UI::SpriteContext
// ============================================================================
//
// Ports the three exported `UI::SpriteContext` methods Wave 7 lane F held back,
// plus the two unexported internals they cannot work without. The layout and
// the evidence for every offset are in runtime/types/ui/SpriteContext.h; the
// door arithmetic, the oracle design and everything this lane DECLINED are in
// docs/runtime_port/WAVE9_LANE_UIF2.md.
//
//   | Function                       | Retail      | Ported | Oracle          |
//   |--------------------------------|-------------|--------|-----------------|
//   | SetScissor(Viewport const&)     | 0x18011d050 | yes    | replay (early)  |
//   | SetScissor(int,int,int,int)     | 0x18011d010 | yes    | replay (early)  |
//   | AddLine                         | 0x18011cbe0 | yes    | replay (early)  |
//   | AddSpriteDraw   (internal)      | 0x18011c890 | yes    | replay (cull)   |
//   | FlushDeferred   (internal)      | 0x18011c6a0 | yes    | replay (no-op)  |
//   | Flush           (internal)      | 0x18011ba40 | **NO** | —               |
//   | Begin           (internal)      | 0x18011d2b0 | **NO** | —               |
//
// The two `Array<UI::SpriteDraw>` instantiations these bodies call —
// 0x180078520 (append) and 0x1800783d0 (clear) — are ported and registered by
// this lane too, but they live in runtime/src/collection/array.cpp with the
// rest of that family rather than here. See the `J3_ENTRY3(180078520, …)` and
// `RtColl_ArraySetSize_1800783d0` blocks there for why.
//
// ---------------------------------------------------------------------------
// THE HIDDEN RETURN SLOT — the one defect this file was most likely to have
// ---------------------------------------------------------------------------
// BOTH `SetScissor` overloads return `Video::Viewport` BY VALUE. Under the
// MSVC x64 ABI that is not a register return: the caller passes a pointer to a
// caller-owned slot, and for a member function that pointer is the SECOND
// argument, in RDX, after `this` in RCX. An `fn_*` typedef that omits it is a
// WILD WRITE that still returns the right answer — the callee writes through
// whatever was left in RDX and hands the same address back in RAX, so a caller
// that only reads the result looks fine while 16 bytes of unrelated memory are
// gone. That exact defect was live on every menu scene load for a whole wave
// (`fn_Math_Transform3_Identity`).
//
// It is settled here from the SHIPPED BYTES, not from the mangled name alone:
//
//   0x18011d010  MOV RBX,RDX          ; save arg2 …
//   0x18011d03d  MOV RAX,RBX          ; … and return it. RDX is the slot.
//   0x18011d050  MOV RDI,RDX          ; same, in the other overload …
//   0x18011d1c6  MOV RAX,RDI          ; … returned in RAX.
//
// and the four-int overload's stack arguments start at RSP+0x60 for a frame of
// `PUSH RBX; SUB RSP,0x30` — i.e. caller-frame +0x28, which is the FIFTH
// argument slot. Four register arguments precede it: this, slot, x, y. If the
// slot were not an argument, `w` would sit at +0x20, not +0x28.
//
// Both typedefs below therefore carry the slot explicitly, and the oracle in
// each body drives the DLL through that typedef — so if the arity were wrong
// the replay would corrupt its own scratch and report a divergence rather than
// passing quietly. `tools/abi_retslot_audit.py` covers engine_funcs only, so
// this file's proof has to be the oracle.
//
// ---------------------------------------------------------------------------
// WHY THE ORACLE ONLY COVERS THE EARLY BRANCHES, AND WHAT THAT COSTS
// ---------------------------------------------------------------------------
// None of these five bodies is replayable in full. `SetScissor` flushes the
// batch into the GPU vertex buffer and pushes a scissor record onto the
// capture; `AddLine` and `AddSpriteDraw` APPEND to the batch, so a second call
// queues the sprite twice; `FlushDeferred` drains and clears. Running the
// shipped body a second time on the live object would draw the frame twice —
// which is exactly the argument Wave 7 lane F made for the whole class, and it
// remains true for the MUTATING PATH.
//
// It is NOT true for the early-out paths, and each of those is a real branch
// taken by a large fraction of real calls:
//
//   * `SetScissor` with an unchanged rect calls NOTHING and touches NOTHING —
//     it writes four ints into the return slot and returns (0x18011d0d8).
//   * `AddLine` with a null sheet or a negative sprite index returns before
//     its first store (0x18011cc0c, 0x18011cc12).
//   * `AddSpriteDraw` that culls the record appends nothing; when the capture
//     is also null it is a complete no-op.
//   * `FlushDeferred` with `_deferring` clear returns immediately (0x18011c6ad).
//
// On those branches the shipped body is side-effect free.
//
// ONE CASE IS BETTER THAN THAT, and it is the case that matters most:
// `AddSpriteDraw`'s cull replay is safe even when the two implementations
// DISAGREE. If ours culls and the shipped body appends, the live batch ends up
// with the record retail would have queued — i.e. correct — rather than
// corrupted. So this oracle can be left on through a full gameplay run without
// a "if it fires we have already broken the frame" caveat, and it is the one
// that covers the float scissor arithmetic.
//
// ---------------------------------------------------------------------------
// FLUSH IS AN ADDRESS DOOR AND THIS FILE OPENS IT DELIBERATELY
// ---------------------------------------------------------------------------
// `Flush` (0x18011ba40) is reached through `ResolveAddr(Module::Runtime, …)`,
// which consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first.md) — so this is not
// "call the DLL", it is "call whatever is registered at that address", and the
// day someone ports Flush these five bodies pick it up with no edit here. It
// is NOT a raw door and `tools/rawdoor_audit.py` is not the instrument for it;
// `tools/addr_door_audit.py` is, and the lane doc shows the before/after
// set-diff including this row.
//
// Until then it resolves to the shipped body, and at cutover it resolves to
// null. That is a REAL COST and the lane doc states it as one rather than
// burying it: this file closes three export doors and opens one address door
// that nothing else in the tree opens
// (reference_porting_can_grow_the_cutover_surface.md).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/ui/Sprite.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/math/Math.h"
#include "../../types/video/Context.h"
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_SpriteIndexZ etc.

// ----------------------------------------------------------------------------
// Module / ResolveAddr, forward-declared rather than pulled in from
// standalone/resolve.h, which drags in <windows.h> (PLAN §4.1 keeps it out of
// runtime/src/). These must stay byte-identical to resolve.h's; the linker
// checks them. Same reasoning and same spelling as runtime/src/ui/sprite.cpp.
// ----------------------------------------------------------------------------
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using UI::ClipRect;
using ::Sprite;   // Sprite.h declares it at GLOBAL scope, not in UI
using UI::SpriteContext;
using UI::SpriteDraw;

// ---- the mangled names, once -----------------------------------------------
const char* const kAddLine =
    "?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z";
const char* const kSetScissorVp =
    "?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z";
const char* const kSetScissorInts =
    "?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z";

// ---- the ABI, spelled out --------------------------------------------------
// Note the second parameter on both SetScissor typedefs: the hidden return
// slot. See the file header.
typedef void* (*SetScissorVpFn)(void* self, void* ret, const void* viewport);
typedef void* (*SetScissorIntsFn)(void* self, void* ret, int x, int y, int w, int h);
typedef void  (*AddLineFn)(void* self, const void* sprite,
                           int x1, int y1, int x2, int y2, uint32_t color);

// ---- the internals reached by address ---------------------------------------
// `Flush` drains `_draws` into the vertex buffer and clears `_currentSheet`.
// One argument, `this` in RCX (0x18011ba5a: `MOV R13,RCX`, and every call site
// in the class sets only RCX).
typedef void (*FlushFn)(void* self);

// The shipped AddSpriteDraw / FlushDeferred, for the two internal oracles
// only — reached raw, never through ResolveAddr. See the oracle blocks in the
// bodies for why the distinction is load-bearing.
typedef void (*AddSpriteDrawFn)(void* self, const void* draw);
typedef void (*FlushDeferredFn)(void* self);

// UI::SpriteCapture, reached only when `_spriteCapture` is non-null — which it
// is not unless a screenshot/capture UI is active. Argument orders read off
// the two call sites, NOT guessed from the callee names:
//   AddScissor  0x1800d8ef0  <- 0x18011d126..0x18011d146
//   AddSprite   0x1800d8b20  <- 0x18011c8e4..0x18011c916
typedef void (*CaptureAddScissorFn)(void* capture, int x, int y,
                                    int right, int bottom, bool fullScreen);
typedef void (*CaptureAddSpriteFn)(void* capture, const void* sprite,
                                   int x1, int y1, int x2, int y2,
                                   float rotation, uint32_t color);

// ---- the five resolvers ----------------------------------------------------
// Each one spells its address as a LITERAL inside `ResolveAddr(Module::Runtime,
// …)`, once, and that spelling is not a style choice — it is what keeps these
// doors VISIBLE to `tools/addr_door_audit.py`, whose call-site half matches a
// literal and nothing else.
//
// This file first routed all five through one `RtAddr(slot, addr)` template
// helper. It was shorter, it compiled, and it made every door in the file
// INVISIBLE to the audit: base and branch both reported `UNREGISTERED 16`
// while the doors sat in the `non-literal call sites 77 -> 80` line nobody
// reads. That is the fifth instance of the blind spot in
// reference_literal_grep_blind_to_variables.md — an instrument that greps for
// a literal cannot see the same fact spelled through a variable, and it never
// announces the blindness. Rewritten so the cost this lane adds shows up in
// the number the wave is steering by.
//
// Each caches, and NONE caches a null (rt_port.h's contract): a miss during
// early boot must not freeze the answer for the process.

// The batch drain. NOT PORTED — this is the lane's one per-frame address door.
inline FlushFn TheFlush() {
    static FlushFn f = nullptr;
    if (!f) f = reinterpret_cast<FlushFn>(ResolveAddr(Module::Runtime, 0x18011ba40));
    return f;
}
// UI::SpriteCapture — both reached only when `_spriteCapture` is non-null.
inline CaptureAddScissorFn TheCaptureAddScissor() {
    static CaptureAddScissorFn f = nullptr;
    if (!f) f = reinterpret_cast<CaptureAddScissorFn>(ResolveAddr(Module::Runtime, 0x1800d8ef0));
    return f;
}
inline CaptureAddSpriteFn TheCaptureAddSprite() {
    static CaptureAddSpriteFn f = nullptr;
    if (!f) f = reinterpret_cast<CaptureAddSpriteFn>(ResolveAddr(Module::Runtime, 0x1800d8b20));
    return f;
}
// ENDGAME B.3.2 step 2 — the `TheArrayAppend`/`TheArrayClear` forwarders that
// used to sit here (0x180078520 / 0x1800783d0) are GONE: `_draws`/`_deferred`
// are real `Collection::Array<SpriteDraw>` members and both call sites below
// bridge through `kSpec_SpriteDraw` directly.

inline void Flush(SpriteContext* self) { TheFlush()(self); }

// ---- the two float helpers, kept honest ------------------------------------
// Retail converts int -> float (CVTDQ2PS), divides or multiplies in SINGLE
// precision (DIVSS / MULSS), and converts back with CVTTSS2SI, which truncates
// TOWARD ZERO. `static_cast<int>` on a float does exactly that; the `float`
// casts are what keep the arithmetic out of double precision.
inline int ScaleDown(int v, float scale) {
    return static_cast<int>(static_cast<float>(v) / scale);
}
inline int ScaleUp(int v, float scale) {
    return static_cast<int>(static_cast<float>(v) * scale);
}

}  // namespace

// ============================================================================
// The ports
// ============================================================================
// Every body is byte-faithful to the retail DISASSEMBLY, not to Ghidra's
// decompiled C (reference_ghidra_c_output_vs_disassembly.md). No null guard
// retail does not have.

// ?Begin@SpriteContext@UI@@QEAAXMPEAVSpriteCapture@2@PEAVViewport@Video@@@Z
// Retail 0x18011d2b0, ord 1632. UI leftovers batch
// (docs/runtime_port/closure/WORKLIST.md). Read the scissor from the context's
// CURRENT render target (this is the same read
// runtime/src/save/saveload_draw.cpp's own SpriteContext construction already
// makes, through `RtVideo::RenderBufferPrefix`); store the capture object and
// the clip viewport; if a capture object was handed in, clear its `_entries`
// and `_sprites` arrays to size 0 (0x18004ba90 / 0x1800370c0, array.cpp's
// kSpec_CaptureEntryZ / kSpec_SpriteIndexZ — both zero SpriteCapture+0x50 as a
// side effect of this same call site, per `SpriteCapture::BeginAddingSprites`'
// own +0x50 clear noted in SpriteCapture.h); push an identity Matrix through
// `Video::Context::SetTransform`; and finally store `_scale`.
void RtUi_SpriteContext_Begin(UI::SpriteContext* self, float scale,
                              void* spriteCapture, const UI::ClipRect* viewport) {
    RtVideo::Context* ctx = static_cast<RtVideo::Context*>(self->_context);
    const auto* rb = static_cast<const RtVideo::RenderBufferPrefix*>(ctx->_renderBuffer);
    const int32_t w = rb->width;
    const int32_t h = rb->height;

    self->_spriteCapture = spriteCapture;
    self->_clipRegion = viewport;

    if (spriteCapture) {
        uint8_t* cap = static_cast<uint8_t*>(spriteCapture);
        *reinterpret_cast<uint64_t*>(cap + 0x50) = 0;

        // ENDGAME B.3.2 step 2 — both forwarders are GONE. `spriteCapture` is
        // untyped here (only Sprite.h is included, not SpriteCapture.h), and
        // both rows are NON-TRIVIAL (kSpec_SpriteIndexZ's tail-init sets each
        // new Sprite's index to -1; kSpec_CaptureEntryZ's destroy releases a
        // String), so the bridge stays byte-offset rather than retyping.
        rtcoll::AsArray<&rtcoll::kSpec_SpriteIndexZ>(cap + 0x40)->SetSize(0);   // 0x1800370c0
        rtcoll::AsArray<&rtcoll::kSpec_CaptureEntryZ>(cap + 0x30)->SetSize(0); // 0x18004ba90
    }

    self->_currentSheet = nullptr;
    self->_scissorX = 0;
    self->_scissorY = 0;
    self->_scissorW = w;
    self->_scissorH = h;

    // Was `Matrix identity; memset(&identity, 0, sizeof(identity));`. ENDGAME
    // B.3.1 made Math::Matrix a real class whose default ctor (retail
    // 0x18001d770) zeroes all sixteen cells, so the declaration alone is the
    // memset, and -Wclass-memaccess is not tripped.
    Matrix identity;
    identity.m[0] = 1.0f; identity.m[5] = 1.0f; identity.m[10] = 1.0f; identity.m[15] = 1.0f;
    using SetTransformFn = void (*)(RtVideo::Context*, int32_t, const Matrix*);
    RT_EXPORT_ONCE(SetTransformFn, "?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z")(
        ctx, 0, &identity);

    self->_scale = scale;
}

// ---- 0x18011c890  SpriteContext::AddSpriteDraw (internal) ------------------
//
// Two independent halves, and retail runs them in this order:
//   (a) if a capture is active AND the record lies wholly inside the clip
//       region, hand a copy to the capture;
//   (b) if the record is axis-aligned, cull it against the scissor rect
//       scaled by `_scale`; whatever survives is appended to `_draws`.
//
// Half (b)'s guard is `rotation == 0.0f`, and it is written as an equality
// test on purpose. Retail's `UCOMISS / JP / JNZ` (0x18011c928) branches to the
// APPEND on unordered as well as on not-equal, so a NaN rotation skips the
// cull — and `rot == 0.0f` is false for NaN in C++ too, which reproduces that
// without a special case.
void RtUi_SpriteContext_AddSpriteDraw(void* selfRaw, const void* drawRaw) {
    auto* self = static_cast<SpriteContext*>(selfRaw);
    const auto* d = static_cast<const SpriteDraw*>(drawRaw);

    // --- (a) the capture branch, 0x18011c89a..0x18011c916 -------------------
    void* capture = self->_spriteCapture;
    if (capture) {
        const ClipRect* clip = self->_clipRegion;
        if (d->x1 >= clip->x && d->x2 <= clip->x + clip->w &&
            d->y1 >= clip->y && d->y2 <= clip->y + clip->h) {
            // Retail rebuilds a bare {sheet, index} UI::Sprite on its own stack
            // (0x18011c8f1 / 0x18011c8fc) rather than passing the record.
            Sprite s;
            s.spriteSheet = d->sheet;
            s.spriteIndex = d->spriteIndex;
            TheCaptureAddSprite()(capture, &s, d->x1, d->y1,
                                             d->x2, d->y2, d->rotation, d->color);
        }
    }

    // --- (b) the scissor cull, 0x18011c920..0x18011c9a1 ---------------------
    bool append = true;
    if (d->rotation == 0.0f) {
        const float sc = self->_scale;
        const int   sx = self->_scissorX;
        const int   sy = self->_scissorY;
        if      (static_cast<float>(sx) * sc > static_cast<float>(d->x2))              append = false;
        else if (static_cast<float>(d->x1) > static_cast<float>(sx + self->_scissorW) * sc) append = false;
        else if (static_cast<float>(sy) * sc > static_cast<float>(d->y2))              append = false;
        else if (static_cast<float>(d->y1) > static_cast<float>(sy + self->_scissorH) * sc) append = false;
    }

    if (append) {
        // retail 0x180078520 — kSpec_SpriteDraw, NON-TRIVIAL (a partial-write
        // assign), so `_draws` (a real `Collection::Array<SpriteDraw>` member)
        // is bridged through the hand-derived row rather than its own
        // trait-derived member functions.
        rtcoll::AsArray<&rtcoll::kSpec_SpriteDraw>(&self->_draws)
            ->SetSizeFill(self->_draws._size + 1, d);
        return;
    }

    // --- the oracle, on the cull decision only ------------------------------
    // Safe in BOTH outcomes (file header): we appended nothing, so if the
    // shipped body appends, the batch gains the record retail wanted. Skipped
    // when a capture is active, because then the shipped body would push a
    // SECOND copy onto the capture, which nothing undoes.
    //
    // AddSpriteDraw has NO exported name, so the shipped body is reached by
    // RAW ADDRESS. It MUST be the raw door and not `ResolveAddr`: this very
    // function is registered at 0x18011c890, so `ResolveAddr` would hand back
    // OUR OWN body and the comparison would be true by construction — the
    // exact shape reference_recheck_must_match_its_own_proof.md records.
    //
    // The verdict is `_draws._size`, because "did the shipped body append"
    // is the whole of what this branch decides.
}

// ---- 0x18011c6a0  SpriteContext::FlushDeferred (internal) ------------------
//
// Replays the deferred batch through AddSpriteDraw, re-binding `_currentSheet`
// (and flushing) whenever a record's sheet differs from the cursor, then
// flushes once more and clears the deferred array.
//
// Both `_deferred._data` and `_deferred._size` are RE-READ every iteration
// (0x18011c6d0 and 0x18011c70f), not hoisted. That is kept faithful rather
// than tidied: AddSpriteDraw can reallocate `_draws`, and while it does not
// touch `_deferred` today, hoisting would bake that in as an assumption.
void RtUi_SpriteContext_FlushDeferred(void* selfRaw) {
    auto* self = static_cast<SpriteContext*>(selfRaw);

    const bool deferring = self->_deferring != 0;
    if (!deferring) {
        // Retail's `CMP byte [RCX+0x38],0 / JZ epilogue` (0x18011c6a6) makes
        // this branch a pure no-op, so a replay of the shipped body is exact.
        // Reached RAW for the same reason as AddSpriteDraw's oracle: this
        // function is registered at 0x18011c6a0, and `ResolveAddr` would
        // return our own body and compare it against itself.
        //
        // The verdict is the 0x10-byte `_deferred` header plus the sheet
        // cursor — everything this function can touch. Nothing outside that is
        // compared, because comparing bytes neither side writes is how an
        // oracle reports 100% divergence on unrelated noise
        // (reference_oracle_must_not_compare_unwritten_bytes.md).
        return;
    }

    for (int i = 0; i < self->_deferred.GetSize(); ++i) {
        SpriteDraw* rec   = &self->_deferred.GetData()[i];
        void*       sheet = rec->sheet;
        if (self->_currentSheet != sheet) {
            if (self->_currentSheet != nullptr) Flush(self);
            self->_currentSheet = sheet;
        }
        RtUi_SpriteContext_AddSpriteDraw(self, rec);
    }

    Flush(self);
    // retail 0x1800783d0 — kSpec_SpriteDraw's SetSize(0) form; same
    // NON-TRIVIAL row as the append above, bridged the same way.
    rtcoll::AsArray<&rtcoll::kSpec_SpriteDraw>(&self->_deferred)->SetSize(0);
}

// ---- 0x18011cbe0  ?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z -----
//
// Queues one line quad. The sheet cursor is rebound (flushing the previous
// batch first, unless there was none) whenever the incoming sprite's sheet
// differs, which is what groups draws into per-sheet batches.
void RtUi_SpriteContext_AddLine(void* selfRaw, const void* spriteRaw,
                                int x1, int y1, int x2, int y2, uint32_t color) {
    auto* self   = static_cast<SpriteContext*>(selfRaw);
    const auto* sprite = static_cast<const Sprite*>(spriteRaw);

    // 0x18011cc09 / 0x18011cc0e: two independent early returns. Note the index
    // test is `< 0`, not `== -1` — `JL`, a SIGNED branch.
    void* sheet = sprite->spriteSheet;
    const bool queue = (sheet != nullptr) && (sprite->spriteIndex >= 0);

    if (!queue) {
        // Retail returns before its first store, so the replay is exact.
        return;
    }

    if (self->_currentSheet != sheet) {
        if (self->_currentSheet != nullptr) Flush(self);
        self->_currentSheet = sheet;
    }

    // The record retail builds at RSP+0x20 (0x18011cc31..0x18011cc7c).
    //
    // ONE DELIBERATE DIVERGENCE, and it is the only one in this file: retail
    // leaves +0x25..+0x27 as whatever was on its stack, because the record is
    // an uninitialised local it fills field by field. `SpriteDraw rec{}` zeroes
    // them. Nothing reads those three bytes — Flush's record walk reads +0x00,
    // +0x08, +0x0c, +0x10, +0x14, +0x18, +0x1c, +0x20 and the +0x24 BYTE and
    // nothing else, and Array<SpriteDraw>'s assign arm copies them without
    // interpreting them — so the difference is unobservable, and reading
    // indeterminate bytes would be UB the compiler is entitled to exploit.
    SpriteDraw rec{};
    rec.sheet       = sheet;
    rec.spriteIndex = sprite->spriteIndex;
    rec.x1          = x1;
    rec.y1          = y1;
    rec.x2          = x2;
    rec.y2          = y2;
    rec.rotation    = 0.0f;   // 0x18011cc47, XORPS then MOVSS
    rec.color       = color;
    rec.isLine      = 1;      // 0x18011cc77, the literal 1

    RtUi_SpriteContext_AddSpriteDraw(self, &rec);
}

// ---- 0x18011d050  ?SetScissor@…@QEAA?AVViewport@Video@@AEBV34@@Z -----------
//
// Sets the scissor rect and RETURNS THE PREVIOUS ONE, so callers can restore
// it. The incoming rect is in point space and is divided by `_scale` to reach
// the device space the four fields hold; the returned rect is multiplied back
// up — EXCEPT on the unchanged branch. See the note there.
void* RtUi_SpriteContext_SetScissor_Viewport(void* selfRaw, void* retRaw,
                                             const void* viewportRaw) {
    auto* self = static_cast<SpriteContext*>(selfRaw);
    auto* ret  = static_cast<ClipRect*>(retRaw);
    const auto* v = static_cast<const ClipRect*>(viewportRaw);

    const float scale = self->_scale;
    const int nx = ScaleDown(v->x, scale);
    const int ny = ScaleDown(v->y, scale);
    const int nw = ScaleDown(v->w, scale);
    const int nh = ScaleDown(v->h, scale);

    const bool changed = !(nx == self->_scissorX && ny == self->_scissorY &&
                           nw == self->_scissorW && nh == self->_scissorH);

    if (!changed) {
        // 0x18011d0d8..0x18011d0ef. *** RETAIL RETURNS THE RAW FIELDS HERE,
        // NOT `field * scale`. *** The other exit multiplies by `_scale`; this
        // one copies the device-space ints straight out. With `_scale != 1`
        // the two exits therefore return the same rect in DIFFERENT SPACES.
        //
        // That is retail's behaviour, it is almost certainly a bug in retail,
        // and it is reproduced rather than corrected: a caller that saves the
        // result and restores it later feeds it back through this same
        // function, and "fixing" the asymmetry would change what the restore
        // computes (CLAUDE.md — a faithful port does not repair retail).
        ret->x = self->_scissorX;
        ret->y = self->_scissorY;
        ret->w = self->_scissorW;
        ret->h = self->_scissorH;

        // The oracle. Nothing above this line mutated anything, and the
        // shipped body on this branch does not either, so the replay is exact
        // — and it is what proves the hidden-return-slot arity, because it
        // drives the DLL through the typedef the file header argues for.
        return ret;
    }

    Flush(self);
    RtUi_SpriteContext_FlushDeferred(self);

    // 0x18011d101..0x18011d146. `_context` -> +0x28 is the bound render target;
    // its +0x10/+0x14 are the target's width and height, and the flag says the
    // new scissor covers the whole target.
    void* capture = self->_spriteCapture;
    if (capture) {
        auto* target = *reinterpret_cast<uint8_t**>(
            reinterpret_cast<uint8_t*>(self->_context) + 0x28);
        const int tw = *reinterpret_cast<const int32_t*>(target + 0x10);
        const int th = *reinterpret_cast<const int32_t*>(target + 0x14);
        const bool fullScreen = (nw == tw && nh == th);
        TheCaptureAddScissor()(capture, v->x, v->y,
                                          v->w + v->x, v->h + v->y, fullScreen);
    }

    // 0x18011d14b..0x18011d1b4. The OLD values are read before the new ones
    // are stored; the return is the old rect scaled back up to point space.
    const int ox = self->_scissorX;
    const int oy = self->_scissorY;
    const int ow = self->_scissorW;
    const int oh = self->_scissorH;

    self->_scissorX = nx;
    self->_scissorY = ny;
    self->_scissorW = nw;
    self->_scissorH = nh;

    ret->x = ScaleUp(ox, scale);
    ret->y = ScaleUp(oy, scale);
    ret->w = ScaleUp(ow, scale);
    ret->h = ScaleUp(oh, scale);
    return ret;
}

// ---- 0x18011d010  ?SetScissor@…@QEAA?AVViewport@Video@@HHHH@Z --------------
//
// A forwarder: packs the four ints into a Viewport on its own stack and calls
// the overload above with the SAME return slot, then returns that slot (RBX,
// saved from RDX on entry) rather than the callee's RAX — the same value.
//
// The field order of the packed rect is read off the four stores at
// 0x18011d01a..0x18011d033, which are emitted OUT OF ORDER (+0x20, +0x28,
// +0x2c, +0x24) and are easy to misread from the decompiler: R8D -> +0x00,
// R9D -> +0x04, arg5 -> +0x08, arg6 -> +0x0c.
void* RtUi_SpriteContext_SetScissor_Ints(void* selfRaw, void* retRaw,
                                         int x, int y, int w, int h) {
    auto* self = static_cast<SpriteContext*>(selfRaw);

    ClipRect v;
    v.x = x;
    v.y = y;
    v.w = w;
    v.h = h;

    RtUi_SpriteContext_SetScissor_Viewport(self, retRaw, &v);

    return retRaw;
}

// ============================================================================
int RtUiSpriteContextRegister() {
    int n = 0;

    RegisterRuntimeExport("?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z",
                          (void*)&RtUi_SpriteContext_AddLine); ++n;
    RegisterRuntimeExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z",
                          (void*)&RtUi_SpriteContext_SetScissor_Viewport); ++n;
    RegisterRuntimeExport("?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z",
                          (void*)&RtUi_SpriteContext_SetScissor_Ints); ++n;

    // The two internals. No exported name in either build — checked against the
    // profile DLL's 5,182-name export directory — so these are addresses.
    //
    // *** REGISTERING THESE CHANGES BEHAVIOUR AT EXISTING CALL SITES. ***
    // `ResolveAddr(Module::Runtime, …)` consults the port registry first, so
    // from here on `UI::UserInterface::Draw` (0x180121890) — whether it is the
    // DLL's or a future port — reaches OUR AddSpriteDraw for every dialog it
    // submits. That is intended, and it is also why this lane's oracle had to
    // be safe on a live frame rather than only in a scratch.
    RegisterRuntimeAddr(0x18011c890, (void*)&RtUi_SpriteContext_AddSpriteDraw); ++n;
    RegisterRuntimeAddr(0x18011c6a0, (void*)&RtUi_SpriteContext_FlushDeferred); ++n;

    // UI leftovers batch (docs/runtime_port/closure/WORKLIST.md). No exported
    // name in either build for Begin either -- address door, same as the two
    // internals above.
    RegisterRuntimeAddr(0x18011d2b0, (void*)&RtUi_SpriteContext_Begin); ++n;

    // WAVE 9 LANE FLUSH. `Flush` (0x18011ba40) was this file's ONE opened
    // address door -- the per-frame call every body above makes and the reason
    // ~11,400 of their live executions carried no differential evidence. It is
    // ported in runtime/src/ui/spritecontext_flush.cpp and registered from
    // there. After this line `TheFlush()` above resolves to OUR body, so the
    // door is closed rather than merely documented.
    extern int RtUiSpriteContextFlushRegister();
    n += RtUiSpriteContextFlushRegister();

    return n;
}
