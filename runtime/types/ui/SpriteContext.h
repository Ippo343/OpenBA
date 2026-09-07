#pragma once
// UI::SpriteContext -- binary-compatible struct definition (PARTIAL, UNPORTED).
// Runtime-x64.dll class. Full sizeof NOT proven -- see the hold-back note.
//
// Decompiled from (Wave 7 lane F, 2026-09-07, Runtime-x64.dll, image base
// 0x180000000), to support the hold-back argument in
// docs/runtime_port/WAVE7_LANE_F.md -- NONE of SpriteContext's methods are
// ported by this lane. This header exists because the brief asks this lane
// to own it, and because the next reader who reconsiders AddLine/SetScissor
// should not have to re-read the disassembly from zero.
//
//   AddLine:              0x18011cbe0 (?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z)
//   SetScissor(Viewport): 0x18011d050 (?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z)
//   SetScissor(4 ints):   0x18011d010 (?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z)
//                         -- a two-line forwarder into the Viewport overload.
//
// WHY NEITHER IS PORTED (see WAVE7_LANE_F.md §2 for the full argument):
// both reach unexported, unreversed internal helpers -- `Flush`,
// `FlushDeferredFonts`, `AddSpriteDraw`, `SpriteCapture::AddScissor` -- that
// submit into the LIVE Video::Context / GPU vertex buffer this SpriteContext
// wraps, not into memory the object itself owns. A scratch copy of the
// SpriteContext object does not isolate a replay: the copy's Video::Context
// pointer (see +0x30 below) still points at the one live context, so calling
// the DLL's body on a "scratch" object would submit a second, real, visible
// draw. That is a materially different hazard from Wave 7 lane F's other
// mutators (SpriteSheet::AddSpriteInfo's shared-string refcount, which only
// leaks memory) and is why this class is held back rather than diffed.
//
// FIELDS THIS LANE HAPPENED TO READ WHILE MAKING THAT ARGUMENT (not a survey,
// not asserted with static_assert -- nothing here is load-bearing for a port):
//
//   +0x00  float     -- a pixels-per-point-style scale; SetScissor divides
//                        incoming ints by it and multiplies outgoing ones,
//                        so it is applied going IN and undone going OUT.
//   +0x08  SpriteSheet* -- "current sheet" cursor; AddLine calls Flush(this)
//                        and rebinds it whenever a line's sheet differs from
//                        the cursor, i.e. sprite batches are grouped by sheet.
//   +0x20  int32 x, +0x24 int32 y, +0x28 int32 w, +0x2C int32 h -- the
//                        current scissor rect, in the post-scale (device)
//                        space. SetScissor reads and overwrites all four.
//   +0x30  pointer -- dereferenced twice as `*(*(this+0x30)+0x28)+0x10/+0x14`
//                        to read a width/height pair inside SetScissor's
//                        SpriteCapture branch; almost certainly the bound
//                        Video::RenderBuffer or a Video::Context, not
//                        reversed further because this lane does not port
//                        through it.
//   +0x80  SpriteCapture* -- null when no screenshot/capture UI is active;
//                        AddScissor is called on it only when non-null.
//
// AddLine's SpriteDraw record, built on the caller's stack and handed to the
// unexported `AddSpriteDraw(SpriteContext*, SpriteDraw*)` -- reversed only
// far enough to show it copies the Sprite by value (2 qwords) and none of it
// is needed further because AddSpriteDraw itself is out of scope:
//   +0x00..0x0B  Sprite (2 qwords, see Sprite.h)   -- wait, see note below
//   +0x0C        int32 flags (0 for AddLine)
//   +0x10        int32 x1
//   +0x14        int32 y1  (param_2 in AddLine's mangled order, i.e. AddLine's
//                            SECOND coordinate pair, not necessarily "x2,y2")
//   +0x18        int32 x2
//   +0x1C        uint8  flags2 (1 for AddLine)
//   ...          uint32 color
// This record layout is approximate (recovered from local stack-slot writes,
// not from a struct definition) and is not asserted; it is here only to
// support "AddLine reaches an internal, unreversed helper", the claim the
// hold-back rests on.
