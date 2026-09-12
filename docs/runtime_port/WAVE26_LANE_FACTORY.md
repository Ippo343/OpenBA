# WAVE 26 LANE FACTORY

Branch `wave26/factory` off `wave26/base` (`7de42dd1`).

Scope: the named cutover wall of attempt 26, and the size of the family behind it.

---

## 0. THE HEADLINE

**The wall is not one function. It is 206 factory slots, and on `wave26/base`
*every single one of them* was an abort trap. MEASURED, not inferred.**

```
run A (wave26/base behaviour, BANISHED_RT_UISTRTAB=0):
[RtMint] BaseTypeInfo factories: 104 rows x 2 slots = 208;
           0 PORTED, 206 ABORT-TRAP, 2 absent in retail too (no factory name)
run C (this lane's port, default env):
           2 PORTED, 204 ABORT-TRAP, 2 absent in retail too
```

`?CreateNewObject@StringTable@UI@@` — the one name attempt 26 printed — is one of
**104 minted `?_typeInfo@...@` rows**, each carrying **two** factory slots
(`factoryFn` +0x20, `inPlaceFactoryFn` +0x28). Attempt 27 will hit the next one,
attempt 28 the one after. **At one name per boot, enumerating this family costs
~200 cutover attempts.** That is the finding this lane exists to hand forward;
§4 says what to do with it.

### 0.1 A number that disagrees with itself, and which one to believe

The static screen (§2.1) says **2 present**; the live screen (§2.2) said **0
ported** on the same tree state. **Both are right and the live one is the one to
quote.** `Dialog@UI` — the only factory the tree held before this lane — is
registered by `dialog_factory.cpp` **only under the opt-in `BANISHED_RT_DLGFACTORY`,
which is unset by default**. So it is ported in SOURCE and absent from the
REGISTRY, which is exactly the shape
`reference_half_registered_port_reads_as_absent.md` warns about, arriving here
from the opposite direction: a source grep reported a port the running process
did not have.

**Consequence for cutover, and it is not cosmetic:** had this lane ported 100
factories behind opt-in switches, the cutover would have hit all 100 walls anyway
and the static census would have read 100/104. The gate is the REGISTRY, not the
tree.

## 1. PART A — the one function (MEASURED)

### 1.1 What was ported

Three rows, all exported, all verified present in the export directory **by RVA
with pefile**, not asserted from a comment
(`reference_aeaa_is_private_not_unexported.md`):

| symbol | retail VA | ord |
|---|---|---|
| `??0StringTable@UI@@QEAA@XZ` | `0x18011d890` | 421 |
| `?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z` | `0x180002330` | 1979 |
| `?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ` | `0x180002280` | 2116 |

Files:
* `runtime/src/ui/stringtable_factory.cpp` — the three bodies + registration
* `runtime/types/ui/StringTable.h` — extended with the full `UiStringTable`
  layout (0x30 bytes, seven static_asserts)
* `runtime/src/ui/rt_ui_port.cpp` — one `extern` + one call

Kill switch `BANISHED_RT_UISTRTAB`, **default ON**. That differs deliberately
from `dialog_factory.cpp`'s opt-in `BANISHED_RT_DLGFACTORY`: that file is a Wave
11 *experiment*; this one's whole job is to be registered before
`RtDataMintInit()` runs, and a registration that does not happen cannot clear a
trap. `RtUiPortInit()` (`standalone/resolve_standalone.cpp:333`) precedes
`RtDataMintInit()` (`:406`) — that ordering is what makes it work, and it was
read, not assumed.

### 1.2 Absence established before a line was written

`reference_grep_for_the_port_before_writing_it.md` (three lanes in one wave
re-ported what the tree already had) and
`reference_half_registered_port_reads_as_absent.md`. Greps **scoped to the source
roots**, both by NAME and by ADDRESS:

```
grep -rn 'StringTable@UI@@SAPEAVBase'  runtime/ code/ standalone/ engine/ dx11/
    -> standalone/rt_data_mint_tables.inc ONLY (the minted row itself)
grep -rn '18011d890\|180002280\|180002330' <same roots>
    -> NOTHING
```

### 1.3 The bodies

Read off the **disassembly**, not the decompiler
(`reference_ghidra_c_output_vs_disassembly.md`). The full transcription with
per-instruction addresses is in the file banner. Two things worth carrying here:

* **The allocation size is read off the ALLOCATION, not inferred from the
  stores.** `MOV EDX,0x30 / LEA R8D,[RDX-0x28]` at `0x1800022b1` gives size 0x30,
  align 8. The eight ctor stores then cover `0x00..0x2f` with **no hole**, which
  corroborates 0x30 rather than being its source. (A `.pdata` extent would have
  been a *lower* bound — `reference_pdata_is_a_lower_bound_on_function_extent.md`
  — and was not used.)
* **`?CreateNewObject@StringTable@UI@@`'s lock/Allocate/unlock prologue was
  compared instruction-by-instruction against `?CreateNewObject@Dialog@UI@@`'s**
  rather than assumed to match from resemblance
  (`reference_a_written_recipe_can_be_wrong.md`). The only differing bytes are
  the size immediate and the LEA displacement that derives align 8 from it. So
  `dialog_factory.cpp`'s transcription as a call to the out-of-line locked
  wrapper `?MemAlloc@System@@` (`0x1800c7a50`) is exactly right here too.

Retail's two null checks (`0x1800022da` allocation failure, `0x180002341`
caller-supplied null) are **retail's own branches**, reproduced, not guards added
by this lane. The two dead vptr stores (`??_7Base@`, then `??_7External@`, both
overwritten unread by `??_7StringTable@`) are kept, for the reason
`wave24_lane_gfxmat.cpp` states: a port that writes only the final one is a
different function under any interruption, and a byte oracle would not notice.

### 1.4 Verification — the trap goes quiet

See §3. The measurement is a **two-run positive control** on the same exe.

---

## 2. PART B — the family, derived twice (MEASURED)

Two independent derivations, one static and one live, and they are independent in
the way that matters: the static one reads **source text**, the live one reads
**the port registry through the mint's own doors**.

### 2.1 Static derivation — `tools/mint_factory_census.py` (NEW)

Parses `standalone/rt_data_mint_tables.inc` for all 104 minted BaseTypeInfo rows
and their slot-0 factory names, then asks whether that literal appears anywhere
in `runtime/ standalone/ code/ engine/` (1,528 files, the mint table itself
excluded — including it would have turned "102 unported" into "0 unported").

On `wave26/base`, before this lane's port:

```
BaseTypeInfo rows parsed: 104
slot-0 factory NAME present somewhere in the tree:   1   (Dialog@UI)
slot-0 factory NAME absent from the whole tree:    102
rows with NO factory at all (nullptr in retail too):  1   (Base@Object)
```

On this branch, after it: **2 present, 101 absent** (the new row is
`StringTable@UI` → `runtime/src/ui/stringtable_factory.cpp`).

**Two blind spots, both stated.** It is a LITERAL grep, so it is blind to a
registration spelled through a named constant
(`reference_census_cannot_see_nonliteral_registrations.md`,
`reference_literal_grep_blind_to_variables.md`); and a name that appears only in
a **comment** reads as present
(`reference_uncommented_checker_manufactures_work.md`), so "present" here is an
UPPER bound on ported-ness. §0.1 is that upper bound being loose by 100%: it
reported 2 where the running process had 0.

### 2.2 Live derivation — `[RtMint] BaseTypeInfo factories:` (NEW)

Added to `standalone/rt_data_mint.cpp` (`RtMintReportTypeInfoFactories()`, printed
from `RtDataMintInit()`). It does **not** re-implement "is it ported?" — it
re-reads the verdict `MintTypeInfos()` already wrote into the storage block, by
comparing each row's `factoryFn`/`inPlaceFactoryFn` against that row's own trap
stub address. It therefore **cannot disagree with the mint**, which is the point
(`reference_two_checks_one_hidden_premise.md` is about two checks that *share* a
premise and pretend to be independent; this one *declares* that it shares one).

It names its subject explicitly (all 104 rows × 2 slots), because a screen that
cannot name its subject manufactures work
(`reference_a_screen_that_cannot_name_its_subject.md`).

`BANISHED_RT_MINT_FACTORYLIST=1` prints the full roster of unported slots — the
whole family in **one** run instead of one name per cutover attempt.

Measured output: see §3.

### 2.3 The bias check the brief asked for

> *a screen that reports "0 unported factories" would be indistinguishable from a
> dead parser.*

The screen is exercised in **both** directions, on the same build:

* **positive control** — a run with `BANISHED_RT_UISTRTAB=0` (the port compiled
  in but *not registered*). The roster must still name
  `?CreateNewObject@StringTable@UI@@`. §3 run A.
* **negative control** — the default run, port registered. That row must
  **leave** the roster and the trapped count must drop by exactly **2** (both
  slots), not by 1 and not by some other number. Diffing the SET, not just the
  count (`reference_diff_the_set_not_the_count.md`). §3 run B.

The static screen gets the same treatment for free: it reports `Dialog@UI` as
present (a factory known ported) and `StringTable@UI` as absent (a factory known
unported) in the same pass.

---

## 3. THE RUNS (MEASURED)

Three runs, one exe, `md5 cc1a396f5c5ea6fd9ff49f8a053dba63`, **verified against
the deployed copy immediately before each launch** (WAVE26_KICKOFF standing
warning 1). Game-dir lock held by `lane-FACTORY` for all three; acquired
foreground and blocking (waited 330s behind lane ATTEND), never piped, never
backgrounded. Released after run C.

| run | env | `[RtMint] BaseTypeInfo factories` | exit | crash log |
|---|---|---|---|---|
| A | `BANISHED_RT_UISTRTAB=0` (port compiled in, **not registered**) | `0 PORTED, 206 ABORT-TRAP` | 0 | 0 bytes |
| B | `BANISHED_RT_UISTRTAB=2` (registered + effectiveness control) | `2 PORTED, 204 ABORT-TRAP` | 0 | 0 bytes |
| C | default (nothing set) | `2 PORTED, 204 ABORT-TRAP` | 0 | 0 bytes |

All three `BANISHED_AUTOQUIT=1` menu-only runs.

### 3.1 The trap went quiet — and the SET moved, not just the count

`reference_diff_the_set_not_the_count.md`: a count that moves the right direction
by the right amount can still hide a swap. The rosters were diffed as sets:

```
comm -23 rosterA rosterB   (cleared by the port)
    ?CreateInPlaceObject@StringTable@UI@@SAPEAVBase@Object@@PEAX@Z
    ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
comm -13 rosterA rosterB   (regressions)
    <empty>
```

Exactly the two intended rows left, nothing else moved in either direction.
`[RtPort] UI: 209 -> 212 exports registered` (+3: the ctor and both factories).

### 3.2 The screen's bias check, both directions

* **Positive control (run A).** The port is in the binary but unregistered; the
  roster must still name it — and did, at lines 312-313:
  `[RtMint]   [ 92] slot0 0x180002280  ?CreateNewObject@StringTable@UI@@...`.
  A "0 unported" reading from this screen is therefore distinguishable from a
  dead parser, because the screen was HANDED a shape and found it
  (`reference_absence_screen_must_be_handed_a_shape.md`).
* **Negative direction (runs B/C).** The same row disappears when, and only when,
  the registration happens.
* **Static screen (`--self-test`).** Both controls PASS: `Dialog@UI` in the
  present bucket, `SpriteSheet@UI` in the absent bucket.

### 3.3 The body's own effectiveness control (run B)

```
[RtPort] UI::StringTable CONTROL FIRED (attendance 0 -> 1, returned the buffer,
         tail zeroed, vptr=00007fff243b4630)
```

`...4630` is `??_7StringTable@UI@@6B@` (`0x180154630`) at this run's ASLR slide —
so the vptr stamp resolved through `RT_DATA_SLOT` to the real vtable, the tail
`0x08..0x2f` came back zeroed, and `CreateInPlaceObject` returned its argument.

### 3.4 What run B's attendance does NOT say

```
[RtPort] UI::StringTable factory ATTENDANCE: ctor=1 CreateNewObject=0 (null=0)
         CreateInPlaceObject=1 (null=0) control=1
[RtPort] UI::StringTable factory VERDICT: the ONLY entries were the control's own.
```

**This lane's bodies were never entered by the game.** That is EXPECTED with the
DLL present and is stated by the instrument itself rather than left for a reader
to notice: Wave 11 lane OBJINT measured that the DLL reaches its own factories
through an in-module function pointer, which is not a door
(`reference_a_ported_row_can_be_permanently_unpromotable.md`). A zero here is not
"verified" (`reference_uncalled_port_is_not_verified.md`). The port is verified
against the **disassembly** and against the **mint's trap**; it is *not* verified
against a live route, and no run in this lane could have provided one.

## 4. WHAT IT MEANS FOR CUTOVER (INFERENCE, labelled)

**This wall is 204 remaining function bodies across ~102 classes, not one, and
Wave 27's right answer is a GENERATOR, not a porting lane.** The brief asked for this verdict explicitly and
the measurement supports the strong form of it.

The inference, stated so it can be attacked:

1. **The bodies are a family, not a hundred unrelated functions.** Every
   `CreateNewObject` read so far is the same six-line shape: locked `MemAlloc(N,
   8)`, retail's null check, an inlined ctor, return. Every `CreateInPlaceObject`
   is the same four-line shape. The *variable* parts are (a) the allocation size,
   (b) the vptr sequence the inlined ctor stamps, (c) the zeroed field extent —
   all three of which are **readable from the disassembly mechanically**.
2. **Therefore the marginal cost per row is dominated by the ctor, not the
   factory.** `StringTable`'s ctor was 8 stores. `MaterialInstance`'s (already
   ported, `wave24_lane_gfxmat.cpp`) was 16. A generator that emits the factory
   pair and leaves the ctor as a named hole would close the *factory* half of all
   206 slots and leave a much smaller, much more uniform residue.
3. **The caveat that keeps this honest.** Wave 11 lane OBJINT measured that
   porting and registering a factory does **not** put our body on the DLL's
   creation path while the DLL is present — the DLL reaches its own factories
   through an in-module function pointer stamped by a static initialiser, and
   that is not a door
   (`reference_a_ported_row_can_be_permanently_unpromotable.md`). So **every one
   of these 204 rows will register with attendance 0 in an ordinary run.** That
   is expected, not a defect, and it must not be read as "ported and verified"
   (`reference_uncalled_port_is_not_verified.md`). The value of porting them is
   *entirely* at cutover, where the minted BaseTypeInfo is the only node there
   is. A lane that ports fifty of these and reports fifty green attendance zeros
   has measured nothing.
4. **Consequence for how Wave 27 should be scoped.** Not "port N factories" —
   that population is real but unverifiable one row at a time. Scope it as: a
   generator that emits all 206 slots from the disassembly, plus **one** cutover
   attempt that either passes this wall entirely or names a wall of a *different
   shape*. The pass/fail of the whole generator is a single boot.
5. **And it must register by DEFAULT.** §0.1 is the trap waiting for that lane:
   `Dialog@UI` has been ported since Wave 11 and contributed **nothing** to the
   live count because its switch is opt-in. A generator that emits 204 bodies
   behind opt-in switches would move the static census to 104/104 and the cutover
   wall not at all.

---

## 5. WHAT THIS LANE DID **NOT** DO

* It did **not** verify the ported bodies against a live route. There is none
  with the DLL present (§4.3), and this lane did not fabricate one. The
  `BANISHED_RT_UISTRTAB=2` effectiveness control exercises
  `CreateInPlaceObject` on a stack buffer and checks the returned pointer and the
  zeroed tail — that is a control on the *instrument*, **not** an oracle on the
  body.
* It did **not** run a cutover attempt. That is lane ATTEND's.
* It did **not** port the other 204 slots.

---

## 6. RESIDUALS — each with an ADDRESSEE

An expiry clause naming a condition but no addressee never fires
(`reference_expiry_clause_needs_an_addressee.md`), so each of these names who.

1. **To the WAVE 27 INTEGRATOR, before scoping a factory lane:** re-derive the
   number, do not quote this document's. One command, no build:
   `python tools/mint_factory_census.py --self-test --expect-unported
   "?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ"`. If the
   `--expect-unported` subject has since been ported the control will FAIL loudly
   rather than pass vacuously — pick another absent row from the roster and say
   which in the wave doc.
2. **To LANE ATTEND, on attempt 27:** the wall should now move OFF
   `StringTable@UI` (measured: its two slots are the only ones that left the
   roster). If attempt 27 dies on a `[RtMint] FATAL: minted BaseTypeInfo::
   factoryFn is not ported` naming *any other* symbol, that is this lane's
   prediction coming true, not a regression. Add `BANISHED_RT_MINT_FACTORYLIST=1`
   to `tools/cutover_attempt26.sh` and the whole remaining roster lands in one
   log instead of one name per attempt.
3. **To whoever next touches `dialog_factory.cpp`:** its opt-in
   `BANISHED_RT_DLGFACTORY` is now the ONLY reason a factory ported nine waves
   ago reads as unported to the live screen. That was correct while the file was
   a Wave 11 experiment. It is a cutover blocker now. Flipping it to default-on
   is a one-line change this lane deliberately did NOT make, because it would
   have moved a second variable inside the same A/B that validates this port's
   own screen (`reference_probe_needs_an_effectiveness_control.md`).
4. **To this lane's reviewer:** the three ported bodies have **attendance 0 from
   the game** and there is no route to give them one with the DLL present (§3.4).
   If that is not an acceptable verification standard for a merge, the correct
   response is to hold the merge, not to invent a route — and the right place to
   get one is a cutover boot, which is lane ATTEND's.

---

## 7. CORROBORATION AND ONE CAVEAT FROM LANE ATTEND (not this lane's measurement)

Lane ATTEND ran cutover attempt 27 on `wave26/base` **without** this lane's port
(`docs/runtime_port/WAVE26_LANE_ATTEND.md`, `wave26/attend` `813155ec`). Reported
to this lane, and recorded here as THEIR measurement, not mine:

* **The wall is stable.** Reproduced 3 times across 2 exes, identical symbol.
  `BANISHED_RT_MAINLOOPFRAME=0` is not load-bearing for it (own control arm).
* **CAVEAT, and it narrows this lane's claim.** Without `BANISHED_RT_MINT=1` the
  boot never reaches the factory wall at all — it dies earlier, in
  `Game::LoadInputNameTables → ExternalFactory::FindOrLoad → RtObject_External_Load`,
  on `??_7FileStream@IO@@6B@` / `??_7CompressedFileStream@IO@@6B@` resolving NULL.

So the honest statement of this lane's scope is: **the 206-slot family is a wall
for a cutover that publishes the minted rows.** The blanket `BANISHED_RT_MINT=1`
is what publishes them today, and WAVE26_KICKOFF §"AN OPEN DISCREPANCY" plus
integrator item **C3** propose not using it at cutover. If C3 wins, this family
stops being the frontier until ~104 blanket-only mint rows get a publisher — and
it becomes the frontier again the moment they do. **The work is not wasted under
either outcome; only its position in the queue changes.** Note also that the §2.2
screen is unaffected either way: `MintTypeInfos()` fills the storage block
unconditionally, so the roster prints in an ordinary run with no mint switch set
(runs A/B/C above set none).
