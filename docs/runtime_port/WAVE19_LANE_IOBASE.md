# WAVE 19 lane IOBASE — the abstract bases, PackageFile, Modification, VisitorStream

Worktree `C:/code/GIT/wt-w19-iobase`, branch `wave19/iobase`, cut from
`wave19/base` @ `e06c162`.

**Every number below was measured in this worktree, on this branch.** Where a
figure is compared against phase 0c it is compared as a **set of named symbols**,
never as a count. No percentage appears anywhere in this document.

Raw evidence: `docs/runtime_port/wave19_iobase/run{1,2_armed,3_ctrl,4_iobase_off,
5_dlgsettings,6_final}/` — each with its complete `standalone_debug.log`,
`standalone_error.log` and `standalone_crash.log`. The runner is
`docs/runtime_port/wave19_iobase/run.sh`, called under
`tools/game_dir_lock.sh run lane-IOBASE`, with `assert-held` gating each deploy.

---

## 0. Headline

Ten rows in, **six rows out**, and the six are named:

```
??_7BinaryReader@IO@@6B@      ??_7BinaryWriter@IO@@6B@    ??_7MemoryStream@IO@@6B@
??_7VisitorStream@IO@@6B@     ??0VisitorStream@IO@@QEAA@XZ  ??1Modification@IO@@QEAA@XZ
```

plus a **bonus row not in the plan's 36**, `??_7Visitor@IO@@6B@`, which is now
READY and PROMOTED and cost nothing extra.

```
IDENTITY      before (e06c162):   83 >= 122 - 5 = 117      34 SHORT
              after  (this tip):  90 >= 123 - 5 = 118      28 SHORT
```

**Four rows did NOT land**: `??_7PackageFile@`, `?_typeInfo@PackageFile@`,
`??1PackageFile@`, `?SetupFileSystem@Modification@`. §7 says exactly what is
left in each and why.

The scoping the brief asked for turned out to be right and then some: **14
distinct bodies cleared 19 trap slots across 8 rows**, and the leverage was not
where the TRAP column said it was.

---

## 1. Bodies closed, and slots thereby cleared

Reported per BODY, as instructed, not per row and not per trap. All read as
**disassembly** from the shipped `Runtime-x64.dll` (`objdump -d`), never from
Ghidra's C (`reference_ghidra_c_output_vs_disassembly.md`). Ghidra MCP was up on
8089 and was **not needed** for any of these — the bodies are small enough to
read whole.

Implementation: `runtime/src/io/iobase_vtables.cpp`, one file, one switch
`BANISHED_RT_IOBASE` (default-on, `=0` kills it), wired into `RtIoPortInit`.

| # | body | VA | door(s) registered | trap slots it cleared |
|---|---|---|---|---|
| 1 | `?GetSource@Stream@IO@@UEBA?AVString@System@@XZ` | `0x18000c6a0` | name + addr | **6** (slot 1 of Stream, MemoryStream, CompressedStream, CompressedMemoryStream, CompressedFileStream, VisitorStream) |
| 2 | MemoryStream vector deleting dtor | `0x1800242a0` | addr (unexported) | 1 |
| 3 | BinaryReader vector deleting dtor | `0x180023a10` | addr (unexported) | 1 |
| 4 | Visitor vector deleting dtor | `0x18002b710` | addr (unexported) | 1 |
| 5 | VisitorStream vector deleting dtor | `0x18002bf80` | addr (unexported) | 1 |
| 6 | `??1BinaryReader@IO@@UEAA@XZ` | `0x1800a82f0` | name + addr | 0 (inlined into #3) |
| 7 | `??1Visitor@IO@@UEAA@XZ` | `0x1800bac90` | name + addr | 0 (inlined into #4) |
| 8 | `??1VisitorStream@IO@@UEAA@XZ` | `0x1800bb040` | name + addr | 0 (inlined into #5) |
| 9 | `?GetSize@VisitorStream@IO@@UEBAIXZ` | `0x1800baff0` | name + addr | 1 |
| 10 | `?GetCurrentPos@VisitorStream@IO@@UEBAIXZ` | `0x1800bb000` | name + addr | 1 |
| 11 | `?SetCurrentPos@VisitorStream@IO@@UEAA_NH@Z` | `0x1800bb010` | name + addr | 1 |
| 12 | `?Read@VisitorStream@IO@@UEAA_NHPEAX@Z` | `0x1800bb020` | name + addr | 1 |
| 13 | `?Write@VisitorStream@IO@@UEAA_NHPEBX@Z` | `0x1800bb030` | name + addr | 1 |
| 14 | `??0VisitorStream@IO@@QEAA@XZ` | `0x1800bb090` | name + addr | 0 (a CODE row, not a slot) |
| 15 | `??1Modification@IO@@QEAA@XZ` | `0x1800b0470` | name + addr | 0 (a CODE row, not a slot) |

**14 bodies, 19 trap slots cleared, across 8 rows** (five of mine, three of lane
CSTREAM's / lane FILE's).

### 1.1 The one measured surprise in the scoping

**`?GetSource@Stream@IO@@` is the sole trap in SIX rows, not four.** Phase 0c
named four; that count PREDATES phase 0b's mint fix, which added
`??_7CompressedFileStream@` and `??_7VisitorStream@`. Verified by VA in
`standalone/rt_data_mint_tables.inc` at lines 785, 800, 815, 1223, 1399, 1569.
`reference_holdback_blockers_are_stale_check_them_first.md` again, on a figure
that was four hours old.

### 1.2 It moves the identity by ZERO, and my first explanation of WHY was wrong

I wrote in `iobase_vtables.cpp`'s header that registering `GetSource` "adds +1
to `called` AND +1 to `ported` in the same step". **Measured afterwards, it adds
nothing to either column.** `runtime_census.py` builds its namespace table only
from symbols that have a **call door** in our tree; a symbol registered but
never resolved is excluded from the table entirely and appears under *"Ports
with no caller in our tree (234 exports, 371 addresses)"*, UNTRIAGED.

The conclusion (zero identity movement) survives; the mechanism I asserted did
not. **"+1/+1" and "0/0" are different claims about the instrument and only one
is true.** The header is corrected in place, with the first draft's error kept
visible rather than silently replaced.

The same correction covers bodies 6, 7, 8 and 9–13: **eight of my fifteen
registrations are caller-less ports that move no count.** That also means the
census's own rule bites them — a port with no caller is not verified by a clean
diff run (`reference_uncalled_port_is_not_verified.md`). What exercises them is
that the mint slots they fill get dispatched; §5 is what I have on that and §8
says what it does not cover.

### 1.3 ~MemoryStream was already ported and I nearly rewrote it

`compressedstream.cpp:657` holds `RtIo_MemoryStream_Dtor`, registered under both
doors at `:963-965`, **with a diff oracle**. My grep did not find it: I searched
the mangled name `??1MemoryStream@IO@@UEAA@XZ` and the VA `0x1800ae890` and got
only comments, because the *body* is spelled `RtIo_MemoryStream_Dtor` in a file
whose name says CompressedStream. **The LINKER caught it** (`multiple definition
of RtIo_MemoryStream_Dtor`), not review and not the grep —
`reference_grep_for_the_port_before_writing_it.md`, fourth instance.

The deleting destructor now reaches lane W's body through the export door, so
there is one implementation. Lane W predicted this exact gap in a comment at
`compressedstream.cpp:966-971`: *"slot 0 is the DELETING destructor thunk
(0x1800242a0), NOT this body, so registering `??1` does not change what a
virtual `delete` dispatches to."* Body #2 is what that note was waiting for.

### 1.4 The one deliberate divergence from the shipped bytes

The `flags & 2` (`delete[]`) arm of all four vector deleting destructors is a
**named abort**, not a port. ENC-2 took that position for BinaryWriter and gave
its reason for BinaryWriter only; I re-derived it per class rather than
inheriting it, because `reference_a_written_recipe_can_be_wrong.md` is precisely
a good lane's dtor recipe being wrong.

I disassembled the helper rather than assuming it: `0x18007ec98` is
`__ehvec_dtor(base, size, count, dtor)` and its loop walks from the LAST element
to the FIRST, destroying **every** one. So the arm cannot be collapsed to a
single destructor call — which is exactly the swap that memory warns about — and
since no producer of any of the four classes lays down an array cookie, the eight
bytes at `this-8` on that path are heap metadata, so continuing would loop an
arbitrary number of times over arbitrary memory. Aborting with a message naming
what would have to have happened is the honest disposition. This is the only
non-byte-faithful code in the file.

---

## 2. Rows moved, as a set-diff against phase 0c

Diffed against **WAVE19_PHASE0C_TRAPS.md §4.1, measured at `ebbec6e`** — not
against CEIL's inherited table. `slots / ported / TRAP / purecall`, from
`BANISHED_RT_MINT_SHADOW=1` on a live boot, both the `init` and the `exit` pass,
identical in both.

| row | phase 0c @ `ebbec6e` | this tip | mine? |
|---|---|---|---|
| `??_7BinaryReader@IO@@6B@` | 26 / 25 / **1** / 0 BLOCKED | 26 / 26 / **0** / 0 **READY [PROMOTED]** | yes |
| `??_7Visitor@IO@@6B@` | 26 / 25 / **1** / 0 BLOCKED | 26 / 26 / **0** / 0 **READY [PROMOTED]** | bonus |
| `??_7MemoryStream@IO@@6B@` | 8 / 6 / **2** / 0 BLOCKED | 8 / 8 / **0** / 0 **READY [PROMOTED]** | yes |
| `??_7VisitorStream@IO@@6B@` | **UNMINTED — UNMEASURED** | 7 / 7 / **0** / 0 **READY [PROMOTED]** | yes |
| `??_7BinaryWriter@IO@@6B@` | 26 / 26 / **0** / 0 READY | unchanged, **[PROMOTED]** | yes — §5 |
| `??_7Stream@IO@@6B@` | 7 / 1 / **1** / 5 BLOCKED | 7 / 2 / **0** / 5 READY, **NOT promoted** | §2.1 |
| `??_7CompressedStream@IO@@6B@` | 9 / 1 / **8** / 0 | 9 / 2 / **7** / 0 | lane CSTREAM's |
| `??_7CompressedMemoryStream@IO@@6B@` | 9 / 3 / **6** / 0 | 9 / 4 / **5** / 0 | lane CSTREAM's |
| `??_7CompressedFileStream@IO@@6B@` | **UNMINTED — UNMEASURED** | 9 / 2 / **7** / 0 | lane FILE's |
| `??_7PackageFile@IO@@6B@` | 9 / 5 / **4** / 0 | **unchanged** | mine, NOT DONE |

Phase 0c's three named expectations all reproduce as its starting values, and the
four rows it did not move are unchanged.

### 2.1 `??_7Stream@IO@@6B@` is now READY *and* still UNPROMOTABLE(NO-OP)

Body #1 filled its last trap. **It is deliberately NOT promoted and its phase-0a
tag is untouched.** A green readiness row says the table is SAFE TO STAMP, not
that anything dispatches it (`reference_a_green_readiness_screen_is_not_a_route.md`);
lane NOOP proved from the shipped bytes that nothing dispatches while this table
is installed, and READY does not bear on that argument at all. Promoting it would
change which table object is briefly written into a `+0x00` that nothing reads.

This is worth stating because a future lane reading only the readiness screen
would see `READY ??_7Stream@IO@@6B@` and reasonably reach for it.

---

## 3. Controls

### C1 — the positive control, scored on APPLICABLE cases only

`BANISHED_RT_IOBASE=0` (run4) removes exactly this lane's bodies. Every row the
switch touches falls back to **exactly** its phase-0c value:

| row | armed (run2) | `RT_IOBASE=0` (run4) | phase 0c |
|---|---|---|---|
| `??_7BinaryReader@` | 26/26/0 | 26/25/**1** | 26/25/1 ✔ |
| `??_7Visitor@` | 26/26/0 | 26/25/**1** | 26/25/1 ✔ |
| `??_7MemoryStream@` | 8/8/0 | 8/6/**2** | 8/6/2 ✔ |
| `??_7Stream@` | 7/2/0 | 7/1/**1** | 7/1/1 ✔ |
| `??_7CompressedStream@` | 9/2/7 | 9/1/**8** | 9/1/8 ✔ |
| `??_7CompressedMemoryStream@` | 9/4/5 | 9/3/**6** | 9/3/6 ✔ |
| `??_7CompressedFileStream@` | 9/2/7 | 9/1/**8** | (unminted then) |
| `??_7VisitorStream@` | 7/7/0 | 7/0/**7** | (unminted then) |

**8 applicable rows, 8 moved, by the mechanism the switch names, in the predicted
direction — GREEN.** The other 54 rows of the 62 are unchanged and are scored
SEPARATELY as a specificity result, not folded into the control's denominator
(`reference_union_control_hides_a_blind_screen.md`).

### C2 — the promotion control

`BANISHED_RT_MINT_PROMOTED=0` (run3): the same five rows read READY with **no
`[PROMOTED]` tag**. So the tags in run2/run6 come from this file's registrations
and not from the readiness screen inventing them.

### C3 — the attendance control

`[W17Attend] CONTROL ?_singleton@MainLoop@Engine@@1PEAV12@EA: init=1 post=N ->
GREEN` printed in **all five** attendance runs before any table. Every `post=0`
below is therefore a finding and not a dead instrument.

### C4 — whole-screen ground truth, quoted with its denominator

Run6, exit phase: **62 vtables / 598 slots · 598 of 598 slot comparisons against
the live DLL, 0 MISMATCH · 272 of 450 non-purecall slots carry our own code ·
31 of 62 complete · 0 slot-count suspects · 0 symbols absent from the DLL ·
4 rows carried by address, for which the export-location cross-check does not
apply and was NOT counted as a pass.** `==== clean: 0 problem(s) ====`.

Every run: `exit 0`, **0-byte crash log**, `[Shutdown] done`, and **zero
`RtMintTrap` firings in stderr** across all six runs.

### A hazard this lane names but does not fix

Run4 prints `BLOCKED … [PROMOTED]` for four rows. `BANISHED_RT_IOBASE=0` removes
the bodies but **not** the promotions, so that combination publishes a
trap-carrying table. It did not fire a trap and did not crash here — but it is
the standing shape for every promoted row whose lane switch is off, and a future
A/B that sets `RT_IOBASE=0` should know it is not a clean control for the
promotion half.

---

## 4. Attendance — what actually runs, per route

`BANISHED_W17_ATTEND=1`. **`post` counts DOOR OPENS after
`AddressRegistryInit`, not dispatches, and `post==1` is not "ran once"** — nearly
every consumer caches in a function-local static.

| row | run1 gameplay | run2/run6 gameplay + a **completed save** | run5 dlgsettings |
|---|---|---|---|
| `??_7MemoryStream@IO@@6B@` | ATTENDED `post=1` | ATTENDED `post=3` | ATTENDED `post=1` |
| `??_7VisitorStream@IO@@6B@` | ATTENDED `post=2` | ATTENDED `post=2` | ATTENDED `post=2` |
| `??_7Stream@IO@@6B@` | ATTENDED `post=4` | ATTENDED `post=7` | — |
| `??_7BinaryReader@IO@@6B@` | never `post=0` | never `post=0` | never `post=0` |
| `??_7BinaryWriter@IO@@6B@` | never `post=0` | never `post=0` | never `post=0` |
| `??_7Visitor@IO@@6B@` | **not in the instrument's 62-row subject — UNMEASURED** | ″ | ″ |
| `?_typeInfo@PackageFile@IO@@` | never `post=0` | never `post=0` | — |

**Two of the five promoted rows are demonstrably live** on the route they were
measured on. One is demonstrably not. For one the instrument has no opinion at
all, which is an **unmeasured cell, not a green cell**.

**Run5 is UNMEASURED, not `post=0`.** `BANISHED_TOOL_HARNESS=dlgsettings` +
`BANISHED_LIVE_TOOLS=1` was chosen because `tool_harness.cpp` drives
`?SaveSettings@DialogInstance@UI@@`, the one route in the tree that ENC-2 names
as building a BinaryWriter. It **did not fire**, and said so itself:

```
[DlgSettings:route] no entity with a UIComponent in this world
(searched tid=0x988e1422 over 23002 entities) -- the route has no subject here
```

I am recording that as a route that exists and has no subject on an autostart
world, not as evidence about BinaryWriter.

---

## 5. `??_7BinaryWriter@IO@@6B@` — the verdict the brief asked for

**ATTENDABLE IN PRINCIPLE · NOT ATTENDED ON ANY ROUTE THIS LANE DROVE ·
PORTABLE AND COMPLETE · NOT permanently unpromotable · PROMOTED, on a
deliberately weaker warrant than the other four.**

### 5.1 The measurement, with its denominator

`26 / 26 / TRAP=0`, reproduced on **six** independent readiness passes this lane
(init and exit of run1, run2, run6), and confirmed unchanged by run3 and run5.
Phase 0c confirmed the same at `ebbec6e`; CEIL first reported it at `3da1503`.
Its door reads `post=0` on **four measured routes**, with the C3 control GREEN on
each.

### 5.2 The strongest thing measured, and it is worth more than it looks

**A full, successful in-game save does not open this door.**

```
run2:  [SaveTest] frame 961: SAVE OPERATION COMPLETE (61 frames elapsed)
run6:  [SaveTest] frame 361: SAVE OPERATION COMPLETE (61 frames elapsed)
       [W17Attend] never init=0 post=0   ??_7BinaryWriter@IO@@6B@
       [Encoding]  ATTENDANCE: none of the four bodies was entered in this run.
```

Two independent instruments agree, and the second is not mine. That settles
something an estimate would have got wrong: **the save path does not run an
`IO::Encoding` BinaryWriter codec.** Anyone scoping "attend BinaryWriter by
saving" would have burned a day.

### 5.3 Why it is NOT the permanently-unpromotable shape

A minted row can be ported in full and never promotable, because we intercept
only at **doors** and an in-DLL `rel32` call or RIP-relative `LEA` stamp is not
one (`reference_a_ported_row_can_be_permanently_unpromotable.md`). **That is not
this row.** All three bodies that install this table are ours and all three stamp
it through a NAME door:

| body | our port | how it stamps |
|---|---|---|
| `??0BinaryWriter@IO@@` | `RtIo_BinaryWriter_Ctor`, `encoding.cpp:259` | `VtblBinaryWriter()` = `ResolveExport("??_7BinaryWriter@IO@@6B@")` |
| `??1BinaryWriter@IO@@` `0x1800a8980` | `RtIo_BinaryWriter_Dtor`, `binary_writer.cpp:187` | same |
| slot-0 deleting dtor `0x180023b50` | `RtIo_BinaryWriter_DeletingDtor`, `binary_writer.cpp:222` | via the above |

So **any** route that constructs a BinaryWriter arrives at whichever table the
promotion selects. What is missing is a **driver**, not an interception point.

### 5.4 Why I promoted it anyway, stated as the weaker claim it is

If nothing ever dispatches it, promoting changes nothing. If something does, it
changes from the DLL's table to one with no traps. Neither is a regression, and
leaving it unpromoted does not verify it — it only keeps the 26 slots
unreachable. That is lane SS's SpriteSheet argument and lane VRES's ImageBuffer
argument, and it is **weaker** than the warrant for `??_7MemoryStream@` and
`??_7VisitorStream@`, which have an observed door open. The distinction is
written into `reg_mint_promoted.cpp` beside the row so it cannot be quoted
without it.

**A reader who wants this row attended should chase the driver, not the port.**
The two candidates I would name: a route that gets a `UIComponent`-bearing entity
in front of `BANISHED_TOOL_HARNESS=dlgsettings`, and
`?Serialize@PackageFile@IO@@` (`0x1800b60a0`), which dispatches the stream
Encoding's `CreateReader` at `+0x18` or **`CreateWriter` at `+0x20`** on
`direction != 0` — i.e. package compilation writes through a BinaryWriter. The
second is §7's unfinished work, so the two open items are related.

---

## 6. Registration and doors

Exported bodies get **both** doors; the four unexported deleting destructors get
the address door only — which is also the door their mint slots use
(`RT_MINT_UNNAMED` on each VA), so registering the address is exactly what turns
the slot from a trap into the body.

Every mangled name was checked against the shipped DLL's export directory before
it was written; a name the DLL does not contain resolves NULL silently
(`reference_resolveexport_phantom_names.md`). `AEAA`/`AEBA` were treated as
PRIVATE, not unexported (`reference_aeaa_is_private_not_unexported.md`) — none of
my rows needed an address-only door on that account.

`??1Modification@` publishes into the **exported** `?_singleton@Modification@IO@@`
slot via `RT_DATA_SLOT`, not into storage of its own
(`reference_port_must_reproduce_side_effects.md`). That works because the
singleton is tagged UNPROMOTABLE(HARMFUL), so the door hands back the DLL's slot
— the tag is load-bearing here, not bookkeeping. Its `~Array<Mod>` calls go
through the registry-first address door onto lane MOD's existing port at
`modification.cpp:573`.

---

## 7. What did NOT land, and exactly what is left

Four of my ten rows are unfinished. They are one cluster and they are all
`IO::PackageFile` and its neighbour, which I under-scoped: the class is `0xf0`
bytes of refcounted Strings, Arrays and Sets, and its destructor alone is 87
instructions with five unported callees (`package_file.cpp:925` already said so).

| row | state | what is left |
|---|---|---|
| `??_7PackageFile@IO@@6B@` | 9 / 5 / **TRAP=4**, unchanged | four bodies: slot-0 deleting dtor `0x1800259d0`, `?Serialize@` `0x1800b60a0`, `?Reconstruct@` `0x1800252d0`, `?Destruct@` `0x180025300` |
| `??1PackageFile@IO@@UEAA@XZ` | not ported | `0x1800b4540`, 87 instructions, 5 unported callees |
| `?_typeInfo@PackageFile@IO@@0…` | not promoted | READY with `idcmp=yes`; door `post=0` on every route measured |
| `?SetupFileSystem@Modification@IO@@` | not ported | `0x1800aebb0`, ~2600 bytes / several hundred instructions to `0x1800af5d5` — **a lane on its own, not a tail** |

Two of the four PackageFile bodies are nearly free once the first lands:
`?Destruct@` is `this->vtable[0](this, 0)` (three instructions) and
`?Reconstruct@` is that followed by a tail jump to `??0PackageFile@` `0x1800b4440`
— both are **blocked on the slot-0 deleting destructor**, which is blocked on
`??1PackageFile@`. So the cluster is one real body and three cheap ones, and a
follow-up should scope it as **1 + 3**, not 4.

`?Serialize@PackageFile@` is separately worth doing because it is §5.3's second
candidate driver for BinaryWriter.

I did not start any of these rather than half-start them.

---

## 8. What this document does NOT claim

* **It does not claim any promoted row is DISPATCHED.** READY means safe to
  stamp. `post>0` means a **door opened**, not that the vtable was dispatched
  (phase 0b's definition, and it binds here). Two rows have an open door, one
  provably does not, one is unmeasured.
* **It does not claim `??_7Visitor@IO@@6B@` is unattended.** It is **absent from
  the attendance instrument's 62-row subject** and therefore UNMEASURED. I did
  not extend the instrument.
* **It does not claim the dlgsettings route says anything about BinaryWriter.**
  The route did not fire; its own line says the world had no subject.
* **It does not claim the MODS route was tested.** It was not attempted at all:
  CLAUDE.md and `wave17_modsroute_crash.txt` record it crashing in
  `ModDialog_ToggleMod_Impl+0x43`, and I neither re-tested nor fixed that.
  My `??1Modification@` port touches the same subsystem and is therefore
  **unexercised on the one route that would stress it hardest**; what does
  exercise it is `engine_lifecycle.cpp:997` at shutdown, which ran in all six
  runs with `[Shutdown] done`.
* **It does not claim mod state was held constant across the comparison** beyond
  not touching it. I mounted and unmounted nothing; a save made with mods
  mounted stalls once the `.pkm` move
  (`reference_mods_off_breaks_save_loading.md`), and this lane did not go near
  that.
* **It does not claim the eight caller-less registrations are VERIFIED.** §1.2:
  a port with no caller in our tree is not verified by a clean run. Their
  evidence is indirect — the mint slots they fill went from TRAP to ported, no
  trap fired in six runs, and the two rows carrying them have an open door.
  **That is weaker than a diff oracle and I did not build one.**
* **It does not compare any ported body against the DLL's behaviour.** There is
  no `RT_DIFF` oracle in this lane. The `598 of 598, 0 MISMATCH` line compares
  minted **addresses** against the DLL's table, not behaviour, and the ~250
  `RT_DIFF_SHADOW` sites elsewhere in the tree are self-checks that never consult
  the DLL (`reference_shadow_marker_is_not_an_oracle.md`). **`runtime/src/io/
  file_stream_probe.cpp` deliberately resolves through `ResolveExportRaw`, so it
  screens RETAIL's VisitorStream constructor, not mine** — I identified it as an
  available oracle skeleton and did **not** build the ours-side pass. That is the
  single largest gap in this lane's verification and the cheapest thing a
  follow-up could add.
* **These counts hold under DEFAULT FLAGS only.** C1 changes eight rows by
  design. A lane running any `BANISHED_RT_*=0` will correctly measure something
  else.
* **It does not re-derive phase 0a or phase 0b.** Both are taken as given, and
  `??_7Stream@`'s and `??_7ReaderWriter@`'s NO-OP tags are left exactly as
  landed.
* **It does not measure performance.** Nothing here was run under `sa-stress`.

---

## 9. Effect on the identity, and the set-diff

```
before  e06c162 :   83 >= 122 - 5 = 117      34 SHORT
after   this tip:   90 >= 123 - 5 = 118      28 SHORT
```

**LEFT the called-but-not-ported set (6, `ported` +7 minus the one new row):**

```
??_7BinaryReader@IO@@6B@
??_7BinaryWriter@IO@@6B@
??_7MemoryStream@IO@@6B@
??_7VisitorStream@IO@@6B@
??0VisitorStream@IO@@QEAA@XZ
??1Modification@IO@@QEAA@XZ
```

**ENTERED `called` (1), and it entered `ported` in the same step:**

```
??_7Visitor@IO@@6B@        <- iobase_vtables.cpp's VtblVisitor() is its only door
```

Net effect on the gap: **-6**. Nothing else entered `called`, and nothing entered
the unported set (`comm` over the two sorted symbol lists, 39 rows before,
33 after).

The eight caller-less registrations of §1.2 moved **neither** column, by
measurement.

**No percentage is quoted anywhere in this document and none should be derived
from it** — `called` sits in both halves of the ceiling fraction
(`reference_the_ceiling_percentage_is_a_moving_target.md`), and this lane grew
`called` by one.

---

## 10. For the other Wave 19 lanes

* **Lane CSTREAM / lane FILE**: `?GetSource@Stream@IO@@` is DONE, registered
  under both doors in `runtime/src/io/iobase_vtables.cpp`. Slot 1 of
  `??_7CompressedStream@`, `??_7CompressedMemoryStream@` and
  `??_7CompressedFileStream@` is no longer a trap; **do not port it again.** Your
  re-measured starting points are in §2 and §3's control table.
* **Lane FILE**: `??_7VisitorStream@` was one of your five unminted rows in
  phase 0b's accounting; it is now READY and PROMOTED and is out of that set.
  `??_7FileStream@` (8/0/8) and `??_7TextFileStream@` (20/0/20) are untouched.
* **Anyone reading the readiness screen**: `??_7Stream@IO@@6B@` now prints READY
  and must stay UNPROMOTABLE(NO-OP). §2.1.

---

# 11. POST-REPORT TRANCHE — re-tasked by the integrator after §0–§10 were sealed

Added **after** this lane reported at `7c88c2b`. **Nothing above §11 is altered**
(§6.16: sealing forbids altering, not annotating). No verdict in §0–§10 moves;
§11.2 **corrects one enumeration inside §5.3** and says so explicitly, and
§11.1 **corrects the correction I was sent.**

Scripts: `docs/runtime_port/wave19_iobase/heads2.py`, `heads3.py`, `owner.py`.
Each takes hex VAs and reads the shipped DLL directly. Static only — no run.

## 11.1 `??_7ReaderWriter@IO@@6B@` is **26 slots**, not 36 and not 31

I was told: *"`??_7ReaderWriter@IO@@6B@` is 31 slots / 0 ported / TRAP=1 / 30
purecall — NOT 36 / 2 / 4 / 30"*, because `adjacent-export` sizing cannot see the
unexported table at `0x1801554a8`. **The diagnosis is exactly right and the
number is still wrong. There are TWO hidden tables in that gap, not one.**

Re-derived by the rule I was given — extent runs to the next **STAMPED** address,
not the next export. `heads3.py`: decode every `lea reg,[rip+d]` in `.text`
(3,071 of them), keep those landing in `.rdata`, **8-byte aligned**, whose own
qword points into `.text` → **168 heads**.

```
0x1801553b0  ??_7ReaderWriter@IO@@6B@     -> next stamped head 0x180155480  = 26 slots
0x180155480  UNEXPORTED, UNMINTED         -> next stamped head 0x1801554a8  =  5 slots
0x1801554a8  UNEXPORTED (the Encoding)    -> next stamped head 0x1801554d0  =  5 slots
0x1801554d0  ??_7BinaryFileStream@IO@@6B@                                   =  8 slots
```

`0x180155480` is a genuine table head, not a coincidence. Its stamper is a
three-instruction leaf constructor with **no `.pdata` entry**, which is why a
`.pdata`-grouped screen cannot see it either:

```
180023e10:  48 8d 05 69 16 13 00   lea 0x131669(%rip),%rax   # 0x180155480
180023e17:  48 89 01               mov %rax,(%rcx)
180023e1a:  48 8b c1               mov %rcx,%rax
180023e1d:  c3                     ret
```

All five of its qwords are `0x18007ec28` (`_purecall`). It is **minted nowhere** —
not as a named row, not as one of the three `(unexported @…)` rows.

**Consequences, corrected:**

* `standalone/rt_data_mint_tables.inc:1320` claims **36** slots. Slots **26–30**
  are the five all-purecall qwords of `0x180155480`; slots **31–35** are the five
  qwords of `0x1801554a8`, which is *also* minted as its own 5-slot row at
  `:1580`. So the row is over-long by **10**, and five of those ten are minted
  **twice**.
* `??_7ReaderWriter@IO@@6B@`'s true state is **26 slots / 0 ported / TRAP=1 /
  25 purecall.** Same shape the correction gave, different numbers.
* The readiness screen's `598 slots` over-counts by **10**, not 5, and its
  `slot-count suspects: 0` is a false green by that much. **Every `598 of 598`
  denominator quoted in §3–§4 above inherits this** and should be read as
  598 reported / 588 distinct. It does not change `0 MISMATCH`: the ten extra
  slots are compared against the DLL bytes that really are at those addresses.
* **Phase 0a is strengthened again, not weakened.** ReaderWriter is purecall in
  **25 of 26** rather than 30 of 36. Both abstract-base rows stay NO-OP and I did
  not touch them.

### A methodological note, because my first version of this screen was wrong

`heads2.py` (kept, not deleted) scans every 4-byte window in `.text` as a signed
`disp32` — decoder-independent, a strict superset. It produced **4,287** `.rdata`
targets and, unfiltered, "sized" `??_7BinaryReader@` at **0 slots** off an
unaligned coincidence at `0x1801552e7`. Adding *aligned* + *holds a `.text`
pointer* cut it to 190 and it still mis-sized `??_7BinaryWriter@` at **21**. Only
the decoder route (`heads3.py`, 168 heads) agrees with the bytes throughout.

**The decoder-independent route is the better instrument for "is this address
referenced at all" and the WORSE one for "where does this table end"** — the
second question needs the reference to be a real instruction operand, and a byte
window cannot tell an operand from a coincidence.

## 11.2 The residual-interior risk on my three 26-slot rows is CLEARED

I was warned that `BinaryReader`, `BinaryWriter` and `Visitor` keep residual
interior risk from `adjacent-export` sizing — *"ends confirmed, interiors
unswept"*. The next-stamped-head rule sweeps interiors **by construction**: if a
hidden table lay inside a row, it would *be* a head inside that range and the
walk would stop there.

| row | mint says | next stamped head | derived | verdict |
|---|---|---|---|---|
| `??_7BinaryWriter@IO@@6B@` `0x180155210` | 26 | `0x1801552e0` | **26** | ✔ |
| `??_7BinaryReader@IO@@6B@` `0x1801552e0` | 26 | `0x1801553b0` | **26** | ✔ |
| `??_7Visitor@IO@@6B@` `0x1801558b0` | 26 | `0x180155980` | **26** | ✔ |
| `??_7MemoryStream@IO@@6B@` `0x1801555b0` | 8 | `0x1801555f0` | **8** | ✔ |
| `??_7VisitorStream@IO@@6B@` `0x180155a20` | 7 | ASCII `"External"` | **7** | ✔ |
| `??_7Stream@IO@@6B@` `0x1801549a0` | 7 | null qword | **7** | ✔ |
| `??_7PackageFile@IO@@6B@` `0x180155700` | 9 | null qword | **9** | ✔ |
| `??_7ReaderWriter@IO@@6B@` `0x1801553b0` | 36 | `0x180155480` | **26** | ✘ §11.1 |

**Eight rows checked, one wrong, and it is not one of mine.** All five promoted
rows are correctly sized, so §2's promotions do not rest on the broken sizing.

The specific worry I was handed — *"`MemoryStream` stamps the hidden table at
`0x1801554a8`, so its own 8-slot figure came from the same sizing"* — is
answered: MemoryStream **does** stamp it (`0x1801204e7`, inside
`?SaveAllSettings@UserInterface@UI@@`, which loads both tables into registers in
adjacent instructions), and its own extent is **still 8**, because
`0x1801555f0` (`??_7CompressedFileStream@`) is a stamped head and nothing lies
between. **Stamping a hidden table and being mis-sized by one are independent
facts; the first does not imply the second.**

## 11.3 `??_7BinaryWriter@` by the STAMPER CRITERION — and a correction to §5.3

The criterion I was given: *a row is promotable exactly insofar as we own the
function that stamps it.* Applied from the bytes — every `lea` targeting
`0x180155210`, each mapped to its containing `.pdata` function (`owner.py`):

| stamp site | containing function | ours? |
|---|---|---|
| `0x180023af2` | `??0BinaryWriter@IO@@QEAA@AEBV01@@Z` (**copy ctor**) | **NO — not ported** |
| `0x1800a899b` | `??1BinaryWriter@IO@@UEAA@XZ` | **YES** — `binary_writer.cpp:187`, both doors |
| `0x1800a8a69` | `??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z` | **YES** — `encoding.cpp`, both doors |

**§5.3 IS WRONG AND THIS SUPERSEDES IT.** §5.3 listed three stampers and claimed
*"all three bodies that install this table are ours"*. Both halves were off:

1. **I listed a function that does not stamp.** The slot-0 deleting destructor
   `0x180023b50` appears nowhere in the LEA list — it *calls* `??1BinaryWriter`,
   which does the stamping. Counting it inflated my "ours" column.
2. **I missed a function that does.** `??0BinaryWriter@IO@@QEAA@AEBV01@@Z`
   (`0x180023ac0`) stamps the table and **is not ported**.

I never enumerated the stamp sites from the binary: I listed our tree's ported
bodies and asserted they were the stampers. That is the same error class as the
§1.2 census claim — **asserting an enumeration instead of measuring one** — and
it is the second time in this lane.

**The verdict of §5 survives; the arithmetic behind it does not.** It is
**2 of 3**, not 3 of 3, and that is still decisively not the
permanently-unpromotable shape: the two stampers on the construct/destruct path
an `IO::Encoding` codec actually uses are both ours.

### 11.3.1 The route exists, is named, and run5's zero was a harness fault

Chasing the stamper rather than the route answered the route question anyway.
`?SaveSettings@DialogInstance@UI@@` (`0x180114240`) stamps `??_7MemoryStream@`
inline at `0x1801143ba` — it builds a MemoryStream, and per ENC-2 it then reaches
a BinaryWriter through `IO::Encoding::CreateWriter`, whose callee is
`??0BinaryWriter@…@AEAVSerializer@1@H@Z` (`0x1800a8a30`) — **ours, registered
under both doors.** So that route **does** open the door.

§4's reading of run5 therefore stands and is now *explained* rather than merely
observed: `post=0` there is **a harness fault, not a property of the row.** The
`dlgsettings` driver reported *"no entity with a UIComponent in this world"* and
never reached `SaveSettings`. **`??_7BinaryWriter@` is attendable, the driver is
identified, and what is missing is a world containing a `UIComponent`-bearing
entity — not a port and not an interception point.**

Three sibling stampers found the same way, offered to whoever wants them:
`?SaveMetaData@SaveLoad@Save@@` (`0x1800c18ef`), `?LoadMetaData@` (`0x1800c1dec`)
and `?OnOpened@DialogInstance@UI@@` (`0x18011502c`) all stamp `??_7MemoryStream@`
**inline** — none is a door, and they are why that row reads ATTENDED while
BinaryWriter does not.

## 11.4 `?GetSource@Stream@IO@@` — an arbitration that arrived after the code

**Reported, not resolved unilaterally. I have not touched the code.**

The arbitration awarding `?GetSource@Stream@IO@@` to lane CSTREAM reached me
after it was already written, committed at `000b727`, registered under both
doors, and verified — it is body #1 of §1, and the C1 control in §3 moved eight
rows through it. Two promoted rows (`??_7MemoryStream@`, `??_7VisitorStream@`)
reach READY only because of it.

I did not delete it, and the reason is not that I disagree with the arbitration:

* Deleting it now returns both rows to **BLOCKED-while-PROMOTED**, which is
  precisely the hazard §3 names — publishing a trap-carrying table. Backing out
  the two promotions as well is possible and is one commit, but it regresses
  verified state to prepare for a body that does not yet exist.
* The instruction — *"do not write a local copy or placeholder"* — is about
  avoiding a **shadowed duplicate registration**. That hazard is created by
  CSTREAM's landing, not by mine standing alone, so it is resolvable at merge
  either way round, and cheaper in the direction that does not throw work away.

Two resolutions, both one commit, integrator's call:

* **(a)** CSTREAM deletes its copy and consumes mine — already landed, controlled,
  with an A/B behind it.
* **(b)** I delete mine and back out the `??_7MemoryStream@` and
  `??_7VisitorStream@` promotions until CSTREAM's body lands.

I recommend **(a)** on the narrow ground that it is the only one of the two with
no interval during which a promoted row carries traps. **Whichever is chosen,
exactly one registration of `0x18000c6a0` must survive the merge** — a second is
a SHADOWED slot where the later binding wins and the checked line is dead code,
and `tools/dispatch_slot_map.py --check` will **not** catch it, because this is a
runtime-port registration and not a dispatch slot.

One detail for the owner, since our two readings differed: the integrator's
disassembly is right that the two `(%rsp)` writes are compiler SEH state and not
object state. My body reproduces the qword at `[rdx]` and the RDX return and
nothing else — `runtime/src/io/iobase_vtables.cpp`, `RtIo_Stream_GetSource`.

## 11.5 What §11 does NOT claim

* **It does not re-run the game.** Every figure in §11 is static, from the
  shipped bytes. No readiness or attendance number above is re-measured, and the
  `598 of 598` denominators in §3–§4 are annotated in §11.1 rather than restated.
* **It does not fix `tools/rt_mint_gen.py`.** ReaderWriter's row is still 36 slots
  in the tree and `0x180155480` is still minted nowhere. I am reporting the defect
  and its exact size, not repairing the generator — that is phase 0b's file, and
  lane MINT added `--check` to it.
* **It does not claim `0x180155480` is dead.** It has a real constructor at
  `0x180023e10`. I did not identify the class, did not enumerate its callers, and
  make **no** NO-OP claim about it. It is an unminted, all-purecall table and it
  is someone's row.
* **It does not claim the stamper criterion is complete for my other rows.** I
  enumerated stamp sites for five tables but scored ownership exhaustively only
  for `??_7BinaryWriter@`.
* **It does not test the `SaveSettings` route.** §11.3.1 identifies the driver
  from the bytes and explains run5's zero; **nobody has yet run a world in which
  it fires**, so `??_7BinaryWriter@`'s door remains observed at `post=0` on every
  route actually driven.
* **It does not resolve §11.4.** That is the integrator's call, and the code is
  untouched pending it.
