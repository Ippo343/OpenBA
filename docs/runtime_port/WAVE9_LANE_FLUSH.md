# Wave 9 lane FLUSH — the wall, and the verification gap behind it

**Branch** `wave9/lane-flush` · **Base** `085782d` · **Date** 2026-09-08 ·
**Worktree** `.claude/worktrees/flush-089e6eb4`

Assigned one primary and two secondaries.

| Assigned | Retail | Verdict |
|---|---|---|
| `UI::SpriteContext::Flush` | 0x18011ba40 | **ported + registered** |
| `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z` | 0x180121890 | **declined** — §6, closure RE-TAKEN |
| `?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z` | 0x1801213c0 | **declined** — §6, closure RE-TAKEN |

Plus the latent ABI defect the brief flagged: **fixed**, §5.

---

## 1. THE HEADLINE, AND IT IS NOT THE ONE THE BRIEF PREDICTED

`Flush` is ported, byte-faithful, registered at 0x18011ba40, and verified
against the shipped body in **9,435 records / ~23,800 vertices / 0 divergences**
with all four negative controls firing.

**But the brief's central claim — "porting `Flush` is what makes those ~11,400
executions verifiable" — is FALSE as stated, and this lane measured that rather
than inheriting it.** §4 has the number. The short version: on the driver the
brief named, `Flush`'s vertex loop runs **zero times**, so the live oracle this
lane built is zero-effectiveness there, and the evidence above comes from a
**driven probe** instead. The reason is structural and is worth more than the
port: the three functions whose branches are unverified reach OUR `Flush`, but
the records they would flush are appended by the DLL's own `AddSpriteDraw`
through a direct CALL no registry can redirect. Porting `Flush` does not put
those branches under an oracle. §4 says what would.

---

## 2. What the function is, and the four things that had to be read

591 instructions, 0x18011ba40..0x18011c698. It drains `_draws` into the
Video::Context's dynamic vertex buffer — six vertices per sprite (two
triangles), two per line — then clears the batch and the sheet cursor.

Four facts were settled from the DISASSEMBLY, not from Ghidra's C, because the
decompiler's rendering of each is either wrong or unusable
(`reference_ghidra_c_output_vs_disassembly.md`):

**(a) The vertex is 32 bytes with FOUR slots, and Ghidra's locals hide it.**
Retail keeps four vertex slots on its frame — `RSP+0x28` (A), `RSP+0x48` (B),
`RBP-0x70` (C), `RBP-0x50` (D), with `RBP == RSP+0x100` for this frame
(`LEA RBP,[RSP-0x1d8]` then `SUB RSP,0x2d8`). Ghidra prints them as ~40
unrelated `local_2xx` scalars with the uv temporaries interleaved between slots
B and C, so the struct is only visible in the stores. Every slot has the same
shape:

```
+0x00 x   +0x04 y   +0x08 z(always 0)   +0x0c color (raw dword)
+0x10 u   +0x14 v   -- the sprite sheet's UV, sprite+0x24..+0x30
+0x18 cu  +0x1c cv  -- the CORNER uv: A(0,0) B(0,1) C(1,1) D(1,0)
```

Corners: A=(x1,y1) B=(x1,y2) C=(x2,y2) D=(x2,y1). Emit order **A, D, C, A, C,
B** (0x18011c4ae..0x18011c58d) — (TL,TR,BR) then (TL,BR,BL).

**(b) The rotation is in DEGREES and the identities are NOT folded.** The
rotated path builds three real `Math::Transform2`s — `T(-centre)`, `R(angle)`,
`T(+centre)` — with 1.0f and 0.0f stored as data, and multiplies them with two
full 2x3 multiplies. The port reproduces that instead of folding to
`R` with a precomputed translation. Folding is algebraically identical and NOT
bit-identical (`reference_moved_term_hides_where_terms_cancel.md`): `0.0f *
-sin` is `-0.0f`, and `-0.0f + x` differs from `x` when `x` is `+0.0f`. Every
`ADDSS` order was copied term for term; the shape is
`(a[i][1]*b[1][j]) + (a[i][0]*b[0][j])`, row-1 term FIRST, translation adding
`b.t` last.

Constants lifted from the shipped `.rdata`, never retyped
(`reference_crt_coefficients_must_be_lifted_not_retyped.md`):
`0x18016167c = 0x3c8efa35` (deg→rad), `0x180161694 = 0x3f000000` (+0.5f),
`0x180161754 = 0xbf000000` (−0.5f), `0x18016169c = 0x3f800000` (+1.0f),
`0x1801617c0 = 0x80000000` (the sign flip that makes `-sin`, so `sin == +0.0f`
becomes `-0.0f` and not `+0.0f`).

`sinf`/`cosf` go through `RtLibm::Sin/Cos` — the PORTED MSVC CRT bodies from
lane L — so this file opens no door for them.

**(c) The LINE path does NOT subtract the half-pixel offset.** Sprites get
`- halfPixel` on every coordinate; lines convert the four ints and use them raw
(`CVTDQ2PS` at 0x18011bceb/0x18011bcee with no `SUBSS`). That asymmetry is
retail's and is kept.

**(d) A failed `BeginVertices` SKIPS the chunk AND skips `EndVertices`.**
`JMP 0x18011c5e6` at 0x18011bc0f jumps past the `CALL [RAX+0x78]`. The chunk cap
is `(isLine ? 2 : 6) << 12` **records**, i.e. 24,576 sprites or 8,192 lines.

---

## 3. Door arithmetic — 1 closed, 0 opened

Measured with `tools/addr_door_audit.py` and `tools/unsatisfied_export_scan.py`,
each run from its own pristine `git archive` extraction of the tree in question,
because those scripts walk `os.walk("runtime")` relative to the CURRENT
DIRECTORY and not to their own location — the trap lane UIF documented in its §7
and nearly published a null result from.

| Instrument | base `085782d` | `wave9/lane-flush` | delta |
|---|---|---|---|
| `addr_door_audit` — **UNREGISTERED** | **15** | **14** | **−1** |
| `addr_door_audit` — by-address call sites | 299 | 301 | +2 |
| `addr_door_audit` — distinct addresses | 212 | 212 | **+0** |
| `addr_door_audit` — registered addresses | 1226 | 1227 | +1 |
| `addr_door_audit` — non-literal call sites (blind spot) | 15 | 15 | +0 |
| `unsatisfied_export_scan` — names with no port and no mint | **114** | **114** | **±0** |
| … of those, outside a diff guard | 114 | 114 | ±0 |
| `abi_retslot_audit` | 0 suspicious of 48 | 0 suspicious of 48 | ±0 |
| `rawdoor_audit` — UNGUARDED-CALL | 0 of 642 | **0 of 645** | +3 scanned |

`distinct addresses` is unchanged at 212 because `Flush` and
`Array<SpriteDraw>::SetSize(0)` were ALREADY by-address call sites in
`spritecontext.cpp`; this lane moved one of them out of `UNREGISTERED` and added
no new address. The two extra call sites are this file's own `ResolveAddr`
spellings of those same two addresses.

**Set-diff, address doors CLOSED (1):**

```
0x18011ba40  UI::SpriteContext::Flush
```

**Set-diff, address doors OPENED: none.** Every callee is already satisfied and
that was checked one at a time rather than asserted:

| Callee | How it is reached | Status |
|---|---|---|
| `MaterialInstance::Setup` 0x180102c90 | exported NAME, `RT_EXPORT_ONCE` | already an open export door (`code/Game/Player/tool_base_tilerender.cpp:100`) — the unsatisfied-export SET does not grow |
| `sinf` 0x180127d90 / `cosf` 0x180126d10 | `RtLibm::Sin/Cos` | ported (lane L) |
| `Array<SpriteDraw>::SetSize(0)` 0x1800783d0 | `ResolveAddr`, same LITERAL spelling `spritecontext.cpp` uses | ported (lane UIF); one door, not two |
| `memcpy` 0x18008a290 | `std::memcpy` | not a door |
| `Device::_singleton` 0x1801fe7a8 | data address | already minted |
| ContextData `+0x2160` vtable slots 0x70/0x78 | virtual call | not a door |

The literal spelling of `0x1800783d0` is not a style choice. `addr_door_audit.py`
matches a LITERAL inside `ResolveAddr(Module::Runtime, …)` and is blind to the
same fact spelled through a variable
(`reference_literal_grep_blind_to_variables.md`) — using a helper here would have
made the door invisible while pretending the lane opened none.

### 3.1 Two claims in §3 that were CHECKED rather than asserted

* **"the unsatisfied-export SET does not grow"** — the two 114-name lists were
  diffed, not just counted: `comm` reports NOTHING closed and NOTHING opened,
  and `?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z` is present
  in BOTH. A count of 114 → 114 would have been consistent with one name closing
  and another opening.
* **"one door, not two" for `0x1800783d0`** — `distinct addresses` stayed at 212
  across the change, which is the mechanical form of that claim.

---

## 4. **THE MEASUREMENT THAT CONTRADICTS THE BRIEF**

The brief's premise, stated twice: `Flush` "is why ~11,400 live executions carry
no differential evidence … porting `Flush` is what makes them verifiable."

Measured on exactly the driver it named
(`BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=utility BANISHED_RT_DIFF=1
BANISHED_RT_SPRITECTX_PROBE=0`), with `Flush` ported and registered:

```
UI::SpriteContext::Flush        calls=0   worked=0    divergences=0
[FlushOracle] calls=2520 oracled=0 | records=0 (lines=0 rotated=0)
[FlushOracle] not oracled: noSheet=1260 emptyBatch=1260 noDevice=0 noRawDoor=0 arena=0
```

**`Flush` is entered 2,520 times and its vertex loop runs ZERO times.** Half the
calls arrive with `_currentSheet` null; the other half with an empty batch. Not
one record is ever drained by our body.

**Why**, and this is the part worth keeping:

* The only bodies that reach OUR `Flush` are the three lane UIF ported —
  `SetScissor`, `AddLine`, `FlushDeferred` — because `ResolveAddr` consults the
  port registry.
* Everything the DLL's own `UI::UserInterface::Draw` batches is appended by the
  DLL's `AddSpriteDraw` and drained by the DLL's `Flush` through **direct
  `CALL rel32` instructions inside the DLL**, which no registry can redirect.
* And our `AddSpriteDraw` reports `calls=5040 worked=0` — every record it sees
  is CULLED, so `_draws` is empty whenever our code flushes.

So a live `divergences=0` from this port would have been **0 of 0** — the exact
shape of `reference_uncalled_port_is_not_verified.md`, arriving with a busy
`calls=` column to hide behind. The `worked=` column is what caught it, which is
the third time this wave that instrument has earned its keep.

**What WOULD close the brief's gap** (not done here, and offered as a finding
rather than a plan): a DETOUR at 0x18011ba40, not a registration. With `Flush`
now ported, a detour that redirects the shipped `SetScissor`'s and `AddLine`'s
own nested `CALL 0x18011ba40` into a RECORDING NO-OP would make those three
bodies replayable — the replay would no longer submit a second draw. That is
what "porting `Flush` makes them verifiable" is true of. Registration alone is
not, because the calls that matter are internal to the DLL.

---

## 5. What IS verified, and how

### 5.1 The live oracle (built, correct, currently zero-effectiveness)

`Flush` runs for real; the SHIPPED body then runs on a scratch `SpriteContext`
whose `_context` is a **byte copy of the live Context** with `+0x2160` swapped
for a fake ContextData this file owns, so it writes its vertices into our arena
and reaches no GPU. Our body tees its own vertices into a second arena.

The reason a scratch Context is possible at all — the thing Wave 7 lane F's
hold-back assumed was impossible and nobody had checked — is that
`MaterialInstance::Setup` (0x180102c90) and the one function it calls,
`VideoProgram::SetupShaders` (0x1800f4520), are **entirely compare-guarded field
writes**: "if the context's field differs from the material's value, store it and
set a dirty bit". No allocation, no refcount, no backend call. Run on a copy of a
context that has ALREADY had that same `Setup` applied, both bodies write
nothing. That was read, not assumed
(`reference_holdback_blockers_are_stale_check_them_first.md`).

It compares: the vertex bytes, the `Begin/EndVertices` argument log, the four
context scissor ints, the context dirty mask, `_draws._size`, `_currentSheet`.
It does NOT compare the whole 0x2168-byte context — comparing bytes neither side
writes reports 100% divergence on noise
(`reference_oracle_must_not_compare_unwritten_bytes.md`).

It also carries a HARNESS check, reported separately from divergences: the
scratch gets its OWN copy of the records, and if the shipped `SetSize(0)` ever
moved the data pointer, lane UIF §6's no-free proof would be wrong and the oracle
unsound. It never fired.

### 5.2 The driven probe — `BANISHED_RT_FLUSH_PROBE=1` (the real evidence)

Runs once, at the first `Flush` call that finds the device up and a real context
and sheet — the earliest moment the shipped body can be called at all. (Lane
UIF's probe faulted at `Runtime-x64.dll+0x11BA66` for running before that
moment; this one is placed where the precondition already holds rather than
tripwired after the fact.) BOTH sides run against synthetic objects — a scratch
Context each with the fake backend, a synthetic sheet, a synthetic sprite table,
a synthetic batch — so the live frame is untouched. The only thing borrowed from
the live object is the real `MaterialInstance*`, because `Setup` must walk a real
one.

The shipped side is reached through **`ResolveRuntimeRaw`, never `ResolveAddr`** —
this lane registers 0x18011ba40, so `ResolveAddr` would hand back our own body
and the comparison would be true by construction
(`reference_recheck_must_match_its_own_proof.md`).

Final run, binary md5 `141d605d81fa46341338e3b774471132`, rc=0, empty crash log:

```
[FlushProbe] 6 cases, 0 divergences vs the shipped body.
[FlushProbe] effectiveness: records=9435 (lines=8197 rotated=721 zeroUv=40)
             multiChunkCases=1 mapFailChunks=1 | defect=0
```

| Case | Records | What it is for |
|---|---|---|
| axis-aligned sprites (512, + negative indices) | 512 | the six-vertex fast path, sheet UV, corner UV, inverted rects, every 37th record with a negative `spriteIndex` (retail's "no sprite" → four zero UVs, not a table read) |
| rotated sprites (721, −360..+360 by 0.5) | 721 | the transform chain: two 2x3 multiplies, a sin/cos pair and eight point transforms per record. A cardinal-angle sample could not distinguish a correct chain from one that happens to agree at 0/90/180 |
| lines | 4 | the two-vertex path and primitive type 2 |
| `BeginVertices` FAILS | 3 | the skip-the-chunk-and-skip-`EndVertices` branch, unreachable in a successful run |
| multi-chunk line batch (8193) | 8193 | the `vertsPer << 12` chunk split — lines, because the sprite equivalent (24,577) would cost 4.7 MB of arena per side |
| scissor push fires | 2 | forces `+0x1e0..+0x1ec` and the 0x400 dirty bit to be WRITTEN rather than left equal by never being touched |

Roughly **23,800 vertices compared byte for byte**, including the raw `color`
dword and both UV pairs.

### 5.3 Negative controls — all four FIRE, and the count is explained

| Control | Injects | Cases that diverged | Why that count is right |
|---|---|---|---|
| `=1` | swaps two emitted vertices (`A,C,D,A,D,B`) | **3 of 6** | only the three SPRITE batches; the other three are line batches, which never take that path |
| `=2` | vertex B's corner-v set to 0 | **3 of 6** | same three |
| `=3` | transposes the rotation matrix (`sin` ↔ `-sin`) | **2 of 6** | only the two batches containing a non-zero rotation |
| `=4` | forces `halfPixel = 0.5f` on OUR side only | **3 of 6** | the three sprite batches; lines do not subtract it (§2c) |

Control 4 exists because of an HONEST LIMIT, §7 item 1. **It did not fire on its
first attempt**, and the cause is recorded rather than quietly repaired: the
override was placed AFTER the probe hook, so the probe ran with the unmodified
value. That is the effectiveness control catching the effectiveness control's own
wiring, and it is exactly the failure `reference_probe_needs_an_effectiveness_control.md`
exists to make visible.

### 5.4 A defect the probe found IN ITSELF, on its first run

The probe's first run reported **6 divergences in 6 cases**. Every one was
`Begin/EndVertices ARGUMENTS`, "ours 2 chunks, theirs 1", with the vertex BYTES
byte-identical in all six. Cause: in the probe both sides go through the fake
backend, which already logs — and our side ALSO set the body-side `g_log`, so our
calls were counted twice and theirs once. The instrument was measuring itself.
Fixed by not arming the body-side tee/log in the probe (the live oracle still
needs both, because there our body runs against the REAL ContextData, which logs
nothing). Recorded in the source at the fix site, not just here.

---

## 6. What was DECLINED, with the closure RE-TAKEN

The brief asked for the `Draw` closure to be re-taken because one row had moved.
Both were re-taken from the current tree, callee by callee.

### `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z` — 0x180121890, 614 insns

| Callee | Lane UIF's status | Status NOW |
|---|---|---|
| 0x1800fe960 `Graphics::Camera::Camera` | ported | ported |
| 0x18001a150 `Video::Context::SetCamera` | ported (`Matrix::Inverse` landed in `f5dbeb8`) | ported |
| 0x18011c890 `AddSpriteDraw` | ported (lane UIF) | ported |
| 0x1800783d0 `Array<SpriteDraw>::SetSize(0)` | ported (lane UIF) | ported |
| 0x1800c6c00 `MemFree` | ported | ported |
| **0x18011ba40 `Flush`** | **unported — a door** | **PORTED — this lane. THE ROW MOVED.** |
| 0x18011d2b0 `SpriteContext::Begin` | new door, "at least 3 behind it" | still a door, but **2 behind it, not 3** |
| 0x1800d8a60 `SpriteCapture::EndAddingSprites` | new door | still a door — but it makes **ZERO calls** and is capture-only |

Two rows moved, not one. `Flush` is gone from the list, and behind `Begin` (40
insns) the three callees are now `0x18001a6b0 Video::Context::SetTransform`
(**ported and registered**), `0x1800370c0` (78 insns, unnamed) and `0x18004ba90`
(115 insns, unnamed).

**So the honest cost of `Draw` today is: close 1 export door, and port FIVE
bodies to open none** — `Draw` (614) + `Begin` (40) + `EndAddingSprites` (self
contained, capture-only) + the two unnamed helpers (78 + 115) ≈ 847 instructions
beyond `Draw` itself. Stopping short of any of them opens 1–2 address doors for
the 1 export door closed, which is the arithmetic the wave is steering away from.

`Draw` is also not replayable in any form: it drives the whole UI frame, walks
every dialog, and submits. There is no early-out branch that a partial oracle
could sit on. Porting it now would land ~1,500 instructions with no oracle at
all, in a wave whose point is that verification must be spent before the DLL goes
away. **Declined**, and the table above is what the next lane should cost it from
— it is a genuinely better proposition than lane UIF found it, just not a
secondary-sized one.

### `?Update@UserInterface@UI@@QEAAHAEBVTime@System@@@Z` — 0x1801213c0, 302 insns

Re-taken the same way. **Nothing moved.** None of
`0x18011f790 CleanRemovedDialogs`, `0x180120f20 CloseDialog`,
`0x18011fc60 UpdateEvents`, `0x180113980 DialogInstance::Update`,
`0x180113d90 ComputeLayout`, `0x18011fa40` (the 58-insn dialog-list walk) or
`0x180033690` (Array grow) is registered anywhere in the tree — checked by
grepping each address for a `RegisterRuntime*` call, which returned nothing.

So lane UIF's 1-for-5 stands unchanged, and its second argument stands with it:
`Update` drains `_addDialogs` allocating a node each, frees the array, integrates
the fade, updates every dialog and consumes input from
`Input::Device::_singleton`. It is not idempotent and has no early-out branch big
enough for a partial oracle. **Declined.**

---

## 7. The latent ABI defect — FIXED

`engine/funcs/engine_funcs.h` declared both `SetScissor` overloads WITHOUT the
hidden return slot:

```c
typedef void (*fn_SpriteContext_SetScissor4_t)(void* self, int x, int y, int w, int h);
typedef void (*fn_SpriteContext_SetScissorVP_t)(void* self, void* viewport);
```

Both return `Video::Viewport` **by value**, so the caller passes a pointer to a
16-byte slot it owns and, for a member function, that pointer is the SECOND
argument in RDX. Calling either typedef would have written 16 bytes through a
**sign-extended `int`** and still returned the right answer, because the callee
hands the same address back in RAX. That is the shape that shipped live for a
whole wave as `fn_Math_Transform3_Identity`.

Latent, not live: nothing calls them —
`code/Game/Component/frustumoutlineelement.cpp:184-196` declares its own correct
typedefs and explains why. But two correctly named, wrongly shaped pointers
sitting beside a correct one are a trap for the next reader, and
`tools/abi_retslot_audit.py` scans exactly this file.

Both now carry the slot, with the evidence in the header comment:
`MOV RBX,RDX` at 0x18011d010 / `MOV RAX,RBX` at 0x18011d03d, `MOV RDI,RDX` at
0x18011d050 / `MOV RAX,RDI` at 0x18011d1c6, plus the independent frame argument:
the 4-int overload's `PUSH RBX; SUB RSP,0x30` frame reads its 5th and 6th
arguments at `RSP+0x60`/`RSP+0x68` — caller-frame +0x28/+0x30, the FIFTH stack
slot, so four register arguments precede `w`.

---

## 8. HONEST LIMITS

1. **The half-pixel offset is 0.0f in this process and cannot be verified
   against a non-zero value.** DeviceData vtable slot 5 is
   `dx11/src/devicedata_stubs.cpp`'s `DeviceData_Stub5_ZeroFloat`. Every
   `- halfPixel` in every run above subtracted zero. Control 4 proves OUR body
   reads the value; it does not prove the two bodies would agree on a DX9
   backend, because there is no DX9 backend in this process to ask.
2. **The live oracle has never oracled a single call.** `oracled=0` in every run.
   It is correct code and it is untested against a real batch. Its arena, its
   context copy and its verdict have only been exercised through the probe's
   shared helpers, not through the live arm itself.
3. **The probe's evidence is SYNTHETIC.** Real sprite indices, real UV values,
   real coordinates and a real `MaterialInstance` were not used — the sheet, the
   sprite table and the batch are this lane's own. What that cannot catch is a
   field offset that is wrong in a way both sides read consistently (e.g. if
   `kSheetSprites` were the wrong offset, both bodies would read the same wrong
   place). The offsets are pinned by disassembly, not by the probe.
4. **`_spriteCapture` is not on `Flush`'s path at all**, so nothing here says
   anything about the capture family.
5. **The chunk-split branch is verified for LINES only** (8,193 records, 2
   chunks). The sprite split at 24,577 records was not run: 4.7 MB of arena per
   side. The code path is shared — the cap is the only thing that differs — but
   that is an argument, not a measurement.
6. **`worked=0` on the `Flush` site is real and permanent on this driver.** A
   future reader must not read `divergences=0` on that row as coverage; §4 is
   what that row means.
7. **The 9,435 records are ONE probe run, not a long soak.** The probe runs once
   per process by design.
8. **No DLL-less cutover boot was run** (per the brief).
9. **`MaterialInstance::Setup` is called through `RT_EXPORT_ONCE`, i.e. the
   shipped body.** At cutover it must be ported or `Flush` will call null. It is
   an already-counted export door, but it is on `Flush`'s hot path now.
10. **The probe's raw door is gated on `BANISHED_RT_FLUSH_PROBE`, not on
    `BANISHED_RT_DIFF`.** `rawdoor_audit` classes it DEFERRED-CALL and reports 0
    UNGUARDED-CALL, and it null-checks `ResolveRuntimeRaw` and prints `SKIPPED`
    when the DLL is absent — so it is cutover-safe. But it is a second gate, and
    a reader looking only for `rt_diff_on__` will not find it.

---

## 9. Gate results

| Gate | Result |
|---|---|
| `mingw32-make standalone-debug` | clean; the only warning in the touched files is the pre-existing `standalone/allocator_lock.cpp:210` |
| Live run, minimap cell, `BANISHED_RT_DIFF=1` | rc=0, zero-byte crash log, binary md5 `141d605d81fa46341338e3b774471132` |
| Gameplay smoke run (`BANISHED_AUTOSTART=1`, no diff) | rc=0, zero-byte crash log — `Flush` is our body on every `SetScissor` |
| Probe | 6 cases, 9,435 records, **0 divergences** |
| Negative controls 1/2/3/4 | 3 / 3 / 2 / 3 cases diverged — **all four FIRE** |
| Tree-wide `[RtDiff]` | 548 sites, 15,946,733 calls, 1 divergence — `Object::Factory::Startup [probe, not diffable]`, pre-existing and not this lane's |
| Harness check (shipped `SetSize(0)` moved the record pointer) | never fired |
| Game-dir lock | acquired in the FOREGROUND as `lane-FLUSH`, released, `.lane_lock/` verified gone |
| `addr_door_audit` set diff | CLOSED `{0x18011ba40}`, OPENED `{}` |
| `unsatisfied_export_scan` set diff | CLOSED `{}`, OPENED `{}` (114 → 114, same names) |
| `abi_retslot_audit` | 0 suspicious of 48, both trees |
| `rawdoor_audit` | 0 UNGUARDED-CALL, denominator 642 → 645 |

---

## 10. Files

| File | Change |
|---|---|
| `runtime/src/ui/spritecontext_flush.cpp` | NEW — the port, the live oracle, the driven probe |
| `runtime/src/ui/spritecontext.cpp` | registers the Flush port from `RtUiSpriteContextRegister()` |
| `standalone/main.cpp` | prints `[FlushOracle]` beside `RtDiffSummary()` |
| `engine/funcs/engine_funcs.h` | §7, the hidden return slot on both `SetScissor` typedefs |
| `docs/runtime_port/WAVE9_LANE_FLUSH.md` | this file |
