# WAVE 29 KICKOFF — 2026-09-12

Base: `wave29/base` at `81b0a238` (= the final merged Wave 28 tip, including
FACTUI1's four late doc-only commits).

Eight lanes. **Every figure below was re-derived on this tip before the document
was written**, and each carries the command that produced it. Nothing in this
kickoff is copied from `WAVE28_CLOSED.md` prose. That is not a stylistic
preference: Wave 28 §7 records that its own integrator quoted rather than
re-derived three times, and §4.4 records that all four "still open, and here is
why" blockers handed to lane MISC misdescribed reality.

---

## 0. STANDING WARNINGS — read all of them, they each cost a previous wave

1. **Re-derive every number you are handed, including the ones in this file.**
   Where this file gives a figure it gives the command. Run it. If your figure
   differs from mine, **yours is the one to trust** and say so in your lane doc.
2. **The itemisation carries the truth, not the headline.** Wave 28's kickoff
   headline said 34 classes and its own list summed to 35. If a count and a list
   in this file disagree, work the list.
3. **A "cheap wins" list is the stalest section of any handover and the first one
   acted on.** So is a "still open, and why" list. **The tell: a blocker naming a
   MISSING CAPABILITY is a claim about the tree and expires; one naming an
   INTRINSIC property does not.** Re-check your lane's stated blockers FIRST —
   three cost one grep each last wave.
4. **Do not "fix" a faithful crash with a null check.** Find the upstream null.
   Retail's unguarded derefs are load-bearing.
5. **Retail SHIPS EMPTY BODIES.** Porting a real body into one is a defect, and
   no census can tell an abstract/empty body from an unported one.
6. **An instrument that prints a verdict without naming what it examined is not
   evidence.** Every screen you write must print its subject and its population.
7. **A screen looking for the ABSENCE of a shape must be HANDED one.** "0 hits"
   is indistinguishable from a dead parser. Same for a control whose normal
   answer is empty — hand it a known positive.
8. **A DEFLATING blind spot outranks an inflating one.** An inflating screen
   manufactures work and is self-limiting; a deflating one reports that work
   does not exist and nothing detects that.
9. **A positive control scored on the UNION of two screens hides a blind screen.**
   Score each screen separately.
10. **Your `*Summary()` is probably never called** (Wave 28 defect #7). A zero
    from a counter means "never printed", not "never called". Verify your
    summary actually prints before you believe any attendance number.
11. **`calls=0` does not mean unexercised.** It can mean the body ran every time
    and always took the branch your oracle does not cover.
12. **A body PORTED but not REGISTERED is indistinguishable from an ABSENT one**
    to a callee census. Before writing "unported", check the registration call.
13. **Before writing "unverifiable, no route": grep `standalone/` and the
    `BANISHED_*` variable list.** `tool_harness.cpp` has repeatedly already held
    the driver.
14. **"No gameplay route" is NOT "no oracle".** A synthesised oracle for a
    provably unreachable body has found real defects.
15. **A raw byte-diff of two copies of one function reports ~23% different**
    because rel32/RIP fields encode distances. Compare position-independently.
16. **Set-difference over the WRONG EXTENT gives a real answer to a different
    question.** `.pdata` understated one extent by 24x. Derive your extent.
17. **Never run `find /`.** Three Wave 22 lanes did and burned ~4,500 CPU-seconds.
    Search the repo with Grep/Glob scoped to your worktree.
18. **Do not edit anything after you report.** If you must, say so loudly in a
    second report. The integrator will run
    `git diff --stat <base>...<your-branch> -- runtime/ standalone/ code/ engine/ dx11/`
    on your tip at merge time; a non-empty result invalidates the wave's board.

## 0.1 WAVE INFRASTRUCTURE — the four defects that cost every lane last wave

- **The venv does not exist in your worktree.** Only `requirements.txt` is
  checked in. **Use plain `python`** — verified 3.12.10 on this machine, and
  every tool in `tools/` used by this kickoff was run with it to produce the
  figures below. Do NOT use the kickoff-literal
  `tools/ghidra_mcp/.venv/Scripts/python.exe` path; it is absent in a worktree.
- **The session scratchpad and `/tmp` are SHARED between sibling lanes**, and
  files were clobbered on basename collisions last wave — once producing the
  *exactly correct expected number* from a sibling's stale file.
  **Each lane below is assigned a private scratch directory. Use only that, and
  prefix every filename with your lane name anyway.**
- **A backgrounded `mingw32-make` wrote a zero-byte output and reported exit 0.**
  Build in the FOREGROUND, and use the build gate in §0.2.
- **`git worktree`**: you are launched into your own private worktree. Do not
  branch-switch in a shared checkout, and do not `cd` out of your worktree.

## 0.2 THE BUILD GATE — exit code plus file size cannot distinguish "linked my work" from "relinked without it"

This is FACTUI1's gate and it is mandatory for any lane that compiles:

```bash
mingw32-make standalone-debug            # FOREGROUND, record exit code
ls -l standalone/build/reimpl-x64.exe    # size AND mtime
find runtime standalone code engine dx11 \
     -newer standalone/build/reimpl-x64.exe \
     \( -name '*.cpp' -o -name '*.h' -o -name '*.inc' \) | head
                                         # must be EMPTY
nm standalone/build/reimpl-x64.exe | grep <your registration function>
strings standalone/build/reimpl-x64.exe | grep <a mangled symbol you added>
```

Both `nm` and `strings` must find your work in the LINKED IMAGE.

## 0.3 THE GAME DIRECTORY IS A SINGLE SHARED RESOURCE

Only `C:\GOG Games\Banished` exists. Any lane that RUNS the exe must take
`tools/game_dir_lock.sh`.

- **Never pipe the acquire.** `acquire | tail` makes a TIMEOUT read as a GRANT,
  because a pipeline exits with the LAST command's status.
- **Never background a blocking acquire.** The lock is held from the moment it is
  GRANTED, not when you notice.
- Poll boot with `tools/boot_state.sh --wait 300`, **never** a `grep -q` loop on
  the log — a blocked boot stays alive and pumping and will burn your whole
  timeout.
- Lanes WALL and MINT both need cutover boots. **WALL has priority**; MINT
  queues behind it and must say in its doc how long it waited.

## 0.4 RECONSTRUCTING A CUTOVER ARM

**Never grep a log for `BANISHED_*` strings** — that matches printed prose, not
the environment. Wave 28's integrator did this and burned three runs. The arm is
printed by the code that READ the variable:

```
[RtAlloc] ATTEST: ARMED / UNARMED
[RtDiff]  ON
```

The arm, from the Wave 28 §2.1 post-mortem: `BANISHED_RT_NO_DLL=1` **and**
`BANISHED_RT_ALLOC=1` (REQUIRED at cutover, harmful with the DLL present) and
**`BANISHED_RT_DIFF` OFF** (RtDiff runs the original alongside the port; with the
DLL absent the original is null, giving EXEC-of-0 at t≈6ms). **Re-derive it from
`tools/cutover_attempt30.sh` and from attempt 31's ATTEST lines before you run.**
Verify with `grep -c "RtDiff] ON"`.

**And always run a CONTROL from the pre-wave tree with the SAME arm.** That is
what converted "the merged tip regressed by 577ms" into a one-line arm error in a
single run last wave. Otherwise a moved wall cannot be told from a moved tree.

---

## 1. THE BOARD, RE-DERIVED ON THIS TIP

```
$ python tools/runtime_census.py
TOTAL              4632     1453     1294        9   89.1%
gap 159 = 24 BODY + 0 CTOR + 0 DTOR + 0 OPNEWDEL + 0 OPERATOR
        + 38 VTABLE + 49 DATA + 48 UNKNOWN
```

Unchanged from Wave 28's published close. Also re-derived and unchanged:

```
$ python tools/mint_factory_census.py     -> 103 PRESENT / 0 ABSENT
$ python tools/cutover_manifest.py --gate -> OK (59 rows)
$ python tools/rt_mint_gen.py --check     -> STALE: 68 ENTERED, 0 LEFT
```

One figure has MOVED and Wave 28's close did not predict it:

```
STALE VERDICT rows printed by the census:  Wave 28 recorded 7, this tip prints 8
  the 8th is  ?StopEffect@Scene@Audio@@QEAAXI@Z
```

That is lane INSTR's, and it is a live demonstration of warning 1.

### 1.1 THE SCOPING CORRECTION THAT RESHAPED THIS WAVE

**The remaining BODY work in the entire runtime port is SEVENTEEN functions, not
24.** The census's own `GATED-BUT-WRITTEN` table (Wave 22 lane ALLOC) shows all
**7 of System's BODY rows are `MemoryAllocator` and are WRITTEN AND UNARMED**
behind `BANISHED_RT_ALLOC`:

```
$ python tools/runtime_census.py | grep -A22 'GATED-BUT-WRITTEN'
  System           19       7 |        7  ...
    BODY  ?Allocate@MemoryAllocator@System@@...   BANISHED_RT_ALLOC
    BODY  ?Free@  ?Get@  ?MemAlloc@  ?MemFree@  ?Shutdown@  ?Startup@
  => "Scoping a porting lane on these would scope work that is already written."
```

**I had a `SYSBODY` lane drafted and deleted it.** The census printed that warning
in its own output, and a lane following a brief built on "System 81.9%, 7 bodies,
second-lowest namespace" would have spent a wave re-porting the allocator. This is
warning 3 firing during kickoff authoring rather than during the wave.

**System's real question is not a port, it is whether `BANISHED_RT_ALLOC` can be
graduated, and that is a measurement and a risk decision, not a lane of bodies.**
It is NOT scoped this wave. `project_allocator_gate_open.md` records the gate as
armable; nobody has run the A/B that would graduate it. Recorded as a Wave 30
input so it stops being rediscovered.

### 1.2 THE 17 REAL BODIES, ITEMISED

Derived per namespace with `python tools/runtime_census.py --unported --namespace <NS>`.
**Work this list, not the count** (warning 2).

| ns | n | symbols |
|---|---|---|
| Graphics | 8 | `Scene::AddCulledDrawItems`, `Scene::AddGlobalDrawItems`, `Renderer::AddLight`, `Renderer::EndDrawGroup`, `SunLight::GetSunCamera` · `AnimationState::CopyInstancedToUnique`, `AnimationState::UpdateNodes`, `Animation::GetRootPoseAtTime` |
| IO | 4 | `Modification::SetupFileSystem` · `TextFileStream::GetObjectName` / `GetObjectType` / `GetObjectVersion` |
| Audio | 3 | `Scene::GetAttenuatedVolume`, `Scene::OnSoundEnded`, `Device::PlayPlatform` |
| Video | 2 | `ShaderProgram::GetConstantName`, `ShaderProgram::GetTextureName` |
| System | 0 | (7 gated-but-written, §1.1) |

The Graphics eight fall into **two families** — a Scene/Renderer draw-item family
and an AnimationState family — and **five of the eight are already CALLED from
`wave28_lane_partsys.cpp`**, i.e. last wave's lane reached them and did not port
them. That is a real, checkable input, not a hint.

### 1.3 THE VTABLE / SLOT SURFACE, RE-DERIVED

```
$ python tools/runtime_census.py    (VTABLE GAP, DECOMPOSED)
  38 VTABLE rows = 20 UNPROMOTABLE + 18 MINTED + 0 NOT MINTED
  => REAL remaining vtable work = 18, NOT 38
     (Video's 12 are ALL unpromotable; Object's 4 are ALL unpromotable)

$ python tools/mint_slot_sweep.py
  vtables 64 ; slots 589 ; ported 430 ; purecall 143 (legitimate)
  UNPORTED -> named trap  16   (16 exported-by-name, 0 address-only)
    0x1801550e0  4/10  ~??_7Particle@Graphics@@6B@
    0x180155040  4/10  ~??_7ParticleSystem@Graphics@@6B@
    0x180158e10  3/11  ~tail-not-a-pointer
    0x180154dc0  3/9   ~tail-not-a-pointer
    0x180154d70  1/9   ~tail-not-a-pointer
    0x180154950  1/2   ~??_7FileStream@IO@@6B@
```

**`~adjacent-export` is an UPPER BOUND on identity, read by three previous waves
as exact.** A row can swallow an unexported table. Three of the six above print
`~tail-not-a-pointer`, i.e. the tool declines to name them at all. **Naming them
is part of lane SLOTS' job and it must not accept the adjacent-export hint as an
identification.**

### 1.4 WHAT THE BOARD CANNOT SEE — carry this into every lane doc

- **The census is structurally blind to the entire 206-body factory family.** Zero
  rows match `CreateNewObject`. Nothing in our tree *resolves* those symbols, so
  they are outside the population. A flat board is not a shortfall.
- It **silently drops a macro door whose first argument is an inline
  function-pointer type** (proved by experiment: moving a typedef moved TOTAL by 1).
- It cannot see a registration reached by a plain C++ call, a non-literal
  `RegisterRuntimeExport` name, or several macro door spellings.
- `mint_stamper_census.py`'s `ours-capable` column **UNDERSTATES** while
  `unattributed` is non-zero: it attributes by `.pdata` and MSVC emits none for a
  frameless leaf. Measured on the full 63-row set: **45 of 47 unattributed sites
  resolved exactly and 24 of 47 were ours by NAME AND ADDRESS while the tool said
  not ours.** Do not read `ours-capable` as ownership.

Both census blind spots are **DEFLATING** (warning 8) and they sit in the
instrument the whole project scores itself with. That is lane INSTR.

### 1.5 THE VERIFICATION DEBT

**~280 bodies are registered and have never executed.** Exactly ONE body in all
of Wave 28 was exercised on a live route (`?Update@Scene@Audio@@`, 799 calls).
Two waves have now deferred this. ~206 of the 280 are factory slots whose only
evidence is FACTUI1's **offline** byte oracle, preserved deliberately outside
`tools/` and deliberately as `.txt` at
`docs/runtime_port/wave28_factui1_oracle.cpp.txt`. It proves our bytes match
retail's; it does not prove anything called them.

**Whether to promote that oracle into `tools/` is this wave's decision and it
belongs to lane VERIFY.**

---

## 2. LANES

Eight lanes. Each gets a private worktree and the private scratch directory named
in its brief.

| lane | subject | pairs with |
|---|---|---|
| **WALL** | the live cutover wall: EXEC-of-0 at `RtUi_SpriteSheet_Serialize+0x1b7`; attempt 32 | MINT |
| **MINT** | regenerate `rt_data_mint_tables.inc` (68 ENTERED), own measurement, own boot | WALL |
| **VERIFY** | the ~280-body verification debt; the FACTUI1 oracle promotion decision | INSTR |
| **INSTR** | the board-owning instrument lane: 2 deflating census blind spots, the stamper `unattributed` fold-in, 8 stale verdicts | VERIFY |
| **GFXBODY** | Graphics' 8 bodies, two families | SLOTS |
| **SLOTS** | the 16 unported named-trap slots across 6 tables, and NAMING the 3 unidentified tables | GFXBODY |
| **IOLANE** | IO's 4 bodies (`SetupFileSystem` 2616 B + 3 `TextFileStream` readers) + IO's 2 minted vtables | AVBODY |
| **AVBODY** | Audio's 3 + Video's 2 bodies; both Video rows return `String` BY VALUE | IOLANE |

### 2.1 PAIRING IS AN INSTRUCTION, NOT A COURTESY

Wave 28's single best methodological result: **15 instrument defects were found
across two lanes and ZERO were found by their own author.** SLOTS validated its
operand reader against the exact blind spot that had defeated GFXBODY's — which
had missed the no-displacement `MOV [RCX],EAX` form and reported the same
impossible hole in *both* Camera bodies, consistently and wrongly.

**Each lane must run its partner's instrument against its own subject, and report
what it found — including "nothing".** Hand your partner's screen a known
positive (warning 7). If your partner has not reported yet, say so and run the
previous wave's equivalent instead; do not skip it.

### 2.2 THE TWO WRITE-WIDTH TRAPS, for every body lane

Neither is visible in decompiled C. Both were found as a **written-byte set
difference over the instruction stream**, and both survive every semantic check:

- **`Camera::operator=` is NOT the copy constructor.** The copy ctor writes
  `+0x60` as a DWORD; `operator=` writes it as a single BYTE and resumes at
  `+0x64`. The obvious body — "the same 53-dword loop as the sibling" — writes
  three bytes retail leaves alone, into a live Camera, 3.3 million times a session.
- **`MeshInstance::AddDrawItems` touches 19 fields of `this`; the
  `EdgeMeshInstance` override touches exactly ONE.** Caught by comparing operand
  SETS by construction, not by vigilance.

**Every body lane: compare the written-operand SET against retail's, and derive
your extent rather than trusting `.pdata`** (warning 16).

Also standing, from this project's ledger:
- A by-value `String` return means a **hidden RDX return slot**. An `fn_*` typedef
  that omits it is a wild write that still returns the right answer. This is
  AVBODY's Video pair, directly.
- Ghidra's decompiled C **drops dereferences, prints MOVSXD as a zero extension,
  and drops whole arguments at a mistyped vtable call.** Read the disassembly.
- Ghidra MCP's `get_function_metrics` **ignores `address=`** and silently falls
  back to a NAME lookup.
- A dtor mangles as `??1`; `QEAA` is the NON-VIRTUAL spelling and the wrong one
  resolves NULL silently.
- `ResolveAddr(Module::Runtime, addr)` is NOT "the DLL" — it checks the port
  registry FIRST.
- `ResolveExport` FALLS THROUGH to the raw impl for an unported symbol, and a
  name the DLL does not contain returns NULL silently.

---

## 3. WHAT SUCCESS LOOKS LIKE, AND WHAT IT DOES NOT

**Do not optimise the percentage.** `ceiling = (called - unpromotable) / called`
has `called` in BOTH halves, so any lane that opens a door moves it: closing
seven trap slots once sent Video's ported% DOWN and its printed ceiling UP with
zero triage change. A faithful port that calls DLL internals by address GROWS
`called`. **For the second consecutive wave the board fell while the wave
succeeded.** Score your lane on the identity you name in your own doc.

**Every lane's doc must have a section titled "WHAT THIS DOES NOT CLAIM."**

Report, per lane: what landed; what did not and why (naming the obstacle class —
intrinsic vs missing-capability, per warning 3); every instrument defect you hit,
in the direction it errs; what you re-derived and what reproduced; your partner's
instrument result; and your own errors.

An expiry or holdback clause must name an **ADDRESSEE**, not just a condition —
"the next lane that counts slots on this row" satisfies a checklist and then waits
forever.

---

## 4. CARRIED-FORWARD INSTRUMENT DEFECT LEDGER

Wave 28's ten, plus Wave 27's twelve. Lane INSTR owns 1, 2, 9, 10 and the stamper
fold-in; §0.1 of this document addresses 3, 4, 5, 6; lane VERIFY owns 7.

1. `runtime_census.py` cannot see the factory family at all. **DEFLATING.**
2. `runtime_census.py` drops a macro door whose first arg is an inline
   function-pointer type. **DEFLATING.**
3. `tools/ghidra_mcp/.venv/` does not exist in a lane worktree. → §0.1
4. The session scratchpad is SHARED between sibling lanes. → §0.1
5. `/tmp` is shared, with a worse failure mode. → §0.1
6. A backgrounded `mingw32-make` wrote a zero-byte file and returned 0. → §0.1, §0.2
7. Every lane's `*Summary()` is written and nothing calls it. → VERIFY
8. `rt_deleting_dtor_verify.py`'s bare `else` reports UNRECOGNISED shapes as `BAD`.
9. `runtime_census.py --csv` takes a value, not a redirect.
10. Stale VERDICT rows the census prints and nobody retires — **now 8, not 7.**

**The clearing move for 4 and 5 is RE-DERIVATION, not inspection.** An artefact
that cannot be cheaply re-derived cannot be cleared of it at all.

---

## 5. WAVE 30 INPUTS, RECORDED NOW SO THEY ARE NOT REDISCOVERED

- **`BANISHED_RT_ALLOC` graduation** (§1.1): 7 written-and-unarmed bodies. Needs an
  A/B with more than one run per arm — one `sa-stress` run cannot resolve a
  5-point effect.
- `?SetupFileSystem@Modification@IO@@` if IOLANE declines it on size.
- `Stream::Run` (`??_7Stream@Audio@@6B@`'s single slot), 1800 bytes, declined by
  two lanes as a size judgement — stated as such, not dressed as a blocker.
- The 48 address-keyed UNKNOWN gap rows (36 `.text`, 8 `.rdata`, 4 `.data`). Not
  48 bodies, and the classifier refuses to guess.
- Object (83.9%) and Engine/Input/Application are AT CEILING. Video's 12 vtable
  rows are ALL unpromotable. **None of those is remaining work.**
