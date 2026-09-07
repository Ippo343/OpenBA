# Wave 3 — Object

Landed 2026-09-06 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2.

Wave 1 was pure maths and Wave 2 was foundation value types. Wave 3 is the first wave whose
classes **own state the DLL also drives**: the reflection registry, the internal handle table and
the external resource table. It is also the wave PLAN §4.3 risk 3 warned about for a specific
reason — "until Wave 3 lands, objects built by retail ctors carry DLL vtables" — and the first to
inherit an explicit debt from its predecessor (WAVE2.md's breached perf gate, marked "owned by
Wave 3, not silently accepted").

## Result

| | |
|---|---|
| Ported | **22 of the 41 Object functions our tree calls** — type system 6, handle table 5, external table 11 |
| Census | `Object 21/41 (51.2%)`; 877 called, 170 ported (**19.4%**) overall, up from 139 / 15.9% |
| Diff oracle | **38,613,179 calls across 59 sites, 0 new divergences** |
| Residual | 138 divergences in 2 sites — the same Wave 1 sin/cos-fed sites, all 1 ULP, unchanged in kind |
| Shadow oracle | `Internal::Internal` / `~Internal` **93,387 calls each, 0 divergences**, ctor and dtor counts identical |
| Perf gate | **PASS at +4.4% / +3.3% / +3.3%** over three runs — the Wave 2 breach is CLOSED |
| Held back | 4 mutators NO-GO (`FindOrLoad`, `Clean`, `SerializeReferenceTable`, `SaveText`), 5 more opt-in behind `BANISHED_RT_INTERNAL_TABLE` |
| Blocked | 4 functions unprovable this wave — the save/load harness stalls, **pre-existing** |

Pre-wave baseline, same harness, Wave 2 code: 45 sites, ~22.7M calls. The wave added 14 sites and
~16M calls and introduced no new divergence.

## What landed

| Piece | Files |
|---|---|
| Wave entry point | `runtime/src/object/rt_object_port.cpp` — one `RtObjectPortInit()`, three per-lane `Register` functions, written BEFORE the lanes started so no two ever edited the same file |
| Type system (lane A) | `runtime/types/object/TypeInfo.h`, `runtime/src/object/type_system.cpp` |
| Handle table (lane B) | `runtime/types/object/Internal.h`, `runtime/src/object/internal.cpp` |
| External table (lane C) | `runtime/types/object/External.h`, `runtime/src/object/external.cpp` |
| Resolve-once helper | `RT_ORIGINAL_ONCE` in `runtime/funcs/rt_port.h` |
| Perf gate line | `tools/stress_report.py` now prints `PLAN 5% gate (sim ms/iter): +N%  PASS/FAIL` |
| Lane detail | [WAVE3_LANE_A.md](WAVE3_LANE_A.md) · [WAVE3_LANE_B.md](WAVE3_LANE_B.md) · [WAVE3_LANE_C.md](WAVE3_LANE_C.md) · [WAVE3_LANE_P.md](WAVE3_LANE_P.md) |

## Environment variables added

| Variable | Effect |
|---|---|
| `BANISHED_RT_OBJECT=0` | Leave the whole Object namespace on the DLL |
| `BANISHED_RT_TYPESYS=0` / `_INTERNAL=0` / `_EXTERNAL=0` | Per-lane kill switches |
| `BANISHED_RT_INTERNAL_SHADOW=1` | Observe-only oracle for the handle-table mutators — predicts the post-state, lets the DLL do the real work, asserts the prediction. Our code never writes the table |
| `BANISHED_RT_INTERNAL_TABLE=1` | **Opt-in, NO-GO.** Arms the four table mutators for real. Mutually exclusive with SHADOW; the code refuses both |
| `BANISHED_RT_INTERNAL_CHECKPTR=1` | Arms the ported `CheckPtr`. Off for a measured reason — see finding 3 |

## The findings that change other waves

### 1. The perf-gate breach was ONE defect, not the cost of the seam.

WAVE2.md attributed its +7.8% to "the registry indirection on hot leaf functions… the price of the
seam, paid on the hottest function in the engine", and left it open for this wave. Both halves of
that were wrong.

**There is no per-call registry indirection.** `resolve_standalone.cpp` swaps the pointer once
inside `ResolveAddr`/`ResolveExport` and callers cache it in `fn_*` statics. And `Random::Rand`'s
16M calls are worldgen — 2 samples out of 5999 in the sim.

What actually cost 5.6 percentage points: **`RtOriginal` is `GetProcAddress`**, and two calls to it
sat *inside the body* of `System::Time`'s ctor and `AsSeconds` — an export-table binary search by
string, under the loader lock, on the value type every `Update(const Time&)` passes. It was
invisible to both profilers because it lands in ntdll self time, not in the caller's.

Caching those two pointers took the Wave 2 boundary from **+7.8% to +2.2%**. `RT_ORIGINAL_ONCE`
now exists so the obvious thing is the correct thing, and `RtOriginal`'s comment says in capitals
that it is not a cheap accessor. Bare `RtOriginal` is correct only inside an `RT_DIFF_*` guard,
which no shipping run evaluates.

### 2. WAVE2.md's perf attribution was backwards, because it rested on one run per config.

| | WAVE2.md said | Measured, 3-5 runs per config |
|---|---|---|
| predates the port | ~4.5 points | **+1.3** |
| Wave 1 | (never measured — `sa-stress` was never run) | **+1.0** |
| Wave 2 | ~2.5 points | **+4.8** |

The wave owned nearly all of it, not a third of it. One run cannot resolve these effects, and that
is exactly how the wrong split stood for a day. **Rule for every later wave: 3+ runs per config,
and take the free `BANISHED_RT_PORT=0` A/B at each boundary** — neither Wave 1 nor Wave 2 did.

### 3. `CheckPtr` and `AssignId` are bare `RET` in the PROFILE build too.

Not a debug-build leftover, which is what `engine_funcs.h` claimed: they are `RET 0` at
`0x18008c5d0` / `0x18008c5e0` in the ToolKit profile build as well, so they were compiled out at
source level in **every shipped configuration**. No build of this engine has ever validated an
`InternalBasePtr`.

The port therefore deliberately does **not** register `CheckPtr`. `engine_funcs.cpp` sniffs the
first body byte and sets `g_checkPtrIsNop` so direct call sites skip the indirect call entirely;
registering a GCC-compiled body would silently switch ~60k calls/frame back on at 800x10 — and in
the perf build it would never execute at all while the census counted it as ported.

### 4. PLAN §4.3 risk 3 does not bind these classes, for a reason that does not generalise.

`??_7Internal@Object@@6B@` (`0x180154720`) and `??_7Base@Object@@6B@` (`0x180154788`) are exported
data symbols, so the ported ctor stamps **the DLL's own vtable by name**. We never own a vtable and
objects our ctor builds are byte-identical to the DLL's. That is why the handle table could be
ported without the two-vtables hazard.

Do not transfer this. It is a per-class argument, and lanes A and C had to make it separately.

### 5. `FindOrLoad` does not re-parse. The mechanism this project has been citing is wrong.

`reference_createplaced_placebitarray_zeroed_by_reparse.md` and several source comments say
"Serialize re-runs on every `FindOrLoad`". `FindOrLoad` does **not** reload a cached resource. The
effect lives in `ExternalFactory::Add` (`0x1800be570`), which ORs `1 << _cleanBitIndex` into the
entry's usage mask and re-invokes vtable slot `+0x28` against an **`IO::VisitorStream`** — a
reference-VISIT pass, not a re-read of the file — and only when that bit was not already set, i.e.
once per resource per clean generation.

**Open, and deliberately not resolved:** `builddescription.cpp` records a measurement of six
`FindOrLoad` passes on the Pasture's `:build` sub-resource with five clobbering re-passes, and the
mechanism above allows at most one per generation. A measurement outranks a decompile reading, so
the comment now states the mechanism precisely, preserves the 6x count verbatim, and says neither
has been re-measured against the other. Instrument `Add` and log `(_cleanBitIndex, usageMask
before/after)` per call to settle it.

### 6. `BaseTypeInfo+0x18` is the file extension, not "abstract".

Pinned by the `_stricmp` in `Factory::GetTypeIdForFileExtension` (`0x1800c04c0`). Three
hand-declared copies in our tree had every **offset** right and this **name** wrong, and the one
call site in the whole tree that passes a non-null value settles it independently:
`REGISTER_TYPE_FULL(NameDatabase, "Object::External", "ndb", …)`. Renamed through
`engine_funcs.h`, `factory_registration.{h,cpp}` and the bulk macros.

### 7. PLAN §2.3 and §2.2 hazard C were both wrong, both in the flattering direction.

* §2.3's `0x1801fc6e0` is `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` — owned by
  **BaseTypeInfo**, not Factory — and it, `?_singleton@Factory@Object@@0PEAV12@EA` and
  `?_crcTable@CRC@System@@0PAIA` are all **exported data symbols**. The "17 have no export and need
  an accessor" figure has never been measured and must be re-measured before a wave plans accessors
  around it.
* §2.2 hazard C said "Retire with Wave 3". **Keep, narrowed.** No detour is installed, so the DLL's
  own internal callers still reach `Runtime+0xC0390` and the crash handler still has to recognise
  the fault for them; only our callers moved into the exe. It becomes retirable when the function is
  detoured, or at cutover.

### 8. The `Set` node trap landed inside a single class, twice, exactly as predicted.

`InternalFactory` alone needs **five** distinct `Array` bodies (one per element width x operation),
and `ExternalFactory`'s two trees are `Set<0x18-element>` (node 0x38) and `Set<0x10-element>`
(node 0x30) — two separate ported bodies inside one class. Both match WAVE2_COLLECTION.md's
`align8(sizeof T) + 0x20` formula, now confirmed by three independent consumers. Budget one helper
per (width x operation), not one per container.

### 9. A trap for Wave 4: the Encoding vtable offsets DIFFER between builds.

`InternalBasePtr::Serialize` uses `+0x78`/`+0x98`/`+0xa0` (bool/uint/int) in **retail** and
`+0x90`/`+0xb0`/`+0xb8` in the **profile** build. PLAN §6 step 1 says the profile build's NAMES win;
this is the counter-case where its OFFSETS must not. Anyone porting a `Serialize` body from a
profile decompile will write the right values through the wrong slots and desync every save.

## Performance at the wave boundary — the gate PASSES

| Run | `sim ms / iter` | vs baseline |
|---|---|---|
| Wave 2 boundary, as WAVE2.md left it | 0.2074 | +7.8% **FAIL** |
| + Timer resolve cache (finding 1) | 0.1967 | +2.2% PASS |
| **Wave 3, run 1** | 0.20 | **+4.4% PASS** |
| **Wave 3, run 2** | — | **+3.3% PASS** |
| **Wave 3, run 3** | — | **+3.3% PASS** |

`perf/baselines/current.csv` was **not** touched. WAVE2.md's warning — "re-baselining is how a 5%
gate becomes a 0% gate that measures nothing" — was followed; the number moved because a defect was
fixed, not because the ruler was.

A real measurement defect was fixed too: `tools/stress_report.py` compared `sim ms/iter` at a **10%**
regression threshold while PLAN's gate is **5%**, which is why the failing Wave 2 boundary printed
"0 regressions over threshold". It now prints the gate verdict explicitly.

## Defects found

### A. `System::Time`'s two `RtOriginal` calls (PRE-EXISTING, Wave 2, live)

Finding 1. Cost 5.6 points of sim time and was the whole of the gate breach. Fixed with
`RT_ORIGINAL_ONCE`. Two more of the same shape in `string.cpp` (`FormatParamCount`,
`FormatParamList`) fixed at the same time; not separately measured, because `String` is near-inert
in the stress town.

### B. `stress_report.py`'s threshold was looser than the gate it reported on (PRE-EXISTING)

Finding 2's tooling half. The harness passing was never the gate passing.

### C. Two stale comments naming a mangling the DLL does not contain (PRE-EXISTING)

`engine_funcs.h` and `action_base_complete.cpp` both named
`?CheckPtr@InternalBasePtr@Object@@QEAAXXZ`. The real export is `…@IEBAXXZ` (protected AND const);
the old name resolved null and only the `ResolveAddr` fallback carried it — which also means a port
keyed on the old name would have been silently inert. Fixed; the `engine_funcs.h` block's
"debug-build leftover" claim was corrected by finding 3 at the same time.

### D. `ExternalFactory::GetReferenceId` dereferences a not-found node unguarded (FAITHFUL)

`MOV EAX,[RBX+0x14]` with no `TEST` in retail. Ported unguarded per CLAUDE.md. Did not fire in any
run. If it ever does, the bug is upstream: something handed us an `External` the factory never
`Add()`ed. `FindNext` has the same shape one level down.

### E. The `BANISHED_AUTOLOAD` harness stalls at the main menu (PRE-EXISTING, **CLOSED 2026-09-06 by Wave 4 lane H — see [WAVE4_LANE_H.md](WAVE4_LANE_H.md)**)

> **Closed.** The stall was `UI::UserInterface::ShowDialog` (Runtime `0x18011fd10`) being a
> nested `MainLoop::Frame` loop around an unanswerable `modsMissing` prompt — which is why it
> looked like "`MenuDLT::Draw` forever": the process really was drawing menu frames, from
> inside the modal. Two further defects sat behind it (a `String` destructor on an
> `InternalBasePtr` corrupting an allocator tag, and a missing `GameContainer_EnsureLists` on
> the Load path). All four functions below are now exercised on a real `Save9`/`Auto` load
> with 0 divergences; the counts are in WAVE4_LANE_H.md §5. The paragraph that follows is
> kept as written so the old claim stays recognisable.

Injects the Load click, then loops in `MenuDLT::Draw` indefinitely with no world ever created.
Reproduces with `BANISHED_RT_OBJECT=0` and with both `Save9` and the documented `Auto`, so it is
**not** a Wave 3 regression and not save-specific. Consequences, stated plainly:

* PLAN §5's "`Save9.sav` load green at each wave boundary" **could not be evaluated at this
  boundary — and could not have been at the previous one either.**
* Four Wave 3 functions live only on that path and are therefore **unproven**:
  `InternalFactory::ClearPersistState` and `ResetPersistState` (shadow sites never executed), and
  `ExternalFactory::GetReferenceId` and `GetPathForReferenceId` (registered, never called).

They are landed as registered-but-unexercised rather than claimed as verified. Fixing the harness
is the prerequisite for closing them, and is the first thing Wave 4 should do, since Wave 4 is IO
and lives entirely on that path.

## What was deliberately NOT done

* **The four `ExternalFactory`/`External` mutators are not ported at all**, not even as forwarders.
  `FindOrLoad` and `Clean` rewrite the shared resource AVL tree and there is no oracle — running
  both would mutate the one tree twice, and a private replica is precisely what a stateful port must
  not keep. `SerializeReferenceTable` and `SaveText` need `IO::Stream`/`Serializer`/`TextFileStream`
  (Wave 4). A forwarder was considered and declined for all four: it adds a census row for zero
  behaviour change (WAVE2 finding 5).
* **The handle-table mutator tier is opt-in with a NO-GO verdict.** It is written, compiles, and has
  never executed one instruction with `BANISHED_RT_INTERNAL_TABLE` set. Unlike the Wave 2 allocator,
  the oracle that would flip it to GO is written and has now RUN clean for the two hot members
  (93,387 calls each); `ClearPersistState`/`ResetPersistState` remain unexercised for defect E's
  reason. Two residual issues survive even a fully green shadow: `ResetPersistState` carries a
  stated non-byte-faithful deviation (both encoding-factory vtable slots called with
  `(factory, &ser)`, because Ghidra's arity analysis at those two sites is demonstrably wrong), and
  `InternalBasePtr::Serialize` has no oracle at all and lands in the save/load path.
* **Both `_typeInfo` data symbols stay on the DLL.** Registering our own storage would hand every
  call site a second `BaseTypeInfo` that no DLL-built object's type id would ever match.
* **`?_singleton@InternalFactory@Object@@0PEAV12@EA` can never be "ported"** — it is exported DATA,
  and there is nothing to register until we construct the factory at cutover. Do not read it as an
  open gap in a post-wave census diff.
* **No detours were installed**, as in Waves 1 and 2. Registration redirects our call sites only.
  Safe here for the same reason: these ports share the DLL's allocator, its exact layouts and its
  own vtables, so an object is interchangeable whichever side made it.

## Notes for later waves

* **Three runs, not one.** Finding 2 is the whole argument. A single stress run cannot resolve a
  5-point effect, and a wave that reports one is reporting noise with a decimal point on it.
* **`RtOriginal` is `GetProcAddress`.** Use `RT_ORIGINAL_ONCE` anywhere a normal run executes.
  There are 92 `RtOriginal` sites in `runtime/src/`; the ones outside a diff guard are audited in
  WAVE3_LANE_P.md §4.3.
* **The profile build's names win; its offsets do not** (finding 9).
* **Check `calls=`, not `divergences=`.** This wave's first integration run reported 22
  registrations and zero divergences while most sites never executed — because `BANISHED_AUTOQUIT`
  drives menu→**Quit**, not menu→New Game. A green summary over an unexercised port is the failure
  mode Waves 1 and 2 both hit, and it is easy to hit by accident.
* **The `build=` stamp in `report.txt` is a ccache-reused `__DATE__`/`__TIME__`** and does not change
  across real rebuilds. It proves a build FAILED; it does not prove a build CHANGED. Verify by
  disassembling the symbol you edited in the deployed exe.
