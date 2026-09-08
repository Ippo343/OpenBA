# Wave 9 — lane ENGIO: `Input::Device::Update`, `Audio::Scene::Update`, and the device-lost pair

Branch **`wave9/lane-engio-work`** (not `wave9/lane-engio` — see §8), based on
`ae8195b` *Name the 16 orphan ADDRESS doors*, which is a superset of the
`fd30180` tip the lane brief named.

**Landed: 4 bodies (`Input::Device::Update` and its three internal helpers),
1 symbol closed, 0 doors opened, and PLAN §5.5's forced device-lost event
EXERCISED FOR THE FIRST TIME.**
**Declined: `Audio::Scene::Update`, with a measured blocker that is not the
recorded one, and `MainLoop::OnVideoError` / `OnVideoRestart`, with lane VID's
stated blocker shown to be STALE and replaced by a smaller, named one.**

---

## 1. The door count — RE-MEASURED on clean trees with ONE instrument version

⚠️ **The numbers this section carried in its first version are WITHDRAWN**, and
not because they were wrong about the port. Two things invalidated them:

1. **`tools/addr_door_audit.py` had no notion of a comment.** It matched
   `ResolveAddr` / `RegisterRuntimeAddr` text inside comment blocks and counted
   it as live call sites — 25 of 295 "call sites" tree-wide were prose. Lane
   ADDR's fix landed in `f5dbeb8`.
2. **This session was pinned into other lanes' worktrees for part of the run**
   (§8.1), and a dirty tree corrupts numbers as well as refs — lane ADDR
   measured a contaminated 296/199/1189/650 against a clean 270/192/1186/647.

So everything below was re-taken. Both sides are **checkouts built straight from
the object store** (`git archive` of the two commits — no working directory is
involved, so no neighbour's uncommitted file can reach either side), and **the
same instrument version, `f5dbeb8`'s, is copied into both.** A "before" from the
old tool against an "after" from the new one is a meaningless diff that reads as
closures nobody made.

```
BEFORE = ae8195b   (this lane's base)
AFTER  = 2d763dc   (this lane's tip)
TOOLS  = f5dbeb8   (comment-aware addr_door_audit), on BOTH sides
```

| instrument | BEFORE | AFTER |
|---|---|---|
| `addr_door_audit` by-address call sites | 270 | **270** |
| `addr_door_audit` distinct addresses | 192 | **192** |
| `addr_door_audit` registered addresses | 1186 | 1187 (+1) |
| `addr_door_audit` UNREGISTERED | 15 | **15** |
| `addr_door_audit` non-literal (its own blind spot) | 15 / 8 | **15 / 8** |
| `unsatisfied_export_scan` names through a door | 848 | **848** |
| `unsatisfied_export_scan` unsatisfied | 126 | **125** |
| `runtime_census` called | 1003 | **1003** |
| `runtime_census` ported | 771 | 772 |
| `runtime_census` Input | 22/28 (78.6%) | 23/28 (82.1%) |
| `port_bypass_audit` | 0 bypasses, both doors | **0 bypasses, both doors** |
| `rawdoor_audit` | 607 sites, 0 UNGUARDED-CALL | 608 sites, **0 UNGUARDED-CALL** |
| `detour_target_audit` | clean | **clean** |

Set-diff of the unsatisfied-name list (126 -> 125):

```
removed: ?Update@Device@Input@@QEAAXAEBVTime@System@@@Z
added  : (none)
```

**Every denominator is unmoved and exactly one thing moves in each column.** The
+1 registered address is this lane's own `RegisterRuntimeAddr(0x1800E1490)`; the
+1 raw-door site is the probe's `ResolveExportRaw`, classified `OK-ORACLE`; the
one removed name is the symbol the lane was given. `called` did not grow, which
is the check WAVE9.md's *"porting a function can GROW the cutover surface"*
section asks for.

### The finding this section originally reported, and what survives of it

The FIRST measurement — old tool, both sides — read **297/200** for a lane that
adds no `ResolveAddr` at all, and the two extra rows were two of this lane's own
banner lines spelling `ResolveAddr(Module::Runtime, 0x…)` verbatim inside a
comment. That was an independent reproduction of the defect `ae8195b` was
committed to fix, found because a lane that knew its own delta should be ZERO
noticed a two. **A lane expecting a small positive delta would have accepted it.**

`f5dbeb8`'s comment-aware version makes the prose invisible, so the two lines no
longer need rephrasing — but the rephrasing is KEPT. It costs nothing, and a
comment that reads as code is worth not writing whatever the current tool does
with it.

**What does NOT survive:** the claim, in the first version of this document, that
lane ADDR's fix was incomplete. It was written against `ae8195b`, which does not
contain the fix; `f5dbeb8` does, and against `f5dbeb8` the prose is correctly
ignored. Withdrawn.

## 2. `Input::Device::Update` — the closure, counted before a line was written

| # | Body | Retail | Instrs | Calls out | Status |
|---|---|---|---|---|---|
| 1 | `Update` | `0x1800E1490` | 61 | the three below + PlatformUpdate | PORTED |
| 2 | `UpdateState` | `0x1800E0FC0` | 234 | **ZERO — a pure leaf** | PORTED |
| 3 | `PlatformGetKeyboardState` | `0x1800E1C60` | 98 | USER32 x2, COM `+0x48`/`+0x38` | PORTED |
| 4 | `PlatformGetMouseState` | `0x1800E1E10` | 192 | USER32 x5, COM `+0x48`/`+0x38` | PORTED |
| 5 | `PlatformUpdate` | `0x1800E2130` | 41 | USER32 x4 | already lane IOA's |

Runtime DATA read by the four: `?_singleton@Application@1@`,
`?_singleton@Configuration@Input@@` and `?_secondsPerTick@Timer@System@@` — all
three MINTED since lane C2. The two byte tables at `0x1801F9480` (0x69 bytes)
and `0x1801F94F0` (0x100 bytes) are LIFTED AS LITERALS, not read through a door,
and that is what makes "no new doors" true rather than nearly true. Neither was
transcribed by hand: `read_memory` returned the bytes, a script re-encoded the
decoded arrays to hex and asserted equality against that same dump, and only
then were they written out.

**Lane INPUT's and lane SYS2's measurements both held up** — 626 instructions
across five bodies, and the COM surface being exactly two `IDirectInputDevice8`
slots. The second is no longer an inherited claim: see `comTraps` in §3.

---

## 3. The blocker that stood, and why the prescribed recorder was not needed

Not the size, and **not "needs an attended run with a real keyboard and mouse"** —
lane INPUT closed that with 1,914 comparisons and no keyboard, and nothing in
these four bodies reads a human. What stands is that **`Update` DRAINS the
DirectInput queue**: an ours-then-theirs replay at the live call site hands the
second body a queue the first already emptied, so the two sides see different
worlds by construction and every comparison is meaningless, green or red.

Lane INPUT specified a `GetDeviceState` RECORDER — one detour on a COM slot, the
`ProbeAudioScene` shape. Building it, the recorder turned out to be
unnecessary, by the rule that dissolved three of six Wave 8 audio holdbacks for
free: **ask what the guard that reaches the dependency actually READS.** The
pollers read `this->[0x11E0]` / `[0x11E8]` and then two slots of that object's
own vtable. Nothing validates the pointer, nothing `QueryInterface`s it, nothing
else touches it. So a **synthetic COM device we own outright** is a DirectInput
device as far as they can tell — and it beats a detour in the way that matters
here: a scripted `GetDeviceState` hands **both sides the same bytes**, so the
drain stops existing rather than being recorded around
(`reference_synthetic_subject_beats_a_fake.md`).

`runtime/src/input/input_device_update_probe.cpp`, `BANISHED_RT_INPUTUPD_PROBE=1`.
Two synthetic `Input::Device` images of `0x1200` bytes, seeded from a **0xA5
filler** rather than zero (so a store the port SKIPS is visible, instead of being
indistinguishable from a store of 0), stepped through the same scripted sequence,
compared byte for byte after EVERY step.

### 3.1 The measurement

Final run, gated on this lane's own identity line read out of that run's own log
(`[RtPort] Input: 30 exports registered (… lane ENGIO: +4 Device::Update bodies)`),
exe md5 `c06ec3efdfae3aeefc6dd9fee5e84bad`, `rc=0`, **crash log 0 bytes**:

```
[InputUpd] cases=32 mismatches=0 unsound=0 ambient-retries=0
           (legacy=8 directinput=24 getstate-fail=8)
[InputUpd] effectiveness: pressedEdges=200 releasedEdges=225 repeatFires=22
           holdAdvances=299 negativeAxis=40 index104Lit=2 acquireSpins=36
           wheel-=6 wheel+=6 comTraps=0
[InputUpd] negative-controls: comparator=CAUGHT subject=CAUGHT

[RtDiff] ==== 509 sites, 16,303,517 calls, 1 divergences in 1 sites ====
[RtDiff]   Input::Device::Update                    calls=184  worked=184  divergences=0
[RtDiff]   Input::Device::PlatformGetKeyboardState  calls=184  worked=88   divergences=0
[RtDiff]   Input::Device::UpdateState               calls=371  worked=179  divergences=0
[RtDiff]   Input::Device::PlatformGetMouseState     calls=184  worked=88   divergences=0
[RtDiff]   Input::Device::PlatformUpdate            calls=184  worked=0    divergences=0
```

The one tree-wide divergence is the pre-existing `Object::Factory::Startup`
self-check (WAVE9.md instrument #6, lane SYS2's open item 2), not this lane's.

### 3.2 What each counter is for, and what a zero would have cost

Every one is read **out of the resulting Device state** — inside the subject —
not inferred from what the script asked for
(`reference_probe_needs_an_effectiveness_control.md`).

* `repeatFires=22` is the only counter that cannot be satisfied by noise: it
  requires the hold timer to cross 0.3 s, which needs a MULTI-STEP case. It is
  why a case is a sequence of 8 frames and not a single call, and it is what
  verifies the two lifted constants `0x3FD3333340000000` and
  `0x3FD0000000000000`.
* `negativeAxis=40` is the `MOVSX`-vs-`MOVZX` control. `UpdateState` loads the
  raw value SIGNED for the divide and tests it as a 16-bit equality-with-zero;
  reading either as the other gives a body wrong ONLY for negative axis values,
  which no keyboard and no mouse button can produce. The mouse-delta script
  feeds them deliberately, including `±32769` and `±40000` to cross 16 bits.
* `acquireSpins=36` proves the `DIERR_INPUTLOST` retry loop actually spun — one
  eighth of the cases script `GetDeviceState` to fail.
* `index104Lit=2` is the retail scan-code quirk (§3.4).
* **`comTraps=0` is the check on the probe's own premise.** Every vtable slot of
  the synthetic device other than `+0x38` and `+0x48` is a TRAP that prints and
  voids the run. The two-slot COM surface was an inherited MEASUREMENT; it is
  now tested rather than believed.

The probe prints a WARN line naming any counter that reads zero and saying what
that costs, rather than leaving a reader to notice. All of them are printed
unconditionally, not through a macro that only speaks on failure — WAVE9.md
instrument #7, *if a control is only visible when the test fails it is not a
control.*

### 3.3 Two negative controls, and why one is not enough

1. **COMPARATOR**: flip a byte of side A and require the same comparator lambda
   the positive cases used to notice. CAUGHT.
2. **SUBJECT**: run a deliberately WRONG `UpdateState` — `released = (prev != 0)`
   with the `raw == 0` term dropped, the single most plausible transcription slip
   in a body whose two flag computations differ only in a `JZ`/`JNZ` — and
   require the comparator to catch it. CAUGHT, at `+0x0727`, keyboard index 4.

Control 1 alone proves only that the comparator can see a corrupted BYTE.
Control 2 is what proves it can see a wrong ALGORITHM, which is the thing under
test. The input is chosen so one key is RELEASED while another stays DOWN, which
is the only shape that separates the two predicates.

### 3.4 A retail quirk preserved deliberately

**155 of the 256 entries in the scan-code table are `0x68` = index 104**, and 104
is a LIVE input (its virtual key is `0xDF`), not a discard slot. So every
unmapped scan code writes into the same state record as scan code `0xDF`, and
because the sweep runs `0..255` in order the LAST unmapped scan code decides what
index 104 holds. Holding an unmapped key lights up index 104. A port that
"fixed" this by routing unmapped codes to a scratch slot would diverge on
exactly that input and on nothing else. The probe presses unmapped keys.

Also preserved, with the reason at the line: **the zeroing loops in `Update` are
not `memset`s.** They write bytes `+0x00..+0x07` and `+0x10..+0x17` of each 24-byte
record and leave `+0x08..+0x0F` alone. It is harmless — `UpdateState` reads only
`src[i].value` — so a `memset` would be byte-identical downstream and is still
not what retail does. Same argument `engine.cpp`'s `MainLoop` ctor gives for its
own partial zero.

### 3.5 A number that varies between runs, said rather than hidden

`repeatFires` read 38 on one run and 22 on the next; `negativeAxis` 48 then 40.
Both runs green. The cause is that the 8 LEGACY cases read `GetAsyncKeyState`
and the real cursor, which are ambient — the 24 DirectInput cases are fully
scripted and deterministic. `ambient-retries=0` on both runs says no case ever
needed its retry, so the variance is in what the legacy cases OBSERVED, not in
whether the two sides agreed.

---

## 4. ⛔ THE FINDING THAT CHANGES WHAT THIS PORT MEANS: `Update` HAS ZERO CALLERS

`engine/funcs/engine_funcs.cpp:1554` resolves
`?Update@Device@Input@@…` into `fn_InputDevice_Update` at init and **nothing in
the tree ever calls that pointer** — grepped across `code/`, `engine/`,
`runtime/`, `standalone/` and `dx11/`; the only hits are the declaration, the
definition and the resolve itself. The per-frame call actually being made is a
**NEAR CALL inside the DLL's own `Engine::MainLoop::Run`**, which is held back,
and a near call inside the DLL cannot be intercepted by a registration.

So the live `calls=` is 0 and stays 0 until `MainLoop::Run` is ported. **All 184
`Update` calls in §3.1 are the probe's.** That is
`reference_uncalled_port_is_not_verified.md` sitting directly on the
deliverable, and it is why the probe is not a nice-to-have — it is the only
verification this function can have.

**The first version of this lane self-scheduled the probe from the top of
`Update` and measured exactly nothing**, printing no `[InputUpd]` line at all
and no diff site, on a run that otherwise looked completely normal. The hook is
now in `Input::KeyMap::GetValue` (measured at `calls=9000` on the autostart
route), and the reason is recorded at both ends rather than only here.

*Generalise it:* **a probe hosted inside the function it verifies inherits that
function's reachability.** For a port whose whole point is that nothing reaches
it yet, that is a guaranteed silent no-op.

---

## 5. DECLINED: `?Update@Scene@Audio@@` — the recorded blocker is stale, and the real one is different

WAVE9.md rows 2-4 hold it on *"every path ends in a STORED COMPLETION CALLBACK
(`CALL RAX`)"* plus *"it consumes `System::Random`"*. Lane SYS2 already showed
both halves stale (the device-level fake exists in `ProbeAudioScene`; the RNG is
a `Random&` PARAMETER, which the mangled name says outright) and reduced the
holdback to size: 438 instructions.

**A third thing was stale and nobody had checked it: the closure is NAMED.**
Read straight out of the shipped export directory with `pefile`, the callees
earlier notes describe as unexported internals are exports:

| retail | export | our side |
|---|---|---|
| `0x1800DFDE0` | `?GetAttenuatedVolume@Scene@Audio@@` ord 2453 | unported |
| `0x1800DFED0` | `?GetVolumeForPosition@Scene@Audio@@` ord 3027 | unported |
| `0x1800DE7A0` | `?PlayPlatform@Device@Audio@@` ord 3424 | unported |
| `0x1800DFFE0` | `?OnSoundEnded@Scene@Audio@@` ord 3283 | unported |
| `0x1800DF550` / `0x1800DEF60` / `0x1800DF370` | StopEffect / PlayEffect / StopEffect3D | **PORTED** |
| `0x1800C7DD0` | `?NextGroup@Random@System@@` | **PORTED** |
| `0x180050D30` | not exported | **already ours** (lane D's J10) |

**None of the four unported names is a door in our tree today** — grepped;
`Scene::Update` is currently one unsatisfied name whose whole implementation
lives in the DLL.

### The blocker that stands, measured here

**1. It closes 1 door and OPENS 1.** A faithful `Scene::Update` must call
`Device::PlayPlatform` and must store `&OnSoundEnded` into the device record.
`PlayPlatform` is the XAudio backend — six indirect vtable calls plus
`?Play@Stream@Audio@@` — and is a different lane. So the census move is
`-1 Update, +1 PlayPlatform`: net zero, for a 438-instruction body. That is the
shape rule 2 of the lane brief refuses.

**2. The device call sequence is UNCOMPARABLE, and this is the new fact.**
`ProbeAudioScene` compares Scene STATE and, in phase 2, the `Device::Stop` call
sequence via a detour. The same trick cannot work for `PlayPlatform`: in OUR
body it would be a resolved call the detour catches, and in THEIRS it is a NEAR
CALL inside the DLL that the detour misses. The two sides would be asymmetric by
construction — **exactly the reason `ProbeAudioScene` already EXCLUDES `StopAll`
from phase 2** (retail inlines `Device::Stop` there), written down in that file.
State comparison alone cannot see a port that played the wrong sound, at the
wrong volume, on the wrong channel, and that is most of what this function does.

So the honest next step is not a sixth `kFns[]` entry, as lane SYS2 proposed —
that entry would compare the half that does not matter and report green. **It is
a lane that takes `PlayPlatform` and `OnSoundEnded` first**, after which both
sides call OUR `PlayPlatform` and the sequence becomes comparable for real.

---

## 6. DECLINED: `OnVideoError` / `OnVideoRestart` — lane VID's blocker IS stale, and the replacement is much smaller

Lane VID declined both with *"not one of its seven internal callees is ported"*
and *"porting either would relocate 17 nulls rather than remove any"*. Re-derived
today against the export directory and against our own registrations:

**`MainLoop::OnVideoError` (0x1800DC5E0) — 7 of 8 distinct callees are ALREADY OURS:**

| callee | name | ours? |
|---|---|---|
| `0x1800C7A50` | `?MemAlloc@MemoryAllocator@System@@` | ✅ |
| `0x1800C6C00` | `?Free@MemoryAllocator@System@@` | ✅ |
| `0x1800C6970` | `?AdjustDelimiters@Filename@System@@` | ✅ |
| `0x180006250` | `?GetPath@External@Object@@` | ✅ |
| `0x1800BE9D0` | `?FindNext@ExternalFactory@Object@@` | ✅ |
| `0x1800F16E0` | `??1Context@Video@@` | ✅ |
| `0x180033880` | not exported | ✅ (address-registered) |
| `0x18004F620` | not exported | ❌ **the only one left** |

`0x18004F620` is not mysterious either: disassembled, it is
`Collection::Array<T>::SetSize(n, fill)` for the 0x18-byte element that
`MainLoop::_videoResources` holds — the same growth policy `Collection.h`
already documents, calling `0x1800067D0` (element assign) and `0x180034440`
(destroy range).

**`MainLoop::OnVideoRestart` (0x1800DC3B0) — 4 of 8, and every missing one is NAMED:**
`?Load@External@Object@@`, `?GetCompiledPath@ExternalFactory@Object@@`,
`?OnVideoRestart@SaveLoad@Save@@`, plus the unnamed `0x180034440`.
`?CreateVideoContext@MainLoop@Engine@@` is already ours.

**So "17 nulls" is wrong by roughly a factor of three, and the residue is one
Array instantiation plus three named exports.** This is a tractable lane. It is
declined HERE only because it is a third lane's worth of work on top of the two
above, and because the ten unexported `.data` type-id globals `OnVideoError`
reads (`0x1801FA548` and nine siblings) still have to be shown to be minted —
which this lane did not check and will not claim.

### But PLAN §5.5's gate is CLOSED, which was the more valuable half

> *"A forced device-lost event exercised before cutover"* — never done, because
> nothing in the harness could make `Present` fail and waiting for a real driver
> reset is not a test.

`BANISHED_FORCE_DEVICE_LOST=<n>` (`dx11/src/contextdata.cpp`) substitutes ONE
synthetic `DXGI_ERROR_DEVICE_RESET` for the result of the Nth `Present` and
nothing else, at the exact place a real reset delivers it. **Everything after it
runs for real on the real device** — the two DXGI comparisons, the `onLost`
notify, `Device::ReleaseDynamic`, the `d3dDevice` null, `DeviceData` slots 3 and
2, `RecreateDynamic`, the `onRestored` notify. The device really is torn down
and recreated; this is not a mock of the recovery, it is the recovery.

```
[DeviceLost] ARMED: a synthetic DXGI_ERROR_DEVICE_RESET will be injected on Present #400
[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET (real Present returned 0x00000000)
[DeviceLost] hr=0x887a0007 onLost=00007ff74d914d99 -> calling
[DeviceLost] recreate OK; onRestored=00007ff74d914dff -> calling
[DeviceLost] device-lost round trip COMPLETE
```

Both callback addresses are inside **our exe** (image base `0x7ff74d640000`), so
the two thunks lane VID installed into `Video::Device+0x50/+0x58` are live and
correct, and neither printed its *"callback fired but … is not available"* line —
which is how we know the DLL's members were reached rather than silently
skipped. Run exited `rc=0`, crash log 0 bytes, and the same run carried the
input probe.

**What it does NOT reproduce, stated rather than implied:** the D3D device is
not genuinely removed, so `GetDeviceRemovedReason` is never consulted — and it
cannot be, because `DEVICE_RESET` is one of the two values that SKIP that query.
This drives the NOTIFY-AND-RECREATE half, which is the half §5.5 is about and
the half both `MainLoop` members live on.

---

## 7. ⛔ A CUTOVER ITEM THIS LANE FOUND AND DOES NOT OWN

The brief asked me to confirm rather than assume that
`[VideoInterface] *** DETOUR FAILED ***` at cutover is expected. It is expected —
and **the consequence the message names cannot happen, which made it worse than
useless at the one moment it is guaranteed to print.**

*"Runtime will LoadLibrary VideoDX11-x64.dll and, if it is missing, silently fall
back to VideoDX9-x64.dll"* needs a Runtime. At cutover there is none. Nothing
loads VideoDX11 and nothing falls back to DX9 either.

The real consequence is quieter: `FillDx11Slot` is reachable **only** from
`WrapInitializeInterfaces` — grepped, there is no other caller — so with the
detour gone **nobody fills the DX11 interface slot at all.**
`Video::Device::InitializeInterfaces` (`0x1800F79F0`) is **UNPORTED AND
UNREGISTERED** (every hit across `runtime/`, `standalone/`, `engine/` and `dx11/`
is a comment).

Two things done about it, neither of them a port:
* the message is corrected at the site, with both the DLL-present and the
  DLL-absent reading spelled out (PLAN §6 step 8 — fixed where it is written,
  not only in a wave doc);
* `VideoInterfaceFillDx11Slot(void*)` is exported from `standalone/video_interface.h`
  so the ported `InitializeInterfaces` has something to call. `VideoInterfacePrefilled()`
  already exists and is the detector.

---

## 8. HONEST LIMITS

1. **`Input::Device::Update` has no live caller and cannot have one until
   `Engine::MainLoop::Run` is ported.** Its `calls=184` is entirely the probe's.
   §4.
2. **The two `IDirectInputDevice8` pointers at `+0x11E0`/`+0x11E8` are created by
   `PlatformConstruct` (0x1800E1820) and `??0Device@Input@@`, NEITHER PORTED.**
   With the DLL absent they hold whatever the minted storage holds, and the
   non-legacy path dereferences them unguarded — faithfully, because retail does.
   No guard was added; the upstream fact to fix is that our `Input::Device` was
   never constructed. **This port does not make `Input` cutover-safe on its own.**
3. **`0x1200` is a LOWER BOUND on `sizeof(Input::Device)`.** Nothing read
   establishes an upper bound, so the probe's byte comparison is SILENT about
   anything past it. Neither implementation is known to write past it; that is
   the absence of a counter-example, not a proof.
4. **The two COM-pointer qwords are the one excluded range** in the comparison,
   because the two sides are deliberately given different synthetic devices.
   Nothing else is excluded — `_mouseClipping`, `_hasFocus`, `wheelAccum` and
   both `lastCursor` fields ARE compared.
5. **`PlatformUpdate worked=0` in every run.** The probe sets `_mouseClipping=0`
   so both sides take `ClipCursor(NULL)`; **the clipping branch is not exercised
   by this lane** and remains lane IOA's coverage, which has no oracle either
   (ClipCursor is global OS state).
6. **The forced device-lost drive was run ONCE, on one machine, on one GPU.** It
   proves the path completes; it is not a soak, and `GetDeviceRemovedReason` is
   untouched by it (§6).
7. **The probe's legacy-path counters vary run to run** because they read ambient
   USER32 state (§3.5). Only the 24 DirectInput cases are fully deterministic.
8. **The ten unexported `.data` type-id globals that `OnVideoError` reads were
   NOT checked against the mint tables.** §6's "tractable" verdict is about the
   CODE closure only.
9. **Branch coverage of `PlatformGetKeyboardState`/`GetMouseState` is
   `worked=88/184`**, i.e. about half the calls took the did-work arm. The
   not-foreground legacy arm and the GetDeviceState-failure arm are both counted
   as "did not work" by design, so this number is a floor on coverage, not a gap.
10. **Branch/verification asymmetry inherited from lane W's finding applies here
    too:** the `getstate-fail` cases drive the retry loop, but only through
    OUR scripted device. Nothing proves the DLL's loop and ours agree on a
    failure the DLL's own Acquire would produce.

### 8.1 Where this lane is, and where its commits are

* **Worktree:** `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\engio-lane`
  — created by this lane, shared with nobody, `git status` empty. The earlier
  worktree (`…/w9-addr-089e6eb4/.claude/worktrees/engio-089e6eb4`) was nested
  INSIDE lane ADDR's tree and had lane MOD's files leak into it; abandoned for
  that reason, not because anything in it was wrong.
* **Branch:** `wave9/lane-engio-work`. NOT `wave9/lane-engio`, which the main
  checkout took.
* `git diff --stat ae8195b wave9/lane-engio-work` is **12 files, all this
  lane's** — checked before handing the branch over, per the standing rule that
  a lane can silently write into a neighbour's worktree during the churn.

### 8.2 Environment damage, and what was done about it

The session was pinned into **other lanes' worktrees** and its working directory
was silently relocated three times mid-run, twice landing edits in another
lane's tree. Consequences, all cleaned up and verified:

* The branch name `wave9/lane-engio` was taken by the MAIN checkout at `ae8195b`
  (lane ADDR's commit). **This lane's commits are on `wave9/lane-engio-work`.**
* Three early edits landed in the main checkout and were reverted; the two files
  touched (`runtime/types/input/Input.h`, `runtime/src/input/input_win.cpp`) were
  restored byte-identical, verified with `cmp`.
* Two files were quarantined into lane UID's worktree as `*.laneENGIO-leaked.bak`
  and one of its files was patched; both removed and the patch reverted, verified
  by grep.
* One `mingw32-make standalone-debug` ran in lane MOD's worktree. It rebuilt
  their exe from their own unmodified sources and was NOT deployed.

Every measurement in this document was taken with absolute paths after the last
relocation, and every one is gated on this lane's identity line read out of that
run's own log.

---

## 9. Files

| File | What |
|---|---|
| `runtime/src/input/input_device_update.cpp` | the four bodies, the two lifted tables, the registration |
| `runtime/src/input/input_device_update_probe.cpp` | the synthetic-COM-device oracle (`BANISHED_RT_INPUTUPD_PROBE=1`) |
| `runtime/types/input/Input.h` | `State` gains `normalized`/`repeat`/`holdTicks`; the Device tail (+0x11E0..+0x11FF) |
| `runtime/src/input/input_win.cpp` | `PlatformUpdate` exposed as `RtInput_Device_PlatformUpdate` |
| `runtime/src/input/input.cpp` | the probe hook, in a function that actually runs |
| `runtime/src/input/rt_input_port.cpp` | registration + the lane identity line |
| `dx11/src/contextdata.cpp` | `BANISHED_FORCE_DEVICE_LOST=<n>` and the three `[DeviceLost]` lines |
| `standalone/video_interface.{h,cpp}` | the corrected DETOUR-FAILED message + `VideoInterfaceFillDx11Slot` |
| `docs/runtime_port/wave9_lane_engio/` | the two runs' evidence and their (empty) crash logs |
