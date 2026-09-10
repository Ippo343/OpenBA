# WAVE 18 — LANE GATE: cutover boot attempt 25

**Worktree:** `C:/code/GIT/wt-w18-gate`
**Branch:** `wave18/gate`, cut at `3b4adb0` ("Merge Wave 18 lane GFXIF")
**Subject:** one controlled cutover boot, per
[WAVE9_CUTOVER_BOOT_RUNBOOK.md](WAVE9_CUTOVER_BOOT_RUNBOOK.md). **Zero rows ported.**
**Single variable protected:** lanes VRES and VDATA were NOT merged. `wave10/base`
carried GFXIF's nine rows and nothing else from this wave.

Evidence: `wave18_bootgate/attempt25_{stderr,crash,debug,boot_state}.txt`, and the
exact procedure that produced them, `wave18_bootgate/attempt25_procedure.sh`.

---

## 1. Headline — the wall MOVED

**It moved off `DeviceData::EnumerateInterfaces` entirely, and off the `Video`
namespace entirely.**

| | attempt 24 (`7fd8a43`, 2026-09-09) | **attempt 25 (`3b4adb0`, 2026-09-10)** |
|---|---|---|
| wall | `DeviceData::EnumerateInterfaces` forwarding to an absent original | **`?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z`** |
| fault | EXEC of 0 | EXEC of 0 |
| faulting frame | (DX11 seam) | **`RtEngine_Engine_Startup+0x777`** |
| call site | `dx11/funcs/dx11_seam.cpp` slot 1 | **`runtime/src/engine/engine_lifecycle.cpp:710`** |
| retail address of the missing body | — | **`0x1800a1670`** (ord 3329, already recorded at `runtime/types/engine/Engine.h:148`) |
| residue | 17 symbols | **7 symbols** |
| exit code | 3 | 3 |

**Distance travelled**, on the runbook's own ladder, and it is two *named*
`Engine::Startup` steps further:

* `?EnumerateVideoModes@Device@Video@@` — `engine_lifecycle.cpp:678`, the call that
  drives the DX11 `EnumerateInterfaces` attempt 24 died inside — **now completes.**
* `g_startupFailAt = 9`, the `Input::Configuration` ctor + `Startup` — **now completes.**
* `engine_lifecycle.cpp:710`, the launcher dialog (`g_startupFailAt = 10`) — **new wall.**

Ordering is read from the source, not inferred: `EnumerateVideoModes` is line 678
and `PlatformConfigure` is line 710 of the same function, so the new wall is
strictly downstream of the old one. Retail enumerates adapters and modes *before*
opening the launcher dialog, which is what the dialog populates its resolution
list from.

Boot did **not** reach `[BootState] RUNNING`; `attempt25_boot_state.txt` reads
`STATE=BOOTING`.

### The measurement is informative — validity gates checked before any conclusion

The runbook lists four ways an attempt is a failed *measurement* rather than a
failed boot. All four are excluded, from the run's own output:

| gate | required | attempt 25 |
|---|---|---|
| the `BANISHED_RT_NO_DLL` prerequisite is in the deployed build | continues past `main.cpp` | `[Standalone] BANISHED_RT_NO_DLL=1 -- continuing with Runtime-x64.dll ABSENT` |
| the mint actually registered | an `[RtMint] registered N` line | `[RtMint] registered 196 by name, 202 by address` |
| the DLL was genuinely absent | shadow says ABSENT | `[RtMintShadow] ==== phase=init  DLL=ABSENT (post-cutover; oracle gone) ====` |
| the allocator was armed | not a `MemAlloc` crash | `[RtAlloc] ARMED (cutover: Runtime-x64.dll absent) -- 27 exports registered` |

Binary provenance: built in this worktree at `Sep 10 2026 13:16:39` (the exe's own
banner), deployed `2026-09-10 13:18:47`, 31,704,602 bytes, under
`game_dir_lock.sh run gate` with `assert-held gate` passing as its own statement
before the copy. The DLL was renamed in place and **restored** — verified present
in the run log.

---

## 2. The set diff against attempt 24's 17 — three explicit lists

`17 − 11 + 1 = 7.` Arithmetic checked.

### GONE (11)

| symbol | why it is gone |
|---|---|
| `??0Adapter@Device@Video@@QEAA@XZ` | ported by GFXIF |
| `??0Adapter@Device@Video@@QEAA@AEBU012@@Z` | ported by GFXIF |
| `??1Adapter@Device@Video@@QEAA@XZ` | ported by GFXIF |
| `??0GraphicsInterface@Device@Video@@QEAA@XZ` | ported by GFXIF |
| `??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z` | ported by GFXIF |
| `??1GraphicsInterface@Device@Video@@QEAA@XZ` | ported by GFXIF |
| `?FindOrAddMode@Adapter@Device@Video@@…` | ported by GFXIF |
| `?GetDisplayMode@Device@Video@@…` | ported by GFXIF |
| `?AdjustDisplay@Device@Video@@QEAAXXZ` | ported by GFXIF |
| `?SaveText@External@Object@@…` | registered at `runtime/src/object/external_savetext_win.cpp:399` (pre-existing, Wave 11 follow-up 2) |
| `?FindOrLoad@ExternalFactory@Object@@…` | registered at `runtime/src/object/external_compiled.cpp:647` (pre-existing, Wave 11 follow-up 2) |

**Nine of the eleven are exactly GFXIF's nine rows.** The two `Object` rows were
already closed before this wave and are the movement the brief predicted would
show up here.

I did not take "gone from residue" as "satisfied" for the two `Object` rows,
because it does not follow — a symbol also leaves the residue when the *door that
asked* is removed. Both were checked against a registration in source. See §5.1
for the grep that nearly got this wrong.

### STILL THERE (6)

    ?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ
    ?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z
    ?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z
    ?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
    ?LoadResources@UpdateLoop@Engine@@QEAAXXZ
    ?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z

**None of these six is a wall, and the residue cannot tell you that.** Every one
was symbolized to its asker, and every asker is an *init-time* resolver, not a
call site:

| symbol | asker (symbolized against this exe) |
|---|---|
| `?LoadResources@UpdateLoop@Engine@@` | `EngineFuncsInit()+0x43B` |
| `?Update@Scene@Audio@@` | `EngineFuncsInit()+0x79F` |
| `?Draw@Renderer@Graphics@@` | `EngineFuncsInit()+0x4682` |
| `?BuildParameters@MaterialInstance@Graphics@@` | `RegisterUnregLane3()+0x1A3` |
| `?Serialize@MeshInstance@Graphics@@` | `RegisterDispatchSlots()+0x91F` |

They are eagerly-resolved function-pointer tables and exe dispatch slots that
came back null and were **stored**, not called. They become walls only when
something calls through them, which this boot never got far enough to do. This is
the same shape as the project's standing warning that a **resolve door count is
not a call count**.

### NEW ENTRANTS (1)

    ?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z

Genuinely absent: one door in the whole tree
(`runtime/src/engine/engine_lifecycle.cpp:710`), no `RegisterRuntime*`, no mint
entry.

**Wave 10's precedent is the right reading and it did not repeat here.** Wave 10
saw 17 → 25 and correctly called the growth progress because the entrants were the
closed body's own callees. Attempt 25 produced exactly **one** entrant and it is
*not* a callee of anything GFXIF closed — it is the next thing `Engine::Startup`
asks for. Both are progress; they are different shapes of it, and only the set
diff distinguishes them.

---

## 3. GFXIF's pre-registered prediction, scored in two separate halves

### Half 1 — "the wall MOVES OFF `DeviceData::EnumerateInterfaces`" — **CORRECT**

Measured directly, with a positive control, on the counter GFXIF correctly said
its own A/B runs could not supply.

| line, emitted by unchanged code | attempt 24 | attempt 25 |
|---|---|---|
| `[Dx11] FAILED to resolve Runtime import` | 9 | **0** |
| `[Dx11] devicedata import UNRESOLVED` | 9 | **0** |
| `[Dx11] devicedata Imports field #N is null` | 9 | **0** |
| `[Dx11Seam] FORWARD TO ORIGINAL: DeviceData slot 1` | 1 | **0** |

**Positive control for a screen whose whole result is a zero.** A zero from an
instrument is worth nothing until the instrument is shown to be live, so:
`git diff 7fd8a43 3b4adb0 -- dx11/src/devicedata.cpp dx11/src/dx11_backend_access.cpp
dx11/funcs/dx11_seam.cpp` is **empty**. The printers are byte-identical between
attempt 24's build and mine. The same unchanged code emitted 28 lines then and 0
now. The zero is a measurement, not an absence of instrumentation.

This is precisely the measurement GFXIF flagged as structurally impossible for it
to make (§4, "What this run CANNOT say"), and it was right to refuse it: with the
DLL present, `ResolveExport` falls through and the table fills either way. With
the DLL absent it cannot, and it did.

### Half 2 — "it does NOT follow that the boot completes" — **CORRECT**

The boot did not complete. It died two `Engine::Startup` steps later, at exit 3.

**The reasoning offered for half 2 is nevertheless refuted in the specific, and it
matters that the two are scored apart.** GFXIF's stated reason was that
"`EnumerateInterfaces` is a long body and the next null is as likely *inside* it
as after it." Measured: the next null is neither inside `EnumerateInterfaces` nor
anywhere in `Video`. `EnumerateInterfaces` ran to completion on our nine bodies,
`EnumerateVideoModes` returned, `Input::Configuration` started, and the fault came
from `Engine::Startup` itself.

A right prediction reached through a mechanism that did not occur is a right
prediction — and separating the claim from its reasoning is the only way to see
that the reasoning was not what carried it.

---

## 4. Three bounded screens, kept separate, partitioned by class family

**Carrying attempt 24's own warning: a cutover boot is a GATE, not a PLANNER.**
Screen 2 alone would name 7 symbols and 6 of them are not even walls.

### Screen 1 — PREDICTED WALLS (upper bound): **92**

`tools/unsatisfied_export_scan.py` — every name resolved through a port-aware
door in `code/ engine/ runtime/ standalone/ dx11/` that has **neither a port nor a
mint entry**. It answers "what would ANY run ask for", not "what did this run ask
for".

Raw output: 974 distinct names resolved through a door, **114** unsatisfied, 111
with at least one site outside a diff guard. **19 of those 111 are false
positives** — see §5.2 — leaving **92**.

| family | rows | family | rows |
|---|---|---|---|
| **IO** | **25** | Audio | 5 |
| **Graphics** | **22** | Engine | 3 |
| System | 9 | Input | 2 |
| UI | 9 | Object | 2 |
| Achievement | 8 | Picking | 1 |
| Video | 6 | | |
| | | **TOTAL** | **92** |

Declared blind spots: the scan's own, measured at **2** resolves reached through a
non-literal argument; plus the satisfaction-side one this lane found and sized at
19 (§5.2). It does **not** establish reachability — a row here is a door that
*could* fault, not one that will.

### Screen 2 — RESIDUE (lower bound): **7**, of which **1 is a wall**

The `[RtUnresolved]` set from attempt 25. It can only name what the process
reached before faulting. Positive control: it printed 7 non-zero entries, so the
recorder was live; a zero here would have been uninterpretable.

| family | rows | of which walls |
|---|---|---|
| Graphics | 4 | 0 |
| Engine | 2 | **1** (`PlatformConfigure`) |
| Audio | 1 | 0 |

### Screen 3 — CENSUS GAP (lossy in BOTH directions): **217**

`tools/runtime_census.py` at `3b4adb0`, `called − ported`. TOTAL **1164 called /
947 ported = 217**.

| family | called | ported | gap | ceiling | gap net of unpromotable |
|---|---|---|---|---|---|
| internal | 159 | 125 | 34 | — | — |
| **IO** | 120 | 83 | **37** | 97.5% | **34** |
| **Graphics** | 99 | 66 | **33** | 96.0% | **29** |
| Video | 150 | 118 | 32 | 88.0% | **14** (= VRES 7 + VDATA 7) |
| System | 100 | 77 | 23 | — | — |
| Achievement | 52 | 36 | 16 | — | — |
| Object | 61 | 51 | 10 | 83.6% | **0 — AT CEILING** |
| Engine | 33 | 23 | 10 | — | — |
| Audio | 30 | 23 | 7 | — | — |
| UI | 191 | 186 | 5 | 99.0% | 3 |
| Input | 29 | 25 | 4 | — | — |
| ordinal | 6 | 2 | 4 | — | — |
| Save | 30 | 29 | 1 | 96.7% | — |
| Application | 16 | 15 | 1 | 93.8% | **0 — AT CEILING** |
| Math / Volume / Picking / Utility | 307 | 307 | 0 | 100% | 0 |

**Reported as the identity, not a percentage**, per §5 of the kickoff as corrected
by VDATA: `ported >= called − unpromotable`. Video is **118 >= 150 − 18 = 132**,
i.e. **NOT yet at ceiling by 14** — which is exactly lanes VRES and VDATA, still
unmerged by design. No percentage delta is quoted anywhere in this document,
because `called` sits in both halves of the ceiling and moves whenever any lane
opens a door.

### The arithmetic ACROSS the three, checked

1. **Screen 2 ⊆ Screen 1.** All 7 residue symbols appear in screen 1's set;
   `comm -23` returns empty. This could have failed, and a failure would have
   indicted one of the two screens rather than revealing work.
2. **Screen 1 ∩ Screen 3 = 69. Screen 1 \ Screen 3 = 23.** So the census gap is
   lossy in the direction the kickoff predicted *and* in the other one: 23 names
   our tree resolves through an unsatisfiable name door are **not** in the
   census's called-but-unported list at all. Spot-checked mechanism: several are
   exe IAT dispatch slots filled with `ResolveExport(<runtime name>)` at
   `standalone/address_registry.cpp:1346-1360` (`?SetMesh@MeshInstance@`,
   `?SetMaterialInstance@MeshInstance@`, `?GetMaterialList@GraphicsMesh@`). At
   cutover each of those slots is filled with **null and stored**, which is a real
   cutover hole the census does not report. I have not established the cause for
   all 23 and do not claim it.
3. **The bounds order correctly**: 7 (reached) ≤ 92 (askable) ≤ 217 (census gap),
   with the caveat from (2) that the third is not a superset of the second.

### What the three agree on, from independent instruments

Screen 1 corrected ranks **IO 25 > Graphics 22 > … > Video 6**. Screen 3 net of
unpromotable ranks **IO 34 > Graphics 29 > Video 14**. Two instruments measuring
different things — unsatisfiable cutover doors versus census gap — produce the
same ordering. That independently corroborates the kickoff §0 steer: **do not
follow this wave with more Video.** And the new wall being an `Engine` row rather
than a `Video` one is a third, independent vote for the same conclusion.

---

## 5. What this measurement CONTRADICTS or corrects

### 5.1 A one-line grep for a registration reproduces the census's own blind spot — and I committed it

Checking whether `?FindOrLoad@ExternalFactory@Object@@` left the residue because
it was *satisfied* or because its *door was removed*, I ran
`grep -rn "<mangled>" | grep -iE "Register"` and got **nothing**, and was one step
from recording it as "gone but unsatisfied" — which would have been a fabricated
finding in the set diff.

It is registered. `runtime/src/object/external_compiled.cpp:647`:

    RegisterRuntimeExport(
        "?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z",
        (void*)&RtObject_ExternalFactory_FindOrLoad); ++n;

The call is split across lines, so the name and the word `RegisterRuntimeExport`
are never on the same line and no single-line grep can pair them. This is
`reference_census_three_blind_spots.md`'s "multi-line literals" item, and the file
containing it carries a comment four lines above warning that the census
"cannot see through a named constant". The warning was in the file I was grepping.

**The rule that would have saved it**: a grep proving a registration ABSENT must
be multi-line, or it is not evidence. The tell that the answer was wrong was
external — the scan and the residue both said satisfied, and the grep disagreed
with both.

### 5.2 `unsatisfied_export_scan.py` has an UNDECLARED blind spot on the SATISFACTION side, and it inflates its own headline by 19

The scan declares one blind spot and sizes it honestly: 2 resolves reached through
a non-literal argument. That declared blind spot is on the **resolve** side — what
counts as a door.

It has a second one, on the **satisfaction** side, and it declares nothing: its
"has a port" test cannot see a `RegisterRuntimeExport` whose first argument is a
**named constant**. `runtime/src/ui/widgets_input.cpp` registers a whole table
that way:

    const char* const kSpinnerIsActive = "?IsActive@SpinnerElement@UI@@QEBA_NXZ";
    ...
    RegisterRuntimeExport(kSpinnerIsActive, (void*)&RtUi_SpinnerElement_IsActive);

**Measured, not estimated: 24 mangled names in the tree are registered through a
named constant, and 19 of them are in the scan's 111.** All 19 are `UI`. So the
scan's headline of 111 is really **92**, and `UI`'s share falls from 28 to 9.

**How it was caught, and it was not by suspicion of the tool.** The partition by
family produced `UI 28` — the largest family in the screen — for a namespace the
project has declared FINISHED at 186/191 with a census gap of **5**. Two numbers
about the same namespace that could not both be right is what forced the check;
neither number was doubted on its own. After correction the two read 9 and 5,
which is a residual worth someone's time and not an alarm.

This is the wave's **fifth** instance of the class §6.13 of the kickoff names — a
screen that cannot accurately state its own subject. It is also, precisely, the
lesson §6.5 drew from `abi_retslot_audit.py`: *"a tool that cannot name its own
subject invites a lane to cite its green for something it never looked at."* The
difference is that this one inflates rather than deflates, so it manufactures work
instead of hiding it. **Recommended (not done by this lane — it is tool work, not
gate work): make the satisfaction test resolve `const char* const` identifiers,
and make it print its blind spot on BOTH sides rather than one.**

### 5.3 The crash log's `VCALL-of-0` block is decoration here, and reading it would have misled

`attempt25_crash.txt` prints a full `VCALL-of-0: this=… vtable=…` block with
thirteen slot values. **There is no virtual call involved.** `RCX` at the fault
was `0x000000A2EC9FF440` — a *stack* address, the `self` argument of a direct
call — and the "vtable" it dutifully dereferenced is unrelated heap
(`vtable in UNKNOWN module (heap? bad ptr?)`, first three entries `…c30000`,
`…c30000`, `0xda000`, i.e. allocator bookkeeping).

`VCALL-of-0` is a heuristic on RCX and cannot express "this was a direct call
through a null resolve", which is what happened. The field that *did* carry the
answer is the `[RtResolve] MISS "?PlatformConfigure@…" -> NULL` line printed
immediately before the fault, plus the symbolized return address. Applying the
project's own test — *could this field express the answer if the answer were
different?* — `VCALL-of-0` fails it, exactly as
`reference_diagnostic_field_cannot_express_the_answer.md` records.

Same test applied to `asked-by`: every `[RtUnresolved] … (asked by <addr>)` in
attempt 25 symbolizes to `RtResolveCached+0x34` or to an `…Init()` /
`Register…()` frame, i.e. **the resolver and the registration pass, never the
consumer.** It is useless for attribution and I did not use it for that; §2's
asker table uses it only for the thing it *can* say — that the ask happened at
init time rather than from a call.

### 5.4 Do not symbolize attempt 24's stack against this binary

Tempting and wrong. Attempt 24's exe is a different build (31,182,702 bytes vs
31,704,602). Resolving its frame `+0x3E481D` against my `nm` output yields
`RtVideoDeviceEnumTypesProbeMaybeRun` — a symbol that **did not exist** in the
Wave 15 build, since GFXIF wrote that probe this wave. A confident, plausible,
entirely fabricated attribution. Attempt 24's wall rests on its own stderr, which
names `DeviceData::EnumerateInterfaces` in plain text.

### 5.5 The runbook's PREREQUISITE section is stale in a harmless direction

`WAVE9_CUTOVER_BOOT_RUNBOOK.md` still presents the `BANISHED_RT_NO_DLL` allowance
as work to be done ("Until that lands, do not run this procedure"). It landed —
`standalone/resolve_standalone.cpp:535` — and attempt 24 already depended on it. A
reader following the runbook top-to-bottom today is told to stop before starting.
Worth one edit by whoever next touches that file; recorded rather than fixed
because this lane changes no code.

---

## 6. What to do with this, stated as a bound and not as a work list

The next wall is **one function**: `?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z`,
retail `0x1800a1670`, the Win32 launcher dialog. One door, no port, no mint.

Two things a future brief should not misread:

* **It is not necessarily cheap.** It is a Win32 dialog procedure, not a value
  type, and CLAUDE.md's "Launcher Dialog" section plus
  `HarnessGuardUnattendedBoot` both depend on its behaviour. It is one *row*, not
  one *hour*.
* **Closing it advances the boot by an unknown distance.** Attempt 25 is the
  second data point in a row where the residue's other members turned out not to
  be walls at all. The only way to know what is behind `PlatformConfigure` is
  attempt 26.

And the standing steer survives contact with the measurement: **IO and Graphics,
not Video.** Screen 1 and screen 3 agree on that ordering from independent inputs,
and the new wall is an `Engine` row.

---

## 7. Provenance

* **Worktree:** `C:/code/GIT/wt-w18-gate`
* **Branch:** `wave18/gate`, cut at `3b4adb0`
* **Final commit:** see the lane's report message
* **Rows ported:** 0. **Code changed:** none.
* **Game directory:** held as `gate` via `game_dir_lock.sh run` (self-releasing),
  `assert-held gate` passing as its own statement before the deploy. Queued ~30s
  behind lane `vres`. `Runtime-x64.dll` renamed in place and restored; verified
  present.
