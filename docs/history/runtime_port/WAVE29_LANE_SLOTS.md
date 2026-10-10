# WAVE 29 LANE SLOTS — the 16 unported named-trap slots are SEVEN

Branch `wave29/slots`, cut from `wave29/base` at `c11511a1`.

---

## 0. THE HEADLINE — `tools/mint_slot_sweep.py` CANNOT SEE A REGISTRATION WHOSE MANGLED NAME IS SPLIT ACROSS TWO SOURCE LINES

**Sixty-seven of this tree's registrations are invisible to it. Nine of the
sixteen slots this lane was briefed to port were ALREADY PORTED behind that
blind spot, two of them since Wave 27. I was four keystrokes from re-porting
`?SetParticle@ParticleInstance@` and `?SetParticle@ParticleSystemInstance@`,
bodies the tree has had for two waves.**

The defect is at `tools/mint_slot_sweep.py:108`. It builds its ported set from
`git grep -h` output parsed **line by line**:

```python
reg_names = set(re.findall(r'RegisterRuntimeExport\(\s*"([^"]+)"', tree))
```

A C adjacent-string concatenation split across two source lines yields only the
**first fragment**, which never matches a mint-table name. The canonical
instance is one address away from this lane's own scope:

```
runtime/src/graphics/wave28_lane_partsys.cpp:1084
    RegisterRuntimeExport("?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@"
                          "V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z",
```

**DIRECTION: INFLATING.** It manufactures port work, which per kickoff warning 8
is the self-limiting direction — but it manufactured *an entire lane's brief*,
and the failure mode it invites is a lane porting what the tree already has,
which this project has recorded three times in one wave
(`reference_grep_for_the_port_before_writing_it.md`).

**`runtime_census.py` WAS ALREADY FIXED FOR EXACTLY THIS** — multi-line literals
are the first of its three recorded blind spots
(`reference_census_three_blind_spots.md`). Its sibling tool was not, and nothing
detected the divergence. That is the finding that outlives the wave: **a defect
repaired in one instrument does not propagate to the instrument beside it, and
no screen in this project compares two tools' answers to the same question.**

### 0.1 The corrected board, re-derived over all 64 minted vtables / 589 slots

Instrument: `<scratchpad>/lane_SLOTS/slots_regscan.py` (joins adjacent literals,
walks whole files rather than grep lines) + `slots_truecount.py` (slot vectors
read from the retail `.rdata`, not from the generated `.inc`).

**Its control, per kickoff warning 7** — a screen looking for the ABSENCE of a
shape must be handed one: the known positive is that `?Draw@SunLight@` split.
The scan **recovers it** and the line-by-line regex **provably does not**; both
are asserted at run time and the tool refuses to print anything else if the
control fails.

```
  UNPORTED, line-by-line (what mint_slot_sweep.py says): 16
  UNPORTED, multi-line aware (the truth)               :  7

  table                                       tool  truth
  ??_7ParticleInstance@Graphics@@6B@            4     0   <-- ALREADY 10/10 COMPLETE
  ??_7ParticleSystemInstance@Graphics@@6B@      4     0   <-- ALREADY 10/10 COMPLETE
  ??_7MeshInstance@Graphics@@6B@                3     3
  ??_7SunLight@Graphics@@6B@                    3     2
  ??_7EdgeMeshInstance@Graphics@@6B@            1     1
  ??_7Stream@Audio@@6B@                         1     1
```

**TWO GRAPHICS VTABLES ARE COMPLETE AND NOBODY KNEW.** The nine phantom slots:

| phantom slot | ported by | slots it occupies |
|---|---|---|
| `?Draw@ParticleInstance@…MHHH@Z` | `wave27_lane_gfxpart.cpp` | 2 (both particle tables) |
| `?AddDrawItems@ParticleInstance@` | `wave27_lane_gfxpart.cpp` | 2 |
| `?GetInstanceTransform@ParticleInstance@` | `wave27_lane_gfxpart.cpp` | 2 |
| `?SetParticle@ParticleInstance@` | `wave27_lane_gfxpart.cpp:1139` | 1 |
| `?SetParticle@ParticleSystemInstance@` | `wave27_lane_gfxpart.cpp:1143` | 1 |
| `?Draw@SunLight@…MHHH@Z` | `wave28_lane_partsys.cpp:1084` | 1 |

Also on the 67-name list and in the neighbourhood of other lanes:
`?AddDrawItem@Renderer@`, `?Draw@Renderer@`, `?BeginDrawGroup@Renderer@`,
`?ApplyEvents@Animation@`, `?Create@TextFileStream@`,
`?PlatformSerialize@ImageBuffer@Video@`, and eight `FileSystem@IO` members.

> **[ADDRESSEE — the multi-line blind spot] lane INSTR of this wave**, which
> already owns the census's two deflating blind spots and the instrument ledger.
> The fix is four lines: read whole files, join adjacent literals, keep the
> control. **EXPIRY CONDITION: the moment `mint_slot_sweep.py` prints 7 rather
> than 16 at this tip.** This lane did NOT fix it — §7 says why — and the
> scanner that proves the number is in the scratchpad, not in `tools/`.

---

## 1. NAMING THE SIX TABLES — ALL SIX, FROM THE EXPORT ADDRESS TABLE

The brief said three of six print `~tail-not-a-pointer`, i.e. the tool declines
to name them, and that naming them was part of the job.

**The tool does not decline. It DISCARDS a name that is on the same line it
already parsed.** `standalone/rt_data_mint_tables.inc` writes each table as

```
// ??_7SunLight@Graphics@@6B@  DLL 0x180158e10  11 slots  [tail-not-a-pointer]
```

and `mint_slot_sweep.py`'s header regex captures only `DLL 0x… N slots …[note]`,
then prints the **note** as the identity. `tail-not-a-pointer` is a statement
about the table's **extent**, not about its name.

That is not good enough as an identification, because the `.inc` is a generated
file and therefore one source, not two. **All six were identified independently
out of the retail PE export directory with `pefile`: an export whose ADDRESS IS
the table VA. That is a record read out of the file, not a hint.**

| VA | export AT that VA (identification) | next export after it (the *hint*) | hint correct? |
|---|---|---|---|
| `0x1801550e0` | `??_7ParticleInstance@Graphics@@6B@` | `??_7Particle@Graphics@@6B@` | **NO** |
| `0x180155040` | `??_7ParticleSystemInstance@Graphics@@6B@` | `??_7ParticleSystem@Graphics@@6B@` | **NO** |
| `0x180158e10` | `??_7SunLight@Graphics@@6B@` | `??_7CollisionMesh@Collision2D@@6B@` | **NO** |
| `0x180154dc0` | `??_7MeshInstance@Graphics@@6B@` | `??_7AnimationGroup@Graphics@@6B@` | **NO** |
| `0x180154d70` | `??_7EdgeMeshInstance@Graphics@@6B@` | `??_7MeshInstance@Graphics@@6B@` | **NO** |
| `0x180154950` | `??_7Stream@Audio@@6B@` | `??_7FileStream@IO@@6B@` | **NO** |

**The adjacent-export hint is wrong on all six, and the kickoff propagated three
of those wrong names into its own scoping table** — it called `0x1801550e0`
`~??_7Particle@Graphics@@6B@` (it is `ParticleInstance`), `0x180155040`
`~??_7ParticleSystem@…` (it is `ParticleSystemInstance`), and `0x180154950`
`~??_7FileStream@IO@@6B@` (it is `Stream@Audio`, and Wave 28 lane SLOTS had
already named it correctly in its own §3.2). The hint is not merely an upper
bound on identity; on a table with a neighbour it is **systematically off by one
row**, and `mint_slot_sweep.py`'s own `KNOWN` dict says so in a comment that its
print loop then ignores.

### 1.1 Extents, derived rather than trusted

The `.inc` slot counts were re-derived from the raw `.rdata`: walk qwords while
each is a `.text` pointer, truncated at the next EXPORTED vtable VA. **The
truncation is load-bearing and `Stream@Audio` proves it** — its run of `.text`
pointers is SEVENTEEN long, because `??_7FileStream@IO@@6B@` begins at
`0x180154960`, two slots later, and the two tables are contiguous with no
separator. A run-length reading alone would have given `Stream@Audio` 17 slots.

| VA | run of `.text` ptrs | true slots | what bounds it |
|---|---|---|---|
| `0x1801550e0` | 19 | **10** | next exported table at `+0x50` |
| `0x180155040` | 39 | **10** | next exported table at `+0x50` |
| `0x180158e10` | 11 | **11** | ASCII `"Surface"` follows |
| `0x180154dc0` | 9 | **9** | a NULL pad qword, then the next table |
| `0x180154d70` | 9 | **9** | a NULL pad qword, then the next table |
| `0x180154950` | 17 | **2** | next exported table at `+0x10` |

All six agree with the `.inc`. Reported because a re-derivation that AGREES still
has to be reported as having happened
(`reference_a_matching_number_can_match_because_it_is_stale.md`).

---

## 2. RECONCILING 16 SLOTS AGAINST THE CENSUS'S 18 VTABLE ROWS

The brief asked for this explicitly, and warned that two tools measuring
near-identical things is where this project finds silent defects.

**They are not measuring near-identical things, and the 16-vs-18 difference is
not a discrepancy at all: the two numbers count DIFFERENT UNITS.**

* The census's **18** = **18 VTABLE DATA ROWS**: `??_7X` symbols our tree
  resolves by name and has not registered. That is *promotion / mint* work on
  the **table pointer**.
* The sweep's **16** = unported **SLOT BODIES** inside minted tables. That is
  *body port* work on the table's **contents**.

Derived by name, not by count, from `runtime_census.py --unported`. The census's
18 are:

```
Achievement 4  Achievement@0 · AchievementInstance · Goal · GoalInstance
Graphics    8  EdgeMesh · EdgeMeshInstance · Light · MeshInstance ·
               Particle · ParticleInstance · ParticleSystemInstance · SunLight
IO          2  PackageFile · TextFileStream
Audio       1  Stream@Audio
System      1  Thread@System
UI          2  ElementDescription · StringTable
```

**All six of this lane's tables are in that 18. The populations NEST, they do
not overlap partially.** So the decomposition nobody has printed is:

```
  18 tables awaiting promotion
   = 12 whose slots are ALL PORTED (COMPLETE; blocked only on promotion)
   +  6 that still had unported slot bodies
        -> of which 2 (ParticleInstance, ParticleSystemInstance) were
           ALREADY COMPLETE and the sweep could not see it (§0)
        -> leaving 4 tables and 7 slots of genuine body work
```

**And 7 slots is 7 distinct bodies, but the sweep's 16 was only 13** — three
`ParticleInstance` bodies each occupy a slot in *two* tables. A slot count is not
a body count (`reference_a_namespace_gap_is_not_a_body_count.md`), and this lane
had to say so twice for the same figure.

Neither tool is wrong about its own question. What is missing is that **nothing
prints the relationship**, so every wave re-asks it.

---

## 3. WHAT LANDED — MEASURED

Two slots of `??_7MeshInstance@Graphics@@6B@`, **6/9 → 8/9**, in
`runtime/src/graphics/wave29_lane_slots.cpp`:

| slot | symbol | retail | extent |
|---|---|---|---|
| 1 | `?Update@MeshInstance@Graphics@@UEAAXAEBVTime@System@@@Z` | `0x180100800` | 343 B |
| 7 | `?SetSharedInstanceData@MeshInstance@Graphics@@UEBAHAEAVContext@Video@@HM@Z` | `0x180101660` | 280 B |

**True unported slots across all 64 tables: 7 → 5.**

### 3.1 The census delta, attributed ROW BY ROW

Same tool, same two tips, and the check was a **SET DIFF** of
`runtime_census.py --unported`, not a comparison of counts
(`reference_diff_the_set_not_the_count.md`).

| | base `c11511a1` | lane tip | delta |
|---|---|---|---|
| called | 1453 | **1457** | **+4** |
| ported | 1294 | **1294** | **0** |
| TOTAL % | 89.1% | **88.8%** | down |
| Graphics called | 132 | **133** | +1 |

**Every one of the +4 is named and every one was declared IN ADVANCE at its own
call site, not discovered afterwards**
(`reference_porting_can_grow_the_cutover_surface.md`):

```
  addr   internal  0x1801ff30c   MeshInstance::Update's static-init guard   (unexported .data)
  addr   internal  0x1801ff310   the 48-entry identity Transform table      (unexported .data)
  export Graphics  ?GetBones@MeshInstance@Graphics@@…
  export Video     ?SetBones@Context@Video@@…
```

Nothing LEFT the unported set. **`ported` moved by ZERO while two bodies were
written, and that is correct, not a disappointment**: both are slots of an
UNPROMOTED vtable, a population the census states in terms is "NOT counted
above", so they land in the PROMOTION BACKLOG and move no column. Wave 28 lane
SLOTS recorded the identical shape. **Score this lane on the slot rows and on
§0, never on the TOTAL** — the percentage fell because `called` sits in both
halves of the ratio and this lane opened four doors
(`reference_the_ceiling_percentage_is_a_moving_target.md`).

### 3.2 The build gate — all four checks, FOREGROUND, with numbers

```
mingw32-make standalone-debug                    MAKE-EXIT=0   (foreground)
ls -l standalone/build/reimpl-x64.exe            33,870,653 bytes, Sep 12 14:37
find runtime standalone code engine dx11 -newer <exe> …        EMPTY
nm  <exe> | grep RtGraphicsWave29Slots           Register + Summary, both T
nm  <exe> | grep RtGraphics_MeshInstance_…       both bodies, both T
strings <exe> | grep -cF '?Update@MeshInstance@…'              2
strings <exe> | grep -cF '?SetSharedInstanceData@MeshInstance@…' 2
strings <exe> | grep -cF '[RtPort] Wave 29 lane SLOTS'         1
cutover manifest gate                            OK (59 rows)
warnings from my file                            0
```

The `strings` count of 2 per name is the registration literal plus the mint
table's literal — the symbols reached the **LINKED IMAGE**, not merely an object
file.

**THE FIRST FOREGROUND BUILD FAILED, AND THE FAILURE WAS A RESULT.** The
Makefile's `check-log-strip` gate refused it:

```
ERROR: qualified `std::printf` cannot survive the log-stripped build.
       standalone/log_strip.h does `#define printf(...) ((void)0)`, and the
       preprocessor cannot replace a qualified name.
  runtime/src/graphics/wave29_lane_slots.cpp:491: std::printf(
```

My `Summary()` would have compiled, linked, passed `nm`, passed `strings`, and
printed **nothing** in the log-stripped build — i.e. it would have manufactured
exactly the kickoff-warning-10 signal ("a zero from a counter means never
printed"). A gate someone else wrote caught my instance of the defect the
kickoff had warned me about. Fixed at the site with the reason recorded there.

### 3.3 The Summary IS wired (kickoff warning 10)

`RtGraphicsWave29SlotsSummary()` is called from `RtGraphicsPortInit()` in
`runtime/src/graphics/rt_graphics_port.cpp`, at init — so a run that registers
this lane provably emits the line **even if neither body ever executes**, which
is the distinction warning 10 is about. Wave 28 defect #7 was that every lane's
`*Summary()` was written and nothing called it.

---

## 4. THE TWO BODIES — WHAT READING THE SHAPE GETS WRONG

Every byte from `objdump` against `C:\GOG Games\Banished\Runtime-x64.dll` and
`pefile` against the same file. **ZERO Ghidra calls in this lane.**

### `?Update@MeshInstance@` (343 B, 1 fragment)

1. **It lazily initialises a process-wide static table and sets the guard bit
   BEFORE filling the table** (`or eax,1` + store at `0x180100834`, fill loop at
   `0x180100850`). Not thread-safe; reproduced as-is. Adding a fence would be a
   behaviour our standalone has and retail does not.
2. **The fill is 48 entries, not 47.** `lea ecx,[rbx+0x2f]` with `rbx = 0`, and
   the loop is `body … dec ecx … jns` — the `dec` is at the END, so the body also
   runs for `ecx = 47` itself. A reader who takes the initialiser as the count is
   short by one, and the missing entry is the last one.
3. **Each 28-byte entry is four stores covering all 28 bytes with NO GAP** —
   `qword+0x00=0`, `dword+0x08=0`, `qword+0x0c=0x3f800000` (the float `1.0f` at
   `+0x0c` and zero at `+0x10`), `qword+0x14=0`. Unlike `Particle`'s ctor, whose
   two untouched gaps cost an earlier lane two false divergences.
4. **Arg 8 is the literal `true` and arg 7 is `?_rootOnlyUpdates@`, not the other
   way round.** Both are BYTE writes into the argument area
   (`mov BYTE PTR [rsp+0x40],0x1`; `mov BYTE PTR [rsp+0x38],al`).
5. **The second `InterpolatedState<Transform>*` is that global identity table**
   (`lea rax,[rip+0xfea23]` → `0x1801ff310`), not a second per-instance array;
   the FIRST is `this->_nodes._data`. Crossing the two hands the animator the
   instance's own poses as its rest pose — a silent no-op on an unanimated mesh
   and wrong on an animated one.

**It writes NOTHING to `this` directly** — 21 memory destinations and not one
through `rdi`. All mutation of the instance happens inside
`AnimationState::Update` and `UpdateHierarchy`, both already ported and both
reached BY NAME so the registry-first lookup hands back our bodies.

### `?SetSharedInstanceData@MeshInstance@` (280 B, 1 fragment)

* **`arg2` is a PACKED PAIR and retail SIGN-EXTENDS before splitting.**
  `movsxd r9,r8d` then `lo = v & 0xfff` (list index) and
  `hi = (v >> 12) & 0xf` (detail level), where the shift is a 64-bit ARITHMETIC
  `sar`. A 32-bit logical shift agrees on every reachable input and differs on a
  negative `arg2`; the signed 64-bit form is kept so they agree everywhere.
* **The early out is a SIGN test, not a zero test** (`cmp …,0` / `jge`), and it
  is the common path.
* **The record stride is 24 bytes**, reached as `lea rdx,[r9+r9*2]` scaled by 8
  in the addressing mode. `lo*3` scaled by 8 and `lo*24` are the same number
  reached two ways; only one of them says what the stride is.
* **IT RETURNS 0 UNCONDITIONALLY.** `xor eax,eax` is the MERGE POINT of both
  paths. Returning the bone count on the working path is a plausible reading of
  the shape and is wrong.
* **THE TWO `dec/jns` LOOPS IN THIS FILE HAVE THE SAME INITIALISER `0x2f` AND
  DIFFERENT TRIP COUNTS**, 48 and 47, because the `dec` sits at the end of one
  and at the head of the other. Counted from the listing in both cases.

---

## 5. EXTENTS — TWO INDEPENDENT ALGORITHMS, AND A PARTNER-FOUND DEFECT IN MINE

**Lane GFXBODY found a DEFLATING defect in this lane's first walker.** v1 walked
FORWARD and required each successor `.pdata` row to begin EXACTLY at the
predecessor's end; alignment padding between fragments terminates that walk early
and the body reports SHORTER than it is. A v2 was written to GFXBODY's design —
index every chained `UNWIND_INFO` on the PARENT `RUNTIME_FUNCTION` in its tail
and close transitively, **never comparing addresses at all** — and both were run
side by side on twelve subjects including **five controls from three lanes**:

| control | source | first entry | full chain | both walkers |
|---|---|---|---|---|
| `?Update@ParticleSystemInstance@` | PARTSYS | 157 | 5242 (33.4x) | AGREE |
| `?AddDrawItems@MeshInstance@` | Wave 28 | 83 | 720 (8.7x) | AGREE |
| `?AddDrawItems@EdgeMeshInstance@` | Wave 28 | 205 | 373 (1.8x) | AGREE |
| `?GetRootPoseAtTime@Animation@` | GFXBODY | 37 | 495 (13.4x) | AGREE |
| `?UpdateNodes@AnimationState@` | GFXBODY | 126 | 2191 (17.4x) | AGREE |

They agree on all seven of my real subjects too. **So my published extents stand
— but v1 was sound by LUCK and v2 is sound by CONSTRUCTION, and that distinction
is the result, not the numbers.**

**A NEW WORST CASE FOR `.pdata` UNDERSTATEMENT: 70.3x.**
`?Draw@EdgeMeshInstance@Graphics@@` (`0x1800a3150`) has a 25-byte first `.pdata`
entry and a 1757-byte chain over 7 fragments. That beats the project's recorded
worst (33x, PARTSYS) by more than two. `?Draw@MeshInstance@` is 29.2x (50 →
1458) and `?DrawShadowMap@SunLight@` is 18.0x (50 → 900). **In this family a
25-to-50-byte first entry means nothing at all**, and a lane scoping on it would
take a 1757-byte body believing it was 25.

---

## 6. WHAT DID NOT LAND, AND THE OBSTACLE CLASS FOR EACH

Five slots remain across four tables. **Every one is a SIZE JUDGEMENT stated in
those words — not a blocker, and with no premise that can expire.** Per the
kickoff's distinction: a MISSING-CAPABILITY blocker expires, an INTRINSIC one
does not; **a size judgement is neither, it is a budget statement about one
lane**, and it is the honest form precisely because the next lane can simply do
the work (which is how Wave 28 SLOTS was able to close `TextFileStream`'s two
deferred slots without falling over a stale claim).

| slot | symbol | retail | extent | why not |
|---|---|---|---|---|
| MeshInstance 2 | `?Draw@MeshInstance@` | `0x180100960` | 1458 B / 5 frags | size |
| EdgeMeshInstance 2 | `?Draw@EdgeMeshInstance@` | `0x1800a3150` | 1757 B / 7 frags | size |
| SunLight 9 | `?DrawShadowMap@SunLight@` | `0x180106bc0` | 900 B / 4 frags | size |
| SunLight 10 | `?Draw@SunLight@…HHHH@Z` | `0x1801067b0` | 1029 B / 1 frag | size |
| Stream@Audio 0 | `?Run@Stream@Audio@@` | `0x1800a0f60` | 1800 B / 1 frag | size |

`?Run@Stream@Audio@@` is the row the brief named: declined by two previous lanes
as a size judgement, re-checked here, and **declined again as a size judgement.
Too big for this lane. Not blocked.** Its extent was re-derived (1800 B, single
unchained entry) and reproduces Wave 28's figure exactly.

**ON THE SunLight PAIR, WITH MY PARTNER'S FIGURE BESIDE MINE.** Lane GFXBODY
measured `?GetSunCamera@SunLight@` at **3775 bytes** — the largest of its eight,
larger than `Scene::AddCulledDrawItems` — and is declining it on size. So the
shadow-camera construction stays in the DLL regardless. My `DrawShadowMap` (900
B) is not the big one on that pair, and porting it alone would leave the caller
ours and the camera retail's.

> **[ADDRESSEE — the five remaining slots] the Wave 30 kickoff author**, to be
> scoped as ONE lane and not five, because `?Draw@MeshInstance@` and
> `?Draw@EdgeMeshInstance@` are the two overrides of one slot and Wave 27 lane
> GFXMESH's standing finding is that this family goes wrong exactly when the two
> are derived from different readings. **EXPIRY: none — this is a budget
> statement, not a claim about the tree, and it cannot go stale.**

Also not done, deliberately:

* **No vtable was promoted** (§8).
* **No run was taken.** No `game_dir_lock.sh` acquisition, so **my wait was
  zero** — I never queued. Every claim in this document is static or build-time.
* **No tool was built, fixed or extended.** §7.
* **No other lane's scope was touched.** GFXBODY's eight bodies; the particle
  family (Wave 27 GFXPART / Wave 28 PARTSYS); IO's four; Audio's and Video's.
  No `RT_UNPROMOTABLE` tag was touched. The only file edited outside my own is
  `rt_graphics_port.cpp`, to wire the registration and the Summary.

---

## 7. INSTRUMENT DEFECTS — OBSERVED, NOT FIXED

| # | instrument | defect | direction |
|---|---|---|---|
| 1 | `tools/mint_slot_sweep.py:108` | line-by-line registration regex; blind to 67 split-literal registrations; reports 16 where the truth is 7 | **INFLATING** |
| 2 | `tools/mint_slot_sweep.py:154` | prints the `.inc`'s **extent note** as the table's **identity**, so a name sitting on the same parsed line reads as "declines to name"; and the adjacent-export hint it prints instead is wrong on **all six** of my tables, systematically off by one row | **MISIDENTIFYING** |
| 3 | *(mine, v1)* forward-adjacency extent walker | terminates on inter-fragment alignment padding; found by lane GFXBODY | **DEFLATING** |
| 4 | `tools/mint_slot_sweep.py` | reads `REV=HEAD` via `git show`/`git grep`, so it **cannot see uncommitted work** — its 16 did not move when my two ports landed in the working tree, while my working-tree scan went 7 → 5 | by design, but undocumented, and it silently answers a different question than the census beside it |

**Defect 2 is worth more than it looks.** The tool has a `KNOWN` dict whose own
comment says *"The `[adjacent-export:X]` note … names the NEXT export after the
vtable, so it is off by one row and must not be read as an identity"* — and the
print loop four functions later does exactly that. **A warning written into a
comment and not enforced in the code is not a check**
(`reference_a_predicate_nobody_evaluates.md`), and this one propagated three
wrong identities into the Wave 29 kickoff.

**WHY I FIXED NONE OF THEM.** `mint_slot_sweep.py` is a board-owning instrument;
lane INSTR owns the board this wave, and two lanes editing one scoring tool in
one wave is how a delta stops being attributable
(`reference_same_tool_both_sides_is_not_enough.md`). My replacement is a
scratchpad script that RE-DERIVES the number rather than patching the tool, which
is the clearing move the kickoff §4 names for a shared-scratchpad artefact.

---

## 8. PROMOTION — NOTHING PROMOTED, AND THE ROUTE SCOPE FOR EACH TABLE

Per the brief: a verdict with its ROUTE SCOPE, never bare
(`reference_unpromotable_is_route_scoped_not_permanent.md`).

| table | slot state | promotable? | scope |
|---|---|---|---|
| `??_7ParticleInstance@Graphics@@6B@` | **10/10 COMPLETE** | **YES — and this is the wave's cheapest promotion candidate** | needs its stamper owned and measured. NOT done here. |
| `??_7ParticleSystemInstance@Graphics@@6B@` | **10/10 COMPLETE** | **YES**, same | same |
| `??_7MeshInstance@Graphics@@6B@` | 8/9 | no — one trap left | — |
| `??_7SunLight@Graphics@@6B@` | 9/11 | no | — |
| `??_7EdgeMeshInstance@Graphics@@6B@` | 8/9 | no | — |
| `??_7Stream@Audio@@6B@` | 1/2 | no | — |

**COMPLETE is the STATIC half of readiness only**: it says promoting would put no
trap in a live table, and nothing more. **A green READY/TRAP=0 screen says the
table is SAFE TO STAMP, not that anything DISPATCHES it**
(`reference_a_green_readiness_screen_is_not_a_route.md`) — one table sat at
attendance 0 for waves behind such a screen. Establishing promotability for the
two particle tables means owning and MEASURING the stamper, which no source scan
answers and which needs a run this lane did not take.

> **[ADDRESSEE — the two COMPLETE particle tables] the Wave 30 kickoff author.**
> These two rows have been COMPLETE since Wave 27 and were invisible for two
> waves. They are the only two rows in the 18 that reached COMPLETE without
> anyone noticing, so nobody has ever asked the promotion question about them.
> **EXPIRY: when a lane measures which constructor stamps `0x1801550e0` /
> `0x180155040` and whether every live instance comes through our copy.**

---

## 9. MY PARTNER'S INSTRUMENT, RUN AGAINST MY SUBJECTS

Per kickoff §2.1 — an instruction, not a courtesy.

I ran GFXBODY's `gfxbody_operands.py` against both bodies I ported. **It found no
discrepancy with my port, and it independently CONFIRMED three claims I had made
by reading the disassembly** — which is the more useful result, because a claim
derived by eye and confirmed by a screen someone else wrote has two sources:

* `?Update@MeshInstance@`: `WRITES via rax : -0x18-0x3` — the identity-table
  entry is **28 contiguous bytes with no gap**, exactly as §4 claims from the
  four stores.
* `?Update@MeshInstance@`: `WRITES via rsp : 0x20-0x38  0x40  0x58-0x73` — note
  `0x40` printed as a SEPARATE range: **`[0x39,0x40)` is never written**, so the
  two `bool` arguments are genuinely byte-width and a `bool` typedef is right.
* `?Update@MeshInstance@`: `WRITES via rip : 0xfead2-0xfead5` — **exactly one**
  RIP-relative write, the init guard. The identity table is written via `rax`.
* `?SetSharedInstanceData@`: `WRITES via rsp : 0x20-0x3b` = 28 bytes, and
  `WRITES via rax : -0x20-0x1f` = 0x40 bytes per pass. Both contiguous. The 28
  corroborates `sizeof(Math::Transform)` from a second body in the same file.
* **Neither body has a Camera-class write-width trap.** Full contiguous
  coverage in both. That is a NEGATIVE result and it is reported as one.

I also scored its controls **separately**, per warning 9: control (a) found both
no-displacement `mov DWORD PTR [rcx],eax` stores (the exact form that defeated
Wave 27 GFXBODY's byte-pattern reader), and control (c) reproduced Wave 28's
published Camera set difference `{0x61,0x62,0x63}` by a different method.

GFXBODY, reciprocally, ran **my** finding against its own walker and found a
DEFLATING defect in it (§5) — and separately reported two defects it found in its
own instruments, one of which is the best either of us produced: **its control
tested `ops[0]` against `^\[reg\]$` while `objdump` prints `DWORD PTR [rcx]`, so
the CONTROL reported the known positive ABSENT while the SCREEN was reading it
correctly.** A control stricter than the screen it polices, scored together with
it, would have read as one clean pass. That is kickoff warning 9 with the roles
reversed and it is not in the project's ledger.

**Wave 28's result reproduces: across this pair, every instrument defect was
found by the OTHER lane's author, none by its own.** Four defects, four
cross-findings, zero self-findings.

---

## 10. WHAT THIS DOES NOT CLAIM

* **It does not claim either ported body is CORRECT.** Neither carries a diff
  oracle and neither has been observed executing. `?Update@MeshInstance@`
  mutates a process-wide static and dispatches virtuals; a differential replay
  would run the animator twice on one frame, tick every attached child twice, and
  set the init guard for a table the second run would then skip.
  `?SetSharedInstanceData@` ends in a real `Video::Context::SetBones` on a live
  device. Both refusals are STRUCTURAL, not convenience
  (`reference_uncalled_port_is_not_verified.md`).
* **It does not claim anything RAN.** No game-directory lock was taken, no boot,
  no attendance. `calls=0` is the expected reading for a virtual slot the DLL
  dispatches from its own table and is not verification.
* **It does not claim the two particle tables are PROMOTABLE** — only that they
  are COMPLETE, which is the static half (§8).
* **It does not claim "7" is the true count for any tip but this one.** It is a
  measurement of the working tree at `wave29/slots`. Six sibling lanes are
  porting concurrently and every one of them can move it.
* **It does not claim the 67 split-literal registrations are all live.** The scan
  proves the line-by-line regex cannot SEE them; it does not audit whether each
  is correct, reached, or wanted.
* **It does not claim `mint_slot_sweep.py` has only these defects.** Two were
  found; the tool was not audited.
* **It does not claim the six table identifications are complete descriptions.**
  Each is an export whose address IS the table VA — that names the table and says
  nothing about its extent, its stamper, or its dispatch.
* **The four new `called` rows GROW the cutover surface by four**, and this lane
  did not adjudicate any of them for cutover.
* **It does not claim `?_rootOnlyUpdates@` can be minted.** It is a DATA export
  read in place; `mesh.cpp` already records that it cannot be registered.

### My own errors

1. **I nearly re-ported two Wave 27 bodies.** I had read
   `?SetParticle@ParticleInstance@`'s 389-byte disassembly, found a real trap in
   it (the six-float change-detect that skips both the zero-stores and the dirty
   bit when all six are already zero), and was about to write it — for a body
   `wave27_lane_gfxpart.cpp:644` has had for two waves. **The multi-line scan
   caught it; my own vigilance did not**, and I had already run a
   `grep -rn SetParticle` earlier in the lane whose output I misread as comments.
2. **My first extent walker had a deflating defect** and I published three
   figures from it before GFXBODY found it. They survived, by luck.
3. **I wrote `std::printf` in a `Summary()`** — manufacturing the exact
   never-printed signal kickoff warning 10 describes, in the same lane where I
   cite warning 10. A Makefile gate caught it, not me.
4. **My first reconciliation script's regex parsed only the truncated-path
   section of `--unported`** and reported 13 of the 18 rows, from which I briefly
   concluded that four of my six tables were "NOT a census gap row". All six are.
   I caught it by re-deriving the 18 a second way rather than by re-reading the
   regex.
