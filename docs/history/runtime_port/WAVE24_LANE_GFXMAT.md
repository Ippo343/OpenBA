# WAVE 24 — LANE GFXMAT

Branch `wave24/gfxmat`, cut from `wave24/base` at `f5d70de2`. Private worktree.
Subject: the Graphics/Serializer cluster (kickoff §2) — seven named rows.

**Headline: six bodies ported, `??_7MaterialInstance@Graphics@@6B@` closed from
5/9 to 9/9 COMPLETE, and all three `?Serialize@` rows have a REAL differential
oracle that needs no game run — `tools/gfxmat24_probe/`, 5 of 6 controls CAUGHT
with the sixth abstaining exactly when it should.** The sixth control fired on
its first run, and chasing *why* instead of retuning it produced the lane's most
useful finding (§5.3): **retail's latched loop bound is load-bearing on every
load where the saved element count differs from the in-memory one.**

*(Rule: every census figure here is BRANCH-LOCAL, measured in this isolated
worktree. Wave 21 published a wave-level number built from seven such figures
and had to correct itself. The integrator must re-measure on the merged tip.)*

---

## 1. RE-DERIVATION — THE KICKOFF IS EXACT

```
cd <worktree> && <venv>/python.exe tools/runtime_census.py --unported
```

At `f5d70de2`, before a line was written:

```
Graphics            599      101 called   80 ported   0 detour   79.2%   ceiling 95.0%
TOTAL              4632     1319 called 1159 ported   8 detour   87.9%
=> 22 of 160 gap rows (14%) are a function to reimplement.
Graphics gap 21 |  BODY 6  CTOR 0  DTOR 0  OPNEWDEL 0  VTABLE 8  DATA 7  UNKNOWN 0
??_7MaterialInstance@Graphics@@6B@        5     9        (i.e. NOT complete)
```

**Every figure in kickoff §0 reproduces exactly.** Nothing in the kickoff was
stale for this lane's board. At the end of the lane, same command, same
worktree:

```
Graphics            599      101 called   82 ported   1 detour   81.2%   ceiling 95.0%
TOTAL              4632     1319 called 1161 ported   9 detour   88.0%
=> 20 of 158 gap rows (13%) are a function to reimplement.
Graphics gap 19 |  BODY 4  CTOR 0  DTOR 0  OPNEWDEL 0  VTABLE 8  DATA 7  UNKNOWN 0
??_7MaterialInstance@Graphics@@6B@        9     9  COMPLETE
```

**READ THE `detour` COLUMN CAREFULLY. Graphics 0 -> 1 IS NOT AN ARMED DETOUR.**
The census counts the *existence of an installer call site*. Mine
(`?Setup@MaterialInstance@`) is **opt-in and returns false** unless
`BANISHED_RT_MATSETUP_DETOUR=1`, so on every default run the DLL is untouched.
A reader who takes that `1` as "the 3D material bind now runs the port" would be
wrong; `RtGraphicsMatSetupDetourInstalled()` is the predicate that actually
answers it, and it is false today because nothing has ever armed it.
*(`reference_the_gate_was_never_a_control.md` — a gate nobody reads is not a
control.)*

`called` did not move: **this lane grew no census rows.** See §6.

---

## 2. THE INSTRUMENT THIS LANE BUILT FIRST — A rel32 SCANNER WITH A CONTROL

Four of the seven rows turn on "does a registration reach retail's traffic?", so
the first thing built was a scan of the whole `.text` for `E8`/`E9` rel32 whose
target is a listed VA. **A screen looking for an absence must be handed a
positive shape** (`reference_absence_screen_must_be_handed_a_shape.md`): the run
includes `?Serialize@ElementDescription@UI@@` (0x180116b00), which
`standalone/elementdesc_detour.cpp` documents at **21 internal callers**.

```
SUBJECT: Runtime-x64.dll .text, 1349120 bytes, E8/E9 rel32 into 12 listed VAs
  0x180116b00  call=21  jmp=0   CONTROL ElementDescription::Serialize (doc says 21)
  0x180102c90  call=2   jmp=0   Setup@MaterialInstance
  0x1801033e0  call=2   jmp=0   BuildParameters@MaterialInstance
  0x180103a70  call=0   jmp=0   Serialize@MaterialInstance (vtable slot 5)
  0x1800fc190  call=0   jmp=0   Serialize@AnimationManager
  0x1800ff680  call=0   jmp=0   Serialize@MeshInstance
  0x180002a50  call=0   jmp=0   Reconstruct@MaterialInstance
  0x180002ad0  call=0   jmp=0   Construct@MaterialInstance
  0x180002b50  call=0   jmp=0   Destruct@MaterialInstance
  0x180104bb0  call=0   jmp=0   Draw@Renderer
  0x1800fdb20  call=3   jmp=0   Update@AnimationState
  0x1800f4520  call=1   jmp=1   SetupShaders@VideoProgram
```

The control reproduces **exactly 21**, so the scanner is not a dead parser.
Consequences, each of which changed a decision:

* **six of my rows have ZERO rel32 callers** — for those a name registration
  *is* the whole door, and no detour is needed or possible.
* **`?Draw@Renderer@` has zero too.** Whenever it is ported, a registration
  suffices; it will not need the `?Setup@` treatment.
* `?Setup@` has exactly two (§3.4), and **`?Destruct@MaterialInstance@` is only
  EIGHT BYTES** (`48 8B 01 33 D2 48 FF 20`, then `CC` padding), so it could
  never be detoured — the smallest inline detour this tree installs is twelve.

Re-derive: `tools/gfxmat24_probe/` documents the method; the one-off script is
reproduced in the port file's header table.

---

## 3. THE ROWS

### 3.1 `??_7MaterialInstance@Graphics@@6B@` — 5/9 -> **9/9 COMPLETE**

Wave 23 GFXBODY §11.2 handed this back with the right measurement: four slot
bodies, three trivial and one not. All four are ported in
`runtime/src/graphics/wave24_lane_gfxmat.cpp`:

| slot | symbol | retail | shape |
|---|---|---|---|
| 5 | `?Serialize@MaterialInstance@…Stream@IO@@` | `0x180103a70` | the big one — §3.2 |
| 6 | `?Reconstruct@MaterialInstance@` | `0x180002a50` | slot-0 call, then Construct's stores |
| 7 | `?Destruct@MaterialInstance@` | `0x180002b50` | 8 bytes: tail-jump slot 0 with flag 0 |
| 8 | `?Construct@MaterialInstance@` | `0x180002ad0` | three vptr stamps + 14 zero stores |

Two transcription points the disassembly settles and a decompile would not:

* **`MOV word ptr [RCX+0x24],DX` is a TWO-byte store.** `Material.h` already
  splits `hasRenderTexture` / `hasRenderedTexture`; this body is the *writer*
  that proves the split, because a dword store would have clobbered `+0x26`.
* **Construct stamps THREE vtables** — `??_7Base@Object@`, `??_7External@Object@`
  then `??_7MaterialInstance@` — the first two being inlined base constructors.
  They are kept. A port writing only the final one is a different function under
  any interruption, and the byte oracle below would not have noticed, which is
  exactly when a "harmless" tidy-up survives.

All three stamped tables are **exported by name** (ordinals 1389 / 1432 / 1464,
checked by address against the export directory), so they are resolved with
`RT_DATA_SLOT` and are **not** address doors.

### 3.2 The three `?Serialize@` rows — PORTED, WITH THE OFFSET TRAP CONFIRMED

`?Serialize@MaterialInstance@` (0x180103a70), `?Serialize@MeshInstance@`
(0x1800ff680, 114 bytes) and `?Serialize@AnimationManager@` (0x1800fc190,
165 bytes).

**The Encoding offset trap was confirmed, not inherited.**
`reference_encoding_vtable_offsets_differ_between_builds.md` says retail and the
profile build disagree (`+0x78/98/a0` vs `+0x90/b0/b8`). Read out of
**Runtime-x64.dll (retail)**, the binary this build links against, at three
mutually corroborating call sites:

```
0x1800fc1ec   CALL qword ptr [RAX + 0xa0]     AnimationManager::Serialize
0x1800ff6b5   CALL qword ptr [RAX + 0xa0]     MeshInstance::Serialize
0x180103ae0   CALL qword ptr [RAX + 0xa0]     MaterialInstance::Serialize
```

A port written from the profile decompile would have used `+0xb8`, compiled,
linked, looked right and desynced every save. **Control 2 of the probe is that
exact mistake, weaponised** (§5.2), so the trap now has a standing detector
rather than a comment.

Two shapes recovered, which is the reusable part:

```
IO::Stream      +0x08 int32 mode (0 == READING)   +0x10 Encoding FACTORY*
IO::Serializer  +0x00 Stream*                     +0x08 Encoding*
```

and **an asymmetry that is retail's, deliberately preserved**:
`MaterialInstance` and `AnimationManager` MANUFACTURE their own Encoding from
the factory and destroy it; `MeshInstance` uses the Serializer's existing one at
`+0x08` and destroys nothing. A tidied port unifies these three and is wrong
three ways at once.

`MaterialInstance::Serialize` also settles a field: it serializes
`MaterialConstant +0x00` with the SAME helper (`0x1800c9930`) that serializes
`MaterialTexture::name`, so **`MaterialConstant::_head` is a `System::String`**.
`Material.h`'s "not read by lane F" comment is superseded, not wrong; the header
is left alone (§7 R2).

### 3.3 `?Update@AnimationState@Graphics@@` — NOT PORTED, SIZED

`0x1800fdb20 .. 0x1800fdf45` = **1062 bytes**, nine arguments, 3 rel32 callers.
Pulled into `called` by Wave 23 GFXBODY, which detours it away in its own probe
and therefore never entered it. It is a real animation evaluator (quaternion
interpolation over a node hierarchy), not a Serializer problem, and it shares
nothing with the rest of this cluster. **Sized out, not argued out.**

### 3.4 `?Setup@MaterialInstance@` — DETOUR WRITTEN, OPT-IN, AND THE INHERITED CLAIM CORRECTED

`standalone/matsetup_detour.cpp`. Kickoff: "registered rather than detoured stays
DEAD in play... retail's traffic to it is a rel32 call". **Re-checked per kickoff
§5, and it is HALF TRUE — the half matters.** The two rel32 callers are:

```
0x180105572   inside Graphics::Renderer::DrawItemList (0x180105240)
0x18011bb37   inside UI::SpriteContext::Flush         (0x18011ba40)
```

`UI::SpriteContext::Flush` **is ported** (`runtime/src/ui/spritecontext_flush.cpp`)
and its port reaches Setup **by name** (:267). So on any run where our Flush is
live, one of retail's two callers already runs the port and the row is NOT dead.
What *is* dead is `Renderer::DrawItemList` — the every-frame 3D material bind —
and only a detour reaches that.

The correct statement is **"live on the UI sprite path, dead on the 3D render
path"**. Writing it the loose way would have made an armed run look like it
proved something it had not.

The detour is **opt-in** (`BANISHED_RT_MATSETUP_DETOUR=1`) because
`runtime_detour.h` requires it — "nothing goes default-on unproven" — and this
lane has never executed a frame. Prologue is fifteen bytes at an exact
instruction boundary with no RIP-relative operand, read from the FILE image:

```
180102c90  48 89 5C 24 08 | 48 89 74 24 10 | 57 | 48 83 EC 20
```

Safety rule honoured: `InstallRuntimeDetourChecked` resolves through
`ResolveRuntimeRaw` (the raw image, never the port registry), and the
expected-prologue check refuses the patch if a different build, a wrong ASLR
delta, **or another detour already there** has moved those bytes. The port makes
exactly one call, to a *different* name (`?SetupShaders@VideoProgram@`), so
there is no recursion hazard and deliberately no trampoline accessor.

### 3.5 `?Draw@Renderer@` and `?BuildParameters@` — REFUSED, WITH WAVE 23's UNCONFIRMED SIZE NOW CONFIRMED

Wave 23 §9 flagged that its refusal of `?BuildParameters@` "rests partly on an
unconfirmed size". Re-measured here from Ghidra function extents:

| row | retail extent | bytes | instrs | calls | Wave 21 said |
|---|---|---|---|---|---|
| `?BuildParameters@MaterialInstance@` | `0x1801033e0..0x18010396d` | **1422** | 361 | 23 | 1422 (unconfirmed) — **CONFIRMED** |
| `?Draw@Renderer@` | `0x180104bb0..0x180104f48` | **921** | — | — | 921 (confirmed) — reconfirmed |

So the inherited size holds and the refusal stands on a measured number. Both
are refused **on size**, not on any oracle-shaped blocker. Note also that
`?Draw@Renderer@` has **zero rel32 callers** (§2), so whoever takes it needs a
registration and not a detour — one fewer unknown than Wave 23 assumed.

`?BuildParameters@` is reached from this lane's `MaterialInstance::Serialize`
**by name** through `RT_EXPORT_ONCE`. It is already in `called`, so that call
grows nothing and adds no address door; today it lands on the DLL's copy, and it
will land on the port the moment anyone writes one.

### 3.6 `?AddDrawItems@MeshInstance@` — NOT MY ROW, AND NOT TOUCHED

Confirmed still tagged `UNPROMOTABLE(HARMFUL)` and still read as an identity
token by `standalone/scene_watch.cpp`. Nothing here changes it.

---

## 4. THE PRE-PORT GREP (rule: grep for the port before writing it)

Denominator: **all 7 rows plus the 11 helper addresses**, greppped across
`runtime/ code/ standalone/ engine/ dx11/` — **scoped, never `grep -rn .`**
(kickoff §1a; my worktree is the 8th copy in the tree). Result: none of the six
bodies existed; `?Setup@` existed and is the reason §3.4 is a correction rather
than a port; none of the ten unexported helpers had a body or a registration.

---

## 5. THE ORACLE — `tools/gfxmat24_probe/`

**PROBE SHAPE (kickoff §1c): SYNTHESISED ORACLE.** Named before starting. Why
not the others: an *input sweep* needs purity and these bodies mutate a stream,
an Encoding and an object; a *shadow site* alone cannot replay a Serialize in
place, because the first arm consumes or emits the stream and the second sees a
different one; *write-path replay* is what this is, against a stream we build;
*two-sided disassembly* is the fallback and was not needed.

```
mingw32-make gfxmat24-sweep
```

It `LoadLibrary()`s the shipped DLL (a **read** of the game directory — nothing
written, no game launched, the same footprint `tools/gfxbody23_probe/` has, and
no lock allocation used), builds an `IO::Stream` / Encoding factory / Encoding
whose **vtables it owns**, overwrites the eleven helper callees with recorders
via a 13-byte absolute JMP **in the loaded image**, then runs the SHIPPED export
over one arena and OUR body over another and compares. Both arms reach the same
recorders, so the comparison is about the CALLER and nothing else.
**It compiles the SHIPPING body** (`wave24_lane_gfxmat.cpp` straight into the
probe), not a retyped copy — Wave 21 GFXBODY closed with "the binary measured in
§7 is not the one described in §4".

### 5.1 THE FULL MATRIX, EVERY CELL NAMED

```
=== NULL CASES (must all agree) ===
write path, count=3 inject=3                   agreed
read  path, count=3 inject=3                   agreed
write path, count=0 inject=0 (empty)           agreed
read  path, count=1 inject=1                   agreed

=== NEGATIVE CONTROLS, count==inject (control 6 must ABSTAIN) ===
corrupt 1  swap the two Array::SetSize helpers        DIVERGED
corrupt 2  int encoder at +0xb8 instead of +0xa0      DIVERGED
corrupt 3  texture stride 0x18 instead of 0x10        DIVERGED
corrupt 4  skip the BuildParameters call              DIVERGED
corrupt 5  pass 3 floats instead of 4                 DIVERGED
corrupt 6  re-read the loop bound (EFFECTIVENESS)     agreed     <- correct
=== THE REAL LOAD CASE, count != inject ===
corrupt 0, count=2 inject=3                           agreed
corrupt 6, count=2 inject=3                           DIVERGED   <- and correct
```

All three subjects AGREE uncorrupted, on both the read and the write path, at
three different element counts including empty. **5 of 6 controls CAUGHT; the
sixth abstains exactly where it should and fires exactly where it should.**

### 5.2 SCREENS ARE SCORED SEPARATELY, NEVER AS A UNION

Screen **(A)** is the call trace (pointers normalised to arena offsets, so the
two arms' different base addresses are not a divergence). Screen **(B)** is the
arena element payload.

**(B) is REDUNDANT BY CONSTRUCTION on this subject, and the probe MEASURES that
rather than assuming it.** Neither body writes the object directly on the
Serialize path — every write goes through a helper, and every helper is a
recorder — so (B) can only fire when (A) already has. Measured: (B) caught 2 of
the 5 firing controls (3 and 5); (A) caught 5 of 5.
`reference_union_control_hides_a_blind_screen.md` says never to report the union
of two screens as one number; the honest answer here was to keep both, print
both, and say that one is a strict subset — **not** to invent controls until
both screens fire.

(B) also *excludes the Array header*, because the two `_data` pointers hold each
arm's own arena address and **must** differ — comparing them would report a
divergence on a correct port
(`reference_a_control_can_corrupt_a_byte_that_should_not_be_compared.md` is the
mirror of this, and both directions have produced false verdicts here).

### 5.3 THE FINDING: CONTROL 6 FIRED, AND CHASING IT BEAT RETUNING IT

Control 6 was written as the **effectiveness control** — the one that must NOT
fire, because "a control set in which every control fires proves only that the
controls were chosen to fire". It fired on the first run.

The tempting move is to retune it until it abstains. The right move was to ask
why, and the answer is a fact about the subject:

> **On the READ path the element count comes FROM THE FILE, and `Array::_size`
> is still the OLD, in-memory one.** Retail latches the bound into `EDI` once
> (`MOV EDI,EAX` / `DEC RDI`). A port that re-read `_size` inside the loop would
> serialize **the wrong number of elements on every load where the saved count
> differs from the live one** — which is every load that isn't a no-op.

So retail's latch is **load-bearing**, not a compiler artefact, and the probe now
takes `--count` and `--inject` separately so both configurations run and both
are reported. That is the lane's most transferable result: it generalises to
every `Serialize` in the tree that latches a count.

### 5.4 AN INSTRUMENT BUG THE CONTROLS COULD NOT HAVE FOUND

The first version of `CompareTrace` used `memcmp` over the record struct. `int
ev` leaves **four bytes of padding** before the first `uint64_t`, which neither
arm writes — so two records that PRINTED IDENTICALLY compared unequal and the
probe reported a divergence on a correct port
(`reference_oracle_must_not_compare_unwritten_bytes.md`).

**No control could have caught it: every control would have scored CAUGHT either
way.** It was found because the *uncorrupted* run disagreed with the expected
answer — instrument rule 1 read in reverse. The comparison is now field by
field, and the reason is in the code.

### 5.5 THE IN-BODY ORACLES, AND THE LABELLING PASS lane DIFF ASKED FOR

`Construct` / `Reconstruct` / `Destruct` each carry a **real differential
oracle** inside the body, live under `BANISHED_RT_DIFF=1`: two scratch `0x60`
arenas, the SHIPPED export resolved through `RT_ORIGINAL_ONCE` (the raw
GetProcAddress door — never `ResolveExport`, which after registration hands back
ourselves and compares ours to ours), our body on the other, then a
recorded-tuple screen and a byte screen, with the vptr scored on its own third
screen. They are safe because these three bodies only store into `this` and
tail-call vtable slot 0, and slot 0 on both arenas is the same recorder.

Mid-lane, **lane DIFF's armed sweep** (99.7M calls, 630 sites) measured that
**at least 286 of 630 sites (45%) cannot catch a shared misunderstanding of
retail** — 115 SELF-CHECK, 38 ATTENDANCE, 133 NO-COMPARISON. Acting on it:

* every `RT_DIFF_BEGIN` name in this lane now carries its class —
  `…::Construct[ORACLE]`, `…::Serialize[ATTENDANCE]` — so the next sweep
  classifies them from the report without reading the source, and so an edit
  that guts an `[ORACLE]` leaves a name that visibly lies.
* the three `Serialize` sites claim **nothing but attendance**. A
  `RT_DIFF_SHADOW` there would have taken the port's own verdict and never
  consulted the DLL. Their comparison is §5, which the sweep cannot run.
* `IO::MemoryStream::MemoryStream` was NOT used as a ctor-oracle template (DIFF
  measured it permanently red and checking nothing); the `Construct` oracle
  scores the vptr on its own screen precisely so it cannot become that.
* `RT_DIFF_REPLAY_PRE/POST` has zero adopters tree-wide and is not used here.

DIFF's neighbouring specimen is carried into the port file so it is not
rediscovered: **`Graphics::MaterialInstance::RenderTextures` measured
worked=201,103 / calls=0 / divergences=0.** MaterialInstance objects are
abundant and live in a normal run — which is evidence these six rows will get
attendance — and **no existing MaterialInstance green may be read as coverage.**

---

## 6. DECLARED: THE CUTOVER SURFACE

**Census rows added: ZERO.** `called` is 1319 before and after; every export this
lane's bodies reach was already in `called`.

**Address doors added: TEN**, all unexported internals, all reached with
`ResolveAddr(Module::Runtime, …)`, each of which **dies at cutover unless
something registers that address**
(`reference_by_address_door_dies_at_cutover.md`). Listed once, in the port
file's header, so the cutover lane finds them with one grep:

```
0x180061100  0x180061880  0x1800616c0  0x1800c9930  0x18005d0f0     (MaterialInstance)
0x1800fe250  0x18005cc60  0x18005d4a0                               (MeshInstance)
0x18005c3d0  0x18005c660                                            (AnimationManager)
```

All ten were checked **absent from the export directory by address** — which is
NOT the same question as "unexported"
(`reference_grep_cannot_tell_internal_from_unported_export.md`), and the names
given beside them in the port file are this lane's reading of their call sites
and strides, not a symbol table.

`?BuildParameters@` is deliberately **not** in that list: it is an export,
already in `called`, reached by name, and grows nothing.

---

## 7. RESIDUALS, EACH WITH A NAMED ADDRESSEE

**R1 — `??_7MaterialInstance@Graphics@@6B@` IS NOW COMPLETE AND IS *NOT*
PROMOTED BY THIS LANE.**
COMPLETE is the STATIC half of readiness only
(`reference_a_green_readiness_screen_is_not_a_route.md`); it says the table is
safe to stamp, not that anything dispatches it. Promoting it is one line and is
the only Graphics vtable whose promotion changes a live dispatch (GFXVT's
finding, unchanged). It is left unpromoted because this lane never executed a
frame, and a promotion's *effect* is only visible in a run.
**SCOPE: DLL-present. ADDRESSEE: the first Wave 25 lane holding a game-directory
slot** — promote it, run one autostart boot, and read the
`Graphics::MaterialInstance::Construct[ORACLE]` line under `BANISHED_RT_DIFF=1`;
it is written so that a promotion shows up as a POINTER move on its own screen,
which is the expected result, not a regression.

**R2 — `runtime/types/graphics/Material.h`'s `MaterialConstant::_head` is a
`System::String`, and the header still says "not read by lane F".**
Proved by `?Serialize@MaterialInstance@` (§3.2). Not renamed here: the field is
included by three other files and renaming it is outside this lane's blast
radius.
**SCOPE: source hygiene, no behaviour. ADDRESSEE: the next lane that edits
`Material.h` for any reason** — rename it then, in one commit with its consumers.

**R3 — WAVE 23 GFXBODY's R4 IS ANSWERED, AND THE ANSWER IS NO.**
R4 asked whoever ported `?Serialize@AnimationManager@` to settle
`runtime/types/graphics/Animation.h +0x10`, on the guess that this body probably
touches it. **It does not** — the only AnimationManager fields it reads are
`+0x00` (`_data`) and `+0x08` (`_size`). `Animation.h +0x10` stays UNVERIFIED and
its note must NOT be deleted. Recorded so nobody re-opens it on the same guess.
**SCOPE: permanent. ADDRESSEE: none needed — this is a closure, not a task.**

**R4 — THE `?Setup@` DETOUR HAS NEVER RUN.** It is written, opt-in, and its
census `detour` count of 1 is an installer's existence, not an arming (§1).
**SCOPE: DLL-present only — at cutover there is no DLL to detour and the row
becomes an ordinary registration. ADDRESSEE: the first Wave 25 lane holding a
game-directory slot** — set `BANISHED_RT_MATSETUP_DETOUR=1`, confirm
`[MatSetupDetour] DETOUR INSTALLED` in `standalone_error.log`, and check
`RtGraphicsMatSetupDetourInstalled()` rather than the env var.

**R5 — `?Update@AnimationState@` (1062 bytes) IS AN ANIMATION LANE, NOT A
SERIALIZER LANE.** It sits in `called` only because Wave 23 GFXBODY's probe put
it there, and that probe DETOURS IT AWAY, so nothing in this project has ever
executed it.
**ADDRESSEE: the Wave 25 kickoff writer** — scope it with the remaining
Graphics BODY rows, not with this cluster.

**R6 — THE `tools/gfxmat24_probe/` SHAPE GENERALISES TO EVERY `Serialize` IN THE
TREE.** Synthetic Stream + owned Encoding vtables + recorder-patched helpers +
two arenas is subject-independent; only the arena builders and the event tags
are specific. Wave 23's R5 asked for `gfxbody23_probe` to be named in a kickoff
so ten lanes do not each conclude "no run, therefore STRUCTURAL"; this is a
second instance of the same shape and the same request.
**ADDRESSEE: the Wave 25 kickoff writer** — name both in the tools section.

---

## 8. WHAT THIS LANE DID *NOT* ESTABLISH — plainly

* **No game run, no frame, no attendance for any of the six bodies.** The
  differential oracles in §5 are synthetic. Nothing here has been observed
  running inside Banished, and the `[GFXMAT24]` summary line exists so the first
  run that *does* happen reports honestly rather than silently.
* **The three `Serialize` bodies were never run against a REAL `IO::Encoding`.**
  Every encoder and every helper in §5 is a recorder. So the probe proves the
  CALLER is right about what it asks for and in what order; it proves nothing
  about what the real Encoding then writes to a file, and it cannot detect a
  defect visible only when a real helper's side effects feed back — e.g. if a
  real `Array::SetSize` reallocated `_data` under the loop. (Retail reloads
  `_data` before the loop and not inside it, so it has the same exposure; that
  is a statement about the transcription, not a verification of it.)
* **No save file was written or read.** "Write-path replay" here means replay
  against a synthetic stream, not against a `.sav`.
* **`?BuildParameters@` and `?Draw@Renderer@` were not read.** Their sizes were
  measured (§3.5); their bodies were not decompiled by this lane, and the claim
  that `Draw` would grow the surface by two exported rows is still Wave 23's,
  taken from the census `called` column and not re-derived.
* **The ten helper addresses were not decompiled beyond what their call sites
  and strides say.** `0x18005c3d0` was read (it is a 383-byte `Array<T>::SetSize`
  with element stride 0x30 that calls the allocator); the other nine were not.
  Their names in the port file are inferences.
* **`??_7MaterialInstance@` was not promoted, so no dispatch changed.** COMPLETE
  is a static property (R1).
* **The `?Setup@` detour was never installed**, so the claim that it puts the
  port on `Renderer::DrawItemList`'s path is an argument from a measured call
  site, not an observation (R4).
* **Ghidra's function extents were used for the three sizes in §3.5.** They are
  contiguous ranges; `reference_pdata_is_a_lower_bound_on_function_extent.md`
  warns that chained fragments understate, and I did not union `.pdata` forward
  to check whether any of the three has a fragment Ghidra missed. For
  `BuildParameters` the extent AGREES with Wave 21's independently derived 1422,
  which is corroboration from a different method; for `Draw` it agrees with a
  figure Wave 21 had already confirmed against Ghidra, so that is the SAME method
  twice and is weaker (`reference_same_tool_both_sides_is_not_enough.md`).
  `?Update@AnimationState@`'s 1062 has no second source at all.
* **Instrument rule 1, honoured once explicitly:** while the rel32 scanner was
  agreeing with me about my six zero-caller rows, its control row was hand-checked
  against `elementdesc_detour.cpp`'s independently documented 21. That is the
  only place in this lane where a tool was checked *while it was agreeing*.
