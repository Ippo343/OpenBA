# WAVE 19 — PHASE 0b, lane MINT: the five unminted IO vtables

**Worktree** `C:/code/GIT/wt-w19-mint` · **branch** `wave19/mint` · **base**
`ebbec6e` (`wave19/base`) · **written 2026-09-10**.

Cell: [WAVE19_IO.md](WAVE19_IO.md) §3.0b. Doctrine: [WAVE18_CLOSED.md](WAVE18_CLOSED.md)
§4 and §7, [WAVE19_IO.md](WAVE19_IO.md) §6 and §7.

---

## 0. The one-sentence answer, and it contradicts the cell's thesis

**The generated file was STALE, not the generator wrong**: `standalone/rt_data_mint_tables.inc`
was last regenerated at `4f037fd` (WAVE 10D, 2026-09-09), `tools/rt_mint_gen.py`
has not changed since `7bf01e4` which *precedes* that, and the four source files
that put these five vtables into the census's `called` set all landed **after**
it (Wave 11f lane IOSTREAM) — so running the generator **unchanged** mints all
five.

§3.0b's thesis was *"This is a `tools/rt_mint_gen.py` harvest gap; fixing the
generator is cheaper than five hand-written rows."* I was told to test that
judgement rather than assume it. **Both halves are false**: the generator needed
no fix, and neither did a hand-written row. The cost was one command.

The generator was **right** at `4f037fd`. Checked at that commit: the only
occurrences of these five names anywhere in the tree were one **comment** in
`runtime/src/io/compressedstream.cpp:725` and the generator's own
`adjacent-export:` labels inside `rt_data_mint_tables.inc`. `rt_mint_gen.py`
already strips comments (`strip_comments`, line 376) and already lists that
`.inc` in `SCAN_SKIP` (line 370), so neither is a reference. Nothing in the tree
named these five, so nothing minted them. **That is the population rule working,
not failing.**

`reference_mint_population_rule_blind_to_its_own_output.md` was named in my
tasking as the likely root cause. I checked it and it is **not** this: that
memory's failure is a symbol only the minted OUTPUT dereferences, and both
closure rules it produced are present and firing (`+49 BaseTypeInfo` on this
run). These five are read by ordinary ported source, through the ordinary door.

---

## 1. The trap, reproduced first, with denominators

The cell warned that the name grep looks like a hit and is not one. Reproduced
against the file **as committed at `ebbec6e`** (2183 lines, **57** vtable rows,
104 BaseTypeInfo rows, 39 storage rows):

| vtable | VA | hits **by VA** | hits **by name** |
|---|---|---|---|
| `??_7BinaryFileStream@IO@@6B@` | `0x1801554d0` | **0** | 4 |
| `??_7CompressedFileStream@IO@@6B@` | `0x1801555f0` | **0** | 2 |
| `??_7FileStream@IO@@6B@` | `0x180154960` | **0** | 0 |
| `??_7TextFileStream@IO@@6B@` | `0x180155980` | **0** | 2 |
| `??_7VisitorStream@IO@@6B@` | `0x180155a20` | **0** | 0 |

**All eight name hits are `adjacent-export:` labels on OTHER rows** — lines 557,
1125, 1225, 1413 and their four `kMintVtables` entries. Not one is the symbol's
own row. Confirmed by reading every hit, not by counting them.

*Correction to the cell*: §3.0b reports the by-name counts as **4, 2, 2, 0, 0**;
measured here they are **4, 2, 0, 2, 0** — `FileStream` 0 and `TextFileStream` 2,
i.e. two entries transposed. The substantive claim (non-zero by name, zero by
VA, every hit an adjacent label) is unaffected.

**The five VAs were re-read from the DLL's own export directory**, not copied
from the cell: all five are exported and all five live in `.rdata`, at exactly
the addresses stated.

**The census names all five today**: `export,<name>,IO,0,0,<consumers>` at rows
418 / 421 / 424 / 429 / 430 of a fresh `runtime_census.py --csv` (1175 rows).
Their consumers are `runtime/src/io/file_stream_probe.cpp`,
`runtime/src/object/external_savetext_win.cpp`, `.../external_load.cpp`,
`.../external_add.cpp`.

---

## 2. The evidence that it is staleness

1. `git log -- standalone/rt_data_mint_tables.inc` → newest is **`4f037fd`**,
   "WAVE 10D: the single wave-close regeneration", 2026-09-09.
2. `git log -- tools/rt_mint_gen.py` → newest is **`7bf01e4`**, also 2026-09-09,
   and it is **older** than `4f037fd`. **The generator has not changed since the
   last regeneration**, so the delta cannot be a generator change.
3. Every census consumer file postdates `4f037fd`: `git merge-base --is-ancestor
   4f037fd <c>` is **YES** for `1b4980a`, `f96ffc1` and `023b1bb` (all Wave 11f
   lane IOSTREAM).
4. `git grep` **at `4f037fd`** shows the only occurrences of these names were a
   comment and the generator's own labels (§0).
5. Running `tools/rt_mint_gen.py` **unmodified** yields 62 vtables and all five
   rows.

Steps 2 and 3 are independent of step 5 and of each other.

---

## 3. The cross-namespace set-diff of the regeneration

Compared **index-independently and by full row CONTENT**, not by count
(`reference_diff_the_set_not_the_count.md`). Each row is keyed by (VA, symbol);
a vtable row's value is its complete ordered slot list plus its count, size
proof and note; a typeinfo/storage row's value is its full literal with the
`kVSlotsN` / `kTypeInfoStorage[i]` / `kStorageBytes[i]` indices normalised away,
because those renumber mechanically when a row is inserted.

| table | OLD | NEW | ENTERED | LEFT | CHANGED | PARSEFAIL |
|---|---|---|---|---|---|---|
| vtable | 57 | 62 | **5** | 0 | **0** | 0 |
| typeinfo | 104 | 104 | 0 | 0 | 0 | 0 |
| storage | 39 | 39 | 0 | 0 | 0 | 0 |

**ENTERED, in full — this is the complete cross-namespace list, there is nothing
outside IO:**

```
0x180154960  ??_7FileStream@IO@@6B@
0x1801554d0  ??_7BinaryFileStream@IO@@6B@
0x1801555f0  ??_7CompressedFileStream@IO@@6B@
0x180155980  ??_7TextFileStream@IO@@6B@
0x180155a20  ??_7VisitorStream@IO@@6B@
```

**LEFT: `<none>`, in all three tables.** **CHANGED: `<none>`** — all 57
pre-existing vtable rows, all 104 BaseTypeInfo rows and all 39 storage rows are
identical in content. The textual diff is 4508 lines; **every line of it outside
the five new rows is `kVSlotsN` renumbering.**

The generator's own summary corroborates the shape: `census 164 unported exports
-> 205 exported DATA (253 rows cross-checked against the round-1 harvest, 0
disagreements)`, `vtables 62 (598 slots, 33 count-proved by adjacency, 29 by
tail probe)`, `typeinfo 104`, `storage 39 (11344 bytes)`.

### 3.1 Controls on the diff instrument

Every arm was scored **separately, on its applicable cases only**
(`reference_union_control_hides_a_blind_screen.md`), and each was verified to
print before any zero above was trusted.

| control | applicable arm(s) | required | read |
|---|---|---|---|
| A · identity (file vs itself) | all three | 0/0/0 | 0/0/0 ✅ |
| B · one vtable row renamed | vtable | LEFT 1, ENTERED 1 | 1, 1 ✅ |
| C · one typeinfo + one storage row renamed | typeinfo, storage | 1/1 each | 1/1, 1/1 ✅ |
| D · one slot VA, one typeinfo field, one storage note mutated | all three (CHANGED) | CHANGED ≥1 each | 2 / 1 / 1 ✅ |

Control D's vtable arm reads **2**, not 1, because the mutated slot target
(`0x1800a81e0`) is shared by `??_7ReaderWriter@IO@@6B@` and the by-address
BinaryEncoding row at `0x1801554a8`. That is the instrument being right about a
shared body, and it is stated here rather than rounded to 1.

Control A alone would have been worthless: it proves only that the tool is
deterministic. **B, C and D are what license the zeros**, and D is the only one
that exercises CHANGED at all — without it, `CHANGED 0` in the table above would
be an unexamined claim.

---

## 4. Per-row status: minted, and what promotion would still take

**Minting is not promoting.** We intercept only at DOORS, and an in-DLL rel32
call or a RIP-relative LEA stamp is not a door
(`reference_a_ported_row_can_be_permanently_unpromotable.md`). A row reading
READY/TRAP=0 says the table is **safe to stamp**, not that anything dispatches it
(`reference_a_green_readiness_screen_is_not_a_route.md`).

Measured on this tip with `BANISHED_RT_MINT_SHADOW=1` — **not inherited from
CEIL §3.4, which could not report these rows at all because no minted row
existed:**

| row | minted? | slots | ported | TRAP | purecall | attended (gameplay route) |
|---|---|---|---|---|---|---|
| `??_7BinaryFileStream@IO@@6B@` | ✅ 8 slots, adjacent-export | 8 | 0 | **8** | 0 | no, `post=0` |
| `??_7CompressedFileStream@IO@@6B@` | ✅ 9 slots, tail-not-a-pointer | 9 | 1 | **8** | 0 | **yes, `post=2`** |
| `??_7FileStream@IO@@6B@` | ✅ 8 slots, adjacent-export | 8 | 0 | **8** | 0 | no, `post=0` |
| `??_7TextFileStream@IO@@6B@` | ✅ 20 slots, adjacent-export | 20 | 0 | **20** | 0 | no, `post=0` |
| `??_7VisitorStream@IO@@6B@` | ✅ 7 slots, tail-not-a-pointer | 7 | 0 | **7** | 0 | **yes, `post=1`** |

**52 slots, 1 ported, 51 traps, 0 purecall.** All five are **BLOCKED**, and none
is close: the single ported slot is `?Read@CompressedStream@IO@@` in
`??_7CompressedFileStream@`. Contrast the cheapest rows in CEIL's subject
(`??_7BinaryReader@` at 25 of 26): **these five are not cheap.**

**Promotion for each therefore requires, in order:**

1. **TRAP → 0**: port and register the 51 trapping slots. They are **41 distinct
   bodies** (counted with a parser bounded strictly by the row headers, because
   an unbounded scan-back pairs across row boundaries and manufactures findings
   — `reference_checker_pairs_across_function_boundaries.md`; my first attempt
   did exactly that and leaked `Goal@Achievement` and `LoaderLoop@Engine` rows
   into the tally, so that first count is discarded and not reported here).
   **12 of the 41 are unexported and known only by address** — 10 of those in
   `??_7TextFileStream@` alone (`0x1800b9a70`…`0x1800bafe0`). Per
   `reference_aeaa_is_private_not_unexported.md`, "unexported" here means the
   export directory has no name for it, **not** that it is unmintable or
   unportable: the mint already carries rows by address.
2. **A DOOR**: the consumer must reach the vtable through
   `ResolveExport`/`ResolveAddr`, not an in-DLL stamp. Attendance answers this
   for two of the five and leaves it **open** for three (§5).
3. **A driven route**, which nothing in this lane provides.

**The FileStream family shares bodies, and that is the actionable finding for
lane FILE**: `?GetSource@FileStream@` and `?Open@FileStream@` each appear as a
trap in **3** of the five rows, and `?Write@`/`?Read@`/`?SetCurrentPos@`/
`?GetCurrentPos@`/`?GetSize@FileStream@` in **2** each. Porting the eight
`IO::FileStream` base slots clears **8 traps in `??_7FileStream@`, 8 in
`??_7BinaryFileStream@` and 7 of the 20 in `??_7TextFileStream@`** — three rows
moved by one body of work. Lane FILE already owns `?Create@FileStream@` and the
two `FileStream` dtors.

### 4.1 The shadow's own verdict on the new rows

`[RtMintShadow] ==== clean: 0 problem(s) ====`, with:

* `ground truth: 598 slot comparisons vs the live DLL, 0 MISMATCH`
* `minted from OUR: 257 slots, 0 MISMATCH  <-- the only number that measures the port`
* `BaseTypeInfo: 624 field comparisons, 0 MISMATCH`
* no `SIZE SUSPECT` on any of the five.

CEIL's run reported **546** slot comparisons. 546 + 52 = **598**, and 52 is
exactly 8+9+8+20+7. The arithmetic closes by identity, which is a second,
independent check that the five rows and nothing else entered.

---

## 5. §0b's sixth question: which two are ATTENDED, and what that means

**ATTENDED** here is lane CEIL's `BANISHED_W17_ATTEND=1` screen
(`standalone/wave17_door_attend.cpp`): a counter placed inside `ResolveExport`,
**before** the registry short-circuit, so it sees every consumer of a row —
direct or through `RT_DATA_SLOT → RtResolveCached` — with one instrument and no
edit to any ported body. `init` and `post` are counted separately across the
`AddressRegistryInit()` boundary, and **only `post` is attendance**: a resolve
during registration is not a consumer running. `post >= 1` proves the consumer
ran; `post == 0` proves it never did on that route; `post == 1` says **nothing
about frequency**, because nearly every site caches in a function-local static.

The cell's claim is **[INHERITED] from `3da1503`**. I re-verified it on this tip,
on the regenerated mint, on the gameplay route
(`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`),
under the game-dir lock. **It reproduces exactly and names the same two:**

```
CONTROL ?_singleton@MainLoop@Engine@@1PEAV12@EA: init=1 post=2 -> GREEN
never     init=0 post=0   ??_7BinaryFileStream@IO@@6B@
ATTENDED  init=0 post=2   ??_7CompressedFileStream@IO@@6B@
never     init=0 post=0   ??_7FileStream@IO@@6B@
never     init=0 post=0   ??_7TextFileStream@IO@@6B@
ATTENDED  init=0 post=1   ??_7VisitorStream@IO@@6B@
35 of 62 subject rows had their door opened AFTER init; 27 never did.
```

The positive control read **GREEN** before any zero above was read; the
instrument **suppresses the whole table** rather than print plausible zeros if it
had read red. Exit 0, 0-byte crash log.

**So the two are `??_7CompressedFileStream@IO@@6B@` and
`??_7VisitorStream@IO@@6B@`, and what attendance buys them is precisely one
thing: their door is proven to open in real play.** That removes step 2 of §4
for those two and leaves 8 and 7 traps respectively. It does **not** make them
ready, and it does **not** mean the *vtable* is dispatched — it means the
*symbol's door* was opened by a consumer.

**What the three zeros do NOT mean.** This is **one** of CEIL's four routes. A
`post=0` here is "no consumer ran **on the gameplay route**", not "no route
exists" — and `??_7FileStream@` and `??_7TextFileStream@`'s consumer is
`runtime/src/object/external_savetext_win.cpp` (`?SaveText@External@Object@@`),
which a save-text route would reach. **"No route driven" is not "no route
exists"**; Wave 18 refuted that four times. Driving the other three routes is not
in this cell.

---

## 6. Effect on the identity

```
ported >= called - unpromotable
IO today:   83 >= 122 - 3 = 119        36 SHORT
```

**Measured on this tip after the regeneration: `IO  407 internal / 122 called /
83 ported / 0 detoured`, and the census still lists all 39 IO export rows as
unported, 3 of them UNPROMOTABLE(HARMFUL).** Identical to WAVE19_IO.md §1.

**This lane's effect on the identity is exactly ZERO.** `called` did not grow;
`ported` did not grow; `unpromotable` did not change. Minting a row does not
port it, and the census correctly continues to count all five as unported. No
percentage is quoted anywhere in this document, by design.

What the wave gets instead is that **five rows moved from "cannot be worked on"
to "ordinary port work with a measured trap list"** — before this, lane FILE and
lane IOBASE had no minted row to promote into, and CEIL's triage had to file them
under *blocked because no minted row exists*.

---

## 7. What this does NOT claim

* **It does not claim any port.** Zero bodies were written. `ported` is unchanged
  in every namespace.
* **It does not claim any promotion.** All five rows are **BLOCKED**, 51 traps
  across them. Nothing was added to `standalone/reg_mint_promoted.cpp`.
* **It does not claim a route.** No route was driven for any of the five. The two
  runs I made were instrument runs; neither exercised a FileStream path on
  purpose, and I make no claim about the three `post=0` rows beyond "not on the
  gameplay route".
* **It does not claim the five are cheap.** They are 41 distinct bodies, 12 of
  them unexported. Only the FileStream-base sharing (§4) is a saving, and I have
  not verified that those eight bodies are unported — I read the shadow's TRAP
  list, which says *not registered*, and a body ported but never registered is
  indistinguishable from an absent one to that screen
  (`reference_half_registered_port_reads_as_absent.md`). **Lane FILE must grep
  before writing.**
* **It does not claim the trap counts for any row outside the five.** I read
  CEIL's other rows off the same log but did not audit them; §0c is a different
  cell and its `[INHERITED]` warning still stands for everything else.
* **It does not claim the generator is now correct in general.** It claims the
  generator was correct on the *specific* question asked, and that its output
  had gone stale. `--check` detects staleness of the row set and of row content;
  it does **not** validate the population rule, and any blindness in that rule
  survives this commit untouched.
* **It does not claim `--check` is wired into anything.** It is a mode, not a
  gate. Adding it to a wave-close checklist is a decision for the integrator.
* **It does not claim the two `post>0` rows are dispatched.** Attendance is about
  the symbol's door, not the vtable's use.
* **It does not re-verify the cell's other phase-0 claims** (0a's purecall counts,
  0c's inherited trap table).

---

## 8. Commits

| commit | what |
|---|---|
| `3620e69` | regenerate `standalone/rt_data_mint_tables.inc` (57 → 62 vtables); the full set-diff and its four controls are in the message |
| `5944325` | `tools/rt_mint_gen.py --check` (staleness gate, exit 3, names entered/left rows) + the two lock-run scripts |

Build: `mingw32-make standalone-debug` **green**, no warnings introduced, done
**without** the game-dir lock. Both measurement runs used the self-releasing
`tools/game_dir_lock.sh run lane-MINT 900 -- …` form, each as its own statement;
both exited 0 with a 0-byte `standalone_crash.log`.

---

# ANNEXE A — POST-REPORT TRANCHE (2026-09-10, after this lane reported)

Added on re-tasking by the integrator. **§0–§8 above are sealed and unaltered;
this annexe only adds** (WAVE19_IO.md §7: sealing forbids altering, not
annotating). Nothing here changes the root cause, the set-diff, or the identity.

## A.1 The integrator's ANCHOR hypothesis is REFUTED

The hypothesis: *"the generator uses an export as an ANCHOR to locate an adjacent
unnamed vtable, mints the NEIGHBOUR, and never mints the ANCHOR itself — the five
are anchors, consumed as coordinates and discarded as subjects."*

**It is wrong, and one fact settles it: the generator, run UNCHANGED, mints all
five.** A generator that structurally discarded anchors could not produce them
from the same code. That result predates this annexe (§2, commit `3620e69`).

Three further refutations, each independent:

1. **`adjacent-export:` is a SIZE PROOF, not a discovery mechanism.** In
   `tools/rt_mint_gen.py` the population is fixed **before** any slot is decoded
   (census ∪ source-literal scan ∪ `EXTRA_*` ∪ two typeinfo closures). The proof
   string is computed *afterwards*, per row, from the next exported RVA:
   `nxt = sorted_rvas[bisect_right(sorted_rvas, rva)]`, then
   `if nxt == end_rva: proof = "adjacent-export:" + by_rva[nxt]`. It records
   *why we believe the slot count*, and **feeds nothing back into the set**.
2. **Anchor and subject are not disjoint — the regenerated file proves it in
   both directions.** `??_7BinaryFileStream@IO@@6B@` is now a minted row *whose
   own proof* reads `adjacent-export:??_7CompressedCopyFileStream@IO@@6B@`,
   while simultaneously being the anchor named by `??_7ReaderWriter@IO@@6B@`'s
   proof. Likewise `??_7TextFileStream@`'s proof names `??_7VisitorStream@`,
   and **both are minted rows**. Under the hypothesis neither pair could exist.
3. **The neighbours were never "minted instead of" the anchors.** The
   by-address row at `0x1801554a8` (BinaryEncoding) is not a discovery of the
   generator at all — it is a hand-written `EXTRA_VTABLES_BY_ADDR` entry from
   Wave 9 lane C2, with the adjacency argument written out in its comment.

### A.1.1 The prediction was true and did not discriminate

The integrator called it a sharp falsifiable prediction: *every non-zero name hit
should be an `adjacent-export:` provenance label.* **I confirm it — all 8 hits
are provenance labels** (§1, verified by reading every hit).

But that prediction is **equally entailed by the true explanation**. If the file
is simply stale, the only way one of these names can appear is as some other
row's proof label — which is exactly what is observed. **A prediction that both
hypotheses make cannot select between them**
(`reference_two_checks_one_hidden_premise.md`). The test that *does*
discriminate is running the generator, and it comes out against the hypothesis.

### A.1.2 A number that looked like a match

The integrator's message reports *"I reproduced the name-grep counts on this tip
and got precisely the plan's predicted 4, 2, 2, 0, 0"* — but the table printed
directly beneath that sentence reads:

```
4  ??_7BinaryFileStream@IO@@6B@
2  ??_7CompressedFileStream@IO@@6B@
0  ??_7FileStream@IO@@6B@
2  ??_7TextFileStream@IO@@6B@
0  ??_7VisitorStream@IO@@6B@
```

That is **4, 2, 0, 2, 0** — which matches **my** measurement (§1) and **not** the
plan's 4, 2, 2, 0, 0. So the two independent measurements agree with each other
and the plan is wrong in the same way I reported. The multiset {4,2,2,0,0} and
{4,2,0,2,0} are identical as a bag, which is what let the prose read as a match.
**Two of the three things a lane could quote here — the total and the sorted
multiset — cannot tell the plan's claim from the truth; only the per-symbol
pairing can.**

## A.2 Ghidra corroboration (port 8089, `Runtime-x64.dll`)

Used as corroboration, not as the subject. `get_function_metrics` was not used
(`reference_ghidra_mcp_address_param_silently_ignored.md`).

**Slot counts confirmed independently of `pefile` and of the mint.** Raw bytes at
`0x180155980` are 20 consecutive image pointers, and qword 20 sits at
`0x180155a20` — the `??_7VisitorStream@` export. The count, the anchor and the
`adjacent-export` proof all close on the same address. This is a *third* route to
the 20 (after the generator's decode and the live shadow's 598-comparison check).

## A.3 The promotion answer, and it is the sharpest finding of the cell

**Every stamp of all five vtables is an in-DLL RIP-relative `LEA`, and per
`reference_a_ported_row_can_be_permanently_unpromotable.md` that is NOT a door.**
Verified as disassembly, not decompiled C — four sampled stamp sites, all the
same two-instruction shape:

```
1800a8270  LEA RAX,[0x1801554d0]     ; ??0BinaryFileStream
1800a8277  MOV qword ptr [RDI],RAX
180023cfe  LEA RAX,[0x1801554d0]  -> MOV [RBX],RAX
1800bc653  LEA R14,[0x1801555f0]  -> MOV [RSP+0x40],R14   ; External::Load
1800be673  LEA RAX,[0x180155a20]  -> MOV [RBP-0x18],RAX   ; External::Add
```

So **a row is promotable exactly insofar as WE own the function that stamps it.**
Mapping every stamper Ghidra lists against what our tree has ported:

| row | stampers in the DLL | ported stamper(s) we own today |
|---|---|---|
| `??_7BinaryFileStream@` | `??0BinaryFileStream` ×2 only | **none** |
| `??_7FileStream@` | FileStream/BinaryFileStream/TextFileStream ctors+dtor, Close/Create/Open, `Engine::Run` ×2 | `?SaveText@External@Object@@`, `?SaveImageToFile@ImageBuffer@Video@@` |
| `??_7CompressedFileStream@` | CompressedFileStream/CompressedCopyFileStream ctors+dtor, `Save`, `IsCurrent` | `?Load@External@Object@@` |
| `??_7TextFileStream@` | TextFileStream ctors+dtor, Close/Create/Open | `?SaveText@External@Object@@` |
| `??_7VisitorStream@` | VisitorStream ctor+dtor, `FUN_18002bf20`, `FUN_18002bf80` | `?Add@External@Object@@` |

**This explains the attendance data exactly, and that is the point.** The two
ATTENDED rows (§5) are precisely the two whose ported stamper actually runs on
the gameplay route — `External::Load` on world load, `External::Add`. `SaveText`
stamps `??_7FileStream@` and `??_7TextFileStream@` but did not run on that route,
so `post=0`. And `??_7BinaryFileStream@` has **no ported stamper at all**, so its
`post=0` is not route-limited — **on the current tree there is no code path that
could ever open its door.** An independent instrument (CEIL's counter) and an
independent binary fact (Ghidra's xref set) agree row by row, having been
gathered by entirely different means.

**Consequence for lane FILE, and it reorders the work:** promoting
`??_7BinaryFileStream@` requires `??0BinaryFileStream@IO@@QEAA@XZ` — already one
of FILE's 11 rows — and that ctor is its *only* stamper, which makes it the
**cleanest promotion of the five**, not the hardest. `??_7FileStream@` is the
opposite: nine distinct stampers including `Engine::Run`, so TRAP=0 there still
leaves most stamps on retail's side.

**What A.3 does NOT claim.** It does not claim any of the five is unpromotable —
a stamper we have not ported yet is ordinary port work, not a ceiling. It does
not claim the ported stampers listed actually install our table today (they
resolve the symbol; whether the minted row is *promoted* is gated by
`standalone/reg_mint_promoted.cpp`, and none of the five is registered there).
It does not claim Ghidra's xref set is exhaustive — it is the xrefs Ghidra has
found in this project's analysis, and an indirect stamp would not appear.

## A.4 For lane TRAPS and lane FILE — the minted set, deliverable now

TRAPS's readiness screen held 9 IO rows where the plan lists 13; the four missing
are my five minus VisitorStream. **They are minted as of `3620e69` and their trap
counts are measured, not zero:**

| row | VA | slots | ported | TRAP | purecall |
|---|---|---|---|---|---|
| `??_7BinaryFileStream@IO@@6B@` | `0x1801554d0` | 8 | 0 | 8 | 0 |
| `??_7CompressedFileStream@IO@@6B@` | `0x1801555f0` | 9 | 1 | 8 | 0 |
| `??_7FileStream@IO@@6B@` | `0x180154960` | 8 | 0 | 8 | 0 |
| `??_7TextFileStream@IO@@6B@` | `0x180155980` | 20 | 0 | 20 | 0 |
| `??_7VisitorStream@IO@@6B@` | `0x180155a20` | 7 | 0 | 7 | 0 |

All five **BLOCKED**. Re-running the readiness screen on this branch will now show
13 IO rows, not 9. Lane FILE's DATA rows are unblocked for scoping; §4 (the shared
`IO::FileStream` base slots) and A.3 (stamper ownership) are the two facts that
should drive its ordering.

---

# ANNEXE B — CORRECTION (2026-09-10, post-report tranche 2)

Raised by **lane FILE** against my `??_7TextFileStream@` row. **FILE is right.**
I confirmed it from the bytes and the disassembly, and it is **two rows, not
one**. §0–§8 and Annexe A stay sealed; this annexe states what they got wrong.

## B.1 What is wrong, and it is a number I published

`adjacent-export` proves *"the last slot ends exactly where the next EXPORTED
symbol starts."* **That is an UPPER bound and the generator read it as exact.**
Every concrete IO stream constructor heap-allocates an 8-byte `IO::Encoding` and
stamps an **unexported** vtable into it; those tables sit in `.rdata` interleaved
with the exported class tables, so the row before one **swallows it**.

| row | VA | minted | **true** | swallowed |
|---|---|---|---|---|
| `??_7ReaderWriter@IO@@6B@` | `0x1801553b0` | 36 | **31** | the 5-slot Encoding table at `0x1801554a8` |
| `??_7TextFileStream@IO@@6B@` | `0x180155980` | 20 | **10** | the Encoding table at `0x1801559d0` |

**§4 of this document says `??_7TextFileStream@` is 20 slots / 20 TRAP. It is 10
and 10.** The corrected totals for the five rows are **42 slots, 1 ported, 41
traps** (not 52 / 1 / 51), and **31 distinct trap bodies** (not 41) — the ten
"unexported, known by address only" entries I listed for TextFileStream
(`0x1800b9a70`…`0x1800bafe0`) are **not TextFileStream's**; they are the
Encoding's. **A.4's table is wrong in the same cell and lane FILE's row halves.**

`??_7ReaderWriter@` is not mine, but the generator is, and the same fix covers
it. Note the sign: it is **30 of 31 purecall, not 30 of 36**, which makes phase
0a's abstract-base argument *stronger*, not weaker. CEIL §3.4 and WAVE19_IO.md
§3.0a both quote the 36.

Evidence, read as disassembly. **The constructor names the address it stamps**,
which is strictly tighter than adjacency to the next export:

```
1800ba431  ??0TextFileStream   LEA RAX,[0x1801559d0] -> MOV [RSI],RAX
1800a82bb  ??0BinaryFileStream  LEA RAX,[0x1801554a8] -> MOV [RSI],RAX
```

Ghidra's xrefs to `0x1801559d0` are that constructor and only that constructor.
Bytes: `0x1801554a0` is ReaderWriter's last `_purecall`; `0x1801554a8` is
`0x1800a81e0`, the Encoding's slot 0. `runtime/types/io/FileStream.h` has said
10 since Wave 11.

## B.2 Why no instrument caught it — including two of mine

**The shadow is structurally incapable of seeing this.** An over-sized row's
surplus slots are read from the DLL *at their real addresses*, so the shadow
compares them and finds them equal. `598 slot comparisons, 0 MISMATCH` was
**true, and silent on extent**. The shadow can only check rows that exist, at
the addresses they claim; it has no opinion on where a row should stop. Same
family as `reference_shadow_marker_is_not_an_oracle.md`.

**§4.1's arithmetic was not the independent check I called it.** I wrote that
`546 + 52 = 598` "closes by identity, which is a second, independent check that
the five rows and nothing else entered." The *rows* part stands. The *sizes*
part does not: both sides of that sum are the generator's own slot counts, so
the identity is self-consistency and **cannot detect a wrong extent**. Ten of
those 52 were another class's table and the arithmetic closed anyway.

**And I had the symptom in front of me and misread it.** Control D (§3.1)
reported that slot VA `0x1800a81e0` appears in **both** `??_7ReaderWriter@` and
the by-address row at `0x1801554a8`, and I wrote it up as *"the instrument being
right about a shared body, and it is stated here rather than rounded to 1."*
**A body appearing in two minted rows is not sharing — it is one row swallowing
the other.** The control was reporting the defect; I explained it away as a
feature of the control. That is the precise failure
`reference_a_reconciliation_retires_the_flag.md` warns about, committed against
my own instrument's output.

## B.3 The fix, and why it is deliberately NOT an automatic scan

FILE proposed bounding a table by the next **stamped** address seen in any
constructor. Right in principle. **I implemented it and it is unsound as a
scan.** A linear byte scan of `.text` for `48/4C 8D /r` with `mod=00 rm=101`
(`LEA r64,[rip+disp32]`) found **2** rows whose extent contains a stamp target:

* `??_7TextFileStream@` → `0x1801559d0` — **real**.
* `??_7ReaderWriter@` → `0x180155480` — **a decoding artefact.** Ghidra finds
  **no reference** to that address and its bytes are mid-run `_purecall` inside
  ReaderWriter. Acting on it would have cut ReaderWriter to **26 slots, silently
  deleting 5 real ones.**

**An over-sized row is a visible surplus; an under-sized row loses slots with no
symptom.** A linear byte scan cannot tell an instruction from a byte sequence
inside one, so it **manufactures** bounds — and it manufactured one on the very
row FILE had already sized correctly by hand. 1 of 2 findings wrong is not a
rule to ship.

So the fix is two parts (commit `f30bf61`):

1. **`VTABLE_EXTENT`** — a hand-checked override keyed by VA, each entry
   carrying the constructor site that proves it, validated by the generator
   (an override larger than the decode is refused).
2. **A fail-closed OVERLAP REFUSAL** — no minted vtable's extent may contain
   another minted vtable's start. No disassembly, no false positives (two rows
   cannot begin inside one table), and it catches ReaderWriter automatically,
   computing 31 by pure arithmetic — **a third independent route to FILE's
   number**. It does **not** catch TextFileStream, because `0x1801559d0` is
   minted by nobody; the code says so, so the green is never read as "no
   over-sized rows remain".

Controls, each verified to fire before any green was trusted:

| control | required | read |
|---|---|---|
| overrides disabled | refuse, name ReaderWriter | exit 2, computes 31 ✅ |
| bogus override (999 slots) | refuse | exit 2, "999 but only 20 decode" ✅ |
| real run | generate | exit 0 ✅ |

Set-diff of the correction, all three tables: vtable **ENTERED 0 / LEFT 0 /
CHANGED exactly 2**; typeinfo 0/0/0; storage 0/0/0; PARSEFAIL 0. Slot total
**598 → 583 = −10 −5**, the two corrections and nothing else. Build green.

## B.4 What is still open

* **The shadow has NOT been re-run** on the corrected tables. The game dir is
  held by an **unlocked** run from another lane (pid 17292, deployed hash
  `f87c552a`, not my build); per the lock tool's own rule I will not kill a
  process I did not start. Escalated with those numbers. **Until that run,
  §4.1's clean shadow describes the OLD extents.**
* **I have not audited the other 60 rows for this defect by constructor
  evidence.** The overlap check clears them of *swallowing a minted row*; it
  says nothing about swallowing an unminted one. Every `adjacent-export` row in
  the file is a candidate, and there are 31 of them.
* **B.1's corrected trap counts are arithmetic on the new extents**, not a fresh
  readiness run — that needs the lock.

---

# ANNEXE C — CORRECTION TO ANNEXE B (2026-09-10, post-report tranche 3)

Annexe B shipped `??_7ReaderWriter@` at **31**. **It is 26.** Lane IOBASE derived
26 independently, the integrator settled it from the bytes, and I have now
verified it myself. Annexes A and B stay sealed; this states what B got wrong.

## C.1 The .rdata there is FOUR tables, not two

| | VA | slots |
|---|---|---|
| `??_7ReaderWriter@IO@@6B@` | `0x1801553b0` | **26** |
| unexported, all `_purecall` | `0x180155480` | 5 |
| unexported `IO::Encoding` (already minted by address) | `0x1801554a8` | 5 |
| `??_7BinaryFileStream@IO@@6B@` | `0x1801554d0` | 8 |

36 swallows **both** intervening tables; B's 31 swallows **one**. Each has its
own leaf constructor, and the two sit adjacent with a clean boundary — verified
here byte for byte:

```
180023df6  c3                    RET            <- previous function ends
180023df7  cc cc cc cc cc cc cc  int3 padding   <- clean function boundary
180023e00  48 8d 05 a1 16 13 00  LEA RAX,[0x1801554a8]
180023e07  48 89 01 48 8b c1 c3  MOV [RCX],RAX; MOV RAX,RCX; RET
180023e10  48 8d 05 69 16 13 00  LEA RAX,[0x180155480]
180023e17  48 89 01 48 8b c1 c3  MOV [RCX],RAX; MOV RAX,RCX; RET
```

## C.2 I dismissed a TRUE finding from my own scan, on two generic bad grounds

B.3 called the hit at `0x180155480` a **decoding artefact**. It is a genuine,
`ret`/`int3`-aligned leaf constructor. Both reasons I gave were wrong in ways
worth naming:

1. **Ghidra reported "No references found to address: 0x180155480" and I read
   that as ABSENT.** It means **NOT ANALYSED** — `0x180023e10` is undefined
   bytes in the project's database. A silence read as a negative
   (`reference_a_screen_that_cannot_name_its_subject.md`), which is the defect
   this whole wave has been cataloguing, committed while cataloguing it.
2. **"It sits mid-run of `_purecall` INSIDE ReaderWriter"** assumes
   ReaderWriter's extent — the very thing in dispute. Circular. Both intervening
   tables *and* ReaderWriter end in `_purecall`, so **the bytes cannot settle
   this at all**; only the constructors can.

## C.3 The scan scored 2 of 2, and the rule I justified with the false positive still stands — on a different argument

**B.3's central empirical claim is withdrawn.** The `.text` LEA scan did not
produce a false positive: it reported `0x180155480` correctly and **I overruled
it by hand.** The scan has a better record on this DLL than the hand-check that
overruled it.

The `VTABLE_EXTENT` comment in `rt_mint_gen.py` cited that non-existent false
positive as measured evidence for "hand-checked, not automatic". It has been
rewritten. **The rule still stands, but on the asymmetry alone:** an over-sized
row is a visible surplus; an under-sized row loses slots with **no symptom** —
so a bound that DELETES slots must carry a named constructor a human checked.
That argument never depended on the scan being wrong, and I should have rested
it there in the first place.

> A rule justified by a measured false positive that never happened is a rule
> resting on nothing. State the asymmetry, not the anecdote.

## C.4 The sweep — `tools/mint_extent_audit.py`

Annexe B left "31 other `adjacent-export` rows could each be swallowing an
unminted table" as an open risk; the integrator recorded it as the wave's
largest known unknown. **This tool is the screen for it.** It decodes every
RIP-relative LEA in `.text` and reports targets landing strictly inside a minted
row's extent, printing per site whether the LEA is at a `ret`/`int3` boundary
and the three bytes after it (`48 89 01` = `MOV [RCX],RAX`, the ctor stamp
shape), so each candidate is settled by hand. **It never edits an extent.**
Three positive controls print before any verdict and it refuses to report if any
is red.

| run | result |
|---|---|
| against `f30bf61^` (pre-fix) | **4 candidates**: ReaderWriter at both 26 and 31, TextFileStream at both 10 and 15 — including a **third** unexported table at `0x1801559f8` that neither lane FILE nor I had found. 10 and 26 are the tight bounds. |
| against this tip | **CLEAN across all 62 rows**, of which **31** still carry an `adjacent-export` proof — the at-risk denominator, printed. |

**Clean is not proof, and the tool says so in its own output:** a table stamped
other than by a plain RIP-relative LEA is invisible to it.

## C.5 Corrected numbers

`??_7TextFileStream@` **10 slots / 10 TRAP** (B.1 stands). `??_7ReaderWriter@`
**26**, so it is **25 of 26 purecall** — B.1 said 30 of 31, and A/B's chain of
"this strengthens phase 0a" holds either way but the figure to quote is 25/26.
Slot total **598 → 583 → 578**. The five rows of this cell are unchanged by C:
**42 slots, 1 ported, 41 traps, 31 distinct bodies.**

A build error that review did not catch: a multi-line proof string is emitted
**both** as a `//` comment header and as a C string literal, and a raw newline
breaks each differently — escaping in `cstr()` alone fixed the literal and left
the comment broken. Collapsed to one line at the source. Found by compiling,
which is why compiling happens without the lock.

## C.6 Still open

* **The shadow re-run remains OPEN.** The unlocked run is cleared (it was lane
  PERSIST's crashed `Save0`; the integrator terminated it), but lane FILE holds
  the lock and the serialised order is CSTREAM → FILE → **MINT** → PERSIST.
  Until that run, §4.1's clean shadow describes the **original** extents.
* **C.4's clean sweep is a screen, not a proof** — see its stated blind spot.
* **Nothing here changes** the root cause (staleness), the population set-diff,
  the stamper/promotion analysis in A.3, or the identity (`83 >= 122 − 3 = 119`,
  36 short, moved by zero).

---

# ANNEXE D — THE SHADOW RE-RUN, AND IT WAS RED FIRST (2026-09-10)

Closes C.6's open item. Two runs, because the first one **failed**.

## D.1 The corrected extents made the shadow FAIL, and the shadow was not wrong

First re-run on the corrected tables: **`==== FAIL: 2 problem(s) ====`**, both
`SIZE SUSPECT`, on exactly the two rows Annexes B and C had just proved:

```
SIZE SUSPECT ??_7ReaderWriter@:   minted 26 slots but the qword after the last
                                  one (00007fff7ed2ec28) is a DLL pointer
SIZE SUSPECT ??_7TextFileStream@: minted 10 slots but the qword after the last
                                  one (00007fff7ed69ab0) is a DLL pointer
```

**That is the tail probe working exactly as designed, on a premise that no
longer holds.** It asks "is the qword past the last slot a valid image
pointer?" — a good proxy for *truncated* when a table is followed by non-pointer
`.rdata`. But a row bounded by a **ctor-stamped neighbour** is followed by that
neighbour's slot 0, which **is** a DLL pointer **by construction**. So every
correctly-bounded row of this new kind would flag, forever.

**A permanently red screen is a screen nobody reads** — worse than the defect it
was added for, and it would have made the next lane "fix" a correct extent back
to a wrong one to clear the red.

## D.2 The fix inverts the check rather than suppressing it

`standalone/rt_data_mint.cpp` gains a third branch for
`ctor-stamped-neighbour:` proofs. Suppressing would have retired a check; instead
the predicate flips to assert what the bound actually rests on:

> if the tail is **NOT** a DLL pointer, the neighbouring table this bound rests
> on **is not there**, and the extent is the thing to doubt.

Same row, same byte, opposite expectation — because the *reason for the bound*
changed, not the data.

**Control, argued rather than re-run** (three lanes are queued on the lock).
The branch is provably **reached**: had it not been, the old `else` would have
printed its message again, and it did not. Its predicate's subject is
**known-true from the failing run's own output** — that run printed the two tail
values, `00007fff7ed2ec28` and `00007fff7ed69ab0`, and printed them *because*
`DllContains(tail)` was true for both. So the new branch is entered and
evaluates a condition whose input was independently observed. I did not run a
control that forces the new arm to fire, and I am not claiming one.

## D.3 Second re-run — clean

```
ground truth       : 578 slot comparisons vs the live DLL, 0 MISMATCH
minted from OUR    : 270 slots, 0 MISMATCH   <-- the only number that measures the port
==== clean: 0 problem(s) ====
```

Exit 0, 0-byte crash log. **§4.1's clean shadow now describes the CORRECTED
extents.** 598 → 583 → 578 slot comparisons across the three generations.

## D.4 The row table has MOVED — other lanes landed ports while I worked

Measured on this run, not carried from §4:

| row | slots | ported | TRAP | purecall | state |
|---|---|---|---|---|---|
| `??_7BinaryFileStream@` | 8 | 0 | 8 | 0 | BLOCKED |
| `??_7CompressedFileStream@` | 9 | **2** | 7 | 0 | BLOCKED |
| `??_7FileStream@` | 8 | 0 | 8 | 0 | BLOCKED |
| `??_7TextFileStream@` | **10** | 0 | **10** | 0 | BLOCKED |
| `??_7VisitorStream@` | 7 | **7** | **0** | 0 | **READY** |

**42 slots, 9 ported, 33 traps, 25 distinct trap bodies.** §4 said 52/1/51/41 and
B.1 corrected it to 42/1/41/31; the port columns have since moved again.

**`??_7VisitorStream@` is the first of the five to reach TRAP=0**, and it is also
one of the two **ATTENDED** rows (§5, `post=1` via `?Add@External@Object@@`). So
it has both a proven door and a complete slot set — **the strongest promotion
candidate of the five, and the one to take first.** Per my own §4 and
`reference_a_green_readiness_screen_is_not_a_route.md`, **READY is still not
promoted**: it is not registered in `standalone/reg_mint_promoted.cpp`, and
A.3's stamper analysis applies unchanged.

`??_7ReaderWriter@` reads **26 slots, 25 purecall, 1 trap** — confirming C.5's
25-of-26 from the live screen, and a stronger abstract-base signal for phase 0a
than the 30-of-36 that CEIL §3.4 and WAVE19_IO.md §3.0a still quote.

## D.5 What D does not close

The attendance figures in §5 are from the **earlier** run and were not re-taken;
ports have landed since, and attendance measures the *door*, not the slots.
D.4's numbers are one gameplay-route run on this tip.
