# Wave 7 — Lane F — `Sprite` / `SpriteSheet` / `SpriteContext` / `StringTable` / `UI::String`

Branch `wave/upgrade-path-complete`. Files owned by this lane:
`runtime/src/ui/sprite.cpp`, `runtime/types/ui/Sprite.h`, `SpriteSheet.h`,
`SpriteContext.h` (new), `StringTable.h` (new), `String.h` (new), this document.

**Result: 20 of 23 ported, 3 held back with a written reason.**
`RtUiSpriteRegister()` returns **20**. `-fsyntax-only` with the wave's exact
command line is **warning-clean** (`g++ 13.1.0`, verified this session).
`tools/abi_retslot_audit.py` reports **0 suspicious of 48 by-value-returning
exports** with none new in this lane. `sizeof(UI::Sprite) == 0x10` (unchanged
from the pre-existing header, now re-verified — see §3).

Not run by this lane, by instruction: the game, `make`, `tools/rt_run.sh`,
`sa-run`, `git commit`. Every runtime number below is therefore a *prediction
of what the oracle will report*, not a measurement — see §7.

---

## 1. The function table

All 23 mangled names were checked against the shipped export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with `tools/pe_exports.py`'s reader
**before any body was written**, and every retail address it returned matches
the address already recorded in `docs/runtime_port/wave7_lanes.txt`
(`reference_resolveexport_phantom_names.md` — a name the DLL does not contain
resolves NULL in silence and the census still counts the port as done, so this
check is not optional).

| Export | Retail | Ported | Oracle |
|---|---|---|---|
| `??0Sprite@UI@@QEAA@XZ` | 0x18011ba10 | yes | replay |
| `??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@AEBVString@System@@@Z` | 0x18011b9c0 | yes | replay |
| `??0Sprite@UI@@QEAA@V?$ExternalPtr@VSpriteSheet@UI@@@Object@@H@Z` | 0x18011ba30 | yes | replay |
| `?IsValid@Sprite@UI@@QEBA_NXZ` | 0x180030c40 | yes | replay |
| `?Width@Sprite@UI@@QEBAHXZ` | 0x180030bf0 | yes | replay |
| `?Height@Sprite@UI@@QEBAHXZ` | 0x180030c10 | yes | replay |
| `??0SpriteSheet@UI@@QEAA@XZ` | 0x18011d600 | yes | replay |
| `?GetSpriteCount@SpriteSheet@UI@@QEBAHXZ` | 0x180003190 | yes | replay |
| `?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z` | 0x1800031f0 | yes | replay |
| `?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z` | 0x180003330 | yes | replay |
| `?SetSpriteInfo@SpriteSheet@UI@@QEAAXHHHMMMM@Z` | 0x180003460 | yes | scratch |
| `?AddSpriteInfo@SpriteSheet@UI@@QEAAXAEBVString@System@@0HHHH@Z` | 0x180003390 | yes | scratch |
| `?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z` | 0x18011cbe0 | **no** | — |
| `?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z` | 0x18011d050 | **no** | — |
| `?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z` | 0x18011d010 | **no** | — |
| `?GetCount@StringTable@UI@@QEBAHXZ` | 0x180002610 | yes | replay |
| `?GetText@StringTable@UI@@QEBA?AVString@System@@H@Z` | 0x180002550 | yes | replay* |
| `?GetText@StringTable@UI@@QEBA?AVString@System@@AEBV34@@Z` | 0x1800025b0 | yes | replay* |
| `??0String@UI@@QEAA@XZ` | 0x18011da40 | yes | replay |
| `??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@AEBV0System@@@Z` | 0x18011da60 | yes | replay |
| `??0String@UI@@QEAA@V?$ExternalPtr@VStringTable@UI@@@Object@@H@Z` | 0x18011da20 | yes | replay |
| `?IsValid@String@UI@@QEBA_NXZ` | 0x180035060 | yes | replay |
| `??BString@UI@@QEBA?AV0System@@XZ` | 0x180035020 | yes | replay* |

`replay*` = replay into a scratch retval, then **release** the extra
refcounted reference the replay created (§4), so the net effect on shared
state is zero even in diff mode. `scratch` = byte-compare against an isolated
scratch replica (§4).

---

## 2. The three hold-backs

### `SpriteContext::AddLine`, `SpriteContext::SetScissor` (both overloads)

All three reach unexported, unreversed internal helpers — `Flush`,
`FlushDeferredFonts`, `AddSpriteDraw`, `SpriteCapture::AddScissor` — that
submit into the **live Video::Context / GPU vertex buffer** the SpriteContext
wraps, not into memory the object itself owns:

```
AddLine (0x18011cbe0):
    if sheet changed: Flush(this); this->currentSheet = sheet;
    build a SpriteDraw record on the stack, call AddSpriteDraw(this, &record)

SetScissor(Viewport const&) (0x18011d050):
    if the new (post-scale) rect == the current one: return the old rect, done
    else:
        Flush(this); FlushDeferredFonts(this);
        if this->spriteCapture: SpriteCapture::AddScissor(...)
        write the new rect, return the old one (pre-scale)

SetScissor(int,int,int,int) (0x18011d010): two-line forwarder into the above.
```

`Flush` and `FlushDeferredFonts` are exactly what their names say: they
submit whatever is batched so far to the GPU. This is a **materially
different hazard** from every other mutator in this lane. `SetSpriteInfo` and
`AddSpriteInfo` mutate a heap block the `SpriteSheet` object owns, so a raw
byte copy of that block fully isolates a replay. `AddLine`/`SetScissor`
mutate state that lives **outside** the object — the bound `Video::Context` —
so even a byte-perfect scratch copy of the `SpriteContext` struct itself
would still hold the SAME live context pointer, and calling the DLL's real
body on that "scratch" object would submit a second, real, visible draw or
scissor change. That is not a leak (AddSpriteInfo's worst case) or a spurious
divergence (a bad byte compare) — it is the game visibly drawing garbage
twice per diffed call.

Reversing `Flush`/`AddSpriteDraw`/`SpriteCapture::AddScissor` far enough to
build a *safe* oracle (e.g. one that redirects the submission into a private
scratch `Video::Context`) is out of this lane's scope — it would mean porting
or shimming three more unexported functions and, transitively, understanding
the SpriteContext↔Video::Context relationship this lane never needed to
reverse for anything else. `runtime/types/ui/SpriteContext.h` records the
partial layout this lane read while making this argument (the scale factor at
+0x00, the current-sheet cursor at +0x08, the scissor rect at +0x20..+0x2C,
the `SpriteCapture*` at +0x80) so the next reader who reconsiders this does
not start from zero.

None of the three has a written-down caller count in this lane (that survey
is `code/Game/Component/frustumoutlineelement.cpp` for both `SetScissor`
overloads and `standalone/address_registry.cpp` for all three) — they are
reachable, just not portable with the evidence this lane gathered.

---

## 3. Layout — three headers, one correction

### `Sprite.h` — RE-VERIFIED, no correction needed

Every claim in the pre-existing header checked out against a fresh decompile:
ctor addresses, the default/by-name/by-index bodies, `IsValid`'s
index-first-then-sheet short-circuit order. Added: `Width`/`Height`
addresses (0x180030bf0 / 0x180030c10) and `kSpriteWrittenBytes = 0x0C`, the
byte count a diff oracle may compare (`pad0C` is never written by any of the
three ctors — comparing it against a freshly poisoned scratch object is
exactly the trap `reference_oracle_must_not_compare_unwritten_bytes.md`
describes).

### `SpriteSheet.h` — CORRECTED

The `SpriteInfo` entry layout was wrong in the version this lane inherited:

| Field | Old header said | Actually (this lane) |
|---|---|---|
| width | +0x18 | **+0x1C** |
| height | +0x1C | **+0x20** |
| u0, v0, u1, v1 | +0x20, +0x24, +0x28, +0x2C (guessed "UV or additional data") | **+0x24, +0x28, +0x2C, +0x30** |
| (unnamed) | not present | **+0x18, always zero** — no read site in the tree touches it |

Evidence (full derivation in the header's comments): `AddSpriteInfo`
(0x180003390) builds a 0x38-byte temporary on its stack from four scalar
writes — a `CONCAT44(y,x)` qword, a `(width << 32)` qword, a plain `height`
qword and two zeroed qwords — and reading that construction field by field
places `x`/`y` at +0x10/+0x14 (the old header had this right), an
always-zero dword at +0x18, `width` at +0x1C and `height` at +0x20.
Independently, `Sprite::Width` (0x180030bf0) reads `entry+0x1C` and
`Sprite::Height` (0x180030c10) reads `entry+0x20` — two one-line accessors
with nothing else in play, so there is no ambiguity about which name belongs
to which offset. `SetSpriteInfo` (0x180003460) writes width/height to the
same two offsets a third time and writes its four float parameters to
+0x24/+0x28/+0x2C/+0x30, and `GetUVs` (0x180003330) reads those same four
offsets back in the same order. Nothing in the current tree ever read the old
(wrong) offsets, so this was a latent defect, not a live one — but the next
Wave 7 lane to touch a sprite sheet's raw entries (none do today) would have
inherited it silently.

Also added: `??_7SpriteSheet@UI@@6B@` IS exported (0x180154590) — the ctor
stamps it rather than needing to clone or mint a vtable, same lever
PLAN.md §2.2 item D already used for `Graphics::Geometry` in Wave 6.

### `StringTable.h`, `String.h`, `SpriteContext.h` — NEW

`StringTable.h` deliberately does **not** declare a full `StringTable` struct
with a `sizeof`: nothing in this lane's scope constructs or destroys one (its
ctor is not one of the 23 functions), so there is no evidence for anything
past `+0x28`, and inventing a size no read site needs is exactly the PLAN
§4.3 risk 1 trap ("a shared layout got wrong silently corrupts every class
that embeds it"). It documents `TextEntry` (0x10 bytes: `name`, `text`, both
refcounted `System::String` data pointers) and the two offsets
(`+0x20` data, `+0x28` count) that `GetCount`/`GetText`/`GetIndexFromName`
all agree on — the same `{data,size,capacity}` shape as
`Collection::Array<T>`.

`String.h` declares `UIString` (not `String`, to avoid colliding with
`RtSystem::String` if both headers are ever included together) — byte-for-byte
the same 0x10-byte `{handle, index, pad}` shape as `Sprite`, which is also
independently corroborated by lane A's own local `UiString` in `Element.h`
(built from `SetToolTip`'s two-qword copy, a completely different call site).
Three independent readings of "a handle-plus-index value type in this engine"
landing on the identical shape is about as much cross-validation as this
project gets without a PDB.

`SpriteContext.h` is intentionally partial — see §2.

---

## 4. Two oracles

### (a) Replay — every pure read and every ctor

None of these has a side effect beyond writing to `this` or the return slot.
The ctors that call `SpriteSheet::GetSpriteIndexFromName` /
`StringTable::GetIndexFromName` only **read** the table; they never mutate
it. So calling both implementations on the same inputs and comparing is
always safe:

- **Scalar reads** (`Sprite::Width/Height/IsValid`, `SpriteSheet::
  GetSpriteCount/GetSpriteIndexFromName`, `StringTable::GetCount`, `UI::
  String::IsValid`): `RT_DIFF_SCALAR(ours, RtOriginal<...>()(...))`.
- **`GetUVs`**: both write four floats to caller-provided out-parameters;
  `RT_DIFF_BYTES` over both 16-byte stack arrays.
- **The six ctors** (`Sprite` x3, `SpriteSheet`, `UIString` x2 plus the
  default): construct into a poisoned (`0xCD`) scratch buffer with the DLL's
  own ctor, `RT_DIFF_BYTES` against the real object. `SpriteSheet`'s ctor
  writes all 0x58 bytes (no gaps, confirmed field by field against the
  decompile) so the whole object is compared. The four 0x10-byte value-type
  ctors (`Sprite`, `UIString`) compare only the first **0x0C** bytes —
  `pad0C`/`_pad0c` is never written by any of the six, and comparing it would
  report a divergence on whatever garbage was already in the caller's
  storage rather than on anything either implementation got wrong
  (`reference_oracle_must_not_compare_unwritten_bytes.md`).

**The three `replay*` functions** (`StringTable::GetText` x2, `UI::String::
operator System::String()`) return a NEW OWNED reference to a refcounted
`System::String` on a hit. Replaying the DLL's body a second time bumps the
SAME live string's refcount a second time — a real side effect, but not a
harmful one on its own (see §4b's discussion of the same class of effect for
`AddSpriteInfo`). This lane closes it to exactly zero net effect instead of
accepting a leak: after the byte-compare, `ReleaseStringRef` (in
`sprite.cpp`, reproducing the exact idiom documented in
`runtime/types/system/String.h`) decrements the refcount the replay created
and frees the block if it reaches zero. So running these three under
`BANISHED_RT_DIFF=1` leaves shared string state byte-identical to not running
diff mode at all.

### (b) Scratch replica — `SetSpriteInfo`, `AddSpriteInfo`

Both mutate a `SpriteInfo` array that lives in a **separate heap block** from
the `SpriteSheet` object (a `Collection::Array<SpriteInfo>`, PLAN §4.3 risk
1) — snapshotting the 0x58-byte `SpriteSheet` does not snapshot the array's
contents, so this lane snapshots the one array element that changes instead:

- **`SetSpriteInfo`**: snapshot the target `SpriteInfo` (0x38 bytes) BEFORE
  mutating the real one. Address the snapshot through a fake 0x28-byte
  header whose `+0x20` field is a pointer arithmetic'd back so that
  `fakeHeader + index*0x38` off `+0x20` lands on the snapshot — the only
  field `SetSpriteInfo` ever reads (it never reads a count or a vtable).
  Run the DLL's real `SetSpriteInfo` on that fake header with the identical
  arguments, then `RT_DIFF_BYTES` the mutated snapshot against the mutated
  real entry. No refcounted field is touched (width/height/UVs are all
  scalars), so this has **zero** side effects of any kind.

- **`AddSpriteInfo`**: this one DOES touch a refcounted field (the new
  entry's `name`/`group`), so the isolation has to be a full second array,
  not one struct. Before calling ours, snapshot the pre-call `{data, count}`.
  Run ours for real (see §5 for what "ours" actually is). Then, only inside
  diff mode: allocate a scratch buffer through the DLL's own
  `MemoryAllocator::MemAlloc`, sized to `oldCount + 1` elements (not
  `oldCount` — see below), raw-`memcpy` the `oldCount` existing entries into
  it (a byte alias, not a refcount bump — nothing is constructed), and call
  the DLL's REAL exported `AddSpriteInfo` on a fake header pointing at that
  buffer. `RT_DIFF_BYTES` compares the two newly-appended entries (ours in
  the real array, theirs in the scratch one). Then `ReleaseStringRef` undoes
  the ONE reference the scratch append created on the shared name/group
  strings, and `MemFree`s the scratch buffer — net effect on shared state:
  zero, same guarantee as (a)'s three `replay*` functions.

  **Why capacity is pre-set to `oldCount + 1`, not `oldCount`:** the DLL's
  append routine only copy-constructs (and refcount-bumps) elements it is
  moving into a NEW buffer. If the scratch array's capacity were left at
  `oldCount` (forcing a reallocation), the DLL's own growth helper would
  copy-construct — and refcount-bump — every one of the `oldCount` EXISTING
  entries too, which this oracle would then have to un-bump one at a time for
  no benefit: the old entries are not what this call is verifying. Presizing
  to `oldCount + 1` takes the no-reallocation path, so only the one new slot
  is touched, symmetric with what `SetSpriteInfo`'s isolation already relies
  on.

---

## 5. What "ours" actually is for `AddSpriteInfo`, and why that is safe

`AddSpriteInfo` (0x180003390) is short but not simple: it hand-builds a
0x38-byte temporary, pre-bumps the incoming `name`/`group` refcounts, calls
an **unexported** growth-and-append helper (0x180004e30 — this project's
Ghidra database has no name for it; it is `Collection::Array<SpriteInfo>`'s
append-by-one body, byte-identical in shape to the growth policy
`runtime/types/collection/Collection.h` already documents from four *other*
instantiations), and then releases its own pre-bump once the helper's
internal copy-construct step has bumped the same strings a second time. Net
effect on the strings: **+1** either way — the pre-bump-then-release is
retail's way of holding a temporary reference across a call that might
reallocate and free things, not a second logical increment.

Rather than re-deriving that whole dance, this lane's port packs the new
`SpriteInfo` **without** any refcount bump and calls the DLL's own internal
helper (0x180004e30) directly — the same pattern `runtime/src/picking/
scene.cpp` already uses for several `Array<T>` teardown/insert helpers
(`RtGridAdd`, `RtFreeArrayEntry`, …: "everything it touches is either the
Scene's own arrays or the allocator, so it is layout-compatible with this
port"). The helper's own copy-construct step supplies the one bump the
caller-visible contract needs. This was verified by hand against the
decompile of both functions (0x180003390's temp-building, 0x180004e30's
append, and their shared inner copy-construct 0x180004220 which the exported
`AddSpriteInfo` also relies on) before writing it, not assumed.

This makes the port SHORTER than a literal transcription and, because the
array mechanics are literally the DLL's own code running against the real
object, removes an entire class of "did I get the growth policy exactly
right" risk that a from-scratch reimplementation would carry.

---

## 6. Stale comments this lane corrected

Per PLAN §6 step 8 — a wrong comment is worse than no comment, because it is
believed.

**`runtime/types/ui/SpriteSheet.h`** — the `SpriteInfo` width/height/UV
offsets, corrected at the source (§3), with the old (wrong) values kept
visible in a comment so a reader who meets the old claim elsewhere (nothing
in the tree currently repeats it) recognises it.

No other file this lane read needed a correction. `runtime/src/ui/
sprite_debug.cpp` and `element_sprite_debug.cpp` (inherited from earlier
work, per the brief) were re-read and are accurate as written: both wrap
`fn_Sprite_*` pointers for printf logging and neither makes a layout claim
that this lane's corrected `SpriteInfo` affects (they only touch `Sprite`
itself — `spriteSheet`/`spriteIndex`/`IsValid()` — which was already
correct).

**A defect this lane's OWN output caused, found and fixed by the wave lead
(2026-09-07), recorded here per the same rule:** `runtime/types/ui/String.h`
used `int32_t`, `size_t` and `offsetof` without including `<cstdint>` /
`<cstddef>`. Latent while this file's only includer was `sprite.cpp` (which
happens to pull those standard headers in first via `rt_port.h`), it stopped
being latent the moment lane A's `Element.h` began including this header
directly: a translation unit that includes `String.h` FIRST fails with
`'int32_t' does not name a type`, and `UIString` silently degrades to an
8-byte struct with no `index` and no `pad0C` — every `static_assert` in both
headers then fires at once, eleven errors that name nothing about the actual
cause. Lane A found it (routed rather than edited, since it depends on this
header) and the wave lead applied the two-line include fix directly, noting
it is why the omission mattered: the header worked from THIS lane's own test
(one known includer, already primed) and failed only from a DIFFERENT
lane's, which a single-lane `-fsyntax-only` check cannot catch by
construction. The wave lead's sweep compiled all 26 `runtime/types/ui/`
headers standalone; this was the only failure among them.

**Independent cross-validation this lane did not have alone:** lane A's
`Element.h` recovered the identical `UIString` shape — `{table +0x00, index
+0x08, pad0C +0x0C}`, 0x10 bytes — from `?SetToolTip@Element@UI@@…`
(0x180035740, a two-qword copy with no refcount touch) and
`?IsToolTipValid@Element@UI@@…` (0x180035640, a pointer test plus a signed-
index test), a completely disjoint pair of functions from this lane's three
ctors and `IsValid`. Two independent derivations landing on the same layout
is the strongest confirmation this project gets without a PDB. It also
surfaced an asymmetry `kUIStringWrittenBytes` exists to express: `Element`'s
own `pad0C` (`Element` +0x54) is never written by `Element::Element`, but IS
written by `SetToolTip` (a full qword store at +0x50) — so the byte must be
EXCLUDED from a ctor-replay oracle and must NOT be excluded from a
`SetToolTip`-replay oracle. Same struct, different owning/writing function,
different answer — which is only visible by holding both classes' evidence
at once. `kUIStringWrittenBytes`/`kSpriteWrittenBytes` already caught one
real instance of getting this wrong: lane E had been comparing all 16 bytes
of a `Sprite` whose ctors write only 12.

---

## 7. What this lane could NOT verify

1. **No run happened.** Per the lane brief, this lane did not build, deploy
   or run anything. `RtUiSpriteRegister()` returning 20 and every oracle
   design above are source-level facts, checked by `-fsyntax-only` and by
   hand against the decompile; **zero diffed calls have executed.** Until the
   wave lead runs `BANISHED_RT_DIFF=1`, every row in §1 is "expected clean,"
   not "clean." The kill switch is `BANISHED_RT_UISPRITE=0` (falls back to
   `BANISHED_RT_UI=0` for the whole namespace).

2. **The `AddSpriteInfo` oracle's fake-header trick is pointer arithmetic
   outside an object's declared bounds.** `SetSpriteInfo`'s isolation
   computes `&before - index * sizeof(SpriteInfo)` and stores THAT as the
   fake header's data pointer, relying on the DLL re-adding the same offset
   to land back on `&before`. This is well-defined on every architecture and
   compiler this project targets (plain integer address arithmetic under the
   hood) but is technically outside what the C++ object model guarantees for
   a single local variable, and it has never been exercised. If `index` is
   ever negative here (it is not, by every call site this lane found, and
   retail has no bound check either way to compare against), the arithmetic
   still works but is even further from anything the standard blesses.

3. **The two unexported internal addresses this port leans on
   (0x180004e30, the array-growth-and-append helper; 0x180004220, its
   element copy-construct, read but not called directly) are not registered
   anywhere and have no Ghidra name.** If a later wave renames or re-derives
   them under a different address (e.g. after inlining differences in a
   different compiler pass), this lane's `AddSpriteInfo` port would need a
   corresponding update. `ResolveAddr(Module::Runtime, 0x180004e30)` is the
   only place that address appears.

4. **`StringTable::GetIndexFromName` (0x180002470) and `SpriteSheet::
   GetSpriteNameFromIndex` (0x1800032d0) are read but not ported** — neither
   is one of the 23 functions this lane owns. `GetIndexFromName` is called
   through the DLL's own implementation from three of this lane's ported
   bodies (`StringTable::GetText(String const&)`, both `UI::String`
   by-name-consuming functions); if a later wave ports it differently, this
   lane's callers pick up that change automatically (correct behaviour, but
   worth knowing it is a live dependency and not a frozen assumption).

5. **Nothing here was measured for performance.** Every diff branch is
   `if (rt_diff_on__)`; a normal run pays for a handful of extra comparisons
   in the ctors and none in a normal `AddSpriteInfo`/`SetSpriteInfo` call
   beyond one `RT_DIFF_BEGIN` static. The PLAN 5% gate is deferred (PLAN §5)
   and this lane did not touch it.

6. **`sizeof(UI::Sprite) == 0x10`**, reported to the wave lead per the brief's
   request — unchanged from the pre-existing header, now independently
   re-verified against a fresh decompile of all three ctors and `IsValid`.

---

## 8. Ghidra

Everything in this lane was read with `program="Runtime-x64.dll"` on every
call. `switch_program` was never invoked (the profile build was not needed —
every one of the 23 mangled names already carries its real engine name in
retail's own export table, so there was no `FUN_`/anonymous name to resolve
through the profile build's strings). No function was renamed in the shared
Ghidra project, so no other lane's view of it changed.

---

## 9. Two defects found by the Wave 7 integration run (2026-09-07), fixed

`docs/runtime_port/wave7_integration_save9.txt` (save-load path,
`BANISHED_AUTOLOAD=Save9`) reported two sites at **100% divergence, both
this lane's, both on the ctor/mutator side of `SpriteSheet`** while
`GetSpriteCount`, `SetSpriteInfo` and `GetUVs` ran clean on the same path
with up to 1.15M calls. Both turned out to be the SAME class of mistake —
comparing bytes neither implementation writes
(`reference_oracle_must_not_compare_unwritten_bytes.md`) — in two different
functions, confirmed from raw **disassembly**, not decompiled C, per this
project's standing caution that Ghidra's C can drop a store.

### (a) `UI::SpriteSheet::SpriteSheet()` — calls=12, divergences=12

```
call #1  ours=+0x51: 00 00 00 00 20 00 00   theirs=+0x51: cd cd cd cd cd cd cd
call #2  ours=+0x51: 05 00 00 d6 04 00 00   theirs=+0x51: cd cd cd cd cd cd cd
call #3  ours=+0x51: cc fc fe 00 01 00 00   theirs=+0x51: cd cd cd cd cd cd cd
```

**Verdict: oracle extent too wide — not a port defect.** Disassembly of the
ctor (retail 0x18011d600) ends:

```
18011d65d: MOV byte ptr [RCX+0x50],DL
18011d660: MOV RAX,RCX
18011d663: ADD RSP,0x18
18011d667: RET
```

— a single byte store at +0x50, then `RET`. Nothing in the ctor ever touches
+0x51..+0x57. `theirs` being uniform `0xCD` at every one of those 7 bytes,
on every call, is exactly what "never written" looks like against a
poisoned scratch buffer. `ours` differing per call and looking like
plausible real data is *also* expected and *also* not a bug: it is whatever
the allocator's last tenant left in that memory, on the REAL object, which
the ctor is equally entitled to leave untouched.

**Fix**: `runtime/types/ui/SpriteSheet.h` gains `kSpriteSheetWrittenBytes =
0x51`, with the disassembly excerpt above as its evidence comment.
`RtUi_SpriteSheet_Ctor` in `sprite.cpp` now compares `kSpriteSheetWrittenBytes`
instead of `sizeof(SpriteSheet)` (0x58).

### (b) `UI::SpriteSheet::AddSpriteInfo()` — calls=12, divergences=12

```
call #1  ours=+0x34: 00 00 00 00   theirs=+0x34: 61 6c 4e 6f   ("alNo")
call #2  ours=+0x35: 10 00 00      theirs=+0x35: 20 00 00
call #3  ours=+0x35: 10 00 00      theirs=+0x35: 20 00 00
```

**Verdict: oracle extent too wide — not a port defect, and not the growth-policy
question it looks like at first.** `theirs` is not poison here because this
oracle's scratch buffer was never poisoned (unlike the ctor's) — it is a
fresh `MemAlloc` the code never wrote a sentinel into, so of course it holds
real leftover heap bytes, which is why call #1 happens to look like ASCII.
The 0x10-vs-0x20 pattern on calls #2/#3 is coincidence, not a capacity field
leaking through — there is no growth-policy bug. Disassembly of the shared
element copy-construct helper both implementations go through
(`FUN_180004220`, retail 0x180004220 — called both by this port's direct
call into the internal growth helper 0x180004e30 for "ours", and by the real
exported `AddSpriteInfo` for "theirs") ends:

```
180004289: MOV EAX,dword ptr [RDX+0x30]
18000428c: MOV dword ptr [RCX+0x30],EAX     ; copies v1, 4 bytes, ends at +0x34
18000428f: MOV RAX,RCX
180004292: ADD RSP,0x18
180004296: RET
```

— the last field it copies is `v1` at +0x30 (4 bytes, ending at +0x34), and
it returns immediately after. `SpriteInfo::_pad34` (+0x34..+0x37) is never
read from the source or written to the destination, by construction, on
EITHER path. `oursAppended` (the real array's new slot) and `theirsAppended`
(an entirely separate scratch allocation built for this one oracle call) are
unrelated allocations with no reason for their untouched tails to agree —
the textbook case the "unwritten bytes" note warns about, just with a
non-`0xCD` "theirs" this time because this oracle never poisons its scratch.

This also caught a stale claim in `SpriteSheet.h` itself: the pre-existing
comment on `_pad34` said AddSpriteInfo "zeroes it", which was never true —
this port's own `newEntry._pad34 = 0` in `sprite.cpp` has no effect on the
shipped entry, because the copy-construct helper never copies that field
from the source struct at all. Corrected at the source (PLAN §6 step 8),
citing the same disassembly.

**Fix**: `SpriteSheet.h` gains `kSpriteInfoWrittenBytes = 0x34`.
`RtUi_SpriteSheet_AddSpriteInfo` now compares `kSpriteInfoWrittenBytes`
instead of `sizeof(SpriteInfo)` (0x38). `SetSpriteInfo`'s existing
`sizeof(SpriteInfo)`-wide compare needed no change: its scratch snapshot is
taken by copying the REAL entry's bytes (including whatever garbage already
sat at +0x34) before mutating it, so the untouched tail is byte-identical by
construction on both sides of that particular oracle — the cross-allocation
hazard above does not arise there, which is exactly why it reported 0
divergences on the same integration run.

### What this changes about §1 and §4

No change to the function table (§1) or the port bodies themselves — both
fixes are entirely in the oracle's compare width, not in `RtUi_SpriteSheet_Ctor`
or `RtUi_SpriteSheet_AddSpriteInfo`'s actual field writes. Verified again with
`-fsyntax-only` (clean) and `tools/abi_retslot_audit.py` (0 suspicious of 48,
unchanged) after the fix; not re-run against the game, per this lane's
standing instruction not to build or run.
