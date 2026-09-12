#pragma once
// ============================================================================
// runtime/types/ui/SpriteContext.h — binary layout for UI::SpriteContext and
//                                     UI::SpriteDraw
// ============================================================================
//
// Started by Wave 7 lane F as an evidence note behind a hold-back. Rewritten by
// WAVE 9 LANE UIF (2026-09-08), which ported three of the class's exports and
// two of its internals, so the offsets below are now LOAD-BEARING and carry
// static_asserts instead of prose. The per-function write-up, the door
// arithmetic and everything this lane declined are in
// docs/runtime_port/WAVE9_LANE_UIF2.md.
//
// PORTABILITY: no <windows.h>, per PLAN §4.1 decision 1.
//
// Retail addresses (Runtime-x64.dll, image base 0x180000000):
//   AddLine                0x18011cbe0  ?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z
//   SetScissor(Viewport)   0x18011d050  ?SetScissor@…@QEAA?AVViewport@Video@@AEBV34@@Z
//   SetScissor(4 ints)     0x18011d010  ?SetScissor@…@QEAA?AVViewport@Video@@HHHH@Z
//   AddSpriteDraw          0x18011c890  (internal, no export in either build)
//   FlushDeferred          0x18011c6a0  (internal, no export in either build)
//   Flush                  0x18011ba40  (internal — NOT PORTED, see below)
//   Begin                  0x18011d2b0  (internal — NOT PORTED, see below)
//
// ---------------------------------------------------------------------------
// SIZE: at least 0x88. NOT PROVEN EXACTLY, and this header says so.
// ---------------------------------------------------------------------------
// Unlike UI::UserInterface, whose 0x128 is a literal in a MemoryAllocator
// call, SpriteContext is never heap-allocated in any body this lane read: it
// is a LOCAL of UI::UserInterface::Draw (0x180121890), built at `RBP-0x80` and
// destroyed inline at the end of the same frame. A stack local's size is not
// spelled out anywhere in the instruction stream.
//
// What IS proven is a lower bound of 0x88: Begin (0x18011d2b0) stores a qword
// at `[RCX+0x80]` (0x18011d2eb), so the object extends at least to 0x88.
// `_size` below is therefore declared as that lower bound, and the
// static_assert asserts the bound rather than pretending to an exact sizeof.
// NOTHING IN THE PORT ALLOCATES ONE, so the bound is sufficient: our three
// ported bodies and two ported internals only ever receive a pointer to an
// object retail itself built.
//
// ---------------------------------------------------------------------------
// THE CLASS IS NOT POLYMORPHIC.
// ---------------------------------------------------------------------------
// Read out of the shipped bytes, not inferred from the absent `??_7`: Begin's
// first store to the object is `MOV [RCX+0x80],RBX` and its second is
// `MOV [RCX+0x78],R9`; the +0x00 slot is written later, and it is written with
// `MOVSS [RDI],XMM6` (0x18011d393) — a FLOAT, not a vtable pointer. Every
// export of the class is `QEAA`, none `UEAA`.
//
// ---------------------------------------------------------------------------
// FIELD EVIDENCE
// ---------------------------------------------------------------------------
//   off    field                proven by                              retail
//   0x00   _scale               MOVSS XMM2,[RCX] then DIVSS in          0x18011d06e
//                               SetScissor and MULSS on the way out;
//                               Begin writes it from the float
//                               argument (`MOVSS [RDI],XMM6`)           0x18011d393
//   0x08   _currentSheet        AddLine compares the incoming sprite's  0x18011cc14
//                               sheet against [RCX+8] and calls Flush
//                               when they differ; FlushDeferred does
//                               the same per deferred record           0x18011c6d4
//   0x10   _draws               AddSpriteDraw appends with
//                               `LEA RCX,[RDI+0x10]` and passes
//                               `[RDI+0x18] + 1` as the new size        0x18011c9a6
//   0x20   _scissorX            SetScissor compares/overwrites all      0x18011d0c1
//   0x24   _scissorY            four; AddSpriteDraw culls against       0x18011d165
//   0x28   _scissorW            them scaled by _scale                   0x18011c92f
//   0x2c   _scissorH
//   0x30   _context             SetScissor reads `*(*(this+0x30)+0x28)` 0x18011d10d
//                               for the render target's w/h; Draw
//                               stores its Video::Context& there        0x180121cc8
//   0x38   _deferring           FlushDeferred returns immediately when  0x18011c6a6
//                               it is 0
//   0x40   _deferred            FlushDeferred walks it with size at     0x18011c6ba
//                               +0x48 and clears it through
//                               Array<SpriteDraw>::SetSize(0)           0x18011c72a
//   0x78   _clipRegion          AddSpriteDraw reads four ints through   0x18011c8b0
//                               it as {x, y, w, h}; Begin stores the
//                               4th argument there, and Draw passes
//                               `&UserInterface::_captureViewport`      0x180121cf3
//   0x80   _spriteCapture       Begin stores the 3rd argument;          0x18011d2eb
//                               AddSpriteDraw and SetScissor both
//                               call into it only when non-null        0x18011c89a
//
// THREE MORE ARRAYS, deliberately left opaque. UserInterface::Draw's epilogue
// frees FOUR buffers belonging to this object — at +0x10, +0x40, +0x58 and
// +0x68 (0x180122242, 0x18012228f, 0x1801222dc, 0x180122329, each zeroing the
// pointer and the qword after it). The first two are `_draws` and `_deferred`
// above. The other two, and the plain qword at +0x50, are NOT modelled: no
// body this lane ported reads them, and inventing a layout we have not proven
// is how a shared struct corrupts silently (PLAN §4.3 risk 1).
//
// WAVE 14 LANE DRAW ADDS A SECOND, INDEPENDENT WITNESS for the same four, and
// one new fact about +0x50. `Save::SaveLoad::Draw` (0x1800c2eb0) builds a
// SpriteContext on its STACK with the constructor fully inlined, so its
// prologue is a field-by-field initialiser list and its epilogue is the
// destructor:
//   * it zeroes BOTH halves of the arrays at +0x10, +0x40, +0x58 and +0x68
//     (0x1800c37e5, 0x1800c3807, 0x1800c3820, 0x1800c382e and their +8 pairs),
//     and frees exactly those four pointers on the way out in the order
//     +0x68, +0x58, +0x40, +0x10 (0x1800c3acd .. 0x1800c3af8);
//   * it NEVER WRITES +0x50. It takes the address (`LEA RAX,[RBP+0xa0]` at
//     0x1800c3815) for the MSVC exception-state record and nothing else. So the
//     qword at +0x50 is left as whatever the storage held — uninitialised on a
//     stack instance. A port of any body that constructs one must NOT zero it,
//     and an oracle must not compare it.
// The two arrays at +0x58 and +0x68 are therefore Collection::Array<T> with the
// usual {_data, _size, _capacity} shape; only their ELEMENT type is still
// unknown, which is why they stay inside `_opaque50` rather than being named.
//
// ---------------------------------------------------------------------------
// WHAT IS STILL HELD BACK, AND WHY THE WAVE 7 ARGUMENT ONLY HALF SURVIVED
// ---------------------------------------------------------------------------
// Wave 7 lane F held the whole class back with one argument: these methods
// "submit into the LIVE Video::Context / GPU vertex buffer", so a scratch copy
// of the object does not isolate a replay — the copy's `_context` still points
// at the one live context, and calling the DLL's body on it would submit a
// second, real, visible draw.
//
// THAT ARGUMENT IS CORRECT, AND IT IS AN ARGUMENT ABOUT `Flush`, NOT ABOUT THE
// THREE EXPORTS. Re-checked instruction by instruction for Wave 9
// (reference_holdback_blockers_are_stale_check_them_first.md — three stated
// blockers dissolved on re-checking in one session):
//
//   * `Flush` (0x18011ba40) is the only body in the class that touches the GPU.
//     It maps the vertex buffer through `_context`'s device vtable
//     (`CALL [RAX+0x70]` / `[RAX+0x78]` at 0x18011bc06 / 0x18011c5e3), memmoves
//     transformed vertices into it, and advances the write cursor. It is NOT
//     ported and NOT replayable, and the Wave 7 sentence is about it.
//   * `AddLine`, both `SetScissor`s, `AddSpriteDraw` and `FlushDeferred` do NOT
//     touch the GPU. They mutate the object's own scissor ints and its own
//     `Array<SpriteDraw>` batch, and they CALL `Flush`. That makes them
//     unreplayable for a different and much narrower reason — a second call
//     would append the record twice — but it does not make them unportable,
//     and the batch they build is ordinary memory this port owns end to end.
//
// So the class is no longer held back; `Flush` and `Begin` are. See the lane
// doc for the door arithmetic that decision produces.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../collection/Collection.h"

namespace UI {

// ============================================================
// UI::SpriteDraw — one queued sprite/line, stride 0x28.
//
// The stride is proven three times over: `ADD R15,0x28` in Flush's batch walk
// (0x18011c5a1) and in Draw's element walk (0x180121faf), and the `LEA
// RCX,[RAX+RAX*4] / SHL RCX,3` (== *0x28) index arithmetic in both
// Array<SpriteDraw> instantiations (0x180078573, 0x180078439).
//
// The FIELDS are read out of the two writers and the one reader agreeing:
//   * AddLine (0x18011cbe0) builds a record on its own stack at RSP+0x20 and
//     writes +0x00,+0x08,+0x0c,+0x10,+0x14,+0x18,+0x1c,+0x20,+0x24;
//   * UserInterface::Draw (0x180121890) builds the same record at RSP+0x28 for
//     the fullscreen fade and for the mouse cursor, writing the same offsets;
//   * AddSpriteDraw (0x18011c890) reads +0x0c/+0x14 as the X pair and
//     +0x10/+0x18 as the Y pair when it culls against the clip region, and
//     hands +0x00/+0x08 to SpriteCapture as a UI::Sprite.
//
// The x/y interleave is the one thing easy to get backwards, so it is nailed
// to a single instruction pair: 0x18011c8ac loads +0x0c and compares it
// against `clip.x`, and 0x18011c8c7 loads +0x10 and compares it against
// `clip.y`. +0x0c and +0x10 are therefore NOT a pair — they are the first
// coordinate of each axis.
// ============================================================
struct SpriteDraw {
    void*    sheet;         // +0x00  ExternalPtr<SpriteSheet>, the batch key
    int32_t  spriteIndex;   // +0x08  -1 is the empty sprite, never 0
    int32_t  x1;            // +0x0c
    int32_t  y1;            // +0x10
    int32_t  x2;            // +0x14
    int32_t  y2;            // +0x18
    float    rotation;      // +0x1c  0.0f selects the axis-aligned fast path
                            //        in both Flush and AddSpriteDraw
    uint32_t color;         // +0x20  ARGB, 0xffffffff is "untinted"
    uint8_t  isLine;        // +0x24  1 from AddLine, 0 from every other writer
    uint8_t  _pad25[3];     // +0x25  never written by the default-init arm
};
static_assert(sizeof(SpriteDraw) == 0x28, "SpriteDraw stride is 0x28");
static_assert(offsetof(SpriteDraw, sheet)       == 0x00, "sheet at +0x00");
static_assert(offsetof(SpriteDraw, spriteIndex) == 0x08, "spriteIndex at +0x08");
static_assert(offsetof(SpriteDraw, x1)          == 0x0c, "x1 at +0x0c");
static_assert(offsetof(SpriteDraw, y1)          == 0x10, "y1 at +0x10");
static_assert(offsetof(SpriteDraw, x2)          == 0x14, "x2 at +0x14");
static_assert(offsetof(SpriteDraw, y2)          == 0x18, "y2 at +0x18");
static_assert(offsetof(SpriteDraw, rotation)    == 0x1c, "rotation at +0x1c");
static_assert(offsetof(SpriteDraw, color)       == 0x20, "color at +0x20");
static_assert(offsetof(SpriteDraw, isLine)      == 0x24, "isLine at +0x24");

// ============================================================
// The clip region AddSpriteDraw culls against, 4 ints at SpriteContext+0x78.
//
// It is a Video::Viewport, spelled out here rather than included because
// UserInterface::Draw passes `&UserInterface::_captureViewport`, which
// runtime/types/ui/UserInterface.h deliberately models as 0x10 opaque bytes.
// The member ORDER is {x, y, w, h} and not {left, top, right, bottom}, proven
// by AddSpriteDraw ADDING +0x08 to +0x00 and +0x0c to +0x04 to obtain the
// far edges (0x18011c8bf, 0x18011c8de) — a left/top/right/bottom region would
// not need the addition.
// ============================================================
struct ClipRect {
    int32_t x;   // +0x00
    int32_t y;   // +0x04
    int32_t w;   // +0x08
    int32_t h;   // +0x0c
};
static_assert(sizeof(ClipRect) == 0x10, "ClipRect is 4 ints");

// ============================================================
// UI::SpriteContext — >= 0x88 bytes, no vtable.
// ============================================================
struct SpriteContext {
    float   _scale;                              // +0x00
    uint8_t _pad04[4];                           // +0x04
    void*   _currentSheet;                       // +0x08

    // The batch being accumulated for `_currentSheet`. Flush drains it.
    Collection::Array<SpriteDraw> _draws;        // +0x10, 0x10 bytes

    // Device-space scissor. SetScissor divides the incoming (point-space)
    // rect by _scale on the way in and multiplies the outgoing one by _scale
    // on the way out — see the port for the ONE branch where it does not.
    int32_t _scissorX;                           // +0x20
    int32_t _scissorY;                           // +0x24
    int32_t _scissorW;                           // +0x28
    int32_t _scissorH;                           // +0x2c

    void*   _context;                            // +0x30  Video::Context*
    uint8_t _deferring;                          // +0x38
    uint8_t _pad39[7];                           // +0x39

    // Records queued while _deferring is set — retail's deferred font pass.
    Collection::Array<SpriteDraw> _deferred;     // +0x40, 0x10 bytes

    uint8_t _opaque50[0x28];                     // +0x50 .. +0x77, see header

    const ClipRect* _clipRegion;                 // +0x78
    void*           _spriteCapture;              // +0x80  UI::SpriteCapture*, may be null
};

static_assert(offsetof(SpriteContext, _scale)         == 0x00, "_scale at +0x00");
static_assert(offsetof(SpriteContext, _currentSheet)  == 0x08, "_currentSheet at +0x08");
static_assert(offsetof(SpriteContext, _draws)         == 0x10, "_draws at +0x10");
static_assert(offsetof(SpriteContext, _scissorX)      == 0x20, "_scissorX at +0x20");
static_assert(offsetof(SpriteContext, _scissorY)      == 0x24, "_scissorY at +0x24");
static_assert(offsetof(SpriteContext, _scissorW)      == 0x28, "_scissorW at +0x28");
static_assert(offsetof(SpriteContext, _scissorH)      == 0x2c, "_scissorH at +0x2c");
static_assert(offsetof(SpriteContext, _context)       == 0x30, "_context at +0x30");
static_assert(offsetof(SpriteContext, _deferring)     == 0x38, "_deferring at +0x38");
static_assert(offsetof(SpriteContext, _deferred)      == 0x40, "_deferred at +0x40");
static_assert(offsetof(SpriteContext, _clipRegion)    == 0x78, "_clipRegion at +0x78");
static_assert(offsetof(SpriteContext, _spriteCapture) == 0x80, "_spriteCapture at +0x80");

// A LOWER BOUND, not a sizeof — see the header's SIZE section. Begin's store
// at [this+0x80] is what pins it.
static_assert(sizeof(SpriteContext) >= 0x88,
              "SpriteContext extends at least to +0x88 (Begin's store at +0x80)");

static_assert(sizeof(Collection::Array<SpriteDraw>) == 0x10,
              "Array<T> is data/size/capacity = 0x10");

} // namespace UI

// ============================================================================
// APPENDED: what `_opaque50` is, read out of UI::SpriteContext::AddFont
// (retail 0x18011ccb0) and the layout helper it calls.
// ============================================================================
//
// The section above declines to model +0x50..+0x77 because "no body this lane
// ported reads them". AddFont does, so the region is named here — but ONLY as
// offsets plus one element struct, because that is all two bodies prove.
//
// AddFont hands `this + 0x50` to `ComputeStringLayout` (0x180116fd0) as its
// ninth argument, and that callee's prologue writes exactly the three members
// this header already deduced from `Save::SaveLoad::Draw`'s inlined ctor:
//
//   0x180117005  LEA RCX,[RDI+0x18] ; CALL 0x18002fea0   -> clear array @ +0x68
//   0x180117010  LEA RCX,[RDI+0x08] ; CALL 0x180076a40   -> clear array @ +0x58
//   0x180117021  MOV [RDI],RBX                           -> zero  qword @ +0x50
//
// So `_opaque50` is one layout object: a qword, then two Collection::Array<T>.
// AddFont then walks the FIRST of those arrays — data at SpriteContext+0x58,
// `_size` at +0x60 — with a stride of 0x10 (`ADD RDI,0x10`, 0x18011ceba),
// reading three ints per element (0x18011cdea..0x18011cdf3):
//   +0x00 the sprite index, +0x04 an x, +0x08 a y.
//
// NOTHING ELSE IS MODELLED. The array at +0x68 holds one int per line break
// (0x180117191 appends the dword at ComputeStringLayout's own `[RSP+0xc0]`), the
// qword at +0x50 is only ever zeroed, and a glyph's fourth dword at +0x0c is
// never read by either body. Naming them would be a claim neither body supports.
// These are OFFSETS INTO `_opaque50`, not a replacement for it — the struct
// keeps its proven size either way.

namespace UI {

enum : size_t {
    kSpriteContextLayout     = 0x50,   // the layout object AddFont passes on
    kSpriteContextGlyphs     = 0x58,   // Array<FontGlyph>::_data
    kSpriteContextGlyphCount = 0x60,   // Array<FontGlyph>::_size
    kSpriteContextLineStarts = 0x68,   // Array<int32_t>, not read by any port
};

// One laid-out glyph. Stride 0x10, three ints read.
struct FontGlyph {
    int32_t spriteIndex;   // +0x00  index into the font's sheet; may be < 0
    int32_t x;             // +0x04  pen x, before the caller's x offset
    int32_t y;             // +0x08  pen y, before the caller's y offset
    int32_t _unread0C;     // +0x0c  never read by AddFont; see the note above
};
static_assert(sizeof(FontGlyph) == 0x10, "FontGlyph stride is 0x10");
static_assert(offsetof(FontGlyph, spriteIndex) == 0x00, "spriteIndex at +0x00");
static_assert(offsetof(FontGlyph, x)           == 0x04, "x at +0x04");
static_assert(offsetof(FontGlyph, y)           == 0x08, "y at +0x08");

// The offsets above must land inside the region the struct already reserves.
static_assert(offsetof(SpriteContext, _opaque50) == kSpriteContextLayout,
              "_opaque50 begins at the layout object");
static_assert(kSpriteContextLineStarts + 0x10 == offsetof(SpriteContext, _clipRegion),
              "the line-start array is the last member before _clipRegion");

} // namespace UI
