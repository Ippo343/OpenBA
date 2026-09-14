// ============================================================================
// runtime/src/ui/spritecontext_add.cpp — UI::SpriteContext's four public "add"
//                                        bodies (the per-frame menu/HUD path)
// ============================================================================
//
// runtime/src/ui/spritecontext.cpp already owns this class's scissor/line half
// and its two internals; runtime/src/ui/spritecontext_flush.cpp owns the batch
// drain. What was still missing is the half that every dialog, every label and
// every HUD panel actually calls:
//
//   | Retail      | Mangled name                                              |
//   |-------------|-----------------------------------------------------------|
//   | 0x18011cf00 | ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHI@Z       |
//   | 0x18011d3c0 | ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z     |
//   | 0x18011cb10 | ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHIM@Z    |
//   | 0x18011ccb0 | ?AddFont@SpriteContext@UI@@QEAAXV?$ExternalPtr@VFont@UI@@@Object@@AEBVString@System@@_NHW4Justify@Font@2@HHHI@Z |
//
// Every fact below is read out of `disassemble_function` on retail
// Runtime-x64.dll, not out of Ghidra's decompiled C
// (reference_ghidra_c_output_vs_disassembly.md). The three AddSprite bodies are
// byte-for-byte the same shape and differ only in where the record's four
// coordinates and its rotation come from, which is why they are written here as
// three separate bodies rather than three wrappers: retail does not share them
// either, and a shared helper would invent a call retail does not make.
//
// ---------------------------------------------------------------------------
// NO HIDDEN RETURN SLOT ON ANY OF THE FOUR
// ---------------------------------------------------------------------------
// The `X` in each mangling says void, and — because the mangling alone is not
// the proof (reference_hidden_retslot_wild_write.md) — the shipped bytes agree:
// all three AddSprite bodies begin `MOV RBX,[RDX]` and immediately treat RDX as
// the `UI::Sprite const&`, and AddFont's first instruction is
// `MOV [RSP+0x10],RDX` — spilling RDX into the home slot of ARGUMENT 1, which it
// later reloads as the `ExternalPtr<Font>` (0x18011cd69). If RDX were a return
// slot, argument 1 would be in R8. None of the four writes RAX before RET.
//
// ---------------------------------------------------------------------------
// THE ARGUMENT HOME SLOTS, SPELLED OUT
// ---------------------------------------------------------------------------
// At the CALLEE's entry [RSP] is the return address, [RSP+0x08] is argument 0's
// home (`this`), and the first STACK argument — argument 4 — is at [RSP+0x28],
// then +0x30, +0x38, ... Each prologue then shifts that window:
//
//   AddSprite x3   4 home stores, PUSH R14, SUB RSP,0x50   -> window at +0x80
//   AddFont        PUSH RSI, PUSH R14, SUB RSP,0xa8        -> window at +0xe0
//
// so AddSprite's argument 4 is `[RSP+0x80]` (0x18011cfc3, 0x18011d431,
// 0x18011cb87) and AddFont's is `[RSP+0xe0]` (0x18011ccfb). That arithmetic is
// what pins every parameter list in the table above; nothing here is inferred
// from a name.
//
// THE FLOAT PARAMETER IN `...IM@Z` IS A STACK ARGUMENT, NOT AN XMM ONE. It is
// parameter 8 by position, and the Microsoft x64 ABI only registerises the first
// four — so the shipped body reads it with `MOVSS XMM0,[RSP+0x98]`
// (0x18011cb64), the eighth slot, and a C++ declaration that lists it eighth
// puts it in exactly that place.
//
// ---------------------------------------------------------------------------
// THE SHEET REBIND IS THE SAME FIVE INSTRUCTIONS IN ALL THREE AddSprites
// ---------------------------------------------------------------------------
//   MOV RAX,[RCX+0x8] ; CMP RAX,RBX ; JZ done
//   TEST RAX,RAX ; JNZ flush ; MOV [RCX+0x8],RBX ; JMP done
//   flush: CALL 0x18011ba40 ; MOV [RDI+0x8],RBX
// i.e. a batch is flushed only when a DIFFERENT non-null sheet was already
// bound. It is identical to the one runtime/src/ui/spritecontext.cpp's
// `AddLine` already ports, and identical to the early-outs in front of it: the
// sprite index test is `CMP dword [RDX+8],0 / JL` — a SIGNED `< 0`, not
// `== -1`.
//
// ---------------------------------------------------------------------------
// WHAT AddFont ACTUALLY DOES, AND THE ONE NEW LAYOUT FACT IT PINS
// ---------------------------------------------------------------------------
// AddFont does not rasterise anything. It asks the font to lay the string out,
// then replays the resulting glyph list through `AddSprite` (the point overload
// ported in this same file) or, while the context is deferring, straight into
// `_deferred`.
//
// The layout call is `ComputeStringLayout` (0x180116fd0, unexported in retail;
// the name is the profile build's), and its NINTH argument is `this + 0x50`.
// That is the previously-opaque region `runtime/types/ui/SpriteContext.h` calls
// `_opaque50`, and ComputeStringLayout's own prologue names its shape:
//
//   0x180117005  LEA RCX,[RDI+0x18] ; CALL 0x18002fea0   -> clear array @ +0x68
//   0x180117010  LEA RCX,[RDI+0x08] ; CALL 0x180076a40   -> clear array @ +0x58
//   0x180117021  MOV [RDI],RBX                           -> zero  qword @ +0x50
//
// which is exactly the three members SpriteContext.h deduced from
// `Save::SaveLoad::Draw`'s inlined constructor, seen from the other side. So
// `_opaque50` is one `UI::Font::Layout` object, and AddFont then walks the array
// at +0x58 with `_size` at +0x60 and a stride of 0x10 (`ADD RDI,0x10` at
// 0x18011ceba), reading three ints out of each element: the sprite index at
// +0x00, an x at +0x04 and a y at +0x08. Those facts are appended to
// SpriteContext.h as `UI::FontGlyph` and the `kSpriteContextLayout*` offsets;
// nothing else about the Layout object is modelled, because nothing else is
// read here.
//
// TWO EARLY-OUTS BRACKET THE LAYOUT CALL, and they are vertical scissor culls:
//
//   before (0x18011ccd7):  (y - font[+0x50])                > (scissorY + scissorH) * scale
//   after  (0x18011cd78):  (scissorY * scale)               > (y + outAscent - font[+0x50])
//
// Both are `COMISS` + `JA`, so an unordered compare falls THROUGH rather than
// returning — and `a > b` is false for a NaN in C++ too, so the plain `>` below
// reproduces that without a special case. The font pointer is dereferenced
// before either test with no null check; that is retail's, and it stays
// (CLAUDE.md — do not "fix" a faithful crash with a null guard).
//
// The two `int*` out-parameters handed to ComputeStringLayout are written into
// AddFont's OWN home-slot area (`[RSP+0xc0]`, the low half of `this`'s home
// slot, and `[RSP+0x100]`, argument 8's home slot after its value has already
// been copied into R14D). They are scratch, they are pre-zeroed by retail
// before the call, and only the second is read afterwards. Two plain locals
// reproduce that exactly.
//
// ---------------------------------------------------------------------------
// THE DOOR ARITHMETIC
// ---------------------------------------------------------------------------
// CLOSES four export doors (the four in the table) and four address doors.
// OPENS exactly ONE: `ComputeStringLayout` at 0x180116fd0, which nothing in the
// tree has ported. It is reached through `ResolveAddr(Module::Runtime, …)`,
// which consults the port registry first
// (reference_resolveaddr_consults_port_registry_first.md), so the day it is
// ported these bodies pick it up with no edit here.
// Every other callee is already satisfied:
//   0x18011ba40  SpriteContext::Flush         — ported, spritecontext_flush.cpp
//   0x18011c890  SpriteContext::AddSpriteDraw — ported, spritecontext.cpp
//   0x180078520  Array<SpriteDraw> append     — ported, collection/array.cpp
//   0x18011cf00  AddSprite (point overload)   — ported HERE, and called as a
//                direct C++ call rather than through the registry, exactly as
//                spritecontext.cpp's FlushDeferred calls its own AddSpriteDraw.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "../../types/ui/Sprite.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/ui/SpriteSheet.h"
#include "../collection/array_internal.h" // ENDGAME B.3.2 step 2 — kSpec_SpriteDraw

// Same forward declarations, same spelling, as spritecontext.cpp and
// spritecontext_flush.cpp — the linker checks them against standalone/resolve.h.
// See spritecontext.cpp's banner for why resolve.h is not included from here.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

using ::Sprite;          // Sprite.h declares it at GLOBAL scope, not in UI
using UI::FontGlyph;
using UI::SpriteContext;
using UI::SpriteDraw;
using ::SpriteInfo;      // SpriteSheet.h declares both at GLOBAL scope too
using ::SpriteSheet;

// ---- the mangled names, once -----------------------------------------------
const char* const kAddSpritePoint =
    "?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHI@Z";
const char* const kAddSpriteRect =
    "?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z";
const char* const kAddSpriteRectRotated =
    "?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHIM@Z";
const char* const kAddFont =
    "?AddFont@SpriteContext@UI@@QEAAXV?$ExternalPtr@VFont@UI@@@Object@@"
    "AEBVString@System@@_NHW4Justify@Font@2@HHHI@Z";

// ---- the internals this file reaches ---------------------------------------
typedef void (*FlushFn)(void* self);
typedef void (*AddSpriteDrawFn)(void* self, const void* draw);
// ComputeStringLayout (0x180116fd0). Read by address through the registry-first
// resolver. This line used to say "NOT PORTED — this file's one opened door";
// that was true when it was written and stopped being true in the same batch:
// runtime/src/ui/font.cpp ports and registers it, so the call lands on ours.
// Argument order is read off the nine stores at 0x18011ccfb..0x18011cd61, NOT
// guessed from the callee's name: note that the `Justify` and the sixth AddFont
// parameter arrive at the callee in the OPPOSITE order to the one AddFont
// declares them in.
typedef void (*ComputeStringLayoutFn)(void* font, const void* text, bool multiline,
                                      int32_t layoutA, int32_t layoutB, int32_t justify,
                                      int32_t* outWidth, int32_t* outAscent,
                                      void* layout);

// Each address is spelled as a LITERAL inside `ResolveAddr(Module::Runtime, …)`
// exactly once, because that literal is what keeps the door VISIBLE to
// `tools/addr_door_audit.py`, whose call-site half matches a literal and nothing
// else (reference_literal_grep_blind_to_variables.md). None of these caches a
// null — a miss during early boot must not freeze the answer for the process.
inline FlushFn TheFlush() {
    static FlushFn f = nullptr;
    if (!f) f = reinterpret_cast<FlushFn>(ResolveAddr(Module::Runtime, 0x18011ba40));
    return f;
}
inline AddSpriteDrawFn TheAddSpriteDraw() {
    static AddSpriteDrawFn f = nullptr;
    if (!f) f = reinterpret_cast<AddSpriteDrawFn>(ResolveAddr(Module::Runtime, 0x18011c890));
    return f;
}
inline ComputeStringLayoutFn TheComputeStringLayout() {
    static ComputeStringLayoutFn f = nullptr;
    if (!f) f = reinterpret_cast<ComputeStringLayoutFn>(
        ResolveAddr(Module::Runtime, 0x180116fd0));
    return f;
}

template <typename T>
inline T& Field(void* p, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

// UI::Font — only two fields are touched by AddFont, and neither is named
// beyond what one body proves. +0x50 is subtracted from the incoming y in both
// scissor tests and is what the layout's second out-int is measured against, so
// it is the glyph origin's vertical bias; +0x60 is the ExternalPtr<SpriteSheet>
// every glyph is drawn from (it is handed straight to AddSprite as the sprite's
// sheet). There is no runtime/types/ui/Font.h to extend, and one body is not
// enough to write one.
enum : size_t {
    kFontOriginY = 0x50,
    kFontSheet   = 0x60,
};

// The sheet rebind retail performs at the head of all three AddSprite bodies.
// 0x18011cf3c..0x18011cf59 (and the identical blocks at 0x18011d3f4,
// 0x18011cb44). Flushing only happens when a DIFFERENT non-null sheet is bound.
inline void BindSheet(SpriteContext* self, void* sheet) {
    if (self->_currentSheet == sheet) return;
    if (self->_currentSheet == nullptr) {
        self->_currentSheet = sheet;
        return;
    }
    TheFlush()(self);
    self->_currentSheet = sheet;
}

// One entry of the bound sheet's sprite table, or {0,0} for a negative index.
// Retail inlines this twice — once in the point overload (0x18011cf59..0x18011cf95)
// and once in AddFont's deferred arm (0x18011ce1e..0x18011ce54) — and both
// copies test the index with `TEST EAX,EAX / JNS`, a SIGNED test on the 32-bit
// value, reading the table only when it is non-negative. The two reads are
// EMITTED SEPARATELY, each re-loading the sheet and re-doing the `idx * 0x38`
// index arithmetic; that is not reproduced instruction for instruction because
// nothing observes it, but the guard and the zero default are.
struct SpriteExtent { int32_t width; int32_t height; };

inline SpriteExtent ExtentOf(void* sheetRaw, int32_t spriteIndex) {
    SpriteExtent e = { 0, 0 };
    if (spriteIndex < 0) return e;
    const SpriteInfo* table = static_cast<SpriteSheet*>(sheetRaw)->m_sprites;
    const SpriteInfo& info = table[static_cast<int64_t>(spriteIndex)];
    e.width  = info.width;    // entry +0x1c
    e.height = info.height;   // entry +0x20
    return e;
}

}  // namespace

// ============================================================================
// The ports
// ============================================================================

// ---- 0x18011cf00  ?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHI@Z -----
//
// The POINT overload: place the sprite at (x, y) at its natural size. The far
// corner is the sheet entry's width/height added to the near corner, which is
// the only thing that distinguishes this body from the rect overload below.
//
// The record retail builds at RSP+0x20 (0x18011cf95..0x18011cfd8) is emitted in
// a scrambled order — +0x00, rotation, x2, spriteIndex, y2, x1, y1, isLine,
// color — which is easy to misread from the decompiler. The offsets, not the
// order, are what is reproduced here.
void RtUi_SpriteContext_AddSprite_Point(void* selfRaw, const void* spriteRaw,
                                        int32_t x, int32_t y, uint32_t color) {
    auto*       self   = static_cast<SpriteContext*>(selfRaw);
    const auto* sprite = static_cast<const Sprite*>(spriteRaw);

    // 0x18011cf29 / 0x18011cf32: two independent early returns, the second a
    // SIGNED `< 0` on the index.
    void* sheet = sprite->spriteSheet;
    if (sheet == nullptr) return;
    if (sprite->spriteIndex < 0) return;

    BindSheet(self, sheet);

    const int32_t      idx = sprite->spriteIndex;
    const SpriteExtent ext = ExtentOf(sheet, idx);

    SpriteDraw rec{};
    rec.sheet       = sheet;
    rec.spriteIndex = idx;
    rec.x1          = x;
    rec.y1          = y;
    rec.x2          = ext.width  + x;   // `LEA EAX,[R9 + R14*0x1]` 0x18011cfa5
    rec.y2          = ext.height + y;   // `LEA EAX,[R10 + RBP*0x1]` 0x18011cfb6
    rec.rotation    = 0.0f;             // XORPS then MOVSS, 0x18011cf98
    rec.color       = color;
    rec.isLine      = 0;                // the literal 0 byte, 0x18011cfd3

    TheAddSpriteDraw()(self, &rec);
}

// ---- 0x18011d3c0  ?AddSprite@…@QEAAXAEBVSprite@2@HHHHI@Z -------------------
//
// The RECT overload: both corners are given, so no sheet-table read happens at
// all. Identical to the point overload in every other respect.
void RtUi_SpriteContext_AddSprite_Rect(void* selfRaw, const void* spriteRaw,
                                       int32_t x1, int32_t y1,
                                       int32_t x2, int32_t y2, uint32_t color) {
    auto*       self   = static_cast<SpriteContext*>(selfRaw);
    const auto* sprite = static_cast<const Sprite*>(spriteRaw);

    void* sheet = sprite->spriteSheet;
    if (sheet == nullptr) return;
    if (sprite->spriteIndex < 0) return;

    BindSheet(self, sheet);

    SpriteDraw rec{};
    rec.sheet       = sheet;
    rec.spriteIndex = sprite->spriteIndex;
    rec.x1          = x1;
    rec.y1          = y1;
    rec.x2          = x2;
    rec.y2          = y2;
    rec.rotation    = 0.0f;
    rec.color       = color;
    rec.isLine      = 0;

    TheAddSpriteDraw()(self, &rec);
}

// ---- 0x18011cb10  ?AddSprite@…@QEAAXAEBVSprite@2@HHHHIM@Z ------------------
//
// The ROTATED rect overload — the rect overload with the record's `rotation`
// taken from the eighth parameter instead of being zeroed. `rotation` is in
// DEGREES: SpriteContext::Flush multiplies it by 0x3c8efa35 (pi/180) before the
// sin/cos pair (see spritecontext_flush.cpp).
//
// The float arrives on the stack, at `[RSP+0x98]` — the eighth argument slot —
// and is copied straight through with MOVSS. It is NOT converted, NOT compared,
// and NOT normalised here, so a NaN or an infinity reaches Flush intact; that is
// what selects Flush's rotated path for a NaN.
void RtUi_SpriteContext_AddSprite_RectRotated(void* selfRaw, const void* spriteRaw,
                                              int32_t x1, int32_t y1,
                                              int32_t x2, int32_t y2,
                                              uint32_t color, float rotation) {
    auto*       self   = static_cast<SpriteContext*>(selfRaw);
    const auto* sprite = static_cast<const Sprite*>(spriteRaw);

    void* sheet = sprite->spriteSheet;
    if (sheet == nullptr) return;
    if (sprite->spriteIndex < 0) return;

    BindSheet(self, sheet);

    SpriteDraw rec{};
    rec.sheet       = sheet;
    rec.spriteIndex = sprite->spriteIndex;
    rec.x1          = x1;
    rec.y1          = y1;
    rec.x2          = x2;
    rec.y2          = y2;
    rec.rotation    = rotation;   // MOVSS from [RSP+0x98], 0x18011cb64/0x18011cb7d
    rec.color       = color;
    rec.isLine      = 0;

    TheAddSpriteDraw()(self, &rec);
}

// ---- 0x18011ccb0  ?AddFont@SpriteContext@UI@@QEAAX… ------------------------
//
// Lay a string out through the font, then submit one sprite per glyph. See the
// file banner for the two scissor culls, the layout object at `this + 0x50` and
// the argument-order swap on the way into ComputeStringLayout.
//
// `layoutA` and `layoutB` are deliberately NOT named beyond their position.
// They are AddFont's fourth and sixth parameters, they are forwarded to
// ComputeStringLayout as its fourth and fifth, and nothing in either body says
// what they mean — naming them "width" and "height" would be a claim this port
// cannot support (one body is not enough to name a field).
void RtUi_SpriteContext_AddFont(void* selfRaw, void* fontRaw, const void* text,
                                bool multiline, int32_t layoutA, int32_t justify,
                                int32_t layoutB, int32_t x, int32_t y,
                                uint32_t color) {
    auto* self = static_cast<SpriteContext*>(selfRaw);

    // --- the first vertical cull, 0x18011ccd7..0x18011ccf5 ------------------
    // `font` is dereferenced here with no null test. Faithful.
    const int32_t top = y - Field<int32_t>(fontRaw, kFontOriginY);
    const float   scissorBottom =
        static_cast<float>(self->_scissorY + self->_scissorH) * self->_scale;
    if (static_cast<float>(top) > scissorBottom) return;

    // --- the layout, 0x18011ccfb..0x18011cd64 -------------------------------
    // Retail zeroes both out-ints before the call even though the callee's own
    // first act is to zero them (0x180117034/0x180117037). Kept.
    int32_t outWidth  = 0;
    int32_t outAscent = 0;
    void*   layout    = reinterpret_cast<uint8_t*>(self) + UI::kSpriteContextLayout;
    TheComputeStringLayout()(fontRaw, text, multiline, layoutA, layoutB, justify,
                             &outWidth, &outAscent, layout);

    // --- the second vertical cull, 0x18011cd69..0x18011cd98 -----------------
    // The font pointer is reloaded from its home slot; the sheet is read out of
    // it BEFORE the compare and is what every glyph below is drawn from.
    void*         sheet  = Field<void*>(fontRaw, kFontSheet);
    const int32_t bottom = (outAscent - Field<int32_t>(fontRaw, kFontOriginY)) + y;
    const float   scissorTop = static_cast<float>(self->_scissorY) * self->_scale;
    if (scissorTop > static_cast<float>(bottom)) return;

    // --- the glyph walk, 0x18011cda8..0x18011cec1 ---------------------------
    // `_deferring`, the glyph array's data pointer and its size are ALL re-read
    // on every iteration (0x18011cde0, 0x18011cde4, 0x18011cebe). That is kept
    // rather than hoisted: both arms below can reallocate an array, and hoisting
    // would bake in an assumption retail does not make.
    for (int32_t i = 0; i < Field<int32_t>(self, UI::kSpriteContextGlyphCount); ++i) {
        const FontGlyph* glyphs =
            Field<const FontGlyph*>(self, UI::kSpriteContextGlyphs);
        const FontGlyph& g = glyphs[i];

        if (self->_deferring == 0) {
            // 0x18011cdea..0x18011ce14. A bare {sheet, index} UI::Sprite on the
            // stack — +0x0c is never written, exactly as Sprite.h records — then
            // the point overload above, which does its own sheet rebind and its
            // own sheet-table read.
            Sprite s;
            s.spriteSheet = sheet;
            s.spriteIndex = g.spriteIndex;
            RtUi_SpriteContext_AddSprite_Point(self, &s, g.x + x, g.y + y, color);
            continue;
        }

        // 0x18011ce1e..0x18011ceb3. The deferred arm builds the SpriteDraw
        // itself and appends straight to `_deferred`, bypassing both the sheet
        // rebind and AddSpriteDraw's scissor cull — SpriteContext::FlushDeferred
        // replays it through AddSpriteDraw later. Note it does NOT re-check the
        // index for the record: a negative index yields a zero extent and is
        // still queued.
        const SpriteExtent ext = ExtentOf(sheet, g.spriteIndex);

        SpriteDraw rec{};
        rec.sheet       = sheet;
        rec.spriteIndex = g.spriteIndex;
        rec.x1          = g.x + x;
        rec.y1          = g.y + y;
        rec.x2          = (g.x + ext.width)  + x;   // LEA / ADD, 0x18011ce84
        rec.y2          = (g.y + ext.height) + y;   // LEA / ADD, 0x18011cea3
        rec.rotation    = 0.0f;
        rec.color       = color;
        rec.isLine      = 0;

        // retail 0x180078520 — kSpec_SpriteDraw, NON-TRIVIAL (partial-write
        // assign), same row spritecontext.cpp's AddSpriteDraw bridges through.
        rtcoll::AsArray<&rtcoll::kSpec_SpriteDraw>(&self->_deferred)
            ->SetSizeFill(self->_deferred._size + 1, &rec);
    }
}

// ============================================================================
int RtUiSpriteContextAddRegister() {
    int n = 0;

    // BOTH doors for every body, unconditionally — no kill switch and no env
    // gate (DIRECTION.md §2: a port registers unconditionally). The address
    // registration is what redirects the DLL's OWN internal callers, of which
    // AddFont's call to the point overload at 0x18011cf00 is one.
    RegisterRuntimeExport(kAddSpritePoint,
                          (void*)&RtUi_SpriteContext_AddSprite_Point);
    RegisterRuntimeAddr(0x18011cf00ull,
                        (void*)&RtUi_SpriteContext_AddSprite_Point); ++n;

    RegisterRuntimeExport(kAddSpriteRect,
                          (void*)&RtUi_SpriteContext_AddSprite_Rect);
    RegisterRuntimeAddr(0x18011d3c0ull,
                        (void*)&RtUi_SpriteContext_AddSprite_Rect); ++n;

    RegisterRuntimeExport(kAddSpriteRectRotated,
                          (void*)&RtUi_SpriteContext_AddSprite_RectRotated);
    RegisterRuntimeAddr(0x18011cb10ull,
                        (void*)&RtUi_SpriteContext_AddSprite_RectRotated); ++n;

    RegisterRuntimeExport(kAddFont, (void*)&RtUi_SpriteContext_AddFont);
    RegisterRuntimeAddr(0x18011ccb0ull, (void*)&RtUi_SpriteContext_AddFont); ++n;

    return n;
}
