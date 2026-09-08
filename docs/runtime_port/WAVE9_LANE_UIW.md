# Wave 9 lane UIW — the UI leaf widgets and the sprite context

**Scope:** the ten symbols `BANISHED_RT_STRICT` recorded unresolved on attempt 14 of the
DLL-less boot that belong to `UI::ComboElement`, `UI::SpriteContext`, `UI::SliderElement`,
`UI::ProgressElement`, `UI::RadioElement` and `UI::GraphElement`
(`docs/runtime_port/wave9_attempt14_unresolved.txt`).

**Result: 4 of 10 closed, 6 declined with the blocking frame named per row.**
Branch `wave9/lane-uiw`, based on integration tip `29cfaa2`. Commit `e0c6087`.

| # | symbol | verdict |
|---|---|---|
| 1 | `?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z` | **PORTED** |
| 2 | `?RemoveAllItems@ComboElement@UI@@QEAAXXZ` | **PORTED** |
| 3 | `?FindCheckBoxes@RadioElement@UI@@QEAAXXZ` | **PORTED** (both overloads) |
| 4 | `?AddGraph@GraphElement@UI@@QEAAXI@Z` | **PORTED** |
| 5 | `?SetCurrentItem@ComboElement@UI@@QEAAXH@Z` | declined — `UserInterface::CloseDialog` (another lane) + `ComboElement::UpdateText` |
| 6 | `?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z` | declined — `TextElement::CacheText` |
| 7 | `?SetProgressTextParameter@ProgressElement@UI@@QEAAXHAEBVString@System@@@Z` | declined — `TextElement::CacheText` |
| 8 | `?AddLine@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z` | declined — `Flush`, `AddSpriteDraw` |
| 9 | `?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@AEBV34@@Z` | declined — `Flush`, `FlushDeferredFonts`, `AddScissor` |
| 10 | `?SetScissor@SpriteContext@UI@@QEAA?AVViewport@Video@@HHHH@Z` | declined — forwards to #9 |

---

## 1. What changed, and why the hold-backs were spendable

All four ported rows were held back by Wave 7 (lanes D and E) and **re-declined by Wave 9
lane UIF**, whose reason was correct and specific: each one calls an unexported
`Collection::Array` internal that no lane owned, so porting it "would MOVE the cutover row
one frame deeper instead of closing it". Lane UIF was right. The answer was to go and get
the frame.

Seven `Collection::Array` instantiations sit under the four rows. **Three of the seven
turned out to be exact clones of specs `runtime/src/collection/array.cpp` already had** —
a claim read out of each body's own disassembly, not assumed from the element type:

| address | reached from | shape read at the address | spec |
|---|---|---|---|
| `0x180024fe0` | `ComboElement::_userData` | `SHL RCX,3` / `EDX=8` / plain `MOV RAX,[src]; MOV [dst],RAX` — **no zero-first store**, so `Assign8` not `Assign8Z` | `kSpec_Fill8` |
| `0x18004bfc0` | `ComboElement::_sprites` | `SHL RCX,4` / `EDX=8` / two qword copies | `kSpec_Fill16` |
| `0x180048fe0` | `RadioElement::_checkBoxes` | `SHL RCX,3` / `EDX=8` / `MOV [dst],R11(=0)` **then** `MOV RAX,[src]; MOV [dst],RAX` | `kSpec_Fill8Z` |
| `0x180034860` | `ComboElement::_strings` | stride 8, refcounting element assign, destroy-range + shrink arm | **`kSpec_StringRef` (new)** |
| `0x1800417d0` | `GraphElement::_graphs` | stride 0x18, nested-Array element assign, destroy-range + shrink arm | **`kSpec_GraphSeries` (new)** |
| `0x180037310` | `ComboElement::_sprites` teardown | `~Array` | J1 (13th address) |
| `0x180024aa0` | `ComboElement::_userData` teardown | `~Array` | J1 (14th address) |

Plus one new body: **`Collection::Array<int32>::operator=` at `0x18001cdf0`**, the assign
arm of `kSpec_GraphSeries`.

### The two `~Array` clones are a byte-level claim, not an eyeball one

Both were read with `read_memory` over all 115 bytes and compared against `0x180033960`
(the canonical J1 body). `0x180037310` differs at exactly **eleven** offsets and
`0x180024aa0` at exactly **fifteen**. Every differing byte in both lies inside one of the
five relocated operands, and all five resolve **in both bodies** to the same absolute
targets the existing J1 note pins:

```
0x1801fc718   0x18014b390   0x1801fc718   0x1800c6c00   0x18014b388
```

Outside those five windows the three bodies are byte-for-byte equal. (The first version of
that check reported `same=False` for both — because the reloc windows were off by one or
two bytes, not because the bodies differ. A comparison whose own offsets are wrong is a
false negative that looks exactly like a real difference; the fix was to decode the
instruction lengths rather than eyeball the diff positions.)

### `GraphSeries` is now reversed rather than opaque

`runtime/types/ui/GraphElement.h` said "GraphSeries stride 0x18, NOT reversed — only
touched via the retail helper". Writing `kSpec_GraphSeries`'s element assign forced the
question:

```
+0x00  colour   uint32
+0x04  (hole)   4 bytes — NEVER WRITTEN BY ANY ARM
+0x08  points   Collection::Array<int32>   data +0x08 / size +0x10 / capacity +0x14
```

The `+0x04` hole is declared as the spec's `assignUnwritten` window. An oracle that
compared it would fail 100% of calls and read like a catastrophic port defect
(`reference_oracle_must_not_compare_unwritten_bytes.md`). The `+0x08` offset is confirmed
twice independently: by `0x1800417d0`'s assign, and by `GraphElement::AddGraphPoint`
(`widgets_layout.cpp`, lane UIF) which already reaches the same nested array as
"record + 8" from a body this lane did not read.

---

## 2. The mistake worth the record: a shadowed address and two live copies

**The first draft of this lane wrote its own bodies for `0x180011010` (the
`System::String` destroy-range) and `0x1800419b0` (the `GraphSeries` destroy-range), and
registered `0x180010ab0` as well.**

All three already belong to `runtime/src/collection/array_destroy.cpp`:

* `0x180010ab0` — **already registered** there as a J2 entry point. A second
  `RegisterRuntimeAddr` for it is a **shadowed address**: the later binding wins and the
  other line is dead code. Nothing in this tree reports that. (`dispatch_slot_map.py
  --check` reports it for the *exe* side; there is no equivalent for the Runtime side.)
* `0x180011010` and `0x1800419b0` — **already ported** there as `kD_Str_08` and
  `kD_InnerArr_18`, and deliberately left unregistered under a note that says in as many
  words: *"Register it the day a caller appears."*

**How it surfaced is the point.** Not by reading, not by an audit — by a diff run printing
a site name this lane had not written:

```
[RtDiff]   Collection::Array::~Array(+elem) @0x180010ab0    calls=1  ...
```

Fixed by taking that file at its word instead of writing a third copy: one `A_ENTRY` line,
one `B_ENTRY` line, two registrations there, and `array.cpp`'s two new specs reach both
through `ResolveAddr` exactly as `DestroyStringExt` already reaches `0x180005930`.

**Rule for the next lane: `array.cpp` and `array_destroy.cpp` are two files and ONE
namespace. Grep the address across both before porting a `Collection` helper.**

A tree-wide duplicate-registration scan was run afterwards and is worth keeping:

```python
# 523 distinct RegisterRuntimeAddr addresses; 5 duplicates, ALL pre-existing:
#   0x1800bfd10 / 0x1800bfdb0 / 0x1800bfe30 / 0x1800bfe40
#       internal.cpp — the shadow and table tiers, MUTUALLY EXCLUSIVE branches. Benign.
#   0x1800e0c80
#       input.cpp:681 and input_config_startup.cpp:220 — same address, SAME target
#       function. Redundant, not a shadow. Benign, but one of them is dead.
```

Zero duplicates originate from this lane after the fix.

---

## 3. Per-function notes on the four ports

### `ComboElement::AddItem` (0x1800750b0)

Eleven instructions of orchestration; the whole risk is the argument shuffle.

* **`R8` is never set before the first call.** Retail relies on the incoming
  `String const&` already being there. `RDX` (the Sprite) is saved to `RBX` at entry and
  becomes the *second* call's `R8`.
* **The third call's `R8` is the ADDRESS of the `userData` home slot, not the value.** The
  element type is `void*` and the helper takes a *pointer to* the fill. Getting this
  backwards stores a stack address as the item's user data and nothing complains until a
  caller reads it back.
* **The return is re-read from memory** (`MOV EAX,[RDI+0xf0]; DEC EAX`), after all three
  appends — so if a growth helper failed to grow, retail returns the stale index. Written
  as a re-read for the same reason lane B3's `RemoveChildren` re-reads its base.

### `ComboElement::RemoveAllItems` (0x180075120)

`_currentItem = -1` (confirmed: `MOV dword ptr [RBX+0x118],0xffffffff`) — worth stating
because the sibling `SetCurrentItem` resets an out-of-range index to **0**, not -1, and a
reader who assumes the two agree gets one of them wrong.

**`_openDialog` (+0x120) is not touched.** Removing every item while the combo popup is
open leaves that pointer dangling in retail. Faithful; not repaired here.

### `RadioElement::FindCheckBoxes` — both overloads

* The type test is a **virtual `Type()` call** (`vtbl[2]`), exact equality — and then
  `IsDerivedFrom` runs anyway on the same object against the same id, only to decide
  whether the stored pointer is the element or NULL. Not redundant: `Type()` is exact,
  `IsDerivedFrom` walks the base chain, so the second can only ever agree here. Both are
  reproduced.
* The type-id global `0x1801fa9a8` is **read twice** (0x1800d575f and 0x1800d577b).
  Reproduced as two reads.
* **`*p` is re-read at every use**, including at the bottom of every loop iteration, and
  the loop bound with it. Caching it would be the obvious cleanup and would change
  behaviour under exactly the tree mutation this shape exists to survive.
* Registered under **both** manglings at **both** addresses. Registering only one of an
  overload set is how `engine_funcs.cpp` came to name the private sibling for the public
  symbol (WAVE9_LANE_R.md row 1) — a real entry point that would have run radio-group tree
  surgery on whatever the caller left in RDX.

### `GraphElement::AddGraph` (0x180040620)

* **`tmp[0x00..0x07]` is deliberately uninitialised.** Retail's fill lives at `[RSP+0x28]`
  — the shadow-space slot — and only `[RSP+0x30]` and `[RSP+0x38]` are zeroed. The colour
  dword the assign copies is stack garbage, and the last line of the function overwrites
  it with the real colour. Zeroing it would be a different instruction sequence for a value
  that is dead either way, and a reader finding `= {}` would reasonably conclude retail
  initialised it.
* **The temp's nested `~Array` is kept**, and it is provably inert (the copy-assign copies
  *from* the temp, so the temp still owns nothing). Kept anyway: dropping a local that is
  written, passed, released and never read is the `dirAlias` mistake WAVE9.md files as a
  rule. Here it *is* provably inert — and honouring it costs one call.
* `old` is `MOVSXD`, reproduced as a signed widening. Ghidra prints `MOVSXD` as a zero
  extension and that misreading caused three Wave 6 defects including a crash.

### The oracle, stated once

**All four are NO-ORACLE ports and say so at the call site.** Three append (a replay
appends a second time, reallocates, and bumps a `System::String` refcount a second time —
the shape `widgets_input.cpp`'s file-level note forbids and that lanes M and N found the
expensive way); the fourth frees three buffers and releases every String reference (a
replay double-frees).

Each carries a shadow, and where possible the shadow checks against the **caller's source
data** rather than restating the body's own rule:

* `AddItem` — the three arrays grew in lockstep, the returned index equals the pre-call
  size, `_userData[i] == userData` and `_strings[i] == *(void**)str`. The last two compare
  against values this body did not derive.
* `AddGraph` — the array grew by one and the new element carries the colour the **caller**
  passed.
* `RemoveAllItems`, `FindCheckBoxes` — post-conditions only, and labelled as weaker than a
  diff.
* `Array<int32>::operator=` — compares the copied buffer against the **source**, which the
  function did not write. Silent when `dst == src` (retail's early return) and when the
  size is zero, so `worked=` separates "entered" from "did work".

---

## 4. Verification

### Static accounting — unmoved denominator plus a set-diff

`tools/unsatisfied_export_scan.py`, run on a clean tree (`git stash`) and again after:

```
before:  789 distinct names resolved through a door; 149 unsatisfied (146 outside a guard)
after:   789 distinct names resolved through a door; 145 unsatisfied (142 outside a guard)
```

**The denominator did not move.** Set-diff of the two name lists:

```
CLOSED (in before, not after):
  - ?AddGraph@GraphElement@UI@@QEAAXI@Z
  - ?AddItem@ComboElement@UI@@QEAAHAEBVSprite@2@AEBVString@System@@PEAX@Z
  - ?FindCheckBoxes@RadioElement@UI@@QEAAXXZ
  - ?RemoveAllItems@ComboElement@UI@@QEAAXXZ
NEW (in after, not before):
  (none)
```

Exactly the four intended, and **zero new unsatisfied names** — which is the falsifiable
part: every callee of the four was checked to be ported, so a new row appearing would have
meant the triage was wrong.

### The other audits

| audit | before | after |
|---|---|---|
| `unsatisfied_export_scan.py` | 149 unsatisfied / 789 doors | **145** / 789 |
| `runtime_census.py` UI | 1625 total / 151 called / 110 ported / 72.8% | 1625 / **152** / **114** / **75.0%** |
| `runtime_census.py` TOTAL | 4632 / 958 called / 665 ported / 69.4% | 4632 / **969** / **679** / **70.1%** |
| `port_bypass_audit.py` address door | 209 sites, **BYPASS 0** | 223 sites, **BYPASS 0** |
| `port_bypass_audit.py` name door | 429 sites, 0 ported-on-a-live-path | 432 sites, **0** |
| `rawdoor_audit.py` | 501 sites, 0 UNGUARDED-CALL, 36 DEFERRED | 504 sites, **0** UNGUARDED-CALL, 39 DEFERRED (+3 = the probe's own oracle doors) |
| `export_name_audit.py` | 662 names, 1 phantom | 662 names, **1** phantom (pre-existing, `rt_resolve_cache.cpp`) |
| `abi_retslot_audit.py` | — | **0 suspicious of 48** by-value-returning exports |

`called` grew 958 → 969 while `ported` grew 665 → 679. That is expected and is
`reference_porting_can_grow_the_cutover_surface.md`: a faithful port that calls internals
by address *adds* to the called surface even as it closes rows. **`called` is not a
denominator counting down to zero.**

Census numbers are post-`29cfaa2` (the macro-door blind-spot fix) on both sides of the
diff, so they are comparable to each other and **not** to any figure quoted before that
commit.

---

## 4a. The runs

DLL present, `BANISHED_RT_DIFF=1`. Two scenarios plus a probe. Logs in
`docs/runtime_port/wave9_lane_uiw/`.

**Run A — `BANISHED_AUTOSTART=1`** (menu → New Game → in-game → pause-menu quit).
`413 sites, 15.4M calls, 1 divergence` — and that one divergence is
`Object::Factory::Startup [probe, not diffable]`, which is **pre-existing**: it appears
identically in a run of a *different* lane's binary taken earlier in this session
(`ours=chain=416 nodes=415 / theirs=chain=416 nodes=416`). None of this lane's sites
diverged.

**Run B — `BANISHED_AUTOMENU_BUTTON=4 BANISHED_AUTOMENU_CLICKS=m6`** (the Options dialog,
then quit). `267 sites, 1.69M calls, 1 divergence` — the same pre-existing one.

| site | run A | run B | run E (A + probe) |
|---|---|---|---|
| `UI::ComboElement::AddItem` | calls=8 | **calls=53** | calls=8 |
| `UI::ComboElement::RemoveAllItems` | — | **calls=6** | — |
| `UI::RadioElement::FindCheckBoxes(InternalPtr)` | calls=73, **worked=9** | — | calls=73, worked=9 |
| `UI::RadioElement::FindCheckBoxes()` | calls=0, **worked=1** | — | calls=0, worked=1 |
| `UI::GraphElement::AddGraph` | — | — | **calls=8, worked=8** |
| `SetSize @0x180034860` (String) | 8 | **53** | 8 |
| `SetSize @0x18004bfc0` (Sprite) | 11 | 63 | 11 |
| `SetSize @0x180024fe0` (void*) | 11 | 63 | 11 |
| `SetSize @0x180048fe0` (checkbox) | 11 | — | 11 |
| `SetSize @0x1800417d0` (GraphSeries) | — | — | **8** |
| `Array<int32>::operator= @0x18001cdf0` | — | — | **7** |
| `~Array @0x180037310` | — | **6** | — |
| `~Array @0x180024aa0` | — | **6** | — |
| `~Array(+elem) @0x180010ab0` | 1 | **6** | 1 |
| elem-destroy(String) `@0x180011010` | 6 | **17** | 6 |
| elem-destroy(inner Array) `@0x1800419b0` | — | — | **3** |

**Every one of them: `divergences=0`.**

### Reading those numbers — three cross-checks that could have failed and did not

1. **`RemoveAllItems` = 6, and each of its three teardown helpers = 6.** The body calls
   exactly one of each per invocation; three independent site counters agreeing at 6 is a
   check on the *wiring*, not on the body.
2. **`AddItem` = 53 and `SetSize @0x180034860` = 53, exactly.** `kSpec_StringRef` has
   `byteCopy = false`, so it emits ONE report per invocation. Its two sibling arrays
   (`byteCopy = true`) report **twice** on a reallocating call — the extra one is
   `SetSizeCore`'s copy-prefix check — so they read 63 = 53 + 10 reallocs. The three
   numbers are only consistent under the right reading of the growth rule; a wrong
   `byteCopy` or a wrong offset would have desynchronised them.
3. **`Array<int32>::operator= @0x18001cdf0` = 7 and the probe independently reports
   "7 NON-EMPTY nested arrays copied through a realloc".** Two instruments that share no
   code arriving at the same 7.

### The population is not uniform, and that was checked rather than assumed

`AddItem`'s run-B population is the Options dialog's four combos — `comboFilter`,
`comboAntialias`, `comboRefresh`, `comboResolution` — 53 appends of distinct
`System::String`s with distinct `userData` (the resolution list alone is all-distinct), and
6 `RemoveAllItems` teardowns of live `String` references. Run A's 8 appends land in the New
Game dialog's combos.

This mattered. The shadow on `AddItem` compares the appended `String` pointer and
`userData` against **the caller's source**, and on a one-item or all-identical population
that comparison is nearly vacuous —
`reference_uniform_population_is_not_a_verification.md` is the case where 13 of 14
identical values let a broken `strcmp` pass four waves of oracles. (The integrator offered
a mods-mounted population as a second source; it turned out the premise was wrong —
copying a `.pkm` into `WinData/` does not mount it, since `FindFiles`' pattern is the
literal `*.pkg` — and the Options dialog was the better population anyway.)

`FindCheckBoxes` is likewise not a degenerate population: **73 entered calls, 9 of which
appended.** The 73 is the recursive walk over the toolbar's element tree, so both the
match and the no-match arms ran, many times.

### The probe: `GraphElement::AddGraph`, which no harness path reaches

`AddGraph`'s only caller in this tree is `townhallui.cpp:1060`, which needs a built Town
Hall clicked in the world — not reachable unattended. Runs A and B both report it absent,
which is the `calls=0 divergences=0` line that *reads* as a pass and is not one.

So it is probed directly against the shipped body, in `standalone/rt_orphan_probe.cpp`
(`BANISHED_RT_ORPHAN_PROBE=1`), the file that exists for exactly this. **A synthetic
subject, not a fake:** `AddGraph`'s only base-relative accesses in
`0x180040620..0x18004068b` are `[RCX+0xF8]` and `LEA RBX,[RCX+0xF0]`, so a zeroed buffer
with a valid `Array` header at `+0xF0` *is* a `GraphElement` as far as it can tell — and
two such buffers are two independent subjects, which is what makes ours-and-theirs safe
where a replay on one object would merely append twice.

```
[RtOrphan] pass UI::GraphElement::AddGraph 8 appends, 8 grew, 3 REALLOCATED
           (final size=8 cap=21), 8 series seeded with points,
           7 NON-EMPTY nested arrays copied through a realloc,
           negative control CAUGHT
[RtOrphan] ==== running total: 19 pass, 0 fail ====
```

`size=8 cap=21` with **3 reallocs** is the growth rule reproduced exactly: capacity
`0 → 1` at append 1 (no treble, `oldSize == 0`), `1 → 6` at append 2 (`n == oldSize+1` and
`oldSize != 0`, so `n*3`), then `6 → 21` at append 7. Three reallocs, not two and not four.

**What is compared, and what is deliberately not:** size, capacity, every element's colour
dword, and the nested point array's header *and contents*. **Not** the outer `_data`
pointer (two separate allocations — comparing it would fail 100% of runs), and **not**
bytes `+0x04..+0x07` of any element, which neither side writes.

**The probe's first version was defective and its own control caught it.** It appended
eight *empty* series, so `Array<int32>::operator=` ran only on its trivial path — an empty
destination, zero elements copied — on all three reallocs. **The copy loop was never
entered, and the probe passed anyway.** That is exactly
`reference_probe_needs_an_effectiveness_control.md`: green comparisons where nothing did
the work. Fixed by seeding each new series with three points (through the *shipped*
`AddGraphPoint`, on both objects, since it is not the subject), so the next realloc has to
copy non-empty arrays forward: **`copiedNonEmpty` went 0 → 7**, and the counter is printed
on every run with a `WARN` line if it is ever zero again.

**Two negative controls, one per term the comparator checks** — a corrupted colour and a
corrupted point value, both driven through the *same* comparator lambda the positive cases
used. One control over one field would have left the contents check unproven, which is how
a predicate passes on a term it never evaluates
(`reference_shadow_predicate_must_name_its_failing_term.md`). Both are caught; if either
is not, the probe reports FAIL rather than a pass.

The probe also refuses to run if `ResolveExport` and `ResolveExportRaw` return the *same*
pointer — that would mean the port is not registered and both doors landed on the DLL,
making the comparison a function against itself and a guaranteed, meaningless pass.

### A defect this found in the lane's own code

`AddGraph`'s element assign originally called `Array<int32>::operator=`'s body as a direct
C++ call. Same instructions — but it bypassed the registered entry point, so
`0x18001cdf0`'s diff site **never fired** and the run reported no coverage at all for a
whole ported body. Changed to go through `ResolveAddr(Module::Runtime, 0x18001cdf0)`,
which is both what retail does (a `CALL` to that address at `0x1800418b1`) and the
convention `Assign38Call` and `DestroyStringExt` in the same file already follow. The site
then reported `calls=7`.

### The harness problem this lane hit, which is not lane-specific

`tools/rt_run.sh` checks `tasklist` for a running `reimpl-x64.exe` and then launches. With
six lanes deploying to the one shared `C:/GOG Games/Banished`, **another lane can start in
that gap, and did — three times.** One of those produced a complete, plausible, entirely
wrong `[RtDiff]` summary from another lane's binary; it was caught only by md5-ing the
deployed exe *after* the run and finding it was not the one built.

`docs/runtime_port/wave9_lane_uiw/run_uiw.sh` is this lane's replacement. Two changes,
both cheap and both worth folding into `tools/rt_run.sh`:

1. **Deploy under a lane-unique name** (`reimpl-uiw.exe`). No other lane writes that path,
   so the exe cannot be replaced between the copy and the launch. The md5 is checked
   before *and* after and the run is declared void if it moved.
2. **Gate on being able to DELETE the shared logs.** The exe opens
   `standalone_{debug,error,crash}.log` by fixed name relative to its cwd, so whoever is
   running holds them open. `rm` failing is the only *reliable* "someone is running right
   now" test — `tasklist` is a snapshot and races.

The runner also reports if a foreign `reimpl` appears mid-run, since the logs are still
shared even when the exe is not.

---

## 5. Declined, with the blocking frame named

Declining is the correct result under this wave's admission test: a port whose callee is
a null address after cutover has relocated the failure, not removed it
(`reference_converting_a_door_is_not_satisfying_it.md`).

### `ComboElement::SetCurrentItem` (0x180112540) — two frames, one of them another lane's

```
SetCurrentItem(this, index):
    if (this->_openDialog) {
        tmp = _openDialog;
        UserInterface::CloseDialog(_openDialog->[0x10], &tmp);   // 0x180120f20
        this->_openDialog = 0;
    }
    if (this->_currentItem != index) {
        if (index >= 0 && index < _strings._size) { _currentItem = index; TAILJMP UpdateText; }
        else                                      { _currentItem = 0;     CALL   UpdateText; }
    }
```

* `UserInterface::CloseDialog` `0x180120f20` is **lane UIF's declined row 4** and is three
  frames deep itself (`UpdateEvents`, `EndDragDrop`, `0x180033690`). It belongs to whoever
  owns `UI::UserInterface`, not here.
* `ComboElement::UpdateText` `0x180112180` is unexported and is **not** small — ~70
  instructions that walk the element tree through `0x180115760`, call
  `TextElement::CacheText` (`0x18011ec10`), call `0x1800c9810`, and do a 16-byte
  compare-and-store of a `UI::String` into the child at `+0x100`.

Note the out-of-range branch stores **0**, not -1. Recorded here because `RemoveAllItems`
stores -1 and the two are easy to conflate.

### `SliderElement::SetParameter`, `ProgressElement::SetProgressTextParameter` — one shared blocker, one frame deeper than lane UIF measured

Lane UIF is right that `TextElement::CacheText` (`0x18011ec10`) closes both rows at once,
and right that three of its four callees are already ported (`??BString@UI@@`
`0x180035020`, `FormatParamList` `0x1800c8dc0`, `MemFree` `0x1800c7ad0`). The remaining
one, `TextElement::SetParameterCount` `0x18011e8e0`, was read for this lane and it is
**not** a leaf:

```
SetParameterCount(this):
    UI::String::operator System::String(this+0xd8, &tmp);   // 0x180035020  PORTED
    take a counted reference to tmp
    n = String::FormatParamCount(tmp);                      // 0x1800c8d20  PORTED (lane B3)
    0x180029a80(this+0xc8, n);                              // <-- UNPORTED
    release both references
```

**`0x180029a80` is the TWO-ARGUMENT `Array<System::String>::SetSize`** — the same element
type as this lane's new `kSpec_StringRef`, with `init` = store zero instead of a fill arm,
and the same destroy-range `0x180011010` that this lane has just registered. So it is
**one `J3_ENTRY2` line and one spec away** (`kSpec_StringRef` with `init = &InitZero8`; it
wants its own spec rather than a mutation of `kSpec_StringRef`, because `0x180034860`
genuinely has no init arm and is only ever called with a fill).

That makes the remaining chain, in order:

```
0x180029a80  Array<String>::SetSize(n)     — one spec line, everything under it is ours
0x18011e8e0  TextElement::SetParameterCount — ~40 instructions, refcount pair
0x18011ec10  TextElement::CacheText         — ~90 instructions, dense refcount arithmetic,
                                              MUST be a no-oracle port (lane M's Finding 1)
then rows 6 and 7 themselves, which additionally do a refcounted String::operator= into a
child's own parameter array.
```

Not attempted here: it is a coherent unit of work larger than the four rows this lane
closed, and half-doing it would leave the two payoff rows still unsatisfied. **It is the
best next move in this cluster**, and it is now one frame shallower than lane UIF recorded.

### `SpriteContext::AddLine` and both `SetScissor` overloads

Both were fully read; neither was ported.

**`SetScissor(Viewport const&)` `0x18011d050`** — and the ABI first, because the brief
flagged it: `RCX = this`, **`RDX` = the hidden return slot**, `R8` = the `Viewport const&`.
It returns a `Viewport` by value. `SetScissor(int,int,int,int)` `0x18011d010` packs its
four ints into a local `Viewport` at `[RSP+0x20]`, calls the reference overload, and
returns `RBX` — the same hidden slot it was handed. `tools/abi_retslot_audit.py` reports
**0 suspicious of 48**; no `fn_*` typedef in the tree gets these wrong today, because
nothing in the tree declares them.

The body, for whoever picks it up:

```
scale = this->[0x00]  (float)
r = { (int)(nw.x/scale), (int)(nw.y/scale), (int)(nw.w/scale), (int)(nw.h/scale) }   // truncating
if (r == this->[0x20..0x2c]) {
    *ret = this->[0x20..0x2c];            // the SCALED-DOWN values, verbatim
} else {
    Flush(this);                          // 0x18011ba40
    FlushDeferredFonts(this);             // 0x18011c6a0
    if (this->[0x80]) {                   // the SpriteCapture
        vp = this->[0x30]->[0x28];
        full = (r.w == vp->[0x10] && r.h == vp->[0x14]);
        AddScissor(capture, nw.x, nw.y, nw.x+nw.w, nw.y+nw.h, full);   // 0x1800d8ef0
    }
    old = this->[0x20..0x2c];
    this->[0x20..0x2c] = r;
    *ret = old * scale;                   // truncating, per component
}
return ret;
```

**Note the asymmetry, which is retail's and is easy to "fix" by accident:** the unchanged
branch returns the stored (scaled-down) values *verbatim*, while the changed branch returns
the old values *multiplied back up by scale*. A port that made the two consistent would be
wrong in one of them.

`AddLine` `0x18011cbe0` builds a 0x28-byte draw record on the stack — sheet, sprite index,
the four ints, a zeroed float at `+0x1c`, the colour, and a `1` byte at `+0x24`, with
`+0x25..+0x27` left unwritten — flushes if the sheet changed, and hands the record to
`AddSpriteDraw`.

**Why declined:** `Flush` `0x18011ba40`, `FlushDeferredFonts` `0x18011c6a0`,
`AddSpriteDraw` `0x18011c890` and `SpriteCapture::AddScissor` `0x1800d8ef0` are all
unexported and all submit into the **live** `Video::Context` / GPU vertex buffer this
`SpriteContext` wraps. Wave 7 lane F's argument still holds and is the reason there is no
oracle here either: a scratch copy of the `SpriteContext` object does not isolate a replay,
because the state being mutated lives outside the object. Lane UIF's assessment that these
two internals gate **four** rows (its rows 2, 12, 13, 14) is confirmed; this is the biggest
single lever left in the UI cluster and it wants its own lane.

---

## 6. What this lane did NOT verify — stated plainly

1. **No cutover boot.** By instruction: renaming `Runtime-x64.dll` is a global action on
   the one shared game directory and the integrator owns those attempts. Everything in §4
   is either static accounting or a DLL-present diff run. **The four rows are predicted to
   disappear from the residue; that prediction has not been tested.**
2. **`kSpec_StringRef` is not in `array_probe.cpp`.** `kSpec_GraphSeries` is now covered
   against the shipped body by the `AddGraph` probe (§4a), but the `Array<System::String>`
   spec is not: `array_probe.cpp` drives a fixed target list and this row was not added to
   it. Its live evidence is 53+8 invocations with a header-algebra shadow and a lockstep
   cross-check against `AddItem`'s own site — good coverage, but **not** a comparison
   against the DLL's body the way `0x1800417d0` now has. Adding one probe case is the
   cheapest strengthening left.
3. **Three of the four ports are verified by shadows and cross-checks, not by a
   comparison against retail.** Only `AddGraph` has an ours-versus-theirs oracle, and only
   because its state is small enough to synthesise. `AddItem`, `RemoveAllItems` and
   `FindCheckBoxes` mutate objects wired into the live UI tree; the reason each cannot be
   replayed is written at its call site, but the honest summary is that their correctness
   rests on the transcription plus a post-condition, not on the DLL agreeing.
4. **`worked=` is the signal on these sites, not `calls=`.** `calls=` is bumped only by
   `RtDiffReport`. A reader who checks `calls=` alone on `FindCheckBoxes()` (the public
   overload) sees **0** and would wrongly conclude the port is dead — `worked=1` is what
   says it ran.
5. **The `FindCheckBoxes` population is one toolbar.** 73 walked elements and 9 checkboxes
   is a real population, but it is a single element tree from a single dialog; a radio
   group nested inside another radio group has not been seen.
6. **`sizeof` of the widget structs remains a lower bound**, as Wave 7 lane D's file-level
   note already says. Nothing here changes that.
7. **The `SetScissor` overloads were read but not exercised.** `abi_retslot_audit.py`
   reports 0 suspicious of 48, but that is vacuous for these two specifically: nothing in
   the tree declares an `fn_*` typedef for either, so there is nothing for it to check.
   The hidden-return-slot ABI recorded in §5 comes from the disassembly and has not been
   run.
