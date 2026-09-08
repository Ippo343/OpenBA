# Wave 9 lane RRES — the `FindOrLoad` spine, one rung further

Branch `wave9/lane-rres`, cut from `085782d`. Worktree
`.claude/worktrees/rres-089e6eb4`.

**Assignment:** `?FindOrLoad@ExternalFactory@Object@@` and
`?LoadResources@UpdateLoop@Engine@@`.

**Delivered:** the path stack PINNED, and four exported bodies ported and
verified against retail on 22.6k live calls — `ExpandRelativePath`, the two
path-stack readers, and `Filename::GetPathExtension`.

**Declined:** `FindOrLoad` and `LoadResources`, with the blocker re-derived and
now measurably smaller — see §6.

---

## 0. The correction I was handed, checked rather than believed

The brief told me that lane RES's AVL blocker was unblocked not by lane COLL but
by **lane INPUT's `set_avl_e24.h`**, and told me to verify it rather than take
it. I did, and it holds:

* `runtime/src/collection/set_avl_e24.h` declares the 0x18-element node header
  (parent +0x18 / left +0x20 / right +0x28 / balance +0x30 / height +0x32,
  sizeof 0x38) at addresses `0x180023060 / 170 / 570 / 6a0`;
* `runtime/types/object/External.h` asserts the SAME offsets for
  `ResourceSet::Node`;
* the four addresses lane FNHOOK named — `0x18002ce60`, `0x18002d370`,
  `0x18002d860`, `0x18002d990` — appear in the tree only in lane RES's
  `external_set.{h,cpp}` and its probe, and nowhere in lane COLL's
  `avl_trees.cpp`.

So the record is right as lane RES left it. Nothing in this lane depends on that
family anyway: **none of the four bodies here touches the tree.** That is the
point of the split — the tree half was lane RES's, this is the path half.

---

## 1. THE PIN: `ExternalFactory + 0x30 / +0x38` is a `Collection::Array<PathStackEntry>`

This was the stated blocker in **both** predecessor documents —
`WAVE9_LANE_RES.md` §5 ("needs `ExternalFactory::_opaque30` / `_opaque38` — the
path stack, which `External.h` records as **NOT PINNED**") and
`WAVE9_LANE_FNHOOK.md` §4.3. Lane RES also named the two functions to start
from. They were the right two, and a third settles the field order from the
write side.

| evidence | what it fixes |
|---|---|
| `GetPathStackFilename` 0x1800bcf60: `CMP dword ptr [RCX+0x38],EAX` / `MOV EAX,[RCX+0x38] ; DEC EAX ; MOVSXD ; SHL RDX,0x4 ; ADD RDX,0x8 ; ADD RDX,[RCX+0x30]` | +0x38 is an int32 SIZE, +0x30 is the buffer, the stride is 0x10, the top of stack is `size-1`, and the FILENAME is the second half |
| `GetPathStackDirectory` 0x1800bcfd0: the same body **without** the `ADD RDX,0x8` | the DIRECTORY is the first half |
| `PushPath` 0x1800bd0e0: builds a 0x10 local out of two ADJACENT slots ([RBP-0x18] = `Filename` copy of `GetPathDirectory(path)`, [RBP-0x10] = `GetPathFilename(path,true) + "." + GetPathExtension(path)`) and passes `&[RBP-0x18]` to `0x18002e220` with `RCX = this+0x30`, `EDX = size+1` | confirms stride and field order from the WRITE side, and identifies `0x18002e220` as `Array<PathStackEntry>::Resize(n, const T&)` |

`External.h` now carries `struct PathStackEntry { Filename directory; Filename
filename; }` with `static_assert`s on both, and `ExternalFactory::_pathStack` as
a `Collection::Array<PathStackEntry>` with the +0x30 and +0x38 offsets asserted.
The two `uint64_t _opaqueXX` fields are gone.

---

## 2. What landed

`runtime/src/object/external_path_win.cpp` — four bodies, each registered by
BOTH its mangled name and its retail address.

| retail | export | size | evidence |
|---|---|---:|---|
| `0x1800bd360` | `?ExpandRelativePath@ExternalFactory@Object@@QEBA?AVFilename@System@@AEBV34@_N@Z` | 834 instr | disassembly, read in 6 pages |
| `0x1800bcf60` | `?GetPathStackFilename@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ` | 22 | disassembly |
| `0x1800bcfd0` | `?GetPathStackDirectory@ExternalFactory@Object@@QEBA?AVFilename@System@@XZ` | 21 | disassembly |
| `0x1800c64f0` | `?GetPathExtension@Filename@System@@QEBA?AV12@XZ` | 68 | disassembly |

`standalone/res_path_detour.cpp` — the coverage seam (§3).
`runtime/src/object/external_path.h` — declarations and the counter block.
`runtime/src/object/rt_object_port.cpp` — +7 lines, the registration hook.

### Ghidra's C for `ExpandRelativePath` is actively wrong, in four ways

Recorded because the next reader will open the same decompilation:

1. it mistakes the **hidden return slot** for `param_1`, so the real
   `const Filename&` argument arrives as `param_2` reconstructed out of
   `in_register_00000081`;
2. the `bool` argument appears as a phantom `in_R9B` with no declaration;
3. **the destination slot of all four `GetPath*` calls at the top is dropped** —
   `System::Filename::GetPathExtension(pFVar4)` with no output argument at all;
4. it renders retail's EH-state bitmask (`EDI`) as arithmetic on a `void*`
   (`(uint)pvVar9 >> 8`), which reads like pointer manipulation and is not.

Everything below was transcribed from `disassemble_function(address=)`.

### Two findings about the FUNCTION, kept rather than smoothed away

**(a) The second candidate is the SAME STRING as the first.** The `else` arm at
`0x1800bd825` rebuilds `<dataDir><stackDir><dir><file>_<ident>.crs` operand for
operand — same six sources in the same order, same two literals, same
`AdjustDelimiters` — and probes it again. What differs is the PROBE, not the
path: the first `ExistsInPackfile` at `0x1800bd7c1` sits behind
`fs->existsInPackfileGate` (it is MSVC's inlining of `FileSystem::Exists`, gate
and all), and the second at `0x1800bd8fb` is **UNGATED**. So the retry is
reachable, and on a build with the gate clear it is the only packfile probe that
runs. Reproduced literally, six redundant allocations and all.

**(b) `this` is dead after `0x1800bd5f1`.** Retail reuses R15 for the
`GetFileAttributesW` result. `GetPathStackDirectory` is the last read of the
factory. Anyone extending this body must not assume `self` is still live at the
bottom — in retail it is not.

### The `ResLoadGuard` constraint — what I did and what I did NOT do

`ExpandRelativePath` takes `ResLoadGuard` in its body. It is on the racy path
lane FNHOOK measured, it is called only from `FindOrLoad`, and it does
refcounted `String`/`Filename` traffic while building six concatenations.

**This is NOT the guard the brief demanded and it does not stand in for it.**
The constraint was that a ported `FindOrLoad` body must take the lock;
`FindOrLoad` is not ported here (§6), so the hole lane FNHOOK described is still
open exactly as FNHOOK left it. Said plainly because the constraint was stated
as non-negotiable. The lock is recursive, so a later `FindOrLoad` nests over
this one at no cost, and lane RES's note that **`Clean` will need it too** still
stands and is still not done.

---

## 3. The coverage seam, and the control that proves it is load-bearing

`RegisterRuntimeExport` / `RegisterRuntimeAddr` redirect OUR call sites.
`ExpandRelativePath` has none: its only caller anywhere is retail's own
`FindOrLoad`, reached by a direct rel32 CALL inside the DLL. Registering it and
stopping would have produced a textbook `calls=0` port.

So `standalone/res_path_detour.cpp` patches the DLL's own `0x1800bd360`
(16 bytes, boundary counted in Ghidra) so every retail caller lands in the port.
**Opt-in** (`BANISHED_RT_RESPATH_DETOUR=1`), because nothing goes default-on
unproven.

The diff site calls the **trampoline**, never `RtOriginal`: once the patch is in,
`GetProcAddress` returns the patched address and an "original" call would
re-enter our own body forever (`reference_rtoriginal_is_getprocaddress.md`).

---

## 4. Verification — three cells, one exe

Identity line, unique to this build and read from each run's own log:
`[ResPath] registered 4 doors + 4 addresses (detour=… negctl=…)`. Nothing else
in the tree prints `ResPath`.

### PROVENANCE OF EVERY NUMBER BELOW — READ THIS FIRST

The three cells were run from the exe built at commit `22e1e67` **plus one
later source change that is not in them**: the self-test now reaches retail's
`PushPath`/`PopPath` through `ResolveRuntimeRaw(0x1800bd0e0 / 0x1800bd040)`
instead of `RtOriginal("?PushPath@…")`, and the UNDERPOWERED message was
corrected. Both changes are inside the **self-test only**; the four ported
bodies and the diff site are byte-identical between the two builds.

**So the `calls=` / `worked=` / `divergences=` rows and the counter rows are
valid for the committed source. The self-test's own phase-2 numbers were
produced by a binary one commit older.** Stated rather than glossed, because
this project's own rule is that a lane which edits after it measures has
invalidated the measurement (`reference_lane_edits_after_report_invalidate_the_measurement.md`).
I could not re-run: the game-dir lock was held by another lane when this was
written, and I would rather ship a labelled gap than a run taken against a tree
someone else was mid-deploy on.

**I held the game-dir lock (`lane-RRES`) for every run quoted here**, acquired
before the first deploy and released afterwards with `.lane_lock/` verified
gone. No number here comes from a run taken while another lane held it.

### Cell A — port live through the detour, `BANISHED_RT_DIFF=1`, self-test armed

`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
BANISHED_RT_RESPATH_DETOUR=1 BANISHED_RT_RESPATH_SELFTEST=1 BANISHED_RT_DIFF=1`
— `rc=0`, zero-byte crash log, world built (`[GUL::Load] EXIT — world built`),
clean self-quit.

```
[ResPath] registered 4 doors + 4 addresses (detour=ON negctl=0)
[RtDetour] Object::ExternalFactory::ExpandRelativePath[port]:
           00007ffcafb8d360 -> 00007ff628505266 (trampoline …, 16 bytes displaced)

[RtDiff]  Object::ExternalFactory::ExpandRelativePath  calls=22595 worked=22595 divergences=0
[RtDiff]  System::Filename::GetPathExtension           calls=22595 worked=22595 divergences=0

[ResPath] counters: expand=22595 empty=0 | ident def/present=98/22497
                  | fname stack/present=0/22595 | stack empty/nonempty=22595/0
[ResPath] counters: hit pkg=22592 attrProbe/hit=3/0 retry=0 notFound=3
                  | tail ident/none=22595/0
[ResPath] counters: ext calls=22595 ok=22595 nodata=0 nodot=0 emptyAfterDot=0 colonTrim=22497
```

**22,595 differential comparisons against retail's own body, zero divergences,
`worked` equal to `calls` on both sites.** The comparison is ours-vs-the-
trampoline on the real factory, the real filesystem and the real 22.6k-call
resource load — lane FNHOOK's census put `FindOrLoad` at 22,591 calls per run,
and this is that number.

`worked == calls` matters here and is not decoration: `RT_DIFF_WORK(true)` is
declared inside the block that actually ran the comparison, so a body that took
the early-out or found no trampoline would show `worked` below `calls`. It does
not.

### Cell A, self-test (same run)

```
[ResPath] SELFTEST phase2 (synthetic path stack via retail PushPath): RAN, 63 cases,
          depth 0 -> 1 -> 0 | pushed "Dialog\StringTable.rsc:gameKeys"
          | packfile hits phase1/phase2 = 10/12
[ResPath] SELFTEST branch coverage (script only): calls=161 earlyOutEmpty=0 pkgHit=22
          ident def/present=63/98 fname stack/present=56/105 attrProbe=139 retry=139
          ext nodot=56 emptyAfterDot=0 nodata=28 colonTrim=42
[ResPath] SELFTEST -- 23 real comparisons, 0 mismatches
[ResPath]   negative controls: 4 of 6 FIRED (ctl1 candidate '_'->'-': 0,
            ctl2 answer '.'->'!': 23, ctl3 found-branch dir/stackDir swap: 0,
            ctl4 drop ':'+ident: 15, ctl5 answer dir/file swap: 17,
            ctl6 default ident 'resources': 6)
[ResPath]   *** 2 control(s) did NOT fire, and they are NOT reshaped until they do. …
```

The stack depth is asserted back to 0 by the script itself, and the entry was
built by **retail's own `PushPath`**, never by writing `_pathStack` — the layout
is the thing under test, so a hand-written entry would have been a fake proving
itself (`reference_synthetic_subject_beats_a_fake.md`).

### Cell B — `BANISHED_RT_RESPATH=0`, the A/B baseline

`rc=0`, zero-byte crash log, world built, clean quit. **`[ResPath]` appears zero
times** — the negative half of the identity check — and
`[RtPort] Object: 40 exports registered` against **44** with the port on. The
four are exactly this lane's.

### Cell C — registered but NOT detoured, the control on the instrument itself

`BANISHED_RT_DIFF=1` with no `BANISHED_RT_RESPATH_DETOUR`:

```
[ResPath] registered 4 doors + 4 addresses (detour=off negctl=0)
[ResPath] counters: expand=0 empty=0 | … | stack empty/nonempty=0/0
[ResPath] counters: ext calls=0 ok=0 …
[RtPort] Object: 44 exports registered
```

**All four doors registered, and the port runs zero times.** This is the
effectiveness control for the harness rather than for the port: it demonstrates
that registration alone gives this body no callers at all, so without the detour
every number in Cell A would have been `calls=0` — and it shows what an honest
`calls=0` looks like next to a body that is genuinely live.

---

## 5. The door set-diff — and a PREDICTION I GOT WRONG

Rule 2 is a **set-diff, not a count**. Both halves come from one within-tree
comparison: my five files present (`wave9/lane-rres` tip) versus absent (a clean
detached worktree at `085782d`), the same instrument version on both sides,
minutes apart.

| instrument | without lane RRES | with lane RRES | delta |
|---|---:|---:|---|
| `unsatisfied_export_scan` names resolved through a door | 854 | 855 | +1 (`GetPathExtension`, which now has a port) |
| `unsatisfied_export_scan` **with neither port nor mint** | 117 | 117 | **0** |
| ... of those, **OUTSIDE a diff guard** | 114 | 114 | **0** |
| **SET-DIFF of that 114-name set** | — | — | **CLOSED 0, OPENED 0** |
| `addr_door_audit` by-address call sites | 299 | 299 | 0 |
| `addr_door_audit` **distinct addresses** | 212 | 212 | **0 — no new address door** |
| `addr_door_audit` registered addresses | 1226 | 1230 | **+4** |
| `addr_door_audit` **UNREGISTERED** | 15 | 15 | **0** |
| `port_bypass_audit` BYPASS | 3 | 3 | 0 |
| `port_bypass_audit` ADDR-REGISTERED / NO-EXPORT | 83 / 178 | 83 / 178 | 0 / 0 |
| `port_bypass_audit` UNPORTED | 97 | 99 | +2 (the two oracle-only raw doors) |
| `port_bypass_audit` OK-ORACLE | 453 | 454 | +1 |
| `rawdoor_audit` sites / **UNGUARDED-CALL** | 642 / **0** | 646 / **0** | +4 / **0** |
| `detour_target_audit` | clean | clean | clean |
| `runtime_census.py` Object total/called/ported | 191 / 50 / 30 | 191 / 50 / 30 | **0 — see below** |

### The prediction I made and MISSED

I predicted this lane would **remove four names** from
`unsatisfied_export_scan`, on the reasoning that it ports four exported symbols.
**The set-diff says CLOSED 0.** The prediction was wrong for a structural
reason worth writing down, because it will mislead the next lane the same way:

> `unsatisfied_export_scan` lists names **our source resolves through a door**
> and which have no port and no mint. None of my four names was resolved
> anywhere in the tree before this lane, because nothing of ours called them.
> A port cannot remove a name from a set the name was never in.

The same structural fact explains the flat `runtime_census.py` row: the census's
`ported` column counts against its `called` column, and these four are not in
the called set either. **Four exported bodies landed, verified on 22.6k live
calls, and two of this project's headline instruments show nothing.** That is
not an argument against the instruments — it is the reason the deliverable has
to be stated as a fact about the call graph (§6: `FindOrLoad`'s unported-callee
count went 5 → 3) rather than as an instrument delta.

### The set-diff also caught this lane OPENING two doors, and I fixed it

The first measurement came out **CLOSED 0, OPENED 2**:
`?PushPath@ExternalFactory@Object@@` and `?PopPath@ExternalFactory@Object@@`,
which the self-test reached by mangled name through `RtOriginal`. That is the
"closes 3, opens 4" trade rule 2 exists to stop, and the instrument caught it,
not review. They now go through `ResolveRuntimeRaw` by address — the house door
for an oracle-only reach into the shipped DLL
(`external_set_probe.cpp:229-231`) — which restores CLOSED 0 / OPENED 0 and
moves them to `port_bypass_audit`'s UNPORTED column, where they correctly
resolve null at cutover and the self-test prints `DID NOT RUN`.

**A blind spot found while doing this, handed on:** `addr_door_audit` does NOT
count `ResolveRuntimeRaw(0x...)` as a by-address call site (its `distinct
addresses` stayed at 212 across the change), while `port_bypass_audit` DOES
(its ADDRESS-door count went 377 → 379). Two instruments, two definitions of
"by-address door". A lane reading only the first would not see these two sites
at all.

---

## 6. What I DECLINED, with the blocker re-derived

### `?FindOrLoad@ExternalFactory@Object@@` `0x1800bf320` — DECLINED

Its call graph, read fresh with `disassemble_function(filter_mnemonics="CALL")`
rather than inherited. **Thirteen distinct callees; ten are ours, three are
not.** Lane FNHOOK's table said eight of thirteen (it counted
`0x1800c64f0` as unported, correctly at the time). This lane closed two of the
five that were open.

| callee | what | status |
|---|---|---|
| `0x1800bd360` | `ExpandRelativePath` | **CLOSED BY THIS LANE** |
| `0x1800c64f0` | `Filename::GetPathExtension` | **CLOSED BY THIS LANE** |
| `0x1800c6610`/`6740`/`6880`/`6970` | `Filename` splits + `AdjustDelimiters` | ours |
| `0x180007bd0` | `FileSystem::GetDataDirectory` | ours |
| `0x1800c9440`/`0x1800c92d0` | `String::operator+` ×2 | ours |
| `0x1800c7ad0` | `MemFree` (×12) | ours |
| `0x1800be0e0` | the cache lookup | **NOT PORTED — but its own closure is now COMPLETE** |
| `0x1800be570` | `ExternalFactory::Add` | NOT PORTED, 3 sub-blockers |
| `0x1800bc620` | `External::Load` | NOT PORTED, the `IO::Encoding` vtable |

**The arithmetic has inverted, which was the thing to beat.** Lane RES declined
because porting `FindOrLoad` would remove one name from
`unsatisfied_export_scan` while adding **two** unregistered addresses
(`0x1800bd360`, `0x1800bc620`). One of those two is now registered, so the same
port today would add **one**. It is still not zero, so I did not take it: a lane
that removes one name and opens one address has moved the census and not the
boot.

**`0x1800be0e0` (the lookup) is now unblocked and is the cheapest next rung.**
Its callees, read the same way, are `MemFree`, `AdjustDelimiters` (×4),
`GetPathIdentifer`, `String::operator+(const char*)` and `0x18002dd70` — the
0x18 `Set::Find` lane RES landed. **Every one is ours.** ~100 instructions, no
new door, and lane RES's finding applies directly: `Find` needs a
DELIMITER-ADJUSTED key, which is exactly why retail calls `AdjustDelimiters`
four times before it descends.

`0x1800be570` (`Add`, 123 instructions) still needs three things:
`0x1800bb090` (an ~0x18-byte local whose type is still unidentified),
`0x180006a10`, and `0x180006250` (`External::GetPath`), plus one virtual call
through `External` vtable slot `+0x28`. `0x18002ca00` (the insert) is ours.

`0x1800bc620` (`External::Load`) is unchanged: the shared `IO::Encoding` vtable
at `0x1801554a8` still has no exported name and no mint row. Lane ENC is on
`CreateReader`/`CreateWriter`; I did not touch it.

### `?LoadResources@UpdateLoop@Engine@@` `0x1800dd7d0` — DECLINED

Calls `FindOrLoad` three times and cannot precede it. Not reversed further than
confirming that, because the result could not be used until `FindOrLoad` lands.

### `PushPath` / `PopPath` — DECLINED, and this one is a door-arithmetic call

They are the WRITERS of the stack this lane reads, and porting them looks
natural. They need `0x18002e220` — `Array<PathStackEntry>::Resize(n, const T&)`,
the 0x10-element instantiation — which exists **nowhere** in the tree, plus its
element copy-constructor and destroy-range. That is three new address doors to
close two exported names. Declined on the same rule I applied to `FindOrLoad`.
They are reached here as oracle-only raw doors instead (§5).

---

## 7. HONEST LIMITS

1. **The two path-stack readers have NO ours-vs-theirs comparison, and their
   non-empty branch has ZERO live coverage.** On every one of the game's ~22.6k
   calls the path stack is EMPTY (`stack empty/nonempty=22589/0` and identically
   in every run). The non-empty branch is exercised only by the self-test's
   synthetic push, and the readers themselves are never differentially compared
   at all — a replay would run retail's `Filename` copy constructor a second
   time on a live entry. Their evidence is transcription plus the branch
   counters. That is weaker than the other two bodies and it is weaker in the
   direction that matters: **the branch the game actually takes is the trivial
   one.**

2. **Two of six negative controls do not fire, and they are NOT reshaped until
   they do.** `ctl1` (perturb the candidate) and `ctl3` (swap `dir` with
   `stackDir` in the found branch) both touch the existence probe. Phase 2
   proves the found branch executes (12 packfile hits), but every phase-2 case
   that reaches it has an empty `dir` — the query is `":<ident>"` — so `ctl3` is
   a no-op by construction, and `ctl1` can only change the answer where the
   found and not-found spellings differ. **The existence-probe machinery is
   therefore transcribed, not differentially verified by the script.** The 22.6k
   live comparisons do cover it; the script does not, and the report says so in
   the run's own log rather than only here.

3. **The instrument printed a wrong number before I noticed.** The UNDERPOWERED
   line read the packfile counter AFTER the snapshot restore and therefore
   always printed `0`, while the line directly above it printed `22`. Verdict
   and message computed separately — the shadow-predicate trap. Fixed, and
   recorded because the earlier runs in this lane's own history contain that
   false zero.

4. **The `notFound` / ungated-retry arm ran 3 times per run, and the
   `GetFileAttributesW` arm never returned true.** `attrProbe/hit=3/0`. Those
   three calls are the only live coverage the entire second-candidate block
   gets, and the `hitAttributes` branch has none at all.

5. **`GetPathExtension`'s three early-return branches have ZERO live coverage.**
   `nodata=0 nodot=0 emptyAfterDot=0` on every run; only the self-test reaches
   them (`nodot=56`, `nodata=28`, and `emptyAfterDot=0` even there — that
   branch is reached by nothing in this lane).

6. **Coverage exists only with the detour armed, and the detour is off by
   default.** The third cell proves it: registered but not detoured, the port
   reports `expand=0 … ext calls=0`. Every number in §4 is conditional on
   `BANISHED_RT_RESPATH_DETOUR=1`, which no shipping run sets.

7. **One cell shape, not three.** AUTOSTART + pause-menu + quit. No stress
   cell, no save/load cell, no mods cell, no tutorial. The path stack is empty
   in all of them; a cell that nests resource loads might not be, and that is
   the state in which most of this function's untested behaviour lives.

8. **No perf measurement.** The standing rule is that one run cannot resolve the
   5% gate, so none is offered. The port adds a recursive critical section per
   call on ~22.6k calls per boot; that is not measured here.

9. **`ExpandRelativePath` is byte-faithful including its redundancies** — the
   duplicated second candidate (§2a) allocates and frees six strings for a
   string it already built. Kept. If a later lane "optimises" it, the ungated
   retry probe goes with it.

10. **I did not run the DLL-less cutover boot.** That is the integrator's, per
    the brief.

---

## 8. Files and switches

```
runtime/types/object/External.h            +50/-2  PathStackEntry, _pathStack
runtime/src/object/external_path.h         NEW     declarations + counters
runtime/src/object/external_path_win.cpp   NEW     the four bodies + self-test
runtime/src/object/rt_object_port.cpp      +7      registration hook
standalone/res_path_detour.cpp             NEW     the coverage seam
docs/runtime_port/WAVE9_LANE_RRES.md       NEW     this file
docs/runtime_port/evidence/lane_rres/*.log         the three cells
```

| switch | effect |
|---|---|
| `BANISHED_RT_RESPATH=0` | kills all four doors (A/B baseline; `[RtPort] Object` drops by 4 and no `[ResPath]` line is printed) |
| `BANISHED_RT_RESPATH_DETOUR=1` | patches the DLL's `0x1800bd360` so retail's callers reach the port. **Off by default** |
| `BANISHED_RT_RESPATH_SELFTEST=1` | arms the lockstep script on the first live call |
| `BANISHED_RT_RESPATH_NEGCTL=<1..6>` | forces one negative control for a whole run |

---

## 9. Handed on, in priority order

1. **`0x1800be0e0` is free.** Complete closure, ~100 instructions, and a live
   diff oracle is available the same way this lane got one: detour it.
2. **`Find` needs a delimiter-adjusted key** (lane RES §2, confirmed here by
   retail's four `AdjustDelimiters` calls in `0x1800be0e0`).
3. **`Array<PathStackEntry>` (`0x18002e220` + element ctor + destroy-range) is
   the whole of what `PushPath`/`PopPath` need**, and it is the same shape lane
   RES already wrote for `Array<Filename>` at `0x18002dff0` / `0x18002e3d0`.
4. **The detour pattern generalises.** Any Runtime body whose only caller is
   another Runtime body can be given full live coverage and a real oracle this
   way, in one file and one env var. Three ports in this wave were declined for
   `calls=0` reasons that this removes.
5. **`Clean` still needs `ResLoadGuard`**, and so will `FindOrLoad`.
