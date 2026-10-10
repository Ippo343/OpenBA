# WAVE 22 PLAN — a focused pass at ONE group: `System`

Base: `wave21/base` at `9fa1d02c` (all seven Wave 21 lanes merged, build clean, checkers
green). Written 2026-09-11. **Not dispatched.**

## 0. WHY `System`, AND THE RULE THIS PLAN OBEYS

Wave 21 closed with `System` as the largest gap containing real body work — 25 rows, 11 of
them `BODY`, against Graphics's 9 on a namespace that has now had three lanes. `System` has
never been a wave subject.

Wave 21's own headline was that **a gap figure is uninterpretable until decomposed**, and
that **the cheap-sounding items in a handover are the stalest**. So this plan does not
inherit "System 25" or even "System 11 bodies". It decomposes them first, at planning time,
and prints the command.

*Re-derived at planning time on the merged tip:*
```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --csv w22.csv
# then classify the 25 unported System rows by mangled-name kind
```
```
System   103 called   78 ported   25 gap
BODY 11 · DATA 11 · CTOR 1 · DTOR 1 · VTABLE 1
```

## 1. THE FINDING THAT SHAPES THE WAVE — "11 bodies" is 6

**Five of the eleven `BODY` rows are `MemoryAllocator`, and all five ARE ALREADY WRITTEN
AND REGISTERED.** Verified at planning time, not inherited:

```
runtime/src/system/allocator.cpp:1433  RegisterRuntimeExport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z", …)
                              :1435  ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
                              :1439  ?Get@MemoryAllocator@System@@SAPEAV12@XZ
                              :1463  ?Startup@MemoryAllocator@System@@QEAA_N_K0@Z
                              :1465  ?Shutdown@MemoryAllocator@System@@QEAAXXZ
```

They read as unported because the census says so **correctly**: that Register function
returns 0 unless `BANISHED_RT_ALLOC` is armed, and the census prints its own rule —
*"A gated registration is not a port. Arm it, measure it, then move it."* (26 registrations
in that file, plus 6 in `allocator_win.cpp`.)

**So the real shape of System is:**

| cluster | rows | what the work actually is |
|---|---|---|
| `MemoryAllocator` | 5 BODY + 1 DATA | **ARM AND MEASURE** — a risk decision, not porting |
| `Timer` | 5 DATA | mint/publish |
| `Error` | 1 BODY + 3 DATA | port + mint |
| `Configuration` | 1 BODY + 1 DTOR + 1 DATA | port + mint |
| `Compression` | 1 BODY + 1 CTOR | port |
| `DateTime` | 2 BODY | port |
| `String::StrCompare` | 1 BODY | port |
| `Debug::_disableHandler` | 1 DATA | mint |
| `??_7Thread@System@@6B@` | 1 VTABLE | promotion (backlog says 1/1 COMPLETE) |

**Six genuinely unported bodies, not eleven.** This is the THIRD consecutive wave in which a
namespace gap meant something other than it appeared to — UI in Wave 10, Graphics in
Wave 21, System here. The decomposition column now catches the kind error; it does **not**
catch the gated-registration error, and nothing does but a person grepping. *That is a
Wave 22 instrument question, listed in §4.*

## 2. LANES — five, deliberately

| lane | subject | why it is one lane |
|---|---|---|
| **ALLOC** | the 5 `MemoryAllocator` rows + `?_singleton@MemoryAllocator@` | highest value and highest risk on the board; the bodies exist, the question is defaulting them on |
| **TIMER** | the 5 `Timer` statics | one coherent cluster, all DATA, shared consumers |
| **ERRCFG** | `Error` (Display + 3 statics) + `Configuration` (GetCommandLineString, dtor, singleton) + `Debug::_disableHandler` | two small clusters that both sit on the engine lifecycle path |
| **SYSBODY** | `Compression` (Compress + ctor), `DateTime` (2), `String::StrCompare`, `??_7Thread@System@@6B@` | the genuinely unported bodies, all leaf-ish |
| **GEOM** | `statusicon_geometry.cpp:341`, the cutover blocker | **A DEBT, NOT A CHOICE — see §3** |

## 3. LANE GEOM IS OWED BY NAME

Wave 21 committed this tag to `standalone/reg_mint_promoted.cpp`:

> `[ADDRESSEE: Wave 22 lane GEOM, which owns the statusicon_geometry.cpp:341 cutover
> blocker — WAVE21_CLOSED.md §7 item 1]`

**Planning Wave 22 without lane GEOM would reproduce the exact defect Wave 21 spent itself
finding** — an expiry naming a condition with nobody to discharge it. The addressee is a
commitment already in the tree, so GEOM runs, or the tag is re-pointed in the same breath
and the new addressee is named. It is also cheap and fully derived: both extents are
measured (9 and 9), the "12" has no basis in either binary, and the derivation already sits
at the loop.

Its two open residuals, recorded by Wave 21 rather than smoothed: retail's slot 9 is
`nullptr` where our stub is not, and the no-dispatch finding is **derived, not observed**.

## 4. WHAT EACH LANE MUST RE-DERIVE FIRST

| lane | re-derivation command | the trap waiting for it |
|---|---|---|
| ALLOC | `BANISHED_RT_ALLOC=1` run + `sa-stress` A/B | **"off by default" was FALSE once**: an unarmed run mutated retail's arena because one wrapper called `rtalloc::MemFree` DIRECTLY. Verify the gate is a real gate before trusting any A/B. And **one `sa-stress` run cannot resolve a 5-point effect** — a single-run control already produced a wrong attribution here. |
| TIMER | `get_xrefs_to` each static in the DLL | a static retail WRITES cannot be minted without split brain; and the verdict is **route-scoped**, so say DLL-present or cutover |
| ERRCFG | ditto, plus the engine lifecycle call sites | `?_errorString@Error@` is read from `game_main.cpp`; a minted copy that nothing publishes into the exported slot is invisible |
| SYSBODY | grep all four symbols across the tree BEFORE porting | three bodies were re-written in one wave that the tree already had; and a body ported but never REGISTERED reads as absent |
| GEOM | read the comment block at the loop | the fix is one character **plus a comment correction**, and the comment is the half that would talk the next reader out of it |

**Every lane:** state your denominator; say what you did NOT check; do not guard a deref
retail leaves unguarded; a green readiness screen is not a route; `calls=0` is not
"unexercised"; work in a private worktree; never pipe a `game_dir_lock.sh` acquire.

## 5. THE INSTRUMENT QUESTION THIS WAVE SHOULD ANSWER

**Nothing detects a GATED port.** Wave 21 taught the census to decompose a gap by symbol
KIND, which would not have caught this: all five allocator rows classify correctly as
`BODY`. They are not unwritten, they are unarmed, and the only thing standing between a
wave and a badly-scoped lane was someone grepping the file the census already names in its
own warning text.

The census **already prints** `40 registration(s) are present in the source but NOT counted
above`. **It does not connect that list to the namespace rows those registrations would
move.** Making it do so is a small change and would have converted this plan's §1 finding
from a grep into a printed line. Assign it to whichever lane finishes first, or to ALLOC,
which has to read that file anyway.

## 6. WHAT THIS WAVE DELIBERATELY DOES NOT DO

- **`internal`, the largest raw gap at 41, stays untouched.** It is 100% unclassifiable
  address-keyed rows. It needs an instrument, not a porting lane, and mixing that into a
  focused pass is how a focused pass stops being one.
- **`.pdata` chain attribution stays declared-not-fixed** (932 of 6045 entries chain).
- **The screenshot driver stays unbuilt**, though it unlocks three items at once and is
  the highest-leverage missing harness on the board. It is a Graphics/IO item and this is
  a System wave. **Listed here so its absence is a decision rather than an oversight.**
- Wave 20 items 7, 9 and 10 remain `[NOT RE-DERIVED]`. Item 9, the cutover wall at
  `?PlatformConfigure@Engine@1@`, will have gone **three waves** without re-verification.

## 7. SCHEDULING — the one thing Wave 21 got wrong mechanically

Seven lanes against one game directory cost a lane its measurement runs; its ports closed
STRUCTURAL and had to be recovered by resuming it afterwards. **Five lanes is the cap here,
and only ALLOC and GEOM are expected to need the game directory.** TIMER, ERRCFG and
SYSBODY are binary-and-source work until their verification step, which should be queued
behind ALLOC rather than raced against it.
