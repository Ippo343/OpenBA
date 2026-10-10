# Wave 9 lane ADDR — the orphan ADDRESS doors

**Branch `wave9/lane-addr`**, cut from `3c26956`.
**Worktree:** none that held. See §0 — every commit in this lane was made with
git PLUMBING, from whatever directory the harness had moved the session into.

Five commits:

| commit | what |
|---|---|
| `bdef5e1` | `tools/addr_door_audit.py` — the census was reading prose as code |
| `5478d4b` | this document, first version |
| `199b31a` | `ParseCommand` + `ParseValue` |
| `bc4dd13` | `MainLoop::Run` + `MainLoop::Update` |
| (this) | doc rewrite + the census triage note |

**HEADLINE: of the nine addresses assigned to this lane, three were not work at
all, four are now closed, and two are declined with measured blockers.**
The address-door watch list went **16 → 12**.

---

## 0. The environment: lanes shared one session directory

This is first because it shaped everything and because two other lanes were
damaged. Several concurrent lanes shared ONE session working-directory pointer,
so another lane's worktree/branch operation moved my HEAD mid-task.

1. My first commit landed on **`wave9/lane-uif`**. **Repaired**: moved to
   `wave9/lane-addr`, `wave9/lane-uif` reset to `fd30180`, its exact creation
   point. Lane UIF had no commits and lost nothing (it has since committed real
   work on that branch).
2. My worktree filled with four other lanes' uncommitted edits, and later with
   two `*.laneENGIO-leaked.bak` files. All left untouched. **This contaminated
   my first measurement** — see §1.
3. A second, uniquely-named worktree was taken over by lane GFXR and my
   `git commit --amend` amended THEIR in-progress merge.
   **Outstanding**: `wave9/lane-gfxr-work` points at `3dc79c4`, carrying my
   commit message. Content is intact — byte-identical tree (`92fefc7b`) and
   identical parents (`ecd0ec9`, `89416c6`) to `fd30180`; only the message
   differs. Team lead is handling it with lane GFXR.
4. Later, an edit I had already made to `configuration.cpp` was **reverted on
   disk** by another lane's checkout before I could hash it.

**THE PROTOCOL THAT WORKED — COMMIT WITH PLUMBING.** After (3), every commit in
this lane was made without a checkout:

```
git hash-object -w <file in the SCRATCHPAD, outside the repo>
GIT_INDEX_FILE=<scratch>.idx git read-tree <my branch>
GIT_INDEX_FILE=<scratch>.idx git update-index --add --cacheinfo 100644,<blob>,<path>
GIT_INDEX_FILE=<scratch>.idx git write-tree
git commit-tree <tree> -p <my tip> -F <message file>
git update-ref refs/heads/wave9/lane-addr <new> <expected old>   # COMPARE-AND-SWAP
```

Two properties matter. The tree is built from **my branch** plus **only my
blobs**, so a neighbour's edits sitting in the working directory cannot leak
into my commit. And `update-ref` with an expected-old value is a
compare-and-swap: if my branch had moved under me the commit would have been
REFUSED rather than silently rewriting someone's work. After (4) I also kept
the authoritative copy of every source file in the **scratchpad**, outside the
repo, where no lane's checkout can revert it.

Builds and the game run were done from a `git archive` extraction of my own
commit into the scratchpad — a tree nobody else could write to.

---

## 1. THREE of the nine were not doors

### 1a. `0x18006f330` — the instrument was manufacturing work

`tools/addr_door_audit.py` had no notion of a comment, so it matched
`ResolveAddr`/`RegisterRuntimeAddr` text inside comment blocks and string
literals as live code. `engine/funcs/engine_funcs.cpp:3200` sits inside a
comment block whose entire purpose is to record that the code was **DELETED**:

```
//     ResolveAddr(Module::Runtime, 0x18006f330)   // "might be in exe"
```

There is no call. Worse, that same comment identifies `0x18006f330` as **0x90
bytes INSIDE the body of an unrelated internal** (`FUN_18006f2a0`, entry
`0x18006f2a0`, body ends `0x18006f3ca`). The instrument was pointing a lane at a
mid-function address to "port".

This is `reference_documenting_a_check_can_delete_it.md` **inverted**: there a
comment deleted a real check from a scanner; here a comment invented a defect.

Fixed in `bdef5e1`. `strip_comments_and_strings()` blanks comments and
string/char literals, replacing each character with a space and preserving every
newline, so line numbers and columns are unchanged. Quote scanning is **bounded
to one line**: an unmatched quote the scanner does not model (a `1'000` digit
separator, an apostrophe in a macro) would otherwise swallow the rest of the FILE
and silently delete every real door below it — failing open in the direction that
HIDES work, which is the failure being fixed.

Measured on a CLEAN tree. **My first run, on the contaminated tree of §0.2, gave
296 / 199 / 1189 / 650 — those numbers are wrong** and the commit that carried
them was corrected. Recording them because they are the cleanest demonstration I
have that a shared checkout corrupts numbers, not just refs.

|                        | before | after |                              |
|------------------------|-------:|------:|------------------------------|
| by-address call sites  |    295 |   270 | 25 rows were prose           |
| distinct addresses     |    198 |   192 | 6 comment-only               |
| registered addresses   |   1186 |  1186 | **negative control, unmoved**|
| — of which literal     |    647 |   647 | **no registration stripped** |
| UNREGISTERED           |     16 |    15 | set-diff = exactly `0x18006f330` |
| non-literal call sites |     77 |    15 | 62 of 77 were prose          |

The unmoved **647** is the control that matters: had the stripper eaten code,
literal registrations would have fallen. The `--csv` set-diff is exactly one
line. The 77 → 15 collapse retires most of a number this tool had been
advertising as a genuine blind spot; only 15 call sites are really invisible.

### 1b + 1c. `0x1801fe7c0` / `0x1801fe7e0` — already closed, and the brief's model was wrong

The brief said these are "one and two 0x20-strides past the MainLoop singleton"
and probably new mint rows. **Both halves are wrong, and acting on it would have
introduced a split-brain bug.**

`0x1801fe7b8` is `?_loader@LoaderLoop@Engine@@1VLoader@2@A` (ord 4794) — note
`VLoader@2@A`, a Loader **object BY VALUE**. So `0x1801fe7c0` and `0x1801fe7e0`
are **fields inside that object**, at `+0x08` and `+0x28`, which is exactly how
our own tree names them (`g_pLoaderFlags`, `g_pLoaderCompletionEvent`).

`standalone/rt_data_mint.cpp:598-603` **already registers those two interior
offsets** into the minted `_loader` storage, and `rt_data_mint_tables.inc:1673`
mints `_loader` with size 56, which covers both. They read as unregistered only
because the registration goes through a variable (`s.dllVa + 0x08`) — the
REG_LIT blind spot the tool's own docstring declares.

Minting them as independent rows would have given them their own storage,
pointing `g_pLoaderFlags` at memory unrelated to the Loader the engine reads.
The usual danger is a null left deliberately; here it was a second copy.

---

## 2. What closed

All three export names in the brief were verified by me against the shipped
export directory with `pefile`, and all three are **correct** (ords 3601, 3311,
3310).

### 2a. `ParseValue` + `ParseCommand` (`199b31a`) — closed 4 doors, opened 0

`configuration.cpp`'s registration block had listed both as DELIBERATELY ABSENT,
and its banner named them as the next fault at cutover for anyone launching with
an argument.

Both are `CA` **statics returning `String` BY VALUE**: RCX = hidden return slot,
RDX = `int&`, R8 = `const String&`, RAX = ret. The bodies return `String*` and
return `ret` for that reason — declaring them `void` would compile and satisfy
every caller in this tree (all discard the result) but would stop setting RAX.
The two existing typedefs already model the slot correctly; checked, not assumed.

Three faithful details a tidier port would break:

1. `GetLength` is re-called every loop iteration, as retail does.
2. The `src[i]` reads at `0x1800c4a92` / `0x1800c4b08` are **unguarded** and can
   run with `i == length`. Correct, not a missing check: the buffer is
   `length + 1` with a NUL, so `src[length]` is a legal `'\0'` which is neither
   `'/'` nor `' '` nor `':'`. A guard would change the result at end-of-string.
3. The empty return writes `ret->data = nullptr` **directly**; it does not
   construct a String. A default-constructed String here would allocate a
   9-byte block retail never allocates, which `SetCommandLine` would then
   release.

Both callees (`String::GetLength` 2750, `String::SubString` 4124) are already
ported, and are called as ordinary C++ functions, so **no new door of either
kind**. Measured set-diff:

| | before | after |
|---|---:|---:|
| address doors UNREGISTERED | 15 | **13** |
| name doors unsatisfied | 129 | **127** |
| registered (literal) | 647 | 649 |
| by-address call sites | 270 | 270 *(unchanged)* |
| distinct addresses | 192 | 192 *(unchanged)* |
| distinct names resolved | 848 | 848 *(unchanged)* |

### 2b. `MainLoop::Run` + `MainLoop::Update` (`bc4dd13`) — verified by boot

`Run` is the single most consequential address door in the wave, and Wave 8 lane
B had explicitly held it back. `game_main.cpp:176` reaches it BY ADDRESS.

**Vtable slots were read out of the binary, not inferred** — dumped from
`??_7DialogLoopT@Engine@@6B@` (`0x1801579a0`), the DERIVED table, because the
base table's slots are all the same `_purecall` thunk and prove nothing:
slot 1 `Startup`, slot 2 `Shutdown`, slot 3 `Update(const Time&)`, slot 0
deleting dtor.

**The clock, and a hazard this port would have been the first to trigger.**
Calling our own `RtSystem_Timer_CurrentTimeTicks()` here would have been the
obvious move and a bug: `timer.cpp` keeps its OWN `_systemInitialTick`, sampled
by OUR `PlatformStartup`, while the DLL's `Frame`/`Draw` — which this port still
calls — read the DLL's globals. `timer.cpp`'s banner says that hazard is bounded
because "no file names `CurrentTimeTicks` at all"; **this is the first function
that would have made it observable.** So the three globals are read through
`ResolveAddr`, exactly as retail reads them: the DLL's storage while the DLL is
present (one epoch, shared with `Frame`/`Draw`), the mint's at cutover.
`QueryPerformanceCounter` is called directly, not through the DLL's IAT slot at
`0x18014b1a8`, which will not exist after cutover.

**The two already-ported callees are reached BY ADDRESS deliberately** —
`ResolveAddr` consults the port registry FIRST, while `ResolveExport` falls
through to the shipped implementation for anything not registered under that
exact name and would silently bypass a port we already have.

**VERIFICATION — a boot, which is the only test this function can have.**
DLL present, `BANISHED_RT_DIFF=1`, `BANISHED_AUTOSTART=1`, driven
menu → New Game → Start → in-game → pause menu → Quit:

```
Engine::MainLoop::Run     calls=0  worked=1  divergences=0
Engine::MainLoop::Update  calls=1  worked=1  divergences=0
```
crash log 0 bytes · `[RtResolve] 0 misses` · gameplay reached (new world at
frame 1, 901 in-game frames, clean self-quit) · the run's only `DIVERGENCE` row
is `Object::Factory::Startup [probe, not diffable]`, another lane's pre-existing
probe.

**Read `Run`'s `calls=0` correctly: it is NOT a dead port.** `calls` counts
COMPARISONS, and `Run` makes none because it genuinely cannot have an oracle —
called once, never returns until quit, mutates the world through the loop
objects. **`worked=1` is the evidence**: the body executed and drove the whole
session. `worked=0` would have meant the registry never handed
`game_main.cpp:176` our function.

**THE SHADOW PREDICATE WAS WRONG AND FIRED — which is why it was worth having.**
The first version asserted `_nextLoop == nullptr` at the end of `Update` and
diverged on the first real transition. The PORT was right, the PREDICATE was
wrong: retail clears `_nextLoop` BEFORE calling `Startup` (`0x1800dd0c8` precedes
`0x1800dd0d7`) precisely so `Startup` may queue the following loop, and the
menu → game chain does exactly that. Asserting the field is still null at the
end outlaws the engine's normal loop chaining. The predicate now samples the
transition step's own post-condition. This is recorded in the source rather than
quietly relaxed — a shadow loosened until it stops complaining verifies nothing —
and it doubles as the **negative control**: the predicate is not vacuously true,
it was observed to fire and went green only after a real fix.

**DOOR ARITHMETIC — this one makes the counted surface go UP, as expected.**

| callee | ported? | door if `Run` is ported |
|---|---|---|
| `0x1800de520` `Audio::Device::Update` | yes | no |
| `0x18009fdf0` `Application::PlatformUpdate` | yes | no |
| `0x1800e1490` `Input::Device::Update` ord 4482 | no, lane INPUT's | already a name door |
| `0x1800dd070` `MainLoop::Update` | **ported here** | no |
| `0x1800dd100` `MainLoop::Frame` ord 2377 | no, own lane | **NEW name door** |

Closes 1 ADDRESS door, opens 1 NAME door (`Frame`; `Input::Device::Update` was
already unsatisfied at `engine_funcs.cpp:1554`). The exchange is worth making:
an unregistered ADDRESS door fails **silently**, a name door prints
`[RtUnresolved]` and is counted. One undiagnosable null call becomes one
diagnosable one.

`MainLoop::Update`'s `worked=1` is ONE transition, and that is the honest
number: our `Update` is reached only from our `Run`, before the frame loop.
Inside the loop the DLL's `Frame` ticks and calls the DLL's `Update` by a direct
internal call the registry cannot redirect. Exercised on exactly the real
menu → game transition and nowhere else until `Frame` is ported.

On its own switch `BANISHED_RT_MAINLOOPRUN=0`, because a defect here shows up as
"the game does not run" rather than as a divergence row.

**Corrects `runtime/types/engine/MainLoop.h`**, which files `Update` and `Frame`
under "Internal, NOT exported". Both ARE exported (ords 4496, 2377). It matters
because it decides whether the residue is a NAMED door the scanners see or a
SILENT one they cannot.

---

## 3. Final measurements

Address-door watch list, `16 → 12`, taken on a clean `git archive` of `bc4dd13`:

| | start | end |
|---|---:|---:|
| UNREGISTERED | 16 | **12** |
| by-address call sites | 295 | 275 |
| distinct addresses | 198 | 194 |
| registered addresses | 1186 | 1190 |
| name doors unsatisfied | 129 | 128 |
| distinct names resolved | 848 | 849 |

Rows removed: `0x18006f330` (phantom), `0x1800c4930`, `0x1800c4a00`,
`0x1800dd3c0`. Rows added: none.

Pre-report audits, all on the same clean tree:

* `detour_target_audit.py` — **clean, every detour target resolves RAW**
* `abi_retslot_audit.py` — **0 suspicious of 48** by-value-returning exports
* `port_bypass_audit.py` — **0** address-door sites reach a function already
  ported by name; **0** raw-door sites are a ported function on a live path
* `rawdoor_audit.py` — 609 sites, **0 UNGUARDED-CALL**, 66 DEFERRED-CALL
* `runtime_census.py` — TOTAL 4632 / called 1003→**1004** / ported 771→**774** /
  76.9% → **77.1%**

`called` +1 is `MainLoop::Frame`, the name door this lane opened. `ported` +3 is
`ParseCommand`, `ParseValue`, `Run`. **`MainLoop::Update` is +1 port that does
NOT appear in `ported`**, because it has no resolve door in our tree — see the
triage below.

**CENSUS TRIAGE — a new name appeared in "Ports with no caller in our tree".**
`runtime_census.py` now lists `?Update@MainLoop@Engine@@`, and the tool's own
note says a NEW name there has NOT been triaged. This one has: it has no resolve
door because `RtEngine_MainLoop_Run` calls it as a **direct C++ call**, exactly
where retail's `Run` calls it (`0x1800dd445`). That is the same category
`WAVE8_5.md` §2 already clears — "exercised by a direct C++ call from a sibling
port" — not the dead-port category. And it is measured, not argued:
`calls=1 worked=1 divergences=0` on the real transition.

---

## 4. HONEST LIMITS

* **Two of the six real doors are NOT closed** — `0x1800042a0` / `0x180005dc0`
  and `0x180052640`. Both are declined with measured blockers in §5, not
  forgotten.
* **`Run` has no oracle and never will.** Its evidence is one boot that reached
  gameplay and quit cleanly. That is strong, but it is a *functional* test of one
  code path on one machine with one set of flags. It does not prove the teardown
  path is byte-faithful, and it does not exercise `_quit` being set from anywhere
  other than the pause-menu Quit.
* **`MainLoop::Update` is exercised exactly ONCE per boot.** Reported as
  `worked=1`, not as coverage. Until `Frame` is ported, that is all it gets.
* **`ParseValue` / `ParseCommand` are UNEXERCISED.** They compile and are
  registered under both doors, but `calls=0` on every current boot: no harness in
  this tree passes a command line (every knob is an env var), and
  `SetCommandLine`'s own banner explains retail's `JLE` returns before the loop
  body on an empty one. **They are ported but not verified**, and I did not build
  the probe that would have driven them. This is the one place in the lane where
  I closed a door without exercising the code behind it.
* **The shadow on `Update` is a self-consistency check, not an oracle.** It
  compares post-conditions the body itself establishes. It would catch a botched
  transition; it would not catch a wrong-but-consistent one.
* **The boot verification used the `-O0` debug build only.** No `-O2` run, no
  stress run, no perf measurement. `Run` is on the hot path and I did not measure
  whether reading the three Timer globals through cached `ResolveAddr` statics
  costs anything against retail's direct RIP-relative loads.
* **My first measurement was contaminated** by other lanes' uncommitted edits and
  reported 296/199/1189/650. Re-taken clean; the committed figures were corrected
  in a follow-up, not amended.
* **One repair is still outstanding on another lane's branch** (§0.3).
* **The doc commit and the census triage note are comment-only changes made
  AFTER the verified boot run.** Comments are not compiled, so the verification
  above still describes the committed code — but the exact binary that was run
  was built from the source without those comments.

---

## 5. What I DECLINED, with the measured blocker

| item | measured blocker |
|---|---|
| `0x1800042a0` / `0x180005dc0` — `ExternalBasePtr<T>::Serialize` ×2 | **Real.** I diffed the two bodies instruction by instruction: they are identical apart from one type-id global (`0x1801fae08` vs `0x1801f9908`), confirming `goal_serialize.cpp`'s template assumption, and all three callees are exported (`External::GetPath` 2835, `ExternalFactory::FindOrLoad` 2359, `Base::IsDerivedFrom` 3110). But **the write path (mode != 3) and the temporary `Filename`'s refcount/lifetime are undecoded.** Porting a `Serialize` on a half-read body risks a save-file corruption the read-path oracle would not catch. |
| `0x180052640` — audio per-element bulk serialize | **Real, and it is a scoping fact.** Fully decoded (stride `0x40`; signature matches the typedef `audio.cpp` already uses; per element: `rw->vtbl[+0x28](rw,3,e+0x00,0)`, `+0x98(e+0x0c)`, `call 0x180051790(e+0x10)`, `+0x90(e+0x18)`, `+0x98(e+0x24)`, `+0x80(e+0x28)`, `call 0x180051e00(e+0x30)`). It closes 1 door and **opens 2** — `0x180051790` and `0x180051e00`, neither exported nor ported. It belongs to the audio lane together with those two, not to this lane alone. Note its ReaderWriter vtable offsets are the **retail** ones; `reference_encoding_vtable_offsets_differ_between_builds.md` records that the profile build's differ. |
| `MainLoop::Frame` (`0x1800dd100`) | Out of scope and correctly so: 683 bytes, ~180 instructions, six Win32 imports, an indirect `call [rbx+rax]` dispatch table, and four internal callees including `?Draw@MainLoop@Engine@@` (`0x1800dcd40`, ord 2251) — the subtree where lane CTX's seven `Video::Context` setters go live. Its own lane. |
| A probe to exercise `ParseValue`/`ParseCommand` | Not attempted. It is the obvious next step for whoever picks these up: feed `SetCommandLine` a synthetic non-empty command line and read `calls=`. |
| Re-opening `0x1801fe7c0` / `0x1801fe7e0` | Declined on measurement: already closed (§1b). |
