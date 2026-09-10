# WAVE 14 — Save::SaveLoad's last seven rows

**Status: ALL SEVEN LANES MERGED, BUILT AND RUN.** Integration branch
`wave14/trial`, base `64d3948`. Integration regression: **24 consecutive saves,
zero divergences on any Wave 14 port.**

Scope was the seven rows the census reported as called-but-not-ported in the
`Save` namespace after Wave 13 closed at 22/29 = 75.9%. Six were function
bodies. The seventh was not — see lane SINGLETON.

## Numbers

| tree | called | ported | % |
|---|---|---|---|
| base `64d3948` | 29 | 22 | 75.9% |
| all 7 lanes merged | 30 | 28 | **93.3%** |

TOTAL across all namespaces: 79.9% → **79.9%**, i.e. it did not move. That is
honest, not disappointing: DRAW's faithful port resolves four NEW retail callees
by name, growing `called` 1133 → 1143, so UI reads 97.4% where it read 98.9%.
Succeeding moves the denominator — the Wave 10B effect, twice in this wave.

**The denominator moved, and not only because we succeeded.** `called` went
29 → 30 because lane SINGLETON's probe names `?Get@SaveLoad@Save@@SAPEAV12@XZ`
as a literal in order to use retail's own RIP-relative read as an oracle. That
is a real dependency and the census is right to count it. It is also a row that
can never be ported — see the wart below.

## Integration test — run by the integrator on the merged tree

Deployed md5 `36a18f23…` verified against the integration build before the run,
so this measures OUR tree and not a lane's leftover.

    BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1
    BANISHED_SAVE_TEST=400 BANISHED_SAVE_TEST_REPEAT=250

* **24 consecutive saves completed, every one in 61 frames** — the same 61 that
  Wave 13's all-DLL control recorded and that lane DRAW measured independently.
* **ZERO divergences on any Wave 14 site.** `Save::SaveLoad::Draw` (4 sites) and
  `Save::SaveLoad::GetAutoSaveName` registered and ran; no `DIVERGENCE` line
  names any of the six ported functions.
* The run's only divergences are the SAME SIX SITES lane DRAW predicted as
  pre-existing: `UI3 pin` and `DLG2 pin` negative controls,
  `UI::DialogInstance::StartTimer[negative control]`, `UI::SpriteSheet::TypeName`,
  `UI::SpriteSheet::Construct`, and `Object::Factory::Startup [probe, not
  diffable]`. DRAW's cross-site control claim is therefore CONFIRMED
  INDEPENDENTLY on the merged tree, by a different run than the one it made the
  claim from.
* Crash log: empty. Evidence: `docs/runtime_port/wave14/integration_run_error.log`.

### A crash that IS real, and is NOT in the ports

Running the same tree with `BANISHED_RT_ORPHAN_PROBE=1` ACCESS_VIOLATIONs in the
allocator:

    Fault: WRITE of 0x648      (RCX=0x640)
    ntdll!…  <-  Runtime-x64.dll+0xC7A8A  <-  reimpl+0x364417
    stack:  RtSystem_String_Format -> RtSystem_String_Set -> RtString_SetLength

It faults immediately after `[RtDiff] site[60] = Save::SaveLoad::GenerateSaveName`,
i.e. inside GENNAME's probe, on the `String::Format` path GenerateSaveName uses.
`null + 0x640` is the signature `rt_resolve_cache.cpp` §5 documents for a missing
`?_singleton@MemoryAllocator`, and the log carries `[REDZONE-DUMP] no allocator
singleton`.

**A/B, run by the integrator:** the SAME binary with the probe DISABLED completes
24 saves with no crash at all. So the fault is in the ORPHAN-PROBE path, not in
any of the seven ported bodies, and not in normal operation. Note also that in
GENNAME's own (orphaned) run the same probe ran as site[538], late, and did not
fault; on the integration tree it runs as site[60], early — so a PROBE-ORDERING
dependency is the leading hypothesis and it is NOT established. Left open.

## What the remaining two rows are — Save's real ceiling

At 28/30 the two rows left are NOT unfinished work. Both are decisions:

    ?_singleton@SaveLoad@Save@@0PEAV12@EA   minted, DELIBERATELY not promoted.
                                            Promotable at cutover; the standing
                                            case and full enumeration are in
                                            reg_mint_promoted.cpp.
    ?Get@SaveLoad@Save@@SAPEAV12@XZ         PERMANENTLY unportable. Dead in all
                                            four shipped binaries AND in the
                                            ToolKit profile build. Present in
                                            `called` only because SINGLETON's
                                            probe names it as a literal to use
                                            retail's own RIP-relative read as an
                                            oracle.

So **Save's true ceiling is 29/30 = 96.7%**, reachable by promoting one data row
at cutover, and 30/30 is not reachable at all. 93.3% is one deliberate deferral
below ceiling — which is a different statement from "two rows to go", and the
distinction is the one Wave 10 had to invent the `ceiling` column to express.

## Per-lane accounting, unsmoothed

| lane | target | SHA | ported | note |
|---|---|---|---|---|
| AUTONAME | `?GetAutoSaveName@SaveLoad@Save@@` 0x1800c1440 | `c2d23d8` | +1 | calls=1, thin but real |
| SETUPLOAD | `?SetupLoad@SavedState@Save@@` 0x1800c0c60 | `f862276` | +1 | two screens, no gameplay route |
| SAVEMETA | `?SaveMetaData@SaveLoad@Save@@` 0x1800c1490 | `3476fc4` | +1 | modded path never ran |
| LOADMETA | `?LoadMetaData@SaveLoad@Save@@` 0x1800c1b30 | `76cf7e4` | +1 | write path never ran; S7 side-effect screen |
| SINGLETON | `?_singleton@SaveLoad@Save@@0PEAV12@EA` | `94d8d15` | **0** | **net −2.6 points alone** |
| DRAW | `?Draw@SaveLoad@Save@@` 0x1800c2eb0 | `63b5e0a` | +1 | 352,256 px bytes byte-identical |
| GENNAME | `?GenerateSaveName@SaveLoad@Save@@` 0x1800c1250 | `f4c07d7` | +1 | **control fired, clean case NEVER RECORDED** |

SINGLETON's branch alone reads 29/22/75.9% → 30/22/73.3%: zero ported, and the
percentage FALLS. The lane disclosed this itself. It kept its oracle and paid
the census row, because retail's own read is the only instrument that can say
where our ctor actually published. A lane that dropped the probe would have
scored better and known less.

## Kill switches, pinned

The integrator gave one of these three spellings over the course of the wave.
THE TREE IS AUTHORITATIVE and these are the names in it:

    BANISHED_RT_SETUPLOAD        SavedState::SetupLoad
    BANISHED_RT_SAVE_METADATA    SaveLoad::SaveMetaData
    BANISHED_RT_LOAD_METADATA    SaveLoad::LoadMetaData
    BANISHED_RT_SAVELOAD_DRAW    SaveLoad::Draw

`RtWaveEnabled` defaults to ON when unset, so a MISSPELLED switch fails safe —
the port stays live — but is silently unusable as an A/B kill switch, which is
the exact independence the rename existed to provide. Flagged by lane LOADMETA.

## A cross-seam hazard that outlives this wave

Lane DRAW's first live run crashed: `READ of 0x1` at `DrawBody+0x19f`. Retail
does `CALL <ctor>; MOV R12,RAX` and is entitled to — MSVC returns `this` in RAX
from every constructor. But `??0ImageBuffer@Video@@QEAA@XZ` is PORTED, and our
`Video_ImageBuffer_Ctor` is declared `void`, so RAX held whatever its last
expression left.

**A byte-faithful transcription of retail is UNSAFE across the port seam at
exactly this construct**, and our ported constructors are not uniform. Surveyed
across `runtime/`:

    VOID-returning ported ctors : 3   <-- unsafe if retail reads RAX
      Video_ImageBuffer_Ctor   [video/buffers.cpp]
      Video_IndexBuffer_Ctor   [video/buffers.cpp]
      Video_VertexBuffer_Ctor  [video/buffers.cpp]
    pointer-returning           : 33

The fix is to use the `operator new` result instead — the same pointer by
construction. `runtime/src/graphics/sunlight.cpp:230-247` ALREADY documents the
identical Wave 9 crash and this fix. DRAW found that note only after paying for
the crash, and named the real lesson itself: it grepped for the SYMBOL and not
for the HAZARD. Evidence: `docs/runtime_port/wave14/draw_ctor_return_crash.txt`.

## The ABI error, and how it was caught

The dispatch brief told three lanes that a by-value return puts the hidden
return slot in the FIRST argument. **That is wrong for this binary**, and it is
the integrator's error, not any lane's.

MSVC's member-function convention is `this` in RCX and the hidden return
pointer in RDX — the OPPOSITE of the GCC/Itanium sret order. We build with
MinGW GCC, so the natural declaration `String f(const SaveLoad*)` puts sret in
RCX and self in RDX: backwards, writing through whatever `self` happened to be.
Lane SETUPLOAD's statement of the consequence is the one to keep: *"Had I
followed the brief, the port would have written 16 bytes through a stale RCX."*

Verified unanimous across 5 call sites spanning 3 functions, e.g. 0x1800c25cc:

    lea  rdx,[rsp+0x28]     ; RETSLOT second
    mov  rcx,rax            ; SELF first
    call 0x1800c0c60

Three lanes caught it independently and by different routes — AUTONAME and
SAVEMETA from the CALL SITES, GENNAME from the CALLEE PROLOGUE
(`R15=RCX=this`, `R14=RDX=ret`).

**The tree already had it right.** `saveload_update.cpp:362` has carried
`NameGetterFn = const char** (*)(const SaveLoad* self, const char** ret)` since
Wave 13, under a comment naming this exact hazard. SAVEMETA was never exposed
because it REUSED Wave 5's existing typedef instead of declaring its own. Reuse
was the defense.

Wave 14 hoisted `SetupLoadFn` out of `saveload_update.cpp`'s anonymous
namespace into `RtSaveDetail` (`saveload_internal.h`) so there is one spelling.
Note the integrator's instruction to "reuse it" was not literally possible
before that hoist — an anonymous namespace is unreachable from another
translation unit.

## The +0x2c signal — attributed, NOT closed

Wave 13 left ~9 unpredicted bytes at `SaveLoad+0x2c` in UpdateList state 4,
writer unknown, worker-thread hypothesis standing.

**Attribution: `?LoadMetaData@SaveLoad@Save@@`'s teardown.**

    1800c1d98  MOV RCX,[R14+0x30]     ; _data
    1800c1d9c  TEST RCX,RCX / JZ
    1800c1da1  CALL 0x1800c7ad0       ; MemFree   <- CONDITIONAL
    1800c1da6  MOV qword ptr [R14+0x30],0        <- UNCONDITIONAL
    1800c1dae  MOV dword ptr [R14+0x2c],0        <- UNCONDITIONAL

`SetupLoad` also writes those bytes but in state 3, which Wave 13 already
covers with an unconditional `Exclude` at `saveload_update.cpp:1878`.

**Evidence grade: STRONG, ONE GRADE BELOW OBSERVATION.** The write was NOT
observed — no hardware watchpoint, the doc's step 3 is untested. What exists is
the disassembly plus a per-frame POST-STATE poll (~40 rounds): state 3 leaves
`_size=85070 _data=0x15e044ef760`, every state-4 round leaves both zero. That
rules out the worker (a worker filling the block leaves a live pointer) but
observes the post-state, not the write.

**The "constant 9" was an instrument artefact.** It is 9 when `_size >= 0x10000`
and 8 otherwise. `rt_predict.cpp` prints an unpredicted-write line only while
`site_->unpredicted <= kMaxVerbose * 8`, so only two lines ever printed. Wave 13
read the cap as a property of the system, and this wave's integrator repeated it
as evidence before being corrected.

The quantitative check is the strongest part: bytes = nzbytes(`_size`) +
nzbytes(`_data`). 12 rounds at 9 + 1 at 8 = 116, against an independently
counted 13 SetupLoad calls, and the measured site total was exactly
`unpredicted=116`.

**Falsifier #3 remains unrun** and is written up as a pass/fail test in lane
SETUPLOAD's report. Both obvious ways to run it are CIRCULAR — the arm's guard
is worker-owned, and the zeroing is the thing under test. The non-circular
discriminator is the embedded SavedState's state at state-4 ENTRY, and PASS
requires at least one round with `pre_data == 0`, which nobody has yet observed.

No `Exclude` was added. Wave 13 declined on the grounds that an `Exclude` is a
CLAIM that the body knows what writes a region; that call stands.

## Integration defects caught by the trial merge

**A link failure git did not flag.** SAVEMETA and LOADMETA independently chose
the same external symbol `RtSaveLoadMetaRegister()` AND the same kill switch
`BANISHED_RT_SAVELOAD_META`. Two non-static definitions of one symbol is a
duplicate-definition link error. The definitions live in DIFFERENT FILES, so
the only conflict git raised was in `rt_save_port.cpp` — resolving that conflict
alone yields a tree that compiles file-by-file and dies at link.

Neither lane could see it: each dedupe was branch-local. SAVEMETA's own
post-mortem is the useful part — its check reported "every address in
`runtime/src/save/` occurs exactly once", which was *true and useless*, because
it checked ADDRESSES and the collision was in a SYMBOL NAME and an ENV STRING.
It had flagged that the check was branch-local; it had not flagged that it was
the wrong KIND of check.

Resolved as `RtSaveSaveMetaDataRegister`/`BANISHED_RT_SAVE_METADATA` and
`RtSaveLoadMetaDataRegister`/`BANISHED_RT_LOAD_METADATA`. The underscore
placement matters: `BANISHED_RT_SAVEMETADATA` sat next to SAVEMETA's harness
vars `BANISHED_RT_SAVEMETA_ORACLE/_CONTROL/_QUIT_AFTER`, and someone typing
`BANISHED_RT_SAVEMETA=0` to disable the port would match nothing, leave the port
on, and see nothing in the log.

**A shared kill switch is not a kill switch.** Even with the symbol fixed, one
env var gating two ports means neither is independently A/B-able, which defeats
the stated reason both lanes wanted their own.

## A green screen has two causes, and they are opposite

Lane LOADMETA's closing generalization, and the most transferable thing in the
wave: **a screen can be green because it is RIGHT, or because it had NOTHING TO
TEST.** A divergence count cannot tell those apart, and neither can an aggregate.

Both of its own results are instances, in opposite directions:

* **S6 scored 11/13**, and the shortfall was NOT a blind screen — 11 of 13 saves
  carry at least one `ModInfo` and two carry none, so on those two the control
  had no subject. That was only visible because the controls were scored PER
  SCREEN; on the union it would have read as a hole in the screen.
* **S7 did not exist** until the integrator asked whether the oracle covered the
  side effect. The original six screens compared out-params only and were
  structurally blind to a missing free-and-zero — while reporting
  `calls=13 divergences=0`. The green was real and the coverage was not.

The same shape recurs across the wave, which is why it is stated here as a rule
rather than as three anecdotes:

* SAVEMETA's SIZE screen scored **0/3** against a value corruption while the
  BYTES screen scored 3/3. Scored on the union: 3/3 twice, and the blind screen
  invisible.
* DRAW's negative control fired **4/4 on `[pixels]` and 0/4 on the other two**
  sites — the reason it split one site into three.
* SETUPLOAD's Screen A reported `wrong=0` and needed a SELF-TEST returning
  `wrong=56` to prove the comparison could go red at all.

The rule that follows: score every screen SEPARATELY, state the mechanism of
each miss, and never report a control result as a union total.

## Instruments and their blind spots

* `tools/abi_retslot_audit.py` scans **only `engine/funcs/`**. It could not see
  any of this wave's six new files in either direction. The 0-of-49 baseline
  says NOTHING about the new ports. Reported by lane SETUPLOAD against its own
  interest.
* A duplicate-address scan **without a comment stripper** reports prose as code.
  The integrator's first scan returned 11 duplicates; 5 were comments mentioning
  an address. Comment-stripped, base and 5-lane merge both read exactly 6, all
  pre-existing, none in `save/`.
* The new duplicate-`Register()`-symbol check is **namespace-blind and
  build-blind**: it reports a namespace-scoped function plus its global
  forwarder as a duplicate, and it reports uncompiled fixtures under
  `tools/fixtures/`. It caught the real collision; keep the caveat with it.
* `runtime_census.py` sees the multi-line `RegisterRuntimeExport(` + literal
  form correctly. A one-line grep for the two on the same line does not.

## Carried forward, deliberately not done here

* **The promoted `_singleton` half is verified and unclaimed.** SINGLETON ran it
  end-to-end — `arrangement=MINTED`, publish check green, 19 pass / 0 fail, a
  real autostarted game reaching `SAVE OPERATION COMPLETE` with an 8.6 MB
  `Auto.sav`, exe import slot following the promotion. Whoever does the
  DLL-absent boot does not need to re-establish that; they need to add the
  three-line triple in `reg_mint_promoted.cpp` and confirm it holds with the DLL
  gone. The standing case is recorded beside the deleted triple.
* **`?Get@SaveLoad@Save@@` is a called-but-never-to-be-called row.** Dead in all
  four shipped binaries AND in the ToolKit profile build. It will sit in the
  `called` denominator permanently — the mirror image of the `UNPROMOTABLE` tag
  from 1afa558, on the other column. A census-schema question for a census lane,
  deliberately not patched mid-wave by a lane whose subject was something else.
* **A 6-vs-5 reader-set asymmetry between builds.** Retail has five references
  to `_singleton`; the ToolKit profile DLL has six (`0x18008e13a/16b/1b5`,
  `0x180093553`, `0x1800d6460/652c`). Different inlining. Left un-pulled, but if
  a future lane finds a retail reader that SINGLETON's enumeration missed, that
  asymmetry is the first place to look.
* **A pre-existing crash, not this wave's.** `BANISHED_RT_PREDICT=1` with
  `BANISHED_SAVE_TEST=400` faults at `reimpl+0x353CAE` in Wave 13's
  `UpdateSaveBody`. An A/B with `BANISHED_RT_SETUPLOAD=0` crashes identically,
  which is the correct control for "predates this lane".
* **`ModInfoDestroyRange` / `ModInfoArraySetSize`** (`0x1800341e0`,
  `0x180033080`, `0x180034010`) exist as file-local statics in BOTH
  `saveload_meta.cpp` and `saveload_loadmeta.cpp`, deliberately, to avoid a
  second collision on top of the symbol one. Consolidating them to one copy is a
  follow-up AFTER both land, re-checked against the bytes — not folded together
  on the assumption they are identical.
* **The Save directory is outside the game-dir lock.** `Auto.sav` changed size at
  20:23 at the boundary between two lanes' runs, unattributed. The lock protects
  the exe and the game-dir logs, not game data; its own header admits this.

## What is NOT verified

Stated per lane rather than aggregated, because the aggregate would read better
than the truth:

* **AUTONAME** — `calls=1`. A real comparison with a working negative control
  (the corruption was visible end-to-end as `Auto2.sav` on disk), but a thin
  exercise count.
* **SETUPLOAD** — the allocation ADDRESS is compared by neither screen. All 13
  real entries came through `UpdateList` state 3; the `UpdateLoad` and
  `UpdateLoadFromFile` arms are untested against a real load. The 26 "absent"
  ids are absent from THIS table only.
* **SAVEMETA** — the MODDED PATH NEVER RAN. Zero `.pkm` mounted, `modCount = 0`
  on all 9 oracle calls, so the data-directory prefix strip, the two
  `String::Serialize` calls, the three uint32 writes and the array growth rule
  were never executed. Confirmed statically, not run. The lane's largest gap.
  The read arm and the allocation-failure arms are transcribed, unreached.
* **LOADMETA** — the WRITING PATH never ran; all 13 observed calls took the
  reading arm, and nothing in the exe calls LoadMetaData to write.
  `?Open@CompressedMemoryStream`'s bool result is discarded as retail discards
  it, so a save with a bad window table is untested.
* **SINGLETON** — cutover with the DLL ABSENT was not run, which is the one
  configuration where the promotion question actually matters. That asymmetry is
  why the row landed minted-not-promoted.
