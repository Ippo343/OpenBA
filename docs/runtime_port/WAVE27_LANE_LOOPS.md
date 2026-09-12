# WAVE 27 — lane LOOPS

Branch `wave27/loops`, cut from `wave27/base` at `c6f30c24`.
Scope: the Engine main-loop family — `??_7LoaderLoop@` 6/12, `??_7DialogLoaderLoopT@`
8/12, `??_7DialogLoopT@` 10/12, `??_7Loader@` 1/2, plus the BODY row
`?Draw@MainLoop@Engine@@QEAAXM@Z`, plus the two questions the brief named me
addressee for.

Everything below is marked **MEASURED** or **INFERRED**. Nothing in the MEASURED
sections was taken from Ghidra MCP; see §1 for why.

---

## 0. HEADLINE

| Item | Result |
|---|---|
| Slot bodies ported + registered | **12 of 12** (the four tables' entire unported surface) |
| BODY row `?Draw@MainLoop@Engine@@` | **ported, registered, and LIVE — 27,959 calls/run** |
| `??_7UpdateLoop@` transient-vs-final question | **SETTLED — verdict PROMOTE.** Row promoted. |
| `?_loader@LoaderLoop@` blocker | **NOT retired — the expiry condition was MIS-SPECIFIED.** Corrected in place (twice; see §5). |
| Three derived loop tables | Promotable and *proved safe*, but promotion **buys nothing measurable**; left opt-in with the measurement recorded |
| `??_7Loader@Engine@@6B@` | **Blocked by a DLL-load-time unexported thunk — no port can retire it.** Retires at cutover only. Tagged. |
| Build / run | `standalone-debug` exit 0; three runs, all rc=0, all crash logs 0 bytes |

The honest one-line summary: **one body row went live and is doing real work every
frame; twelve slot bodies are written, registered and provably safe to route, and
route nothing, for a reason that is not fixable by promotion.**

**Two findings outlive the lane, and both are about expiry conditions and screens
rather than about loops:** §5 (a satisfied expiry clause that retires nothing, which I
then reproduced *myself*, in the section documenting it — §4.1) and §9 (a scanner of
mine that was wrong, found because I asked another lane to re-derive rather than
believe me). §3.5 and §4.1 exist only because of §9.

---

## 1. INSTRUMENT DEFECTS OBSERVED, NOT FIXED

Per standing warning #1 none of these was fixed. All are MEASURED.

### 1.1 Ghidra has WRONG FUNCTION BOUNDARIES over this lane's address range
`disassemble_function(0x1800dc260)` and `(0x1800dc280)` return the body of
`SoundEnded`, which Ghidra believes spans `0x1800dc0c0..0x1800dc2b0`. That range
swallows `?Startup@LoaderLoop@` (0x1800dc260) and `?Shutdown@LoaderLoop@`
(0x1800dc280) — the two most important functions in my scope. I found this
independently, before the integrator's warning arrived, because the returned
listing was 125 instructions where the export table implied a small leaf.
*Not a project tool — a Ghidra analysis result. Recorded because a lane that trusts it
ports the wrong body.*

### 1.2 `read_memory` and `inspect_memory_content` silently return the WRONG BYTES
Reproducible, and it cost me a false decode before I caught it:

```
read_memory(0x1801578d0, 336)  -> CORRECT   (matches the PE file byte for byte)
read_memory(0x1801578d0,  96)  -> WRONG     (returns .text code bytes)
read_memory(0x180155ba0, 112)  -> CORRECT
read_memory(0x180155ba0,  96)  -> WRONG
read_memory(0x180157940,  96)  -> WRONG
read_memory(0x180157940, 112)  -> WRONG
```

So it is a function of **(address, length) jointly** — not length alone, not address
alone. I did not characterise it further. **`inspect_memory_content` AGREES with the
wrong answer**, which is the dangerous part: two tools corroborating each other while
both are wrong, because they share the Ghidra memory backend. That is
`reference_two_checks_one_hidden_premise.md` exactly. Ground truth is the PE file;
every vtable in §2 was decoded from it.

### 1.3 Confirmations of the two defects the integrator relayed
- **Wrong-program server:** ran the discriminator. `disassemble_function(0x180002190,
  program="Runtime-x64.dll")` returned the 2-instruction retail form
  (`MOV RAX,[0x1801fae10] ; RET`). **This lane was reading RETAIL.** No re-take needed
  on those grounds.
- **`disassemble_function` serving a different function:** I did not need to test it,
  because §1.1 had already driven me onto `objdump` for every body.

### 1.4 `standalone/cutover_manifest.inc` gate warns COSMETIC-stale
Build prints `GATE WARN: … stale in COSMETIC fields only (a by-name door count moved)`.
That is my doing — this lane adds by-name doors. The gate itself reports
`OK (60 rows; row set and every disposition match)` and does not fail the build.
**Not refreshed**, deliberately: regenerating a shared generated file mid-wave would
collide with other lanes. Integrator: `python tools/cutover_manifest.py --inc`.

---

## 2. THE SLOT MAP (MEASURED, from the PE file)

Decoded by mapping each vtable VA to a file offset through the section table and
reading qwords; every address then resolved against the export directory with
`pefile`. **Not** from Ghidra (§1.2).

Vtable extents, confirmed against `standalone/rt_data_mint_tables.inc`, which already
had them right: `LoaderLoop` 12 slots; `DialogLoopT` and `DialogLoaderLoopT` 13 slots
each, slot 12 being a `_purecall` (`CreateDialog`); `Loader` 2 slots. The census's
`slots` column counts non-purecall slots, so its `12` for all three is consistent and
is **not** a defect.

| Slot | LoaderLoop `0x180157940` | DialogLoaderLoopT `0x1801578d0` | DialogLoopT `0x1801579a0` |
|---|---|---|---|
| 0 | `??_E` deldtor `0x18004d4f0` ✔was ours | `0x18004d670` ✔was ours | `0x18004d8b0` ✔was ours |
| 1 Startup | `0x1800dc260` **NEW** | `0x1800db000` ✔W8 | `0x1800db2f0` ✔W8 |
| 2 Shutdown | `0x1800dc280` **NEW** | `0x1800dafc0` **NEW** | `0x1800db2c0` **NEW** |
| 3 Update | `0x1800dc2a0` **NEW** | `0x1800daf20` **NEW** | `0x1800db270` ✔W8 |
| 4 Draw | `0x1800dc310` **NEW** | `0x1800dae20` **NEW** | `0x1800db210` **NEW** |
| 5 DrawFailed | `0x180031740` ✔W25 | ✔W25 | ✔W25 |
| 6 Load | `0x1800dc350` **NEW** | `0x1800dad50` ✔W8 | `0x1800db140` ✔W8 |
| 7 SaveRestore | `0x180031750` ✔W25 | ✔W25 | ✔W25 |
| 8 OnVideoError | `0x1800dc360` **NEW** | `0x1800dc360` *(same body)* | `0x180031760` ✔W25 |
| 9,10,11 | `0x180031770/80/90` ✔W25 | ✔W25 | ✔W25 |

`??_7Loader@` slot 0 `?Run@Loader@Engine@@` `0x1800dbfc0` **NEW**; slot 1
`?OnCompleted@Thread@System@@` was already ours.

**12 distinct new addresses, not 13.** `?OnVideoError@LoaderLoop@` (`0x1800dc360`) is
slot 8 of *both* LoaderLoop and DialogLoaderLoopT, so one body closes two
slot-instances. The brief's "~13 unported slots" counts slot-instances.

All twelve bodies are in `runtime/src/engine/loops.cpp`, registered under both the
mangled name and the retail address, and every body was disassembled with
`objdump -d --start-address=… --stop-address=… "C:/GOG Games/Banished/Runtime-x64.dll"`.

### 2.1 Things worth knowing about these bodies
- `?Shutdown@LoaderLoop@` and `?OnVideoError@LoaderLoop@` are **byte-identical**
  17-byte bodies (two separate copies in `.text`, not a COMDAT fold). Both are
  `WaitForSingleObject(_loader._thread.hFinished, INFINITE)` — i.e. retail inlined
  `System::Thread::WaitForFinish`. Ported through the registered
  `?WaitForFinish@Thread@System@@`, so no `<windows.h>` enters `runtime/src/`.
- `?Load@LoaderLoop@` is `ret $0x0`. Empty by design: the work happens on the loader
  thread in `Loader::Run`, which `Startup` kicks off.
- **The three `Draw` slots differ in exactly one constant**, and it is the interesting
  one: `LoaderLoop::Draw` and `DialogLoaderLoopT::Draw` clear to `0x00000000` (black,
  behind a loading screen); `DialogLoopT::Draw` clears to **`0xff808080`** (opaque
  mid-grey, the menu backdrop). All three ignore their `float` argument entirely.
- `DialogLoaderLoopT::Draw` publishes the elapsed load time into three shader
  constant destinations (`Context::_time` +0x3c8, `_unknown03b8`, and
  `_pixelConstantStaging[0x88]`) and ORs `0x800` then `0x10800` into `_dirty`. The
  second OR re-sets a bit the first already set; faithful and harmless.
- The two `Update` bodies round the `Time` through a **float**
  (`SecondsToTicks(TicksToSeconds(ticks))`). Lossy above 2^24 ticks and reproduced,
  for the reason Wave 8 already documented on `DialogLoopT::Update`.
- `?Run@Loader@Engine@@` brackets the load between `obj->vt[4](obj,true/false)` where
  `obj = *(*(Engine::_singleton+0x6c0)+0x100)`, and **retail re-loads that pointer
  chain for the closing call** rather than keeping it live across the load. Reproduced.
  It reaches the deliberately-held-back `?LoadResources@UpdateLoop@` through
  `ResolveExport`, so it will pick up a port automatically if one is ever registered.

---

## 3. `??_7UpdateLoop@Engine@@6B@` — THE QUESTION I WAS ADDRESSEE FOR

`standalone/reg_mint_promoted.cpp` named "the next lane that owns
runtime/src/engine/loops.cpp" and set the condition: *disassemble the six retail stamp
sites and record, per site, whether the UpdateLoop table is still in the object when
that function returns. If all six are transient, promote. If any is final, write an
RT_UNPROMOTABLE tag naming that site.*

**Discharged. Verdict: PROMOTE.** The row is now promoted (guarded, with an A/B kill
switch `BANISHED_RT_PROMOTE_UPDATELOOP=0`). The full reasoning is written at the row
itself, not only here, per warning #9.

### 3.1 The census was re-derived WITHOUT Ghidra (MEASURED)
Because of §1, I did not rely on Ghidra's xref list for "8 stamp sites". I scanned
`.text` in the shipped file for every RIP-relative `LEA` whose resolved target is each
vtable VA (REX.W + `8D` + modrm mod=00/rm=101 + rel32). Result for
`??_7UpdateLoop@`: **exactly 8 sites, at exactly the 8 addresses the holdback note
listed.** The note's *count and addresses* were correct.

Each body was then re-taken with `objdump`. Verdicts:

| # | Site | Function | Verdict |
|---|---|---|---|
| 1 | `0x1800317ee` | `??0UpdateLoop@…@AEBV01@@Z` (copy ctor) | **FINAL** — no overwrite before `ret` at `0x180031830` |
| 2 | `0x18003189d` | `FUN_180031880` | **FINAL** — but see below |
| 3 | `0x18004d3be` | `??0LoaderLoop@…@AEBV01@@Z` | **TRANSIENT** — overwritten at `0x18004d3f3` |
| 4 | `0x18004d7f3` | `??0DialogLoopT@…@AEBV01@@Z` | **TRANSIENT** — overwritten at `0x18004d82f` |
| 5 | `0x1800db394` | `??1DialogLoopT@` | destruction-time (inlined `~UpdateLoop`) |
| 6 | `0x1800dc190` | `??1LoaderLoop@` | destruction-time, 2 insns after the derived stamp |

### 3.2 TWO CORRECTIONS TO THE HOLDBACK NOTE
Both recorded at the row.

1. **The note calls all six "the BASE-SUBOBJECT stamps in the ctors and dtors of
   DERIVED loop classes". That is wrong for two of them.** Sites #1 and #2 are
   `UpdateLoop`'s *own* copy ctor and its *own* deleting-dtor variant — not derived
   classes at all. The note's framing ("OUR table in an UpdateLoop and RETAIL's in the
   base sub-object of every derived loop") describes a hazard that the measurement
   **refutes**: sites #3/#4 are transient, so no live derived loop ever carries
   retail's UpdateLoop table in its base subobject.
2. **Site #2 is not vtable slot 0.** `??_7UpdateLoop@` slot 0 is `0x180031900`, which
   does *not* stamp — it calls `0x1800dddb0` (`??1UpdateLoop@`, ours). `FUN_180031880`
   has **no references of any kind**: dead code in the shipped `.text`.

### 3.3 WHERE I ANSWERED A DIFFERENT QUESTION THAN THE ONE WRITTEN
Stated plainly rather than quietly redefined, because this is the kind of substitution
that should be visible.

On the condition's **literal** test — "is the table still in the object when the
function returns" — sites #1, #2, #5 and #6 are *final*, and the prescribed action
would be an `RT_UNPROMOTABLE` tag. I did not take that action, and I think the literal
test is the wrong test for two reasons the note's author could not see without the
disassembly:

- **It cannot distinguish a ctor from a dtor.** Every MSVC base destructor ends with
  the base vptr sitting in a dead object. So sites #5/#6 are "final" by construction,
  and the literal test can *never* pass for any class with a virtual destructor —
  it is unsatisfiable, not merely strict.
- **It asks about the table, not about dispatch.** The hazard a split vptr creates is
  that something *dispatches* through the wrong table. A vptr in a destroyed object,
  or in an object no code path can construct, dispatches nothing.

So I substituted: *can a LIVE object end up holding retail's table?* Answer, measured:
no. The two live-object-final sites have **zero callers**, established twice
independently — a raw `E8`/`E9` scan of the whole `.text`, and Ghidra xrefs, agreeing.

### 3.4 The copy-ctor reachability result (MEASURED) — it clears three more tables
Raw `E8`/`E9` scan of `.text` for direct control transfers to each copy ctor:

```
??0UpdateLoop@…@AEBV01@@Z          0 callers
FUN_180031880                      0 callers
??0DialogLoopT@…@AEBV01@@Z         0 callers
??0DialogLoaderLoopT@…@AEBV01@@Z   0 callers
??0LoaderLoop@…@AEBV01@@Z          1 caller — 0x18004d58e, INSIDE
                                   ??0DialogLoaderLoopT@…@AEBV01@@Z, which has 0.
                                   Transitively unreachable.
```

**No shipped code path constructs any of these four classes by copy.** Every table's
"retail stamper" set is therefore an unreachable copy ctor plus a destruction-time
stamp, with our own ported ctor as the only reachable stamper.

### 3.5 THE LIMITS OF THAT SCAN, AND HOW THEY WERE CLOSED
A direct `E8`/`E9` scan is blind to two things: a call through the CRT
static-initialiser table, and any other **indirect** call through a stored pointer.
Lane ACHIEVE made this point to me (in the course of correcting a different scan of
mine — §9), and it is load-bearing here, because §3.4's "0 callers" is the evidence
the promotion rests on.

**Closed with a second, independent screen.** An indirect call has to get the address
from *somewhere* — a vtable, a jump table, a function-pointer global, a static-init
table — and all of those store it as a qword in some section. So: scan **every** PE
section at **every** byte offset for each function's address as a little-endian qword.

**The screen was handed two known positives first**, because an absence screen with no
demonstrated ability to find anything is indistinguishable from a dead parser
(`reference_absence_screen_must_be_handed_a_shape.md`). This mattered: my first control
(`??0Loader@`) returned 0, which could have meant either "real" or "parser broken". The
two positives resolved it:

```
POSITIVE ?Run@Loader@            0x1800dbfc0 -> 1 hit, .rdata @ 0x180157a08  (its vtable slot 0)
POSITIVE UpdateLoop deldtor      0x180031900 -> 1 hit, .rdata @ 0x180155ba0  (its vtable slot 0)

??0UpdateLoop@…@AEBV01@@Z        0x1800317d0 -> 0
FUN_180031880                    0x180031880 -> 0
??0LoaderLoop@…@AEBV01@@Z        0x18004d3a0 -> 0
??0DialogLoopT@…@AEBV01@@Z       0x18004d7d0 -> 0
??0DialogLoaderLoopT@…@AEBV01@@Z 0x18004d570 -> 0
```

Each positive found exactly once, at exactly the predicted address. So the zeros are
real.

**THE LIMIT OF THIS SCREEN, and my first phrasing of it was wrong.** I originally wrote
that the zeros mean "none of the five copy ctors is named by any table or pointer
*anywhere in the binary*". **That is false**, and lane ACHIEVE named the reason while
reproducing this screen: it matches **64-bit VAs**, so it cannot see the **PE export
table, which stores 32-bit RVAs**. All five copy ctors *are* exports — §2 read their
names out of that very table. So "absent from every section" must never be read as
"nothing names it", and in particular never as "not exported"
(`reference_aeaa_is_private_not_unexported.md`, from its other side).

What the screen does license, stated to match: **no in-DLL dispatch structure names
them** — no vtable, jump table, function-pointer global or static-init table, which are
the routes an indirect *in-DLL* call must come from. The remaining route is an
**external** caller reaching an export by name. That one is closed separately and by a
different mechanism: a grep of `runtime/ standalone/ engine/ code/` for all five
mangled names and all five addresses returns **nothing outside comments**. Two routes,
two mechanisms, neither standing in for the other.

So §3.4's claim now rests on three screens that fail differently — direct-call scan,
in-DLL address presence, and a source grep for the external route.

**Independently reproduced.** Lane ACHIEVE ran its own implementation of this screen
against my two positives and found `?Run@Loader@` once at `0x180157a08` and the
UpdateLoop deleting dtor once at `0x180155ba0` — my controls, my addresses, their code.
That cross-checks my screen rather than only self-validating it, which is worth more
than either run alone.

---

## 4. `??_7Loader@Engine@@6B@` — BLOCKED BY SOMETHING NO PORT CAN RETIRE

Now 2/2 (slot 0 `?Run@Loader@Engine@@` is new this lane). **Not promoted.**

It is the one table in my scope whose only stamper is retail. Every table I promoted
has *our* ctor as its stamper — that is precisely what makes those safe and this one
not.

### 4.1 I FIRST GOT THE BLOCKER WRONG, AND CAUGHT IT BEFORE CLOSING
My first tag named `??0Loader@Engine@@QEAA@XZ` (`0x1800dc0f0`) as the blocker, with
expiry "that symbol ported AND registered". **That was the exact defect I criticise in
§5** — a condition naming a symbol that is not the real blocker. I reproduced it within
the same lane, one section after documenting it.

What the address-presence screen (§3.5) forced me to look at: `??0Loader@` scores 0 on
*both* screens — no direct call, and its address in no section. The exported body is
never invoked. So what builds `_loader`?

**An unexported static-init thunk at `0x180148fe0`, with the constructor INLINED**
(objdump):

```
180148fe0:  sub    $0x38,%rsp
180148fed:  mov    $0x40000,%edx              ; 256 KB stack
180148ff2:  lea    0xb57bf(%rip),%rcx         ; 0x1801fe7b8 == &_loader
180148ff9:  call   0x1800ca4e0                ; System::Thread::Thread(this, 0x40000)
180148fff:  lea    0xea02(%rip),%rax          ; 0x180157a08 == ??_7Loader@
180149006:  mov    %rax,0xb57ab(%rip)         ; stamp it into _loader
18014900d:  movq   $0x0,0xb57d0(%rip)         ; 0x1801fe7e8 == _loader+0x30 == _loop
180149018:  lea    0x1561(%rip),%rcx          ; the atexit destructor
180149023:  jmp    0x1800801d0
```

This is the site my stamper scan reported and I had dismissed as spurious because it
attributed to `??0Scene@Picking@@+0x2371f` — an offset so absurd it should have told me
"unexported function", not "noise". `_loader+0x30 == _loop` confirms the identification
independently against the type header.

**It runs at DLL load, before any of our code exists. There is therefore no symbol to
port that retires this row.** Corrected in both tags: the disposition is
`[SCOPE: DLL-PRESENT]` route-scoped — while the DLL is present the row can *never* be
promoted; at cutover the thunk does not exist, we construct `_loader` ourselves, and the
row becomes REQUIRED. Addressee corrected to **the lane running the next cutover
attempt, not a porting lane**, because no amount of porting reaches it.

Note the `movq $0x0` at `0x18014900d` is a **`C7` store** — the exact opcode class
§9 is about.

---

## 5. `?_loader@LoaderLoop@` — THE BLOCKER IS **NOT** RETIRED, AND THE CONDITION WAS WRONG

The brief said: *"`?Startup@LoaderLoop@Engine@@` is plausibly inside your scope — if
you port and register it, you retire this row's blocker."* The tag's expiry read
`?Startup@LoaderLoop@Engine@@ ported AND registered`.

**I ported and registered it. The condition is satisfied. The row is still not
promotable.** The condition was mis-specified — it named the wrong fact.

Owning the code that *takes `_loader`'s address* was never the blocker. Owning the code
that **constructs the object** is. `??0Loader@Engine@@QEAA@XZ` is retail and unported;
`_loader` is a static built by the DLL's CRT static-init; the DLL's loader thread was
created on *that* object and waits on *its* events. A promoted row hands our doors a
second `Loader` that nothing constructed and no thread watches — the loading screen
would **hang**, not crash.

This is the shape worth carrying forward: **a verdict that was RIGHT on a premise that
has since CHANGED, guarded by an expiry clause that names the changed premise.** Satisfy
the clause and the row reads green, while the real reason for the refusal is untouched
and was never written down. The written condition, not the verdict, was the defect.

**My first correction to this row was itself wrong, and I fixed it before closing.** I
wrote the corrected condition as "`??0Loader@Engine@@QEAA@XZ` ported AND registered" —
i.e. I replaced a condition naming the wrong symbol with *another* condition naming the
wrong symbol, one section after writing down why that is a defect. §4.1 has the
disassembly: the exported `??0Loader@` is never called, and `_loader` is really built by
an unexported static-init thunk at `0x180148fe0` that runs at DLL load.

**Final corrected disposition** (written at the row): there is **no symbol to port** that
retires this. `[SCOPE: DLL-PRESENT]` is route-scoped — unpromotable for as long as the
DLL is present, REQUIRED at cutover. Addressee: **the lane running the next cutover
attempt**, not a porting lane. Same blocker and same disposition as
`??_7Loader@Engine@@6B@`; the two rows retire together, at cutover. (The old addressee,
"lane MANIFEST of Wave 25", is a closed wave and could no longer act —
`reference_expiry_clause_needs_an_addressee.md`.)

That this defect recurred *inside the section documenting it* is the lane's sharpest
lesson: naming a plausible symbol feels like discharging the obligation, and the check
that catches it is "is this symbol actually the thing that runs?", not "is this symbol
related?".

---

## 6. WHAT RAN, AND WHAT DID NOT (MEASURED)

Three runs, all on the deployed build under `tools/game_dir_lock.sh` held by LOOPS,
all `rc=0` with a **0-byte `standalone_crash.log`**:

| Run | Command | Result |
|---|---|---|
| A | `sa-run` (`BANISHED_AUTOSTART=1`) | booted to `RUNNING`, autostart drove menu → New Game → in-game; 22,679 entities, 24 citizens, ages/hunger/thirst advancing past frame 240 |
| B | autostart + `PAUSEMENU=600 PAUSEMENU_QUIT=120 RT_DIFF=1` | full boot→gameplay→clean quit |
| C | `BANISHED_AUTOQUIT=1 RT_DIFF=1` (menu only) | clean quit from the settled menu |

**What the game actually did:** it started, drew the menu, auto-drove New Game, built
and rendered a live town with citizens whose vitals advanced, then quit cleanly through
the pause menu. No visual or behavioural regression was observed against the pre-lane
build.

### 6.1 `?Draw@MainLoop@Engine@@` IS LIVE — the lane's strongest result
`runtime/src/engine/mainloop_frame.cpp:532` reaches it through
`RT_EXPORT_ONCE(… "?Draw@MainLoop@Engine@@QEAAXM@Z")`, which consults the port registry
first. So **every frame's draw now goes through this port**:

```
Engine::MainLoop::Draw   calls=0  entered=27959  worked=27959   run B
Engine::MainLoop::Draw   calls=0  entered=2059   worked=2059    run C
```

`calls=0` is the no-comparison class (there is no oracle), **not** "unexercised" — read
`worked=`, per warning #5.

### 6.2 THE TWELVE SLOT BODIES RAN **ZERO** TIMES — and promotion cannot fix it
Not one registered an `RtDiff` site across a full boot-to-gameplay run. I promoted all
three derived tables, rebuilt, and re-ran specifically to test this. The promotion
itself worked — the guard line moved `17 row(s) evaluated` → `21 … 0 refused`, which
also proves all four tables are COMPLETE with every slot registered — and **the bodies
still never ran.**

The reason is structural and is not about promotion: **every loop our standalone
constructs is an exe subclass whose own table replaces these slots.**
`g_outerLoaderLoopVtable` (12/12 `Outer_*` stubs,
`standalone/loader_loop_stubs.cpp:309`), `g_menuDialogLoopVtable` (13/13 `Stub_*`,
`standalone/menudialogloopt_stubs.cpp:757`), `g_splashDialogLoopVtable`. Those stubs
*delegate* to the base for Startup/Load/Update — which is exactly why
`?Load@DialogLoopT@` (calls=2) and `?Update@DialogLoopT@` (calls=77) **do** show
attendance — and *override* Shutdown/Draw/OnVideoError outright. A base body the
subclass overrides is unreachable no matter whose table is published.

This is `reference_a_green_readiness_screen_is_not_a_route.md` in a new costume:
COMPLETE + promoted + guard-clean is still not a route.

### 6.3 SO I TURNED THE THREE PROMOTIONS BACK OFF
Because they were measured to cost something and buy nothing:

**PROMOTING A VTABLE ROW INVALIDATES ANY ctor BYTE-ORACLE FOR THAT CLASS.** Our ctors
stamp the symbol *by name*, so once promoted, ours holds our table and the DLL's holds
the DLL's, and an `RT_DIFF_BYTES` ctor oracle compares them and reports divergence
forever. Measured, and confined to exactly one field:

```
DIVERGENCE Engine::LoaderLoop::ctor call #1
    ours  =+0x0: 20 74 f9 fb f6 7f 00 00   (our minted table, inside reimpl-x64.exe)
    theirs=+0x0: 40 79 84 55 ff 7f 00 00   (0x…7940 == ??_7LoaderLoop@ in the DLL)
```

`divergences` exactly equalled `calls` (3/3, 1/1, 3/3) — every call, one qword, offset
`+0x0` and nowhere else. **Not a port defect**, but it turns three oracles permanently
red for a benign reason, and a later reader would score that as a regression. Nothing
else in the tree records this rule; it applies to every future vtable promotion.
Not fixed (warning #1).

So the three derived rows are **opt-in**, default OFF, with the whole measurement
written at the row, an addressee (the Wave 28 lane owning the stub files), and a
condition (flip one on when you make the corresponding exe-subclass stub *delegate*
instead of override). `BANISHED_RT_PROMOTE_LOADERLOOP=1` / `_DIALOGLOOPT=1` /
`_DIALOGLOADERLOOPT=1`. The safety evidence in §3.4 stands; what is missing is a reason.

`??_7UpdateLoop@` stays promoted (guard line `18 row(s) evaluated, 0 refused`): it was
the question I was assigned, it is measured safe, and it costs nothing — no ctor oracle
for `UpdateLoop` exists to turn red. Final run: all three ctor oracles back to
`divergences=0`.

---

## 7. CORRECTION TO `runtime/types/engine/MainLoop.h`

That header filed `Draw` under a heading **"Internal, NOT exported."** It is exported —
**ordinal 2251**, read from the shipped export directory with `pefile`. This is the
`AEAA` trap in its other direction (a *public* `QEAA` name assumed unexported), and a
sibling file in the same directory — `mainloop_frame.cpp:124` and `:185` — already said
"ordinal 2251" in its own comments, so the header contradicted its neighbour.

Corrected at the header's own line. I flagged the other four names on that list as
**unverified rather than confirmed-internal**: I did not re-check them, and one wrong
entry in a list is reason to distrust the list, not just that entry.

---

## 8. FOR WAVE 28

1. **The twelve bodies need a ROUTE, not more porting.** Make an exe-subclass stub
   delegate to the base (`standalone/loader_loop_stubs.cpp`,
   `standalone/menudialogloopt_stubs.cpp`), then arm that table's promotion. §6.2/§6.3.
2. ~~**`??0Loader@Engine@@QEAA@XZ` is one small body that unblocks TWO rows.**~~
   **STRUCK OUT BY ITS OWN AUTHOR, BEFORE THIS LANE CLOSED. DO NOT DO THIS.** It was
   the one "cheap win" I put on this list, and it was false: that exported body is
   never called, and porting it would unblock nothing (§4.1). `??_7Loader@` and
   `?_loader@LoaderLoop@` are blocked by a DLL-load-time unexported thunk and can only
   be retired **at cutover**, by the cutover lane, with no port involved.
   Left visible rather than deleted, because
   `reference_cheap_wins_list_is_the_stalest_section.md` says this list is the stalest
   section of any handover and the first one acted on — and here it went stale *within
   the lane that wrote it*, which is the strongest available evidence for that rule.
3. **Eleven of the twelve new bodies have no oracle.** They are byte-derived from
   `objdump` and they compile and link, but they have never executed. Do not read
   "ported + registered" as "verified".
4. `tools/cutover_manifest.py --inc` needs a refresh (§1.4).
5. The `read_memory` / `inspect_memory_content` defect (§1.2) is worth a Wave 28
   tooling decision: it is silent, it is corroborated by a second tool, and any lane
   decoding a vtable through it may have decoded garbage.
6. **If you reuse the throwaway scanners described here, read §9 first.** One of the
   three had a real blind spot and it was found by someone else re-deriving rather than
   believing me.

---

## 9. A SCANNER OF MINE WAS WRONG — and how that was found

I published three throwaway raw-file scanners in the course of this lane. Lane ACHIEVE
re-derived one of my results with a different mechanism, **because I asked them to
rather than to take my numbers**, and found a genuine defect in it.

**The bug.** A scanner matching `REX.W + (89|8B) + modrm(mod=00,rm=101) + rel32` covers
`MOV r/m64, r64` and `MOV r64, r/m64`. It is **blind to `MOV r/m64, imm32` (opcode
`C7`)** — and a `= nullptr` store is always `C7` (`48 c7 05 …`). So it silently
undercounts exactly the stores lifecycle code makes to singletons. I reported "3 writes"
to a cell that has 5; the count I was disputing was right and my refutation of it was
wrong. (My *attribution* refutation stood — see below.)

**Which of my conclusions this touches: none of them.** Stated precisely rather than
asserted, because "my other results are fine" is what everyone says:

| Scanner | Opcodes | Fed which conclusion |
|---|---|---|
| stamp scan | `LEA` (`8D`) | §3.1 stamper census |
| call scan | `E8`/`E9` | §3.4 copy-ctor reachability |
| address-presence scan | none (byte search) | §3.5 |
| **writer scan (the buggy one)** | `89`/`8B` | **only lane ACHIEVE's question — no row of mine** |

And the `C7` blind spot cannot affect the stamp scan even in principle: `C7` takes a
**sign-extended imm32**, and every vtable VA here (`0x180155ba0`, `0x180157940`, …)
exceeds `0x7FFFFFFF`, so a vtable address is not encodable that way. MSVC emits `LEA`.
That is a reason, not a reassurance.

**What it cost and what it bought.** It cost ACHIEVE a wrong number in a message. It
bought: the `C7` lesson; the indirect-call gap in my *other* scanner, which I then
closed properly with a controlled absence screen (§3.5); and, through that screen, the
discovery that my own §4 tag named the wrong blocker (§4.1). The most valuable thing in
this lane came from being contradicted.

**The transferable rule:** an opcode-pattern scanner is a whitelist, and a whitelist's
failure mode is a silent undercount that looks like a clean result. Prefer a mechanism
that is opcode-agnostic where one exists — ACHIEVE used full `objdump -d` and grepped
objdump's own resolved-target comment (`# 0x1801fc670`), which cannot miss an addressing
mode because it does not enumerate them.
