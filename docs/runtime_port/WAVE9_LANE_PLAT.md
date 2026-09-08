# Wave 9 lane PLAT — the `PlatformStartup` family

Branch `wave9/lane-plat`, base `798bfd3`.
Worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\lane-plat2`
(the lane's FIRST worktree, `…\lane-plat`, was taken over mid-session — see §9).

## 1. The assignment, and what it turned into

Attempt 21's DLL-less boot died at

```
[RtUnresolved] ?PlatformStartup@Application@1@AEAA_NXZ
[RtResolve] MISS "?PlatformStartup@Application@1@AEAA_NXZ" -> NULL
Fault: EXEC of 0000000000000000     (engine_lifecycle.cpp:638)
```

`AEAA` means **private**, not unexported. It is ordinal 3402 at `0x18009fc10`,
confirmed against the shipped export directory with pefile before any body was
written — as was every other mangled name this lane registers.

The lane was asked to scope the whole family rather than port one row. It did,
and then took every row that is on the cutover boot path.

## 2. The family, measured

Nine exports share the name. Status was established by grepping BOTH the address
and the mangled name across `code/`, `engine/`, `runtime/` and `standalone/`
before writing anything — a body that is ported but never registered reads
exactly like an absent one.

| Ord | Export | Retail | Before this lane | After |
|---|---|---|---|---|
| 3401 | `?PlatformStartup@AchievementManager@Achievement@@AEAA_NXZ` | 0x18009eb70 | unported | ported (UNREACHABLE) |
| 3402 | `?PlatformStartup@Application@1@AEAA_NXZ` | 0x18009fc10 | unported — **the wall** | ported |
| 3403 | `?PlatformStartup@Device@Audio@@AEAA_NXZ` | 0x1800de600 | unported | ported |
| 3404 | `?PlatformStartup@Device@Input@@AEAA_NXZ` | 0x1800e1880 | unported | ported |
| 3405 | `?PlatformStartup@Device@Video@@AEAA_NXZ` | 0x1800f8260 | ported (lane VID2, `device_startup.cpp:844`) | unchanged |
| 3406 | `?PlatformStartup@Engine@1@QEAA_NXZ` | 0x1800a2100 | unported | ported (UNREACHABLE) |
| 3407 | `?PlatformStartup@ExternalFactory@Object@@AEAA_NXZ` | 0x1800bf6c0 | unported | ported (UNREACHABLE) |
| 3408 | `?PlatformStartup@MemoryAllocator@System@@AEAA_NXZ` | 0x1800c7c40 | ported (`allocator_win.cpp:129`) | unchanged |
| 3409 | `?PlatformStartup@Timer@System@@CA_NXZ` | 0x1800ca860 | ported (`timer.cpp:318`) | unchanged |

**Four of the nine are on the cutover boot path**, and the path is our own
`Engine::Startup` port: Application at `g_startupFailAt == 6`, Video at 11,
Audio at 12, Input at 13. All four are now closed.

**Three are on no path at all.** Ghidra's xrefs for Achievement, ExternalFactory
and Engine `PlatformStartup` are the export address table and nothing else, and
`Application-x64.exe`'s import table (read with pefile) names none of them.
They are registered because each is two or seven instructions, and they are
reported as UNREACHABLE — never as verified. Their `calls=` will be 0 for a
reason that is known in advance, which is the only kind of `calls=0` that is
not a defect.

## 3. What was ported, and why each extra row was not optional

| Export | Ord | Why this lane had to take it |
|---|---|---|
| `?PlatformStartup@Application@1@AEAA_NXZ` | 3402 | the wall |
| `?WindowProc@Application@1@CA_JPEAUHWND__@@I_K_J@Z` | 4546 | the wall STORES `0x18009f930` into `WNDCLASSW::lpfnWndProc`. At cutover that address is not mapped, so porting only the outer function registers a window class whose procedure is a null dereference on the first message. |
| `??0WideString@System@@QEAA@PEBD@Z` | 514 | the wall's first instruction pair is `LEA RDX,[0x180154858] ; CALL 0x1800cb110` — `WideString className("Banished")`, whose buffer is the class name and the window title. Lane FS ported four of this class's five constructors; nothing called this one until now. |
| `?PlatformStartup@Device@Audio@@AEAA_NXZ` | 3403 | the wall after Video's |
| `?PlatformStartup@Device@Input@@AEAA_NXZ` | 3404 | the wall after Audio's |
| `?OnFocusMessage@Device@Input@@…` | 3275 | installed by 3404 as a live message callback; a null there faults on the first WM_SETFOCUS |
| `?OnLostFocusMessage@Device@Input@@…` | 3276 | same, WM_KILLFOCUS |
| `?OnMouseWheelMessage@Device@Input@@…` | 3277 | same, WM_MOUSEWHEEL |
| `?PlatformStartup@AchievementManager@…` | 3401 | `MOV AL,1 ; RET` |
| `?PlatformStartup@ExternalFactory@Object@@…` | 3407 | `MOV AL,1 ; RET` |
| `?PlatformStartup@Engine@1@QEAA_NXZ` | 3406 | `CoInitializeEx(NULL, COINIT_MULTITHREADED)` then `return true` |

Files: `runtime/src/application/platform_startup_win.cpp`,
`runtime/src/audio/audio_platform_startup_win.cpp`,
`runtime/src/input/input_platform_startup_win.cpp`,
plus the fifth `WideString` constructor in `runtime/src/system/widestring.cpp`.
All registered behind one switch, `BANISHED_RT_PLAT`, from
`runtime/src/application/rt_application_port.cpp`. Cross-namespace on purpose:
what the eleven share is the export name and the boot path, not a namespace —
the shape lane X's `singleton_ctors.cpp` established.

## 4. A correction this lane owes the tree

`runtime/src/input/input_win.cpp` said, of `Input::Device::PlatformUpdate`:

> PlatformUpdate (0x1800E2130) is NOT registered by name — it has no mangled
> export in either the retail or the profile DLL (checked against the export
> directory with pefile before writing a line of this file).

**That is false.** It is `?PlatformUpdate@Device@Input@@AEAAXXZ`, ordinal 3411
at `0x1800e2130` in retail and ordinal 3564 at `0x1800e27b0` in the profile
build. Both were re-checked with pefile against both files before this was
written down. It is the same `AEAA` trap the wall itself was.

The cost was live, not cosmetic: while the NAME was unregistered every by-name
resolve of it fell through to the DLL, and the three Input message callbacks
this lane adds are exactly such callers. The missing `RegisterRuntimeExport` is
added and the comment corrected in place.

## 5. Verification

### 5.1 The instruments

Everything is off by default and each has an effectiveness control and a
negative control.

* **`BANISHED_PLAT_ORACLE=1`** (with `BANISHED_RT_DIFF=1`) turns on three
  comparisons:
  1. **Application::PlatformStartup, 14 fields.** The shipped body runs FIRST on
     a scratch `Application`; the class it registers is read back out of USER32
     with `GetClassInfoW`; its window and class are torn down; then our body
     runs for real and its class is read back the same way. `lpfnWndProc` is
     excluded and reported separately — it MUST differ, and comparing it would
     fail 100% of calls for the one reason that is correct.
     Effectiveness control: `nonTrivial`, the count of compared fields that were
     non-zero on at least one side.
     The teardown is the risky step and every stage of it is checked rather than
     assumed: `DestroyWindow` sends WM_DESTROY to the DLL's own WindowProc, which
     calls `PostQuitMessage(0)` — that WM_QUIT is drained from this thread's
     queue and **the drain's return value is tested**; `UnregisterClassW`'s
     result is tested; and `GetClassInfoW` is then required to FAIL before our
     own `RegisterClassW` is allowed to depend on it. This is why the oracle is
     behind its own switch rather than behind `BANISHED_RT_DIFF` — a teardown
     failure would put the live boot on retail's own `return false` path.
  2. **WindowProc, 10 cases over 6 branches.** Ours and the shipped body are
     driven over the same synthetic message table against a scratch
     `Application` that `Application::_singleton` is temporarily repointed at,
     and both the return value and the one byte either writes (`+0x11`) are
     compared. The scratch's `_running` is seeded to `0x5A`, a value neither
     branch produces, so "untouched" is distinguishable from "written 0".
     Effectiveness control: `branches`, the count of DISTINCT switch branches the
     table reached.
     **Honest limit:** the scratch's callback Set is empty, so the AVL descent
     and the callback dispatch at the top of the body are NOT covered.
  3. **Audio::Device::PlatformStartup.** The shipped body runs on a scratch
     device buffer — XAudio2 permits a second independent engine — and the
     OUTCOME SHAPE is compared (return value, engine present, mastering voice
     present), not the pointers, which are two different COM objects.
     Effectiveness control: whether either side actually got an engine.
* **`RtLanePlatInputFormatProbe`** runs unconditionally under the diff harness
  and needs no switch. It compares MinGW's `c_dfDIKeyboard` / `c_dfDIMouse2`
  against the DLL's embedded blobs at `0x18014bc20` / `0x18014be30`, field by
  field including every one of the 267 object entries, with each entry's GUID
  compared BY VALUE (the pointers live in two modules and can never be equal).
  Effectiveness control: `fields`, which must be 1078.
  Negative control: the same comparator on keyboard-vs-mouse, run every time,
  which must report a mismatch.
* **`BANISHED_PLAT_NEGCTL=1`** perturbs one compared `WNDCLASSW` field
  (`hbrBackground` 6 → 7) and swaps the WindowProc under test for a deliberately
  wrong copy (VK_F11 instead of VK_F10). Both oracles must then report exactly
  one divergence. A control that cannot fire is worse than none.
* **`BANISHED_RT_PLAT=0`** is the lane kill switch, and also the baseline side of
  the residue set-diff — same binary, one flag apart, so the delta is
  self-consistent by construction.

### 5.2 Build identity, and the one prediction that was wrong

The predictions for every number in §5.3 were written down BEFORE the first run
and are archived beside the logs at
`…\scratchpad\PREDICTION.md`, together with the script that produced the
set-diff (`residue_diff.py`) and the raw logs of every run.

One prediction was wrong and it is worth naming: it said the identity line would
read `registered=11`. It reads `registered=10`. The SET is exactly what was
predicted (5 from the Application aggregator + 1 Audio + 4 Input); the 11 was an
arithmetic slip in the prediction, not a missing registration. Nothing else in
P1–P4 missed.



Every number below was read from its own run's log, gated on
`[LanePLAT] rev=PLAT-r5 registered=10`. Not on "I rebuilt", not on a timestamp.

### 5.3 Results

All numbers below come from runs whose own log carries
`[LanePLAT] rev=PLAT-r5 registered=10`, read from that run's log. The tool that
produced the residue set-diff is `standalone/resolve_standalone.cpp`'s
`BANISHED_RT_STRICT` census, **unchanged by this lane from base 798bfd3**, and
both sides of the diff are the SAME BINARY one flag apart.

Every run drove the full harness (`BANISHED_AUTOSTART=1`, autostart → new game →
300 frames → quit) and exited cleanly on its own, which is itself the headline
functional result: **the game now boots, plays and shuts down on OUR window
class and OUR window procedure**, with `Engine::Engine::~Engine calls=1`.

#### Residue set-diff — P1 predicted exactly, and it landed exactly

```
base(BANISHED_RT_PLAT=0) = 207   mine = 206

REMOVED by the lane (in base, not in mine): 3
   - ?PlatformStartup@Application@1@AEAA_NXZ
   - ?PlatformStartup@Device@Audio@@AEAA_NXZ
   - ?PlatformStartup@Device@Input@@AEAA_NXZ

ADDED by the lane (in mine, not in base): 2
   + addr:0x18014bc20
   + addr:0x18014be30
```

The three removed are the three predicted. The two added are the two predicted:
they are the DIDATAFORMAT probe's own address reads, diagnostic scaffolding whose
only caller is the probe, which does not run at cutover.

**The COUNT moved by one (207 → 206) while the CONTENT moved by five.** That is
exactly why the wave asks for a set-diff and not a count; a lane reporting "−1"
here would have been reporting a number that is true and says nothing.

Two negative facts that were predicted and held:
* `?PlatformUpdate@Device@Input@@AEAAXXZ` did NOT appear as an addition, which is
  what confirms §4's newly added name registration actually took.
* the seven registrations that nothing calls (WindowProc, Achievement,
  ExternalFactory, Engine, and the three Input callbacks) moved nothing, because
  they were never on the list. A diff of 10 would have meant the census counts
  registrations rather than resolutions.

#### Diff-harness counters — `BANISHED_RT_DIFF=1`

```
Application::Application::PlatformStartup   calls=1  worked=1  divergences=0
System::WideString::ctor(const char*)       calls=1  worked=1  divergences=0
Audio::Device::PlatformStartup              calls=1  worked=1  divergences=0
Input::Device::PlatformStartup              calls=1  worked=1  divergences=0
```

Exactly P2. No site reports `calls=0` beside a non-zero `worked=`.

#### `BANISHED_PLAT_ORACLE=1`

```
[PlatOracle] compared=15 differing=1 nonTrivial=10 classBrushFromWindow ours=6 theirs=9437201;
[PlatProbe]  WindowProc cases=10 differing=0 branches=6
[PlatOracle] Audio::Device::PlatformStartup ours=ret=1 engine=1 voice=1 theirs=ret=1 engine=1 voice=1
[PlatProbe]  DIDATAFORMAT keyboard_mismatch=0 mouse_mismatch=0 fields=1078 |
             NEGATIVE-CONTROL(kbd vs mouse) mismatch=2 (fired, as it must)
```

* **14 of 15 compared fields agree**, including `classInfoOk`, `style`,
  `cbClsExtra`, `cbWndExtra`, `hInstance`, `hIcon`, `hCursor`, the registered
  `hbrBackground` CONSTANT (read out of retail's own `.rdata` at 0x180161780 and
  compared against our literal), `windowOk`, the window's style, exstyle, width
  and height, and the class name. `nonTrivial=10`, so this is not ten zeros
  agreeing with ten zeros.
* **`fields=1078`** is the predicted number exactly: 2×5 header comparisons plus
  (256 + 11)×4 object comparisons. MinGW's `c_dfDIKeyboard` and `c_dfDIMouse2`
  are field-for-field identical to the DLL's embedded blobs, GUIDs compared by
  value. The substitution the Input port makes is proven, not asserted.
* **The one disagreement is `hbrBackground` as READ BACK, and §7.2 records
  everything measured about it.**

#### `BANISHED_PLAT_ORACLE=1 BANISHED_PLAT_NEGCTL=1` — both controls fire

```
[PlatOracle] compared=15 differing=2 ... style ours=2 theirs=0; classBrushFromWindow ...
[PlatProbe]  WindowProc NEGATIVE-CONTROL cases=10 differing=1 branches=6
             msg=0x7 wp=0x0 ours=(0,0) theirs=(0,1);
```

The class oracle catches the perturbed `style` (CS_HREDRAW = 2 against retail's
0). The WindowProc probe catches the perturbed `_running` byte on WM_SETFOCUS.

**The FIRST version of the WindowProc control could not fire, and the probe
caught it.** That version swallowed VK_F11 instead of VK_F10, so on the
`WM_SYSKEYDOWN wp=VK_F10` case the wrong body fell through to `DefWindowProcW`
— which also returns 0 for that message, making the perturbation invisible. It
reported `differing=0` on the run whose entire purpose was to disagree. That is
a fact about the SUBJECT and not only about the control, and it is recorded as a
limit in §7.3: `branches=6` counts branches REACHED, not branches
DISCRIMINATED.

## 6. What this lane DECLINED, with re-derived blockers

Nothing in the family was declined. The two rows the assignment expected might
be declined — Audio (3403) and Input (3404) — were taken, and re-deriving their
blockers is what made that possible:

* Audio's recorded shape ("COM vtable calls into an unreachable device") is
  true and is not a blocker: the two GUIDs identify XAudio2 2.7 unambiguously,
  the vtable slots are pinned by argument arity at both call sites, and every
  failure path returns TRUE, so a machine without the redistributable is a
  supported outcome rather than a boot failure.
* Input's blocker turned out to be **wider than recorded, and then solvable**.
  The recorded worry was DirectInput COM plus three unexported callbacks. In
  fact the three callbacks ARE exported (3275–3277) and so is `PlatformUpdate`
  (3411, mis-recorded in the tree — see §4). The genuinely unrecorded blocker is
  a different one: `DirectInput8Create`'s `HINSTANCE` comes from `0x1801fc680`,
  an **unexported** `.data` global whose only writer is `0x18009fbb0`, called
  twice from `0x1800803ec` — the CRT's DllMain dispatcher, with a `CMOVZ` that
  stores only on `DLL_PROCESS_ATTACH`. It is **Runtime-x64.dll's own HINSTANCE**,
  and at cutover there is neither a module nor an export name the mint could
  cover. The port passes `GetModuleHandleW(nullptr)` instead, which is a real
  behavioural difference and is documented at the call site rather than buried.

## 7. Honest limits

### 7.1 `Application::PlatformStartup` has no oracle on the live path
and cannot: the class name is a hardcoded literal, so any replay's
`RegisterClassW` fails with `ERROR_CLASS_ALREADY_EXISTS`. The
`BANISHED_PLAT_ORACLE` comparison is real but it is a SEPARATE, deliberately-run
experiment; an ordinary `BANISHED_RT_DIFF` run exercises none of it.

### 7.2 UNEXPLAINED: the class background brush reads back differently

This is the lane's one open disagreement and it is stated in full rather than
smoothed over.

WHAT WAS MEASURED, all in the same run:

| Observation | ours | shipped |
|---|---|---|
| the registered CONSTANT (retail's own `.rdata` at 0x180161780, read live) | 6 | 6 — **equal** |
| `GetClassInfoW(...).hbrBackground` | `0x6` | `0x900011` |
| `GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND)` | `0x6` | `0x900011` |
| a THIRD class registered from OUR module with the same constant 6 | `0x6` | — |
| is `0x900011` any `GetSysColorBrush(0..30)`? | **no** | |
| `GetSysColorBrush(COLOR_WINDOW)` on this machine | `0x1000d2` | |

So: both sides pass the same value (proven by comparing against retail's own
constant, not by reading our source); the two readback APIs agree with each other
within each side, so it is not a `GetClassInfoW` quirk; the shipped class holds a
real GDI handle that is not a system-colour brush; and a second registration from
our own module with the identical constant reads back the raw 6, so it does not
track the value.

**I could not explain it, and this lane does not pretend to.** What can be said:

* the port passes what retail passes — that is the part the port controls, and it
  is measured equal, so this is not a transcription error in the WNDCLASSW fill;
* every other class and window observable is identical, including both style
  words and the window's size;
* **the field is unreachable in both implementations.** `hbrBackground` is used
  by `DefWindowProc`'s WM_ERASEBKGND handling, and BOTH window procedures — the
  shipped one at 0x18009f930 and the transcription here — return 0 for
  WM_ERASEBKGND without calling `DefWindowProcW` (0x18009f9b0 → 0x18009f9f0).
  Nothing ever paints with it;
* behaviourally, the game boots, runs 300 frames and exits cleanly on our class.

The oracle deliberately still reports this as `differing=1` rather than
suppressing it. Anyone re-running `BANISHED_PLAT_ORACLE=1` will see the same one
divergence, with the whole measurement printed beside it.

### 7.3 The WindowProc probe covers less than `branches=6` suggests
* the scratch Application's message Set is EMPTY, so the AVL descent, the
  `CALL [node+8]` dispatch, and the "callback claimed the message, return 0" path
  are covered by NO oracle;
* the `WM_SYSKEYDOWN` / VK_F10 branch is **indistinguishable from the default
  path by anything the probe can observe** — `DefWindowProcW` returns 0 for that
  message too, which is how the first negative control failed to fire (§5.3).
  `branches` counts branches REACHED, not branches DISCRIMINATED.

### 7.4 `Input::Device::PlatformStartup` has no oracle at all
A replay would create a second `IDirectInput8` with two more devices, Acquire
them against the same window, and re-run three `RegisterMessageCallback` calls
into the LIVE Set. Its `RT_DIFF_SHADOW` is a description, not a comparison. The
one substitution inside it that CAN be proven — the data formats — is proven
(1078 fields, negative control fired).

### 7.5 The `GetModuleHandleW(nullptr)` substitution is unverifiable by comparison
by construction: the value it replaces (Runtime-x64.dll's own HINSTANCE, from an
unexported global written in that DLL's DllMain) does not exist at cutover. The
claim that DirectInput does not care which module handle it is given is an
argument, not a measurement.

### 7.6 Three registrations are never executed
Achievement, ExternalFactory and Engine `PlatformStartup` have no caller in this
process. Their `calls=` is 0 for a reason known in advance, which is the only
kind of `calls=0` that is not a defect — but it does mean three of the eleven
bodies here have never run.

### 7.7 The `Audio::Device` layout is claimed only at +0xa88 and +0xa90
No struct was invented for `runtime/types/audio/Audio.h`; the two offsets carry
the instruction that pins them. The 0x1298 allocation size comes from
`Engine::Startup` and is the only other extent fact in evidence. The Audio oracle
compares outcome SHAPE (return value, engine present, voice present), not COM
pointers, which are two different objects.

### 7.8 The DLL-less cutover boot was NOT run by this lane
That is the integrator's. §8 says what to expect.

### 7.9 The Audio result is machine-dependent
`engine=1 voice=1` on both sides means this machine HAS the XAudio2 2.7
redistributable. On a machine without it both sides would report
`engine=0 voice=0`, the comparison would agree trivially, and the effectiveness
control (`worked`) would go to 0 to say so.

## 8. What the integrator should expect from the cutover boot

The four `Engine::Startup` `PlatformStartup` calls at `g_startupFailAt` 6, 11,
12 and 13 should all now resolve to ported bodies, so the boot should pass
`Engine::Startup` and stop at whatever the NEXT unported export is, rather than
at `EXEC of 0` in `engine_lifecycle.cpp:638`.

Two specific things to watch, because they are the parts of this lane that
cutover exercises differently from a live run:

* the window is created by OUR `RegisterClassW` with OUR `WindowProc`, so any
  message the game relies on now goes through
  `platform_startup_win.cpp:RtApp_WindowProc`. A hang at the first frame with a
  visible window is that code, not the renderer.
* `Input::Device::PlatformStartup` calls `DirectInput8Create` with the exe's
  module handle. If it fails there, retail's own error text
  (`InputDevice: The application failed to create a DirectInput interface.`)
  will be set and `Engine::Startup` will end at `g_startupFailAt == 13`.

## 9. A stray file found in this worktree

`runtime/src/engine/mainloop_video.cpp` was present and UNTRACKED in this
worktree at first build, and it does not compile (`reference to 'Filename' is
ambiguous` at :492, because `RtSystem::Filename` and the forward declaration in
`runtime/types/engine/../common.h` are both visible). It is not this lane's
work — this session was re-pointed by the harness into four other lanes'
worktrees during setup, and the traffic evidently went both ways.

It was **copied to the scratchpad and moved aside, not deleted**:
`…\scratchpad\STRAY_mainloop_video.cpp` and `…\scratchpad\mainloop_video.cpp.stray`.
Whoever owns it should collect it; it needs a `RtSystem::Filename` qualification
at :492 before it will build.

## 10. Worktree interference during this session — three separate incidents

Recorded because the next lane will hit the same thing and because two of the
three could have silently corrupted a result.

1. **A stray untracked file** (§9) appeared in this lane's worktree from another
   session and broke the build.
2. **Six tracked files were reverted on disk** to base `798bfd3` — `Makefile`,
   `rt_application_port.cpp`, `input_win.cpp`, `widestring.cpp`, `widestring.h`
   and `Application.h` — WHILE the lane was running measurements. This was
   caught because a system notice showed `Application.h` back at `_unknown08`,
   and it was harmless only by timing: the revert landed AFTER the last build,
   so every number in §5.3 came from a binary built from the correct sources.
   **That was luck, not method.** The files were restored from this lane's own
   commit, rebuilt, and **every measurement in §5.3 was then re-run from the
   restored tree and reproduced identically** — which is the only reason they
   are quoted here at all.
3. **The worktree was checked out onto another lane's branch entirely**
   (`wave9/lane-rres2`), removing this lane's uncommitted r3–r5 oracle work from
   disk. It survived only because it had been copied to the scratchpad. The lane
   finished in a second, detached worktree (`lane-plat2`) created from its own
   commit.

The practical lesson, beyond "commit early": **a measurement is only trustworthy
if it is re-run after the tree is known-good.** Incident 2 produced no wrong
number, but nothing in the workflow would have caught it if the revert had
landed one minute earlier.
