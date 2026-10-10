# WAVE 10 — lane UIORACLE

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a19c28d0d6d662603`
**Branch** `wave10/lane-uioracle` (cut from `wave10/base` @ `3318690`)

**Assignment.** Close the two UI census rows recorded as *"not replayable, needs the
predictive oracle"*, both parked behind the allocator NO-GO:

| row | retail |
|---|---|
| `?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription@UI@@@4@0@Z` | `0x1800DA130` (ord 1589) |
| `?CacheText@TextElement@UI@@AEAAXXZ` | `0x18011EC10` (ord 1680) |

**Both are ported, registered and verified against the shipped DLL. Both census rows
are closed.**

---

## 0. THE ANSWER TO THE QUESTION THE LANE WAS ASKED TO ANSWER EXPLICITLY

> *"say explicitly in your report whether your conclusion depends on lane ALLOC's
> re-test of the allocator gate."*

**IT DOES NOT — for either function, and the reason is the same for both.**

Neither body *contains* an allocator. Every allocation and every free either of them
performs happens **inside a callee that is already ported and registered**:

| allocation site | callee | owner |
|---|---|---|
| `CacheText` format branch | `?FormatParamList@String@System@@…` | Wave 9 lane B3 |
| `CacheText` release arm | `?MemFree@MemoryAllocator@System@@…` | Wave 2 |
| `SetParameterCount` | `Collection::Array<System::String>::SetSize` `0x180029A80` | **this commit** |
| `AddTab` × 2 | `ExternalPtr<ElementDescription>::vtbl+0x40` (indirect) | not ported — see §3 |
| `AddTab` × 2 | `Collection::Array<InternalPtr<Element>>::SetSize(n,fill)` `0x180076900` | Wave 9 lane UIW |

Nothing in either function decides *where memory comes from*. Whatever lane ALLOC
concludes about `MemoryAllocator::Allocate`, these two rows land or fall on their own.

The allocator NO-GO was, for these two rows, a **proxy for the real blocker** — that
mutating a refcounted heap object makes the *replay* oracle a use-after-free. That is
a different problem with a different fix, and the fix (the predictive oracle) landed in
this same wave.

---

## 1. `?CacheText@TextElement@UI@@AEAAXXZ` — CLOSED, VERIFIED

`runtime/src/ui/textelement_cachetext.cpp` (also ports its private callee
`?SetParameterCount@TextElement@UI@@AEAAXXZ`, `0x18011E8E0`).

### 1a. What the recorded blocker actually was

Re-derived from the disassembly rather than inherited
(`reference_holdback_blockers_are_stale_check_them_first.md`). Three claims, three
different verdicts:

* **"Not replayable" — STILL TRUE, and it is Wave 9 lane M's defect verbatim.** The
  body swaps a refcounted `System::String` at `this+0xF8`. Ours releases the old
  buffer; a prefix replay then hands retail a shallow copy holding a pointer ours has
  already freed. That is not a weak oracle, it is a use-after-free. WAVE9.md finding (1)
  says the same thing about `NumberElement::SetValue`, whose `CacheText` is the
  *sibling* at `0x1801189F0`.
* **"Needs the predictive oracle" — TRUE, AND IT NOW EXISTS.** `runtime/funcs/rt_predict.h`
  landed this wave. Ours predicts and writes nothing (so nothing is released and
  nothing is freed), THEIRS runs for real on pristine state, and we compare.
* **Allocator NO-GO — NOT LOAD-BEARING.** §0.

One dependency **was** real and is closed here: `SetParameterCount`'s own callee
`0x180029A80` was an unported, unexported `Array<System::String>::SetSize`. Wave 9's
`array_destroy.cpp` predicted this exact arrival in as many words — *"It will get a
SECOND caller the day `0x180029A80` (the two-argument sibling,
`TextElement::SetParameterCount`'s) lands."*

### 1b. The Collection spec, and the trap in it

`0x180029A80` is ported in `runtime/src/collection/array.cpp` as **`kSpec_StringRefZ`**
and registered with `RegisterRuntimeAddr`. It is **not** `kSpec_StringRef`
(`0x180034860`), and the difference is the one field the two-argument form even has:

```
180029b2a  MOV qword ptr [RDX],R10      ; R10 == 0 — the `else` arm
```

`kSpec_StringRef`'s `init` is `nullptr`, which this file defines as *"retail leaves the
new tail untouched"* — true of `0x180034860`, whose only caller passes a fill. The
two-argument sibling **zeroes** the tail. Reusing the lookalike (the first draft, on a
"same element type, same destroy, same assign" argument) would have left every newly
grown `System::String` slot holding raw `MemAlloc` garbage, and the *next*
`SetParameterCount`'s destroy arm would have decremented a refcount through it. Same
trap, different field, as `kSpec_U64ZeroA4`'s note: similarity is corroboration, not
evidence.

### 1c. The two branches need two oracles, and both are real

* `_size == 0`: the cached text becomes what `UI::String::operator System::String`
  returns, which is `StringTable::GetText(id)` under an eight-instruction wrapper — a
  refcounted copy of **the table's own buffer**. Ours and retail produce the
  **identical pointer**, so this branch is exactly comparable and *is* compared.
* `_size != 0`: `String::FormatParamList` builds a **fresh allocation**. Ours and
  retail get different blocks, so predicting the pointer would fail 100 % of calls —
  one step over `reference_oracle_must_not_compare_unwritten_bytes.md`. On this branch
  the pointer prediction is declared with `Exclude()`, so the round records it in the
  **`unverifiable=`** column rather than comparing it, and a **separate content
  oracle** carries the branch. The hole is counted, never silent.

The content oracle is a **real** oracle, not an `RT_DIFF_SHADOW`: `theirs` is read out
of the element *after the shipped DLL's own `CacheText` has run on pristine state*.
Nothing in it consults our own expectation.

### 1d. Two modelling choices, both stated rather than hidden

* **The predictor runs `SetParameterCount` FOR REAL.** Not an exception to "predict
  mode writes nothing" — a choice with a reason: the params array is
  `FormatParamList`'s **third** argument (`MOV R8,[RDI+0xC8]` at `0x18011ECBD`, a
  dereference **Ghidra's decompiled C drops entirely** —
  `reference_ghidra_c_output_vs_disassembly.md`), so a predictor that skipped the
  resize would format from a stale params pointer and predict a string retail never
  produces. It is safe because the function is idempotent: a second call computes the
  same `n` and `0x180029A80` returns at its first `CMP EDI,ESI; JZ`. The array header
  is declared with `Exclude()` so the write is **counted**.
* **The old buffer's refcount is PREDICTED but deliberately NOT WATCHED.** This lane
  walked into the false positive and backed out of it. Predictions are compared whether
  or not a `Watch` covers them; `Watch` exists only to report changes *nothing*
  predicted. The predictor must keep one reference on its own answer alive across
  retail's run so the content comparison does not read a released buffer — and when the
  cache was already correct, that answer *is* the same block, so the held reference
  moves the word by +1 **for a reason that belongs to the instrument, not to either
  body**. Watching it would have reported the harness's own bookkeeping as an
  unpredicted retail write. (The reference is *handed over* from one of the body's own
  temporaries rather than freshly acquired, so the net count is exactly what the live
  body would have left.)

### 1e. RESULTS

Route: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60`,
`BANISHED_RT_PREDICT=1`. Exe `bd5f19e5…`, deployed under `tools/game_dir_lock.sh`.

| run | rounds | predictions | changed | **wrong** | unpredicted | unverifiable | content oracle |
|---|---|---|---|---|---|---|---|
| D (clean, gameplay) | 14 | 19 | **11** | **0** | **0** | 8 | `interned=6 formatted=8 changed=14/14`, **0 divergences** |
| C (clean, gameplay) | 15 | 21 | **13** | **0** | **0** | 8 | `changed=15/15`, **0 divergences** |
| A (clean, menus+Options tabs) | 2 | 2 | 0 | 0 | 0 | 2 | `formatted=2 changed=2/2`, 0 divergences |

**`calls=` was read, not just `divergences=`** — 14–15 rounds per gameplay run, not
zero. The site is genuinely exercised, and both branches are:
`interned=6 / formatted=8` in run D.

**BOTH negative controls fired, and they are scored SEPARATELY** (a union score would
hide a blind screen — `reference_union_control_hides_a_blind_screen.md`):

* **`RtPredict`'s own control** (run E, `BANISHED_RT_PREDICT_SELFTEST=1`): *"0 site(s)
  could NOT demonstrate a failure"*, `wrong=5`, and the log names each corrupted
  subject and its failing term, e.g.
  `TextElement::_cachedText … pre=0x0 ours=0x184af40a8f9 theirs=0x184af40a8f8` and
  `old cached text: refcount … pre=0x3 ours=0x3 theirs=0x2`. Note what the first line
  independently proves: on the interned branch **the pointer really is exactly
  comparable** — one flipped bit is the whole difference.
* **The content oracle's own control** (same run): `selftest=13/13 (n/a 0)`. Every
  round had a non-empty predicted string to corrupt, and every corruption was caught.
  A round with an empty string would have been counted `n/a`, never as a pass.

**Effectiveness control:** `changed=11/19` predictions and `changed=14/14` content
rounds. Nothing here is a green board built out of predictions the DLL could satisfy by
doing nothing.

> **A DEFECT IN THIS LANE'S OWN INSTRUMENT, FOUND AND FIXED BEFORE IT WAS REPORTED
> ON.** In runs A–E the content oracle registered its verdict with `RtDiffReport`
> only — and the `[RtDiff]` summary prints under `BANISHED_RT_DIFF`, which is a
> *different switch* from the `BANISHED_RT_PREDICT` this oracle runs under. So every
> column of the instrument printed except the one that says whether it **failed**: a
> board that could not have expressed a failure
> (`reference_diagnostic_field_cannot_express_the_answer.md`). Runs A–E's content-oracle
> rows above are therefore evidence of **coverage and effectiveness**, and their
> zero-divergence status rests on the indirect argument that under self-test the same
> comparison went red 13/13. The counter is ours now and prints unconditionally
> (`WRONG=`); the numbers below are the ones taken with it.

> **A caveat stated rather than buried.** 14 rounds is what *our tree* drives, not what
> the process does. `CacheText` has **ten DLL-internal callers** (`get_xrefs_to
> 0x18011EC10`), all direct `rel32`, so without a detour they keep running the DLL's
> copy. That is a knowingly-chosen split brain, exactly as `widgets_layout.cpp` chose
> for `UpdateContainers` before its opt-in detour existed. A `BANISHED_RT_*_DETOUR` on
> `0x18011EC10` would raise the round count by orders of magnitude and is the obvious
> next step for whoever wants it; this lane did not install one it had not measured.

---

## 2. `?AddTab@TabElement@UI@@…` — CLOSED

`runtime/src/ui/tabelement_addtab.cpp`.

### 2a. The stated blocker was a MISREADING OF THE STACK FRAME

WAVE7_LANE_E.md §2: *"the body constructs a temporary ElementDescription/DialogInstance
and is not a leaf accessor."*

The temporary is neither. It is a plain **`UI::Dialog`, default-constructed**, and the
construction is `??0Dialog@UI@@QEAA@XZ` **inlined** — the two listings agree store for
store:

| AddTab `0x1800DA163..1D2` | `??0Dialog` `0x180112DC1..E2A` |
|---|---|
| vptr := `0x180154788`, `0x1801547A0` | identical |
| `+0x08/+0x10/+0x18` := 0 | identical |
| vptr := `0x1801562A0` (Dialog vftable) | identical |
| `+0x20..+0x28` := 0 (one `MOVDQA`) | two `MOV`, same 16 bytes |
| `+0x30` := `0xFFFFFFFF` | identical |
| `+0x34/+0x38/+0x40/+0x48..+0x80` := 0 | identical |

So the object is **0x88 bytes** and the exported constructor *is* the inline sequence;
the teardown matches `??1Dialog@UI@@UEAA@XZ` (`0x180112E40`) the same way. The port
calls them rather than open-coding a memset it would have to keep in sync with a class
it does not own. (`??1` and **`UEAA`, the virtual spelling** — the `QEAA` spelling of a
virtual destructor is not in the export table and would have resolved to null in
silence: `reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`. Both names were checked
against the export directory, not inferred: `reference_resolveexport_phantom_names.md`.)

WAVE8_5's row-22 blocker (*needs `ElementDescription::CreateInstance` first*) is **true
and is not a blocker for the body**: AddTab never calls it by name. It makes an
**indirect** call through the `ExternalPtr`'s own vtable slot `+0x40`
(`0x1800DA1E5`, `0x1800DA235`) — a pointer both the port and the DLL load from the same
object. Porting the body needs the *slot*, not the callee. What the missing callee does
block is the **oracle's** coverage of the two objects created; that is §2c.

### 2b. THE PORT WAS DEAD ON ARRIVAL AND THE ORACLE'S `rounds=0` IS WHAT SAID SO

First measured run: `AddTab` registered, and the predictive oracle reported **the site
was never constructed at all**. `rounds=0` reads exactly like *"no route exists"* — the
`calls=0` ambiguity (`reference_uncalled_port_is_not_verified.md`).

It was not a routing problem. `engine/funcs/engine_funcs.cpp` resolved
`fn_TabElement_AddTab` **from exe dispatch slot `0x1401529C8` first**:

```c
void** dispatch = ResolveAddr(Module::Exe, 0x1401529c8);
if (dispatch) fn_TabElement_AddTab = (fn_TabElement_AddTab_t)(*dispatch);
```

That slot is a **PE import slot**, so `*dispatch` is the RAW address inside
Runtime-x64.dll and **the port registry is never consulted** — a live instance of
`reference_bypassed_port_needs_a_source_checker.md`, in the one call site that could
reach the new port. Fixed by trying `ResolveExport` first and keeping the dispatch slot
as the fallback (the order `fn_DialogInstance_OnElementEvent` twenty lines above already
uses). `RuntimePortInit()` runs **before** `EngineFuncsInit()` —
`standalone/main.cpp` §2c says so in as many words — so `ResolveExport` reaches the port.

**The generalisable part:** a `RegisterRuntime*` call is not sufficient for a port to be
live. Any call site that caches a raw import-slot value bypasses it, and the only
symptom is a *silence* in the oracle.

### 2c. The oracle: what is compared, what is EXCLUDED, and the surviving blocker

A **replay** oracle is not merely weak here, it is corruption: retail's body **creates
two Elements and appends them**, so running it a second time on the same object yields
four children and a tab count of two.

The predictive round therefore compares the half that does not depend on the
unported callee, and **declares the other half** so it prints rather than hides:

**COMPARED, non-vacuously** (`changed=` is ≥ 1 every round because the tab count always
moves):

* `this+0xC0`, the tab count, `+1` (`0x1800DA286`).
* The **DIRTY WALK**: `p->_flags |= 0x8000000` for `this` and every ancestor up the
  `_parent` (+0x98) chain — run **twice**, once after each append (`0x1800DA211`,
  `0x1800DA270`), and **UNCONDITIONALLY**. Note the contrast with `UpdateContainers`,
  whose walk is *guarded* on "did bit 0 change": a tidied port that shared one helper
  between the two would be wrong in one of them. The second walk's predictions land on
  addresses the first already predicted and are reported in `superseded=`.

**EXCLUDED — holes that PRINT:**

* The children Array header at `this+0xA0` (data pointer, size, capacity). Retail
  appends two elements our suppressed body did not create.
* The two created Elements, including the `_parent` store at `elem+0x98`. They do not
  exist in predict mode.

**THE SURVIVING BLOCKER, NAMED:**
`?CreateInstance@ElementDescription@UI@@QEBAXV?$InternalPtr@VElement@UI@@@Object@@@Z`
(and the `ExternalPtr` vtable `+0x40` thunk that reaches it) is unported, so the append
half is covered only in the sense that its call target and arguments are **recorded**
(`RtSink::Call`). The day that callee lands, the `Exclude` on `this+0xA0` becomes a
prediction and this site's `excluded=` should go to zero.

Because ours writes nothing in predict mode, **the caller still receives retail's answer
in its return slot** — a predicting run is a working run.

### 2d. Routing — CHECKED, NOT ASSUMED

`get_xrefs_to 0x1800DA130` returns **four references and not one is a CALL**: three data
references (export table + two RTTI/relocation slots) and the external entry point.
**The DLL never calls its own `AddTab`.** The only caller in the process is ours,
`UIDesc_AddControllers_Impl` (`code/Game/Dialog/toolui.cpp:133`), which runs while a
dialog whose `UIDescription` carries tab entries is built. So — unlike
`TabElement::UpdateContainers`, which needed one — **this port needs no detour: there is
no split brain, because there is no second caller.**

The aliasing is retail's own and is called out in the source: `ret` is **both** AddTab's
hidden return slot **and** the destination of the second `CreateInstance`
(`0x1800DA232` loads RSI, the incoming RDX). An oracle that snapshotted `self` and `ret`
independently would be comparing one buffer with itself —
`reference_oracle_must_snapshot_aliased_inputs.md`.

### 2e. RESULTS

**See section 8** for the measured route, the numbers and the firing negative control.
The short version: three routes reported `rounds=0`, a three-counter reachability census
showed WHICH of the four causes that was, and the HOUSE PANEL route is the one that
builds tabs -- 2 rounds, 22 predictions, changed=2, wrong=0, **unpredicted=0**, with the
self-test naming `TabElement._tabCount` as its failing term.

---

## 3. CENSUS — THE SET-DIFF, NOT THE COUNT

`tools/runtime_census.py --unported --namespace UI`, after:

```
UI   total 1625   called 166   ported 137   detour 1   82.5% called-covered
```

**LEFT the "called but not ported" set (the two target rows, both closed):**

* `?AddTab@TabElement@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@V?$ExternalPtr@VElementDescription@UI@@@4@0@Z`
* `?CacheText@TextElement@UI@@AEAAXXZ`

**ENTERED it (two new doors, both named, stated rather than netted out** —
`reference_diff_the_set_not_the_count.md`**):**

* `??0Dialog@UI@@QEAA@XZ` — `runtime/src/ui/tabelement_addtab.cpp`
* `??1Dialog@UI@@UEAA@XZ` — `runtime/src/ui/tabelement_addtab.cpp`

Two closed, two opened. The alternative — open-coding the inline `UI::Dialog`
construction and teardown — would have opened **three ADDRESS doors** instead
(`0x180039280`, `0x180039360`, `0x180006B80`, none exported, none ported), which is
strictly worse: an address door is invisible to the export scanners and has no obvious
future owner. Two exported `UI::Dialog` lifecycle names have one.

**One row that looks like a regression and is not:**
`?SetParameterCount@TextElement@UI@@AEAAXXZ` now appears under *"Ports with no caller in
our tree"*. It is registered (name **and** address) and it does have a caller — a direct
C++ call from `CacheTextBody` in the same translation unit, which is exactly what retail
does (`CALL 0x18011E8E0` at `0x18011EC29`). The census's `called` scan only sees
`ResolveExport` literals, so an intra-TU caller is invisible to it. **Not vestigial.**

---

## 4. FILES

| file | change |
|---|---|
| `runtime/src/ui/textelement_cachetext.cpp` | **new** — `CacheText` + `SetParameterCount` + the predictive round + the content oracle |
| `runtime/src/ui/tabelement_addtab.cpp` | **new** — `AddTab` + the predictive round |
| `runtime/src/collection/array.cpp` | `kSpec_StringRefZ`, `J3_ENTRY2(180029a80, …)`, registration |
| `runtime/src/collection/array_internal.h` | `extern const ArraySpec kSpec_StringRefZ;` |
| `runtime/src/ui/widgets_layout.cpp` | two `Register` calls (bodies live in their own TUs so lane E's busy file is not a merge point) |
| `engine/funcs/engine_funcs.cpp` | **port-bypass fix** — `fn_TabElement_AddTab` resolves through `ResolveExport` first |

Registration is **name AND address** for all three new bodies.

---

## 5. STATUS / WHAT THE NEXT LANE SHOULD PICK UP

1. `CacheText` — **verified, both branches, both controls firing.** Done.
2. `AddTab` — **verified on the house-panel route**, negative control firing. Done.
2b. **The reachability census (`BANISHED_UIDESC_CENSUS=1`) is reusable** and should be:
   it turns any future `rounds=0` on a UI controller port into a measured statement
   about which of the four causes applies.
3. **Optional and not done:** a `[port]` detour on `0x18011EC10` to bring the DLL's ten
   internal `CacheText` callers to the port. It would raise the oracle's round count by
   orders of magnitude and close a knowingly-chosen split brain. `ui_detour.cpp` is the
   template.
4. `?CreateInstance@ElementDescription@UI@@…` is the single named thing standing between
   `AddTab`'s oracle and full coverage.

---

## 6. AUDIT RESULTS, INCLUDING TWO BLIND SPOTS THIS LANE MEASURED IN THE AUDITORS

| tool | result |
|---|---|
| `tools/port_bypass_audit.py` | **no new bypass.** All 5 findings (3 address, 2 name) pre-date this lane and are in `spritecontext_probe.cpp`, `platform_startup_win.cpp` and `widgets_layout.cpp`. |
| `tools/addr_door_audit.py` | **no new UNREGISTERED address door.** Both address doors this lane uses (`0x180029A80`, `0x180076900`) are registered; the 16 unregistered are elsewhere. |
| `tools/unsatisfied_export_scan.py` | 114 unsatisfied of 883. Entered: `??0Dialog@UI@@QEAA@XZ`, `??1Dialog@UI@@UEAA@XZ` — **the same two the census names**, from two independent instruments. |
| `tools/rawdoor_audit.py` | 692 sites, **2** `UNGUARDED-CALL` — see below. |

**Blind spot 1 — the auditor's guard vocabulary does not include the predict gate.**
`rawdoor_audit.py` flags `textelement_cachetext.cpp:505` (`RT_ORIGINAL_ONCE(CacheText)`)
as `UNGUARDED-CALL`. It is not reachable outside `BANISHED_RT_PREDICT`: the function
returns before it when `RT_PREDICT_ON` is false. The auditor recognises `rt_diff_on__`
as a guard and not `rt_predict_on__`. Note that it flags **`widgets_layout.cpp:770`**
— Wave 10's own `UpdateContainers` oracle door, identical in shape — for the same
reason, so this is a property of the auditor, not of either port. Left as-is rather than
reshaped to please a checker; the fix belongs in the checker.

**Blind spot 2 — and this one is the interesting half.** `AddTab`'s oracle door is
`RT_ORIGINAL_ONCE` on the *same* raw-export mechanism, on the same live path, in the
same commit. **`rawdoor_audit.py` does not see it at all** — because the mangled name is
long enough that it is written as a **multi-line concatenated string literal**, and the
scanner matches a literal on one line. Two doors of identical shape: one flagged, one
invisible, and the only difference is source formatting. This is the multi-line-literal
blind spot `reference_census_three_blind_spots.md` recorded for `runtime_census.py`,
alive in a second tool. **Anyone reading `2 UNGUARDED-CALL` as "there are two" is
reading a number the instrument cannot produce correctly.**

---

## 7. TWO COORDINATOR ITEMS, ANSWERED

### 7a. The `Dialog.h` `ExternalPtr` field swap — THIS LANE IS NOT AFFECTED, and it has an independent third witness

Lane UI3 found `runtime/types/ui/Dialog.h:92-93` naming
`DialogControllerConfigEntry`'s two `ExternalPtr` fields backwards (`_config` at
`+0x08`, `_elementDesc` at `+0x10`; `0x180114A13` / `0x180114A1C` say the opposite), and
the header's own warning comment carries the error, so reading the comment confirms the
wrong answer.

`AddTab` takes two `ExternalPtr<ElementDescription>` arguments, so the question is fair.
**Neither new file includes `Dialog.h`,** and neither reads those fields by name or by
offset: `AddTab` takes both descriptions as opaque pointers and calls the `ExternalPtr`'s
own vtable slot `+0x40` on each, exactly as retail does (`0x1800DA1E5`, `0x1800DA235`).

More usefully, **`AddTab`'s only caller is an independent third witness that lane UI3 is
right.** `UIDesc_AddControllers_Impl` (`code/Game/Dialog/toolui.cpp:38-46`) was derived
from the exe call site, not from `Dialog.h`, and its entry table reads:

```
//   +0x08: ExternalPtr<ElementDescription> elementDesc
//   +0x10: ExternalPtr<DialogControllerConfig> config
//   +0x18: ExternalPtr<ElementDescription> tabDesc
```

— `+0x08` is the DESCRIPTION, which is lane UI3's reading and the opposite of
`Dialog.h`'s. Two derivations from two different call sites agree; the header is the
outlier. (The argument order into `AddTab` is `R8 = entry+0x18` (tabDesc, becomes the
tab) and `R9 = entry+0x08` (elementDesc, becomes the content page handed to
`AddController`), which is consistent with what each half is used for downstream.)

### 7b. Lock discipline and exe provenance

`tools/game_dir_lock.sh acquire` was called **bare with its status checked** in every run
script this lane used (`acquire … || exit 1`), never through a pipe, so the discarded-exit-
code defect the coordinator describes did not apply here. The AddTab run additionally uses
the hardened script (`f9a9a9c`, cherry-picked onto this branch) with `verify lane-UIORACLE`
immediately before each launch, and `set -o pipefail`.

**Exe provenance.** Runs A–E printed the md5 of *our build* at deploy time
(`f2357c54…` for A–C, `bd5f19e5…` for D–E) but not of the *deployed copy* immediately
before launch. None of them spans 23:00–23:01, the window the coordinator flagged (A–C
22:24–22:30, D–E 22:31–22:36). Independently of the md5, those runs cannot have measured
another lane's binary: `[RtPredict] UI::TextElement::CacheText` and the `[UiOracle]`
summary line exist **only** in this lane's build, and both are present in every one of
their logs. The AddTab run compares `mine` against the **deployed** md5 before and after
each launch and prints all three.

---

## 8. FINAL RESULTS — BOTH SITES VERIFIED, BOTH CONTROLS FIRING

Everything below is from the exe `e6975e65ffbfce042a095530f8b40802`, whose md5 was
compared against the **deployed** copy immediately before *and* after every launch
(printed in every cell). Lock acquired bare with its status checked, and
`game_dir_lock.sh verify lane-UIORACLE` re-run before each launch.

### 8a. `?AddTab@TabElement@UI@@…` — the route was FOUND, not given up on

Three routes reported `rounds=0`, which reads identically to *"the port is dead"* and to
*"no route exists"*. Rather than write "unverifiable", this lane **measured which**, with
a three-counter census in `AddTab`'s only caller
(`UIDesc_AddControllers_Impl`, `BANISHED_UIDESC_CENSUS=1`) — armed at **static init**, not
on first entry, because an `atexit` armed by the first call can never print the "never
ran" case, which is the case worth reporting:

| route | controller entries seen | element found | **took the TAB path** | verdict |
|---|---|---|---|---|
| autostart gameplay | 0 | 0 | 0 | `UIDesc_AddControllers_Impl` never ran |
| Options dialog + `BANISHED_OPTIONS_TAB` | 0 | 0 | 0 | never ran — the Options tabs are **not** built by `AddTab` |
| trading-post panel | 7 | 7 | **0** | ran, and this panel builds no tabs |
| **house panel** (`BANISHED_TOOL_HARNESS=housepanel BANISHED_BUILD_DEMO_HOUSE=1`) | 7 | 7 | **2** | **the route** |

So the earlier zeroes were not a dead port and not a missing route: they were three
routes that provably do not build tabs, and one that does.

**RESULTS on the house-panel route:**

| cell | rounds | predictions | changed | **wrong** | unpredicted | superseded | excluded | suppressed calls |
|---|---|---|---|---|---|---|---|---|
| `tabClean` | 2 | 22 | **2** | **0** | **0** | 10 | 32 B | 6 |
| `tabControl` (self-test) | 2 | 22 | 2 | **2** | 0 | 10 | 32 B | 6 |

* `superseded=10` is the **second dirty walk** showing up exactly where the port
  documents it — retail runs the walk twice, unconditionally, and the second run's
  predictions land on addresses the first already predicted. That column is the design
  working, not a wart.
* `excluded=32 B` is 2 rounds × the 16-byte children Array header — the append half,
  held out because `ElementDescription::CreateInstance` is unported. `6 suppressed
  indirect call(s)` = 2 rounds × (2 `CreateInstance` + 1 `UpdateContainers`).
* **`unpredicted=0`** is the load-bearing one: every byte retail changed inside the
  watched regions (`this->_flags`, every ancestor's `_flags`, `this+0xC0`) was covered by
  a prediction. Predictions alone are a lower bound; this is the column that turns the
  round into an oracle.
* **The negative control fired and NAMED ITS TERM:** *"0 site(s) could NOT demonstrate a
  failure"*, `wrong=2`, with
  `TabElement._tabCount … pre=0x0 ours=0x0 theirs=0x1` and
  `… pre=0x1 ours=0x3 theirs=0x2`. A verdict computed over N terms whose message names
  none of them is the defect `reference_shadow_predicate_must_name_its_failing_term.md`
  was written about; this one names them.

### 8b. `?CacheText@TextElement@UI@@AEAAXXZ` — 27,198 and 45,666 rounds, clean

| route / cell | rounds | predictions | changed | **wrong** | unpredicted | content oracle |
|---|---|---|---|---|---|---|
| trade panel, `cellK` | 27,198 | 15,434 | 279 | **0** | **0** | `WRONG=0 interned=12043 formatted=15155 changed=174` |
| trade panel, `cellL` (self-test) | 27,198 | 15,434 | 279 | 151 | 0 | `WRONG=0 selftest=26946/27198 missed 0 na-mismatch 0 (n/a 252)` |
| house panel, `tabClean` | 45,666 | 30,316 | 25 | **0** | **0** | `WRONG=0 interned=15375 formatted=30291 changed=51` |
| house panel, `tabControl` (self-test) | 47,124 | 30,315 | 24 | 17 | 0 | `WRONG=0 selftest=31984/47124 missed 0 na-mismatch 0` |
| autostart gameplay, `cellJ` | 14 | 19 | 11 | **0** | **0** | `WRONG=0 interned=6 formatted=8 changed=14/14` |

Both branches are exercised in volume on every route (`interned` and `formatted` are both
non-zero everywhere), `wrong=0` and `unpredicted=0` throughout, and **`missed=0`** — no
round was corrupted on purpose and still agreed.

### 8c. THE TWO DEFECTS IN THIS LANE'S OWN INSTRUMENT, found at volume

Both were caught because the clean and control cells were compared **column by column**
rather than read as two independent green boards.

1. **The effectiveness counter read freed memory.** `changed` compared against `pre` —
   the buffer retail is about to RELEASE, and on the freeing arm hands to `MemFree`. Two
   runs of the *identical, deterministic* 27,198-round route reported `changed=4384` and
   `changed=7540`. A counter that disagrees with itself across a control pair is
   measuring the allocator, not the port. The pre **text** is now snapshotted before the
   round; the same two cells now report `changed=174` and `changed=174`.
2. **The verdict counted the control's own collateral.** On a self-test run, a round whose
   predicted text was empty had nothing to corrupt, and a mismatch there fell through to
   `WRONG` — reporting `WRONG=4` on the control run against `WRONG=0` on the identical
   clean run, i.e. **attributing to the port a difference only the control had.** Now
   split into `missed` (corrupted and still agreed — the control failing) and
   `na-mismatch` (armed, not applicable). Both are 0.

### 8d. A REAL PORT DEFECT IN A THIRD FILE, exposed by this port and fixed here

`RtUi_StringTable_GetText_Index` (`runtime/src/ui/sprite.cpp`, retail `0x180002550`) read
`data` (`this+0x20`) and `count` (`this+0x28`) **above** its `index >= 0 && index < count`
test. Retail's first act is the sign test, and on a negative index it returns an empty
String **without dereferencing `this` at all** (`0x18000256C TEST R8D,R8D / JNS / MOV
[RDX],R9`). A `UI::String` that names no text is `{table = nullptr, id = -1}`, and
`UI::String::operator System::String` forwards it here verbatim — so retail's guard
**order** is what makes the ordinary "no text" value harmless, and the hoisted loads made
it an `ACCESS_VIOLATION` reading `0x20`.

This is **not** the "don't fix a faithful crash with a null check" case. No guard is
added; retail's guards are restored to retail's order, and the upstream null is not a
defect to hunt — it is the representation of "no text".

**The four-cell A/B that pinned it** (same exe md5 in every cell, trading-post route):

| cell | switches | result |
|---|---|---|
| 1 | default | **CRASH** `+0x36807E` |
| 2 | `BANISHED_RT_UIWIDGETSLAY=0` (CacheText back on the DLL) | clean |
| 3 | `BANISHED_RT_UISPRITE=0` (this body back on the DLL) | clean |
| 4 | `BANISHED_RT_UI=0` | clean |

Two independent switches, each removing exactly one half of the pair, each making it go
away — so the fault needed both ports live and was not a property of the route. Porting
`CacheText` made our tree the first caller to route a never-assigned `UI::String` through
that port instead of through the DLL's own copy.
