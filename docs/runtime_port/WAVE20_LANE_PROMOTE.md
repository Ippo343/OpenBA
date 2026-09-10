# WAVE 20 LANE PROMOTE — one row promoted, six declined, and the NO-OP pair reasoned to a NEGATIVE

**Worktree** `C:/code/GIT/wave20/promote` · **branch** `wave20/promote` · cut from
`b081875` (Phase 0 merged). Every number below was measured **in this worktree**,
on the tree the commit at the end of this document names. Nothing is quoted from
another document without being re-derived here.

---

## 0. The promoted set, RE-DERIVED before anything was claimed

Counted from `standalone/reg_mint_promoted.cpp` at `b081875`, by grepping the
`RegisterRuntimeExport("??_7...")` literals:

```
10 vtable rows promoted of 62 minted
  UI/Video  SpriteSheet  Element  DialogInstance  ToolTipDialog  ImageBuffer
  IO        BinaryReader  Visitor  MemoryStream  VisitorStream  BinaryWriter
```

Correction **C2** holds on this tree: **`??_7VisitorStream@` IS ALREADY PROMOTED**
and nothing was scoped on it. None of the seven candidates below appears in that
file.

---

## 1. THE GATE, and the instrument built for it

> **A row is promotable insofar as WE OWN THE FUNCTION THAT STAMPS IT.**
> (WAVE19_CLOSED.md §5.1)

Every stamp is an in-DLL RIP-relative `LEA` followed by a store. That is **not a
door**, so `TRAP=0` says nothing whatever about who installs the table.

**`tools/mint_stamper_census.py`** (new, this lane). It does **not** build a
second decoder: it `import`s `tools/mint_extent_audit.py` (lane EXTENT, phase 0d)
and uses its writer channel unchanged — 417,832 instructions parsed, 3,188
rip-relative `LEA`s, 146 `movabs`. What this lane adds is **grouping by the
`.pdata`-bounded function containing each site**, and the **ownership join**
against a literal `RegisterRuntimeExport` / `RegisterRuntimeAddr` scan of
`runtime/ engine/ code/ standalone/ dx11/` **with C and C++ comments stripped
first** — prose naming a symbol is not a registration, and in this tree the prose
most likely to name one is a comment saying the code was deleted.

**Controls, all of which must fire before it reports anything:**

| control | reads |
|---|---|
| decoder (inherited, re-evaluated) | 3 of 3 known stamped tables FOUND |
| ownership POSITIVE `??0BinaryFileStream@IO@@QEAA@XZ` | REGISTERED |
| ownership NEGATIVE (a synthetic symbol) | not-registered |
| scan denominators | 1,519 files; 1,118 distinct export names; 1,158 addresses |

It refuses to report (exit 2) if any is red, and the first draft's control
**did** read red — a mangled name this lane guessed for `?GetSource@Stream@IO@@`
was not the shipped one, the screen printed `*NOT REGISTERED*` and stopped. The
screen can fail.

**It prints `ours-capable`, never `ours`, and says so in its own output:** a
registration is a DOOR, not proof that every caller uses it. An in-DLL
`call rel32` to a stamper still runs retail's body. The count is an **UPPER
BOUND in the OPTIMISTIC (unsafe) direction**, which is the direction that has to
be declared.

---

## 2. THE STAMPER CENSUS — enumerated from the binary

```
row                                 stampers  ours-capable  retail  unattributed
??_7BinaryFileStream@IO@@6B@               2          2         0       0
??_7CompressedMemoryStream@IO@@6B@         7          5         2       0
??_7CompressedStream@IO@@6B@               9          5         4       0
??_7CompressedFileStream@IO@@6B@           8          1         7       0
??_7FileStream@IO@@6B@                    17          6        11       0
??_7ReaderWriter@IO@@6B@                  20          7        10       3
??_7Stream@IO@@6B@                        50         22        25       3
```

`unattributed` = a stamp site outside every `.pdata` range. `.pdata` names ~6,045
of ~15,290 functions, so such a site cannot be attributed to a function at all;
it prints with a nearest-export **HINT** which is a hint and not an attribution,
and it is scored as **NOT OURS** — you cannot own a function you cannot name.

### The one row that passes: `??_7BinaryFileStream@IO@@6B@`

```
0x1800a8270  mov %rax,(%rdi)   ??0BinaryFileStream@IO@@QEAA@XZ        +0x70
0x180023cfe  mov %rax,(%rbx)   ??0BinaryFileStream@IO@@QEAA@AEBV01@@Z +0x5e
```

Both are exports; both are registered in `runtime/src/io/file_stream.cpp` **by
name and by address**. **This agrees with lane FILE's independent
`get_xrefs_to` derivation** (recorded in that file's own header) — two
derivations by different means — and **both disagree with the Wave 19 brief's
"ONE stamper"**. On the briefed number the row would have been declared fully
owned with the copy constructor's stamp still in the DLL.

Slots: `8 ours / 8 slots, 0 purecall, COMPLETE` (census PROMOTION BACKLOG, this
tree). Slot 0 is the unexported deleting-dtor thunk `0x180023d80`, registered by
address; slots 1..7 are the seven exported `FileStream` methods.

### The rows that fail, and why each is a different failure

* **`??_7FileStream@`** — the integrator asked me to assess it because the guide
  does not name it. **It fails hard: 6 of 17.** The eleven retail stampers
  include `?Run@Stream@Audio@@` (2 sites), `?Close@/`?Create@/`?Open@TextFile@`,
  three unexported `FUN_1800b6...` bodies, `?BuildPackageFile@PackageFile@`,
  `?AreResourcesEqual@PackageFile@`, `??0FileStream@` itself and
  `??0TextFileStream@IO@@QEAA@XZ`. `8/8 COMPLETE` and `TRAP=0` on this row is
  **not** a promotion. It is the textbook case the guide's §5 item 4 warns about.
* **The three Compressed rows** — 5/9, 5/7 and **1/8**. **Lane CSTREAM's
  recommendation to promote none of them is confirmed on a different instrument.**
  My per-function counts differ from CSTREAM's per-site 2/3/0 (I group by
  containing function and my channel reaches save/load metadata bodies its scan
  did not), and the *ordering and the verdict are the same*: `CompressedFileStream`
  is the worst of the three by both.

---

## 3. THE TRAP THE INTEGRATOR ASKED ME TO RESOLVE: `??_7ReaderWriter@` reads 0/1

The guide §2 and WAVE19_CLOSED §7.5b both say promote `??_7Stream@` and
`??_7ReaderWriter@` FIRST as the lowest-risk items, because they are proven
NO-OPs. Their safety rests **entirely** on that NO-OP claim, which is
impossibility-shaped. §7.4 says decompose it and check the premise as a separate
line. Here is the premise, checked four ways.

### 3.1 What the row actually contains — read out of the shipped `.rdata`

`tools/vtable_dump.py 0x1801553b0 26` (new, this lane; `_purecall` identified at
`0x18007ec28`, the same constant `tools/rt_mint_gen.py:107` carries, and the tool
refuses to report a "0 purecall" verdict if it cannot name `_purecall` at all):

```
??_7ReaderWriter@IO@@6B@   slot 0  0x180023bd0  FUN_180023bd0
                           slots 1..25          _purecall
                           purecall 25 of 26 ; concrete 1
```

**So CENSUS's `0 ours / 1 slot` is exactly right and it is not a slot-count bug:
the row has ONE concrete slot, and `0x180023bd0` is registered NOWHERE in this
tree** (grepped across `runtime/ standalone/ code/ engine/`; zero hits, comments
included).

**Promoting this row therefore publishes a table whose single reachable slot is
a named abort trap, replacing a working retail destructor thunk.**

### 3.2 Can anything reach slot 0? — the enumeration

1. **All 20 references to `0x1801553b0` in the whole DLL are rip-relative `LEA`
   stamps.** Enumerated here by the writer channel; **20 is the same number lane
   NOOP reported by two other means**, and my `??_7Stream@` count of **56** is
   likewise NOOP's. Two lanes, three channels, same integers.
2. **No shipped image imports the symbol, and no image holds its VA as a data
   qword** — checked over all four binaries with a control (each scan reports a
   real import it did find, so a "False" is not a dead scan):
   `Application-x64.exe` (899 imports), `Runtime-x64.dll` (189),
   `VideoDX11-x64.dll` (163), `VideoDX9-x64.dll` (146) — **0 sites, 0 imports**
   for `??_7ReaderWriter@`, `??_7Stream@` and `??_7BinaryFileStream@` alike.
3. **The class is abstract** — 25 of 26 slots purecall — so no leaf object
   carries this table. It is the live `+0x00` only in the window between
   `??0ReaderWriter@` and the derived constructor (and symmetrically on the way
   down), and slot 0 is a *deleting destructor*: reaching it means `delete` on a
   pointer of static type `ReaderWriter*`, which cannot happen inside that
   window.
4. Even our own stamps mirror retail's stamp-then-overwrite: `??0BinaryReader@`
   at `+0x11` stamps ReaderWriter's table and `+0x32` overwrites it with
   BinaryReader's, in the same body.

**VERDICT ON THE PREMISE: it HOLDS. Nothing can reach slot 0.** §7.5b is not
refuted on the point it argues.

### 3.3 …and I still recommend promoting NEITHER, which is a refinement of §7.5b, not a refutation

§7.5b's argument is: *promotion is risky only because a wrong slot might be
dispatched; for a proven NO-OP nothing is dispatched, so there is no risk to
weigh — the safest promotion available.* Every step is true. What it does not ask
is **what the promotion buys**, and the answer is fixed by the same proof:

> **The proof that removes the risk removes the whole benefit. "Safest" and
> "pointless" are, for a NO-OP row, THE SAME FACT.**

Against that zero benefit stand three real costs:

1. **Both rows are already tagged `RT_UNPROMOTABLE … | NONE, EVER` in
   `standalone/reg_mint_promoted.cpp`** (Wave 19 phase 0a lane NOOP), and that
   block is `tools/runtime_census.py`'s **source of truth for the ceiling**.
   Promoting means deleting two tags, which mechanically **lowers IO's printed
   ceiling** for zero behaviour change — the moving-target arithmetic Wave 18
   already paid for once.
2. **For `??_7ReaderWriter@` the residual risk is ONE-SIDED and lands on a trap.**
   If the NO-OP proof is wrong anywhere, the failure mode is `abort()` where
   retail ran a working thunk, because the row's only concrete slot is unported.
   A promotion whose downside is a trap and whose upside is provably nil is not
   the lowest-risk item on the board; it is a coin with one blank face.
3. **`??_7Stream@` does not have cost 2** — its 2 concrete slots of 7 are both
   ported, so a surprise dispatch would land in a real body — but it still has
   cost 1 and zero benefit, and **22 ours-capable of 50 stampers**: 25 stamps
   stay retail's whatever we do.

**Cutover is not an argument for promoting them either.** §7.5b's own observation
is that what cutover needs from a NO-OP row is *the address to stamp*, not our
slot pointers, and cutover registers every minted row regardless — the
`??_7Internal@Object@@` tag in the same file already records that a
`NONE, EVER` tag does not stand in cutover's way.

**EXPIRY on this recommendation**, because a recommendation without one is a
TODO: it lapses the moment `0x180023bd0` is ported and registered (which would
give `??_7ReaderWriter@` a concrete slot worth publishing), or the moment any
instrument observes a dispatch through either table — the premise in §3.2 is an
existence claim and a single counter-example retires it.

---

## 4. THE PROMOTION, ITS A/B CONTROL, AND ITS ATTENDANCE

`??_7BinaryFileStream@IO@@6B@` is promoted in `standalone/reg_mint_promoted.cpp`
behind **`BANISHED_RT_PROMOTE_BFS=0`**, a **per-row** kill switch rather than the
file-wide `BANISHED_RT_MINT_PROMOTED=0`, so the A/B moves **one variable**.
Default-on, per this project's rule that a registration which only runs when
asked is not a port.

Two runs, same exe (`md5 34ad28…` rebuilt with the reporting change), same route
(`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`),
under `tools/game_dir_lock.sh`, both exiting 0 with an empty
`standalone_crash.log` and no process left behind:

```
                                      promoted (default)   BANISHED_RT_PROMOTE_BFS=0
EFFECT     stamps                          2847                     2847
           stampedMinted                   2847                        0
ATTENDANCE slotEntries                    18906                        0
           viaMintBFS                     18906                        0
           viaDllBFS / other                 0 / 0                     0 / 0
BODIES     Read                            9453                        0
           SetCurrentPos                   3759                        0
           GetSize                         2847                        0
           Open                            2847                        0
           GetSource / Write /
           GetCurrentPos / deletingDtor /
           copyCtor                           0                        0
```

**Read this as three separate claims, at three different strengths.**

* **EFFECT — MEASURED, denominator 2847.** `stamps` is every stamp by either of
  the row's two stampers and it is **identical in both halves**, so *coverage did
  not move, the VALUE did*: with the row promoted every one of 2,847 stamps
  installs OUR minted table; with the switch set, none does. The control reads
  **non-zero** and is scored on applicable cases only (a run constructing no
  `BinaryFileStream` would print an explicit *I MEASURED NOTHING* line instead).
* **ATTENDANCE — MEASURED, and this is the wave's point.** Wave 19 handed over
  ten verified bodies at **TRAP=0 with attendance ZERO**. Four of them now read
  **9,453 / 3,759 / 2,847 / 2,847**, dispatched through a table we published:
  `18,906` of `18,906` entries arrived carrying our minted vptr. In the control
  half the same four read **0**, which independently confirms lane FILE's
  diagnosis that *unpromoted means retail's bodies run* — those 18,906 dispatches
  were going into the DLL.
* **NOT VERIFICATION.** Attendance says a body ran, not that it agrees with
  retail's. The oracle for these bodies is
  `runtime/src/io/file_stream_slots_probe.cpp` and it is a **separate and
  stronger tier**. Four of the eight bodies are still at attendance **0** on this
  route (`GetSource`, `Write`, `GetCurrentPos`, and both destructor paths) — an
  honest zero, printed rather than folded into a total.

### Why the vptr test, and why it is a POSITIVE test

`??_7FileStream@` and `??_7BinaryFileStream@` have **byte-identical slots 1..7**,
and only one of the two rows is promoted — so a per-body counter alone cannot say
which table dispatched. The test compares the object's live vptr against the
DLL's own table (reached through `ResolveRuntimeRaw`, which never consults the
port registry — asking `ResolveAddr` would compare the answer with itself) and
identifies our minted table **positively**, because `??_7TextFileStream@`'s slots
1 and 7 are these same two bodies and an "anything that is not the DLL's table"
test would score a TextFileStream dispatch as attendance for this row.

---

## 5. CENSUS SET-DIFF (not a percentage delta)

`tools/runtime_census.py`, run in this worktree, before and after:

```
ENTERED `called` :  NOTHING.  1306 -> 1306 rows; IO 211 -> 211.
                    All 8 slot bodies already had resolve doors, so the fold-in
                    made no row newly visible (111 of 135 -> 111 of 143).
ENTERED `ported` :  ??_7BinaryFileStream@IO@@6B@   (the vtable symbol itself)
                    IO 194 -> 195 ; TOTAL 1118 -> 1119
PROMOTION BACKLOG:  164 bodies / 49 vtables, 26 COMPLETE
                 -> 156 bodies / 48 vtables, 25 COMPLETE
                    (the row leaves the backlog with its 8 bodies)
promoted rows    :  36 -> 37 ; minted-vtable rows folded in 135 -> 143
                    vtables promoted 10 -> 11 of 62
```

**Caveat on the census's own count, in its own words:** the promotion figure is a
**source scan** and *"a source scan cannot see an env var; it does not try."* My
registration sits inside an `if (kill switch not set)`, so on a
`BANISHED_RT_PROMOTE_BFS=0` run the census's `11 of 62` is an overstatement by
one — exactly the caveat it already prints for `BANISHED_RT_MINT_PROMOTED=0`.

---

## 6. A BROKEN INSTRUMENT FOUND IN PASSING — reported, not fixed

`runtime/src/io/file_stream_cutover_probe.cpp`'s door screen **refuses to report
on this tree** and it is right to:

```
[FileCutover] CONTROL negative (must read NOT-PORTED) ?Commit@CompressedStream@IO@@QEAAXXZ : PORTED -> RED
[FileCutover] *** MEASURED NOTHING: a control failed ... ***
```

Its negative control was chosen as a symbol nobody had ported; **someone ported
it**. The screen behaves correctly — it says it measured nothing — but the whole
25-door board and its slot-attendance line are dark until the control is
re-chosen, and **it took my counters down with it** on my first measurement run,
which printed nothing at all. Fixed on my side by giving `[BfsPromote]` its **own
`atexit`**: a control failing for the DOOR BOARD says nothing about the counters,
which are a different subject with their own liveness control. **The door board
itself is left for whoever owns that file** — re-pointing another lane's control
is how a screen quietly stops being able to fail.

---

## 7. WHAT I DO NOT CLAIM

1. **I do not claim the eight `BinaryFileStream` slot bodies are CORRECT.** I
   claim they are now DISPATCHED. Correctness is `file_stream_slots_probe.cpp`'s
   tier and I did not run it.
2. **I do not claim `ours-capable` equals `ours`.** It is a registration scan: a
   door, not proof every caller uses it, and the error direction is optimistic.
   For the promoted row the gap is closed by measurement (2847 of 2847 stamps
   landed on our table), **not** by the scan.
3. **I do not claim the four zero-attendance bodies are unreachable** — only that
   this route did not reach them.
4. **I do not claim `??_7ReaderWriter@` / `??_7Stream@` are unpromotable.** I
   claim promoting them is measurably worthless and, for ReaderWriter,
   one-sidedly bad; the tags already in the tree say the stronger thing and I did
   not re-derive their proofs beyond §3.2.
5. **I did not re-derive the 62-row denominator** — inherited from
   `rt_mint_gen.py --check` via Phase 0.
6. **My stamper counts include sites the writer channel could not pair with a
   store within three instructions** (they print the following instruction
   instead, e.g. a bare `lea`). Those may be a table address used as a VALUE
   rather than a stamp, which would make my stamper counts an OVER-count — the
   safe direction for a gate that must not declare a row owned, and the wrong
   direction for the `retail` column, which is therefore also an upper bound.
7. **I did not measure whether promotion changes anything the player can see.**
   Both halves boot, play and quit 0; I ran no screenshot or save comparison.
