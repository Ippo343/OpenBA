# WAVE 26 CLOSED — 2026-09-12

Seven lanes, all merged. Base `wave26/base`. Board re-derived **after** the last
merge and **after** the mint table regeneration, per Wave 25's own rule that the
board in the close is the post-regeneration one.

## 1. THE BOARD

```
              called   ported        gap
  opened        1333     1185  88.9%  148
  closed        1372     1237  90.2%  135
```

### 1.1 DO NOT READ THIS AS 52 ROWS OF PORTING

The +39 `called` / +52 `ported` movement decomposes as:

| source | called | ported | what it was |
|---|---:|---:|---|
| **DOORS** | +18 | +11 | **ported NOTHING.** Taught the census to see registrations it was already making |
| PROMOIO | — | +13 | 4 IO stream vtables promoted, slots folded |
| INT33 | +2 | +8 | 8 address-keyed rows |
| PROMOGFX | — | +4 | 2 Graphics vtables, 2 Achievement `_typeInfo` |
| FACTORY | +19 | +16 | StringTable ctor + 2 factories + 6 of 9 slots |

**A third of the movement is the instrument ceasing to lie**, not work done. The
single most misleading way to report this wave would be "88.9% -> 90.2%".

### 1.2 THE NUMBER THAT SHOULD BE QUOTED INSTEAD

```
  gap 135  =  18 BODY + 2 DTOR + 42 VTABLE + 46 DATA + 27 UNKNOWN
  of the 42 VTABLE rows, 20 are UNPROMOTABLE
  => REAL remaining vtable work = 22, NOT 42
  of the 18 BODY rows, 7 are the allocator: WRITTEN and GATED
  => REAL unwritten body surface = 11 + 2 DTOR
```

Promotion backlog: **194/49/29 -> 156/44/23**.

## 2. WHAT THIS WAVE ACTUALLY WAS

**An instruments wave that did not intend to be one.** Three lanes were scoped as
porting lanes; all three spent their highest-value hours impeaching the tools the
board is made of. Every one of the following was MEASURED, and every one changed a
number the project had been quoting:

1. **15-ish promoted vtables were counted as "called but not ported"** —
   `PromoteVtableGuarded` registers via a variable, `ported_exports` was
   literal-only. `??_7BinaryFileStream@IO@@6B@` — the row Wave 20 closed on at
   **18,906 attendance** — printed as unported. Found independently by TWO lanes.
2. **`mint_stamper_census.py` bucketed stamp sites as `NOT OURS` wrongly in 45 of
   47 cases** (MSVC emits no `.pdata` for a frameless leaf). It **DEFLATES** —
   the direction that makes work look impossible and produces promotions that
   never happen, which nothing detects.
3. **A raw NUL byte makes `git grep -h` skip a file** and print `Binary file
   matches` instead of the lines. Hid 38 registrations and **inflated**
   `mint_slot_sweep.py` by 29.
4. **`cutover_manifest.inc` was STALE at attempt 26** — 46 committed rows against
   60 fresh, missing exactly lane DATA's 14. The generator was never re-run.
5. **`mint_slot_sweep.py`'s validation claim was PROSE and had silently decayed**;
   its docstring names a boot line character-for-character and it had stopped
   holding at some revision after `fc884df`.
6. **Two of three vptr checks in `file_stream_probe.cpp` have been structurally
   false since Waves 19/20** — the probe builds subjects with retail's ctor and
   compares against `RT_DATA_SLOT`, which consults the port registry first.

## 3. THE FINDING THAT OUTLIVES THE WAVE

### 3.1 The control rule, hit from FOUR sides in one wave

The project knew two faces of it. Wave 26 added two more, from four different
lanes, none aware of the others:

| # | rule | how it was learned here |
|---|---|---|
| 1 | must corrupt something the check **compares** | known (Wave 23) |
| 2 | must NOT corrupt something it **should not** | known (Wave 21) |
| 3 | **must be able to OBSERVE the channel it polices** | PROMOGFX: `git grep -l` vs `-l -a` showed no difference because `-l` prints only names and NEVER suppresses. It nearly cleared a true hypothesis |
| 4 | **when its normal answer is EMPTY, it must be handed a known POSITIVE** | integrator: an uncommitted-work sweep returned empty; so did the control; that is what revealed the world had moved rather than the loop being dead |

### 3.2 A number that MATCHES its prediction can match BECAUSE it is stale

Attempt 26 reported `46 tagged gap rows`, "precisely the numbers MANIFEST
projected". **46 was the PRE-DATA count.** The match was the SYMPTOM. Wave 25's
queue had said in as many words *"re-derive rather than trusting those numbers"* —
correct, addressed to the integrator, and defeated by the figure arriving on time.
`--check` answers in one second and nobody ran it.

**ATTEND's addition, which sharpens it:** the match was only possible because
something else was silently MASKING the defect — the blanket `BANISHED_RT_MINT=1`
switch published the 14 missing rows, so the boot went deep and the figure looked
right. **Removing the masking agent is what exposed it. That is an argument for
occasionally running the configuration you do NOT ship.**

### 3.3 A correction must live on the surface the next person will REACH

Converged on independently by three lanes. **The reader who most needs a
correction is by definition the one who did not open the file explaining it.**
Three grep keys, three surfaces, all three closed this wave: the row name (beside
the row), the script name (header **plus a runtime echo**), and a tool's printed
OUTPUT. **A commit message is a permanently uncorrectable artefact.** A lane doc
has the same weakness it describes.

### 3.4 THE POSITIVE COUNTER-EXAMPLE — a residual that FIRED

Everything above is a catalogue of residuals that did not fire. This one did.
Attempt 29's wall is `Collection::Array<ExternalArrayElement>::SetSize`
(`0x18002c6a0`), and the lane that wrote `external_load.cpp` **predicted that
exact failure in a comment AT THE CALL SITE**:

> *"NOT exported in either build, so this is a genuine ADDRESS door... Nothing in
> the tree registers it today; at cutover it needs a body or this call site needs
> to be revisited."*

Every clause correct. It named its **SYMBOL**, its **CONDITION** and its
**CONSEQUENCE**, and it sat at the call site rather than in a doc. **It cost the
next lane nothing to find.** If one thing from Wave 26 belongs in the Wave 27
kickoff, it is this shape — not any single finding.

## 4. CUTOVER: THE WALL MOVED TWICE, IN ONE WAVE

| attempt | wall | what it cost to clear |
|---|---|---|
| 26 | `?CreateNewObject@StringTable@UI@@` (mint FATAL) | FACTORY: ctor + 2 factories |
| 28 | `MISS ??_7StringTable@UI@@6B@` -> null vptr, AV at `GetTypeIdVirtual+0x1f` | FACTORY: 6 of 9 vtable slots |
| 29 | **`Collection::Array<ExternalArrayElement>::SetSize`**, EXEC-of-0 at `ArraySetSize+0x4d` | open |

Progress within `RtObject_External_Load`: `+0x33c` -> `+0x470`.

**A cutover boot is a GATE, not a PLANNER.** Attempts 28 and 29 each named ONE
symbol. Neither is a roadmap and neither is written as one.

## 5. WHAT IS EXPLICITLY NOT CLOSED

- **`??_7StringTable@UI@@6B@` is BLOCKED at 7/9.** Slot 0 (`0x180002740` deleting
  dtor) and `?Serialize@StringTable@UI@@` are unported, and **attempt 29 is not
  evidence about either** — it left the class entirely.
- **FACTORY's and INT33's bodies have attendance 0**, and for FACTORY no route can
  give them one with the DLL present. Verified against disassembly and the mint's
  trap, NOT a live route. The two cutover boots are ATTEND's validation, not theirs.
- **PROMOGFX's two vtable promotions have NO dispatch evidence either way** — the
  attendance watch counts `ResolveExport` doors and both rows are reached by
  `ResolveAddr`-by-address, so they print the same `never` as a dead row.
- **Every stamper ratio quoted in this wave came from the DEFECTIVE tool.** The fix
  landed late and changes what FUTURE runs print. Verdicts survive only where the
  `unattributed` count is 0 — a per-row check.
- **Four `?_typeInfo@` rows are ROUTE-UNDECIDED**, not rejected. Re-running
  autostart is explicitly NOT the condition.
- **`??_7UpdateLoop@Engine@@6B@`** is 7/7 COMPLETE and deliberately NOT tagged:
  6 of 8 stampers are base-subobject stamps and transient-vs-final was not
  determined. **An unfinished check, not a verdict.**
- **The machine-load columns have never been read back from a real 6000-frame run.**

## 6. WAVE 27 INPUTS

1. **`Collection::Array<ExternalArrayElement>::SetSize`** — the live frontier.
   **WARNING, load-bearing:** the three ported sibling instantiations
   (`Array<bool>`, `Array<Time>`, `Array<InternalPtr>` in `achievement_manager.cpp`)
   are the **WRONG TEMPLATE**. They are POD-element arrays using realloc+memcpy;
   this element carries a refcounted `System::String` and calls two per-element
   helpers (`0x1800067d0` grow, `0x180006ac0` shrink). **A port following the
   sibling recipe compiles, runs, and leaks or double-releases a refcount, and no
   byte oracle on `SetSize` itself would catch it.** Growth is not `capacity*=2`.
2. **The 206-slot factory family and the 82 TI classes with no minted vtable row.**
   FACTORY's finding: these need **no second generator**. `rt_mint_gen.py`'s rule is
   "mint what our SOURCE names", so a generated factory stamping its vptr through
   `RT_DATA_SLOT` with the vtable's mangled name **as a literal** makes the next
   regeneration harvest that vtable for free. It turns a second workstream into a
   property of how the first is written.
3. **Fix `mint_stamper_census.py`'s consumers, not just the tool** — re-run any
   Wave 26 stamper ratio whose `unattributed` count was non-zero.
4. **`--lock` becomes the DEFAULT.** Decided by the integrator; ADDRESSEE: whoever
   writes the Wave 27 kickoff; CONDITION: the kickoff being written.
5. **Widen `phantom_symbol_scan.py`'s NUL population to every text file including
   `docs/`** — it examined 1547 files and excluded the directory where verdicts live.
6. Probe-only resolves in `called` (DOORS, deliberately undecided);
   `mint_slot_sweep.py --verify-against <log>`; promote the CALLERS that would give
   INT33's 8 rows attendance.

## 7. GATES, ON THE FINAL MERGED TIP

```
mingw32-make standalone-debug        exit 0
mingw32-make standalone-perf         exit 0
runtime_census.py --selftest-all     ALL GREEN
rt_mint_gen.py --check               UP TO DATE (64 vtables, 104 BaseTypeInfo)
cutover_manifest.py --gate           OK (60 rows) -- and now FIRES INSIDE THE BUILD
phantom_symbol_scan.py               NUL screen none, controls pos+neg ALIVE
game_dir_lock_selftest.sh            15/15
game_dir_lock_mutations.sh           5/5   (M1 IS the Wave 25 code; M3 IS ALLOC's silence)
machine_load_controls.sh             9/9   (3 cells exist only so UNEXAMINED != CLEAN)
```

**The stale-`.inc` class is now structurally unmergeable**, not merely detectable:
`--gate` is wired into all three build targets, because a guard missing from one
target is a guard that does not fire. It fails on the row SET, the dispositions, an
unminted REQUIRED row and an undeclared scope; it only WARNs on a moved door count,
because a gate nobody can afford to leave on is worth what one nobody runs is worth.
**It was exercised on a real cross-lane merge, not only on injected controls:**
FACTORY moved two door counts, the gate printed WARN, the build completed, no lane
was blocked.

## 8. ONE THING THE INTEGRATOR GOT WRONG, RECORDED BECAUSE IT IS THE WAVE'S OWN LESSON

The kickoff listed **twelve Video `*Data` tables as promotion scope**. All twelve
have been tagged **PERMANENTLY UNPROMOTABLE since Wave 17**, in
`reg_mint_promoted.cpp` — the same file promotions are written into. The scope was
built from the census's `COMPLETE` column without reading the tags.

**The kickoff states the exact rule this violates, in bold, two paragraphs above the
error.** Stating a trap is not checking for it.

Separately, the integrator reported to the user that a NUL-screen result had been
**independently corroborated**. It had not: the corroborating command excluded `.md`
and **does not detect NULs at all**. It agreed because it was blind in the same
place. And an integrator-log entry asserting a tool was "STILL DEFLATING" went
**false within the hour** and had to be superseded.

Both are the same failure the wave spent its time finding in the lanes' instruments,
committed by the person auditing them.
