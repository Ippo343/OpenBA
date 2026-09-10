# Wave 14 — lane DRAW

**Target:** `?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z`
**Retail:** `0x1800c2eb0`, ordinal 2260, 724 instructions.
**Worktree:** `C:/code/GIT/wt-wave14-draw`, branch `wave14/draw`, cut from `64d3948`.
**Port:** `runtime/src/save/saveload_draw.cpp`. Kill switch `BANISHED_RT_SAVELOAD_DRAW=0`.

One of the last seven unported rows in the `Save` namespace, and the only one
that had been declined **twice** — by Wave 5 lane C and again by the Wave 8.5
hold-back re-check.

---

## 1. Both stated blockers were false, and re-checking them cost twenty minutes

`reference_holdback_blockers_are_stale_check_them_first.md` says to re-check a
hold-back's stated blocker before accepting it. Here is what the two rows said
and what is actually true.

### Blocker 1 — "it hand-constructs five Wave 6/7 classes; porting Draw means porting their constructors first, which is a wave, not a function"

**False.** Those constructors are *inlined* into Draw — that is exactly why the
decompiler shows raw vtable stamps and long runs of zero stores instead of
calls. Nothing had to be ported first.

What Draw actually calls is 22 distinct targets. Resolving each **called
address against the shipped export table** (not against a source grep — see
`reference_grep_cannot_tell_internal_from_unported_export.md`) gives:

| kind | count | examples |
|---|---|---|
| named function exports | 18 | `??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z` (787), `?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z` (1882), `??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z` (72), `?AddSpriteDraw@SpriteContext@UI@@AEAAXAEBUSpriteDraw@12@@Z` (1586) |
| named DATA exports (4 vtables + 2 singletons) | 6 | `??_7MaterialInstance@Graphics@@6B@` (1464), `?_singleton@Device@Video@@0PEAV12@EA` (4807) |
| genuinely unexported internals | 4 | `0x18001c750`, `0x180033bc0`, `0x180033de0` (three byte-identical `Array<POD>::~Array` instantiations), `0x180061480` (`Array<{String,void*}>::SetSize`) |

All four internals are five- to sixty-line bodies and are **transcribed inline**
in the port, so this lane opens **no address door into the DLL**.

Two of those vtable names — `??_7MaterialInstance@Graphics@@6B@` and
`??_7SpriteSheet@UI@@6B@` — are the ones the hold-back called "vtables this
project does not own". Owning a vtable and being able to *resolve* one are
different things, and `renderer.cpp:559-561` had already been resolving two of
these three by name for a whole wave.

### Blocker 2 — "no oracle is possible; running both implementations would draw twice and leak a render target every frame"

Wrong on the frequency and wrong about whose problem the leak is.

* **Frequency.** Draw is a no-op unless `SaveLoad+0x7c`'s low byte is set, and
  the only writer of that byte is `UpdateSave` state 0
  (`saveload_update.cpp:1027`). It runs on **exactly one frame per save**, not
  every frame. The Wave 5 row even says "it is a no-op unless +0x7c is set" two
  sentences later, and the "every frame" phrasing survived anyway.
* **The leak.** It is the *harness's* to fix. `Draw` restores every piece of
  shared state it touches (render target, viewport) and the backbuffer it
  samples is not among the things it writes, so a second run is not fed spoiled
  inputs. The round in `RtSave_SaveLoad_Draw` snapshots our pixels into private
  storage, `MemFree`s our readback buffer, rewinds `_capturePixels`,
  `_captureFlags`, `Context::_renderBuffer`, `_dirty`, `_viewport` and
  `_scissor`, and only then calls retail. Retail runs on pristine state, and the
  live post-state the game keeps is **retail's**, so a diff run cannot leave the
  save holding our thumbnail.

Both corrections were made **at their source** (PLAN §6 step 8):
`runtime/src/save/saveload.cpp`'s HELD BACK block, `WAVE5_LANE_C.md` row 6 and
its prose paragraph, and `WAVE8_5_HOLDBACK_RECHECK.md` row 10.

---

## 2. What the function does

One frame per save, it renders the current frame letterboxed into a private
`_captureWidth` x `_captureHeight` offscreen target and reads it back into
`_capturePixels`:

1. `Graphics::FrameCopyImageBuffer` sized to the device, `Create`d then
   `Generate`d from the live `Video::Context` — the backbuffer copy.
2. A `Video::ImageBuffer` at the capture size, wrapped in a `Video::RenderBuffer`.
3. A `Graphics::MaterialInstance` with exactly one parameter, `"diffuse"` bound
   to the frame copy, then `BuildParameters`.
4. A `UI::SpriteSheet` with exactly one sprite, whose destination rect is the
   letterbox fit and whose UV rect is that fit normalised by the device size.
5. Redirect the context at the offscreen target, set the viewport to the capture
   size, clear to `0xffff6000` / depth 1.0.
6. An **orthographic** `Graphics::Camera` (six literal planes; only the right
   plane's `-w` and the top plane's `+w` carry the capture size) plus an identity
   `Transform3`; `SetCamera` with the camera's transform widened to 4x4 and its
   projection copied out.
7. A stack `UI::SpriteContext`, identity transform, one `AddSpriteDraw`, `Flush`.
8. Restore the render target and viewport, leave the **scissor** at the full
   device size (the one thing it does not put back), `MemAlloc(w*h*4)`, read back
   through the backend's `kImg_Read` slot, destroy all four objects, clear
   `_captureFlags`.

`UpdateSave` state 1 then blocks until `_capturePixels` is non-null. So a port
that silently does nothing does not fail quietly — **the save never completes.**

---

## 3. Four things Ghidra's decompiled C got wrong

Recorded because three of the four would have produced a body that compiles,
runs, and is wrong.

1. **A DROPPED ARGUMENT.** Ghidra prints the capture as
   `(**(code**)(*(longlong*)pMVar18 + 0x50))(pMVar18, param_1)` — two arguments.
   `MOV R8D,0x1` at `0x1800c2fa1` makes it three.
   *Confirmed a second way, which matters because "I read the disassembly more
   carefully" is a claim only its author can check:* slot 0x50 of
   `??_7FrameCopyImageBuffer@Graphics@@6B@` (0x180154f00) holds `0x1800a3b20`,
   and the export table names that
   `?Generate@FrameCopyImageBuffer@Graphics@@UEBAXAEAVContext@Video@@H@Z` —
   `void Generate(Video::Context&, int) const`. The **signature** settles the
   argument count without reference to any register.
   The same walk types the 12-entry call above it: slot 0x48 is
   `?Create@…UEAA_NW4ImageType@…`, eleven parameters exactly.
2. **A FABRICATED 64-BIT ARGUMENT.** `uVar22 = CONCAT44(uVar24,0x100)` and then
   `uVar22 & 0xffffffff00000000` passed to the second call. The bytes are two
   independent DWORD literals, `0x100` and `0`.
3. **AN INVENTED ALIAS.** `local_320` is reused for the MaterialInstance, the
   SpriteSheet, a String slot and the saved render buffer. Following the C would
   have restored the sprite sheet pointer into `Context::_renderBuffer`.
4. **A DEGENERATE-LOOKING IDENTITY.** The twelve floats of the camera transform
   read as three rows of `(1,0,0,0)` if you assume a 16-byte stride.
   `Math::Transform3` is twelve *contiguous* floats (four `Vector3`s), so the
   same bytes are the identity.

---

## 4. The `ExternalPtr<Graphics::Material>` by-value parameter

The lane brief flagged this as a trap: a non-trivial class by value goes by
hidden reference in the MS x64 ABI, and an `ExternalPtr` copy might carry a
refcount side effect.

**Neither applies.** The prologue stores `R8` straight into its home slot
(`MOV qword ptr [RAX+0x18],R8`, `0x1800c2eb3`) and `0x1800c3192` loads that same
qword whole into `MaterialInstance+0x28` — so the parameter *is* the 8-byte
value, passed in a register: `ExternalPtr<T>` is a trivially copyable single
pointer. And there is **no increment on the way in and no release on the way
out**: the exe's caller (`gameupdateloop.cpp:2231`) passes `m_uiMaterial`, which
it keeps owning, and retail treats it as borrowed for the life of the
MaterialInstance. A port that retained or released it would be wrong in the
direction that does not crash.

---

## 5. Two pieces of retail behaviour reproduced, not "fixed"

* **`SpriteContext+0x50` is never initialised.** The inlined constructor zeroes
  thirteen other members and takes the *address* of +0x50 (`LEA RAX,[RBP+0xa0]`,
  `0x1800c3815`) for the MSVC exception-state record without ever writing it.
  Our stack object leaves it alone too. This is now recorded in
  `runtime/types/ui/SpriteContext.h`, which previously said only that +0x50 was
  "not modelled" — it is still not modelled, and it is additionally *never
  written by a constructor*, which is a fact a future porter of any other
  SpriteContext-building body needs.
* **The four `vtable[0]` teardown calls are unguarded.** If an `operator new`
  had returned null, retail dereferences null. CLAUDE.md's rule applies exactly;
  left unguarded.

`SpriteContext.h`'s `_opaque50` comment also gains a second, independent witness
for the arrays at +0x58 and +0x68: Draw's inlined constructor zeroes both halves
of each and its inlined destructor frees exactly those two pointers.

---

## 6. Verification

All runs: debug standalone (`md5 6b7f8907bb28638121af1172d5b58bf2`), autostart, four
autosaves per run (`BANISHED_SAVE_TEST=200 BANISHED_SAVE_TEST_REPEAT=120`), graceful quit
via the in-game pause menu so `RtDiffSummary()` prints. Logs archived under
`docs/runtime_port/wave14/draw_*/`. Every run `rc=0`, `standalone_crash.log` **0 bytes**.

### 6.1 The oracle

Replay, `BANISHED_RT_DIFF=1`, three separately-scored sites. Capture size measured, not
assumed: **172 x 128 = 88,064 bytes per round**, printed per round by `[DrawDiff]` — a
zero-length buffer would compare clean, so "0 divergences" is only a result once the
extent is on the record.

| run | site | calls | worked | divergences |
|---|---|---|---|---|
| `final` (ours vs theirs) | `Save::SaveLoad::Draw [pixels]` | 4 | 4 | **0** |
| | `… [SaveLoad post-state]` | 4 | 4 | **0** |
| | `… [Video::Context post-state]` | 16 | 4 | **0** |
| `final_pos` (POSITIVE control, theirs vs theirs) | `… [pixels]` | 4 | 4 | 0 |
| | `… [SaveLoad post-state]` | 4 | 4 | 0 |
| | `… [Video::Context post-state]` | 16 | 4 | 0 |
| `final_neg` (NEGATIVE control, right clip plane halved) | `… [pixels]` | 4 | 4 | **4 — CAUGHT** |
| | `… [SaveLoad post-state]` | 4 | 4 | 0 — correctly blind |
| | `… [Video::Context post-state]` | 16 | 4 | 0 — correctly blind |

**352,256 bytes of rendered pixels compared against the shipped DLL, byte for byte, zero
differences.**

Read `worked=`, not just `divergences=`. `worked=4` on every site is the
`_captureFlags` early-out reporting that the body ran on all four saves; a run whose
`worked` were 0 would have a clean board that means nothing
(`reference_uncalled_port_is_not_verified.md`). The umbrella site
`Save::SaveLoad::Draw` shows `calls=0 worked=4` **by construction** — it declares work and
delegates every comparison to the three screens, so its `calls=0` is not the
"unexercised" reading of `reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`.

**The positive control is the one that makes the pixel green mean anything.** Two
consecutive retail draws in one frame produce byte-identical readbacks, so the readback is
deterministic and an ours-vs-theirs match is a real match rather than an artefact of a
comparison that cannot fail.

**The negative control corrupts a subject the pixel screen actually compares**
(`reference_control_must_pick_a_compared_subject.md`): the frustum's right clip plane,
which every rendered pixel depends on. The divergence dump names differing bytes at +0x0
(`ours=da bd a5 ff …  theirs=d7 bd a0 ff …`). It is caught **4/4 by the pixel screen and
by neither other screen** — which is the point of scoring three sites instead of one
(`reference_union_control_hides_a_blind_screen.md`): a union score of "caught" would have
hidden that two of the three screens are structurally blind to a rendering defect.

### 6.2 The functional screen — the port alone drives real saves

`final_live`, no diff, our body is the only Draw that runs:

```
[SaveTest] frame 261: SAVE OPERATION COMPLETE (61 frames elapsed)
[SaveTest] frame 443: SAVE OPERATION COMPLETE (61 frames elapsed)
[SaveTest] frame 625: SAVE OPERATION COMPLETE (61 frames elapsed)
[SaveTest] frame 807: SAVE OPERATION COMPLETE (61 frames elapsed)
[SaveTest]   Auto.sav   8509312 bytes  written 0s ago
```

**61 frames is the number to look at.** `UpdateSave` state 1 spins until
`_capturePixels` is non-null, so a Draw that quietly did nothing would hang the save
forever, and a slow one would stretch that count. 61 frames is exactly what Wave 13's
all-DLL integration run recorded (`docs/runtime_port/wave13/integration_savetest.txt`).

### 6.3 A cross-site control, because a port can dirty someone else's board

The main run reports 17 divergences in 6 sites overall, including
`UI::SpriteSheet::TypeName` (1) and `UI::SpriteSheet::Construct` (1) — and our Draw builds
a SpriteSheet, so those are exactly the sites a careless port would have dirtied.

Measured rather than assumed. `draw_dllbase`, identical except
`BANISHED_RT_SAVELOAD_DRAW=0` so retail's Draw runs:

| | ours registered | DLL Draw |
|---|---|---|
| total | 17 divergences in 6 sites | **17 divergences in 6 sites** |
| `UI::SpriteSheet::TypeName` | calls=1 div=1 | **calls=1 div=1** |
| `UI::SpriteSheet::Construct` | calls=3 div=1 | **calls=3 div=1** |

Identical. Both are pre-existing and neither is this lane's.

### 6.4 A defect this lane hit, and what it cost

The **first** live run crashed: `ACCESS_VIOLATION`, `Fault: READ of 0x1`, at
`DrawBody+0x19f` — `mov (%rax),%rax` on the value returned by
`??0ImageBuffer@Video@@QEAA@XZ`.

Retail reads a constructor's return (`CALL <ctor>; MOV R12,RAX`) and is right to: MSVC
returns `this` in RAX from every constructor. But that name is **PORTED**, and
`Video_ImageBuffer_Ctor` (`runtime/src/video/buffers.cpp:387`) is declared `void`, so its
RAX was whatever its last expression left there — here, `1`. A byte-faithful transcription
of retail is *unsafe across the port seam* at exactly this one construct, and the tree's
constructors are not uniform (`RtGraphics_Geometry_Ctor` does return `Geometry*`).

`runtime/src/graphics/sunlight.cpp:230-247` already documents this hazard and its
resolution — use `p`, which is the same pointer by construction — from an identical crash
in Wave 9. **I found that note only after paying for the crash myself**, which is the real
lesson: I grepped the tree for the *symbol* and not for the *hazard*. The fix follows the
existing convention and is commented at both sites in the port.

### 6.5 What is NOT verified — stated plainly

* **The GPU-side rendering is verified only through the readback.** The two suppressed
  vtable calls (the backend clear at `_contextData+0x20` and `Generate`) are not compared
  argument by argument; what is compared is the 88,064 bytes they ultimately produce. That
  is a strong end-to-end check and it is not an instruction-level one — a difference the
  readback cannot express would not be caught.
* **Only the `_captureFlags` byte is compared on the `SaveLoad` object.**
  `_capturePixels` is deliberately excluded: two `MemAlloc`s cannot return one address.
  Its *content* is the pixel screen, so the exclusion loses nothing.
* **Only four `Video::Context` fields are compared** — `_renderBuffer`, `_dirty`,
  `_viewport`, `_scissor`. Anything else `SetCamera`, `SetTransform` or `Flush` leaves in
  the Context is outside the model. Those are all suppressed-callee footprints, and the
  callees are the DLL's own in both runs.
* **One frame size, one aspect ratio.** Every run was 172x128 against the same backbuffer,
  so the letterbox branch was exercised in only ONE direction. The other arm of
  `fDevW/fDevH > aspect` is transcribed but has never executed. A run at a different
  window aspect would close that; this lane did not do one.
* **The allocation-failure arms have never run**, and cannot without an allocator fault
  injector. They are transcribed (including retail's unguarded null dereferences) and
  untested.


---

## 7. Census effect

The port resolves 24 retail symbols **by name** and **zero by address**. Twenty
of those names were already referenced somewhere in `code/`, `engine/`,
`runtime/`, `standalone/` or `dx11/`; **four are new to the tree** and will grow
the census `called` column:

* `??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z`
* `??2MaterialInstance@Graphics@@SAPEAX_K@Z`
* `??2SpriteSheet@UI@@SAPEAX_K@Z`
* `?Set@SpriteSheet@UI@@QEAAXHHAEBVString@System@@0_N@Z`

That is `+1` ported in `Save` and `+4` called across `Graphics` and `UI`. A
percentage that moves down in those two namespaces is this, not a regression
(`reference_porting_can_grow_the_cutover_surface.md`).

`tools/abi_retslot_audit.py`: **0 suspicious of 49** before and after — this
function returns `void`, so it cannot add a row.
