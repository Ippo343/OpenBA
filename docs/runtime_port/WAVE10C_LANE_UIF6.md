# Wave 10C lane UIF6 — the six UI callee rows

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-ae85bf25d38873eaa`
**Branch** `wave10c/lane-uif6`, based on `wave10/base` @ `e868337` (verified with
`git merge-base --is-ancestor` — the worktree was seeded from `965361d`, an
ancestor, and the branch was cut at `e868337` explicitly).

All six rows are ported, both doors registered, debug and release green, two
harness cells clean (`rc=0`, `[Shutdown] done`, 0-byte crash log).

---

## 1. The per-function table

Addresses were recovered from the shipped export table with `pefile`, not from
Ghidra — see §4 for why that mattered for one of them.

| # | symbol | addr | size | route used | entries | verdict |
|---|---|---|---|---|---|---|
| 1 | `?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z` | `0x180120830` | 0x28 | **input sweep** vs the shipped export | **1014** (205 true) | **VERIFIED** — 0 divergences, control caught 37 |
| 2 | `?EndDragDrop@UserInterface@UI@@QEAAXXZ` | `0x180120af0` | 0x8b | none found | **0** | ported, **NOT verified** |
| 3 | `?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z` | `0x180113d10` | 0x6c | none found | **0** | ported, **NOT verified** |
| 4 | `?BeginToolTip@UserInterface@UI@@QEAAX…` | `0x180120860` | 0x205 | none found | **0** | ported, **NOT verified** |
| 5 | `?UpdateStandAloneUI@UserInterface@UI@@CAXAEBVTime@System@@PEAX@Z` | `0x180120310` | 0xd6 | `BANISHED_MODAL_DRIVER=2 BANISHED_MODAL_DRIVER_TEST=1` | **12** (audio arm 12/12) | **LIVE**, not differentially verified |
| 6 | `?DrawStandAloneUI@UserInterface@UI@@CAXAEAVContext@Video@@PEAX@Z` | `0x1801203f0` | 0x0e | same cell | **28** | **LIVE**, not differentially verified |

Files: `runtime/src/ui/userinterface_hover.cpp` (rows 1–4),
`runtime/src/ui/userinterface_standalone.cpp` (rows 5–6).
Switches: `BANISHED_RT_UIHOVER` (1–3), `BANISHED_RT_UITOOLTIP` (4),
`BANISHED_RT_UISTANDALONE` (5–6) — all default ON.
Sweep: `BANISHED_UIF6_SWEEP=1`, default OFF.

### What "NOT verified" means here, precisely

Rows 2, 3 and 4 are **mouse-driven**, and an unattended run has no mouse. They
are called from `runtime/src/ui/element_vtable.cpp`'s `Element::Update`, on the
hover / drag-drop / tooltip arms, all of which are gated on pointer state that
nothing in `standalone/` synthesises. I checked for a mouse-injection harness
before writing this — `grep` over `standalone/*.cpp` for mouse/inject/env
handling — and there is none
(`reference_grep_the_harness_before_declaring_no_route.md` cuts both ways: the
grep is the obligation, not the guaranteed find).

The counter that says so is
`[RtPort] UI::UserInterface::BeginToolTip entries=0 …` and its two siblings,
printed unconditionally in every configuration. They are **not** oracle
counters: `calls=` on an `RtPredictSite` counts suppressed indirect calls and
cannot separate "ran and agreed" from "never reached"
(`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`).

Two things this is **not**:
* It is not an early-resolve bypass. `[RtResolveEarly] 0 registry-aware resolves
  happened before RuntimePortInit() finished` and `[RtResolve] 0 misses` on both
  cells, so the doors are live and the bodies are genuinely just not called.
* It is not a dead registration. `[RtPort] UI: 192 exports registered` and row 1,
  in the same file behind the same registration path, entered 1014 times.

Rows 5 and 6 **ran**, on a real nested `ShowDialog` modal pump, and
`UpdateStandAloneUI` took its audio-scene arm on all 12 entries. They are marked
"not differentially verified" only because neither is replayable: `Update` and
`Draw` mutate the live UI, and a `Random` seeded from `time(NULL)` is not
reproducible by construction.

---

## 2. The sweep — how row 1 is actually verified

`IsInsideClipRegion` is twelve instructions with no calls and no writes, so it
is **pure**, and for a pure function an input sweep against the shipped export
beats waiting for a caller (`project_pure_functions_verify_by_sweep.md`). OURS
predicts, THEIRS runs for real — a true differential, not an `RT_DIFF_SHADOW`
self-check (`reference_shadow_marker_is_not_an_oracle.md`). THEIRS is reached
with `RtOriginal`, never `ResolveExport`, or the oracle would compare ours to
ours.

```
[UIF6/sweep] IsInsideClipRegion: cases=1014  divergences=0  (effectiveness: 205
             cases returned true, so the comparison is not trivially satisfied)
[UIF6/sweep] negative control (right edge made EXCLUSIVE): caught=37 of 1014.
[UIF6/sweep] VERDICT: PASS
```

Both controls matter and both are reported separately:

* **Effectiveness** — 205 of 1014 cases return `true`. A sweep whose subject
  always returns `false` would report `divergences=0` while comparing nothing.
* **Discrimination** — the negative control flips the RIGHT edge from inclusive
  to exclusive (`x > right` → `x >= right`), exactly the mistake a reader of
  `JG` would make. It is caught on 37 cases. It is only visible on inputs that
  land *exactly* on a boundary, which is why the grid is built at `edge + {-2,
  -1, 0, 1, 2}` for all four edges rather than sampled at random
  (`reference_control_must_pick_a_compared_subject.md` — the control has to be a
  plausible defect on a subject the comparison actually covers).

The six rectangles include a degenerate single pixel, one straddling the origin,
one wholly negative, and one **inverted** (`right < left`, admitting nothing).
The `UserInterface` buffer is poisoned to `0xAB` before each rectangle, so a
port reading any field other than the four at `+0xfc..+0x10b` would diverge.

Note the 1014 sweep calls **inflate row 1's entry counter**; the gameplay
contribution in cell A is 0, same as its three neighbours.

---

## 3. THE REGRESSION I INTRODUCED, AND THE FIX

**The first B cell hung to its 420 s timeout with no error line, and it was my
fault.** This is the most useful thing this lane found.

`standalone/modal_driver.cpp:214` identified the DLL-installed nested-UI
callback by pointer:

```c
void* wanted = reinterpret_cast<void*>(ResolveAddr(Module::Runtime, kUpdateStandAloneUI));
for (int i = 0; i < count; ++i) if (arr[i].fn != wanted) continue;
```

Its own comment claimed detection "works identically whether the ShowDialog that
registered it was the shipped DLL's or ours". That held **only while nothing was
registered at `0x180120310`**. `ResolveAddr(Module::Runtime, …)` consults the
**port registry first**
(`reference_resolveaddr_consults_port_registry_first.md`), so the moment this
lane's `RegisterRuntimeAddr(0x180120310, …)` landed, `wanted` became *our*
function pointer while the DLL's own `ShowDialog` kept installing the DLL's.
Nothing ever matched, `FindModalDialog` returned null forever, the modal was
never dismissed, and the run hung — silently, with a plausible log.

That is the diagnostic shape this project keeps paying for: an instrument
reporting confidently about the wrong thing. It also cost the *first* B cell
entirely, and the failure looked like "lane SHOW's harness is flaky" rather than
"a lane just broke it".

**Fix**: the function's real predicate is "either implementation of
`UpdateStandAloneUI`", so it now names both doors explicitly — the raw DLL
address via `ResolveRuntimeRaw` (what the DLL's `ShowDialog` installs) and the
registry address via `ResolveAddr` (what our `ShowDialog` port installs) — and
matches either. A null from either simply never matches, which is the correct
degenerate behaviour at cutover. After the fix, cell B runs to `rc=0` and both
statics report entries.

**Note for the integrator:** this is a fix to a file lane SHOW owns. It is
required for `BANISHED_MODAL_DRIVER` to keep working now that these two
addresses are registered — reverting my `runtime/src/ui/` changes without also
reverting `standalone/modal_driver.cpp` is fine, but keeping the ports while
reverting the driver fix re-hangs the modal test.

---

## 4. Three findings that are not the code

### (a) `DialogInstance+0x98` is `_hovering`, not `_closing`

`runtime/types/ui/DialogInstance.h` carried `_closing` flagged **"NAME IS A
GUESS"**, and `+0xa0` as `void* _opaqueA0`, "nothing in this lane reads it".
Both are now named from evidence:

* `?IsHovering@DialogInstance@UI@@QEBA_NXZ` (`0x1800455a0`) is the **entire
  function**: `MOVZX EAX, byte ptr [RCX+0x98]` / `RET`. A one-line exported
  accessor is the strongest field-evidence class this tree recognises
  (`UserInterface.h`, "FIELD EVIDENCE"). `EndHover` clears the same byte and
  emits UI event `0x11`; `element_vtable.cpp:735` gates the tooltip
  begin/move arm on it. "Closing" was actively misleading.
* `+0xa0` receives `(int64)(0.0 / _secondsPerTick)` from `EndHover` — a
  `System::Time` tick count, not a pointer. Retyped `void*` → `int64_t`; same
  eight bytes, same zero store, so no layout change. `dialog_create.cpp:779-780`
  updated to match.

### (b) Ghidra has no function at `DrawStandAloneUI`

`decompile_function` and `disassemble_function` both return *"No function found
at or containing address 0x1801203f0"*, `create_function` fails, and
`read_memory` fails on the range — while `disassemble_bytes` on the same range
reports `"success": true, "bytes_disassembled": 96`. A lane that trusted the
error would have recorded "no such function"; one that trusted the success would
have recorded a disassembly it never saw. The bytes were read straight out of
the shipped PE and disassembled with `objdump` instead, which is how the
fourteen-byte body was recovered.

### (c) `UpdateStandAloneUI` inlines `System::Random`'s constructor

The 0xa10-byte stack frame is the tell: retail builds 2500 bytes of
Mersenne-Twister state **every tick** just to hand `Audio::Scene::Update`
something to draw from. This port calls `??0Random@System@@QEAA@XZ` rather than
re-inlining the recurrence, and the equivalence was **proven, not assumed** —
the export reaches the same seed callee (`0x18007fb9c`, `_time64`), writes
`index = 0` first, and runs the same 623-iteration recurrence (the export counts
`R8D` down from `0x26f`; the inline counts up to `R8D < 0x270` from 1 — the same
623 writes to `mt[1..623]`). Re-typing a recurrence we can call by name is how
transcription defects get minted
(`reference_crt_coefficients_must_be_lifted_not_retyped.md`).

---

## 5. Faithfulness notes kept rather than tidied

* **`BeginToolTip`, `0x1801208ca`**: `MOV RAX,[RCX+0x30] / CMP RAX,RAX / JNE` —
  a register compared with **itself**, so the branch never fires and the guard
  it looks like is inert: an existing tooltip is **always** closed. Reproduced.
  Writing the comparison the code looks like it wanted (`tip->_dataSource !=
  elem`) would be a behavioural change, not a cleanup.
* **Event record `+0x24` is never written** at either push site. The record is a
  raw stack local in retail and stays one here; zeroing it would be a real
  difference in the bytes `SetSize` copies, and an oracle must not compare them
  (`reference_oracle_must_not_compare_unwritten_bytes.md`).
* **`EndDragDrop` does not clear `_dragDropActive` (+0x88)**, though it clears
  every neighbour up to `+0x80`. Left alone.
* **`EndDragDrop` stores `_dragDropElement = 0` twice** (`0x180120b5a` and
  `0x180120b5e`) — a duplicated compiler tail. Both reproduced.
* **`elementId` differs between the two event pushes**: `-1` in `EndDragDrop`,
  `0` in `EndHover`. Not normalised.
* **`BeginToolTip` calls `GetToolTipString` twice** — once to test emptiness,
  then again on the non-empty arm. Not hoisted: it is not obviously pure and
  hoisting would change the refcount traffic the DLL sees.
* **`BeginToolTip` hands a possibly-null `_toolTipDialog` to `SetText`** when
  `operator new` returns null. Left unguarded per CLAUDE.md — that is retail's
  contract with its own allocator, not a bug for this port to paper over.
* **The `System::String` release asymmetry** (freeing branch leaves a dangling
  `data - 8`, sharing branch nulls) is reproduced; `String.h` says in so many
  words "Reproduce it; do not tidy it." `MemFree` is reached with
  `RT_ORIGINAL_ONCE`, never `ResolveExport`, because string storage must go back
  to the DLL's own allocator.

---

## 6. Census set diff — measured, and where

Measured with `tools/runtime_census.py` run **from this worktree root** (the
scanners walk `os.walk("runtime")` relative to CWD,
`reference_scanner_walks_cwd_not_script.md`), using the main checkout's venv
interpreter because this worktree has no `.venv`.

The **full** census (not just `--namespace UI`) was measured **both ways on the
same tree**, by moving this lane's two `.cpp` files aside and back, so the diff
cannot be contaminated by another lane's changes:

```
full census, unported:  196  ->  194
```

```
LEFT (6, all this lane's rows)
  ?BeginToolTip@UserInterface@UI@@QEAAXV?$InternalPtr@VDialogInstance@UI@@@Object@@V?$InternalPtr@VElement@UI@@@4@HH@Z
  ?DrawStandAloneUI@UserInterface@UI@@CAXAEAVContext@Video@@PEAX@Z
  ?EndDragDrop@UserInterface@UI@@QEAAXXZ
  ?EndHover@DialogInstance@UI@@QEAAXPEAVUserInterface@2@@Z
  ?IsInsideClipRegion@UserInterface@UI@@QEBA_NHH@Z
  ?UpdateStandAloneUI@UserInterface@UI@@CAXAEBVTime@System@@PEAX@Z

ENTERED (4, ALL of them BeginToolTip's callees)
  ??2ToolTipDialog@UI@@SAPEAX_K@Z
  ??_7ToolTipDialog@UI@@6B@
  ?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ
  ?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z
```

UI namespace: **9 rows → 7 rows**. Baseline archived at
`docs/runtime_port/wave10c/baseline_census_uif6_e868337.txt`, final at
`docs/runtime_port/wave10c/final_census_uif6.txt`.

**Read the set, not the ratio** (`reference_diff_the_set_not_the_count.md`). The
four entrants are the honest price of not opening four silent address doors
(`reference_porting_can_grow_the_cutover_surface.md`), and all four are
`BeginToolTip`'s — the one row in this lane that allocates and constructs.

**Nothing entered outside UI**, which is worth stating because five of the new
bodies' callees live elsewhere: `?MemFree@MemoryAllocator@System@@`,
`??0Random@System@@QEAA@XZ`, `?Update@Scene@Audio@@`,
`?_secondsPerTick@Timer@System@@0NA` and the two already-ported `UserInterface`
halves were all already in the `called` set or already ported. That is why the
full-census check was run rather than only the UI one — a `--namespace UI` diff
is a lower bound on residue and would have hidden any of those.

### New address doors: **none**

The only two by-address callees are `0x180033690`
(`Array<UI::Event>::SetSize`, already owned by `userinterface_frame.cpp`,
`userinterface_close.cpp` and `element_vtable.cpp` — this is the fourth call
site, not a fourth obligation) and `0x18007a0d0`
(`Array<InternalPtr<DialogInstance>>::SetSize`, already owned by
`userinterface_showdialog.cpp` and already **ported** by
`runtime/src/collection/array.cpp`). Both go through
`ResolveAddr(Module::Runtime, …)`, which consults the port registry first.

Every other callee is a **name** door, checked against the shipped export table
with `pefile` rather than assumed — including the two `CAXA…` private statics
and `??_7ToolTipDialog@UI@@6B@`, which is exported DATA
(`reference_aeaa_is_private_not_unexported.md`).

---

## 7. Build and run evidence

* **Debug**: green. **Release**: green from a genuine `make clean` first — a
  `make standalone` straight after a debug build says "Nothing to be done" and
  proves nothing (shared object dir).
  The release build **initially failed in both of this lane's files**:
  `standalone/log_strip.h` `#define printf(...)` collides with `std::printf`, so
  the qualified spelling does not compile under `BANISHED_STRIP_LOGS`. Same
  defect as commit `965361d`. Fixed by unqualifying `printf`/`fflush`; a no-op
  in debug.
* **Both cells re-run after that edit**, on a clean-rebuilt debug exe, rather
  than claiming the change was cosmetic
  (`reference_lane_edits_after_report_invalidate_the_measurement.md`). Numbers
  reproduced exactly.
* Both cells: `rc=0`, `[Shutdown] done`, **no crash log**,
  `[RtResolve] 0 misses`, `[RtResolveEarly] 0`.
* All runs under `tools/game_dir_lock.sh run lane-UIF6 1800 --snapshot
  ./lane_uif6_logs`; `acquire` was never piped and never backgrounded.
* Raw logs: `lane_uif6_logs/{A_utility,B_modal}.{out,err}.log`. Driver:
  `tools/uif6_run.sh`.

## 8. What is honestly open

1. **`EndDragDrop`, `EndHover`, `BeginToolTip` have never executed.** They need
   mouse input. The cheapest real route would be a mouse-position/click injector
   in `standalone/` — none exists today. A **detour** on these three addresses
   was considered and rejected: it would change *who* reaches the body, not
   whether the game hovers, so it adds DLL-prologue-patching risk and buys no
   coverage. `IsInsideClipRegion` was the exception only because it is pure.
2. **`BeginToolTip` is the risk row.** It allocates, runs the DLL's
   `DialogInstance` ctor, stamps a vtable, and does two `System::String`
   releases — all unexercised. It has its own switch (`BANISHED_RT_UITOOLTIP`)
   for exactly that reason, and a merge that wants to be conservative can ship
   it off without touching the other five.
3. **Rows 5 and 6 are entered but not differentially verified** — neither is
   replayable, and `UpdateStandAloneUI` seeds a PRNG from `time(NULL)`.
4. **The four new census rows** are the next lane's scope, and they are a
   coherent cluster: `ToolTipDialog`'s `operator new`, vtable, and `SetText`,
   plus `Element::GetToolTipString`.
