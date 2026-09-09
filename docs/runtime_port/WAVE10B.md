# Wave 10B — the last six UI census rows

**Opened 2026-09-09 on `wave10/base` @ `453e26d`.** Wave 10 closed the UI census
at 96.5% (170 called / 164 ported). This wave is the remainder, and the
remainder is now small enough to name completely.

## The board — measured, not inherited

`tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported --namespace UI`
returns exactly **six rows**:

| row | kind | lane |
|---|---|---|
| `??_7SpriteSheet@UI@@6B@` | minted vtable, 1 trap | **SS** |
| `??_7DialogInstance@UI@@6B@` | minted vtable, 2 traps | **DLGV** |
| `??_7DialogController@UI@@6B@` | minted vtable, 9 traps | **DLGV** |
| `??_7Element@UI@@6B@` | minted vtable, 8 traps | **ELEM** |
| `??_7Dialog@UI@@6B@` | **not minted at all** | **MINT** |
| `?ShowDialog@UserInterface@UI@@…` | the only CODE row | **SHOW** |

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

## One prescription checked before dispatch, and it was wrong

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
a step, which nobody ran until a lane was scoped on it. Lane MINT owns opening
that gate properly — the +50 chain closure is 50 rows nothing in this tree
names, and minting them as ports would be
`reference_uncalled_port_is_not_verified` at scale.

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
