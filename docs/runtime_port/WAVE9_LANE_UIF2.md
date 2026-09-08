# Wave 9 lane UIF — the UI frame and the sprite context

**Branch** `wave9/lane-uif` · **Base** `fd30180` · **Date** 2026-09-08

> A second lane was already called UIF this wave and its write-up is
> [WAVE9_LANE_UIF.md](WAVE9_LANE_UIF.md) (the 24 UI rows of cutover attempt 06).
> That document is untouched. This one is the five-symbol assignment.

**Assigned 5. Ported 3, plus 4 internals they need. Declined 2, with the
closure measured rather than asserted.**

| Assigned symbol | Retail | Verdict |
|---|---|---|
| `?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z` | 0x18011cbe0 | **ported** |
| `?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z` | 0x18011d050 | **ported** |
| `?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z` | 0x18011d010 | **ported** |
| `?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z` | 0x1801213c0 | **declined** — §5 |
| `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z` | 0x180121890 | **declined** — §5 |

Also ported, as enabling internals (none has an exported name in either
shipped build, so each is a `RegisterRuntimeAddr`):

| Internal | Retail | Where it lives |
|---|---|---|
| `SpriteContext::AddSpriteDraw` | 0x18011c890 | `runtime/src/ui/spritecontext.cpp` |
| `SpriteContext::FlushDeferred` | 0x18011c6a0 | `runtime/src/ui/spritecontext.cpp` |
| `Array<SpriteDraw>::SetSize(n, fill)` | 0x180078520 | `runtime/src/collection/array.cpp` |
| `Array<SpriteDraw>::SetSize(0)` | 0x1800783d0 | `runtime/src/collection/array.cpp` |

Commits: `33c4877` (the port), `779f790` (a correction the lane made to
itself — §7).

---

## 1. The door arithmetic, and it is LATERAL, not forward

This is the number the brief asked for first, so it goes first, and it is not
a flattering one.

Both columns are the **same tool, same flags**, each run with its working
directory set to a pristine `git archive` extraction of the tree in question.
That detail is load-bearing: `addr_door_audit.py` and
`unsatisfied_export_scan.py` walk `os.walk("runtime")` etc. **relative to the
current directory, not to the script's own location.** Running the two
copies of the script from one working directory measures that directory
twice and prints two identical reports that look like "my change did
nothing". This lane did exactly that and nearly published it — see §7.

**`addr_door_audit.py` here is the FIXED version from `f5dbeb8`** (lane ADDR),
not the one on this lane's base. The old one matched its pattern inside
comments; §7 reports that this lane found the same defect independently and
left it for its owner, and its owner fixed it in the same wave. Both columns
below use the fixed tool, so they are comparable to each other and to anything
measured after `f5dbeb8`. The pre-fix numbers are in §7 for continuity.

| Instrument | base `fd30180` | `wave9/lane-uif` | delta |
|---|---|---|---|
| `unsatisfied_export_scan` — names with no port and no mint | **129** | **126** | **−3** |
| … of those, outside a diff guard | 126 | 123 | −3 |
| `addr_door_audit` — UNREGISTERED | **15** | **18** | **+3** |
| `addr_door_audit` — by-address call sites | 270 | 275 | +5 |
| `addr_door_audit` — distinct addresses | 192 | 197 | +5 |
| `addr_door_audit` — registered addresses | 1186 | 1190 | +4 |
| `addr_door_audit` — non-literal call sites (blind spot) | 15 | 15 | **+0** |

**Set-diff, exports CLOSED (3):**

```
?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z
?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z
?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z
```

**Set-diff, exports OPENED: none.**

**Set-diff, address doors OPENED (3):**

```
0x18011ba40  SpriteContext::Flush            runtime/src/ui/spritecontext.cpp:211
0x1800d8ef0  SpriteCapture::AddScissor       runtime/src/ui/spritecontext.cpp:217
0x1800d8b20  SpriteCapture::AddSprite        runtime/src/ui/spritecontext.cpp:222
```

**Set-diff, address doors CLOSED: none** — the four internals this lane
ported were not previously *called* by anything in our tree, so registering
them adds to the `registered` column without removing from `UNREGISTERED`.

### So: three closed, three opened. Read it honestly.

By the brief's rule ("a lane that closes 3 doors and opens 4 has gone
backwards") this lane has not gone backwards, and it has not gone forwards
either. What it has done is **move the wall from three symbols whose bodies
nobody had written to three symbols whose bodies are now the only thing
missing.** Two claims support calling that progress, and both are checkable:

1. **Two of the three new doors are not on the boot path.**
   `SpriteCapture::AddScissor` and `SpriteCapture::AddSprite` are reached only
   through `if (_spriteCapture)`. `UserInterface`'s constructor zeroes
   `_spriteCapture` (+0x110) along with the rest of the object, `Begin`
   (0x18011d2b0) copies that field into `SpriteContext+0x80`, and
   `EndSpriteCapture` (0x18011f4a0) sets it back to 0. Nothing sets it except
   a screenshot/capture UI. So on an ordinary boot-to-gameplay run both
   resolve, both stay uncalled, and at cutover both are null pointers nobody
   dereferences. **The lane's real cutover cost is ONE door: `Flush`.**
2. **`Flush` was already the wall.** It is what all three assigned exports
   call, and porting them without it was never going to close anything by
   itself. The difference is that the wall is now one unexported internal with
   a known address and a known shape, instead of three exported names each of
   which had to be re-reversed before anyone could see that.

**What this lane did NOT do, stated plainly: it did not reduce the number of
things that must exist before `reimpl-x64.exe` boots without the DLL.** It
reduced the number of them that still need reverse engineering.

---

## 2. The Wave 7 hold-back, re-checked — and it was right about the wrong scope

`docs/runtime_port/WAVE7_LANE_F.md` §2 held all three exports back with one
argument:

> both reach unexported, unreversed internal helpers — `Flush`,
> `FlushDeferredFonts`, `AddSpriteDraw`, `SpriteCapture::AddScissor` — that
> submit into the LIVE `Video::Context` / GPU vertex buffer this SpriteContext
> wraps, not into memory the object itself owns. A scratch copy of the
> SpriteContext object does not isolate a replay.

Re-read instruction by instruction rather than inherited
(`reference_holdback_blockers_are_stale_check_them_first.md`), **that sentence
is true of exactly one of the four helpers it names.**

* **`Flush` (0x18011ba40) — the argument holds completely.** It reaches the
  bound vertex buffer through `_context`'s device vtable
  (`CALL qword ptr [RAX+0x70]` at 0x18011bc06 to map,
  `CALL qword ptr [RAX+0x78]` at 0x18011c5e3 to unmap), builds transformed
  vertices with `sinf`/`cosf` (0x180127d90 / 0x180126d10) and `memmove`s them
  in at `_context+0x20e0`, advancing that write cursor by `_context+0x20e8`
  each time. Replaying it draws the frame twice. **Still held back.**
* **`AddSpriteDraw` (0x18011c890) — the argument does not hold.** 70
  instructions. It compares the record against the clip region, optionally
  hands a copy to the capture, culls against the scissor, and appends to
  `_draws`. It never touches `_context`.
* **`FlushDeferredFonts` (really `FlushDeferred`, 0x18011c6a0) — does not
  hold.** 30 instructions, all of which are a loop over `_deferred` calling
  `AddSpriteDraw`, plus two `Flush` calls and a clear. It reaches the GPU only
  *through* `Flush`.
* **`SpriteCapture::AddScissor` (0x1800d8ef0) — does not hold, and is not on
  the path anyway.** It appends to two arrays inside the capture object. See
  §1 claim 1.

The three exports themselves touch no GPU state at all. They mutate four
`int32`s and an ordinary `Collection::Array<SpriteDraw>`, and they call
`Flush`. That is a real reason not to *replay* them (§4), and it is not a
reason not to *port* them.

---

## 3. The hidden return slot — the flagged hazard, and how it is closed

Both `SetScissor` overloads return `Video::Viewport` **by value**, so the
return slot is an argument: RCX = `this`, **RDX = the slot**, then the visible
arguments. A typedef that omits it is a wild write that still returns the
right answer, because the callee writes through whatever was in RDX and hands
that same address back in RAX.

Settled from the shipped bytes, not from the mangled name:

| Evidence | Address |
|---|---|
| `MOV RBX,RDX` — the 4-int overload saves arg 2… | 0x18011d010 |
| `MOV RAX,RBX` — …and returns it | 0x18011d03d |
| `MOV RDI,RDX` — the Viewport overload saves arg 2… | 0x18011d080 |
| `MOV RAX,RDI` — …and returns it | 0x18011d1c6 |

And an independent check that does not depend on reading the moves at all:
the 4-int overload's frame is `PUSH RBX; SUB RSP,0x30`, and it reads its 5th
and 6th arguments at `RSP+0x60` and `RSP+0x68` — caller-frame **+0x28** and
**+0x30**. The first stack argument slot is caller-frame +0x28, so **four
register arguments precede `w`**: `this`, the slot, `x`, `y`. If the slot were
not an argument, `w` would be the 4th register argument and `h` would sit at
+0x20.

`tools/abi_retslot_audit.py` reports `0 suspicious of 48` on both base and
branch — but it only scans `engine/funcs/engine_funcs.{h,cpp}`, so **it does
not cover this file and its clean exit is not evidence about this lane.** The
proof here is the oracle instead: both `SetScissor` sites drive the *shipped*
body through the very typedef the port declares, into a scratch pre-filled
with `0xCD`, and compare 16 bytes. A missing slot parameter would put `x` in
RDX, scribble the Viewport through it, and fail that comparison — rather than
passing quietly, which is exactly how the `fn_Math_Transform3_Identity`
version of this bug survived a whole wave.

The two overloads differ only in their argument list, so getting one right and
one wrong was the specific failure mode to avoid. Both carry the slot; both
have their own oracle; the two oracles are independent.

---

## 4. Oracles: what they cover, and the effectiveness split

**No body here is replayable in full.** `AddLine` and `AddSpriteDraw` append
to the batch, so a second call queues the sprite twice; `SetScissor` flushes
and pushes a scissor record; `FlushDeferred` drains and clears. Each site
therefore replays **only on its early-out branch**, where the shipped body is
provably side-effect free, and declares the mutating branch through
`RT_DIFF_WORK`.

| Site | Replay covers | `worked=` counts | Reached raw? |
|---|---|---|---|
| `SetScissor(Viewport const&)` | unchanged-rect exit (0x18011d0d8) | calls that changed the rect | by name |
| `SetScissor(int,int,int,int)` | same, decided before forwarding | same | by name |
| `AddLine` | null sheet or index < 0 (0x18011cc0c/0x18011cc12) | calls that queued a record | by name |
| `AddSpriteDraw` | the record was CULLED and no capture is active | calls that appended | **by address, raw** |
| `FlushDeferred` | `_deferring` clear (0x18011c6a6) | calls that drained | **by address, raw** |

**So `divergences=0` from this file will mean "0 divergences in the calls the
oracle could see", and the `worked=` column is how many it could not.** That
is weaker than a Wave 6/7 replay figure and the lane doc says so rather than
letting a large `calls=` imply coverage it does not have.

**One exception, and it is the one that matters.** `AddSpriteDraw`'s cull
oracle is safe *even when the two implementations disagree*: our body appended
nothing, so a shipped body that appends leaves the live batch holding the
record retail wanted — correct, not corrupted. It can therefore be left on
through a full gameplay run, and it is the oracle that covers the float
scissor arithmetic (four `CVTDQ2PS` / `MULSS` / `COMISS` comparisons against
`_scale`), which is the part of this lane most likely to be subtly wrong.

**Both internal oracles resolve through `ResolveRuntimeRaw`, not
`ResolveAddr`.** This file *registers* 0x18011c890 and 0x18011c6a0, so
`ResolveAddr` would hand back our own body and the comparison would be true by
construction — `reference_recheck_must_match_its_own_proof.md`. Both raw calls
sit inside `if (rt_diff_on__)`; `rawdoor_audit.py` reports **0 UNGUARDED-CALL**
on the branch, with the scanned denominator *rising* 607 → 613.

---

## 5. What was DECLINED, and the measured blocker for each

Neither blocker is inherited. Both are this lane's own disassembly of the
body, with each callee checked against the tree for a registration.

### `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z` — 0x180121890

Closes **1** export door and opens **2** address doors, whose own closures are
unmeasured:

| Callee | Status |
|---|---|
| 0x1800fe960 `Graphics::Camera::Camera` | ported + registered (`renderer.cpp:682`) |
| 0x18001a150 `Video::Context::SetCamera` | ported + registered (`context.cpp:908`) |
| 0x18011c890 `AddSpriteDraw` | **ported by this lane** |
| 0x1800783d0 `Array<SpriteDraw>::SetSize(0)` | **ported by this lane** |
| 0x1800c6c00 `MemFree` | ported |
| 0x18011ba40 `Flush` | **unported** (already this lane's door) |
| 0x18011d2b0 `SpriteContext::Begin` | **unported — NEW door** |
| 0x1800d8a60 `SpriteCapture` bounds pass | **unported — NEW door** |

`Begin` is 40 instructions but is not self-contained: it calls 0x1800370c0,
0x18004ba90 and 0x18001a6b0, none of them registered, so the true cost of
porting `Draw` is 2 new doors plus at least 3 behind `Begin`. Declining it
keeps the arithmetic at −3/+3 instead of −4/+5-or-worse.

**Note for whoever picks it up — and this half of it is now CLOSED.** Lane
CTX's warning about `?Inverse@Matrix@Math@@` inside
`Video::Context::SetCamera` is live on `Draw`'s path: `Draw` calls `SetCamera`
at 0x180121ca8. This lane did **not** port `Inverse`, per the brief's
instruction to leave it to lane GFXR, and did not touch `context.cpp`.
**Lane GFXR landed it in `f5dbeb8`** (bit-exact over 2,240,021 adversarial
cases, negative control firing at 89.01%), and the same change converts
`MatrixInverse()` from a frozen `static Fn fn = ResolveExport(...)` to
`RT_EXPORT_ONCE`, so a call arriving before `RtMathPortInit()` no longer
caches a null forever.

So `Draw`'s `SetCamera` dependency is satisfied and is **no longer part of
this decline.** The decline stands on the other two rows unchanged:
`SpriteContext::Begin` (0x18011d2b0, plus at least three unregistered callees
of its own) and the `SpriteCapture` bounds pass (0x1800d8a60). Anyone
re-costing `Draw` after `f5dbeb8` should re-take the closure rather than
trusting this table — one of its rows has already moved.

### `?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z` — 0x1801213c0

302 instructions. Closes **1** export door and opens **5** address doors:

| Callee | Status |
|---|---|
| 0x1801223d0 `UserInterface::Play` | ported (Wave 9 lane Z) |
| 0x1800c6e60 / 0x1800c6c00 `MemAlloc` / `MemFree` | ported |
| 0x18011f790 `CleanRemovedDialogs` | **unported — NEW door** |
| 0x180120f20 `CloseDialog` | **unported** — see below |
| 0x18011fc60 `UpdateEvents` | **unported — NEW door** |
| 0x180113980 `DialogInstance::Update` | **unported — NEW door** |
| 0x180113d90 `DialogInstance::ComputeLayout` | **unported — NEW door** |
| 0x18011fa40 unnamed `UserInterface` dialog-list walk (58 insns) | **unported — NEW door** |
| 0x180033690 `Array` grow | unported |

**One of those is not a new symbol, and the distinction is worth recording.**
`CloseDialog` (0x180120f20) is *already* an unsatisfied EXPORT door
(`?CloseDialog@UserInterface@UI@@…`, `engine/funcs/engine_funcs.cpp:1607`).
Porting `Update` would add a second, by-address door onto the same function.
The `UNREGISTERED` count would rise by 1 while the number of *functions* that
must exist before cutover would not — so on this row the address-door count
overstates the cost, in the opposite direction to §7's understatement. Neither
instrument is wrong; they answer different questions, and a lane that quotes
one without the other will mis-price its own work.

`Update` is also not replayable and not even idempotent: it drains
`_addDialogs` into the live list allocating a node each, frees the array,
integrates the fade, calls `DialogInstance::Update` on every dialog, and
consumes input from `Input::Device::_singleton`. There is no early-out branch
big enough to give it the partial oracle §4 uses, so it would have landed with
no oracle at all, in a wave whose whole point is that verification must be
spent before the DLL goes away.

---

## 6. `0x1800783d0` is NOT an `~Array`, and registering it as one would corrupt

Recorded because the mistake is available and cheap to make: eleven addresses
in `array.cpp`'s J1 family look like this one and **free the buffer**.

0x1800783d0 does not free. Its epilogue is
`CMP RSI,[RBX] / JZ return` (0x1800784a1), comparing the data pointer captured
on entry against the current one. With the new size folded to the constant 0
nothing between them can reallocate — the grow branch is
`CMP [RCX+0xc],0 / JGE skip` and capacity is never negative — so the two are
always equal and the `MemFree` at 0x1800784d2 is **unreachable**. The element
loop at 0x180078441 is unreachable for the same reason (`TEST EAX,EAX / JNS`
on the old size, which is > 0 on every path that reaches it).

The whole observable effect is `_size = 0`, capacity and buffer kept — which
is what `FlushDeferred` needs, since it drains the deferred batch on every
scissor change and would thrash the heap otherwise. Registering the J1 body
here would free a buffer retail keeps and hand the next `AddSpriteDraw` a
dangling pointer.

It is implemented through `SetSizeCore` anyway, not as a hand-written
`_size = 0`, so that it reproduces the same unreachable branches for the same
reasons — if the reasoning above is wrong the two disagree under the oracle
instead of agreeing by construction.

### A retail quirk, reproduced rather than repaired

`SetScissor`'s **unchanged-rect** exit returns the raw device-space ints
(0x18011d0d8..0x18011d0ef); the **changed** exit returns them multiplied by
`_scale` (0x18011d195..0x18011d1b4). With `_scale != 1` the two exits return
the same rectangle in **different spaces**. That is almost certainly a bug in
retail. It is kept: callers save the result and feed it back through this same
function to restore, so "fixing" the asymmetry changes what the restore
computes.

---

## 7. A correction this lane made to itself, while measuring itself

The first version of `spritecontext.cpp` routed all five by-address internals
through one `RtAddr(slot, addr)` template helper. It was shorter, it compiled,
and **it made every address door in the file invisible to the instrument the
wave steers by.** Same tool, same trees:

| | base | with the helper | with literals (shipped) |
|---|---|---|---|
| `UNREGISTERED` | 16 | **16** | **19** |
| non-literal call sites | 77 | 80 | 80 |

Three real cutover doors sat in the `non-literal call sites` line — which the
tool prints as a self-declared blind spot and nobody steers by — while the
headline number said the lane had added nothing. This is the fifth instance of
`reference_literal_grep_blind_to_variables.md`, and the lane committed it
*while writing the section that quotes the number*. Fixed in `779f790`: each
of the five spells its address once, as a literal, in its own resolver.

**A second, smaller finding in the same instrument — reported not fixed, and
its owner then fixed it.** After the literal fix above,
`addr_door_audit --nonliteral` still attributed 3 non-literal sites to this
file: lines 98, 190 and 607. All three were **prose** — comments that mention
`ResolveAddr(Module::Runtime, …)` while explaining the doors. The scanner
matched its pattern inside comments, so *documenting* a door manufactured a
phantom one, the mirror image of
`reference_documenting_a_check_can_delete_it.md`. It inflated only the
self-declared blind-spot line, never `UNREGISTERED`, so it was harmless; this
lane left it alone because a shared instrument should be fixed by one owner in
one commit and this lane is not that owner.

**Lane ADDR reached the same defect from the other end and fixed it in
`f5dbeb8`** ("a census that was inventing work"). Two lanes finding one
instrument defect independently, and the owner landing the fix, is the
mechanism working. The correction is large — it was counting a lot of prose
tree-wide, not just this file's:

| `addr_door_audit` on base `fd30180` | pre-fix | fixed (`f5dbeb8`) |
|---|---|---|
| by-address call sites | 295 | 270 |
| distinct addresses | 198 | 192 |
| UNREGISTERED | 16 | 15 |
| non-literal call sites | 77 | 15 |

**This lane's conclusion is unchanged under the fixed tool**, which is the
point of re-running rather than patching the old numbers: −3 exports for +3
address doors either way, the same three rows
(0x18011ba40, 0x1800d8ef0, 0x1800d8b20), and the non-literal delta now **+0**
instead of +3 — the fixed tool no longer counts this file's prose, which is
the independent confirmation that the 3 were phantom. §1 carries the fixed
numbers; these pre-fix ones are kept only so a reader comparing against an
earlier draft can see why they moved.

**And the trap that nearly ate the whole measurement:** both scanners walk
`os.walk("runtime")` **relative to the current working directory**, not to the
script's location. Invoking two different checkouts' copies of the script from
one directory measures that directory twice. The first base-vs-branch run in
this lane did exactly that and printed two byte-identical reports, which reads
as "this change has no effect on any door" — the most reassuring possible
form of a wrong answer. Every number in §1 was re-taken with `Push-Location`
into the extraction being measured.

---

## 8. Gate results

Every denominator **rose or held**; none shrank
(a shrinking denominator has faked a clean pass twice in this project).

| Gate | base `fd30180` | `wave9/lane-uif` |
|---|---|---|
| `port_bypass_audit` address door | 334 sites, **0** bypasses | 341 sites, **0** bypasses |
| `port_bypass_audit` name door | 477 sites, **0** bypasses | 480 sites, **0** bypasses |
| `rawdoor_audit` | 607 sites, **0** UNGUARDED-CALL, 66 DEFERRED | 613 sites, **0** UNGUARDED-CALL, 66 DEFERRED |
| `detour_target_audit` | clean | clean |
| `abi_retslot_audit` | 0 of 48 | 0 of 48 (**does not cover this file** — §3) |
| `addr_door_audit` (fixed, `f5dbeb8`) | 270 sites, UNREG 15 | 275 sites, UNREG **18** |
| `runtime_census` UI | 153 called / 118 ported / 77.1% | 158 called / 123 ported / 77.8% |
| `runtime_census` internal | 106 called / 94 ported / 88.7% | 108 called / 96 ported / 88.9% |

Note the census `called` column rises by exactly as much as `ported` does
(+5 UI, +2 internal). That is `reference_porting_can_grow_the_cutover_surface.md`
visible in the census: a faithful port that calls DLL internals by address adds
to both columns, so coverage barely moves. It is the same fact §1 states as
"lateral", seen through a different instrument, and the two agree.

---

## 9. HONEST LIMITS

1. **THIS HAS NOW BEEN RUN. §11 supersedes what this item used to say.**
   8,165 differential comparisons against the shipped bodies, 0 divergences --
   but three branches ran ~11,000 times with NO oracle covering them and are
   still not verified against retail. That distinction is the whole of §11.
2. **A `calls=0` from this lane can mean "always took the branch the oracle
   does not cover" -- and in one measured case it did.** This item used to say
   only that a `calls=0` "would not mean unexercised", which was right in the
   abstract; the lane then mis-diagnosed a concrete instance of it. See §11's
   `FlushDeferred` correction.
3. **`sizeof(UI::SpriteContext)` is a LOWER BOUND (0x88), not a proven size.**
   The object is a stack local of `UserInterface::Draw`, never heap-allocated,
   so no allocation site spells its size. Nothing in this lane allocates one,
   so the bound is sufficient — but a future lane that *constructs* one must
   pin the real size first.
4. **Three of the four arrays inside `SpriteContext` are unmodelled** (+0x50,
   +0x58, +0x68). `UserInterface::Draw` frees all four; this lane reads only
   two and deliberately did not invent a layout for the rest.
5. **The `kSpec_SpriteDraw` init arm has TWO unwritten windows** (+0x1c..+0x1f
   and +0x25..+0x27) and `ArraySpec` carries one per arm. Only the first is
   expressed. It is not a live risk for either registration — the 3-argument
   form always takes the assign arm, and the 0-argument form never grows — but
   a future two-argument `Array<SpriteDraw>::SetSize` would be the first
   caller that can reach it.
6. **One deliberate divergence from retail.** `AddLine` builds its record as
   `SpriteDraw rec{}`; retail leaves +0x25..+0x27 as stack garbage. Nothing
   reads those three bytes (`Flush`'s record walk reads +0x00, +0x08, +0x0c,
   +0x10, +0x14, +0x18, +0x1c, +0x20 and the +0x24 byte and nothing else), and
   reading indeterminate bytes is UB the compiler may exploit. Documented at
   the site.
7. **`Update` and `Draw` remain unsatisfied**, so this lane does **not** move
   the cutover boot past `UserInterface`. §5 is the handover.
8. **It DOES build and link.** `mingw32-make standalone-debug` run from a
   pristine `git archive` extraction of this branch (not from the shared
   worktree, which holds other lanes' uncommitted files) exits 0 and produces
   a 28 MB `reimpl-x64.exe`. `nm -C` on it finds all seven bodies —
   `RtUi_SpriteContext_{AddLine,AddSpriteDraw,FlushDeferred,SetScissor_Ints,
   SetScissor_Viewport}`, `RtUiSpriteContextRegister`, and
   `rtcoll::RtColl_{ArraySetSizeFill_180078520,ArraySetSize_1800783d0}` — so
   the registration really is reachable from `RtUiSpriteRegister`. Per-file
   `-fsyntax-only` alone would not have caught the
   `extern int RtUiSpriteContextRegister();` link edge.
   **A successful link is not a successful run**; limit 1 still stands.

## 10. Provenance of this branch, and the worktree hazard

**Worktree:** `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/lane-addr`
— a directory named for a DIFFERENT lane, which is itself part of the
incident below. **Branch:** `wave9/lane-uif`. **Base:** `fd30180`.
`git diff --name-only fd30180 wave9/lane-uif` is exactly the five files listed
at the top of this document.

### This branch was written to by another lane and reset. Nothing was lost.

`git reflog wave9/lane-uif` records the whole sequence, and the ORDER is what
settles it:

```
@{6}  branch: Created from fd30180              <- this lane's `checkout -b`
@{5}  commit: WAVE 9 LANE ADDR: the address-door census was reading prose as code
@{4}  branch: Reset to fd30180                  <- lane ADDR's own repair
@{3}  commit: WAVE 9 LANE UIF: SpriteContext opens…
@{2}  commit: Lane UIF: spell the five internal addresses as literals…
@{1}  commit: Lane UIF: the write-up…
@{0}  commit: Lane UIF: the 4-int forwarder…
```

Lane ADDR's commit landed on this branch and lane ADDR reset it back to the
creation point **before this lane had committed anything**. All four UIF
commits are `@{3}`..`@{0}`, i.e. after the repair. ADDR's assessment ("you had
no commits at that moment, so nothing of yours was lost") is confirmed from
this side, from the reflog rather than from either lane's recollection.

There is a pleasing symmetry worth recording: the commit that briefly sat on
this branch was ADDR's fix for *the same instrument defect this lane reported
in §7* — an address-door census reading prose as code.

### The hazard itself

This session's worktree assignment **moved between tool calls**, four times,
landing on `lane-mod9` (branch `wave9/lane-mod`), `lane-uid-089e6eb4` and back.
The directory `lane-addr` that ended up holding this branch also contained, at
various points, **uncommitted work belonging to other lanes**:
`runtime/src/object/internal.cpp`, `runtime/src/object/external_set.{cpp,h}`,
`runtime/src/input/input_win.cpp`, `runtime/src/math/rt_math_port.cpp`,
`runtime/src/math/wave9_lane_gfxr.cpp`, `runtime/src/video/context.cpp`.

Consequences, both handled:

* **Every commit on this branch names its paths explicitly**
  (`git commit -- <paths>`), so no other lane's work was swept in. Verified:
  `git diff --stat fd30180 wave9/lane-uif` is exactly 4 files.
* **The first export-door baseline was taken in a different tree** and showed
  5 names closing, of which only 3 are this lane's — the other two
  (`?Clean@ExternalFactory@Object@@`, `?Inverse@Matrix@Math@@`) came from
  other lanes' uncommitted files being visible to the scanner. Every number in
  §1 was re-taken from `git archive` extractions of `fd30180` and of this
  branch, so no foreign edit is inside them.

**The integrator should assume other lanes running in these shared worktrees
may have had the same experience and may not have noticed.** A lane that ran
`git commit -a`, or that took a "before" measurement in one directory and an
"after" in another, has a contaminated result.

---

## 11. THE RUNS — what is verified, what merely executed

Added 2026-09-08 after the lane was merged. **This section supersedes §9's
first two items**, which said nothing had been run.

Two runs, both with `Runtime-x64.dll` present. Together: **8,165 differential
comparisons against the shipped bodies, 0 divergences.** That number is worth
less than it looks and §11.3 says why.

### 11.1 The driven probe — `BANISHED_RT_SPRITECTX_PROBE=1`

```
[SpriteCtxProbe] ALIVE -- 28 cases, 0 divergences vs the shipped bodies.
Effectiveness: appended=11 culled=4 earlyOut=5 (all three branches exercised).
Both negative controls FIRED (cull=1 retslot=1). Flush-tripwire skips=0.
```

**It crashed the boot on its first attempt, and that is recorded rather than
repaired away.** ACCESS_VIOLATION at `Runtime-x64.dll+0x11BA66` reading address
`0x100`, return address `0x18011D0F9` — `SetScissor`'s `CALL 0x18011ba40`,
i.e. inside `Flush`, at `MOV RCX,[RAX+0x100]` with `RAX` = the Video device
global at `0x1801fe7a8`, which is null at registration time. The probe walked
into the exact hole its own header documents, and the cause was its own
retslot negative control forcing the changed branch. Fixed by keeping both
subjects on the unchanged branch while still forcing their returns to differ,
plus a `WouldReachFlush()` tripwire that skips any future case that would
reach `Flush` and prints why.

### 11.2 The live minimap cell

`BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=utility BANISHED_RT_DIFF=1
BANISHED_RT_SPRITECTX_PROBE=0`, binary md5 `f31d14e43499a36d2b9f2c89624ffd13`
(own build of `e608c6c`), rc=0, zero-byte crash log. `FireUtilityDialog` opened
`Template\UtilityMap.rsc` at frame 60. Probe OFF, so every number is a real
caller with no subtraction.

```
SetScissor(int,int,int,int)   calls=0     worked=1633   divergences=0
SetScissor(Viewport const&)   calls=1633  worked=1633   divergences=0
FlushDeferred                 calls=0     worked=1633   divergences=0
AddLine                       calls=0     worked=6532   divergences=0
AddSpriteDraw                 calls=6532  worked=0      divergences=0
```

**The hidden return slot is now verified LIVE.** `calls=1633` on
`SetScissor(Viewport const&)` is 1,633 differential comparisons through the
real caller's register state, each checking all 16 bytes of the returned
Viewport *and* that RAX is the slot. The slot mechanism is identical on both
branches — only the values differ — so this closes the hazard §3 was written
about, and closes it with a mechanism `tools/abi_retslot_audit.py`
structurally cannot see (§9 item 8, and the 21-pointer blind spot).

### 11.3 A PREDICTION THIS LANE GOT WRONG, AND THE SHAPE OF THE ERROR

This document, and two messages to the integrator, said a `FlushDeferred`
`calls=0` would be **"expected, not dead, because `_deferring` is retail's
deferred-font path and nothing on the boot path sets it."**

**That is wrong.** `worked=1633` means `_deferring` was set on *every* call and
the **drain path ran 1,633 times** — once per changed scissor, since
`SetScissor`'s changed branch calls `FlushDeferred`. The `calls=0` is real, but
its cause is the opposite of the one predicted: not "never called" but
**"called 1,633 times and always took the branch the oracle does not cover."**

**STATE IT POSITIVELY, because the negative form is what gets misread:
`FlushDeferred`'s drain is LIVE ON ANY PATH THAT CHANGES THE SCISSOR, not
dormant.** `SetScissor`'s changed branch calls it unconditionally, so any UI
that moves a clip rect drains the deferred batch. Anyone re-costing this row
later should start from that model and not from the deferred-font reading.

The lane had warned the integrator, in the abstract, that a `calls=0` is
ambiguous between "not exercised" and "not covered" — and then mis-diagnosed a
concrete instance of that ambiguity in its own port, on the basis of a
plausible reading of `_deferring` rather than a measurement. **A branch's
frequency is not deducible from what the branch appears to be for.**

### 11.3b `calls=0` has FOUR meanings and only one of them is harmless

Recorded by the integrator from this lane's miss, and repeated here because
this document is where someone will meet it:

1. never called (genuinely unexercised)
2. never registered, or the wrong build was measured
3. a raw resolve missed and the oracle skipped silently
4. **called every time, and always down the branch the oracle does not cover**

Only (1) is "no coverage to worry about". **(4) is a body fully live in
production and entirely unverified**, and in a summary it is indistinguishable
from (1). `calls=0` beside a NON-ZERO `worked=` is the loudest available signal
that an oracle is aimed at a branch the code does not take — this lane produced
exactly that pattern twice in one run (`FlushDeferred` 0/1633, `AddLine`
0/6532) and read it correctly only after measuring.

### 11.4 The ledger: verified vs merely executed

`divergences=0` across five sites reads like five verified functions. It is
not, and the split matters more than the total.

| Branch | Live executions | Differentially verified? |
|---|---|---|
| `AddSpriteDraw` cull | 6,532 | **YES** — 6,532 comparisons |
| `SetScissor(Viewport)` unchanged exit | 1,633 | **YES** — 1,633, incl. retslot |
| `AddSpriteDraw` append | 0 live | probe only (11 cases) |
| `AddLine` early-out | 0 live | probe only (4 cases) |
| `SetScissor` changed branch | 3,266 | **NO — no oracle** |
| `AddLine` queueing path | 6,532 | **NO — probe only** |
| `FlushDeferred` drain | 1,633 | **NO — no oracle** |

Roughly **11,400 executions carry no differential evidence at all.** They ran
without faulting, which is real but is absence-of-crash, not
agreement-with-retail. Specifically unverified against the shipped body: the
values `SetScissor` returns on its changed branch, and everything the drain
does.

Closing those needs an oracle that survives a `Flush` call — i.e. a port of
`Flush` (0x18011ba40), which is this lane's remaining wall and was its one
opened address door all along.
