# WAVE 29 LANE WALL — the live cutover wall; attempt 32

Branch `wave29/wall`, cut from `wave29/base` (`c11511a1`).
Partner lane: **MINT**.

---

## 1. HEADLINE

**The wall moved, and it moved because a missing ADDRESS-DOOR REGISTRATION was
fixed — which is the one cause WAVE28_CLOSED §2 ruled out.**

| | attempt 31 (published) | attempt 32 (this lane) |
|---|---|---|
| faulting frame | `RtUi_SpriteSheet_Serialize+0x1b7` | `RtGraphics_MaterialInstance_Serialize+0x83` |
| fault | EXEC of 0 | EXEC of 0 |
| depth below `RunGameMain` | 4 frames | 10 frames |
| object being loaded | `Dialog/SpriteSheet.rsc` | the sheet's **MaterialInstance**, a NESTED load |
| `RtUnresolved` | 0 | 0 (and see §5 defect 1 — that number is blind) |
| boot phase reached | `input bindings + name tables`, t=535.00ms | same phase, t=588.77ms |

Both bodies this lane added are **proven to execute at cutover**: the wall could
not have advanced past `+0x1b7` unless `RtUi_SpriteSheet_SerializeEntries` ran,
and `RtObject_ExternalPtrSerialize_MaterialInstance` is a **named frame in
attempt 32's own stack** (frame #6).

Board, re-derived on this tip: `TOTAL 4632 1453 1296 9 89.2%` (from 1294 /
89.1%). `called` did not move. **Do not read that as the lane's result** — two
rows is not why this lane existed, and §3 of the kickoff says to score on the
identity I name. Mine is: *the boot got one whole object deeper into the
resource graph, and the fault class that stopped it is now named and
enumerated.*

---

## 2. THE FAULT, IDENTIFIED BY OFFSET AND NOT BY INFERENCE

`RtUi_SpriteSheet_Serialize` calls four helpers through lazily-resolved
pointers. Reading the source top-down fingers `StringSerialize()`, which is
fine. The offset settles it:

```
attempt 31 return address  = RtUi_SpriteSheet_Serialize + 0x1b7 = 0x1403d7caa
instruction at 0x1403d7ca8 = ff d0      CALL *%RAX
RAX loaded at 0x1403d7c8b  = CALL (anonymous namespace)::EntriesSerialize()
crash log                  = RAX=0000000000000000
```

`EntriesSerialize()` is `ResolveAddr(Module::Runtime, 0x180078ad0)`.

**FAULT CLASS: an ADDRESS DOOR onto an unported, unexported DLL internal.**
Not a wrong `ResolveExport` spelling, not a `??1`/`QEAA` dtor mangling, not a
pointer somebody failed to publish. `ResolveAddr` is registry-first; with the
DLL present it falls through to retail and works, and with the DLL absent
there is nothing to fall through to. This is
`reference_by_address_door_dies_at_cutover.md` exactly.

Four doors in that one body, and their states were **not uniform** — which is
why the offset mattered:

| address | what | state before this lane |
|---|---|---|
| `0x1800c9930` | `String::Serialize` | REGISTERED (`system/string.cpp:1386`) |
| `0x180078950` | `Array<SpriteInfo>::SetSize` | REGISTERED (`collection/array.cpp:2210`) |
| `0x180078ad0` | per-`SpriteInfo` record walker | **UNREGISTERED — attempt 31's wall** |
| `0x180014520` | `ExternalPtr<MaterialInstance>::Serialize` | **UNREGISTERED — 19 bytes later** |

---

## 3. THE UPSTREAM FIX — TWO PORTS, NO NULL CHECKS

Kickoff warning 4 is honoured: nothing was guarded. Both fixes supply the
missing body.

**(a) `0x180078ad0` ported** — `runtime/src/ui/spritesheet_vtable.cpp`,
`RtUi_SpriteSheet_SerializeEntries`. Extent **derived from the instruction
stream, not `.pdata`** (warning 16): `0x180078ad0..0x180078bd8`, 0x109 bytes,
ending at the `REPZ RET` at `0x180078bd7` followed by seven INT3 and an
unrelated body at `0x180078be0`.

**THE ONE THING THAT COULD HAVE BEEN SILENTLY WRONG.** Retail serializes the
five ints in the order `+0x10, +0x14, +0x1c, +0x20, +0x18` — the always-zero
`_reserved18` is emitted **FIFTH, not third**, because the record is walked in
`AddSpriteInfo`'s parameter order (x, y, width, height) with the reserved field
appended. A loop written in declaration order emits the same eleven values in
the wrong SEQUENCE. That is not a crash; it is a corrupt sprite atlas and every
subsequent record desynchronised, and nothing in this tree reports it. The
order was transcribed one LEA displacement at a time, and §4's negative control
then confirmed it independently.

Written-operand SET compared against retail by reading both disassemblies
(kickoff §2.2): strings `+0x00,+0x08`; ints `+0x10,+0x14,+0x1c,+0x20,+0x18`;
floats `+0x24,+0x28,+0x2c,+0x30`; stride `0x38`; `+0x34` never touched by
either side. **Identical set, identical order.**

**(b) `0x180014520` registered** —
`runtime/src/object/external_ptr_serialize.cpp`, a sixth `EXTPTR_THUNK`. This
is not a new body: that file already ports the one 748-byte
`ExternalPtr<T>::Serialize` parameterised by type id, and this is a two-line
instantiation. The type id `0x1801fa778` →
`?_typeInfo@MaterialInstance@Graphics@@` was derived **four independent ways**
that all agree: objdump of the RIP operand at `0x180014578`;
`tools/extptr_family_sweep.py`; `pefile` on the export directory (ordinal
4883); and `edgemesh_vtable.cpp:221`, written in Wave 27.

**I registered (b) at the same time as (a) deliberately.** Fixing only (a)
would have moved the wall NINETEEN BYTES, to `+0x1ca`, and bought a boot that
looked different and was not. That prediction was written into the source
comment **before** the run, and attempt 32's stack shows frame #7 at exactly
`RtUi_SpriteSheet_Serialize+0x1ca`.

### 3.1 THE ARM — §0.4's RECIPE IS RIGHT AND ITS ANSWER IS INCOMPLETE BY TWO

§0.4 says to recover the arm from the `[RtAlloc] ATTEST:` / `[RtDiff] ON`
lines, never from a log grep for `BANISHED_*` strings. That is correct and I
followed it. Its *summary* of the arm — `BANISHED_RT_NO_DLL` + `RT_ALLOC`,
`RT_DIFF` off — is **missing two variables**:

```
BANISHED_RT_NO_DLL=1   error.log:1   "[Standalone] BANISHED_RT_NO_DLL=1 -- continuing ... ABSENT."
BANISHED_RT_ALLOC=1    error.log:6   "[RtAlloc] ATTEST: ARMED (cutover, DLL absent) (exports=27)"
BANISHED_RT_MINT=1     debug.log:137 "[RtMint] ... registration is ON (BANISHED_RT_MINT=1)"   <-- MISSING
BANISHED_AUTOSTART=1   error.log:36  "[AutoStart] ENABLED (BANISHED_AUTOSTART=\"1\")"         <-- MISSING
BANISHED_RT_DIFF       unset; grep -c "RtDiff] ON" == 0
```

`BANISHED_RT_MINT=1` is the load-bearing one and `tools/cutover_attempt30.sh`'s
own header argues AGAINST it ("the blanket switch integrator-queue item C3 says
not to use at cutover"). That guidance is **stale**: the printed string is a
ternary over `EnvOn("BANISHED_RT_MINT")`
(`standalone/rt_data_mint.cpp:1093-1097`), so a log carrying the ON form cannot
have been produced without it, and `rt_data_mint.cpp:1062` shows that without
it `RegisterMintedData()` never runs at all — no minted `_typeInfo` row binds,
which the ported bodies read.

**These were not recovered from `docs/runtime_port/cutover31/`.** That directory
and its control contain ONE file each, `standalone_boot_state.txt`, reading
`STATE=BOOTING pid=33932 uptimeSec=0` — worthless. See §5 defect 2. The real
artefacts were still LIVE in `C:\GOG Games\Banished` from 13:48 and I copied
them out before a sibling lane overwrote them. I confirmed they are attempt
31's by symbolising the crash log against the exe sitting in the game dir: all
five frames resolve to exactly the five WAVE28_CLOSED §2 publishes.

**The arm is then confirmed by MEASUREMENT, not by reading.** See §4.

`tools/cutover_attempt32.sh` bakes this arm in (attempt 30's takes it as argv)
and writes an `attest.txt` — deliberately not `.log` — so the next wave
inherits the evidence rather than the reconstruction.

---

## 4. WHAT WAS MEASURED, AND THE CONTROLS

Four runs, all under `tools/game_dir_lock.sh` held by WALL, lock verified
literally (`grep -c DENIED` on the acquire output = 0, holder printed as WALL)
before anything was deployed.

**RUN 1 — CUTOVER CONTROL, and it reproduced.** The preserved attempt-31
binary (md5 `f4bee9b2343a79127ccd1b2704631572`) on the four-variable arm:

```
#1 reimpl-x64.exe + 0x3D7CAA   = RtUi_SpriteSheet_Serialize +0x1b7
RAX=0 RDX=0x225 RSI=0x25 RBX=8
```

identical offset, identical five frames and identical registers to the
published attempt 31. **An arm that reproduces the published result on the
published binary is the arm that produced it**, and this is what makes the
moved wall in run 2 a moved WALL rather than a moved TREE — §0.4's stated reason
for requiring a control. Evidence: `docs/runtime_port/cutover32_control/`.

**RUN 2 — ATTEMPT 32.** Same arm, this lane's build. Wall moved; stack in §1
and §6. Evidence: `docs/runtime_port/cutover32/`.

**RUN 3 — DLL-PRESENT WRITE-PATH ORACLE.** Found by grepping `standalone/`
before assuming no route existed (warning 13):
`standalone/spritesheet_serialize_probe.cpp` already replays the write path on
the **real boot-time sheet** (212+ sprites). It is a genuine oracle for the new
body and not a self-check, because retail's outer `Serialize` reaches
`0x180078ad0` by an in-DLL `rel32` CALL — not a door, so we do not intercept it
— while our side goes through the registry to mine.

```
[RtSaveTrace] pass UI::SpriteSheet::Serialize: 7151 emissions identical
[RtSaveTrace] pass UI::SpriteSheet::Serialize[swapped]: 7151 emissions identical
[RtSaveTrace] ==== 24 compared, 0 diverged ====
```

Both side orders. **7151, not zero** — the probe's own message warns that a pass
over zero emissions is not a verification, and that is the number it tells you
to read.

**RUN 4 — NEGATIVE CONTROL, because "identical" does not prove the oracle
reached MY body.** I permuted exactly the three ints whose ORDER §3 claims is
load-bearing, rebuilt, and re-ran:

```
[RtSaveTrace] DIVERGE UI::SpriteSheet::Serialize at emission 28: ours int(+0xa0)=1  theirs int(+0xa0)=48
[RtSaveTrace] DIVERGE UI::SpriteSheet::Serialize[swapped] ... (same)
[RtSaveTrace] ==== 24 compared, 2 diverged ====
```

This does three things at once: it proves the oracle **can** fail, it proves it
is **reaching this lane's body** on the DLL-present arm (nothing else changed),
and it **independently confirms the +0x18-is-fifth finding** — permuting those
exact three fields is what broke it. It was then reverted; `grep -c "NEGATIVE
CONTROL"` on the file = 0.

Both verdicts: `docs/runtime_port/cutover32/oracle_and_negative_control.txt`.

**RUNS 3 AND 4 WERE THEN BOTH REDONE**, because of §5 defect 4: the build is
not byte-reproducible. Every number quoted above is from the exact image I am
committing (md5 `934e50522a719a8e80c7ff2104561cba`).

---

## 5. INSTRUMENT DEFECTS — five, direction stated, deflating ones first

**1. `RtUnresolved == 0` CANNOT MEAN WHAT WAVE 28 READ IT TO MEAN. DEFLATING,
and it is in the reasoning the kickoff hands the next lane.**
WAVE28_CLOSED §2: *"`RtUnresolved` count for the whole run is ZERO — this is
not a resolve miss, so it is a null slot or a null stored pointer, **not a
missing registration**."* It WAS a missing registration. The counter is
structurally incapable of seeing one of this kind, and our own tree says so:

```
runtime/src/engine/mainloop_run.cpp:13
  "An unregistered address door prints no [RtUnresolved] line and no ..."
```

A NAME door that misses prints `[RtUnresolved]` and is counted; an ADDRESS door
that misses is a silent null. So the zero ruled out precisely the fault that
was present. This is
`reference_diagnostic_field_cannot_express_the_answer.md` — a field read for
something it cannot express — and it cost the class of hypothesis that turned
out to be right. Attempt 32's `RtUnresolved` is also 0, and also says nothing.

**2. `docs/runtime_port/cutover31/` IS EMPTY OF EVIDENCE. DEFLATING.**
It and `cutover31_control/` contain only `standalone_boot_state.txt`
(`STATE=BOOTING uptimeSec=0`). Cause: `.gitignore` carries a blanket `*.log`,
so `cutover_attempt30.sh` copies the three real logs into the outdir and git
silently drops all three. The kickoff §0.4 tells the lane to derive the arm
from those files; **the route it prescribes does not exist on this tip.** My
partner MINT hit this independently and concluded, correctly, that the logs
were not in the repo. Fixed forward: `cutover_attempt32.sh` writes
`attest.txt`, and I copied both crash logs to `crash.txt`.

**3. `tools/addr_door_audit.py` CANNOT SEE A DATA-DRIVEN DOOR AT ALL; "UNREGISTERED
40" IS A FLOOR, NOT A CENSUS, AND IT MISSED THE WALL I ACTUALLY HIT. DEFLATING.**

> **SHARPENED BY MINT AFTER THIS DOC WAS FIRST WRITTEN — see §7.1.** My original
> statement of this defect was "40 is a floor", i.e. *partial* blindness. MINT
> applied the tool's stated failure mode to its own subject and showed the
> blindness is **TOTAL over any door whose target comes from DATA rather than a
> call-site literal**: `rt_data_mint.cpp` resolves every slot through `OurPort(s)`
> driven by the `kMintVtables` table, so the audit would report **0 doors for a
> file that describes 1312 of them**. **The mint is the largest data-driven door
> table in the tree and the audit is blind to 100% of it.** If this lane's "40"
> is quoted anywhere, that is the caveat that must travel with it.
It harvests `ResolveAddr\(\s*Module::Runtime\s*,\s*(0x…)` literals. The next
wall, `0x180061100`, is reached as `RtAddr<SerExtPtrFn>(0x180061100ull)` — the
address is an ARGUMENT — so `grep -c 0x180061100` in its output returns **0**.
The tool prints "non-literal call sites 40 … invisible to both halves", which
is honest, but the headline number is what gets quoted. Fifth+ recurrence of
`reference_literal_grep_blind_to_variables.md`, and this time it hid the very
next failure.
*Effectiveness control run on it:* after my two registrations it moved 42 → 40
and BOTH subjects left the list (I diffed the SET, not the count —
`reference_diff_the_set_not_the_count.md`), so it is live, just narrow.

**4. THE BUILD IS NOT BYTE-REPRODUCIBLE, so md5 cannot certify "this is the
image I measured". INFLATING for me (extra runs); DEFLATING for anyone who
trusts it.**
Reverting my negative control restored the source exactly
(`grep -c "NEGATIVE CONTROL"` = 0) and the rebuilt exe had md5
`934e5052…` where the pre-control build was `f6125fe4…`. I had intended to use
hash identity to avoid re-running; I could not, and re-ran everything. **Anyone
pinning an A/B pair by md5 must not rebuild either side** — I have told MINT,
whose plan depended on it.

**5. The Bash tool BACKGROUNDS a foreground lock acquire at its 600s cap, and
the completion notice reads "exit code 0". INFLATING toward a FALSE GRANT —
the worst direction available, since it invites an unlocked run.**
§0.3 says never to pipe or background the acquire. I did neither; the harness
backgrounded it anyway. My command was compound (`acquire` then `echo
ACQUIRE_EXIT=$?`), which is the same status-destroying shape the "never pipe"
rule is about — **piping is one instance of it, not the rule.** I did not trust
the notification: I checked `grep -c DENIED` on the output file (0) and that
`status` named WALL as holder. **MINT hit this same defect independently in the
same wave and was shown a false 0 while its acquire had actually been DENIED.**
Two independent hits on one tool defect makes it a wave-level item, not a lane
anecdote. Mitigation: make the acquire the LAST thing in the command, and read
the tail for the literal word `DENIED`.

---

## 6. THE NEXT WALL, NAMED TO THE SAME PRECISION I WAS HANDED THIS ONE

```
  #0  0x0                                             <-- EXEC of 0
  #1  RtGraphics_MaterialInstance_Serialize      +0x83
  #2  RtObject_External_Load                     +0x592
  #3  RtObject_ExternalFactory_FindOrLoad        +0x2b1
  #4  (anon)::CallFindOrLoad                     +0x26
  #5  RtObject_ExternalPtr_Serialize             +0x25a
  #6  RtObject_ExternalPtrSerialize_MaterialInstance +0x7c
  #7  RtUi_SpriteSheet_Serialize                 +0x1ca
  #8  RtObject_External_Load                     +0x592
  #9  RtObject_ExternalFactory_FindOrLoad        +0x2b1
  #10 StandaloneEnsureSpriteSheetLoaded          +0x14b
  #11 Game::RunGameMain                          +0x97c
```

- **`RtUnresolved` = 0.** Per §5 defect 1, that is not evidence of anything.
- **Boot reached `input bindings + name tables` at t=588.77ms** (control:
  635.19ms; attempt 31: 535.00ms — same phase, run-to-run jitter).
- **The null, identified by offset**: return address `+0x83` =
  `0x14030dbc4`; the instruction at `0x14030dbc2` is `CALL *%RAX`; RAX came
  from `RtAddr<...>(0x180061100)` at `0x14030dbb1`.
  `runtime/src/graphics/wave24_lane_gfxmat.cpp:605`.
- **`0x180061100` is `ExternalPtr<Graphics::Material>::Serialize`** — the
  SEVENTH member of the same 25-instantiation family, `_typeInfo` at
  `0x1801fa740`. It is UNREGISTERED, and invisible to `addr_door_audit.py`.

**THE WALL IS NOW WALKING ONE FAMILY.** 6 of 25 registered, 19 not
(`tools/extptr_family_sweep.py` ∩ every `RegisterRuntimeAddr` literal; the
screen was handed the four Wave-10 rows as a known positive and passed):

```
  REGISTERED  0x1800042a0 StringTable@UI     0x180004590 SpriteSheet@UI
              0x180005dc0 Goal@Achievement   0x180014520 MaterialInstance@Graphics  <- this lane
              0x180038f90 ElementDescription@UI  0x180076610 Dialog@UI
  remaining 19, in load-graph order of interest:
    0x180061100 Material@Graphics        <-- THE NEXT WALL
    0x180014230 GraphicsMesh@Graphics    0x18005d0f0 ImageBuffer@Video
    0x1800542a0 VertexProgram@Video      0x180055380 PixelProgram@Video
    0x180055090 VertexLayout@Video       0x180055670 RenderState@Video
    0x180060930 VideoProgram@Video       0x18003f1e0 Font@UI
    0x180076e90 CharacterSet@UI          0x1800756d0 DialogControllerConfig@UI
    0x180051790 SoundEffect@Audio        0x180052a50 SoundBuffer@Audio
    0x1800594a0 AnimationEvent@Graphics  0x18005b840 AnimationGroup@Graphics
    0x180067120 Surface@Collision2D      0x18002f4c0 External@Object
    0x18003dc90 ExternalList@Object      0x180006d10 Achievement@1
```

**I DID NOT REGISTER THE OTHER 19, AND THAT IS A DELIBERATE SCOPE CALL, NOT AN
OVERSIGHT.** Wave 10 lane EXTPTR held 21 back with a *specific and checkable*
reason — "the oracle route does not cover them, and a defect in this body would
land on all of them at once". That reason is still true for 19 of them. It was
NOT true for `0x180014520`, which is why that one moved: it is on the boot
critical path and was the next wall by measurement.

**ADDRESSEE for the expiry of this holdback** (per §3: a clause naming a
condition but no addressee never fires): **the lane that runs cutover attempt
33.** Not "a future graphics lane". Registering `0x180061100` is a two-line
change of the identical shape, its type id is already printed by
`tools/extptr_family_sweep.py`, and its `_typeInfo` is minted, so it will
resolve with the DLL absent.

**OBSTACLE CLASS for each of the 19: MISSING-CAPABILITY, therefore EXPIRING.**
The blocker is "no oracle covers this instantiation", which is a claim about
the tree — the same claim that expired for `0x180004590` in Wave 26 and for
`0x180014520` today. It is NOT intrinsic. Whoever takes attempt 33 should
expect the wall to keep walking this family and should decide whether to
register them one-at-a-time (one boot per row, which is what four waves of
factory work cost) or as a block behind `BANISHED_RT_EXTPTR`.

---

## 7. PARTNER RESULT — lane MINT

**Pairing was run in both directions and it was not symmetric; the useful
findings went MINT → me and me → MINT roughly equally.**

**What I found by running MINT's instrument against my subject:**
`tools/mint_slot_sweep.py` reports **nothing at all** about my subject, and
that is the finding. It scores MINTED VTABLE SLOTS. `??_7SpriteSheet@UI@@6B@`
is 9/9 ported and therefore appears in the sweep's "60 of 64 COMPLETE" — a
green row — while one of those nine ported bodies called four unported
internals by address and took the boot down. `grep -c ResolveAddr
tools/mint_slot_sweep.py` = **0**: it has no concept of an address door.
**A COMPLETE vtable is not a bootable one, and no instrument in this wave
crosses that gap** — the sweep sees slots, `addr_door_audit.py` sees literal
address doors and misses the templated ones (§5 defect 3), and the census sees
neither. That is a real hole between three instruments, and it is where the
cutover wall has now lived twice.

**What MINT found that changed my work:** MINT independently confirmed my
`BANISHED_RT_MINT` correction from the SOURCE side rather than from my logs —
explicitly refusing to let two checks share one premise — and in doing so
established the consequence I had not: `rt_data_mint.cpp:1062` is the ONLY
publisher of the minted rows, and line 1068's polarity makes the promoted-row
set and the blanket switch **alternatives, not layers**. MINT also caught §0.4's
prescribed evidence route not existing, before I had finished proving the same
thing, and reported defect 5 above from the other side.

**What I gave MINT:** the two missing arm variables. On MINT's two-variable arm
`RegisterMintedData()` would never have run, so all 132 regenerated rows —
including its 68 — would have bound nothing, and its A/B would have reported "no
difference" about a subject that was never armed. That is a deflating null
result of the kind nothing detects. Also the non-reproducible-build finding,
which invalidates MINT's stated md5-pinned A/B plan if either side is rebuilt.

**Queueing:** MINT honoured §0.3 and queued behind me; the lock tool's own queue
showed "queued behind 'WALL'", so priority was enforced by the tool and not only
by agreement. I released the lock immediately after run 4 and told MINT.

### 7.1 MINT'S FINAL MEASURED RESULT — added after this doc was first written

Recorded here rather than left in a message, because three of its findings change
what this lane may claim.

**(a) MY ARM IS NOW CONFIRMED BY A THIRD RUN ON A DIFFERENT TREE.** MINT attested
all four variables plus `grep -c "RtDiff] ON" == 0` on `wave29/base`, which does
NOT contain my two registrations. §3.1's correction no longer rests on my
artefact alone.

**(b) MY WALL IS INDEPENDENT OF MINT'S REGENERATION — MEASURED, NOT GREPPED.**
MINT's A/B (pre-regen 64 minted vtables vs post-regen 132, 553 unported traps all
minted AND registered) produced the **identical** fault on both sides: same
registers, and frames `+0x3D7CAA / +0x36B463 / +0x36A2C4`. Critically MINT did
**not** compare raw offsets between two different images and call it a match — it
symbolised each offset **in its own image**, both resolving to
`RtUi_SpriteSheet_Serialize+0x1b7` → `External_Load+0x592` →
`FindOrLoad+0x2b1`. So arming 68 additional vtables at cutover introduced no
earlier fault and cleared nothing.

**(c) THE 19 UNREGISTERED ExternalPtr ROWS CANNOT COLLIDE WITH MINT'S 68 BY
CONSTRUCTION.** I had only checked one of them and flagged the rest as an open
question for MINT. MINT answered it for all 19 with a better argument than mine:
its 68 rows are `??_7` vtable rows in **`.rdata`** at
`0x18015xxxx–0x1801592c0`, while the ExternalPtr family is **`.text`** at
`0x18006xxxx`. Different sections — orthogonal by construction, not by
inspection. §6's holdback is therefore unaffected by `wave29/mint`.

**(d) MINT's discriminator warning, which I did not need but the next A/B will.**
Proving two exes differ by `strings | grep "??_7ComboDialog@UI@@6B@"` is INVALID
— the control returned 1, because the mangled name exists in the tree
independently of the mint. The valid discriminator is a symbol that can only
exist if the row exists: `nm | grep -c RtMintTrapStub_131_` gave 0 vs 7.

**(e) BUILD NON-REPRODUCIBILITY: two independent hits, and a sharper
consequence.** MINT hit §5 defect 4 separately (`0529bd9c` → `3ed328e6` from the
committed tree) and adds the part I missed: `game_dir_lock.sh assert-held <lane>
<exe>` compares md5, so it is sound for *"is the deployed file the file I just
built"* and **UNSOUND for *"is the deployed file built from my source"***.
Anyone reading that assert as a source-provenance check is wrong.

**What I gave MINT, restated:** the two missing arm variables — without which
`RegisterMintedData()` never runs and its entire A/B would have measured an
unarmed mint — and the non-reproducible-build finding, which invalidated its
stated md5-pinned plan if either side were rebuilt. MINT built each exe once and
deployed from scratch, and says the correction saved the only measurement its
lane exists to make.

**Both of us hit the Bash-600s false-grant defect (§5 defect 5) independently in
this wave.** That, plus the coordinator's separate report of lane GFXBODY hitting
it, makes it three lanes on one tool defect — a wave-level item.

---

## 8. MY OWN ERRORS, PLAINLY

1. **I was handed a stale worktree and nearly worked in it.** My worktree was
   parked on `1afa558e` with a Wave-11-era `CLAUDE.md`, not on `wave29/base`.
   Had I not checked `git log` first I would have branched Wave 29 work off a
   Wave 20 tree. Caught before any edit, but only because it was the first thing
   I ran.
2. **I wrote two wrong offsets into a source comment and had to correct them.**
   I first wrote that the next wall was "0x2d bytes" / "forty-five bytes" past
   the first. It is nineteen (`+0x1b7` vs `+0x1ca`). I caught it by actually
   subtracting rather than eyeballing the disassembly. The corrected figure is
   what the run then confirmed; had I not checked, a true result would have sat
   next to a false number and lent it credibility.
3. **I briefly concluded an existing annotation was WRONG when it was RIGHT.**
   Seeing retail emit `+0x1c` and `+0x20` before `+0x18`, I drafted a note
   saying this file's field list had the order wrong. It does not — the list
   ("name, group, x, y, width, height, reserved, u0…") is exactly right, and
   `SpriteSheet.h` explains why. I had pattern-matched onto
   `reference_pdata_understates_by_24x_and_annotations_lie.md` and nearly
   manufactured a correction. **A standing warning that annotations lie is
   itself a prior that can be wrong**, and I have recorded the annotation as
   HELD in the source so the next reader sees a checked claim rather than an
   unchecked suspicion.
4. **I used a compound command for the lock acquire**, destroying its exit
   status (§5 defect 5). The grant was real, but I proved that separately
   rather than by the exit code I had asked for.
5. **I planned to certify the final image by md5 and could not** (§5 defect 4),
   costing two extra runs of lock time that MINT was waiting on.

---

## 9. WHAT THIS DOES NOT CLAIM

- **It does not claim the cutover boots.** It does not. It dies one object
  deeper, at `RtGraphics_MaterialInstance_Serialize+0x83`. The measured result
  is a moved wall, not a working boot.
- **It does not claim the remaining distance is known.** The wall has moved
  onto the 7th of 25 instantiations of one family; whether the family is the
  whole remaining wall, or merely the next several rows of it, is **not
  measured**. Nineteen unregistered rows is an upper bound on that family only
  — it says nothing about what lies past `Material@Graphics`.
- **It does not claim the 19 held-back rows are safe to register.** I checked
  the boot path for exactly one of them. The Wave 10 concern — one body, 25
  callers, a defect lands on all at once — is untested for the other 19.
- **It does not claim `RtUnresolved == 0` is now meaningful.** I showed it is
  blind to address doors. I did NOT fix it, count the blind population, or
  audit what else has been concluded from a zero.
- **It does not claim `addr_door_audit.py`'s 40 is the remaining work.** It is
  a floor. The tool missed this lane's own next wall. I did not extend it.
- **The DLL-PRESENT oracle does not verify the cutover path.** It verifies the
  EMISSION SEQUENCE of `SerializeEntries` over 7151 emissions on the WRITE
  path with retail present. At cutover the read path runs, against minted
  `_typeInfo` data rather than retail's. Those are different conditions and
  only the first was compared against an oracle.
- **`0x180014520` is registered, not verified by an oracle of its own.** Its
  body was verified by Wave 10; this lane added a sixth type id and proved the
  thunk EXECUTES (it is a frame in attempt 32's stack). It did not compare that
  instantiation's output against retail's.
- **The board moving 1294 → 1296 is not this lane's result** and should not be
  quoted as progress on the runtime port. `called` did not move.
- **Registering `0x180014520` is a live behavioural change on the DLL-PRESENT
  path** for `edgemesh_vtable.cpp:254` and `wave27_lane_gfxpart.cpp:395`, which
  resolve that address and until now ran retail's copy. Stated, not discovered.
  `BANISHED_RT_EXTPTR=0` reverts all six together.
- **Three of the four runs used `BANISHED_AUTOSTART=1`**, because attempt 31
  did. No claim is made about a cutover boot without it.
- **§7.1(b)'s independence result is about `wave29/mint` ONLY.** MINT measured
  that its 68 regenerated vtable rows do not move this wall. That says nothing
  about the other six Wave 29 lanes, none of which I ran against. A merged tip
  has not been tested and this lane's attempt 32 is not a prediction for it.
- **§7.1(c) proves non-collision by SECTION, which is narrower than it sounds.**
  `.rdata` vtable rows cannot be the same addresses as `.text` bodies. That rules
  out an address collision; it does not rule out MINT's rows changing which
  bodies get REACHED, and neither of us tested that.
