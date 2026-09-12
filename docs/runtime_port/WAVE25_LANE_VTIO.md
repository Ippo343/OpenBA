# WAVE 25 LANE VTIO — vtable promotion, IO / Engine / Audio / System side

Branch `wave25/vtio`, cut from `wave25/base` at `fc56353c`. Private worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a85f6a3649414d2da`.

Every binary read below is against **retail** `C:\GOG Games\Banished\Runtime-x64.dll`.
The profile build was not opened for anything in this report.

**Rows promoted: ZERO.** That is the correct answer and §2 is the measurement behind it.
**Stamper movement: ZERO**, stated plainly because the brief asked for a number.
What this lane actually produced is in §3 (a measurement that shrinks the block by 53%),
§4 (six ports that take one row to COMPLETE), §5 (two named residuals discharged), and
§7 (two defects I found in my own instruments, one of which I had already half-written
into a verdict).

---

## 0. HEADLINE — THE `retail` STAMPER COLUMN COUNTS FUNCTIONS NOTHING CAN CALL

`tools/mint_stamper_census.py` decides promotability by asking *who stamps this table,
and do we own that function*. A stamper we do not own is printed as `RETAIL` and reads
as a block.

**25 of the 47 `RETAIL` stampers across my eleven rows carrying stamper data cannot be
reached by anything.** Not by an in-DLL `call`/`jmp`, not by an import in any shipped image, not
through a stored pointer, not by an RVA reference, not by having their address taken.
They are MSVC-emitted copy constructors and default constructors that no caller in the
shipped product ever invokes.

**A stamper that is never called never installs the table, so it cannot defeat a
promotion.** The column therefore overstates the block by **53%** on my rows (25 of 47), and it
overstates in the INFLATING direction — the direction that manufactures work
(`reference_a_screen_that_cannot_name_its_subject.md`).

This does **not** rescue any row: after the correction every one of those eleven still has
at least one reachable retail stamper, so **the promotion verdicts do not change**. What
changes is the SIZE of each block, and three rows turn out to be blocked by exactly
**one** function each (§3.2). That is the difference between "seven retail stampers" and
"one dtor", and it is the difference between a row nobody schedules and a row somebody
can.

---

## 1. THE BOARD, RE-DERIVED ON MY OWN TIP

```
tools/runtime_census.py --unported      # branch-local, wave25/vtio at fc56353c
TOTAL   4632   1323 called   1171 ported   9 detour   88.5%
IO 14 | BODY 1 DTOR 1 VTABLE 8 DATA 4      Engine 8 | BODY 1 VTABLE 4 DATA 3
Audio 4 | BODY 1 VTABLE 1 DATA 2           System 19 | BODY 7 VTABLE 1 DATA 11
```

**The kickoff's §0 table re-derives EXACTLY**, every namespace, every kind column. No
correction to report. My fourteen rows are the 8 IO + 4 Engine + 1 Audio + 1 System
`VTABLE` rows, and all fourteen matched by name.

**After this lane (branch-local — NOT a board figure; the integrator re-derives on the
merged tip, because Wave 21 published from lane reports and had to correct itself):**

```
TOTAL   4632   1325 called   1173 ported   9 detour   88.5%
```

### 1a. DECLARED SURFACE GROWTH (§1f), MEASURED BY A ROW-SET DIFF

Not reasoned about — `tools/runtime_census.py --csv` on both sides of a stash, then
`comm` on the key column. **My first draft of this declaration said `+0` and was
wrong**, which is §1f's own warning landing on the lane that was quoting it.

```
called  1323 -> 1325  (+2)     ported  1171 -> 1173  (+2)     net gap  +0
rows added:  ?GetResourceFilename@UpdateLoop@Engine@@UEAAPEBDXZ
             ?AllowsModChanges@UpdateLoop@Engine@@UEAA_NXZ
```

**None of that growth belongs to the ports.** A registration alone does not create a
census row — the population is built from RESOLVE DOORS — so four of my six new bodies
move no column at all. The two rows that appeared are the two my **oracle** opens an
`RT_ORIGINAL_ONCE` door on. They enter `called` and `ported` in the same breath, so the
gap is unchanged, and the cost belongs to the instrument.

One further growth is REAL and the board **cannot show it**: the new `deleting_dtor.cpp`
row makes `??1PackageFile@IO@@UEAA@XZ` (`0x1800b4540`) newly reachable through our
thunk, but `DeletingDtorCore` reaches it through a table field rather than a literal at
a call site, which is the census's known non-literal blind spot. Recorded in the source
comment rather than left to be rediscovered.

### 1b. AND A THIRD ROW APPEARED THAT SHOULD NOT HAVE — MY OWN CONTAMINATION

The first measured diff was **+3**, and the third row was
`?NoSuchSlot@UpdateLoop@Engine@@UEAAXXZ` — **my oracle's negative control**. Written as
a string literal at a resolve call site, the census cannot know the symbol does not
exist, so it minted a permanent gap row for a symbol I had invented.

A negative control that inflates the board costs a future lane a lookup into nothing.
Fixed by assembling the phantom name at run time from three fragments, which defeats the
literal scan and changes nothing about what the control tests. Re-measured: +2.

**This generalises, and it is free to check:** any lane whose probe passes a
deliberately-absent mangled name as a literal is adding a phantom row. I did not sweep
the tree for other instances.

---

## 2. THE FOURTEEN ROWS — SLOTS, PURECALL, PORTED, STAMPERS, VERDICT

Slot and purecall counts from `tools/vtable_dump.py` against retail. `ours/non-purecall`
from the census PROMOTION BACKLOG (M=NONPURECALL). Stampers from
`tools/mint_stamper_census.py`, all controls green (decoder 3/3; positive
`??0BinaryFileStream@IO@@QEAA@XZ` REGISTERED; synthetic negative not-registered).
"eff." is the §3 correction: retail stampers minus those with no route by five screens.

| row | VA | slots | pure | ours (after this lane) | stamp | ours-cap | RETAIL | unattr | **eff.** | promotion changes a dispatch? | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|
| `??_7CompressedFileStream@IO@@6B@` | 0x1801555f0 | 9 | **0** | 9/9 COMPLETE | 8 | 1 | 7 | 0 | **1** | would, if promotable | **HOLD** |
| `??_7CompressedMemoryStream@IO@@6B@` | 0x180155560 | 9 | **0** | 9/9 COMPLETE | 7 | 5 | 2 | 0 | **1** | would | **HOLD** |
| `??_7CompressedStream@IO@@6B@` | 0x180155640 | 9 | **0** | 9/9 COMPLETE | 9 | 5 | 4 | 0 | **2** | would | **HOLD** |
| `??_7FileStream@IO@@6B@` | 0x180154960 | 8 | **0** | 8/8 COMPLETE | 17 | 6 | 11 | 0 | **6** | would | **HOLD** |
| `??_7PackageFile@IO@@6B@` | 0x180155700 | 9 | **0** | **6/9** (was 5/9) | 4 | 2 | 2 | 0 | **1** | would | **HOLD** |
| `??_7TextFileStream@IO@@6B@` | 0x180155980 | 10 | **0** | 8/10 | 8 | 2 | 6 | 0 | **3** | would | **HOLD** |
| `??_7ReaderWriter@IO@@6B@` | 0x1801553b0 | 26 | **25** | 1/1 COMPLETE | 20 | 7 | 10 | 3 | n/m | **NO** (Wave 19: 17 windows, 0 indirect calls) | **HOLD — tagged, not re-opened** |
| `??_7Stream@IO@@6B@` | 0x1801549a0 | 7 | **5** | 2/2 COMPLETE | 50 | 22 | 25 | 3 | n/m | **NO** (Wave 19: 58 windows, 0 dispatches) | **HOLD — tagged, not re-opened** |
| `??_7UpdateLoop@Engine@@6B@` | 0x180155ba0 | 12 | 5 | **7/7 COMPLETE** (was 1/7) | 8 | 2 | 6 | 0 | **3** | would | **HOLD** |
| `??_7LoaderLoop@Engine@@6B@` | 0x180157940 | 12 | 0 | **6/12** (was 1/12) | 3 | 1 | 2 | 0 | **2** | would | **HOLD** |
| `??_7DialogLoopT@Engine@@6B@` | 0x1801579a0 | 13 | 1 | **10/12** (was 4/12) | 3 | 1 | 2 | 0 | **1** | would | **HOLD** |
| `??_7DialogLoaderLoopT@Engine@@6B@` | 0x1801578d0 | 13 | 1 | **8/12** (was 3/12) | 4 | 1 | 3 | 0 | **1** | would | **HOLD** |
| `??_7Thread@System@@6B@` | 0x180155f20 | 2 | 1 | 1/1 COMPLETE | 6 | 4 | 2 | 0 | **1** | — | **HOLD → lane MANIFEST** (§6) |
| `??_7Stream@Audio@@6B@` | 0x180154950 | 4 | **0** | **3/4** | — | — | — | — | — | not a promotion question yet (§6) | **HOLD** |

**Every IO and Audio row in my set has ZERO purecall slots.** By the brief's own sorting
axis — readiness and value are anti-correlated along the purecall count — these are the
most valuable tables in the backlog, and they are the ones with the largest stamper
debts. The anti-correlation reproduces on my rows without my looking for it.

**Wave 23 lane IOPORT's stamper table re-derives EXACTLY** — all eight IO rows, all four
columns, including the two rows Wave 20 had omitted. A measured section held again. My
only additions to it are the `eff.` column and the four Engine rows.

### 2a. THE TWO TAGGED ROWS — RE-DERIVED, NOT RE-OPENED

Per §0b I re-derived both tags before touching anything, and **neither moves**.

- **`??_7ReaderWriter@IO@@6B@`** — `UNPROMOTABLE(NO-OP)`, `1/26 ported`, 25 purecall.
  `tools/vtable_dump.py 0x1801553b0 26`: **25 purecall, slot 0 concrete** at
  `0x180023bd0`, which is `deleting_dtor.cpp` row 42 and is ours. Confirmed.
- **`??_7Stream@IO@@6B@`** — COMPLETE-and-unpromotable, block is stamper ownership.
  `tools/vtable_dump.py 0x1801549a0 7`: 5 purecall, 2 concrete, both ours. Stamper
  census: **50 / 22 ours-capable / 25 RETAIL / 3 unattributed** — Wave 23's figure to
  the unit.

**On the coordinator's suggestion that `??_7Stream@IO@@6B@` "remains the row where
converting stampers actually pays": I measured it and I disagree, and the tree already
contains the refutation.** `WAVE19_PHASE0A_NOOP.md` establishes that promoting this row
changes *which table object is briefly written into a `+0x00` that nothing reads before
it is overwritten or released* — 58 stamp windows, zero dispatches through the stamped
object, corroborated three independent ways. So even at **0** retail stampers the
promotion would change no dispatch.

**The row with the most convertible stampers in my set is the row where conversion is
provably worthless.** That is the anti-correlation at its sharpest, and it is why the
stamper count must never be read as a value ranking on its own. (Route-scoped, as
always: at cutover this row is REQUIRED and belongs to lane MANIFEST, not to me.)

---

## 3. THE MEASUREMENT — "CAN ANYTHING REACH THIS STAMPER?", FIVE SCREENS

### 3.1 The screens, and the two that were added because the earlier ones were blind

| # | route | what it finds | control |
|---|---|---|---|
| R1 | import table of `Application-x64.exe`, `VideoDX11`, `VideoDX9` | the exe calling an export | exe imports = **833**, matching CLAUDE.md exactly |
| R2 | 8-byte-aligned qword in `.rdata`/`.data` holding the VA | a vtable slot or function-pointer table | `0x18000c6a0` (slot 1 of `??_7Stream@IO@@6B@`) reads **7**; synthetic reads 0 |
| R3 | in-DLL direct `call`/`jmp` rel32 | ordinary calls and SEH tail-jumps | `0x180006a10` reads **354** |
| R4 | 4-byte-aligned **RVA** dword, per section | RVA-form references (`.xdata`, EAT) | every subject reads `.pdata=1` — its own RUNTIME_FUNCTION |
| R5 | **address taken by rip-relative `LEA`** | indirect calls, atexit/CRT registration | `0x18014a580` reads **1**; non-function-start reads 0 |

**R3's control reads 316, not Wave 19's 354, and that is a UNIT difference, not an
error.** 316 `call` + 38 `jmp` = **354** exactly: Wave 19's `callers.py` counted both as
one figure. I reconciled it before using the channel rather than after
(`reference_a_cross_wave_comparison_reads_as_an_instrument_disagreement.md`).

**R4 exists because of `WAVE19_PHASE0A_NOOP.md` §10.2** — a screen that compared a
32-bit reference against `VA & 0xffffffff` could never match, because a 32-bit reference
in a PE is an RVA. I built the RVA form in from the start rather than rediscovering it.

**R5 exists because R1–R4 were about to make me publish a false verdict — see §7.1.**

### 3.2 The result

Across the eleven rows the stamper census covers: **47 `RETAIL` stampers, 25 with no
route found by any of the five screens.** (The twelfth untagged row,
`??_7Stream@Audio@@6B@`, is not in `rt_data_mint_tables.inc`, so the tool has no entry
for it -- see §6.2. The two tagged rows are excluded by §0b.) The unreachable set is, without exception, MSVC's
compiler-emitted copy constructors (`??0X@…@AEBV01@@Z`), three default constructors, two
public-API `Object::External` statics the exe does not import, and three private
functions with no reference of any kind.

Three rows collapse to a single reachable blocker:

| row | the ONE function standing between it and 0 retail stampers | size |
|---|---|---|
| `??_7PackageFile@IO@@6B@` | `??1PackageFile@IO@@UEAA@XZ` (0x1800b4540) | 318 B `.pdata` LOWER / 336 UPPER |
| `??_7CompressedFileStream@IO@@6B@` | `??1CompressedFileStream@IO@@UEAA@XZ` (0x1800a9330) | 112 / 128 B |
| `??_7CompressedMemoryStream@IO@@6B@` | `?BuildPackageFile@PackageFile@IO@@` (0x1800b1820) | ≥ 0x1b00 B — **out of reach**, and it is the real wall on the Compressed cluster |

`??_7DialogLoopT@` and `??_7DialogLoaderLoopT@` also reduce to one each
(`??1DialogLoopT@`, `??1DialogLoaderLoopT@`), but both remain slot-blocked, so the
stamper number is not their binding constraint.

**`??_7PackageFile@IO@@6B@` is therefore the narrowest fully-specified promotion path in
IO**, and the specification is complete: one 318-byte dtor whose body
`WAVE23_LANE_IOPORT.md` §2.4 already transcribed, plus the three remaining slots in §5.1.
Wave 23 named this cluster as its most valuable carry-forward (R2); this lane confirms it
**and halves it** — Wave 23 listed two retail stampers, and the copy constructor is one
of the 25 that nothing can call.

### 3.3 WHAT THIS MEASUREMENT DOES NOT CLAIM

- **"No route found by five screens" is not "provably dead."** A call through a pointer
  computed at run time from something other than a stored VA or a taken address would be
  invisible to all five. I found no such mechanism in this DLL, but I did not prove one
  cannot exist.
- **It says nothing about cutover.** All five routes are DLL-present. At cutover a
  "retail" stamper is not a competitor; it is a function that must be ported or the
  object is never constructed. The table in §2 is the DLL-present one.
- **I did not fix `tools/mint_stamper_census.py`.** Adding a reachability column would be
  the right change and it is a shared file three lanes are editing this wave. Handed on
  as R3 in §8 with a named addressee instead.

---

## 4. WHAT I PORTED — THE SIX SHARED `UpdateLoop` BASE SLOTS

`runtime/src/engine/loops.cpp`.

**The finding that made this cheap:** Engine's four `VTABLE` gap rows are not four
independent bodies of work. Their **43 concrete slot-instances resolve to 26 distinct
function addresses**, and six of those appear in all four tables (five of them) or two.
Taken from the shipped `.rdata` by reading each table's qwords, with the registration
scan controlled on both arms (positive NAME, positive ADDR, synthetic negative).

> **A namespace gap of N is not N bodies — and a vtable backlog of `1/12 + 4/12 + 3/12 +
> 1/7` is not 43 bodies either. It was 26, of which 9 were already ours and 17 unported.
> The per-row backlog counts the same 9 bodies 43 ways, and the census prints no union.**

All six sit at 16-byte spacing and every one is a leaf of at most three bytes:

```
180031740  ?DrawFailed@          c2 00 00     ret        180031780  ?GetResourceFilename@  33 c0 c3  xor eax,eax; ret -> nullptr
180031750  ?SaveRestore@         c2 00 00     ret        180031790  ?AllowsModChanges@     32 c0 c3  xor al,al  ; ret -> false
180031760  ?OnVideoError@        c2 00 00     ret
180031770  ?OnVideoRestart@      c2 00 00     ret
```

Ported faithfully and unguarded. `?GetResourceFilename@` returns a null **pointer**, not
an empty string; `?AllowsModChanges@` zeroes only AL, and the Win64 ABI defines only AL
for a `bool` return, so `return false` is correct.

**Effect, re-derived with the census after the edit:**

```
??_7UpdateLoop@Engine@@6B@         1/7  -> 7/7  COMPLETE
??_7LoaderLoop@Engine@@6B@         1/12 -> 6/12
??_7DialogLoopT@Engine@@6B@        4/12 -> 10/12
??_7DialogLoaderLoopT@Engine@@6B@  3/12 -> 8/12
```

**WHAT THIS IS NOT.** It promotes nothing and changes no dispatch today. COMPLETE is the
static half of readiness only, and `??_7UpdateLoop@` still has three reachable retail
stampers. I checked the dispatch question rather than assuming it: our own exe-subclass
tables (`g_menuDialogLoopVtable`, `g_splashDialogLoopVtable`, `g_outerLoaderLoopVtable`,
`gameupdateloop_stubs.cpp`) define their **own** stubs for all six slots, so nothing in
our tree reaches the Runtime base bodies. The value claimed is **cutover readiness** —
at cutover retail's six are gone and the minted tables are the only owner — plus the
reduction of Engine's vtable backlog from 17 unported bodies to 11.

### 4.1 The oracle, and why it is an ORACLE

These six have no dispatch route, so a shadow site inside them would read `calls=0`
forever and look like a pass. **"No route" is not "no oracle"**, so the probe synthesises
one: `RtEngineUpdateLoopBaseSlotsOracle()`, gated on `BANISHED_RT_VTIO_ORACLE`, run once
at registration. Every "theirs" value comes from `ResolveExportRaw` (GetProcAddress) —
never `ResolveExport` or `ResolveAddr`, both of which consult the port registry first and
would compare the port with itself.

Three screens, **scored separately, never unioned**, each with an effectiveness counter:

| screen | subject | control |
|---|---|---|
| **S1 BINDING** | does each mangled name resolve to the address whose bytes I read? | negative: a run-time-assembled phantom name must resolve NULL |
| **S2 NO-WRITE** | retail's body handed a 0x100-byte poisoned subject must write nothing (the Wave 12 `?PlatformDestruct@Application@1@` template) | **positive shape**: a synthetic writer must be caught, or "0 writes" is indistinguishable from a dead `memcmp` |
| **S3 RETURN** | ours vs the shipped body's return, for the two value slots | negative: a corrupted return — of a value S3 actually compares — must be caught |

The four void bodies carry sites labelled `[ATTENDANCE]` in their own names, **not**
`RT_DIFF_SHADOW`: there is nothing to compare inside a body that reads nothing and
returns nothing, and a site that prints `divergences=0` and cannot print anything else is
worse than no site. The two value bodies carry `[ORACLE]` sites with a real
`RT_DIFF_SCALAR` against the DLL.

**S1 is DISCHARGED OFFLINE, without a run.** Against retail's export directory: all six
names bind to exactly the addresses whose bytes I disassembled, 6 examined / 0 mismatches,
positive control (a name that must be present) GREEN, negative control (the phantom is
not in the export table) GREEN — and the three bytes at each address re-read as quoted.

**S2 and S3 have NOT been run.** §1d gives my lane no game-directory lock, and I did not
take one. I checked the harness before writing that (`standalone/`, the `BANISHED_*`
list): the probe is wired and reachable, only the run is missing. `calls=0` here means
**never executed**, which is a different statement from "ran and measured nothing".

---

## 5. TWO NAMED RESIDUALS DISCHARGED

`WAVE23_LANE_IOPORT.md` §8 items 2 and 3 record, plainly, that that lane measured the
slot counts but "did not enumerate which four slots are unported". Here they are.

### 5.1 `??_7PackageFile@IO@@6B@` — the four missing slots, and one of them is now ours

| slot | body | state | note |
|---|---|---|---|
| 0 | `FUN_1800259d0` | **PORTED THIS LANE** | the deleting-dtor thunk |
| 5 | `?Serialize@PackageFile@IO@@UEAA_NAEAVStream@2@@Z` (0x1800b60a0) | unported | 482 B `.pdata` LOWER / 6096 UPPER — **a `Serialize`, so the WRITE path is replayable** |
| 6 | `?Reconstruct@PackageFile@IO@@UEAAPEAVExternal@Object@@XZ` (0x1800252d0) | unported | **29 / 32 B** — trivial |
| 7 | `?Destruct@PackageFile@IO@@UEAAXXZ` (0x180025300) | unported | **no `.pdata`; 16 B UPPER** — trivial |

Slot 0 is the new `deleting_dtor.cpp` row 44,
`RT_DELDTOR(0x1800259d0, 0x1800b4540, 0x0f0, "IO::PackageFile")`, verified by
`tools/rt_deleting_dtor_verify.py`, which re-reads the DLL independently of the source:
**44/44 OK, 0 problems**, `size=0xf0 dtor=0x1800b4540`.

`sizeof(IO::PackageFile) == 0xf0` (240). Wave 23 §2.4 estimated "0xc8+ bytes". 240 is the
shipped figure; it is larger, which **strengthens** that lane's stated reason for not
porting `??1PackageFile@` (a fabricated 240-byte subject would be a fake, not a
synthetic one) rather than weakening it.

**Two of the three remaining slots are under 32 bytes.** With slot 0 closed, the row is
6/9 and the residue is one large `Serialize` plus two trivia — a far smaller task than
"4 unported slots" suggested.

### 5.2 `??_7TextFileStream@IO@@6B@` — the two missing slots

Re-derived, and they are **exactly** where Wave 21 lane ENC left them: slot 2
`?Write@TextFileStream@IO@@UEAA_NHPEBX@Z` (0x1800ba9f0, 462 B) and slot 8
`?Open@TextFileStream@IO@@…` (0x1800ba640). Still NOT ATTEMPTED, a size judgement with
no expiry. No correction.

---

## 6. THE TWO ROWS THAT ARE SOMEBODY ELSE'S

### 6.1 `??_7Thread@System@@6B@` — NOTED, NOT REGISTERED

Per the brief this row registers at cutover and belongs to lane MANIFEST's publish list.
I coordinated by noting it. What I can add for MANIFEST:

- 2 slots, **1 purecall**, 1 concrete, **1/1 COMPLETE**.
- 6 stampers, 4 ours-capable, **2 RETAIL** — and of those two, `??1Loader@Engine@@QEAA@XZ`
  has **no route by any of the five screens**, leaving a single reachable retail stamper:
  `FUN_18014a580`, the dynamic initializer that stamps the static
  `?_loader@LoaderLoop@Engine@@1VLoader@2@A` at `0x1801fe7b8`.
- **I nearly reported this row's effective retail as 0.** See §7.1 — that would have been
  wrong, and MANIFEST should take the figure as **1**.

### 6.2 `??_7Stream@Audio@@6B@` — the cleanest row in my set, and not a promotion question

4 slots, **0 purecall**, **3 of 4 ours**. It is **not in `rt_data_mint_tables.inc`**,
deliberately and documented at `runtime/src/audio/audio_stream_win.cpp:142`, so there is
no minted table to promote *to*; its census presence is an address door.

Wave 24 lane SMALLBODY already performed the stamp conversion the brief pointed me at:
our `Audio::Stream` ctor/dtor stamp the vptr through
`ResolveAddr(Module::Runtime, 0x180154950)`, turning retail's RIP-relative LEA into a
real door. **That work is done; there is no stamper left for me to move on this row.**

Its single unported slot is `?Run@Stream@Audio@@UEAAXXZ` (0x1800a0f60, **1800 B `.pdata`
LOWER / 1808 UPPER**, zero in-DLL callers — it is dispatched only through the vtable).
Porting it would do double duty: it is **also** a reachable retail stamper of
`??_7FileStream@IO@@6B@`. It is a 1.8 KB body and out of scope for a slot sweep.

---

## 7. TWO DEFECTS IN MY OWN INSTRUMENTS, BOTH CAUGHT WHILE THEY WERE AGREEING WITH ME

### 7.1 The one that was already half-written into a verdict

With four screens, `??_7Thread@System@@6B@` read **0 effective retail stampers** — a
COMPLETE table with no reachable retail stamper, i.e. *promotable*. It was the most
interesting result in the lane and I had started writing it down.

It is false. `FUN_18014a580` is reached at `0x180149018` by
`lea 0x1561(%rip),%rcx` — its **address is taken** and it is called indirectly, which is
how a CRT-style initializer or an `atexit` registration is handed to a helper. A rel32
scan cannot see that, and neither can a stored-pointer scan, because the pointer is never
stored.

I found it only because the claim was load-bearing and I pushed on that one rather than
on the twenty that did not matter. **R5 was built in response, given a green positive and
negative control, and re-run over the whole set**; it changed exactly one verdict, which
is also the honest effectiveness score for it: **1 of 26 subjects moved.**

> A screen that examines the right subjects can still be blind to a whole *reference
> kind*, and it announces nothing. Four independent-looking routes agreed here and all
> four shared the premise that a reference is either a rel32 or a stored value.

### 7.2 The one the caller channel announced itself

My first `call`/`jmp` decoder reported **13,664 call sites across 1 distinct target** and
a positive control of **0** where 354 was expected. objdump prints targets as `0x180…`;
my `[0-9a-f]+` matched the leading `0` of every `0x`, so every target decoded as address
zero. **The control caught it on the first run** — which is the whole argument for
carrying one: the tool was confidently wrong and internally consistent.

This is the AT&T/format trap the kickoff names, in a third costume: not the syntax, the
*literal formatting* of the operand.

### 7.3 And one place I was about to record a false blocker

Reading `FUN_1800259d0` alone, its `test $0x2,%dl` / `mov -0x8(%rcx),%r8d` array branch
looked like a vector-deleting form distinct from the `deleting_dtor.cpp` family, and I
began writing "this row needs a different recipe". Disassembling the **sibling** row
`0x18002ba90` (IO::TextFileStream, already in the table) refuted it in one step: the two
are the same instruction sequence, operand for operand, differing only in the two
constants the macro carries.

**A shape looks unfamiliar until you disassemble the thing it is supposed to resemble.**
Cost: one objdump.

---

## 8. WHAT I DID NOT ESTABLISH

1. **No live run of anything.** S2 and S3 of the oracle are `calls=0` because they were
   never executed, not because they ran and found nothing. No attendance figure.
2. **Zero stamper movement.** I moved no stamper from RETAIL to ours on any row. The
   brief asked for a number and the number is 0. What I moved instead was the
   *measurement* of how large each stamper block really is (§3).
3. **The 3 unattributed stampers** on `ReaderWriter` and `Stream@IO` — Wave 23 left them
   unchased and so did I.
4. **`??_7FileStream@IO@@6B@`'s 6 effective retail stampers** were counted but not
   individually scoped or sized.
5. **I did not sweep the tree for other phantom-literal control rows** (§1b) — I fixed
   only my own.
6. **`tools/mint_stamper_census.py` is unmodified.** The reachability column belongs in
   it; I left it alone because three lanes are editing shared tool files this wave.
7. **The exe side of the reachability screen is import-table only.** I did not sweep
   `Application-x64.exe`'s own `.text` for computed references, so R1 is a lower bound.
8. **`RtMintVtableTrapCount()` (lane VTGFX) was not adopted**, because this lane promotes
   nothing — there is no stamp for it to refuse. Its missing firing negative control is
   still open and is not mine.
9. **I did not re-derive Wave 19's window analysis** for `ReaderWriter`/`Stream@IO`. I
   took its verdicts as given after confirming the slot and stamper figures it rests on.

---

## 9. DENOMINATORS

- "1323 called / 1171 ported", "1325 / 1173": `tools/runtime_census.py` on **this branch
  only**. Branch-local; not a board figure.
- "ours/slots": census PROMOTION BACKLOG, **M=NONPURECALL**. The `RT_UNPROMOTABLE` tags
  use **M=TABLE**, which is why `??_7Stream@IO@@6B@` reads `2 2` in one place and `2/7`
  in another. Both declared.
- "stampers / ours-capable / RETAIL": `tools/mint_stamper_census.py`. `ours-capable` is an
  **UPPER bound** — a registration is a door, not proof every caller uses it.
- "eff." (§3): RETAIL minus stampers with no hit on any of R1–R5. A **lower bound on the
  block**, and its error direction is stated in §3.3.
- Extents: `.pdata` is a **LOWER** bound (MSVC chains fragments); next-symbol is an
  **UPPER** bound. Both are quoted everywhere; neither is presented as exact.
- "26 distinct bodies across 4 tables": union of non-purecall qwords read from retail
  `.rdata`, joined against a comment-stripped literal registration scan of
  `runtime/ engine/ code/ standalone/ dx11/`, both arms controlled.

---

## 10. RESIDUALS, EACH WITH A SCOPE AND AN ADDRESSEE

| # | item | scope | addressee |
|---|---|---|---|
| R1 | `??1PackageFile@IO@@UEAA@XZ` (318 B) is the **single** reachable retail stamper of `??_7PackageFile@IO@@6B@`; with slots 6 (29 B) and 7 (16 B) also trivial, the row is 3 small bodies + 1 `Serialize` from promotable | DLL-PRESENT | **the Wave 26 lane that takes IO vtables.** Do NOT schedule the copy ctor — §3 shows nothing can call it |
| R2 | `?Serialize@PackageFile@IO@@` (slot 5, 482 B LOWER) — **a `Serialize`, so the WRITE path is replayable**; "no oracle possible" was wrong for four Serialize ports across three waves | DLL-PRESENT | **the same lane as R1** |
| R3 | `tools/mint_stamper_census.py` should carry a **reachability** column; today its `RETAIL` count overstates the block by 53% on my rows (25 of 47) and cannot say so | tool, route-independent | **lane SWEEP**, or the Wave 26 instruments lane. §3.1 has the five screens and their controls |
| R4 | Run `BANISHED_RT_VTIO_ORACLE=1` (S2 + S3 of §4.1). S1 is already discharged offline | DLL-PRESENT | **lane SWEEP**, which holds the lock second, or the Wave 25 integrator on the merged tip |
| R5 | `??_7Thread@System@@6B@`: 1/1 COMPLETE, effective retail **1** (`FUN_18014a580`, the Loader static's initializer), NOT 0 | inverts at cutover | **lane MANIFEST** (§6.1) |
| R6 | `?Run@Stream@Audio@@` (1800 B) is the last slot of `??_7Stream@Audio@@6B@` **and** a reachable retail stamper of `??_7FileStream@IO@@6B@` — one body, two rows | DLL-PRESENT | **the Wave 26 lane that takes Audio or IO bodies** |
| R7 | A probe that passes a deliberately-absent mangled name as a **string literal** mints a phantom census gap row (§1b). I fixed mine; the tree was not swept | tool, route-independent | **lane SWEEP** |
| R8 | Engine's 4 vtable rows are **11 unported bodies** after this lane (was 17), not 43 slot-instances; `??_7UpdateLoop@` is COMPLETE and the other three need 6 / 2 / 4 more | DLL-PRESENT | **the Wave 26 lane that takes Engine** |

**Merged nothing.**
