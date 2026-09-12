# WAVE 28 CLOSED — 2026-09-12

Seven lanes, all merged onto `wave28/base`. Every lane was a **decomposition-only
lane**: no lane was permitted to build, fix or extend a tool. Board re-derived on
the final merged tip, **after** the `cutover_manifest.inc` regeneration.

## 1. THE HEADLINE IS NOT THE BOARD

```
              called   ported        gap
  opened        1430     1277  89.3%  153
  closed        1453     1294  89.1%  159
```

**The percentage fell again and the gap grew by 6.** For the second consecutive
wave that is the arithmetic of a faithful port enlarging the cutover surface:
`called` grew by **23** while `ported` grew by **17**. Do not read this table as
the wave's result. The wave's result is §2.

### 1.1 THE NUMBER THAT MATTERS

```
  FACTORY FAMILY        opened:   2 of 103 present, 101 ABSENT
                        closed: 103 of 103 present,   0 ABSENT
```

**The 202-body factory family is CLOSED.** Re-derived on the merged tip with
`tools/mint_factory_census.py`: `slot-0 factory name PRESENT in tree: 103`,
`ABSENT: 0`, with the one row that has no factory in retail either
(`?_typeInfo@Base@Object@@`) correctly excluded.

**The census cannot see any of this.** Measured two-sided by lane FACTUI2 and
reproduced by the integrator: the census CSV contains **zero** rows matching
`CreateNewObject` — not this wave's 206, not Wave 26's `StringTable@UI`, not Wave
11's `Dialog@UI`. Nothing in our tree *resolves* those symbols, so they are
outside the population entirely. **The board could not have moved for the three
factory lanes however much they landed**, and a flat board was the correct
expectation, not a shortfall. This was discovered mid-wave and pushed to every
running lane; without it a factory lane following its brief literally would have
gone hunting for a phantom name that did not exist.

### 1.2 THE OTHER IDENTITIES

```
  PROMOTION BACKLOG   175 bodies / 41 tables / 34 COMPLETE
                  ->  191 bodies / 42 tables / 38 COMPLETE
  gap 159 = 24 BODY + 0 DTOR + 0 OPNEWDEL + 38 VTABLE + 49 DATA + 48 UNKNOWN
  REAL remaining vtable work   18 (unchanged; 20 of the 38 are UNPROMOTABLE)
```

- **The DTOR and OPNEWDEL gap columns are both 0.** They were the wave's only
  such rows and lane MISC closed both.
- **Only FOUR tables in the whole backlog are still incomplete:**
  `EdgeMeshInstance` 8/9, `MeshInstance` 6/9, `SunLight` 9/11, `Stream@Audio` 1/2.
- **Graphics 77.8% -> 81.8%**, ceiling 96.0% -> 97.0%, BODY rows 12 -> 8.
- **UI 95.8% -> 96.7%.**
- `internal` UNKNOWN grew 38 -> 48: address-keyed rows the classifier refuses to
  guess at, pulled in by this wave's ports. Not 48 bodies.

## 2. CUTOVER ATTEMPT 31 — THE WALL MOVED, AND OFF THE FACTORY FAMILY

| attempt | wall | cleared by |
|---|---|---|
| 30 | `?CreateNewObject@SpriteSheet@UI@@` — the mint's per-slot FATAL | **FACTUI2** (ported it first, alone, in its own commit) |
| 31 | **`RtUi_SpriteSheet_Serialize+0x1b7`** — EXEC-of-0, our own code | open |

**There is no `[RtMint] FATAL` anywhere in the attempt-31 logs.** The factory
wall that gated attempts 26 through 30 — four consecutive waves, one symbol per
boot — is gone as a class, not as an instance.

The new wall is one step further into the same object and is a different *kind*
of thing. Stack, symbolised against the linked image:

```
  #0  0x0                       <-- EXEC of 0
  #1  RtUi_SpriteSheet_Serialize        +0x1b7
  #2  RtObject_External_Load            +0x592
  #3  RtObject_ExternalFactory_FindOrLoad +0x2b1
  #4  StandaloneEnsureSpriteSheetLoaded +0x14b
  #5  Game::RunGameMain                 +0x97c
```

So the SpriteSheet now **constructs** and boot proceeds into **loading** one,
where `Serialize` calls a null function pointer. **`RtUnresolved` count for the
whole run is ZERO** — this is not a resolve miss, so it is a null slot or a null
stored pointer, not a missing registration. That is a concrete, well-localised
Wave 29 subject: read the disassembly at `+0x1b7` and find what it calls.

Boot reaches `input bindings + name tables` at **t=535.00ms**, the same phase
attempt 30 reached at t=583.58ms. Same phase, different failure, and the earlier
one is gone.

### 2.1 THREE FAILED RUNS BEFORE THIS ONE, AND WHAT THEY COST

The integrator ran the cutover **four times** and the first three were arm
errors, not results:

1. **No `BANISHED_RT_NO_DLL`** -> died at `LoadLibrary`, never reached the
   cutover. **This is Wave 27 §7.3 verbatim, repeated by the same role one wave
   later**, in a wave whose own kickoff warning 11 says knowing a rule and
   writing it down does not execute it.
2. **`BANISHED_RT_DIFF=1` added on a guess** -> EXEC-of-0 at t=6ms, because
   RtDiff runs the original alongside the port and with the DLL absent the
   original is null. The arm was inferred from a `grep` of attempt 30's *log
   text* for `BANISHED_*` strings — which matches printed prose, not the
   environment. Attempt 30 had RtDiff **OFF**; `grep -c "RtDiff] ON"` on its log
   returns 0, and that one command would have settled it.
3. **No `BANISHED_RT_ALLOC`** -> t=61ms, dead on
   `?Startup@MemoryAllocator@System@@`. The allocator is **REQUIRED at cutover**
   and harmful with the DLL present — the route-scoped rule this project has
   already written down.

**What caught #2 was a CONTROL, and it is the one methodological win of the
integration.** Faced with "the merged tip dies at 6ms where attempt 30 reached
583ms", the temptation was to bisect seven lanes. Instead the pre-merge tree was
built from `backup/pre-wave28-integrate` and run **with the same arm**: it died
at t=6.33ms with the identical fault and identical registers. That is the
script's own header's reason for existing — *otherwise a moved wall cannot be told
from a moved tree* — and it converted an apparent seven-lane regression into a
one-line arm error in a single run.

**Wave 29 must reconstruct the arm from an ATTEST line, never from a log grep.**
`[RtAlloc] ATTEST: ARMED/UNARMED` and `[RtDiff] ON` are printed *by the code that
read the variable*. The environment is not recoverable from prose.

## 3. WHAT EACH LANE DID

| lane | landed | did not |
|---|---|---|
| FACTUI2 | 35/35 classes, 70 slots + 32 ctors; **`SpriteSheet` first, alone** | 2 ctors needing a private `CacheText` |
| FACTREST | 35/35 non-UI classes, 70 slots, 68 fns; live control **2 -> 74 PORTED slots** | — |
| FACTUI1 | 31/31 classes, 74 registrations; **3-pass byte oracle, 71/71 identical** | — |
| SLOTS | **`0x180157c60` IDENTIFIED**, 12 slot bodies, 3 tables closed | `Stream@Audio` (1800 B, size judgement) |
| PARTSYS | **`Update@ParticleSystemInstance`** (0x147a), PSI table 10/10, SunLight 6->9/11 | 2 SunLight draw slots |
| GFXBODY | 8 of 10 Graphics BODY rows | 3 large inlined-quaternion bodies |
| MISC | 5 of 6 rows incl. `Audio::Scene::Update` **exercised, 799 calls** | `SetupFileSystem` (2616 B, size judgement) |

`0x180157c60` is **the vtable of `IXAudio2VoiceCallback`**, implemented by an
internal unexported class embedded 64x inside `Audio::Device` at `+0xa98`, stride
0x20 — one per audio channel. **The interface is IDENTIFIED; the class name is
UNIDENTIFIED and not recoverable from the retail file.** SLOTS did not invent a
name to fill the column, and that is the standard.

## 4. THE FINDINGS THAT OUTLIVE THE WAVE

### 4.1 A reader can name a field's TYPE and never its MEANING

Wave 27 named the particle record's fields from `ParticleInstance::Draw`, which
only *reads* them. **A 3-float pair lerped by an alpha is indistinguishable
between a position and a colour even in principle from the read side.** PARTSYS
ported `Update` — the writer — and found `+0x28` is the particle **colour**
(written as `byte/255.0f`; a byte over 255 is not a coordinate) and `+0x40` its
**alpha** (both halves written with one float in a single 8-byte store, making
the reader's lerp a no-op *by construction*).

Per kickoff warning 13 it repaired the **licence**, not the two sentences: Wave
27's constants are deliberately not renamed, and the correction sits in the file
the next particle lane opens.

**Corollary, handed to MISC and now general: a `Serialize` is a WRITER**, so it
outranks any field naming taken from code that only reads those fields.

### 4.2 Retail ships EMPTY bodies, and porting one is a defect

**Three lanes found this independently, in three shapes:** FACTUI1 4 of 31
factories and FACTREST 10 of 34 are `xor eax, eax ; ret` — the class is not
constructible through the object model; PARTSYS found two `SunLight` slots that
are literally `C2 00 00` + INT3 **with no `.pdata` entry at all**.

Writing the generic alloc+ctor body for one of those makes a null-returning
factory allocate and construct. **No census distinguishes an abstract slot from
an unported one.** FACTUI1's oracle is the only instrument in the wave that can:
it *calls* retail's function and observes that it really returns null, rather
than reading two bytes and inferring.

The split between the two factory lanes (13% vs 29%) is not a discrepancy —
**leaf widgets vs base classes**, and both lanes said so independently.

### 4.3 `.pdata` understates by up to 24x, and understating makes work look cheap

PARTSYS: `?Update@ParticleSystemInstance@` sizes at **0x9d** from its first
`.pdata` entry; the full `UNW_FLAG_CHAININFO` chain is **0x147a**. That is **24x**,
against the 12% instance the existing warning was written about — *on the very
function that instance was written about.*

SLOTS found the same failure wearing different clothes: an `RT_UNPROMOTABLE` tag
**said 0xd0 bytes; a chain walk measures 720** (8.7x). **That is three lanes this
wave finding a wrong or stale fact inside an annotation whose entire purpose was
to inform the next reader.** An annotation is not evidence; it is a claim with a
timestamp nobody checks.

### 4.4 THE "STILL OPEN, AND HERE IS WHY" SECTION IS AS STALE AS THE CHEAP-WINS LIST

MISC's finding, and it generalises Wave 27 §4.5 one level up. **All four stated
blockers it was handed misdescribed reality, and the two that mattered named the
WRONG OBSTACLE rather than being merely out of date:**

- `?Update@Scene@Audio@@` was held for twenty waves on "it needs a snapshot/
  restore of the Random state". That capability has existed since **Wave 2**. The
  RNG was never the obstacle.
- `?SetupFileSystem@Modification@IO@@` was recorded "NOT VERIFIABLE, the only
  unattended mod route is the broken one". **Measured false**: it is reached on
  the ordinary autostart path via `Loader::Load`, proven by a residue row from a
  run with no mods env var set.

> The census re-derives what **is** ported every wave. The **reasons rows stay
> open** get copied forward as prose, and re-reading prose is not re-deriving it.

**The tell is cheap: a blocker naming a MISSING CAPABILITY is a claim about the
tree and expires. One naming an INTRINSIC property does not.** Three items cost
one grep, one grep, and one 3-minute run.

FACTREST hit the same trap from the other side: its draft residual asserted
twelve constructors were unported; re-checking each by its registration call
showed **five were already ported**.

### 4.5 A DEFLATING instrument blind spot, found by two lanes in one wave

`runtime_census.py` is blind to the entire factory family (§1.1) and, MISC
proved by experiment, **silently drops a macro door whose first argument is an
inline function-pointer type** — moving a typedef changed TOTAL 1434 -> 1435.

Direction decides urgency, as Wave 27 §4.7 established: an inflating screen is
self-limiting; **a deflating one reports that work does not exist and nothing
detects that.** Two deflating blind spots in one wave, in the instrument the
whole project scores itself with.

### 4.6 Cross-lane instrument validation, in a wave forbidden from building instruments

SLOTS **validated its own readers against other lanes' measured failures before
trusting them**: it handed its operand reader a positive control for the exact
blind spot that had defeated GFXBODY's (which missed the no-displacement
`MOV [RCX],EAX` form and reported the same impossible hole in *both* Camera
bodies — consistent, and wrong), and validated its chain walker against PARTSYS's
0x9d -> 0x147a case, before believing its own `UNPARSED: 0`.

**The no-tooling rule produced better verification, not less.** That is the
opposite of what the rule's critics would predict and it is worth stating plainly.

### 4.7 Two write-width traps that survive every semantic check

- **`Camera::operator=` is NOT the copy constructor** (GFXBODY). The copy ctor
  writes `+0x60` as a **DWORD**; `operator=` writes it as a **single BYTE** and
  resumes at `+0x64`. The obvious body — "the same 53-dword loop as the sibling",
  and that loop is sitting in `wave9_lane_gfxd.cpp` — writes three bytes retail
  leaves alone, into a live Camera, **3.3 million times a session**.
- **`MeshInstance::AddDrawItems` touches 19 fields of `this`; the
  `EdgeMeshInstance` override touches exactly one** (SLOTS). Ported together and
  compared as operand **sets**, so the divergence was caught by construction
  rather than by vigilance.

Both were found as a **written-byte set difference over the instruction stream**.
Neither is visible in decompiled C. **And a set difference over the wrong extent
gives a real answer to a different question** — PARTSYS's first pass used too wide
a window and reported eleven distinct RNG draws where the truth is ten identical
plus one benign encoding variant.

## 5. WHAT IS EXPLICITLY NOT CLOSED

- **The overwhelming majority of this wave's bodies are UNVERIFIED by execution.**
  ~206 factory slots have attendance 0 by design (the DLL reaches its own
  factories through a static-initialiser-stamped pointer, which is not a door).
  PARTSYS's five bodies: attendance 0. GFXBODY: six of eight printed nothing.
  **Exactly one body in the entire wave was exercised on a live route** —
  `?Update@Scene@Audio@@`, 799 calls, by MISC. Wave 27's ~70-body verification
  debt is now roughly **280**, and this wave was explicitly scoped not to pay it.
- **FACTUI1's byte oracle is the only thing standing behind ~206 factory bodies**,
  and it is an **offline** oracle: it proves our bytes match retail's, not that
  anything called them. It is preserved at
  `docs/runtime_port/wave28_factui1_oracle.cpp.txt`, deliberately NOT under
  `tools/` and deliberately `.txt`. **Promoting it is Wave 29's decision.**
  Whether writing it was tooling at all is a judgement the integrator accepted
  and is recording rather than burying.
- **`standalone/rt_data_mint_tables.inc` is STALE: 68 vtables ENTERED, 0 LEFT**,
  because the factory ports pulled 68 new classes into the tree's reference set.
  **It was deliberately NOT regenerated.** Regenerating introduces 68 minted
  tables that no lane built or tested, mid-integration, into the same boot that
  measures the wave — the exact "second variable inside the same A/B" that Wave
  26 FACTORY refused. **Wave 29 owns it, with its own measurement.**
- **Nothing was promoted.** PARTSYS declined with a measurement, closing Wave 27
  GFXPART residual 1: we own 1 of 3 stampers for `ParticleSystemInstance`, 2 of 3
  for `SunLight`. **38 COMPLETE tables is not 38 cheap promotions.**
  `mint_stamper_census.py` re-run on the merged tip prints its own standing
  caveat: do NOT read `ours-capable` as ownership while `unattributed` is
  non-zero.
- **Four backlog tables remain incomplete** (§1.2), and `Stream@Audio`'s single
  slot is `Stream::Run`, measured at 1800 bytes — declined by two lanes as a size
  judgement, stated as such rather than dressed as a blocker.
- **`?SetupFileSystem@Modification@IO@@`** — now known reachable (§4.4) and still
  unported, 2616 bytes, half inlined `System::String` refcount release on the
  live loader path.
- Every instrument defect below.

## 6. INSTRUMENT DEFECTS RECORDED AND NOT FIXED

Carried forward with Wave 27's twelve.

1. `runtime_census.py` cannot see the factory family at all (§1.1, §4.5).
2. `runtime_census.py` drops a macro door whose first argument is an inline
   function-pointer type (MISC, proved by experiment).
3. `tools/ghidra_mcp/.venv/` **does not exist in a lane worktree** — only
   `requirements.txt` is checked in. The kickoff's literal census command fails
   for every lane. Reported by four lanes independently.
4. **The session scratchpad is SHARED between sibling lanes** and files were
   clobbered mid-run on basename collisions (`exp.py`, `pdata.py`, `exports.py`).
   FACTUI1's `exp.py` was overwritten during analysis.
5. `/tmp` is shared between lanes with a **worse** failure mode (MISC): a
   short-circuited command read a sibling's stale census file and printed the
   *exactly correct expected number*.
6. A backgrounded `mingw32-make` wrote a **zero-byte output file and reported
   exit 0** (GFXBODY; FACTUI1 hit it independently).
7. **Every lane's `*Summary()` is written and nothing calls it** (GFXBODY), so
   counter-based attendance numbers may be structurally invisible — a zero means
   "never printed", not "never called".
8. `rt_deleting_dtor_verify.py`'s bare `else` reports UNRECOGNISED shapes as
   `BAD` (carried from Wave 27, hit again).
9. `runtime_census.py --csv` takes a value, not a redirect (PARTSYS).
10. 7 pre-existing STALE VERDICT rows the census prints and nobody retires.

**The clearing move for #4 and #5 is RE-DERIVATION, not inspection.** FACTUI1
regenerated its 637-line table from scratch in a private directory and diffed it
byte-identical. **An artefact that cannot be cheaply re-derived cannot be cleared
of this at all.** GFXBODY, PARTSYS and SLOTS each re-derived everything and all
three reproduced exactly.

## 7. MY OWN ERRORS, RECORDED

1. **The kickoff headline said 34 non-UI factory classes; its own itemised list
   sums to 35.** FACTREST caught it and worked the real list. **The itemisation
   carried the truth and the headline did not** — the exact defect shape this
   project logs repeatedly, committed in the document instructing everyone else
   to watch for it. It cost nothing only because the brief said to re-derive.
2. **I propagated "12 inlined RNG draws" into the kickoff AND the PARTSYS brief
   without re-deriving it. It is ELEVEN** — five independent instruction
   signatures, each appearing exactly 11 times. `WAVE27_CLOSED.md` §5 and
   `GFXPART` §6.1 both say twelve; I copied it forward twice while telling lanes
   not to trust a handover's figures.
3. **I ran the cutover with three successively wrong arms** (§2.1), the first of
   which is Wave 27 §7.3 verbatim. The arm was inferred by grepping a log for
   `BANISHED_*` strings, which matches printed prose rather than the environment.
4. **I corrupted `rt_ui_port.cpp` during conflict resolution** — a `replace` on
   `'=======\n'` matched inside a banner comment's `====` run and joined two
   lines. Caught by inspecting the file rather than by the build. Redone
   line-exact against `^=======$`.

**1, 2 and 3 are one failure: I quoted rather than re-derived, in a wave whose
standing warnings told every lane not to.** The lanes complied; the integrator
did not.

## 8. GATES, ON THE FINAL MERGED TIP

```
mingw32-make standalone-debug        exit 0, FOREGROUND
  artefact                           33,753,900 bytes, mtime after last edit
  find <sources> -newer <exe>        EMPTY
  nm  <6 lane register fns>          all present in the LINKED IMAGE
  strings <5 mangled symbols>        all present in the LINKED IMAGE
cutover_manifest.py --gate           OK (59 rows), REGENERATED ON THE MERGED TIP
mint_factory_census.py               103/103 PRESENT, 0 ABSENT
runtime_census.py                    1453/1294, 89.1%
rt_mint_gen.py --check               STALE: 68 ENTERED, 0 LEFT  <-- deliberate, S5
cutover attempt 31                   factory wall CLEARED as a class; new wall named
deleting-dtor index audit            kSpecCount 50, NO lane claimed an index
```

The strengthened build gate (artefact + `-newer` + `nm` + `strings`) is FACTUI1's
and it earned its place: **exit code plus file size cannot distinguish "linked my
work" from "relinked without it".**

## 9. WAVE 29 INPUTS

1. **`RtUi_SpriteSheet_Serialize+0x1b7`** — the live cutover wall, in OUR code,
   with zero unresolved symbols in the run. A null slot or null stored pointer.
   Read the disassembly at the offset. **Strongest input, and unlike the last
   four walls it is one function, not a family.**
2. **The mint tables: 68 ENTERED, 0 LEFT.** Regenerate with its own measurement
   and its own cutover boot, as a deliberate step. Do not fold it into another
   change.
3. **A VERIFICATION lane, and the debt is now ~280 bodies.** Two waves have
   deferred it. Start with whether FACTUI1's offline oracle should be promoted
   into `tools/` and retargeted across the factory family.
4. Ledger items #1-#5 above are the DEFLATING ones and outrank the rest;
   #3, #4 and #5 are wave-infrastructure and cost every lane something this wave.
5. Four incomplete backlog tables; `Stream::Run` (1800 B); `SetupFileSystem`
   (2616 B, now known reachable); GFXBODY's three inlined-quaternion bodies.
6. **A PROMOTION lane with its own measurement** for the 38 COMPLETE tables —
   still not an integration step, and still gated on owning the stamper.
