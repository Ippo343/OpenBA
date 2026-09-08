// ============================================================================
// runtime/src/graphics/wave9_lane_gfxd.cpp — WAVE 9 LANE GFXD
// The bottom half of the Graphics::Renderer class lane.
// ============================================================================
//
// WHAT THIS LANE IS, AND WHY IT IS SHAPED THIS WAY
//
// `?Draw@Renderer@Graphics@@` was declined by lanes GFX, F, GFX2 and GFXR. Lane
// GFXR's correction is the reason this file exists: nobody had run `Draw`'s five
// callees through the DLL's EXPORT TABLE, and four of the five turn out to be
// ordinary named exports (`Renderer::Reset`, `Renderer::AddScene`,
// `Renderer::BeginDrawGroup`, `Scene::AddCulledDrawItems`). Only `0x180062d60`
// is an unexported internal. So `Draw` is one unnamed body behind four portable
// exports — a specifiable class lane rather than a wall.
//
// This lane takes the two of those four whose closures are finite and readable,
// and declines the other two with measured blockers (WAVE9_LANE_GFXD.md §4).
//
// *** AND IT REPEATS LANE GFXR'S LESSON ON ITS OWN CALLEES, WHICH IS THE POINT
// OF THE LESSON. *** Every address below was run through the export table
// before it was written down. Three that a source-tree grep would have recorded
// as "unnamed internals" are exports:
//
//     0x180017a70   ??0Camera@Graphics@@QEAA@AEBV01@@Z   (copy constructor)
//     0x1800fe7f0   ??0Camera@Graphics@@QEAA@XZ          (default constructor)
//     0x180017c40   ??4Camera@Graphics@@QEAAAEAV01@AEBV01@@Z  (operator=)
//
// A FOURTH CORRECTION FALLS OUT OF THE SAME LOOKUP, and it is a live one:
// renderer.cpp's registration block is headed "Graphics::Camera (all five of
// the class's exports)". **`Graphics::Camera` has THIRTEEN exports**, listed in
// WAVE9_LANE_GFXD.md §2. Five of them were ported by lane F; two more are
// ported here; six remain. Nothing depended on the wrong count, but a lane
// reading that comment would have believed the class was closed.
//
// ---------------------------------------------------------------------------
// A FACT NEITHER RECORD CARRIED: `Renderer::Reset`'s own closure
// ---------------------------------------------------------------------------
// GFXR's table says `Draw` is blocked by ONE unnamed body. That is true of
// `Draw`'s own call list and it understates the lane: `Reset` — one of the four
// "portable exports" — is itself three unexported bodies deep, and none of the
// three was in the tree:
//
//     0x1800626a0   clear a DrawGroup's three per-pass item arrays  [NOT EXPORTED]
//     0x180062ae0   Array<Renderer::SceneEntry>::SetSize(0)         [NOT EXPORTED]
//     0x180062c40   Array<Renderer::Drawable>::SetSize(0)           [NOT EXPORTED]
//
// and `AddScene` two more (`0x1800637b0`, `0x18001bdc0`), which drag in
// `0x180062860` and `0x18001ce90`. "Portable" was the right word — all seven are
// small and all seven are ported below — but "one unnamed body" is the count for
// `Draw`, not for the lane. Recorded so the next lane sizes `BeginDrawGroup` and
// `AddCulledDrawItems` from their own closures rather than from GFXR's table.
//
// ---------------------------------------------------------------------------
// THE ARRAY BODIES ARE LANE Q'S `SetSizeCore`, NOT A SECOND COPY OF IT
// ---------------------------------------------------------------------------
// Five of the seven internals are `Collection::Array<T>` instantiations, and
// every one of them matched `rtcoll::SetSizeCore` instruction for instruction:
// the `newSize == _size` early return, the `newSize > _capacity` grow with the
// `* 3` bump on a by-one append from non-empty, the `realloced ? 0 : oldSize`
// loop start, the realloc-destroy arm and the shrink-tail arm. So they are
// registered as new `ArraySpec` rows against lane Q's body.
//
// They are declared HERE rather than in array.cpp, which is where lane GFX3 and
// lane UIF put theirs. That precedent's stated reason is "a private second copy
// of SetSizeCore in the graphics tree is exactly the drift this parameterisation
// exists to prevent" — and that reason is satisfied: this file calls
// `rtcoll::SetSizeCore` and writes no loop of its own. What it avoids is a
// third concurrent lane editing array.cpp in the same wave. The cost is that
// array_internal.h's "Only two files include this" is now stale by one; that is
// recorded in WAVE9_LANE_GFXD.md §7 rather than fixed under a neighbour.
//
// ---------------------------------------------------------------------------
// UNWRITTEN BYTES — three separate holes, all real, none of them modelling laziness
// ---------------------------------------------------------------------------
// `reference_oracle_must_not_compare_unwritten_bytes.md`. Each arm below leaves
// bytes alone that retail also leaves alone, and the probe excludes exactly
// those and no others:
//
//   Camera copy ctor  (0x180017a70): copies 53 dwords (+0x00..+0xd3) and the
//                     BYTE at +0xd4. **+0xd5..+0xd7 is never written.**
//   Camera default    (0x1800fe7f0): writes +0x00..+0x60 and +0x64..+0xd4.
//                     **+0x61..+0x63 and +0xd5..+0xd7 are never written.**
//   SceneEntry assign (0x18001bdc0): the Camera hole lands at element +0xe5..
//                     +0xe7, and **+0xfc..+0xff is never written** (retail
//                     copies five separate dwords at 0xe8/ec/f0/f4/f8 and skips
//                     0xfc).
//   SceneEntry init   (0x180062860): the two Camera holes at element +0x71..
//                     +0x73 and +0xe5..+0xe7, plus **+0xfc..+0xff**.
//
// `ArraySpec` carries ONE assign window and ONE init window, and kSpec_SceneEntry
// needs two and three. That limit is already filed against kSpec_Sprite38 and
// kSpec_SpriteDraw. Rather than widen a shared struct under another lane, this
// file sets both windows to "none" and its OWN probe carries the real hole
// lists — which is sound because the only consumer of those two fields is a
// probe, and this spec's probe is here. Stated as an honest limit rather than
// left to be discovered: **if a future caller drives kSpec_SceneEntry through
// array_probe.cpp, that probe will compare bytes neither side writes and report
// a divergence that is not one.** WAVE9_LANE_GFXD.md §6 limit 3.
//
// ---------------------------------------------------------------------------
// WHAT THIS LANE DOES *NOT* CLAIM
// ---------------------------------------------------------------------------
// Nothing in our tree calls `Renderer::Reset` or `Renderer::AddScene` today —
// the only live door on this class is `?Draw@Renderer@Graphics@@`
// (code/Game/Game/gameupdateloop.cpp:627 and engine/funcs/engine_funcs.cpp:3599),
// and `Draw` is still the DLL's, so the DLL's own internal `CALL 0x180105110`
// reaches the DLL's `Reset`, not ours. These registrations are therefore INERT
// today and their `calls=` is 0 by construction, not by accident
// (`reference_uncalled_port_is_not_verified.md`). The oracle is the probe at the
// bottom of this file, and the probe is the ONLY thing that executes these
// bodies before cutover. That is exactly lane X's situation and it is stated the
// same way.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../funcs/rt_diff.h"
#include "../../funcs/rt_port.h"
#include "../../types/graphics/Camera.h"
#include "../collection/array_destroy_internal.h"
#include "../collection/array_internal.h"
#include "wave9_lane_gfxd.h"

using RtGraphics::Camera;
using RtGraphics::Renderer;

// Byte-identical to standalone/resolve.h's declaration; the linker checks the
// two against each other. Not included, for the <wingdi.h> `Rectangle` reason
// rt_port.h's banner gives.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtgfxd {

using rtcoll::ArrayHdr;
using rtcoll::ArraySpec;
using rtcoll::Site;

// ============================================================================
// The two DLL internals this lane CALLS rather than ports.
//
// Both spell their address as a LITERAL inside ResolveAddr, and must keep doing
// so: tools/runtime_census.py scans for that literal at the call site, and
// hiding an edge behind a one-line helper is how an edge goes missing from the
// census (reference_census_cannot_see_the_macro_doors.md).
//
// Both are ALREADY OURS — ResolveAddr consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md), and lane Q registered
// both — so neither is a cutover residue row. Neither caches a null: a miss is
// retried, per the contract in rt_resolve_cache.cpp.
// ============================================================================

// Collection::Array<int32>::operator= — retail 0x18001cdf0, lane Q's
// RtColl_I32ArrayAssign_18001cdf0. The tail call of SceneEntry's copy-assign.
void RtArrayAssignI32(void* dst, const void* src) {
    using Fn = void (*)(void*, const void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001cdf0));
    fn(dst, src);
}

// Collection::Array<int32>::~Array — retail 0x18001c750, lane Q's
// RtColl_ArrayDtor_18001c750. AddScene's last act, on its stack record.
void RtArrayDtorI32(void* arrayTriple) {
    using Fn = void* (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18001c750));
    fn(arrayTriple);
}

// ============================================================================
// Graphics::Camera — the two constructors, both EXPORTS.
// ============================================================================

// ??0Camera@Graphics@@QEAA@AEBV01@@Z — retail 0x180017a70. Returns `this`
// (`MOV RAX,RCX` at 0x180017c37).
//
// FIFTY-THREE dword copies covering +0x00..+0xd3, then ONE byte at +0xd4
// (`MOVZX EAX,byte ptr [RDX+0xd4]` / `MOV byte ptr [RCX+0xd4],AL`). sizeof is
// 0xd8, so +0xd5..+0xd7 is a hole retail does not touch — see the banner.
Camera* RtGraphics_Camera_CopyCtor(Camera* self, const Camera* src) {
    uint32_t*       d = reinterpret_cast<uint32_t*>(self);
    const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
    for (int i = 0; i < 0xd4 / 4; ++i) d[i] = s[i];        // 53 dwords
    reinterpret_cast<uint8_t*>(self)[0xd4] =
        reinterpret_cast<const uint8_t*>(src)[0xd4];
    return self;
}

// ??0Camera@Graphics@@QEAA@XZ — retail 0x1800fe7f0. Returns `this`.
//
// Both float constants were LIFTED AS RAW BITS out of the shipped .rdata rather
// than retyped as decimals (reference_crt_coefficients_must_be_lifted_not_
// retyped.md): 0x18016175c is 0xbf800000 and 0x18016169c is 0x3f800000. Both
// happen to be exactly representable, so the two spellings agree here — the
// discipline is kept anyway, because "it looked exact" is not a check.
//
// INSTRUCTION ORDER, and where this file departs from it deliberately: retail
// zeroes the first 0x60 bytes as twelve qwords in the order 0x00, 0x10, 0x08,
// 0x18, 0x20 ... and then overwrites every one of those bytes with the 24
// MOVSS stores below. The reorder to plain ascending order is safe *for this
// specific pass only*, because all twelve stores write the SAME constant (zero)
// to DISJOINT addresses; no float arithmetic is reassociated anywhere in this
// function, so the "moved term" hazard does not arise
// (reference_moved_term_hides_where_terms_cancel.md).
Camera* RtGraphics_Camera_DefaultCtor(Camera* self) {
    uint8_t* const p = reinterpret_cast<uint8_t*>(self);

    float kOne, kNegOne, kZero;
    const uint32_t bOne = 0x3f800000u, bNegOne = 0xbf800000u, bZero = 0x00000000u;
    std::memcpy(&kOne, &bOne, 4);
    std::memcpy(&kNegOne, &bNegOne, 4);
    std::memcpy(&kZero, &bZero, 4);

    for (int i = 0; i < 12; ++i)                       // +0x00..+0x5f
        std::memset(p + i * 8, 0, 8);
    p[0x60] = 1;                                       // MOV byte [RCX+0x60],1

    // Volume::Frustum's 24 floats, in retail's store order and with retail's
    // exact constants. XMM2 is XORPS'd (+0.0f), XMM1 is 1.0f, XMM0 is -1.0f.
    static const signed char kFrustum[24] = {
        //  0     1     2     3     4     5     6     7
        //  0x00  0x04  0x08  0x0c  0x10  0x14  0x18  0x1c
             0,    1,    0,    0,    0,   -1,    0,   -1,
        //  0x20  0x24  0x28  0x2c  0x30  0x34  0x38  0x3c
             1,    0,    0,   -1,   -1,    0,    0,   -1,
        //  0x40  0x44  0x48  0x4c  0x50  0x54  0x58  0x5c
             0,    0,   -1,   -1,    0,    0,    1,   -1 };
    for (int i = 0; i < 24; ++i) {
        const float v = (kFrustum[i] == 0) ? kZero : (kFrustum[i] > 0 ? kOne : kNegOne);
        std::memcpy(p + i * 4, &v, 4);
    }

    for (int i = 0; i < 6; ++i)                        // Transform3, +0x64..+0x93
        std::memset(p + 0x64 + i * 8, 0, 8);

    for (int i = 0; i < 16; ++i)                       // Matrix, +0x94..+0xd3
        std::memcpy(p + 0x94 + i * 4, &kZero, 4);

    p[0xd4] = 0;                                       // MOV byte [RCX+0xd4],DL
    return self;
}

// ============================================================================
// Renderer::SceneEntry — the element operations of the 0x110-stride array.
// Both are unexported COMDATs; both are reached only through kSpec_SceneEntry.
// ============================================================================

// Retail 0x18001bdc0 — copy-assign. Returns `dst` (`MOV RAX,RDI`).
//
// The zero-then-assign of the inner Array at +0x100 is retail's, not a
// defensive addition: 0x18001be3c..0x18001be41 stores two zero qwords over the
// triple and only then calls the array's operator=, which is what makes the
// assignment safe against a destination holding a foreign pointer.
uint8_t* SceneEntryAssign(uint8_t* dst, const uint8_t* src) {
    std::memcpy(dst + 0x00, src + 0x00, 8);
    std::memcpy(dst + 0x08, src + 0x08, 8);
    RtGraphics_Camera_CopyCtor(reinterpret_cast<Camera*>(dst + 0x10),
                               reinterpret_cast<const Camera*>(src + 0x10));
    for (uint32_t off : {0xe8u, 0xecu, 0xf0u, 0xf4u, 0xf8u})   // +0xfc SKIPPED
        std::memcpy(dst + off, src + off, 4);
    std::memset(dst + 0x100, 0, 8);
    std::memset(dst + 0x108, 0, 8);
    RtArrayAssignI32(dst + 0x100, src + 0x100);
    return dst;
}

// Retail 0x180062860 — default construct. Returns `dst` (`MOV RAX,RBX`).
// +0xf8 is 0x40 and +0xf4 is -1; +0xfc is not written.
uint8_t* SceneEntryInit(uint8_t* dst) {
    std::memset(dst + 0x00, 0, 8);
    std::memset(dst + 0x08, 0, 8);
    RtGraphics_Camera_DefaultCtor(reinterpret_cast<Camera*>(dst + 0x10));
    std::memset(dst + 0xe8, 0, 8);                     // MOV qword [RBX+0xe8],RDI
    const uint32_t zero = 0u, neg1 = 0xffffffffu, x40 = 0x40u;
    std::memcpy(dst + 0xf0, &zero, 4);
    std::memcpy(dst + 0xf4, &neg1, 4);
    std::memcpy(dst + 0xf8, &x40, 4);                  // +0xfc SKIPPED
    std::memset(dst + 0x100, 0, 8);
    std::memset(dst + 0x108, 0, 8);
    return dst;
}

// Retail 0x18001ce90 — destroy a RANGE of SceneEntries: free each element's
// inner Array buffer at +0x100 and clear its 16-byte header, unconditionally.
// That is exactly lane DESTROY's BODY A with kD_InnerArr_110, which is the spec
// row already written for THIS address (array_destroy.cpp:255). Registered
// below, which lane DESTROY did not do — it registered only 0x18001c7e0, the
// `~Array` that CALLS this.
void SceneEntryDestroyRange(void* /*ignoredThis*/, uint8_t* base, int count) {
    rtcoll::FreeInnerArraysCore(base, count, &rtcoll::kD_InnerArr_110);
}

// `ArraySpec`'s ElemAssign/ElemInit are VOID-returning, and retail's two
// bodies both return `this` in RAX (0x18001be52 `MOV RAX,RDI`, 0x1800628bc
// `MOV RAX,RBX`). The REGISTERED entry points must keep that return, because a
// by-address caller reads it; the spec gets these two thin adapters instead.
// Bending the port's signature to fit the table would have been the wrong way
// round -- an fn_* typedef that drops part of the ABI is the
// reference_hidden_retslot_wild_write.md shape.
void SceneEntryAssignSpec(uint8_t* dst, const uint8_t* src) { SceneEntryAssign(dst, src); }
void SceneEntryInitSpec(uint8_t* dst) { SceneEntryInit(dst); }

// ---- element ops of the two trivially-destructible arrays ------------------

// Renderer::DrawItem, stride 0x38. The ASSIGN is retail's seven-qword unrolled
// copy at 0x180062762 (a pure byte copy of the whole element). The INIT arm at
// 0x1800627a1 is NOT a zero-fill: it writes qwords at +0x00/+0x08/+0x10, a dword
// 0 at +0x18, the dword **1** at +0x20 and a dword 0 at +0x30, leaving +0x1c,
// +0x24..+0x2f and +0x34..+0x37 alone.
void DrawItemAssign(uint8_t* dst, const uint8_t* src) { std::memcpy(dst, src, 0x38); }
void DrawItemInit(uint8_t* dst) {
    const uint32_t zero = 0u, one = 1u;
    std::memset(dst + 0x00, 0, 8);
    std::memset(dst + 0x08, 0, 8);
    std::memset(dst + 0x10, 0, 8);
    std::memcpy(dst + 0x18, &zero, 4);
    std::memcpy(dst + 0x20, &one, 4);
    std::memcpy(dst + 0x30, &zero, 4);
}

// Renderer::Drawable, stride 0x10. ASSIGN copies two qwords; INIT writes a zero
// QWORD at +0x00 and a zero DWORD at +0x08, leaving +0x0c..+0x0f alone — a
// single window, so this spec can express it exactly.
void DrawableAssign(uint8_t* dst, const uint8_t* src) { std::memcpy(dst, src, 0x10); }
void DrawableInit(uint8_t* dst) {
    const uint32_t zero = 0u;
    std::memset(dst + 0x00, 0, 8);
    std::memcpy(dst + 0x08, &zero, 4);
}

// ============================================================================
// The three ArraySpec rows.
//
// `byteCopy` is TRUE only where `assign` really is a pure byte copy, because it
// is what gates SetSizeCore's realloc prefix memcmp. kSpec_SceneEntry's assign
// leaves two holes, so it is FALSE — turning that check on would compare bytes
// neither side wrote and fail 100% of reallocs.
//
// The unwritten windows are 0/0 ("none") on kSpec_SceneEntry and kSpec_DrawItem
// because both need MORE THAN ONE window and the struct carries one; the real
// hole lists live in this file's probe. See the banner. kSpec_Drawable's init
// hole fits and is spelled properly.
// ============================================================================

const ArraySpec kSpec_SceneEntry = {          // 0x180062ae0, 0x1800637b0
    "Array<Graphics::Renderer::SceneEntry>", 0x110, 8,
    &SceneEntryAssignSpec, &SceneEntryInitSpec, &SceneEntryDestroyRange,
    /*byteCopy*/ false,
    /*assign unwritten*/ 0, 0,       // really {0xe5,0xe8} and {0xfc,0x100}
    /*init   unwritten*/ 0, 0,       // really {0x71,0x74},{0xe5,0xe8},{0xfc,0x100}
    /*seed zero       */ 0x100, 0x110 };   // the inner Array is DEREFERENCED

const ArraySpec kSpec_DrawItem = {            // inside 0x1800626a0, x3
    "Array<Graphics::Renderer::DrawItem>", 0x38, 8,
    &DrawItemAssign, &DrawItemInit, nullptr, /*byteCopy*/ true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,       // really {0x1c,0x20},{0x24,0x30},{0x34,0x38}
    /*seed zero       */ 0, 0 };

const ArraySpec kSpec_Drawable = {            // 0x180062c40
    "Array<Graphics::Renderer::Drawable>", 0x10, 8,
    &DrawableAssign, &DrawableInit, nullptr, /*byteCopy*/ true,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0x0c, 0x10,
    /*seed zero       */ 0, 0 };

// ============================================================================
// The registered array entry points.
//
// Two of the three are the CONSTANT-FOLDED one-argument shape lane UIF
// documented at 0x1800783d0: retail's `newSize` is the immediate 0 and the body
// takes ONE argument, so registering them behind the two- or three-argument
// signature would read a register the caller never set.
//
// NEITHER IS A `~Array`, and reading them as one would be a live defect rather
// than a harmless mislabel: `~Array` frees the buffer unconditionally, and both
// of these reach their `CMP old,[a] / JZ return` epilogue without freeing
// anything on the path Reset takes (capacity is already >= 0, so no realloc
// happens and `old == _data`). Reset REUSES the buffers; it does not release
// them.
// ============================================================================

Site g_site_180062ae0 = { { "Array<SceneEntry>::SetSize(0) @0x180062ae0", 0, 0, false, false },
                          0x180062ae0 };
Site g_site_180062c40 = { { "Array<Drawable>::SetSize(0) @0x180062c40", 0, 0, false, false },
                          0x180062c40 };
Site g_site_1800637b0 = { { "Array<SceneEntry>::SetSize(n,fill) @0x1800637b0", 0, 0, false, false },
                          0x1800637b0 };
// ONE site, THREE SetSizeCore calls per invocation — so this site's `calls=` is
// three times the number of DrawGroups cleared, and it is named that way so the
// summary is not read as a per-call count.
Site g_site_1800626a0 = { { "DrawGroup pass-clear @0x1800626a0 (3 arrays per call)",
                            0, 0, false, false }, 0x1800626a0 };

extern "C" void RtGfxd_SceneArraySetSize0(void* a) {
    rtcoll::SetSizeCore(static_cast<ArrayHdr*>(a), 0, nullptr, &kSpec_SceneEntry,
                        &g_site_180062ae0);
}

extern "C" void RtGfxd_DrawableArraySetSize0(void* a) {
    rtcoll::SetSizeCore(static_cast<ArrayHdr*>(a), 0, nullptr, &kSpec_Drawable,
                        &g_site_180062c40);
}

extern "C" void RtGfxd_SceneArraySetSizeFill(void* a, int n, const void* fill) {
    rtcoll::SetSizeCore(static_cast<ArrayHdr*>(a), n,
                        static_cast<const uint8_t*>(fill), &kSpec_SceneEntry,
                        &g_site_1800637b0);
}

extern "C" void RtGfxd_SceneEntryDestroyRange(void* ignoredThis, uint8_t* base, int count) {
    SceneEntryDestroyRange(ignoredThis, base, count);
}

// Retail 0x1800626a0 — clear one DrawGroup's three per-pass item arrays, then
// zero the group's first qword (`MOV qword ptr [R12],R13` at 0x180062819, with
// R13 still the zero it was XOR'd to at entry). The three arrays sit at
// group+0x1c0 with stride 0x10, which is what RendererDrawGroup::items already
// records.
extern "C" void RtGfxd_DrawGroupClearPasses(void* group) {
    uint8_t* const g = static_cast<uint8_t*>(group);
    for (int i = 0; i < 3; ++i)
        rtcoll::SetSizeCore(reinterpret_cast<ArrayHdr*>(g + 0x1c0 + i * 0x10), 0,
                            nullptr, &kSpec_DrawItem, &g_site_1800626a0);
    std::memset(g, 0, 8);
}

// ============================================================================
// ?Reset@Renderer@Graphics@@QEAAXXZ — retail 0x180105110.
//
// Store order is retail's, and it is not the order a reader would guess: +0x34
// is set to -1 FIRST, before the group loop, and +0x40 and +0x38 are cleared
// LAST, after both arrays. +0x40 is `frameAlpha`, a float, and retail writes it
// with `MOV dword ptr [RBX+0x40],0x0` — an integer store of the zero bit
// pattern, which is +0.0f. It is spelled as a dword here for that reason.
//
// The loop re-reads `_drawGroups._size` from memory on every iteration
// (`CMP EDI,dword ptr [RBX+0x18]` at 0x180105145), which is faithful and also
// harmless: the callee cannot change it.
//
// NOTE WHAT RESET DOES NOT DO. It does not touch `_drawGroups`' own header at
// all — the group objects are cleared in place and the array keeps its size and
// its buffer, so the next frame's BeginDrawGroup reuses them. Only `_scenes` and
// `_drawables` are emptied, and even they keep their capacity.
// ============================================================================
extern "C" void RtGraphics_Renderer_Reset(Renderer* self) {
    uint8_t* const p = reinterpret_cast<uint8_t*>(self);

    const int32_t neg1 = -1;
    std::memcpy(p + 0x34, &neg1, 4);

    for (int32_t i = 0; i < *reinterpret_cast<const int32_t*>(p + 0x18); ++i) {
        uint8_t* groups = *reinterpret_cast<uint8_t* const*>(p + 0x10);
        RtGfxd_DrawGroupClearPasses(groups + static_cast<int64_t>(i) * 0x1f0);
    }

    RtGfxd_SceneArraySetSize0(p + 0x00);        // _scenes
    RtGfxd_DrawableArraySetSize0(p + 0x20);     // _drawables

    const uint32_t zero = 0u;
    std::memcpy(p + 0x40, &zero, 4);            // frameAlpha, as a dword store
    std::memcpy(p + 0x38, &zero, 4);
}

// ============================================================================
// ?AddScene@Renderer@Graphics@@QEAAXPEBVScene@2@PEBVRenderBuffer@Video@@
//                                  AEBVCamera@2@HHHI@Z — retail 0x180105700.
//
// *** THE FOUR STACK ARGUMENTS ARE NOT STORED IN SOURCE ORDER, AND THAT IS THE
// ONE THING IN THIS FUNCTION A READER WILL GET WRONG. *** Read off the four
// `[RSP+0x1xx]` loads at 0x18010572c..0x18010576f:
//
//     arg5 (H, [RSP+0x170])  ->  entry +0xe8
//     arg6 (H, [RSP+0x178])  ->  entry +0xf8      <-- out of order
//     arg7 (H, [RSP+0x180])  ->  entry +0xec      <-- out of order
//     arg8 (I, [RSP+0x188])  ->  entry +0xf0
//
// and +0xf4 is the immediate -1 (the entry's drawGroupIndex, which Draw writes
// back later from Renderer+0x34).
//
// `rec` IS DELIBERATELY UNINITIALISED. Retail builds this record on the stack
// and never writes +0xfc, nor the Camera's +0xd5..+0xd7 tail at +0xe5..+0xe7;
// the assign then copies those uninitialised bytes into the array element. A
// port that zeroed them would be "helpful" and WRONG in the one way that shows
// up as a byte divergence against a DLL-built Renderer
// (reference_oracle_must_not_compare_unwritten_bytes.md, and the same reasoning
// renderer.cpp's Renderer ctor gives for leaving +0x3c and +0x4c alone).
//
// The trailing `~Array<int32>` on `rec+0x100` is retail's (0x18010579c). It is
// not redundant: SetSize's assign copy-constructs a fresh buffer into the array
// element, so the record's own buffer — allocated by the Array's operator= at
// 0x18001cdf0 — has to be released before the record leaves scope.
// ============================================================================
extern "C" void RtGraphics_Renderer_AddScene(Renderer* self,
                                             const void* scene,
                                             const void* renderBuffer,
                                             const Camera* camera,
                                             int32_t arg5, int32_t arg6,
                                             int32_t arg7, uint32_t arg8) {
    uint8_t rec[0x110];                          // uninitialised ON PURPOSE

    std::memcpy(rec + 0x00, &scene, 8);
    std::memcpy(rec + 0x08, &renderBuffer, 8);
    RtGraphics_Camera_CopyCtor(reinterpret_cast<Camera*>(rec + 0x10), camera);

    const int32_t neg1 = -1;
    std::memcpy(rec + 0xe8, &arg5, 4);
    std::memcpy(rec + 0xec, &arg7, 4);
    std::memcpy(rec + 0xf0, &arg8, 4);
    std::memcpy(rec + 0xf4, &neg1, 4);
    std::memcpy(rec + 0xf8, &arg6, 4);
    std::memset(rec + 0x100, 0, 8);
    std::memset(rec + 0x108, 0, 8);

    uint8_t* const p = reinterpret_cast<uint8_t*>(self);
    RtGfxd_SceneArraySetSizeFill(p + 0x00,
                                 *reinterpret_cast<const int32_t*>(p + 0x08) + 1, rec);
    RtArrayDtorI32(rec + 0x100);
}

}  // namespace rtgfxd
// ############################################################################
// PROBE  (appended to wave9_lane_gfxd.cpp)
// ############################################################################
//
// WHY THERE IS A PROBE AT ALL, AND WHY IT IS IN TWO PHASES
//
// Everything above is inert before cutover (see the file banner): nothing in
// our tree calls `Renderer::Reset` or `Renderer::AddScene`, so every site's
// `calls=` is 0 and the diff oracle has nothing to compare
// (reference_uncalled_port_is_not_verified.md). The probe IS the oracle. It
// drives OURS and the SHIPPED body on two independently seeded copies of the
// same subject and compares the post-state, excluding exactly the bytes neither
// side writes and no others.
//
// THE SHIPPED BODIES COME THROUGH ResolveRuntimeRaw, NEVER ResolveAddr, for the
// reason scene_grid_probe.cpp spells out: `ResolveAddr(Module::Runtime, …)`
// consults the port registry FIRST, this lane registers every one of these
// addresses, and the probe would otherwise compare ours to ours and agree by
// construction (reference_resolveaddr_consults_port_registry_first.md, and
// reference_recheck_must_match_its_own_proof.md for the general shape of that
// mistake).
//
// TWO PHASES, because of WHERE each arm can run:
//
//   PHASE 1 runs at registration. Every arm is ALLOCATION-FREE BY
//   CONSTRUCTION — each subject array gets a static buffer and a capacity that
//   already covers its size, so `newSize > _capacity` is never true and neither
//   side reaches MemAlloc or MemFree. That is not fastidiousness: this init runs
//   right after ResolveInit()'s LoadLibrary, where the DLL's MemoryAllocator is
//   CONSTRUCTED but not STARTED, and allocating there takes a critical section
//   that does not exist yet and faults on null+0x640 (array_internal.h's
//   banner; lane PKG2 paid for this once already).
//
//   Phase 1 is NOT a reduced test of `Reset`. SetSize(0) is the only arm
//   `Reset` can ever take — shrinking to zero never grows a capacity — so the
//   arm phase 1 skips is one `Reset` cannot reach.
//
//   PHASE 2 needs the allocator: `AddScene` grows the scene array from empty
//   and the SceneEntry assign's inner-array copy allocates. It is armed at init
//   and fired by the first live ProbeGate(), the same self-scheduling contract
//   scene_grid.cpp and picking/scene_grid.cpp use. Its gate sits in
//   Graphics::Scene::Clean, which runs per frame and cannot be reached before
//   Engine::Startup has built a scene.
//
// BOTH PHASES ARE OFF BY DEFAULT, behind BANISHED_RT_GFXD_PROBE. A shipping run
// executes none of this and allocates nothing. The env var is also what puts an
// identity line only this build emits into the log.
//
// CONTROLS
//   * EFFECTIVENESS: every comparison counts how many COMPARABLE bytes the body
//     actually changed relative to the seed. An arm that changed nothing was
//     compared vacuously and is reported as VACUOUS, not as a pass
//     (reference_probe_needs_an_effectiveness_control.md). "64 green
//     comparisons where nothing happened" is the failure this exists to catch.
//   * NEGATIVE: four deliberately-wrong variants that MUST be flagged, and the
//     run reports how many actually fired. They are deliberately SMALL — one
//     dword short, one field's value off by one, two arguments swapped — because
//     a negative control that a memcmp would catch even with the port deleted
//     says nothing about the comparison's resolution.
//   * SEEDING is a varying non-zero pattern. All-zero subjects are the one
//     seeding under which an unwritten-byte defect is invisible, because both
//     sides read zero there and agree by accident (array_internal.h's
//     `seedZero` note; kSpec_Sprite38's open row is exactly that bug).
//
// If a shipped body does not resolve, the probe SKIPS that arm and says so,
// rather than reporting a clean run over zero comparisons.
// ############################################################################

// Byte-identical to standalone/resolve.h:108; the linker checks them.
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace rtgfxd {

bool g_probePending = false;

namespace {

bool g_probeRunning = false;
int  g_cmp = 0, g_bad = 0, g_vacuous = 0;
int  g_neg = 0, g_negFired = 0, g_skipped = 0;
long g_changed = 0;

bool ProbeArmed() {
    const char* e = std::getenv("BANISHED_RT_GFXD_PROBE");
    return e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0;
}

struct Hole { uint32_t lo, hi; };

inline bool InHole(size_t i, const Hole* h, int n) {
    for (int k = 0; k < n; ++k)
        if (i >= h[k].lo && i < h[k].hi) return true;
    return false;
}

// The one comparison primitive.
//
// `seed` is the pre-run snapshot of OURS, so `changed` counts comparable bytes
// this body actually wrote differently from what it was handed. THAT NUMBER IS
// THE EFFECTIVENESS CONTROL and it cuts both ways, which is why `expect` exists:
//
//   expect = Changes  — the arm is supposed to write. changed == 0 means the
//            comparison was VACUOUS: both sides agreed because neither did
//            anything, and a green result proves nothing
//            (reference_probe_needs_an_effectiveness_control.md).
//   expect = Untouched — the arm is supposed to leave this buffer alone, and
//            the assertion is `changed == 0`. A non-zero count here is a real
//            defect (a port that "helpfully" scrubbed a tail), and reporting it
//            as VACUOUS would have inverted the meaning of the check.
//
// The first draft had only the first rule, which would have flagged the four
// deliberate untouched-buffer checks below as vacuous and buried the two that
// matter.
enum Expect { Changes, Untouched };

void Cmp(const char* what, const void* ours, const void* theirs, const void* seed,
         size_t n, Expect expect = Changes,
         const Hole* holes = nullptr, int nHoles = 0) {
    const uint8_t* o = static_cast<const uint8_t*>(ours);
    const uint8_t* t = static_cast<const uint8_t*>(theirs);
    const uint8_t* s = static_cast<const uint8_t*>(seed);
    bool ok = true;
    int changed = 0;
    size_t firstBad = 0;
    for (size_t i = 0; i < n; ++i) {
        if (InHole(i, holes, nHoles)) continue;
        if (o[i] != t[i]) { if (ok) firstBad = i; ok = false; }
        if (s && o[i] != s[i]) ++changed;
    }
    ++g_cmp;
    g_changed += changed;
    if (s && expect == Changes && changed == 0) {
        ++g_vacuous;
        std::printf("[GfxdProbe] VACUOUS: %s wrote 0 comparable bytes -- "
                    "this comparison proves nothing\n", what);
    }
    if (s && expect == Untouched && changed != 0) {
        ++g_bad;
        std::printf("[GfxdProbe] UNEXPECTED WRITE: %s changed %d bytes it must "
                    "not touch\n", what, changed);
    }
    if (!ok) {
        ++g_bad;
        std::printf("[GfxdProbe] MISMATCH: %s at +0x%zx ours=0x%02x theirs=0x%02x\n",
                    what, firstBad, o[firstBad], t[firstBad]);
    }
}

// A comparison that is EXPECTED to disagree. Reports loudly when it does not,
// because a negative control that never fires is a broken instrument, not a
// clean bill of health.
void Neg(const char* what, const void* ours, const void* theirs, size_t n,
         const Hole* holes = nullptr, int nHoles = 0) {
    const uint8_t* o = static_cast<const uint8_t*>(ours);
    const uint8_t* t = static_cast<const uint8_t*>(theirs);
    ++g_neg;
    for (size_t i = 0; i < n; ++i)
        if (!InHole(i, holes, nHoles) && o[i] != t[i]) { ++g_negFired; return; }
    std::printf("[GfxdProbe] NEGATIVE CONTROL DID NOT FIRE: %s\n", what);
}

void Seed(void* p, size_t n, uint32_t salt) {
    uint8_t* b = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < n; ++i)
        b[i] = static_cast<uint8_t>(0x5au ^ (i * 31u) ^ (salt * 7u) ^ (i >> 5));
}

// ---- the shipped bodies, raw door only -------------------------------------
using Ctor2Fn    = void* (*)(void*, const void*);
using Ctor1Fn    = void* (*)(void*);
using SetSize1Fn = void  (*)(void*);
using SetSize3Fn = void  (*)(void*, int, const void*);
using ResetFn    = void  (*)(void*);
using AddSceneFn = void  (*)(void*, const void*, const void*, const void*,
                             int32_t, int32_t, int32_t, uint32_t);

struct Shipped {
    Ctor2Fn    camCopy    = nullptr;   // 0x180017a70
    Ctor1Fn    camDefault = nullptr;   // 0x1800fe7f0
    Ctor2Fn    entAssign  = nullptr;   // 0x18001bdc0
    Ctor1Fn    entInit    = nullptr;   // 0x180062860
    SetSize1Fn sceneZero  = nullptr;   // 0x180062ae0
    SetSize1Fn drawZero   = nullptr;   // 0x180062c40
    SetSize3Fn sceneFill  = nullptr;   // 0x1800637b0
    Ctor1Fn    groupClear = nullptr;   // 0x1800626a0  (void* in, ignored out)
    ResetFn    reset      = nullptr;   // 0x180105110
    AddSceneFn addScene   = nullptr;   // 0x180105700
    bool       complete   = false;
};

Shipped g_dll;

void ResolveShipped() {
    g_dll.camCopy    = reinterpret_cast<Ctor2Fn>(ResolveRuntimeRaw(0x180017a70));
    g_dll.camDefault = reinterpret_cast<Ctor1Fn>(ResolveRuntimeRaw(0x1800fe7f0));
    g_dll.entAssign  = reinterpret_cast<Ctor2Fn>(ResolveRuntimeRaw(0x18001bdc0));
    g_dll.entInit    = reinterpret_cast<Ctor1Fn>(ResolveRuntimeRaw(0x180062860));
    g_dll.sceneZero  = reinterpret_cast<SetSize1Fn>(ResolveRuntimeRaw(0x180062ae0));
    g_dll.drawZero   = reinterpret_cast<SetSize1Fn>(ResolveRuntimeRaw(0x180062c40));
    g_dll.sceneFill  = reinterpret_cast<SetSize3Fn>(ResolveRuntimeRaw(0x1800637b0));
    g_dll.groupClear = reinterpret_cast<Ctor1Fn>(ResolveRuntimeRaw(0x1800626a0));
    g_dll.reset      = reinterpret_cast<ResetFn>(ResolveRuntimeRaw(0x180105110));
    g_dll.addScene   = reinterpret_cast<AddSceneFn>(ResolveRuntimeRaw(0x180105700));
    g_dll.complete = g_dll.camCopy && g_dll.camDefault && g_dll.entAssign &&
                     g_dll.entInit && g_dll.sceneZero && g_dll.drawZero &&
                     g_dll.sceneFill && g_dll.groupClear && g_dll.reset &&
                     g_dll.addScene;
}

// ---- hole lists, one per ARM (not per type) --------------------------------
// reference_never_written_needs_by_whom.md: a written extent belongs to the
// WRITER. The Camera copy constructor and the Camera default constructor leave
// DIFFERENT holes, and the SceneEntry assign and init inherit different ones.
const Hole kHoleCamCopy[]  = { {0xd5, 0xd8} };
const Hole kHoleCamInit[]  = { {0x61, 0x64}, {0xd5, 0xd8} };
const Hole kHoleEntAssign[]= { {0xe5, 0xe8}, {0xfc, 0x100} };
const Hole kHoleEntInit[]  = { {0x71, 0x74}, {0xe5, 0xe8}, {0xfc, 0x100} };

// ---- subject storage -------------------------------------------------------
// Static and 16-byte aligned. Static is what keeps phase 1 allocation-free; the
// alignment is for the qword stores both sides make.
constexpr int kNs = 3;     // scene entries
constexpr int kNg = 2;     // draw groups
constexpr int kNd = 5;     // drawables
constexpr int kNi = 4;     // draw items per pass

alignas(16) uint8_t g_ent[2][kNs * 0x110];
alignas(16) uint8_t g_grp[2][kNg * 0x1f0];
alignas(16) uint8_t g_drw[2][kNd * 0x10];
alignas(16) uint8_t g_itm[2][kNg * 3 * kNi * 0x38];
alignas(16) uint8_t g_rnd[2][0x50];

// Write an Array triple {data, size, capacity} in place.
void PutArray(uint8_t* hdr, void* data, int32_t size, int32_t cap) {
    std::memcpy(hdr + 0x00, &data, 8);
    std::memcpy(hdr + 0x08, &size, 4);
    std::memcpy(hdr + 0x0c, &cap, 4);
}

// Build one side's scene-entry buffer.
//
// Every entry's inner Array DATA POINTER at +0x100 is nulled on purpose:
// FreeInnerArraysCore frees whatever it finds there, and a static buffer handed
// to the DLL's Free is heap corruption, not a test. Only the `if (p) MemFree(p)`
// branch is skipped; the walk and the clear are fully exercised.
//
// *** THE SIZE AND CAPACITY AT +0x108 ARE LEFT SEEDED, AND THAT IS LOAD-BEARING.
// *** The destroy's whole observable effect on these entries is zeroing
// +0x100..+0x10f. Zeroing all sixteen bytes here would mean the destroy wrote
// zeros over zeros, `changed` would come back 0, and the arm would pass while
// proving nothing — the exact "uniform population is not a verification" trap
// (reference_uniform_population_is_not_a_verification.md). With +0x108 seeded,
// the arm's effectiveness count is non-zero and the clear is actually observed.
void BuildEntries(uint8_t* buf, uint32_t salt) {
    Seed(buf, kNs * 0x110, salt);
    for (int i = 0; i < kNs; ++i)
        std::memset(buf + i * 0x110 + 0x100, 0, 8);   // the POINTER only
}

void BuildGroups(uint8_t* groups, uint8_t* items, uint32_t salt) {
    Seed(groups, kNg * 0x1f0, salt);
    Seed(items, kNg * 3 * kNi * 0x38, salt + 1);
    for (int g = 0; g < kNg; ++g)
        for (int p = 0; p < 3; ++p) {
            uint8_t* pass = items + ((g * 3) + p) * kNi * 0x38;
            PutArray(groups + g * 0x1f0 + 0x1c0 + p * 0x10, pass, kNi, kNi);
        }
}

}  // namespace

// ============================================================================
// PHASE 1 — allocation-free. Runs at registration.
// ============================================================================
namespace {

// Per-subject snapshots. One buffer per subject rather than one shared scratch:
// several arms compare TWO buffers against their own pre-run state in the same
// block, and a single shared snapshot silently makes the second comparison read
// the first one's bytes.
alignas(16) uint8_t g_snapEnt[kNs * 0x110];
alignas(16) uint8_t g_snapGrp[kNg * 0x1f0];
alignas(16) uint8_t g_snapDrw[kNd * 0x10];
alignas(16) uint8_t g_snapItm[kNg * 3 * kNi * 0x38];
alignas(16) uint8_t g_snapRnd[0x50];

}  // namespace

// ============================================================================
// PHASE 1 — allocation-free. Runs at registration.
// ============================================================================
namespace {

void Phase1() {
    // ---- arm A: Camera copy constructor -----------------------------------
    {
        alignas(16) uint8_t src[0xd8], a[0xd8], b[0xd8], snap[0xd8];
        Seed(src, sizeof(src), 11);
        Seed(a, sizeof(a), 12); std::memcpy(b, a, sizeof(a));
        std::memcpy(snap, a, sizeof(a));

        RtGraphics_Camera_CopyCtor(reinterpret_cast<Camera*>(a),
                                   reinterpret_cast<const Camera*>(src));
        g_dll.camCopy(b, src);
        Cmp("Camera copy ctor 0x180017a70", a, b, snap, sizeof(a), Changes,
            kHoleCamCopy, 1);

        // NEGATIVE: 52 dwords instead of 53, so +0xd0..+0xd3 is never copied.
        // One dword short, entirely inside the compared region and nowhere near
        // the hole — a control that only fired on the hole would be testing the
        // hole list, not the copy.
        alignas(16) uint8_t n[0xd8];
        std::memcpy(n, snap, sizeof(n));
        for (int i = 0; i < 52; ++i) std::memcpy(n + i * 4, src + i * 4, 4);
        n[0xd4] = src[0xd4];
        Neg("Camera copy ctor, one dword short", n, b, sizeof(n), kHoleCamCopy, 1);
    }

    // ---- arm B: Camera default constructor --------------------------------
    {
        alignas(16) uint8_t a[0xd8], b[0xd8], snap[0xd8];
        Seed(a, sizeof(a), 21); std::memcpy(b, a, sizeof(a));
        std::memcpy(snap, a, sizeof(a));

        RtGraphics_Camera_DefaultCtor(reinterpret_cast<Camera*>(a));
        g_dll.camDefault(b);
        Cmp("Camera default ctor 0x1800fe7f0", a, b, snap, sizeof(a), Changes,
            kHoleCamInit, 2);
    }

    // ---- arm C: SceneEntry default construct ------------------------------
    {
        alignas(16) uint8_t a[0x110], b[0x110], snap[0x110];
        Seed(a, sizeof(a), 31); std::memcpy(b, a, sizeof(a));
        std::memcpy(snap, a, sizeof(a));

        SceneEntryInit(a);
        g_dll.entInit(b);
        Cmp("SceneEntry init 0x180062860", a, b, snap, sizeof(a), Changes,
            kHoleEntInit, 3);

        // NEGATIVE: +0xf8 written as 0x41 rather than retail's 0x40 — one byte,
        // in a field a port could plausibly misread off the disassembly.
        alignas(16) uint8_t n[0x110];
        std::memcpy(n, a, sizeof(n));
        const uint32_t wrong = 0x41u;
        std::memcpy(n + 0xf8, &wrong, 4);
        Neg("SceneEntry init, +0xf8 off by one", n, b, sizeof(n), kHoleEntInit, 3);
    }

    // ---- arm D: Array<Drawable>::SetSize(0) -------------------------------
    // The header is emptied; the ELEMENT BUFFER must not be touched at all.
    // That second check is the one that catches a port which scrubbed the tail.
    {
        Seed(g_drw[0], sizeof(g_drw[0]), 41);
        std::memcpy(g_drw[1], g_drw[0], sizeof(g_drw[0]));
        std::memcpy(g_snapDrw, g_drw[0], sizeof(g_drw[0]));

        alignas(16) uint8_t hdrA[0x10], hdrB[0x10], snapH[0x10];
        PutArray(hdrA, g_drw[0], kNd, kNd);
        PutArray(hdrB, g_drw[1], kNd, kNd);
        std::memcpy(snapH, hdrA, sizeof(hdrA));

        RtGfxd_DrawableArraySetSize0(hdrA);
        g_dll.drawZero(hdrB);

        Cmp("Array<Drawable>::SetSize(0) 0x180062c40 header", hdrA, hdrB, snapH,
            0x10, Changes);
        Cmp("Array<Drawable>::SetSize(0) 0x180062c40 element buffer",
            g_drw[0], g_drw[1], g_snapDrw, sizeof(g_drw[0]), Untouched);
    }

    // ---- arm E: Array<SceneEntry>::SetSize(0), the DESTROY arm ------------
    {
        BuildEntries(g_ent[0], 51);
        BuildEntries(g_ent[1], 51);
        std::memcpy(g_snapEnt, g_ent[0], sizeof(g_ent[0]));

        alignas(16) uint8_t hdrA[0x10], hdrB[0x10], snapH[0x10];
        PutArray(hdrA, g_ent[0], kNs, kNs);
        PutArray(hdrB, g_ent[1], kNs, kNs);
        std::memcpy(snapH, hdrA, sizeof(hdrA));

        RtGfxd_SceneArraySetSize0(hdrA);
        g_dll.sceneZero(hdrB);

        Cmp("Array<SceneEntry>::SetSize(0) 0x180062ae0 header", hdrA, hdrB, snapH,
            0x10, Changes);
        // This one MUST change: the destroy clears every entry's +0x100..+0x10f,
        // and BuildEntries leaves the size/capacity half seeded so the clear is
        // observable. See BuildEntries.
        Cmp("Array<SceneEntry>::SetSize(0) 0x180062ae0 element destroy",
            g_ent[0], g_ent[1], g_snapEnt, sizeof(g_ent[0]), Changes);
    }

    // ---- arm F: DrawGroup pass-clear --------------------------------------
    {
        BuildGroups(g_grp[0], g_itm[0], 61);
        BuildGroups(g_grp[1], g_itm[1], 61);
        std::memcpy(g_snapGrp, g_grp[0], sizeof(g_grp[0]));
        std::memcpy(g_snapItm, g_itm[0], sizeof(g_itm[0]));

        for (int g = 0; g < kNg; ++g) {
            RtGfxd_DrawGroupClearPasses(g_grp[0] + g * 0x1f0);
            g_dll.groupClear(g_grp[1] + g * 0x1f0);
        }
        Cmp("DrawGroup pass-clear 0x1800626a0", g_grp[0], g_grp[1], g_snapGrp,
            sizeof(g_grp[0]), Changes);
        Cmp("DrawGroup pass-clear item buffers", g_itm[0], g_itm[1], g_snapItm,
            sizeof(g_itm[0]), Untouched);
    }

    // ---- arm G: Renderer::Reset (the composite) ---------------------------
    {
        for (int s = 0; s < 2; ++s) {
            BuildEntries(g_ent[s], 71);
            BuildGroups(g_grp[s], g_itm[s], 72);
            Seed(g_drw[s], sizeof(g_drw[s]), 73);
            Seed(g_rnd[s], sizeof(g_rnd[s]), 74);
            PutArray(g_rnd[s] + 0x00, g_ent[s], kNs, kNs);
            PutArray(g_rnd[s] + 0x10, g_grp[s], kNg, kNg);
            PutArray(g_rnd[s] + 0x20, g_drw[s], kNd, kNd);
        }
        std::memcpy(g_snapRnd, g_rnd[0], sizeof(g_rnd[0]));
        std::memcpy(g_snapEnt, g_ent[0], sizeof(g_ent[0]));
        std::memcpy(g_snapGrp, g_grp[0], sizeof(g_grp[0]));
        std::memcpy(g_snapDrw, g_drw[0], sizeof(g_drw[0]));

        RtGraphics_Renderer_Reset(reinterpret_cast<Renderer*>(g_rnd[0]));
        g_dll.reset(g_rnd[1]);

        Cmp("Renderer::Reset 0x180105110 object", g_rnd[0], g_rnd[1], g_snapRnd,
            sizeof(g_rnd[0]), Changes);
        Cmp("Renderer::Reset scene buffer", g_ent[0], g_ent[1], g_snapEnt,
            sizeof(g_ent[0]), Changes);
        Cmp("Renderer::Reset group buffer", g_grp[0], g_grp[1], g_snapGrp,
            sizeof(g_grp[0]), Changes);
        // Reset empties the drawable HEADER and leaves the elements alone.
        Cmp("Renderer::Reset drawable buffer", g_drw[0], g_drw[1], g_snapDrw,
            sizeof(g_drw[0]), Untouched);

        // NEGATIVE: a Reset that also empties _drawGroups. That is the single
        // most plausible misreading of this body — it empties the other two
        // arrays' headers and deliberately leaves this one alone so the groups
        // can be reused next frame — and it is four bytes.
        alignas(16) uint8_t n[0x50];
        std::memcpy(n, g_rnd[0], sizeof(n));
        const int32_t z = 0;
        std::memcpy(n + 0x18, &z, 4);
        Neg("Reset that also empties _drawGroups", n, g_rnd[1], sizeof(n));
    }
}

// ============================================================================
// PHASE 2 — the allocating arms. Deferred to the first live ProbeGate().
// ============================================================================
void Phase2() {
    // ---- arm H: SceneEntry copy-assign with a NON-EMPTY inner array -------
    // The arm that allocates: Array<int32>::operator= grows the destination to
    // match the source.
    {
        alignas(16) uint8_t src[0x110], a[0x110], b[0x110], snap[0x110];
        static int32_t ids[4] = { 7, -3, 0x40000000, -1 };
        Seed(src, sizeof(src), 81);
        PutArray(src + 0x100, ids, 4, 4);
        Seed(a, sizeof(a), 82);
        std::memset(a + 0x100, 0, 0x10);          // dst starts with no buffer
        std::memcpy(b, a, sizeof(a));
        std::memcpy(snap, a, sizeof(a));

        SceneEntryAssign(a, src);
        g_dll.entAssign(b, src);

        Cmp("SceneEntry assign 0x18001bdc0 (fixed part)", a, b, snap, 0x100,
            Changes, kHoleEntAssign, 2);
        // The two inner buffers are DIFFERENT allocations, so the POINTERS must
        // not be compared — only the size/capacity pair and the contents.
        // Comparing the pointers is the classic way to make a correct port read
        // as a divergence.
        Cmp("SceneEntry assign inner size/cap", a + 0x108, b + 0x108, snap + 0x108,
            8, Changes);
        int32_t* pa = *reinterpret_cast<int32_t**>(a + 0x100);
        int32_t* pb = *reinterpret_cast<int32_t**>(b + 0x100);
        if (pa && pb) {
            Cmp("SceneEntry assign inner contents", pa, pb, nullptr, 4 * 4);
        } else {
            ++g_skipped;
            std::printf("[GfxdProbe] SKIP: inner buffer null on one side "
                        "(ours=%p theirs=%p)\n",
                        static_cast<void*>(pa), static_cast<void*>(pb));
        }
        rtcoll::MemFree(pa);
        rtcoll::MemFree(pb);
    }

    // ---- arm I: Renderer::AddScene on a fresh Renderer --------------------
    // The live shape: the scene array is empty, so SetSize(1) takes the REALLOC
    // arm that phase 1 cannot reach.
    {
        alignas(16) uint8_t camera[0xd8];
        Seed(camera, sizeof(camera), 91);
        alignas(16) uint8_t ra[0x50], rb[0x50];
        Seed(ra, sizeof(ra), 92);
        std::memcpy(rb, ra, sizeof(ra));
        for (int s = 0; s < 2; ++s) {
            uint8_t* r = (s == 0) ? ra : rb;
            PutArray(r + 0x00, nullptr, 0, 0);
            PutArray(r + 0x10, nullptr, 0, 0);
            PutArray(r + 0x20, nullptr, 0, 0);
        }
        const void* scene = reinterpret_cast<const void*>(0x1234567800abcdefULL);
        const void* rbuf  = reinterpret_cast<const void*>(0x0fedcba987654321ULL);

        RtGraphics_Renderer_AddScene(reinterpret_cast<Renderer*>(ra), scene, rbuf,
                                     reinterpret_cast<const Camera*>(camera),
                                     0x11, 0x22, 0x33, 0x44444444u);
        g_dll.addScene(rb, scene, rbuf, camera, 0x11, 0x22, 0x33, 0x44444444u);

        // Sizes and capacities must match; the two _data pointers are different
        // allocations and are excluded by starting the compare at +0x08.
        alignas(16) uint8_t hdrSeed[8];
        std::memset(hdrSeed, 0, sizeof(hdrSeed));
        Cmp("AddScene 0x180105700 scene size/cap", ra + 0x08, rb + 0x08, hdrSeed,
            8, Changes);

        uint8_t* ea = *reinterpret_cast<uint8_t**>(ra + 0x00);
        uint8_t* eb = *reinterpret_cast<uint8_t**>(rb + 0x00);
        if (ea && eb) {
            Cmp("AddScene entry (fixed part)", ea, eb, nullptr, 0x100,
                Changes, kHoleEntAssign, 2);
            Cmp("AddScene entry scalars +0xe8..+0xfb", ea + 0xe8, eb + 0xe8,
                nullptr, 0x14);

            // NEGATIVE, and the most valuable control in this lane: the two
            // stack arguments AddScene stores OUT OF SOURCE ORDER. arg6 goes to
            // +0xf8 and arg7 to +0xec. A port that assumed source order writes
            // them swapped, and nothing else in the function would look wrong.
            alignas(16) uint8_t n[0x110];
            std::memcpy(n, ea, sizeof(n));
            const int32_t a7 = 0x33, a6 = 0x22;
            std::memcpy(n + 0xf8, &a7, 4);
            std::memcpy(n + 0xec, &a6, 4);
            Neg("AddScene with arg6/arg7 swapped", n, eb, sizeof(n),
                kHoleEntAssign, 2);
        } else {
            ++g_skipped;
            std::printf("[GfxdProbe] SKIP: AddScene produced a null buffer\n");
        }

        // Release both scene arrays through the shipped ~Array<SceneEntry>
        // (0x18001c7e0, which lane Q ported): each entry's inner array is empty
        // here, so this is one Free per side and the probe leaks nothing.
        using DtorFn = void* (*)(void*);
        DtorFn dtor = reinterpret_cast<DtorFn>(ResolveRuntimeRaw(0x18001c7e0));
        if (dtor) { dtor(ra + 0x00); dtor(rb + 0x00); }
    }
}

void Report(const char* phase) {
    std::printf("[GfxdProbe] %s: comparisons=%d mismatches=%d vacuous=%d "
                "changedBytes=%ld negControls=%d fired=%d skipped=%d\n",
                phase, g_cmp, g_bad, g_vacuous, g_changed, g_neg, g_negFired,
                g_skipped);
    if (g_neg != g_negFired)
        std::printf("[GfxdProbe] %s: *** %d NEGATIVE CONTROL(S) DID NOT FIRE -- "
                    "the comparison is not discriminating and the PASS above "
                    "cannot be believed ***\n", phase, g_neg - g_negFired);
    std::printf("[GfxdProbe] %s: %s\n", phase,
                (g_bad == 0 && g_vacuous == 0 && g_neg == g_negFired)
                    ? "PASS" : "REVIEW");
}

}  // namespace

// The deferred gate. See the PROBE banner for why phase 2 cannot run at init.
void ProbeGate() {
    if (!g_probePending || g_probeRunning) return;
    g_probeRunning = true;
    g_probePending = false;
    if (g_dll.complete) { Phase2(); Report("phase2"); }
    g_probeRunning = false;
}

void ProbeRunPhase1() {
    if (!ProbeArmed()) return;
    ResolveShipped();
    if (!g_dll.complete) {
        std::printf("[GfxdProbe] SKIPPED -- one or more shipped bodies did not "
                    "resolve; NOT reporting a clean run over zero comparisons\n");
        return;
    }
    Phase1();
    Report("phase1");
    g_probePending = true;      // arm phase 2
}

}  // namespace rtgfxd

// ============================================================================
// REGISTRATION
//
// FOUR EXPORTS and ELEVEN ADDRESSES. Every mangled name is spelled as a
// LITERAL, never through a named constant: tools/runtime_census.py matches
// `RegisterRuntimeExport("<lit>"` and cannot see through a constant, which is
// how Wave 3's Object count read 21/41 when it was really 27/41
// (reference_census_cannot_see_nonliteral_registrations.md).
//
// EVERY ONE OF THE ELEVEN ADDRESSES WAS CHECKED FOR AN EXISTING REGISTRATION
// FIRST. A duplicate is last-binding-wins and silently turns one of the two
// bodies into dead code, which review does not catch and a diff table does
// (reference_grep_for_the_port_before_writing_it.md). All eleven came back with
// zero `RegisterRuntime*` hits across runtime/, standalone/, code/, engine/ and
// dx11/; the only pre-existing mentions are five COMMENTS, listed in
// WAVE9_LANE_GFXD.md §5.
//
// THESE REGISTRATIONS ARE INERT TODAY and that is stated rather than implied.
// No door in the tree resolves any of the four names or any of the eleven
// addresses, so nothing changes in a live run and every site's `calls=` is 0.
// They are what a lane porting `Draw` needs to exist, and they are cheap
// insurance in the sense lane GFXR's limit 3 describes — NOT evidence of
// anything, and they must not be read as closing a residue row.
//
// ONE OF THE ELEVEN IS SOMEONE ELSE'S PROOF. 0x18001ce90 is `FreeInnerArraysCore`
// with lane DESTROY's kD_InnerArr_110 — the spec row written for THIS address
// (array_destroy.cpp:255) — and array_destroy_probe.cpp:729 already drives that
// exact pairing against the shipped body. Lane DESTROY verified it and
// registered only 0x18001c7e0, the `~Array` that calls it. This lane registers
// the callee, and does not re-prove what that probe proved.
// ============================================================================

int RtGraphicsWave9GfxdRegister() {
    int exports = 0;

    // ---- Graphics::Renderer, two of the four exports Draw calls ------------
    RegisterRuntimeExport("?Reset@Renderer@Graphics@@QEAAXXZ",
                          (void*)&rtgfxd::RtGraphics_Renderer_Reset); ++exports;
    RegisterRuntimeAddr(0x180105110, (void*)&rtgfxd::RtGraphics_Renderer_Reset);

    RegisterRuntimeExport("?AddScene@Renderer@Graphics@@QEAAXPEBVScene@2@PEBVRenderBuffer@Video@@AEBVCamera@2@HHHI@Z",
                          (void*)&rtgfxd::RtGraphics_Renderer_AddScene); ++exports;
    RegisterRuntimeAddr(0x180105700, (void*)&rtgfxd::RtGraphics_Renderer_AddScene);

    // ---- Graphics::Camera, two MORE of its thirteen exports ---------------
    // renderer.cpp says "all five of the class's exports". The export table says
    // thirteen; these are the sixth and seventh. See the file banner.
    RegisterRuntimeExport("??0Camera@Graphics@@QEAA@AEBV01@@Z",
                          (void*)&rtgfxd::RtGraphics_Camera_CopyCtor); ++exports;
    RegisterRuntimeAddr(0x180017a70, (void*)&rtgfxd::RtGraphics_Camera_CopyCtor);

    RegisterRuntimeExport("??0Camera@Graphics@@QEAA@XZ",
                          (void*)&rtgfxd::RtGraphics_Camera_DefaultCtor); ++exports;
    RegisterRuntimeAddr(0x1800fe7f0, (void*)&rtgfxd::RtGraphics_Camera_DefaultCtor);

    // ---- the seven unexported internals underneath them -------------------
    RegisterRuntimeAddr(0x18001bdc0, (void*)&rtgfxd::SceneEntryAssign);
    RegisterRuntimeAddr(0x180062860, (void*)&rtgfxd::SceneEntryInit);
    RegisterRuntimeAddr(0x18001ce90, (void*)&rtgfxd::RtGfxd_SceneEntryDestroyRange);
    RegisterRuntimeAddr(0x180062ae0, (void*)&rtgfxd::RtGfxd_SceneArraySetSize0);
    RegisterRuntimeAddr(0x180062c40, (void*)&rtgfxd::RtGfxd_DrawableArraySetSize0);
    RegisterRuntimeAddr(0x1800637b0, (void*)&rtgfxd::RtGfxd_SceneArraySetSizeFill);
    RegisterRuntimeAddr(0x1800626a0, (void*)&rtgfxd::RtGfxd_DrawGroupClearPasses);

    // Phase 1 of the oracle. No-ops unless BANISHED_RT_GFXD_PROBE is set, and
    // allocation-free when it is not — see the PROBE banner for why that
    // matters at this point in the boot.
    rtgfxd::ProbeRunPhase1();

    return exports;
}
