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
