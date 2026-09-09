# Wave 10B — lane DLGV: two dialog vtables, eleven trapped slots

**Worktree** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a9112cfd6df5c6512`
**Branch** `wave10b/lane-dlgv`, cut from `wave10/base` @ `453e26d`.

**Outcome in one line:** all eleven trapped slots are ported and registered;
`??_7DialogInstance@UI@@6B@` is **PROMOTED**; `??_7DialogController@UI@@6B@` is
**READY but NOT promoted**, because on two routes its nine bodies were entered
**zero** times and a run that cannot tell a safe promotion from an unexercised
one is not a gate.

---

## 1. The eleven slots

| vtable | slot | symbol | DLL | what the body ACTUALLY is | how verified |
|---|---|---|---|---|---|
| DialogInstance | 2 | `?Type@` | `0x1800453e0` | `MOV EAX,[0x1801faac0] / RET` — `_typeInfo.typeId` | disassembly; the +0x00/+0x08 pair with the already-ported `?TypeName@` at `0x180045400` |
| DialogInstance | 4 | `?OnOpened@` | `0x180114fb0` | ~250 instructions: controller `OnOpened` loop, then an INLINED MemoryStream + Serializer read of the settings blob | full disassembly; the two inlined helpers matched instruction-for-instruction against the exported `??0/??1MemoryStream@IO@@` |
| DialogController | 1 | `?TypeName@` | `0x18003b6d0` | `MOV RAX,[0x1801f9e50] / RET` — `_typeInfo.name` (+0x08) | disassembly |
| DialogController | 2 | `?Type@` | `0x18003b6b0` | `MOV EAX,[0x1801f9e48] / RET` — `_typeInfo.typeId` (+0x00) | disassembly; same object as the row above |
| DialogController | 4 | `?OnCreated@` | `0x1800cd2b0` | **`RET 0` — empty** | bytes + individual disassembly (see §2) |
| DialogController | 5 | `?OnOpened@` | `0x1800cd2a0` | **`RET 0` — empty** | ditto |
| DialogController | 6 | `?OnClosed@` | `0x1800cd290` | **`RET 0` — empty** | ditto |
| DialogController | 7 | `?OnUpdateLayout@` | `0x1800cd280` | **`RET 0` — empty** | ditto |
| DialogController | 8 | `?OnUpdate@` | `0x1800cd270` | **`RET 0` — empty** | ditto |
| DialogController | 9 | `?OnElementEvent@` | `0x1800cd260` | **`RET 0` — empty** | ditto |
| DialogController | 10 | `?OnSerializeSettings@` | `0x1800cd250` | **`RET 0` — empty** | ditto |

Before writing a line, every one of the eleven was grepped by ADDRESS and by
MANGLED NAME across `code/`, `runtime/`, `engine/` and `standalone/`. All the
hits were comments, mint-table rows, or **call sites** (`tool_base_toolui.cpp`
and `mod_dialog_vtables.cpp` name `?Type@DialogInstance@` as a symbol to
*resolve*, not to define). None was already ported.

Each of the eleven mangled names was then checked against the **shipped export
table** with `pefile` before the build: 11/11 present at exactly the address the
brief gave, no phantom manglings.

### The wave brief's scoping was inverted, and it is worth recording

The brief told this lane to do the DialogInstance pair FIRST because it was "2
slots to a promotion", against DialogController's nine. **Slot count was not the
cost.** Nine of nine DialogController slots are trivial (seven empty, two
one-instruction loads); one DialogInstance slot — `?OnOpened@` — is the only
substantial body in the lane. The cheap promotion was the nine-slot one.

---

## 2. "Seven empty bodies" was a hypothesis, and it was checked three ways

`0x1800cd250`–`0x1800cd2b0` sit exactly 16 bytes apart, which *suggests* a run of
base-class do-nothing virtuals. The brief was explicit that this must not be
assumed from one reading, and it was not:

1. One 128-byte read at `0x1800cd250` returns the identical 16-byte cell seven
   times over:
   `c2 00 00 cc cc cc cc cc cc cc cc cc cc cc cc cc`.
   `C2 00 00` is `RET 0` — a near return popping zero argument bytes, i.e. a
   plain return on the Microsoft x64 ABI. The remaining thirteen bytes are
   `INT3` padding, so each body is 3 bytes and none falls through into the next.
2. Each of the seven was **also disassembled individually**. Ghidra reads
   `RET 0x0` and nothing else at every one of the seven.
3. `0x1800cd240`, immediately before the run, is `C3` followed by `CC` padding —
   so `0x1800cd250` is a function **start**, not the tail of a larger body that
   the 16-byte stride happened to sample.

"Trivial" did not exempt them: all nine are registered under both doors, and the
promotion they unlock is gated on the A/B below like any other.

---

## 3. `?OnOpened@DialogInstance@` — the one real body

It is **not** the mirror of `?OnClosed@`. `?LoadSettings@DialogInstance@` exists
at `0x180114450` and `OnOpened` does not call it; retail inlines a second,
different copy of the load, and runs the controller loop **first**.

Four things the decompiled C would have flattened, all taken off the
disassembly:

* **The inlined MemoryStream ctor/dtor were verified, not assumed.**
  `??0MemoryStream@IO@@QEAA@XZ` (`0x1800ae710`) is instruction-for-instruction
  the prologue at `0x180115019..94` — same two vtable stamps (`0x1801549a0` then
  `0x1801555b0`), same field zeroing, same lock / `MemAlloc(size=8, align=4)` /
  unlock, same `0x1801554a8` (the `IO::Encoding` vtable) stamped into the 8 bytes,
  same release of the previous `+0x10`. `??1MemoryStream@IO@@UEAA@XZ`
  (`0x1800ae890`) is exactly the teardown at **both** of the body's exits. The
  two different free forms retail uses on the two paths — an out-of-line
  `CALL 0x180006a10` and an inlined lock/`MemFree` — were each disassembled and
  are byte-identical COMDAT copies of one function
  (`if (p) { lock; MemFree(alloc, p); unlock }`). So the port calls the two
  exported bodies, exactly as `SaveSettings` already does, and the inlining is
  not reproduced.
* **The controller loop bound is RE-READ every iteration** (`CMP EDI,[RSI+0x80]`),
  like `OnClosed`'s and unlike `SaveSettings`'s snapshot — two loops over one
  container in one file disagreeing about that, and both reproduced.
* **When `_userFlags < 4`, retail reads UNINITIALIZED STACK for the magic.**
  `MOV EAX,dword ptr [RBP + 0x40]` at `0x180115102`, and `RBP+0x40` is the
  **caller's first home slot**, which nothing in this function has written by
  that point. This is the **one place the port is deliberately not
  byte-faithful**: reading an uninitialized local is UB in C++, so the local is
  initialized to a non-magic value and the bail-out is taken. Observable only if
  a caller left exactly `0xDEADBEEF` in its own shadow space, in which case
  retail would build a reader over a buffer shorter than its own 12-byte header.
  Named here rather than silently cleaned up.
* **The per-controller Serializer's CreateReader/CreateWriter branch is LIVE
  here**, where in `SaveSettings` it is dead — the mirror image, from the same
  inlined ctor.

The `found < 0` arm calls ReaderWriter slot `+0x10`, which
`runtime/types/io/Serializer.h` already pins as `kSkip`. A settings record whose
controller type id is no longer present is **skipped**, not misapplied — which is
what makes `SaveSettings`'s per-controller framing load-bearing rather than
decorative.

---

## 4. The A/Bs

Four runs of the pair, all under `tools/game_dir_lock.sh` as `lane-DLGV`, all read
from the `--snapshot` copies in `lane_dlgv_logs/`.

Each pair is `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120` — autostart to gameplay, then a pause-menu **Quit** —
with the candidate half adding
`BANISHED_RT_MINT_ROWS=??_7DialogController@UI@@6B@,??_7DialogInstance@UI@@6B@`.

**Gate (a), both rows.** On a live `BANISHED_RT_MINT_SHADOW=1` boot
(`docs/runtime_port/wave10b/dlgv_shadow_bodies.txt`):

```
READY  ??_7DialogController@UI@@6B@   slots=11 ported=11 TRAP= 0 purecall= 0
READY  ??_7DialogInstance@UI@@6B@     slots=10 ported=10 TRAP= 0 purecall= 0
```

(baseline at `7358732` was `2 ported / 9 TRAP` and `8 ported / 2 TRAP`).
`[RtPort] UI: 165 exports registered` → `176`, i.e. exactly the eleven new rows.

**Gate (b), pair 1 (`lane_dlgv_logs/ab/`).** Both halves reached gameplay
(`StorageDisplayComp::Draw`, `ProfessionList::Startup`, `EntityList` resolved),
opened the in-game pause menu, clicked Quit, ran the staged shutdown and left a
**0-byte crash log**. A whole-log frequency diff — every line normalised for
pointers and numbers, then compared as a multiset — finds **three** differing
lines, and all three are the `[RtMint] ROW ON (env)` / `registered 2 row(s)`
lines the candidate half prints. Everything else that moves is splash-loader tick
and fade frame counts (`ChunkedSceneLoader::Tick` 26 vs 32, `UIFade` 123 vs 118).

**The effectiveness control — the part that makes gate (b) mean anything.**
Registering a vtable is not passive: our ported ctors stamp the table, so the
DLL's own dispatch runs our slots. But ten of the eleven bodies write nothing
observable, so an identical A/B is *equally consistent with* "safe" and "never
ran". Both files therefore carry an ATTENDANCE counter (the
`runtime/src/io/encoding.cpp` idiom; deliberately **not** `RT_DIFF` sites, whose
`calls` moves only inside a comparison and would print 0 for a body that ran).
Pair 2 re-ran the same A/B with the counters in:

| | control | candidate |
|---|---|---|
| `[DlgSettings] OnOpened` | *not entered* | **entries=2** noBuffer=2 badMagic=0 records=0 |
| `[DlgCtrl]` (nine bodies) | *none entered* | ***none entered*** |

**Pair 3 (`lane_dlgv_logs/ab3/`)** went looking for a DialogController route
rather than declaring one impossible. `standalone/tool_harness.cpp` has a
`housepanel` mode; with `BANISHED_TOOL_HARNESS=housepanel BANISHED_LIVE_TOOLS=1`
the candidate half opened a residence info panel and left it open. Result:

| | control | candidate |
|---|---|---|
| `[DlgSettings] OnOpened` | *not entered* | **entries=3** noBuffer=3 badMagic=0 records=0 |
| `[DlgCtrl]` (nine bodies) | *none entered* | ***none entered*** |

That result was then **reproduced on the final default-on tip** (no env var at
all beyond the harness), and that is the run kept as the evidence —
`lane_dlgv_logs/ab3/housepanel_defaulton_*`, which carries
`[HousePanel] OPEN house[0]=… dialog=…` in its stderr alongside
`[RtMint] promoted 11 minted DATA row(s)`, `OnOpened entries=3 noBuffer=3`,
`[DlgCtrl] ATTENDANCE: none of the nine bodies was entered`, and a 0-byte crash
log. (Housekeeping note, since it bears on what a reader can re-check: the
original pair-3 stderr files were deleted while trimming the log directory before
commit, so the pair was re-run rather than the claim being softened.)

**Pair 4, the final tip (`lane_dlgv_logs/final/`)**, with the promotion written
into `reg_mint_promoted.cpp` and therefore **default-on, no env var**:

```
defaulton      [RtMint] promoted 11 minted DATA row(s) -- live on every run
               [DlgSettings] ATTENDANCE ... OnOpened entries=2 noBuffer=2
promoted_off   [RtMint] BANISHED_RT_MINT_PROMOTED=0 -- no minted row registered
               [DlgSettings] ATTENDANCE: OnOpened was not entered in this run.
```

Both 0-byte crash logs; the kill switch works in both directions; the shadow on
that same tip still reads READY/TRAP=0 for both vtables.

### 4.1 What this licenses, and what it does not

**`??_7DialogInstance@UI@@6B@` — PROMOTED.** READY on the screen, and the A/B has
a measured effect: our `?OnOpened@` went **0 → 2** and **0 → 3** entries on two
independent routes, with the rest of the boot byte-for-byte the same story and
no crash. Our body is what the DLL dispatches once the row is registered.

**`??_7DialogController@UI@@6B@` — READY, NOT PROMOTED.** Gate (a) is green.
Gate (b) is **uninformative**: zero entries in *both halves of both pairs*,
including the pair that had a real in-game panel open. That panel's dialog did
reach our `?OnOpened@` (entries 2 → 3) and its `_controllers` list was empty, so
not one controller virtual was dispatched. Every `UI::DialogController` subclass
in `code/Game/Dialog/` (DestroyUI, CitizenUI, MapUI, BuildUI, CropUI, DebugUI,
TutorialUI, …) carries its own **exe-side** vtable; nothing this lane could drive
constructs an object holding the Runtime **base** table. A run in which
registering the row changes nothing and our bodies run zero times cannot
distinguish a safe promotion from a port with no caller — which is
`reg_mint_promoted.cpp`'s own "WHY NOT ALL 104" rule, applied to a row whose
readiness number says go. **It expires the moment any route makes
`[DlgCtrl] ATTENDANCE` print a non-zero total.** The instrument is in the tree
and prints on every run, so nobody has to rediscover the question.

### 4.2 PORTED, LIVE, AND ONLY PARTLY COVERED — stated, not footnoted

Every one of the five `?OnOpened@DialogInstance@` entries reported `noBuffer`:
`_userData` was null, so the body took its second-line early exit. The controller
loop ran (over an empty list); **the settings-reading half — MemoryStream, the
magic, the Serializer, the per-record loop — has not been exercised by any route
this lane found.** `badMagic=0 records=0` on every run. That is the shape where a
zero counter inverts the conclusion
(`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`), caught by
splitting the counter across the body's exits rather than counting entries only.

The nine DialogController bodies are likewise **ported and registered but never
entered** in any run this lane made.

---

## 5. Census set diff — the SET, not the count

Both sides measured with the same `tools/runtime_census.py`, the base tip
materialised from `git archive 453e26d` so the scanner walks the right tree
(`reference_scanner_walks_cwd_not_script.md`).

```
base 453e26d           lane tip
UI  170 called  164 ported  96.5%      UI  171 called  165 ported  96.5%
                                        TOTAL 80.2% -> 80.3%

Called but not ported: 6  ->  6
```

**The count did not move. The set did.**

* **LEFT:** `??_7DialogInstance@UI@@6B@` — promoted.
* **ENTERED:** `?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A`.

The entrant is **not new cutover surface.** `runtime/src/ui/dialog_create.cpp`
has resolved that exact symbol by name since lane UI3 — through
`TypeIdOfStatic(mangled, cache)`, where the name arrives as a **variable**, which
the census's `called` scan cannot see
(`reference_literal_grep_blind_to_variables.md`). This lane's
`RT_DATA_SLOT(…, "?_typeInfo@DialogController@UI@@…")` puts the same name in as a
literal, so the row became *visible*, not *true*. Anyone reading "6 → 6" as "the
lane achieved nothing", or the entrant as a regression, has both backwards.

Note also what the census **cannot** see: the eleven function ports themselves
move neither column, because nothing in our tree *calls* e.g.
`?OnCreated@DialogController@UI@@`. The `+1 / +1` is the vtable promotion and the
typeinfo's new visibility. The lane's actual work is invisible to this
instrument.

### 5.1 The entrant is one promotion away — measured, not guessed

On the final tip's exit-phase shadow report:

```
READY  ?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A  id=0xf2c192b8 idcmp=yes strs=0 fac=2traps
```

(the init-phase line for the same row reads BLOCKED / `idcmp=NO` only because the
DLL's own `Factory::Startup` has not run yet — the shadow *skips and counts* that
comparison rather than passing it.) It has live consumers, which is the whole
difference MINTSEL's ten rows turned on. **This lane did not promote it**, for
one reason worth naming rather than a shortage of time: promoting it makes
`?TypeName@DialogController@` return **our** node's `name` pointer instead of the
DLL's, and pointer identity is exactly the failure mode this lane's own comment
rejected a string literal for. The shadow compares string *content* (`strs=0`),
not identity. That question should be answered before the row moves, and it is a
question about the already-promoted rows too.

---

## 6. Files

| file | what |
|---|---|
| `runtime/src/ui/dialog_controller_base.cpp` | **new** — the nine DialogController bodies, the `[DlgCtrl]` ATTENDANCE counter, registration under both doors |
| `runtime/src/ui/rt_ui_port.cpp` | wires it in behind `BANISHED_RT_DLGCTRL` |
| `runtime/src/ui/dialog.cpp` | `?Type@DialogInstance@`; corrects (rather than deletes) the now-stale ⚠ note beside `?TypeName@` |
| `runtime/src/ui/dialog_settings.cpp` | `?OnOpened@DialogInstance@`, the `kCtrlVtblOnOpened` constant, the `[DlgSettings]` ATTENDANCE counter |
| `standalone/reg_mint_promoted.cpp` | the promotion + the rewritten "NOT PROMOTED, and why" paragraph |
| `docs/runtime_port/wave10b/dlgv_shadow_bodies.txt` | the gate-(a) shadow log |
| `lane_dlgv_logs/` | every run's snapshot (`shadow1`, `ab`, `ab3`, `final`) |

Debug, and **release** (`mingw32-make standalone`, full rebuild from a cleaned
object dir, rc=0 — the nine `error` matches in that log are all filenames and
function names containing the word) both build clean.
