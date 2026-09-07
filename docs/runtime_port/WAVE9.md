# Wave 9 — Cutover

Prep opened 2026-09-07 on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2
wave 9. Nothing is ported yet; this document exists to replace that row, which said
**"Remove `LoadLibrary` of Runtime; retire the §2.2 patches", size S**.

**That row is wrong, and it is wrong in a way that would have cost a session to discover.**
The measurements below are what the row should have said.

## The one success criterion

PLAN §5, the only unchecked box in the whole document:

> `reimpl-x64.exe` boots to gameplay with `Runtime-x64.dll` absent from the game directory.

Everything here is in service of that single line.

## What "S" missed

### 1. The gap is 319 items, not "a LoadLibrary call"

`tools/runtime_census.py`, 2026-09-07, after the three tool defects below were fixed
(`docs/runtime_port/census_20260907_prewave9.csv`, per-item list in
`docs/runtime_port/wave9_lanes.txt`):

**823 called, 504 ported (61.2%) — 319 unported.**

| Kind | Count | What it is |
|---|---|---|
| function | 145 | ordinary named exports |
| unnamed DLL internal, by address | 57 | `ResolveAddr(Module::Runtime, 0x1800xxxxx)` with no export name — the `FUN_` pool |
| ctor/dtor | 51 | `??0` / `??1`; these bind the class-atomicity rule below |
| `??_7` vtable | 28 | exported DATA |
| `?_singleton@…` | 17 | exported DATA |
| `?_typeInfo@…` (`BaseTypeInfo`) | 14 | exported DATA |
| ordinal | 4 | |
| DLL `.data` global, by address | 3 | `0x1801f…`, e.g. the `MainLoop` singleton region |

By namespace: internal 60, Video 59, UI 48, Graphics 28, System 23, Object 22, IO 17,
Achievement 16, Engine 14, Audio 8, Input 7, Math 5, ordinal 4, Application 3, Save 3,
Utility 1, Volume 1.

**59 of the 319 are DATA, not code.** A vtable, a singleton slot and a `BaseTypeInfo` cannot
be "ported" in the sense the per-function recipe means — there is no body to reimplement.
They have to be *minted*, with a layout that every ported and unported consumer agrees on.

### 2. Every verification mechanism this project has built dies at cutover

This is the part that makes Wave 9 unlike waves 1–8, and it is not a scheduling detail — it
decides the order of the work.

| Mechanism | What happens when the DLL is gone |
|---|---|
| `BANISHED_RT_DIFF` differential oracle | **Gone.** It compares our result against the DLL's. There is no second implementation to diff against. Every one of the 79.0M diffed calls that closed Wave 8 was borrowed from a DLL that will not be there. |
| `RtOriginal` / `RT_ORIGINAL_ONCE` | **Gone.** It is `GetProcAddress` against a module that is not loaded (`reference_rtoriginal_is_getprocaddress.md`). Every call site must be compiled out, not left to return null. |
| "Stamp the DLL's own exported `??_7`" | **Gone.** Waves 6 and 7 both avoided a detour because `??_7Geometry` and all 68 UI vtables are exported, so a ported ctor copies the DLL's vtable byte for byte. With no DLL there is no vtable to copy: every ported ctor must mint one whose slots ALL point at ported functions. |
| `BANISHED_RT_PORT=0` A/B | **Gone.** The kill-switch routes to the DLL. |
| The detour question | **Moot, and that is the good news.** "The DLL's own internal callers" (§4.1 decision 3) is only a problem while the DLL runs its own code. It never needed a detour in 8 waves; at cutover it stops being a category. The count stays 0 forever. |

**The consequence, stated plainly: nothing may be ported during Wave 9 that has not been
diff-verified before the DLL is removed.** Waves 1–8 could port and verify in the same
sitting. Wave 9 cannot. The verification budget has to be spent *first*, on the DLL that is
still there, and the removal is the last commit, not the first.

That inverts the per-function recipe (§6 step 6) for this wave only, and it is why the
holdbacks matter more than usual: **the 32 functions Wave 8 held back, the 5 handle-table
mutators, the 4 `ExternalFactory` mutators and the whole allocator all have to come back and
be verified against the DLL now**, while an oracle still exists.

### 3. The allocator NO-GO stops being optional

`runtime/src/system/allocator.cpp` + `allocator_win.cpp` are 27 registrations behind
`BANISHED_RT_ALLOC`, with a written **NO-GO** verdict ([WAVE2_ALLOCATOR.md](WAVE2_ALLOCATOR.md)).
Wave 8 found the NO-GO is really four functions wide, not the ctor/dtor pair PLAN predicted:
`Engine::Startup` writes three allocator fields directly and `Shutdown` calls the allocator's
shutdown.

With the DLL absent there is no other allocator. Every `MemAlloc`/`MemFree` in the process
becomes ours, including the ones inside ported bodies that currently allocate through the
DLL's heap. `?MemFree@…` and `?MemAlloc@…` are the two highest-count entries in the residue
census below, which is what you would expect and also what makes this the single riskiest
item in the wave: `str-4`/`str-8` header conventions, the boundary-tag layout, and the
half-written-pointer corruption family all live here.

**This is the one item that cannot be staged.** A heap cannot be half-ours.

### 4. `dx11/` is part of the cutover surface and was not being counted

`dx11/` is our VideoDX11 replacement and the existence proof that a DLL can be removed —
but it *imports from Runtime-x64.dll* exactly as the shipped plugin did, and 60 of those
imports were invisible to the census until 2026-09-07 (defect 1 below). Video's called count
goes 69 → 112 on that correction alone. Any cutover plan that starts from `code/` and
`runtime/` and forgets `dx11/` is short by ~50 functions.

## The tool this wave needed first: `BANISHED_RT_STRICT`

`tools/runtime_census.py` answers "how much is ported" **from source**. It cannot answer the
question the cutover gate actually asks, which is **"does anything still ASK the DLL for
something at runtime"** — because the source list is a superset that counts every resolve
site in the tree, including ones no reachable path executes.

`BANISHED_RT_STRICT=1` (`standalone/resolve_standalone.cpp`, commit `f95c1af`) records every
`ResolveExport` / `ResolveOrdinal` / `ResolveAddr(Module::Runtime)` that misses the port
registry and falls through to the shipped DLL, with the return address of whoever asked, and
prints one `[RtResidue]` line per distinct miss beside `[RtCensus]`.

It is **log-only by design**. It does not null the result or hand back a trap stub: 59 of the
misses are exported DATA, and poisoning those corrupts the process long before the call that
would name the culprit. Strict mode observes. Deleting the DLL is what simulates the DLL
being gone.

**An empty `[RtResidue]` list across the full harness matrix is the real cutover gate.**

Two runs so far, both kept in `docs/runtime_port/`:

| Run | How | Distinct requests still reaching the DLL |
|---|---|---|
| `wave9_residue_autostart.txt` | `BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1` | **260** |
| `wave9_residue_gameplay.txt` | `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120` | **309** |

Kinds, from the 309-entry gameplay run: 100+ ordinary functions, ~57 unnamed DLL internals by
address, ~39 ctors/dtors, 27 `?_typeInfo@…`, 19 `.data` globals by address, 11
`?_singleton@…`, 7 `??_7` vtables, 2 ordinals. 47 of them are reached only from `dx11/`.

Counts are per *resolve*, not per call — most are pointer-table fills at init, so `?MemFree@…`
showing 7 does not mean seven frees. The residue measures the **resolve surface**, which is
exactly what the cutover gate is about.

### A correction, recorded rather than quietly fixed

**This document first claimed the residue set was path-independent, and that was wrong.**

The evidence for it looked strong: an `AUTOSTART`+`AUTOQUIT` run and a `BANISHED_AUTOLOAD=Save9`
run produced *byte-identical* 260-entry lists, which was written up as `EngineFuncsInit` filling
the `fn_*` tables eagerly at boot.

The real explanation is duller and the lesson is sharper. **`BANISHED_AUTOQUIT=1` injects a Quit
click at the MAIN MENU** (`[AutoStart] menu settled — injecting Quit click`). Neither run ever
reached gameplay. They were byte-identical because **they were the same path**, and the boot half
of that path is genuinely deterministic. Two runs agreeing is evidence about the runs, not about
the program — and "different env vars" is not the same as "different path".

Re-run through the pause menu, which does reach gameplay, the residue is **309**: a strict
superset, **49 new entries and none lost**. The 49 are exactly what a menu-only run cannot
reach — `Achievement::ReadAchievements`, `AchievementManager::CreateInstance`,
`SaveLoad::Draw`, `Audio::Scene::StopAll`, `Matrix::Inverse`, `~UserInterface`, five Video
`*Data` dtors, and five more unnamed internals.

So the residue is **monotone in coverage, and lane 0's matrix is required after all** — which is
what the lane table said before this section was written. The cheap-lane-0 conclusion is
withdrawn.

**And the fact was already written down.** WAVE3.md's "Notes for later waves" says it plainly:

> **Check `calls=`, not `divergences=`.** This wave's first integration run reported 22
> registrations and zero divergences while most sites never executed — because
> `BANISHED_AUTOQUIT` drives menu→**Quit**, not menu→New Game.

Six waves later the same env var produced the same wrong conclusion in a different tool. That is
worth more than the correction itself: **this project's failures are not usually unknown facts,
they are known facts that did not reach the next wave.** It is the same shape as the two Wave 5
phantom manglings that Wave 8 had to fix again because Wave 5 diagnosed them and never fixed them
at source, and it is what PLAN §6 step 8 exists to prevent. The mitigation here is not another
note in a wave doc nobody opens — it is that `BANISHED_AUTOQUIT`'s behaviour is now in the
`BANISHED_RT_STRICT` memory entry and in lane 0's row of the table above, where someone reaching
for the tool will actually meet it.

**What survives, and is the useful half:** an entry, once present, never disappears on a longer
run. That makes the union across the matrix a sound work list, and makes any *shrink* between two
runs a tool bug rather than a fact — the shape of
`reference_truncated_tool_output_manufactures_calls_zero.md`, where a `tail -80` faked sixteen
sites disappearing.

**And it is still not a reachability proof.** An eagerly-resolved name appears whether or not
anything ever calls through the pointer — the opposite hat of
`reference_uncalled_port_is_not_verified.md`. Residue answers "what does the process ASK the DLL
for", which is the right question for *cutover* (an unresolvable import is fatal whether or not
it is called) and the wrong one for *verification*.

## What the residue census found on its first run

Comparing 260 runtime misses against the census's static list found **three defects in the
census, in both directions**. All three are fixed in `f95c1af`.

1. **Undercount, 51 functions.** `dx11/` resolves through its own
   `Dx11ResolveRuntimeImport(mangled, iat)` and passes the name from a table literal, not at
   the call site, so `RE_EXPORT` saw none of it. All 60 dx11-only names were then checked
   against the shipped export directory: **0 phantoms**.
2. **Overcount, 6 functions.** A `RegisterRuntimeExport` inside an `if (<flag>)` block that
   is off by default was counted as a live port; `GATED_OFF_FILES` only knew whole files.
   All six are WAVE3.md's NO-GO handle-table tier in `runtime/src/object/internal.cpp`, and
   the residue census caught four of them *arriving at the DLL* while the census called them
   ported. Object goes 28 → 22 — **which is what WAVE3.md said all along.** The wave doc was
   right and the tool was wrong, for a full wave.
3. **18 registrations passing a `const char* const kX = "?Mangled…"` name** are now resolved
   rather than warned about. Naming the mangling once, and using it at both the
   `RT_ORIGINAL_ONCE` site and the registration, is good practice — the oracle and the
   registration cannot then drift — so the census resolves it. UI 98 → 101.

Net: **the honest census is 823 / 504 / 61.2%**, not the 772 / 507 / 65.7% recorded at the
Wave 8 boundary. The denominator grew and the numerator shrank, and both moves were the tool
being wrong rather than the port changing.

This is the same lesson as `reference_uncalled_port_is_not_verified.md` and
`reference_census_cannot_see_nonliteral_registrations.md`, arriving for the third time:
**every progress number this project has published came from a tool that had not itself been
checked against the process.** The residue census is the first thing in the tree that asks
the process.

## A porting a function can GROW the cutover surface

Measured on the first holdback closed by porting. `KeyMap::RegisterInput` landed and the census
went **823 called / 504 ported → 825 / 505**: +1 ported, **+2 called**. Net coverage did not
move (61.2% both times).

The two new entries are the port's own dependencies. Retail's insert calls two unexported
`Collection::Set` internals — the node allocator `0x180023060` and the AVL rebalance
`0x180023170` — and a faithful port calls them the same way, by address, exactly as `ClearAll`
already did. Each `ResolveAddr(Module::Runtime, …)` is a new row in the **unnamed DLL internals**
bucket, which is the one bucket with no export name to work from (lane D, the `FUN_` pool).

**Consequences for lane sequencing:**
* **`called` is not a fixed denominator counting down to zero.** A port that reaches into DLL
  internals moves work from the named column into the hardest unnamed one. A wave that measured
  progress purely as "ported / called" could port for a day and show no movement while doing
  real work — or worse, look like it was going backwards.
* **Prefer ports whose callees are already ours**, and when a port must call an internal, treat
  that internal as a lane D row created by the port, not as a free dependency.
* The two rows this one created are shared `Collection::Set<E24>` internals, so they are paid for
  once and every other `KeyMap` mutator (`Load`, and Wave 3's tree mutators) reuses them. That is
  the good case; it is not automatic.

## Proposed lane order

The order is forced by §2 (verify while the oracle exists) and §3 (the heap cannot be half
ours), not chosen.

| Lane | Scope | Gate |
|---|---|---|
| **0. Residue matrix** | Run `BANISHED_RT_STRICT=1` across the whole harness matrix — menu, mods menu, all 4 tutorials, `Save9.sav` autoload, `sa-stress`, and gameplay through the pause menu. Union the residue lists; entries are monotone, so the union is sound and any *shrink* is a tool bug. **Use a route that reaches gameplay** — `BANISHED_AUTOQUIT=1` quits at the MAIN MENU and cost this document one wrong finding (see the correction above); menu-only 260 vs gameplay 309. **No attended run is needed for `Input::Device` after all** — see the CLOSED section below. | A single work list that is what the process reaches, not what the tree mentions |
| **S. The save-stream A/B oracle** — **BUILT AND PROVEN 2026-09-07**, `runtime/src/io/save_trace.{h,cpp}`, `BANISHED_RT_SAVE_TRACE=1` | Replays both bodies into two independent **synthetic** sinks and compares the **emission sequence** — not the bytes on disk, because a desync IS a different sequence of typed writes, and because a synthetic sink needs no `MemoryStream` and no codec. First subject: `Audio::Scene::Serialize`, **32,772 emissions identical, 0 diverged.** | An identical emission sequence |
| **A. Reclaim the holdbacks** | Port + diff-verify, against the still-present DLL, everything held back in waves 3–8. **Triaged below into a per-function disposition** — P (1, a probe), B (10, blocked on a named dependency), N (7, allocator-bound or oracle-hard), D/X (14, stubs or already-unblocked). Start with X: four `ExternalFactory` mutators whose stated blocker was a Wave 4 dependency that has since landed. | 0 divergences and non-zero `calls=` for the ports; a written reason for each stub |
| **B. The 145 named functions + 51 ctors/dtors** | The ordinary port wave, class-atomic (§4.1 rule 5). | 0 divergences |
| **C. Data: 28 vtables, 17 singletons, 14 `BaseTypeInfo`** | Mint them. Each vtable's slots must all point at ported functions, so this lane *follows* B per class and cannot lead it. | `static_assert` on every offset; a shadow oracle comparing our minted vtable against the DLL's exported one, byte for byte, while both exist |
| **D. The 57 unnamed internals** | **The known-hard one.** PLAN §4.3 risk 4 records the `FUN_` pool as MEASURED AND UNSOLVED and CLOSED TO AUTOMATION: all three levers (hash/fuzzy, string anchors, call-graph) run to a fixed point at 2% touched, 9 names proposed, none verified, and no PDB exists. 57 is a far smaller target than the pool, and each has a call site that says what it is used for — that is the lever, not the name. | Each one either ported, or proved unreachable by lane 0 |
| **E. Allocator** | Arm `BANISHED_RT_ALLOC`, revisit the NO-GO, and take the heap. Includes `Engine::Startup`'s three direct field writes and `Shutdown`. | `sa-stress` clean under `BANISHED_REDZONE=watch` |
| **F. Retire the §2.2 patches** | The four byte-patch files, the crash handler's Runtime instruction offsets (§2.2 C, kept and narrowed by Wave 3), the `MessageBoxW` IAT swap, and the direct layout reads in §2.2 B. | Each one deleted, not carried |
| **G. Remove `LoadLibrary`** | The actual cutover. Delete the DLL from the game directory and boot. | The PLAN §5 box |
| **H. Perf gate** | The deferred +7.7% breach. The user deferred it at the Wave 4 boundary explicitly *to this stage*. With the DLL gone the seam's `GetProcAddress`/registry indirection can be compiled out entirely, which is the main reason it was deferred here. Three runs minimum — `reference_one_perf_run_cannot_resolve_the_gate.md`. | ≤5% over the recorded baseline |

## Open items inherited from earlier waves — FOUR CLOSED, the rest triaged (2026-09-07)

### CLOSED — the 8 ports with no caller in our tree

`tools/runtime_census.py` has ended with this list since Wave 1, and
`reference_uncalled_port_is_not_verified.md` exists because a wrong
`Quaternion::operator*` hid in it for a whole wave. Each now has a verdict.

| Port | Verdict |
|---|---|
| `??DQuaternion@Math@@QEBA?AV01@AEBV01@@Z` | **Body exercised.** Three direct C++ calls from `Quaternion::Rotate` (x2) and `Transform::operator*` — only the export registration is idle. This is the one that was WRONG in z, and those callers are what caught it. |
| `??0Quaternion@Math@@QEAA@AEBVVector3@1@00@Z` | **Body exercised** from `RtMath_Transform_FromDirection` (`transform.cpp:770`). |
| `?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z` | **Body exercised** from `transform.cpp:798`; Wave 1 found a reassociated float sum in it through that path. |
| `?Serialize@Serializer@IO@@QEAAXHPEACPEBD@Z` (schar[]) | **Structurally verified, deliberately dead.** |
| `?Serialize@Serializer@IO@@QEAAXHPEADPEBD@Z` (char[]) | Ditto. |
| `?Serialize@Serializer@IO@@QEAAXHPEAFPEBD@Z` (short[]) | Ditto. |
| `?Normalize@Vector2@Math@@QEBA?AV12@AEAVScalar@2@@Z` | **NOW VERIFIED** — see the probe below. |
| `??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z` | **NOW VERIFIED** — see the disassembly comparison below. |

The three `Serializer` overloads are the char/schar/short array members of an 18-member
forwarder family. `RtIoSerializerRegister` verifies every one of the 18 **structurally at
registration**: it decodes the DLL's own 3-instruction tail-jump and asserts the ReaderWriter
vtable slot our port uses is the slot the DLL jumps to. The other 15 have a `ResolveExport`
site; these three do not, so they never run. Deleting 3 of 18 to satisfy a census line would
make the family asymmetric and buy nothing — they are kept, and the verification they DO have is
the structural check, not a `calls=` number.

**`standalone/rt_orphan_probe.cpp` (new)** is for the remainder: a port no harness path reaches,
no sibling calls, and no structural check covers. It calls our registered port and the shipped
DLL side by side over inputs chosen to cover every branch. It guards against the vacuous pass —
if `ResolveExport` and `ResolveExportRaw` return the same pointer then the port is not
registered and the probe would be comparing the DLL against itself, which it reports as a
failure rather than a pass.

`Math::Vector2::Normalize(Scalar&)`: **10 cases, 10 pass, 0 fail** — zero vector (the one
branch), unit axes, exact-length-5, denormals, overflow-to-infinity, mixed magnitudes and
`-0.0`. Output buffers are poisoned with `0xCD` on both sides first, so a field neither side
writes cannot read as agreement (`reference_oracle_must_not_compare_unwritten_bytes.md`).

`Graphics::EdgeMeshInstance::EdgeMeshInstance(const&)` cannot be probed: constructing a valid
source in isolation and destroying the copy would run `Geometry::~Geometry` over an attach array
shared with the source and clear live siblings' parent links. It is closed the other way, by
**static comparison against the retail disassembly** at `0x1800174f0`, which is five statements:

| Retail | Port |
|---|---|
| `CALL 0x180015b60` | `RtGeometryCopyCtor()(self, other)` |
| `LEA RAX,[0x180154d70]; MOV [RDI],RAX` | `emi->_base._vtable = RtEdgeMeshInstanceVtable()` |
| `MOV RAX,[RBX+0xc0]; MOV [RDI+0xc0],RAX` | `emi->_mesh = src->_mesh` |
| `MOV RAX,[RBX+0xc8]; MOV [RDI+0xc8],RAX` | `emi->_followed = src->_followed` |
| `MOV RAX,RDI; RET` | `return self` |

All three bindings confirmed against the shipped export directory with `pefile`:
`??0Geometry@Graphics@@QEAA@AEBV01@@Z` **is** `0x180015b60`,
`??_7EdgeMeshInstance@Graphics@@6B@` **is** `0x180154d70`, and `_mesh` / `_followed` are
`static_assert`-pinned at `+0xc0` / `+0xc8`, 8 bytes each. Instruction for instruction.

### CLOSED — `Input::Device`, and WAVE8.md's premise about it was half wrong

WAVE8.md said the whole of `Input::Device` "**needs an ATTENDED run with real keyboard and mouse
input** before it counts as exercised — the harness injects dialog clicks by calling handlers
directly and never passes through the input layer."

**The second half is true and the first half does not follow.** `GetValue`, `Pressed` and
`Released` never touch the hardware: each computes `DeviceArrayBase(this, device, deviceCtx) +
inputIdx` and returns one field of the `State` record it lands on. Lane D gave all three a FULL
REPLAY oracle for exactly that reason. What they need is not a human at a keyboard — it is a
**caller**. Whether a key was down changes the value both sides read; it does not change whether
they read the same address, and the address arithmetic is the entire port.

A probe also covers it *better* than a person could: an attended run reaches only the triples
the player happens to press, while `RtOrphanProbeInGame` sweeps every valid one.

**570 comparisons, 0 mismatches** — joystick 216 (4 contexts x 18 inputs x 3 functions),
keyboard 315 (105 x 3), mouse 39 (13 x 3), against the live `Input::Device::_singleton` on a
real gameplay frame. It runs once, from `Camera_UpdateMovement`, which is on the input path and
only ticks with a live world: the boot probe cannot reach the singleton (`Engine::Startup`
builds it later) and the exit dumps cannot either (`Engine::Shutdown` destroys it first).

Deliberately NOT probed, each for a stated reason: `Device::ResetState` **writes** (it memsets
the three State ranges) and its oracle is a self-check rather than a replay, so a probe adds
nothing and would zero live input mid-frame; `KeyMap::SetInput` / `ClearInput` mutate the
binding tree and would rewrite the player's key bindings. `KeyMap::GetDeviceForId` /
`GetInputForId` / `GetIdForDeviceInput` are const readers and are probeable in principle, but
only against a populated binding tree — **they are what actually remains of the attended-run
item**, and it is now three functions rather than "the whole of `Input::Device` and most of
`KeyMap`".

### CLOSED (as a decision) — the Wave 1 1-ULP residual

**The decision was already made and written**, in `runtime/funcs/rt_libm.h`: our own libm is the
default and `BANISHED_RT_LIBM=dll` is a diagnostic only, because "a port that cannot survive the
DLL's removal is not a port". Nothing about cutover changes that; the diagnostic simply stops
working, exactly as its own comment says.

What was genuinely open is the risk that same header flags: *"a last-bit difference in a routine
that feeds world generation is not automatically harmless, because generation is
seed-deterministic and small differences compound."* That had never been measured. It now has
been — three `sa-stress` runs at `--seed 12345 --frames 1200`, all on one build:

| Column | mingw #1 | DLL CRT | mingw #2 (control) |
|---|---|---|---|
| entities @600 | 13577 | 13577 | 13577 |
| entities @1200 | 13637 | 13706 | **13739** |
| citizens / homed / worked | 802 / 801 / 215 | 802 / 801 / 215 | 802 / 801 / 215 |
| age_mean, hunger_mean | 2.25, 5.5 | 2.25, 5.5 | 2.25, 5.5 |

**The control moves further than the change does.** Two runs of the *same* libm differ by 102
entities at frame 1200; our libm and the DLL's differ by 69. Every column that is deterministic
at all is identical across all three.

Stated at the strength the evidence supports: **the 1-ULP residual is below this harness's own
run-to-run noise floor** over 1200 frames and ~13,600 entities. That is a bound, not a proof of
bit-identity — the stress town steps in real time at 10x and is not frame-deterministic, so it
*cannot* resolve a 1-ULP effect. A bit-exact answer needs a deterministic world-gen fingerprint
that nothing in the tree currently emits; that is the follow-up if lane B ever wants one.

**The trap this nearly fell into is worth recording**, because the first attempt compared two
`standalone_debug.log`s and found exactly one differing line — the menu camera Transform — which
read like a clean libm signal. It is not: the control shows that same line differing between two
runs of the *same* libm. And those logs contained no world-generation output at all, so the
comparison was close to vacuous. Both errors are
`reference_one_perf_run_cannot_resolve_the_gate.md` in a new costume: check what your comparison
actually contains, and never attribute a difference without a control.

### CLOSED — `Input::KeyMap`'s three const readers

Closed the same way and in the same run as `Input::Device`, once the point was
generalised: they need a **populated** binding tree, and a live gameplay frame has one — the
player's real key configuration, reached through the same `g_pKeyMapObjectExref` triple-deref
`Camera_UpdateMovement` already uses. There was never anything attended about it.

Swept exhaustively, not sampled — `id` is narrowed to a `uint8_t` by the port and by retail, so
0..255 **is** the whole domain:

| Reader | Domain | Comparisons | Result |
|---|---|---|---|
| `GetDeviceForId` | id 0..255 x slot 0..1 | 512 | 0 mismatches |
| `GetInputForId` | id 0..255 x slot 0..1 | 512 | 0 mismatches |
| `GetIdForDeviceInput` | device 0..2 x input 0..104 | 315 | 0 mismatches |

The third matters most. It is the only member of the family that is **not** a lookup — it is an
in-order walk of the whole tree checking both slots of every binding, so it is the only place a
port could get `Leftmost`/`Successor` wrong, and a lookup-shaped test would never touch it.

**With this, WAVE8.md's attended-run item is closed in full**: 1,909 comparisons across
`Input::Device` and `Input::KeyMap`, 0 mismatches, no human at a keyboard.

### The 32 wave-3-to-8 holdbacks: triaged, with a cutover disposition each

These are the largest inherited item and they are **not paperwork** — most are real porting work
that needs a dependency ported first. What was missing was not effort, it was a *decision per
function*, so that lane A is a work list rather than a re-reading of five lane docs.

**The reframe that makes the list tractable:** at cutover a held-back function cannot "stay on
the DLL", because there is no DLL — but it does not automatically need a faithful port either.
It needs to **not be a missing symbol**. So every row below resolves to one of *port*, *probe*,
or *stub with a written reason* — and the three dead `IO::Serializer` overloads above are the
precedent for the third being a legitimate answer.

| # | Holdback | Disposition |
|---|---|---|
| **P — dissolvable by probe: DONE 2026-09-07** | | |
| 1 | `UpdateLoop::IsRunning` (lane A) | **CLOSED — 5 cases, 0 mismatches, true branch REACHED.** Retail is two instructions: `MOV RAX,[0x1801fe750]; CMP RCX,[RAX+0x20]` — `this` is COMPARED, never dereferenced, which is what makes the false branch safe to probe with pointers that are not `UpdateLoop`s. Cases: the running loop (true), **`_nextLoop` (+0x28) — the near miss, because a port comparing the pending loop instead of the current one would pass a one-sided test**, null, an unrelated stack address, and the `MainLoop` itself. The probe prints whether the true branch was reached, so five agreeing falses cannot read as a pass. |
| **B — blocked on a named dependency; ordinary port work once it lands** (**1 of the 10 PORTED AND VERIFIED 2026-09-07**) | | |
| 2-4 | `Audio::Scene::Update`, `Audio::Device::Update`, `FreeChannelPlatform` | **PORT after `Audio::SoundBuffer`/`SoundEffect`.** All three reach `Device`'s COM channel objects; none of the three classes was in Wave 8's scope. `Scene::Update` also consumes `System::Random`, so it needs an oracle design, not just a body. |
| 5-7 | `Audio::Scene::StopAll`, `Pause`, `StopEffect3D` | **PORT with 2-4.** Each calls into `Device::Stop` / the platform channel array. Porting the outer loop alone would put two implementations of one state machine in play (WAVE8.md §1). |
| 8 | `Audio::Scene::Serialize` `0x1800e0050` | **PORTED AND VERIFIED 2026-09-07 — 32,772 emissions identical, 0 diverged**, by lane S. The stated blocker was answered by a fact the tree had since Wave 4 (retail's Encoding offsets, bool `+0x78` / uint `+0x98` / int `+0xa0`, in `rt_io_port.cpp`'s warning block). 40 instructions touching one Encoding slot (`kScalarInt +0xa0`) and one ReaderWriter slot (`kArrayBool +0x20`, n=5 for `Scene::paused`). Its two "unexported callees" are ordinary `Collection` internals called by address as `ClearAll` and `RegisterInput` already do. |
| 9 | `Engine::MainLoop::Run` (lane B) | **PORT after the allocator (row 12-15).** Lane B records the allocator embedding as the reason it is a holdback. |
| 10 | `Input::KeyMap::RegisterInput` (lane D) | **PORTED AND VERIFIED 2026-09-07 — 50 calls, 0 divergences.** Lane D held it back because it could not pin `FUN_180023060`'s first argument. Reading the callee answers it: **RCX is never read** — `0x180023097 MOV RCX,RBX` destroys it before any use. The call sites agree in the stronger form: RCX is `this` at one site and a zero-extended id byte at the other two, and **a value that differs like that across call sites cannot be a parameter the callee reads.** Oracle: **our writer, their readers** — there is no replay (a second insert of the same id returns early, making replay vacuous), so the insert is verified by asking the DLL's own `GetDeviceForId`/`GetInputForId`/`GetIdForDeviceInput` to read it back, the last of which walks the WHOLE tree and so exercises our linkage and the rebalance through code we did not write. |
| 11 | `Input::KeyMap::Load` (lane D) | **STILL BLOCKED — and this row's own first draft was wrong to bundle it with 10.** `Load` does not touch `FUN_180023060` at all, so the resolved ABI does nothing for it. It is blocked on three separate things: an unexported registry-string parse helper (`FUN_18007f034`), a `System::String::Format` allocation pair per binding, and a lookup into a **second, differently-typed** `Collection::Set<Pair<String,String>>` inside `IO::Persistence`. Lane D called it the densest of its 21 in every dimension, and it still is. |
| **N — NO-GO until the allocator is ours; then mandatory** | | |
| 12-15 | `Engine::Engine` ctor, `~Engine`, `Engine::Startup`, `Engine::Shutdown` (lane B) | **PORT AS PART OF LANE E.** `Startup` writes three allocator fields directly and `Shutdown` calls the allocator's shutdown, which is why lane B found the NO-GO is four functions wide rather than the ctor/dtor pair PLAN predicted. These cannot be done before the heap and must not be done after it. |
| 16 | `Input::Device::Update` (lane D) | **NO ORACLE BY CONSTRUCTION, and it is not optional.** Four unexported, unnamed internal helpers plus DirectInput8 COM pointers past `Device+0x11D0` (unmapped). Replaying it drains the input queue twice, so "call ours, call theirs, compare" corrupts the state being compared. A wrong port is a game that silently stops responding to keyboard and mouse. **This is the hardest single item in the wave** and deserves its own lane with a bespoke oracle (record-and-replay of the queue, not call-twice). |
| 17 | `Input::Device::EnableMouseClipping` (lane D) | **PORT with 16** — same object, same unmapped region. |
| 18 | `Engine::UpdateLoop::LoadResources` (lane A) | **PORT after the IO holdbacks.** |
| **D — dead by construction; stub with a reason, do not port** | | |
| 19-20 | `MainLoop::OnVideoError`, `OnVideoRestart` (lane B) | **STUB.** Wave 8 measured exactly ONE internal caller each — the static thunks the `MainLoop` ctor installs into `Video::Device::_singleton` +0x50/+0x58, reached only on device lost/restore. This is the detour case Wave 8 found and declined. At cutover they need to exist, not to be faithful; a port would be dead code reporting `calls=0`. **If a lane does port them, the device-lost path has to be forced, or the port is unverifiable.** |
| 21-24 | `Save::~SaveLoad`, `Save::Draw`, and lane E's shadow markers (`Application::PlatformUpdate`, `SleepThread`, `YieldToThreads`) | **STUB or PORT-WITHOUT-ORACLE, decided per function.** Lane E states these have no oracle by construction. |
| **X — already resolved by earlier waves, listed for completeness** | | |
| 25 | `ExternalFactory::SerializeReferenceTable` `0x1800bcb60` (Wave 3) | **UNBLOCKED, but NOT cheap — this row's own "cheapest row in the table, do it first" was wrong and is withdrawn.** Reading it: ~200 instructions that tear down and rebuild a `Collection::Set`, do reference-counted `System::String` traffic through the allocator (the `INC`/`DEC [RAX-4]` header pattern, `SUB RCX,8` then free), run an in-order tree walk with an inlined string compare, **and dispatch through the `IO::Encoding` vtable at +0x18/+0x20/+0x98/+0xa0 on the SAVE PATH** — the same hazard filed under row 8, which the first draft of this table put in a different category. A silent save desync is this project's most-cited failure mode; this wants a lane, not a slot. |
| 26 | `External::SaveText` `0x1800bb9d0` (Wave 3) | **STILL BLOCKED, on a class no wave has touched.** It drives the `+0x28 Serialize(IO::Stream&)` slot with an **`IO::TextFileStream`**, and WAVE4.md is explicit that `TextFileStream` (with `FileStream`, `BinaryFileStream`, `VisitorStream` and the codec) was NOT in Wave 4. Needs `TextFileStream` first. |
| 27-28 | `ExternalFactory::FindOrLoad` `0x1800bf320`, `Clean` `0x1800be310` (Wave 3) | **NO ORACLE, and it was never a Wave 4 dependency.** Both rewrite the shared resource AVL tree, so running ours and theirs mutates the one tree twice, and a private replica is exactly what a stateful port must not keep. Same class of problem as `Input::Device::Update` (row 16): they need a bespoke oracle, not a later wave. |
| 29-32 | 5 handle-table mutators + `CheckPtr` (Wave 3, opt-in NO-GO) | **ARM AND MEASURE.** These are the `if (table)` / `if (shadow)` / `if (checkPtr)` blocks the census was wrongly counting as ported (see above). `CheckPtr` is a bare `RET` in every shipped build, so registering a real one silently re-enables ~60k indirect calls/frame — that is a perf decision, not a correctness one. |

**What this triage changes.** Two of the four groups are not porting work at all: **P** is one
probe on machinery that now exists, and **D** is six functions that need a stub and a sentence
rather than a faithful body. That leaves **B**, **N** and **X**, which are genuine
porting work with named prerequisites, and inside them **`Input::Device::Update` needs a bespoke
oracle rather than more time.**

**A correction to this table's own first draft, which is the reason to write it out per
function.** It began by asserting that all four `ExternalFactory`/`External` mutators were
"held back on a Wave 4 dependency that has since landed", by analogy with
`project_jack_of_all_trades_closed.md` — where both listed blockers turned out to be already
fixed and nobody had re-run the census. Checking each one instead of the group shows a **1/1/2
split**: only `SerializeReferenceTable` is actually unblocked; `SaveText` needs
`IO::TextFileStream`, which WAVE4.md says in as many words it did NOT port; and `FindOrLoad` and
`Clean` were never blocked on Wave 4 at all — they have **no possible oracle**, because both
rewrite the one shared AVL tree. A group summary ("held back for no oracle / Wave 4") that was
accurate about the group was wrong about three of its four members, and only a per-function pass
catches that. Which is the argument for this table.

None of these can be closed by writing a body in this session without an oracle, and doing so
would be the exact failure `reference_uncalled_port_is_not_verified.md` warns about — a green
`calls=0` line over unverified code. They are lane work with a decided disposition, which is the
honest end state for them before the wave opens.

### Lane A, opened: the remaining holdbacks are not 28 independent problems

Opening lane A on its two most tractable rows produced a better result than either port would
have: **the critical path is one missing tool, shared by four rows.**

`Audio::Scene::Serialize` turned out to be 40 instructions whose stated blocker was already
answered (above), and whose two unexported callees are ordinary `Collection` internals a faithful
port may call by address exactly as `ClearAll` and `RegisterInput` already do —
`0x180051390` is `Collection::Array<Emitter>::Resize` (0x40-byte elements, 3x growth heuristic,
allocator-locked free of the old buffer) and `0x180052640` is the bulk element serialize. Nothing
there is a wall.

**The wall is the oracle, and it is the same wall four times over.** Everything on the save path
shares one property: *replay is impossible*. Calling ours and then theirs either consumes the
stream twice or writes it twice, and — the specific trap — a second call often **succeeds
differently and silently**, which makes the oracle vacuous while it reports green. That is the
`RegisterInput` lesson (a second insert of the same id returns early) applied to a whole family.

Rows that are blocked on exactly this and nothing else:

| Row | Function | Why replay fails |
|---|---|---|
| 8 | `Audio::Scene::Serialize` `0x1800e0050` | consumes/emits the stream |
| 25 | `ExternalFactory::SerializeReferenceTable` `0x1800bcb60` | same, plus it rebuilds a `Set` |
| 26 | `External::SaveText` `0x1800bb9d0` | same, via `TextFileStream` |
| (Wave 3, opt-in tier) | `InternalBasePtr::Serialize` | WAVE3.md: "has **no oracle at all** and lands in the save/load path" |

**So lane A's first deliverable should be a tool, not a port: a save-stream A/B harness.** Write
the same object through *two* independent `IO::MemoryStream`s — one driven by our port, one by
the shipped body — and `memcmp` the bytes. The pieces already exist: `??0MemoryStream@IO@@QEAA@XZ`
(`0x1800ae710`) is already constructed by the Wave 4 `CompressedMemoryStream` port, and Wave 4
landed 22 `Serializer` functions with 0 divergences, so the `Serializer`/`Encoding` pair over a
scratch stream is assemblable from parts that are already ours.

That harness converts four "no oracle possible" rows into ordinary ports, and it would
retroactively strengthen Wave 4's own `Serializer` family, which today is verified by call-level
diffing rather than by byte-comparing what it actually wrote.

#### And "no oracle possible" is WRONG for the write path — which is the half that desyncs

Wave 3 wrote off `InternalBasePtr::Serialize` as having "no oracle at all", Wave 5 and Wave 8
used the same reasoning, and this document repeated it above. It is only true of the READ path.

**On the WRITE path a `Serialize` is a pure read of the object.** It walks the object and emits
bytes; it does not mutate what it is serializing. So calling ours and then theirs is a legitimate
replay **provided the two calls emit into different sinks** — the thing that made replay
impossible was never the function, it was that both calls shared one stream.

That gives lane S a much cheaper shape than a full `MemoryStream` pair. The comparison that
actually matters is not the bytes on disk but **the sequence of typed emissions**, because a
desync IS "a different sequence of typed writes". So the sink can be a synthetic double: a stub
`Stream` whose `+0x08` says writing and whose `+0x10` points at a stub `Encoding` whose vtable
slots record `(slot, size, value)` into a vector instead of touching a buffer. Run the port
against one recorder and the shipped body against another, then compare the two traces.

Everything needed to shape those doubles is already measured: `Serializer+0x00` is the `Stream*`,
`Stream+0x08` is the mode (non-zero = writing), `Stream+0x10` is the `Encoding*`, the
`+0x18`/`+0x20` pair returns the field object, and retail's field slots are bool `+0x78` /
uint `+0x98` / int `+0xa0`.

**The read path stays hard** — it consumes the stream and mutates the object — but it is also the
half that a write-path oracle indirectly protects: if our writer emits the same sequence retail's
does, a retail reader will accept our save, which is the property that actually matters.
**This is the single highest-leverage item left in the wave**, and it is why the remaining
holdbacks should not be attacked in the order they appear in the table.

### Lane S, built — and the harness found its own bug the way it was designed to

`runtime/src/io/save_trace.{h,cpp}`, gated on `BANISHED_RT_SAVE_TRACE=1`.

Two design choices are worth carrying to the remaining three rows:

**1. Compare the emission SEQUENCE, not the buffer.** A desync is precisely "a different sequence
of typed writes", so the sink does not need a real `MemoryStream` or a codec at all — a stub
`Stream` whose direction says writing, pointing at a stub `Encoding` whose factory returns a stub
`ReaderWriter` that records `(slot, args, value)`. That collapsed the estimated work from "build a
MemoryStream pair" to one file, and it makes divergences *legible*: the report names the emission
index, the slot, and both values, instead of "the buffers differ at byte 4118".

**2. Bind every slot, not the known ones.** The harness's first run crashed `EXEC-of-0` on the
stub `Encoding` vtable. Retail reaches an `Encoding` slot that `Scene::Serialize`'s own
disassembly never names, because the bulk element pass (`0x180052640`) derives its **own**
reader/writer from the `Stream` rather than reusing the caller's. A null slot faults and tells you
nothing; a bound one records and names itself. **The unknown slots are exactly the ones worth
binding** — that is the whole reason the harness could diagnose itself in one run instead of
needing a debugger.

The first subject came out clean at **32,772 emissions identical** — the full 4,096-entry effect
array plus the header and the five paused flags — and a separate gameplay run showed no whole-tree
regression (251 sites, 28,412,621 calls, 99 divergences, all in the two known Wave 1 sin/cos
sites).

#### CORRECTION: lane S unblocks ONE of the four rows, not four

This document claimed lane S "unblocks four rows at once". **Checking the other three
per-function shows it does not, and the reason is precise enough to be useful.**

Lane S rests on one property: *on the write path a Serialize is a pure read of the object.* That
is what makes two calls into two sinks a legitimate replay. **Three of the four target rows break
that property** — each does real work on the write path beyond emitting:

| Row | What it mutates on the WRITE path |
|---|---|
| `Audio::Scene::Serialize` | **nothing** — the resize is guarded to the reading side (`0x1800e00c0`'s JNZ). This is why it worked. |
| `ExternalFactory::SerializeReferenceTable` | walks the reference set writing an id into every node (`MOV [RBX+0x14],EAX` inside the `0x1800bcdf4` branch) |
| `InternalBasePtr::Serialize` | ends in `self->ptr = InternalFactory::PersistObject(...)` — mutates both the pointer and the global factory |
| `External::SaveText` | needs `IO::TextFileStream` before the question even arises |

Running ours-then-theirs on any of those three would perform the mutation **twice**, on the one
live tree or the one live factory. That is the same shared-state trap the family was held back
for, wearing a different hat — and `RegisterInput` already showed what it looks like when a second
call succeeds *differently* rather than failing.

**So the next unit of work is a PREFIX oracle, not more ports.** The emission sequence is still
fully determined by the object at entry, so what these three need is a way to compare the
emissions *up to the first mutation* — either by stopping the body at a chosen emission index, or
by snapshotting and restoring the small amount of state each one touches (`InternalBasePtr` is
eight bytes; the reference-set ids are one dword per node). Lane S already records the sequence;
what it lacks is a stop.

**Recorded as a correction rather than fixed silently, because it is the fourth time this session
that a group-level claim of mine failed per-function checking** — after the `ExternalFactory`
four, `KeyMap::Load`, and `SerializeReferenceTable` as "the cheapest row". The per-function checks
have been right every time; the summaries over them have been wrong every time. That is a
measured property of this material, not bad luck: **these functions differ from each other in
exactly the details that decide whether a technique applies.**

### The pattern this session hit three times: a known fact that did not reach the next wave

Three separate holdbacks and one wrong finding, all the same shape — **the information existed in
this repository and the person who needed it did not have it.**

1. **`BANISHED_AUTOQUIT` quits at the main menu.** WAVE3.md's "Notes for later waves" says so, and
   records that it had already produced one green-over-unexercised report. Six waves later it
   produced a wrong path-independence finding in a new tool.
2. **`KeyMap::RegisterInput`'s disputed parameter.** Lane D could not pin `FUN_180023060`'s first
   argument from the call sites. The callee answers it in nine instructions: RCX is never read.
   The function is now ported, 50 calls, 0 divergences.
3. **`Audio::Scene::Serialize`'s `IO::Encoding` offsets.** Lane C held it back because *that lane*
   had not re-derived retail's offsets. Wave 4 had already written them into
   `runtime/src/io/rt_io_port.cpp` — bool +0x78 / uint +0x98 / int +0xa0 — where they have sat
   ever since.

**Each holdback was correct on the evidence its lane had.** The failure is not judgement, it is
transport: a lane doc records what a lane learned, and the next lane reads its own brief. Two of
the three facts above were in a *wave doc* and did not travel; the one that did travel was in a
**source comment at the point of use** (`rt_io_port.cpp`), and even that one was not consulted by
a lane in a different namespace.

**This is the argument for lane 0 of Wave 9 being a reading pass, not just a residue matrix.**
Before porting anything, re-check each holdback's stated blocker against what the tree now knows —
because the measured hit rate on that question in this session was **three for three**.

### Still open going into the wave

Everything inherited now has either a verification or a written disposition. What remains is
work, not undecided work:

* **The 31 remaining holdbacks** — triaged above. **P is DONE** (`UpdateLoop::IsRunning`,
  closed by probe). What is left is B (10, blocked on a named dependency), N (7,
  allocator-bound or oracle-hard) and D/X (14, stubs or, for one of them,
  `SerializeReferenceTable`, genuinely unblocked and lane A's cheapest row). Lane A executes
  that table.
* **`Input::Device::Update` needs a bespoke oracle**, not more time — call-twice corrupts the
  DirectInput queue it is reading. It is the hardest single item in the wave.
* **The perf gate**, deferred by the user to lane H.
* **The 1-ULP libm answer is a bound, not a bit-exact result.** If lane B wants the stronger
  answer it needs a deterministic world-gen fingerprint, which nothing in the tree emits today.

## Sizing

Not S. Waves 6 (113 fns) and 7 (101 fns) were sized L and XL. Wave 9 is **319 items across
17 namespaces, including the four things every previous wave was allowed to defer** — the
allocator, the `FUN_` pool, the data exports and the holdbacks — **with the diff oracle
switched off partway through.** Call it XL with a hard ordering constraint, and expect lane D
to be the one that decides the schedule.
