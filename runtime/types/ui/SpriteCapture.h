#pragma once
// ============================================================================
// runtime/types/ui/SpriteCapture.h — UI::SpriteCapture, 0x58 bytes
// ============================================================================
//
// WAVE 10 LANE UI4. Recovered from the retail DISASSEMBLY of five bodies, not
// from Ghidra's decompiled C — the C output of `AddScissor` reads as if the
// coordinate record it builds were a dead local, and it is not. See the STRIDE
// note below, which is the single fact that decides the whole layout.
//
//   ??0SpriteCapture@UI@@QEAA@XZ        0x1800d9140  ctor
//   ?BeginAddingSprites@…@QEAAXXZ       0x1800d9010  clears +0x50 and both arrays
//   ?AddSprite@…@QEAAXAEBVSprite@2@HHHHMI@Z  0x1800d8b20
//   ?AddScissor@…@QEAAXHHHH_N@Z         0x1800d8ef0
//   ?GetWidth@…@QEBAHXZ                 0x1800d9220  MOV EAX,[RCX+0x50]; RET
//   ?GetHeight@…@QEBAHXZ                0x1800d9210  MOV EAX,[RCX+0x54]; RET
//
// A SpriteCapture is the recording buffer behind the dev-only sprite-capture
// tool (`SpriteCaptureUI`, exe 0x1400792b0). It records a PARALLEL PAIR of
// streams, one entry per drawn thing:
//
//   _sprites[i]  the UI::Sprite that was drawn (sheet + index)
//   _entries[i]  where and how it was drawn, plus which sheet it came from
//
// plus a de-duplicated list of the sheets seen, `_sheets`, which
// `_entries[i].sheetIndex` indexes. A SCISSOR is recorded in the same two
// streams with `_sprites[i] = Sprite{null, -1}` and a NEGATIVE sheetIndex
// (-1 = normal scissor, -2 = full-screen), which is how a reader tells the two
// kinds of entry apart.
//
// ---------------------------------------------------------------------------
// THE STRIDE FACT, and why Ghidra's C is misleading here
// ---------------------------------------------------------------------------
// Both AddSprite and AddScissor end in two calls:
//
//     CALL 0x18004bfc0   with RCX = this+0x40      ; Array<T>::SetSize(n, fill)
//     CALL 0x18004bde0   with RCX = this+0x30
//
// 0x18004bfc0 is already ported in runtime/src/collection/array.cpp as
// `kSpec_Fill16` — stride 0x10 — so `this+0x40` is `Array<UI::Sprite>`.
// 0x18004bde0 computes its byte count as `LEA RCX,[RAX+RAX*4]; SHL RCX,3`
// (0x18004be38) = **element stride 0x28**, NOT 4.
//
// That number is what makes the 0x28-byte block each body builds on its stack
// the ELEMENT it appends, rather than the dead local it looks like in the
// decompiled C. Read as stride 4 the block is unreferenced and both functions
// throw away every coordinate they were handed; read as stride 0x28 it is the
// entry, and every field lands somewhere. The disassembly settles it and the
// decompiler does not — reference_ghidra_c_output_vs_disassembly.md.
//
// ---------------------------------------------------------------------------
// `Entry::name` IS OWNED BY THE ARRAY — and how that was nearly recorded wrong
// ---------------------------------------------------------------------------
// `Entry::name` is a System::String (a refcounted char*). Both bodies build
// the entry holding an OWNED reference, hand it to SetSize, and then release
// the local's reference on the way out. Read only that far, the net change is
// zero and the stored copy is held by nobody — which is exactly what this
// header said in its first draft, complete with a paragraph calling it a
// shipped defect that the port reproduces faithfully.
//
// It was WRONG, and what caught it was the probe's REFCOUNT LEDGER: the count
// on a shared name rose by +1 per implementation, symmetrically, where the
// theory predicted zero. 0x18004bde0 does not copy elements bitwise. Its
// element copier (0x18004be97..0x18004bed0) COPY-CONSTRUCTS:
//
//     MOV EAX,[RDX]         MOV [RCX],EAX            ; sheetIndex
//     MOV [RCX+0x8],R12                              ; name = null
//     MOV RAX,[RDX+0x8]     TEST RAX,RAX   JZ …
//     MOV [RCX+0x8],RAX     INC dword ptr [RAX-0x4]  ; ++refcount   <-- HERE
//     … +0x10 +0x14 +0x18 +0x1c +0x20 +0x24 as dwords …
//
// So the array element takes its own reference and retail's accounting is
// exactly right. Two things worth keeping from that: a "retail is buggy"
// conclusion deserves the same scepticism as any other (CLAUDE.md), and the
// instrument whose number disagreed with the theory was believed over the
// theory.
//
// That listing also CONFIRMS `pad04` from a second, independent witness: the
// copier writes +0x00, jumps to +0x08, and never touches +0x04..+0x07.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace UI {

// The array header every Collection::Array<T> uses. Same shape as
// runtime/src/collection/array.cpp's ArrayHdr, spelled again here so this
// header stays standalone.
struct CaptureArray {
    void*   data;       // +0x00
    int32_t size;       // +0x08
    int32_t capacity;   // +0x0c
};
static_assert(sizeof(CaptureArray) == 0x10, "CaptureArray must be 0x10 bytes");

// One recorded draw or scissor. Stride 0x28 — proved by 0x18004bde0's
// `LEA RCX,[RAX+RAX*4]; SHL RCX,3`, see the banner.
//
// Field offsets read off AddScissor's stores at 0x1800d8f82..0x1800d8f96 and
// AddSprite's at 0x1800d8caa..0x1800d8e54; the two agree.
struct CaptureEntry {
    int32_t sheetIndex;   // +0x00  index into _sheets, or -1/-2 for a scissor
    int32_t pad04;        // +0x04  never written by either body
    char*   name;         // +0x08  System::String — see THE DANGLING NAME above
    int32_t x1;           // +0x10
    int32_t y1;           // +0x14
    int32_t x2;           // +0x18
    int32_t y2;           // +0x1c
    float   rotation;     // +0x20
    uint32_t color;       // +0x24
};
static_assert(sizeof(CaptureEntry) == 0x28, "CaptureEntry must be 0x28 bytes");
static_assert(offsetof(CaptureEntry, sheetIndex) == 0x00, "");
static_assert(offsetof(CaptureEntry, name)       == 0x08, "");
static_assert(offsetof(CaptureEntry, x1)         == 0x10, "");
static_assert(offsetof(CaptureEntry, y1)         == 0x14, "");
static_assert(offsetof(CaptureEntry, x2)         == 0x18, "");
static_assert(offsetof(CaptureEntry, y2)         == 0x1c, "");
static_assert(offsetof(CaptureEntry, rotation)   == 0x20, "");
static_assert(offsetof(CaptureEntry, color)      == 0x24, "");

// `pad04` is the ONE window in a CaptureEntry that neither body writes: the
// stack block is filled field by field and +0x04 is never stored. An oracle
// that memcmp's two entries must skip it or it reports a divergence on
// whatever the two stacks happened to hold —
// reference_oracle_must_not_compare_unwritten_bytes.md.
inline constexpr size_t kCaptureEntryPadOff = 0x04;
inline constexpr size_t kCaptureEntryPadLen = 0x04;

// UI::SpriteCapture (0x58). Derives Object::External (0x20).
struct SpriteCapture {
    void*        vtable;      // +0x00
    void*        ext08;       // +0x08  Object::External
    void*        ext10;       // +0x10  Object::External
    void*        ext18;       // +0x18  Object::External
    CaptureArray _sheets;     // +0x20  Array<ExternalPtr<SpriteSheet>>, stride 8
    CaptureArray _entries;    // +0x30  Array<CaptureEntry>,             stride 0x28
    CaptureArray _sprites;    // +0x40  Array<UI::Sprite>,               stride 0x10
    int32_t      _width;      // +0x50  GetWidth
    int32_t      _height;     // +0x54  GetHeight
};
static_assert(sizeof(SpriteCapture) == 0x58, "SpriteCapture must be 0x58 bytes");
static_assert(offsetof(SpriteCapture, _sheets)  == 0x20, "");
static_assert(offsetof(SpriteCapture, _entries) == 0x30, "");
static_assert(offsetof(SpriteCapture, _sprites) == 0x40, "");
static_assert(offsetof(SpriteCapture, _width)   == 0x50, "");
static_assert(offsetof(SpriteCapture, _height)  == 0x54, "");

}  // namespace UI
