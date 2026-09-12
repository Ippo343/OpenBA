# WAVE 28 LANE FACTUI2

Branch `wave28/factui2` off `wave28/base` (`5d2594de`), private worktree.

Scope: the `?CreateNewObject@<C>@UI@@` / `?CreateInPlaceObject@<C>@UI@@` pair for
every UI class in the minted-BaseTypeInfo absent-factory roster sorting at or
after `ImageDescription`.

---

## 0. HEADLINE, AND WHAT IT DOES NOT SAY

**`SpriteSheet` LANDED, first, alone, in its own commit (`bf6dc59e`).** It is the
live cutover wall: attempt 30 died on
`?CreateNewObject@SpriteSheet@UI@@SAPEAVBase@Object@@XZ` (`0x180002f40`) at the
mint's per-slot FATAL. Both its slots are ported and registered.

All **35** classes in the slice are complete: **70 factory slots + 32
constructors = 102 registrations**, every one with BOTH doors (name and
address).

**AND THE RUNTIME CENSUS MOVED BY EXACTLY ZERO. That is MEASURED, with a
two-sided control, and it is the most important sentence in this document.**
See §4.

**Every row here registers with attendance 0 in an ordinary run.** That is
EXPECTED — the DLL reaches its own factories through an in-module pointer
stamped by a static initialiser, which is not a door (Wave 11 lane OBJINT). The
value is entirely at cutover. **This lane has measured nothing about whether
these 102 bodies are correct, and does not report "ported + registered" as
"verified".**

---

## 1. WHAT LANDED

| file | what |
|---|---|
| `runtime/src/ui/ui_factory_wave28.cpp` | all 102 bodies + `RtUiFactory2Register()` |
| `runtime/src/ui/rt_ui_port.cpp` | one `extern` + one call |

Kill switch `BANISHED_RT_UIFACT2`, **DEFAULT ON**, following
`stringtable_factory.cpp` and deliberately NOT `dialog_factory.cpp`: that file's
opt-in `BANISHED_RT_DLGFACTORY` is why `Dialog@UI` has been ported since Wave 11
and has contributed nothing to the live factory count for seventeen waves.

Registration runs from `RtUiPortInit()` (`standalone/resolve_standalone.cpp:333`),
which precedes `RtDataMintInit()` (`:406`). **That ordering was read in this
worktree on this tip, not assumed** (kickoff §2.2 caveat 2).

Classes NOT reached: **none.** All 35 are complete in both slots.
Constructors NOT ported: **2** — see §5 residual R1.

---

## 2. THE FAMILY IS NOT UNIFORM — this lane's strongest finding

WAVE28_KICKOFF.md §2.1 says, as the argument for hand-porting without a
generator:

> "The shape is uniform. Every `CreateNewObject` read so far: locked
> `MemAlloc(N, 8)`, retail's null check, an inlined ctor, return. … The variable
> parts are the allocation size, the vptr sequence the inlined ctor stamps, and
> the zeroed field extent — **all three readable mechanically from the
> disassembly.**"

MEASURED across all 35 classes of this slice, that describes **6 of them.**

```
shape A   6   ctor fully inlined, no calls at all          <- the kickoff's shape
shape B   8   ctor inlined AROUND an out-of-line call to a BASE ctor
shape C  21   factory is a bare `alloc / null test / call ??0<C>@UI@@ / return`;
              the entire body lives in the separately-exported constructor
```

The consequence is not cosmetic. For shapes B and C the "three variable parts"
claim covers only the **allocation size**; the vptr sequence and the zeroed
extent are not in the factory at all, and the marginal cost is a CONSTRUCTOR
that may live in another lane's class. `29 of 35` of this slice fall outside the
brief's model of the work.

The kickoff's own hedge — "Every `CreateNewObject` **read so far**" — was
accurate about its evidence and was then used to size 101 classes. **A shape
claim sized on the members that were cheap to read is biased towards the cheap
shape**, and the bias is invisible because the sample is not named.

Per class (allocation size and shape both read from the disassembly):

| class | alloc | shape | out-of-line base ctor | ctor ported by this lane |
|---|---|---|---|---|
| ImageDescription | 0x80 | A | – | yes |
| ImageElement | 0xe0 | B | `??0Element@UI@@QEAA@XZ` | yes |
| LabelDescription | 0xb8 | C | – | yes |
| LabelElement | 0x130 | C | – | yes |
| LayoutDescription | 0x78 | A | – | yes |
| LayoutElement | 0xd0 | B | `??0Element@UI@@QEAA@XZ` | yes |
| ListDescription | 0x90 | C | – | yes |
| ListElement | 0x100 | C | – | yes |
| NumberDescription | 0x88 | A | – | yes |
| NumberEditDescription | 0x88 | C | – | yes |
| NumberEditElement | 0x100 | B | `??0EditElement@UI@@QEAA@XZ` | yes |
| **NumberElement** | 0xe8 | C | – | **NO** (R1) |
| PageDescription | 0x80 | C | – | yes |
| PageElement | 0xc8 | B | `??0Element@UI@@QEAA@XZ` | yes |
| ProgressDescription | 0xb0 | C | – | yes |
| ProgressElement | 0x130 | C | – | yes |
| RadioDescription | 0x80 | C | – | yes |
| RadioElement | 0xd8 | B | `??0Element@UI@@QEAA@XZ` | yes |
| RibbonDescription | 0x80 | A | – | yes |
| RibbonElement | 0xd8 | B | `??0Element@UI@@QEAA@XZ` | yes |
| ScrollDescription | 0xc8 | C | – | yes |
| ScrollElement | 0x178 | C | – | yes |
| SliderDescription | 0xb8 | C | – | yes |
| SliderElement | 0x130 | C | – | yes |
| SpinnerDescription | 0x88 | C | – | yes |
| SpinnerElement | 0xc8 | B | `??0Element@UI@@QEAA@XZ` | yes |
| SpriteCapture | 0x58 | A | – | yes |
| **SpriteSheet** | 0x58 | A | – | reused, sprite.cpp |
| TabDescription | 0xb8 | C | – | yes |
| TabElement | 0x108 | C | – | yes |
| TableDescription | 0x98 | C | – | yes |
| TableElement | 0xf0 | C | – | yes |
| TextDescription | 0x90 | C | – | yes |
| **TextElement** | 0x100 | C | – | **NO** (R1) |
| ToolTipDialog | 0xc0 | B | `??0DialogInstance@UI@@QEAA@XZ` | yes |

---

## 3. METHOD — and the three-way agreement that gates every body

Bytes, addresses, extents and bodies came from `objdump -d` against
`C:\GOG Games\Banished\Runtime-x64.dll`, plus `pefile` for the export directory.
Ghidra was used for nothing a port depends on (standing warning 2).

**Every class's transcription was checked three ways before a line was written:**
the ctor as inlined into `CreateNewObject` (object pointer in RDI), the ctor as
inlined into `CreateInPlaceObject` (RCX), and the separately-exported
`??0<C>@UI@@QEAA@XZ` (RCX) must produce an identical op list. **35 of 35 agreed
on the `CreateNewObject`↔`CreateInPlaceObject` pair.** For the 21 shape-C classes
the third copy is a *call*, so the comparison there is degenerate by
construction — see §6 UNVERIFIED.

The transcriber is an abstract interpreter over the disassembly that **REFUSES
on any instruction it does not model**, so an unmodelled store cannot be
silently dropped. Final state: **35 modelled, 0 refused.**

Two checks that a green run does not imply, both run and both clean:

* **extent check** — no store in any factory or ctor reaches past that class's
  allocation size. 0 overruns.
* **shape-C check** — for each of the 21, the factory body is *exactly one call*
  and that call is to **its own** `??0<C>@UI@@QEAA@XZ`, not to some neighbour's.
  21/21.

### 3.1 Two defects my own instrument had, and what caught them

Recorded because both were of the shape this project keeps rediscovering: a
screen that returns a confident answer about a subject it has not checked.

1. **A whitelist "uniformity screen" passed a class the parser could not
   parse.** My first pass grepped every instruction in all 105 functions against
   a regex whitelist and reported "every instruction fits one uniform pattern" —
   which I nearly wrote down as the finding. The very next tool refused
   `ImageElement` on its second instruction. The whitelist was **over-broad**:
   it matched the *form* `mov %r?,0x??(%rdi)` without ever asking what the
   source register held, so `movl $0xffffffff,0x3c(%rdi)` and a call to
   `??0Element@UI@@` both read as "the standard pattern". **A screen built from
   a whitelist of what is ALLOWED cannot report what is PRESENT**, and its
   passing verdict is exactly as loud as a real one.
2. **A sign-extension "fix" that corrupted every sentinel.** I applied
   `if imm >= 0x80000000: imm -= 2**32` to `movq $imm,m64`, reasoning that
   `movq` sign-extends imm32. objdump **already prints the sign-extended 64-bit
   value** (`movq $0xffffffffffffffff`, not `$0xffffffff`), so the adjustment
   turned `0xffffffffffffffff` into `0xfffffffeffffffff` and
   `0xffffffff80000000` into `0xffffffff00000000` — in **7 classes**. Nothing
   downstream could have caught it: a sentinel is *supposed* to look arbitrary
   (standing warning 4, which names exactly this for RNG constants and applies
   verbatim to `-1`). It was caught only by printing the values in hex beside
   the disassembly I had already read by eye.

---

## 4. CENSUS DELTA — attributed row by row (standing warning 5)

### 4.1 `tools/mint_factory_census.py` — the instrument that CAN see this work

Re-derived on this tip, not quoted:

```
                                  wave28/base   this branch
minted BaseTypeInfo rows              104           104
slot-0 factory PRESENT in tree          2            37     (+35)
slot-0 factory ABSENT                 101            66     (-35)
no factory in retail either             1             1
```

**My slice's PRESENT count, re-derived: 35 of 35.** Attributed row by row: each
of the 35 classes in the §2 table contributed exactly one, and the file the
census names for every one of them is `runtime\src\ui\ui_factory_wave28.cpp`.
The other 2 are Wave 11's `Dialog@UI` and Wave 26's `StringTable@UI`, untouched.

Note the census's own caveat, which applies to my rows too: PRESENT is an
**upper bound on ported** — it is a name grep, and a comment counts as a hit.

### 4.2 `tools/runtime_census.py` — ZERO. Measured, with a control.

```
                    with my file present      with my file moved aside
UI    total/called/ported   1625 / 215 / 206      1625 / 215 / 206
TOTAL total/called/ported   4632 / 1430 / 1277    4632 / 1430 / 1277
```

**Identical in both directions.** 102 registrations, **zero rows of board
movement**, in both the `called` and the `ported` column.

The mechanism is MEASURED, not inferred: `runtime_census.py --csv` emits **0
rows whose key contains `CreateNewObject`** — not mine, not Wave 26's
`StringTable@UI`, not Wave 11's `Dialog@UI`. **The entire 206-slot factory
family is outside `runtime_census.py`'s population**, because `called` is "the
distinct functions OUR tree calls today" and nothing in our tree calls a
factory: the only consumer is the minted `BaseTypeInfo` row in
`standalone/rt_data_mint_tables.inc`, which the `called` scan does not read as
a call site.

**This matters beyond this lane.** The wave's own board (`1430/1277, gap 153`)
is the number three of its seven lanes are being scored against, and **it cannot
move for any of the three factory lanes, however much they land.** A reader who
diffs the board after the merge and sees nothing will conclude the factory lanes
underdelivered. They did not; the instrument does not observe them.
`mint_factory_census.py` is the only board that does.

---

## 5. RESIDUALS — each with an addressee

**R1 — two constructors are NOT ported, so their factories MOVE the wall rather
than clear it.**
`??0NumberElement@UI@@QEAA@XZ` (`0x180118c70`) and `??0TextElement@UI@@QEAA@XZ`
(`0x18011eb80`) each end by calling a **private** member —
`?CacheText@NumberElement@UI@@AEAAXXZ` (`0x1801189f0`) and
`?CacheText@TextElement@UI@@AEAAXXZ` (`0x18011ec10`). This lane did not read
those and did not port them; the two factory pairs call the exported ctor
through a port-registry-aware door, so with the DLL present they are correct and
at cutover they will fail on the ctor.
*Addressee:* **WAVE 29, as its own lane on the `CacheText` pair.** Not FACTUI1
or FACTREST — neither owns these classes.

**R2 — two base constructors this lane CALLS are unported anywhere in the tree.**
`??0EditElement@UI@@QEAA@XZ` (`0x1800d0f10`, needed by `NumberEditElement`) and
`??0DialogInstance@UI@@QEAA@XZ` (`0x180114ca0`, needed by `ToolTipDialog`).
Greps by NAME and by ADDRESS across `runtime/ code/ standalone/ engine/ dx11/`
returned nothing. `??0Element@UI@@QEAA@XZ`, needed by six of my classes, **is**
ported (`runtime/src/ui/element.cpp:482`, registered `:1895` by name and `:1896`
by address) and is reached.
*Addressee:* **lane FACTUI1 of this wave** — it owns `EditElement` and
`DialogInstance`. If FACTUI1 does not reach them, **WAVE 29's factory lane**.
When either lands, my two rows are satisfied with no edit to my file: the call
goes through `RtResolveCached` on the mangled name, so a registration reaches it.

**R3 — `runtime_census.py` cannot see the factory family (§4.2).** This is an
instrument gap, not a porting task, and this lane is forbidden from fixing it.
*Addressee:* **the WAVE 28 INTEGRATOR, before quoting a post-merge board.** The
board will read `1430/1277` after all three factory lanes merge; that is not
evidence they underdelivered. Quote `mint_factory_census.py` for them.

**R4 — `SpriteSheet`'s constructor has a NAME door but no ADDRESS door.**
`runtime/src/ui/sprite.cpp:985` registers `??0SpriteSheet@UI@@QEAA@XZ` by name
only; `0x18011d600` is not in the address registry. My factory reaches it by
name so this lane is unaffected, but a by-address consumer would walk past the
port into the DLL.
*Addressee:* **lane FACTUI1** if it touches `sprite.cpp`; otherwise **WAVE 29**.
I did not add it: `sprite.cpp` is not this lane's file.

---

## 6. MEASURED vs INFERRED vs UNVERIFIED

### MEASURED (I ran it on this tip)

* 35 of 35 classes transcribed with 0 refusals from the modelling transcriber.
* 35 of 35: `CreateNewObject`'s inlined ctor ≡ `CreateInPlaceObject`'s.
* 0 extent overruns; 21 of 21 shape-C factories call their own ctor.
* 102 export/address pairs: **0 phantom names, 0 address mismatches, 0 door
  mismatches** — every registered name verified present in the export directory
  **by RVA with pefile**, and every `RegisterRuntimeAddr` address equal to that
  symbol's export VA. 37 vtable symbols and 6 base-ctor symbols likewise present.
* 0 self-recursions (no `Ctor_X` both registered as `??0X` and calling `??0X`).
* `mingw32-make standalone-debug` exits **0**, no warnings.
* `mint_factory_census.py` PRESENT 2 → 37.
* `runtime_census.py` UNCHANGED with a two-sided control (§4.2).
* **A live `make sa-run` booted all the way into a rendering in-game world with
  all 102 registrations active: `standalone_crash.log` 0 bytes, 0 `FATAL` lines,
  `[RtPort] UI: 324 exports registered`.**
* None of my 35 rows shares a slot-1 symbol with another minted row (checked
  after FACTREST flagged that `?CreateInPlaceObject@Internal@Object@@` is slot-1
  for five rows, two of them UI — both are FACTUI1's, neither is mine).

### INFERRED (reasoned, not run)

* That reusing `?MemAlloc@System@@` (`0x1800c7a50`) for the inlined
  lock/Allocate/unlock triple is behaviourally identical. I *did* disassemble
  `0x1800c7a50` and diff it against the inlined copy instruction by instruction
  (rather than inherit `stringtable_factory.cpp`'s claim), so the *equivalence*
  is measured; that the extra call frame is unobservable is inference.
* That calling a ported ctor out-of-line where retail inlined it is unobservable.

### UNVERIFIED — in that word

* **Every one of the 102 bodies is UNVERIFIED.** The live run proves they do not
  *break* a booting game; it does not execute them. Attendance is 0 and the
  attendance line did not print, which is the expected result and **is not
  evidence of correctness in either direction.**
* **The 21 shape-C constructors have NO independent second copy.** For shapes A
  and B the three-way agreement is a real cross-check; for shape C the factory
  merely calls the ctor, so the "three-way" check degenerates to one reading of
  one function. Those 21 ctor bodies rest on a single transcription.
* The `BASE()` unresolved-base path has never been exercised (it needs the DLL
  absent). Its message text is UNVERIFIED.

---

## INSTRUMENT DEFECT OBSERVED, NOT FIXED

1. **`tools/runtime_census.py` — the 206-slot factory family is outside its
   population.** 0 CSV rows match `CreateNewObject`. Measured §4.2. Not fixed
   (no-tooling rule). File: `tools/runtime_census.py`; the `called` scan, which
   does not read `standalone/rt_data_mint_tables.inc`'s `factoryName` fields as
   call sites.

2. **`tools/game_dir_lock.sh` — `acquire --timeout N <holder>` takes `--timeout`
   itself as the holder name.** It printed `[gamelock] ACQUIRED by --timeout`
   and then refused `release factui2` with "lock is held by '--timeout'". The
   lock is real and was released by passing the literal string `--timeout` as
   the holder. A lane that named a holder and could not release it would leave a
   live lock blocking six sibling lanes. File: `tools/game_dir_lock.sh`,
   argument parsing.

3. **The session scratchpad is SHARED between sibling lanes.** `scratchpad/`
   is documented as "session-specific", but FACTUI1 and I are subagents of one
   session, so we share it: my `extract.py` and `exp.py` were both overwritten
   by FACTUI1's files of the same name mid-analysis. I moved to
   `scratchpad/factui2/`. **Two lanes silently clobbering each other's
   intermediate data is a contamination channel with no detector** — the second
   writer's tool simply produces the first writer's answer. Not a repo file, so
   nothing to fix here; recorded for the wave.

4. **`tools/ghidra_mcp/.venv/` does not exist in a worktree checkout** (it is
   untracked), so the kickoff's literal command
   `tools/ghidra_mcp/.venv/Scripts/python.exe tools/...` fails with
   "No such file or directory" for every lane in a private worktree. I used the
   main checkout's interpreter. File: `docs/runtime_port/WAVE28_KICKOFF.md` §2,
   and every lane brief that quotes that path.

---

## 7. NOT DONE

* No `?CacheText@` port (R1).
* No `EditElement` / `DialogInstance` ctor (R2 — another lane's classes).
* No verification of any body (§6). The wave explicitly does not pay that debt;
  this lane adds 102 rows to it.
