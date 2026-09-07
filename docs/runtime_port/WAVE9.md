# Wave 9 — Cutover

Prep opened 2026-09-07 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2
wave 9. Nothing is ported yet; this document exists to replace that row, which said
**"Remove `LoadLibrary` of Runtime; retire the §2.2 patches", size S**.

**That row is wrong, and it is wrong in a way that would have cost a session to discover.**
The measurements below are what the row should have said.

## The one success criterion

PLAN §5, the only unchecked box in the whole document:

> `reimpl-x64.exe` boots to gameplay with `Runtime-x64.dll` absent from the game directory.

Everything here is in service of that single line.

## What "S" missed

### 1. The gap is 319 items, not "a LoadLibrary call"

`tools/runtime_census.py`, 2026-09-07, after the three tool defects below were fixed
(`docs/runtime_port/census_20260907_prewave9.csv`, per-item list in
`docs/runtime_port/wave9_lanes.txt`):

**823 called, 504 ported (61.2%) — 319 unported.**

| Kind | Count | What it is |
|---|---|---|
| function | 145 | ordinary named exports |
| unnamed DLL internal, by address | 57 | `ResolveAddr(Module::Runtime, 0x1800xxxxx)` with no export name — the `FUN_` pool |
| ctor/dtor | 51 | `??0` / `??1`; these bind the class-atomicity rule below |
| `??_7` vtable | 28 | exported DATA |
| `?_singleton@…` | 17 | exported DATA |
| `?_typeInfo@…` (`BaseTypeInfo`) | 14 | exported DATA |
| ordinal | 4 | |
| DLL `.data` global, by address | 3 | `0x1801f…`, e.g. the `MainLoop` singleton region |

By namespace: internal 60, Video 59, UI 48, Graphics 28, System 23, Object 22, IO 17,
Achievement 16, Engine 14, Audio 8, Input 7, Math 5, ordinal 4, Application 3, Save 3,
Utility 1, Volume 1.

**59 of the 319 are DATA, not code.** A vtable, a singleton slot and a `BaseTypeInfo` cannot
be "ported" in the sense the per-function recipe means — there is no body to reimplement.
They have to be *minted*, with a layout that every ported and unported consumer agrees on.

### 2. Every verification mechanism this project has built dies at cutover

This is the part that makes Wave 9 unlike waves 1–8, and it is not a scheduling detail — it
decides the order of the work.

| Mechanism | What happens when the DLL is gone |
|---|---|
| `BANISHED_RT_DIFF` differential oracle | **Gone.** It compares our result against the DLL's. There is no second implementation to diff against. Every one of the 79.0M diffed calls that closed Wave 8 was borrowed from a DLL that will not be there. |
| `RtOriginal` / `RT_ORIGINAL_ONCE` | **Gone.** It is `GetProcAddress` against a module that is not loaded (`reference_rtoriginal_is_getprocaddress.md`). Every call site must be compiled out, not left to return null. |
| "Stamp the DLL's own exported `??_7`" | **Gone.** Waves 6 and 7 both avoided a detour because `??_7Geometry` and all 68 UI vtables are exported, so a ported ctor copies the DLL's vtable byte for byte. With no DLL there is no vtable to copy: every ported ctor must mint one whose slots ALL point at ported functions. |
| `BANISHED_RT_PORT=0` A/B | **Gone.** The kill-switch routes to the DLL. |
| The detour question | **Moot, and that is the good news.** "The DLL's own internal callers" (§4.1 decision 3) is only a problem while the DLL runs its own code. It never needed a detour in 8 waves; at cutover it stops being a category. The count stays 0 forever. |

**The consequence, stated plainly: nothing may be ported during Wave 9 that has not been
diff-verified before the DLL is removed.** Waves 1–8 could port and verify in the same
sitting. Wave 9 cannot. The verification budget has to be spent *first*, on the DLL that is
still there, and the removal is the last commit, not the first.

That inverts the per-function recipe (§6 step 6) for this wave only, and it is why the
holdbacks matter more than usual: **the 32 functions Wave 8 held back, the 5 handle-table
mutators, the 4 `ExternalFactory` mutators and the whole allocator all have to come back and
be verified against the DLL now**, while an oracle still exists.

### 3. The allocator NO-GO stops being optional

`runtime/src/system/allocator.cpp` + `allocator_win.cpp` are 27 registrations behind
`BANISHED_RT_ALLOC`, with a written **NO-GO** verdict ([WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md)).
Wave 8 found the NO-GO is really four functions wide, not the ctor/dtor pair PLAN predicted:
`Engine::Startup` writes three allocator fields directly and `Shutdown` calls the allocator's
shutdown.

With the DLL absent there is no other allocator. Every `MemAlloc`/`MemFree` in the process
becomes ours, including the ones inside ported bodies that currently allocate through the
DLL's heap. `?MemFree@…` and `?MemAlloc@…` are the two highest-count entries in the residue
census below, which is what you would expect and also what makes this the single riskiest
item in the wave: `str-4`/`str-8` header conventions, the boundary-tag layout, and the
half-written-pointer corruption family all live here.

**This is the one item that cannot be staged.** A heap cannot be half-ours.

### 4. `dx11/` is part of the cutover surface and was not being counted

`dx11/` is our VideoDX11 replacement and the existence proof that a DLL can be removed —
but it *imports from Runtime-x64.dll* exactly as the shipped plugin did, and 60 of those
imports were invisible to the census until 2026-09-07 (defect 1 below). Video's called count
goes 69 → 112 on that correction alone. Any cutover plan that starts from `code/` and
`runtime/` and forgets `dx11/` is short by ~50 functions.

## The tool this wave needed first: `BANISHED_RT_STRICT`

`tools/runtime_census.py` answers "how much is ported" **from source**. It cannot answer the
question the cutover gate actually asks, which is **"does anything still ASK the DLL for
something at runtime"** — because the source list is a superset that counts every resolve
site in the tree, including ones no reachable path executes.

`BANISHED_RT_STRICT=1` (`standalone/resolve_standalone.cpp`, commit `f95c1af`) records every
`ResolveExport` / `ResolveOrdinal` / `ResolveAddr(Module::Runtime)` that misses the port
registry and falls through to the shipped DLL, with the return address of whoever asked, and
prints one `[RtResidue]` line per distinct miss beside `[RtCensus]`.

It is **log-only by design**. It does not null the result or hand back a trap stub: 59 of the
misses are exported DATA, and poisoning those corrupts the process long before the call that
would name the culprit. Strict mode observes. Deleting the DLL is what simulates the DLL
being gone.

**An empty `[RtResidue]` list across the full harness matrix is the real cutover gate.**

First run — one `BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1` boot → gameplay → quit, log kept
at `docs/runtime_port/wave9_residue_autostart.txt`:

**260 distinct requests still arrived at Runtime-x64.dll.**

| Kind | Count |
|---|---|
| function | 100 |
| unnamed DLL internal, by address | 55 |
| ctor/dtor | 39 |
| `?_typeInfo@…` | 27 |
| DLL `.data` global, by address | 19 |
| `?_singleton@…` | 11 |
| `??_7` vtable | 7 |
| ordinal | 2 |

47 of the 260 are reached only from `dx11/`.

Counts are per *resolve*, not per call — most of these are pointer-table fills at init, so
`?MemFree@…` showing 7 does not mean seven frees. The residue measures the **resolve
surface**, which is exactly what the cutover gate is about.

### The residue is almost entirely path-independent, and that cuts both ways

A second run on a completely different path — `BANISHED_AUTOLOAD=Save9`, which quit at the
menu after 20 s and never reached gameplay at all — produced a residue set **identical to the
full boot → gameplay → quit run. Same 260 entries, zero either way.**

That is `EngineFuncsInit` / `RuntimeFuncsInit` doing their job: the typed `fn_*` tables are
filled eagerly at boot, so almost every name in the tree is resolved before the first frame
regardless of what the run then does.

**Good news for lane 0:** the work list is essentially complete after one boot. The residue
matrix is much cheaper than a per-path sweep would suggest.

**But it is not a reachability proof, and must not be read as one.** An eagerly-resolved name
appears in the residue whether or not anything ever calls through the pointer — this is
`reference_uncalled_port_is_not_verified.md` wearing the opposite hat. The residue answers
"what does the process ASK the DLL for", which is the right question for *cutover*
(an unresolvable import is fatal whether or not it is called), and the wrong question for
*verification*. Lane 0 still needs the matrix, for the opposite reason: to find the **lazily**
resolved names the eager pass misses. `Serialize` bodies are the classic case — they are the
last methods to take their lazy resolve
(`reference_serialize_lazy_resolve_and_wrong_slot.md`), which is exactly why the static census
lists 319 unported where one boot only surfaces 260.

## What the residue census found on its first run

Comparing 260 runtime misses against the census's static list found **three defects in the
census, in both directions**. All three are fixed in `f95c1af`.

1. **Undercount, 51 functions.** `dx11/` resolves through its own
   `Dx11ResolveRuntimeImport(mangled, iat)` and passes the name from a table literal, not at
   the call site, so `RE_EXPORT` saw none of it. All 60 dx11-only names were then checked
   against the shipped export directory: **0 phantoms**.
2. **Overcount, 6 functions.** A `RegisterRuntimeExport` inside an `if (<flag>)` block that
   is off by default was counted as a live port; `GATED_OFF_FILES` only knew whole files.
   All six are WAVE3.md's NO-GO handle-table tier in `runtime/src/object/internal.cpp`, and
   the residue census caught four of them *arriving at the DLL* while the census called them
   ported. Object goes 28 → 22 — **which is what WAVE3.md said all along.** The wave doc was
   right and the tool was wrong, for a full wave.
3. **18 registrations passing a `const char* const kX = "?Mangled…"` name** are now resolved
   rather than warned about. Naming the mangling once, and using it at both the
   `RT_ORIGINAL_ONCE` site and the registration, is good practice — the oracle and the
   registration cannot then drift — so the census resolves it. UI 98 → 101.

Net: **the honest census is 823 / 504 / 61.2%**, not the 772 / 507 / 65.7% recorded at the
Wave 8 boundary. The denominator grew and the numerator shrank, and both moves were the tool
being wrong rather than the port changing.

This is the same lesson as `reference_uncalled_port_is_not_verified.md` and
`reference_census_cannot_see_nonliteral_registrations.md`, arriving for the third time:
**every progress number this project has published came from a tool that had not itself been
checked against the process.** The residue census is the first thing in the tree that asks
the process.

## Proposed lane order

The order is forced by §2 (verify while the oracle exists) and §3 (the heap cannot be half
ours), not chosen.

| Lane | Scope | Gate |
|---|---|---|
| **0. Residue matrix** | Run `BANISHED_RT_STRICT=1` across the whole harness matrix — autostart, menu, mods menu, all 4 tutorials, `Save9.sav` autoload, `sa-stress`, and an **attended** run with real keyboard and mouse (the only way to reach `Input::Device`, per the Wave 8 holdback). Union the residue lists. | A single work list that is what the process reaches, not what the tree mentions |
| **A. Reclaim the holdbacks** | Port + diff-verify, against the still-present DLL, everything held back in waves 3–8 that lane 0 shows is reached: 32 from Wave 8, 5 handle-table mutators, 4 `ExternalFactory` mutators, `Renderer::Draw`, `MainLoop::OnVideoError`/`OnVideoRestart`. | 0 divergences, non-zero `calls=` on each |
| **B. The 145 named functions + 51 ctors/dtors** | The ordinary port wave, class-atomic (§4.1 rule 5). | 0 divergences |
| **C. Data: 28 vtables, 17 singletons, 14 `BaseTypeInfo`** | Mint them. Each vtable's slots must all point at ported functions, so this lane *follows* B per class and cannot lead it. | `static_assert` on every offset; a shadow oracle comparing our minted vtable against the DLL's exported one, byte for byte, while both exist |
| **D. The 57 unnamed internals** | **The known-hard one.** PLAN §4.3 risk 4 records the `FUN_` pool as MEASURED AND UNSOLVED and CLOSED TO AUTOMATION: all three levers (hash/fuzzy, string anchors, call-graph) run to a fixed point at 2% touched, 9 names proposed, none verified, and no PDB exists. 57 is a far smaller target than the pool, and each has a call site that says what it is used for — that is the lever, not the name. | Each one either ported, or proved unreachable by lane 0 |
| **E. Allocator** | Arm `BANISHED_RT_ALLOC`, revisit the NO-GO, and take the heap. Includes `Engine::Startup`'s three direct field writes and `Shutdown`. | `sa-stress` clean under `BANISHED_REDZONE=watch` |
| **F. Retire the §2.2 patches** | The four byte-patch files, the crash handler's Runtime instruction offsets (§2.2 C, kept and narrowed by Wave 3), the `MessageBoxW` IAT swap, and the direct layout reads in §2.2 B. | Each one deleted, not carried |
| **G. Remove `LoadLibrary`** | The actual cutover. Delete the DLL from the game directory and boot. | The PLAN §5 box |
| **H. Perf gate** | The deferred +7.7% breach. The user deferred it at the Wave 4 boundary explicitly *to this stage*. With the DLL gone the seam's `GetProcAddress`/registry indirection can be compiled out entirely, which is the main reason it was deferred here. Three runs minimum — `reference_one_perf_run_cannot_resolve_the_gate.md`. | ≤5% over the recorded baseline |

## Open items inherited from earlier waves

- **~22 of Wave 8's 49 ports produced no diff site.** All of `Input::Device` and most of
  `Input::KeyMap`. The unattended harness injects dialog clicks by calling handlers directly
  and never passes through the input layer, so `calls=0` there means "the harness did not
  ask", not "verified". Lane 0's attended run is the only fix.
- **8 ports have no caller in our tree** (`??0EdgeMeshInstance`, `??0Quaternion`,
  `??DQuaternion`, `Transform::FromDirection`, `Vector2::Normalize`, three
  `Serializer::Serialize` overloads). Each needs a verdict, not a shrug — a wrong
  `Quaternion::operator*` survived all of Wave 1 in exactly this state.
- **The Wave 1 1-ULP residual is closed as a cause but not as a number.** Wave 8's
  `BANISHED_RT_LIBM=dll` run drives the whole tree to 0 divergences, proving it is libm
  rounding. At cutover there is no DLL libm to fall back to, so the wave must decide whether
  to ship MinGW's rounding or carry the fix.

## Sizing

Not S. Waves 6 (113 fns) and 7 (101 fns) were sized L and XL. Wave 9 is **319 items across
17 namespaces, including the four things every previous wave was allowed to defer** — the
allocator, the `FUN_` pool, the data exports and the holdbacks — **with the diff oracle
switched off partway through.** Call it XL with a hard ordering constraint, and expect lane D
to be the one that decides the schedule.
