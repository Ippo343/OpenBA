# Wave 10B — the last six UI census rows

**Opened 2026-09-09 on `wave10/base` @ `453e26d`.** Wave 10 closed the UI census
at 96.5% (170 called / 164 ported). This wave is the remainder, and the
remainder is now small enough to name completely.

## The board — measured, not inherited

`tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported --namespace UI`
returns exactly **six rows**:

| row | kind | lane |
|---|---|---|
| `??_7SpriteSheet@UI@@6B@` | minted vtable, 1 trap | **SS** ✅ PROMOTED |
| `??_7DialogInstance@UI@@6B@` | minted vtable, 2 traps | **DLGV** |
| `??_7DialogController@UI@@6B@` | minted vtable, 9 traps | **DLGV** |
| `??_7Element@UI@@6B@` | minted vtable, 8 traps | **ELEM** |
| `??_7Dialog@UI@@6B@` | minted 2026-09-09, 8 traps | **MINT** ✅ |
| `?ShowDialog@UserInterface@UI@@…` | the only CODE row | **SHOW** ✅ ported+verified |

Five vtables and one function. Every vtable row is a `??_7` whose promotion is
blocked by named abort-trap slots, and the trap list is a LIVE reading from
`BANISHED_RT_MINT_SHADOW=1`, archived at
`docs/runtime_port/wave10b/baseline_shadow_7358732.txt`:

```
??_7SpriteSheet@UI@@6B@         slots= 9 ported= 8 TRAP= 1 purecall= 0
    slot  5  ?Serialize@SpriteSheet@UI@@UEAA_NAEAVStream@IO@@@Z
??_7DialogInstance@UI@@6B@      slots=10 ported= 8 TRAP= 2 purecall= 0
    slot  2  ?Type@DialogInstance@UI@@UEBAIXZ
    slot  4  ?OnOpened@DialogInstance@UI@@UEAAXXZ
??_7Element@UI@@6B@             slots=12 ported= 3 TRAP= 8 purecall= 1
??_7DialogController@UI@@6B@    slots=11 ported= 2 TRAP= 9 purecall= 0
```

**The numbers in `WAVE10_UI_CENSUS.md` §1 were already stale** (it records
`DialogController` 9, `Element` 8, `SpriteSheet` 8, `DialogInstance` 2 — the
SpriteSheet and DialogInstance figures moved during the wave). Reading them as
current would have scoped two lanes wrong. Lanes are dispatched off the live
table, not off the doc.

## One prescription checked before dispatch — and my own reading of it was wrong too

**CLOSED by lane MINT at `2f8e970`.**

Both `WAVE10_UI_CENSUS.md` and `standalone/reg_mint_promoted.cpp` say
`??_7Dialog@UI@@6B@` "expires by re-running `tools/rt_mint_gen.py`". It does
not. The generator **refuses to run** today:

```
REFUSING TO GENERATE — verification failed:
  ?_disableHandler@Debug@System@@0_NA: exported DATA in .data with no known size
  vtable closure: +4 BaseTypeInfo pulled in by a minted vtable
  chain closure: +50 BaseTypeInfo that exist in the DLL's reflection chain
                 but nothing in this tree names
```

It writes nothing, so "I re-ran the generator and the file did not change" is
the failure presenting as a no-op. That is the same shape as
`reference_a_prescription_read_as_a_completion`: a remedy written as if it were
a step, which nobody ran until a lane was scoped on it.

**But there was ONE refusal, not three, and the wave doc got that wrong because
I read a merged stream.** `rt_mint_gen.py 2>&1` merges a BUFFERED stdout into an
UNBUFFERED stderr, so two ordinary progress lines (`+4 vtable closure`, `+50
chain closure`) sorted underneath the `REFUSING` banner and read as items in its
error list. In the source those two are `print()`; only the size complaint is
`errors.append()`. **The tell was inside the output I already had: the banner
sorted ABOVE the lines it appeared to introduce, and a summary cannot precede
its own items.** `2>/dev/null` on the same command shows stderr carrying exactly
one entry.

There was no ~54-row expansion either. All 54 closure rows were ALREADY in the
committed `.inc` (typeinfo count 104 before, 104 after); the generator
re-derives and re-prints them every run. The two judgement calls the lane was
asked to adjudicate had been made and shipped by Wave 9 lanes Z and TYPE, so it
adjudicated nothing — the right answer.

The one real blocker was `?_disableHandler@Debug@System@@0_NA`'s missing size,
pinned at **1** from the DLL rather than from the mangling (this tree has been
burned by `?_errorString@…0PADA`, whose `PAD` says "pointer" and whose real size
is `0x2000`), with a **discriminating control**: the same width scan over the
real 8-byte `?_exceptionCallback@` three bytes below reports qword, so `width=1`
is a reading and not a scanner that only ever says 1.

### What minting `??_7Dialog` actually bought — a planning correction

```
BLOCKED  ??_7Dialog@UI@@6B@   slots= 9 ported= 1 TRAP= 8 purecall= 0
```

**`??_7Dialog` is the FURTHEST of the five UI vtables from promotion, not the
closest.** It sits behind SpriteSheet (1 trap), DialogInstance (2) and Element
(8, but 3 already ported), roughly level with DialogController (9). The census
set-diff is EMPTY — six rows before, six after — which is the correct result:
**minting is not porting**, and a row with eight traps is deliberately
unpromoted. Contrary to the dispatch's expectation, minting did not grow the
surface, because the new row's only dependency was storage rather than a
newly-referenced export.

Regression on the other four lanes: CLEAN. The whole 56-row readiness table
diffs against the archived baseline with exactly one hunk, the added Dialog
line; all ten promoted rows cleared `RtMintPromotionStorage`'s aborting VA check
on a live boot (536/536 slots, 624/624 BaseTypeInfo fields).

## The integrator's census, at the merge tip — READ THE SET, NOT THE COUNT

After merging MINT, SS and SHOW the UI count reads **6 → 6**, which looks like
nothing happened. The SET moved by four:

```
LEFT     ??_7SpriteSheet@UI@@6B@                       (lane SS, promoted)
LEFT     ?ShowDialog@UserInterface@UI@@…               (lane SHOW, ported+verified)
ENTERED  ?DrawStandAloneUI@UserInterface@UI@@CAXA…     (lane SHOW's own port)
ENTERED  ?UpdateStandAloneUI@UserInterface@UI@@CAXA…   (lane SHOW's own port)
```

The two entrants are **correct and deliberate**, not a defect:
`runtime/src/ui/userinterface_showdialog.cpp` resolves both static callbacks by
NAME, having checked the shipped export directory rather than assuming — they
mangle `CAXA…` (private static), and this DLL exports its private members
(`reference_aeaa_is_private_not_unexported`). Reaching them by address when a
name exists is how silent address doors accumulate. So a faithful port GREW the
cutover surface, exactly as
`reference_porting_can_grow_the_cutover_surface` predicts, and the two new rows
are the honest price of not opening two address doors.

**Discrepancy recorded rather than smoothed:** lane SHOW reported its own
set-diff as `6 → 5, ENTERED: none`. At the merge tip both entrants are present
and they come from that lane's own file. The tip is the measurement of record;
the lane's reading was most likely taken before the resolve block reached its
final form. This is precisely why the wave rule is to diff the SET
(`reference_diff_the_set_not_the_count`) — on the count alone, four moves
cancelled to zero and would have read as a wasted afternoon.

## Lanes (dispatched 2026-09-09)

| lane | branch | scope |
|---|---|---|
| **SS** | `wave10b/lane-ss` | retail `??_7Encoding@IO@@6B@` slot map → `Serialize@SpriteSheet` → promote |
| **DLGV** | `wave10b/lane-dlgv` | `DialogInstance` 2 traps, `DialogController` 9 traps → promote both |
| **ELEM** | `wave10b/lane-elem` | `Element` 8 traps → promote; `Element::Update` is the frame-hot risk |
| **MINT** | `wave10b/lane-mint` | the generator gate, then mint and assess `??_7Dialog` |
| **SHOW** | `wave10b/lane-show` | the `MainLoop+0x90` harness callback, then `ShowDialog` |

Round-2 candidates, deliberately NOT dispatched: whatever lane MINT's
regeneration adds to the board, and whatever nested-pump work lane SHOW's route
unlocks.

## The risk that is specific to this wave

**Registering a minted vtable is not passive.** Our ported ctors *stamp* these
tables into the objects, so the DLL's own dispatch — `UserInterface::Update`
walking every element every frame — runs our slots. A promoted vtable with one
surviving trap is an `abort()` on a live path, which is why the gate is
`TRAP= 0` and an A/B boot, not a slot count that looks close enough.

The second wave-specific risk is that lane MINT regenerates
`standalone/rt_data_mint_tables.inc`, the file the other four lanes' promotions
read, while they are branched off the pre-regeneration tree. MINT is required to
diff the four subject vtables against the archived baseline and flag any
movement for the merge.

## Standing rules (unchanged from Wave 10 §3)

* One worktree per lane; the report states its **PATH and branch**.
* Every deploy/run through `tools/game_dir_lock.sh run … --snapshot …`. Never
  pipe `acquire`; never background a blocking acquire.
* Poll boot with `tools/boot_state.sh --wait 300`.
* Grep for the port before writing it, by ADDRESS *and* by MANGLED NAME.
* Read the oracle's `calls=` column, not just `divergences=`.
* `RT_DIFF_SHADOW` is a self-check, not an oracle.
* No lane edits after it reports.
