# Wave 10B lane MINT — the mint generator gate, and `??_7Dialog@UI@@6B@`

Branch `wave10b/lane-mint`, base `wave10/base` @ `453e26d`.

**Outcome in one line:** the generator had **one** blocker, not three; it is now
fixed and the tables are regenerated; `??_7Dialog@UI@@6B@` **is minted** (9 slots,
sizeProof `tail-not-a-pointer`) and **is not promoted**, because it has **8 trap
slots** — it is the *furthest* of the five UI vtables from promotion, not the
closest. The regeneration moved **nothing** any other lane depends on.

---

## 1. The three refusals were one refusal

The lane was dispatched to open a "generator gate plus a ~54-row expansion of the
minted set", on the strength of this output:

```
$ tools/.../python.exe tools/rt_mint_gen.py 2>&1 | tail -80
REFUSING TO GENERATE — verification failed:
  ?_disableHandler@Debug@System@@0_NA: exported DATA in .data with no known size …
  vtable closure: +4 BaseTypeInfo pulled in by a minted vtable
      …
  chain closure: +50 BaseTypeInfo that exist in the DLL's reflection chain but nothing in this tree names
      …
```

That reads as a three-item error list. It is not one. **`2>&1` merged a buffered
stdout into an unbuffered stderr and reordered them.** Separating the streams
settles it:

```
$ python tools/rt_mint_gen.py 1>out.txt 2>err.txt ; echo exit=$?
exit=2
=== STDERR (the real errors) ===
REFUSING TO GENERATE — verification failed:
  ?_disableHandler@Debug@System@@0_NA: exported DATA in .data with no known size
    — add it to EXTRA_SIZES with the header that pins it
```

The stderr list has **exactly one entry**. In the source the two closure lines are
`print(...)` — progress output — while only the size complaint is an
`errors.append(...)`. And the "REFUSING" banner sorted *above* the lines it
appeared to introduce, which is the tell: a summary cannot precede its own items.

### The +4 and +50 are not an expansion either

They are **steady state**. Every one of those 54 rows is already in the committed
`standalone/rt_data_mint_tables.inc`, put there by the last successful
generation; the generator re-derives and re-prints them on every run, successful
or not.

```
$ grep -c 'typeInfo@' standalone/rt_data_mint_tables.inc      # BEFORE any change
104
?_typeInfo@Base@Object@@…              PRESENT
?_typeInfo@Internal@Object@@…          PRESENT
?_typeInfo@ToolTipDialog@UI@@…         PRESENT
?_typeInfo@DialogControllerConfig@UI@@ PRESENT
```

TYPEINFO row count before regeneration: **104**. After: **104**. Unchanged.

So the two questions the brief asked me to adjudicate — "is pulling the +4 in
correct?" and "should the +50 be minted without presenting uncalled rows as
ported?" — **were already answered and shipped**, by Wave 9 lanes Z and TYPE,
whose reasoning is in the generator's own comments at `rt_mint_gen.py:509-590`.
Nothing was decided here and nothing needed to be. Recorded per refusal as asked:

| refusal | what the guard protects | fact-finding or judgement? | status |
|---|---|---|---|
| `?_disableHandler` size | Minting a `.data` static at a guessed size either overruns `kStorageBytes` or under-covers the object. The precedent it exists for is `?_errorString@Error@System@@0PADA`, whose `PAD` mangling says "pointer" and whose real size is `0x2000` — minted at 8 it overran by 8 KB on the first cutover `SetText`. | **Fact-finding.** The DLL settles it. | **Fixed** (§2) |
| `+4 vtable closure` | A minted vtable whose own `?_typeInfo@` is absent is a half-minted row: our ported `TypeName`/`Type` slots fetch that symbol and no source file spells it, so the "mint what our source names" rule is blind to it by construction. | Judgement, **already made** (Wave 9 lane Z) — and correct: pulling them in is what stops a half-minted row. | **Not a refusal.** Steady-state stdout. |
| `+50 chain closure` | `Object::Factory::Startup` walks one `?_firstTypeInfo@` chain and resolves each `parentName` **by name against that chain**, so any subset of the chain is a chain retail does not have. Measured: 415 nodes with the DLL vs 357/4-unresolved at cutover with 46 minted; `415 − 357 = 58 = 104 − 46` closes the gap by identity. | Judgement, **already made** (Wave 9 lane TYPE). These are needed for *chain integrity*, not presented as ported. | **Not a refusal.** Steady-state stdout. |

On the "uncalled rows presented as ported" worry specifically: it does not arise.
A minted row is not a port in this project's accounting — `runtime_census.py`
excludes `rt_data_mint.cpp` from the ported side entirely, and only an explicit
row in `reg_mint_promoted.cpp` counts. That is confirmed empirically in §6: the
census is byte-identical after minting a new vtable.

*(Class: `reference_diagnostic_field_cannot_express_the_answer` — a stream-merged
banner cannot express which lines are errors, and it was read for exactly that.
Also `reference_a_prescription_read_as_a_completion`, again: the stale
`reg_mint_promoted.cpp` note is the proximate cause of the misdiagnosis.)*

---

## 2. `?_disableHandler@Debug@System@@0_NA` — pinned at 1, from the DLL

`_NA` does decode as `bool`, but the brief asked for a DLL-side proof and this
project has been burned by a mangled suffix before (`?_errorString@…0PADA`).
The suffix is corroboration here, never the proof. Two independent measurements
plus a control:

**(1) Adjacent-export gap = 1.** From the export table:

```
0x1801fc700  gap=8   ?_exceptionCallback@Debug@System@@0P6A…EA
0x1801fc708  gap=1   ?_disableHandler@Debug@System@@0_NA      <== SUBJECT
0x1801fc709  gap=1   ?_hideDisplay@Error@System@@0_NA         (already pinned 1)
0x1801fc70a  gap=1   ?_stopApplication@Error@System@@0_NA     (already pinned 1)
0x1801fc70b  gap=5   ?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA (pinned 1)
```

It is the head of a packed four-byte run of crash-handler bools, three of which
this table already pins at 1. It cannot be wider than one byte without
overlapping a symbol that is itself pinned at 1. This is the same
"adjacent-export" evidence the vtable `sizeProof` rule treats as *exact*.

**(2) Every access in `.text` is byte-width.** All five rip-relative references
to `0x1801fc708` use 8-bit operand forms — `cmp byte ptr` at `0x1800c5b6f`, and
`mov byte ptr …, imm8` at `0x1800c5b9b`, `0x1800c5bee`, `0x1800f8671`,
`0x1800f8697`. No dword or qword form references it at all.

**(3) Control — the instrument can say something other than 1.** The same scan
over `?_exceptionCallback@Debug@System@@` at `0x1801fc700`, a genuine 8-byte
function pointer three bytes below, reports **qword** accesses. So "width=1" for
the subject is a discriminating result, not a scanner that only ever says 1.

Added to `EXTRA_SIZES` at `tools/rt_mint_gen.py` with all of the above recorded
inline. Generator then exits 0.

---

## 3. Symbol set-diff of the regenerated tables

Diffed as **sets of rows and slots**, not line counts (`docs`-side dumper compared
row symbol + VA + slot count + `sizeProof`, and every slot's target VA and name).
Row counts reconcile against the file's own header on both sides
(before 55/104/38, after 56/104/39).

**Rows — two entered, zero left:**

```
> STORAGE   ?_disableHandler@Debug@System@@0_NA   0x1801fc708   size=1
> VTABLE    ??_7Dialog@UI@@6B@                    0x1801562a0   slots=9  proof=tail-not-a-pointer
```

**Slots — 527 → 536, i.e. +9, all of them Dialog's.** The slot-level set diff
with Dialog filtered out is **empty**: no pre-existing slot changed VA, name or
index.

`??_7Dialog@UI@@6B@` therefore has a plausible slot count (9) and a `sizeProof`
(`tail-not-a-pointer`), as required. Its nine slots also take part in the
shadow's ground-truth screen — `536 slot comparisons vs the live DLL, 0
MISMATCH` — so the decode is checked against the DLL, not just plausible.

---

## 4. `??_7Dialog@UI@@6B@` readiness — BLOCKED, 8 traps

Verbatim from `BANISHED_RT_MINT_SHADOW=1` (full log:
`docs/runtime_port/wave10b/shadow_lane_mint_postregen.txt`):

```
[RtMintShadow]   BLOCKED  ??_7Dialog@UI@@6B@            slots= 9 ported= 1 TRAP= 8 purecall= 0
[RtMintShadow]              TRAP slot  0  (unexported; known by address only)
[RtMintShadow]                            DLL 0x180038b20
[RtMintShadow]              TRAP slot  1  ?TypeName@Dialog@UI@@UEBAPEBDXZ
[RtMintShadow]              TRAP slot  2  ?Type@Dialog@UI@@UEBAIXZ
[RtMintShadow]              TRAP slot  3  ?GetFileVersion@Dialog@UI@@UEBAHXZ
[RtMintShadow]              TRAP slot  5  ?Serialize@Dialog@UI@@UEAA_NAEAVStream@IO@@@Z
[RtMintShadow]              TRAP slot  6  ?Reconstruct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ
[RtMintShadow]              TRAP slot  7  ?Destruct@Dialog@UI@@UEAAXXZ
[RtMintShadow]              TRAP slot  8  ?Construct@Dialog@UI@@UEAAPEAVExternal@Object@@XZ
```

The single ported slot is **4**, `?Compile@External@Object@@` (inherited).

**NOT PROMOTED**, per the playbook in `reg_mint_promoted.cpp`: our ctors stamp
these tables and the DLL's own dispatch runs the slots, so a promoted vtable with
any trap in it is an `abort()` waiting for the first dispatch. The gate is
`TRAP = 0`; this row is at 8. It never reached step 2 of the playbook (the
`BANISHED_RT_MINT_ROWS` A/B), because step 1 did not pass — running the A/B on a
row the instrument already calls BLOCKED would prove nothing.

**What would clear it:** port and register those eight bodies. Ranked by apparent
cost — slots 1/2 (`TypeName`/`Type`) are the two-line reflection accessors every
other minted UI vtable already has ported; slot 3 (`GetFileVersion`) is a
constant reader; slots 6/7/8 (`Reconstruct`/`Destruct`/`Construct`) are the
External lifecycle triple; slot 5 (`Serialize`) is the real work, and is
replayable on the write path (`reference_write_path_serialize_is_replayable`);
slot 0 is the unexported scalar-deleting destructor at `0x180038b20`, carried by
address. For the wave's planning: **`??_7Dialog` is eight rows from promotion,
not one** — it is behind `SpriteSheet` (1 trap), `DialogInstance` (2) and
`Element` (8, but 3 already ported), and roughly level with `DialogController` (9).

---

## 5. Regression check on the other four lanes' vtables — CLEAN

Against `docs/runtime_port/wave10b/baseline_shadow_7358732.txt`:

| vtable | VA | slots | sizeProof | slot targets | readiness |
|---|---|---|---|---|---|
| `??_7SpriteSheet@UI@@6B@` | `0x180154590` | 9 | `tail-not-a-pointer` | identical | `BLOCKED … ported= 8 TRAP= 1` unchanged |
| `??_7DialogInstance@UI@@6B@` | `0x180156d70` | 10 | `adjacent-export:??_7ListElement@UI@@6B@` | identical | `BLOCKED … ported= 8 TRAP= 2` unchanged |
| `??_7Element@UI@@6B@` | `0x180156160` | 12 | `adjacent-export:??_7ElementDescription@UI@@6B@` | identical | `BLOCKED … ported= 3 TRAP= 8 purecall= 1` unchanged |
| `??_7DialogController@UI@@6B@` | `0x1801565f0` | 11 | `tail-not-a-pointer` | identical | `BLOCKED … ported= 2 TRAP= 9` unchanged |

Stronger than asked: the diff of the **whole** `READY`/`BLOCKED` readiness table
(all 56 vtables, not just the four) against the baseline has exactly **one** hunk
— the added `??_7Dialog` line. Nothing else moved.

Promoted rows still pass `RtMintPromotionStorage`'s VA check: the helper
`abort()`s on a mismatch, and the boot printed
`[RtMint] promoted 10 minted DATA row(s) -- live on every run`, so all ten
cleared it. Supporting screens from the same boot:

```
[RtMint] minted 56 vtables (536 slots: 204 ported, 184 unported-trap, 148 purecall-trap),
         104 BaseTypeInfo, 39 storage symbols (11344 bytes)
[RtMintShadow] ground truth : 536 slot comparisons vs the live DLL, 0 MISMATCH
[RtMintShadow] minted from OUR : 204 slots, 0 MISMATCH
[RtMintShadow] BaseTypeInfo : 624 field comparisons, 0 MISMATCH, 0 typeId not-yet-comparable
```

`standalone_crash.log` empty; boot state `RUNNING`.

**Merge note for the integrator: nothing the other four lanes depend on moved.**
Their vtables' VAs, slot counts, slot targets and readiness are bit-for-bit the
baseline. The only additive changes are one storage row and one new vtable row.

---

## 6. Census set-diff — EMPTY (and that is the correct result)

`tools/runtime_census.py --unported --namespace UI`, before and after:

```
Called but not ported (6):
  export UI  ??_7Dialog@UI@@6B@
  export UI  ??_7DialogController@UI@@6B@
  export UI  ??_7DialogInstance@UI@@6B@
  export UI  ??_7Element@UI@@6B@
  export UI  ??_7SpriteSheet@UI@@6B@
  export UI  ?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
```

**Rows entered: none. Rows left: none.** Six before, six after, same set.

This is the right answer, not a failure to deliver: **minting is not porting.**
The census counts a symbol as ported only when something registers it outside
`rt_data_mint.cpp`, and `??_7Dialog` is minted but deliberately unpromoted. The
census will drop it when its eight trap slots are ported and the row is promoted
— which is the same thing that has to happen for the other four.

Worth stating plainly because the brief anticipated the opposite: minting *can*
grow the surface, and here it did not, because the new row's dependencies
(`?_disableHandler`) were storage rather than newly-referenced exports.

---

## 7. Files changed

* `tools/rt_mint_gen.py` — `?_disableHandler@Debug@System@@0_NA` added to
  `EXTRA_SIZES` at 1 byte, with the gap proof, the five byte-width access sites
  and the 8-byte control recorded inline. Generator now exits 0.
* `standalone/rt_data_mint_tables.inc` — regenerated. +1 storage row, +1 vtable
  row (`??_7Dialog@UI@@6B@`, 9 slots), +9 slots. Nothing removed, nothing moved.
* `standalone/reg_mint_promoted.cpp` — the stale
  "NOT MINTED AT ALL … expires by re-running `tools/rt_mint_gen.py`" note
  replaced with the measured state (minted, 8 traps, each named) and an explicit
  correction of *both* wrong halves, so the next reader is not sent down the same
  path.
* `docs/runtime_port/wave10b/shadow_lane_mint_postregen.txt` — the post-regen
  shadow log this document quotes.

Builds: `standalone-debug` green; `standalone` (release, `-O3 -DNDEBUG`) green
from clean.
