# Wave 4 — Lane H: the `BANISHED_AUTOLOAD` save/load harness

Ground-truthed 2026-09-06 on branch `wave/upgrade-path-complete` (base `562d2ff`), built and run
from a private worktree at `C:/code/GIT/laneH` so concurrent `runtime/src/io/` lanes could not
break the build. Every number below came from a run of the debug standalone exe
(`mingw32-make sa-deploy`, `-O0 -g`) against `C:/GOG Games/Banished`.

**Result: closed.** `BANISHED_AUTOLOAD=Save9` and `BANISHED_AUTOLOAD=Auto` now load, build the
world, and reach gameplay unattended. Two defects were behind it, one of them the long-standing
"load-path wild write", which turns out not to be a wild write at all. The four Wave 3 functions
that lived only on this path are now exercised, with `calls=` and `divergences=` for each.

---

## 1. The repro, and what it actually was

Command (debug exe, deployed 2026-09-06 20:42):

```
BANISHED_AUTOLOAD=Save9   # and BANISHED_AUTOLOAD=Auto
```

`tools/boot_state.sh` reported `RUNNING`, the process stayed alive and pumped frames, and
`standalone_debug.log` showed nothing after the Load click but `[MenuDLT::Draw]` / `[UIFade]`
lines forever — the symptom PLAN.md §5 records.

The driver was not at fault. `standalone_error.log` showed it doing its whole job:

```
[AutoStart] menu settled (mainMenu=…) — injecting Load click
[AutoStart] save list ready (12 entries) — selecting #1 ("Save9") and injecting Load click
[AutoStart] AUTOLOAD sequence fired — load operation queued; driver done
```

and `standalone_debug.log` showed `MainMenuDialog::OnCommand … -> OpenLoad`, the SaveLoad.rsc
resource loading, and then — the tell — `[MessageDialog::Slot6_Stub]` / `Slot7_Stub`. A
`MessageDialog` had been constructed and shown.

**Root cause A: `UI::UserInterface::ShowDialog` is a nested modal loop, and nothing answered it.**

`ShowDialog` (Runtime `0x18011fd10`, decompiled) ends in

```c
do {
  … walk this_00's dialog list looking for the one we were given …
  bVar3 = Engine::MainLoop::Engine_MainLoop_Frame(Engine::MainLoop::_singleton, true);
} while (!bVar3);
```

It re-enters `MainLoop::Frame` until the dialog is removed. So the process keeps drawing
`MenuDLT::Draw` at full frame rate while blocked — which is exactly why the stall reads as "the
menu never advances". `AutoStart::TickMenu` is called from inside that nested loop, but its phase
is already `Done`, so no driver can reach the modal. This is the same shape as
`ModDialog::Apply`'s `BANISHED_MODAL_ANSWER` escape (`code/Game/Dialog/moddialog.cpp`).

**Which modal, and why it is faithful.** Instrumenting `SaveLoadDialog::DoLoad` gave the answer
directly:

```
[SaveLoadDialog::DoLoad] selectedIdx=1 saveInfo=… save-mod-count=1 gameWorld=0000000000000000
[SaveLoadDialog::DoLoad] installed mod count=0
[SaveLoadDialog::DoLoad]   save mod[0] "Debug Menu" CheckMod=1
[SaveLoadDialog] entering MODAL ShowDialog "Dialog/SaveLoad.rsc:modsMissing"
```

Every save in `~/Documents/Banished/Save/` records one mod, `"Debug Menu"`, and there is no `.pkm`
anywhere under the game directory or the documents directory — so
`Save::SaveLoad::CheckMod` (Runtime `0x1800c1090`) correctly returns 1 (missing) and retail
correctly puts a `modsMissing` prompt on screen. **Nothing was broken about the prompt; the
unattended harness simply had no way to answer it.** `gameWorld=0` also confirms the *other*
modal on this path (`confirmLoad`, shown only when a game is already running) was never involved.

This is why the stall was not save-specific and reproduced with `BANISHED_RT_OBJECT=0`: it has
nothing to do with any port, and everything to do with the save set on this machine naming a mod
that is not installed.

---

## 2. Root cause B — the "load-path wild write" is a `String` destructor on an `InternalBasePtr`

Once the modal was answered, the load proceeded and immediately hit the second wall: an
`ACCESS_VIOLATION` inside `System::MemoryAllocator::Allocate` (Runtime `+0xC70A2`), with the
signature recorded in `reference_load_wild_write_high_dword_7fc.md` — a valid heap pointer whose
high dword is the true value **plus exactly 0x7FC**. Two more data points, on top of that note's
three:

| run | real block (R8) | fault address | delta in high dword |
|---|---|---|---|
| Save9 | `00000228_4FCEF498` | `00000A24_4FCF07E0` | +0x7FC |
| Auto  | `0000028E_5CB6A2D8` | `00000A8A_5CB73A90` | +0x7FC |
| Save6 | `00000237_CD6D97E0` | `00000A33_CD6DBA68` | +0x7FC |

**`Save6.sav` (2.8 MB, 2026-08-29) crashes identically**, so the note's framing — "loading a large
hand-built town (3,500 road tiles)" — was a coincidence of which saves had been tried. It is every
load whose save names a missing mod.

### The mechanism, read out of the allocator

`MemoryAllocator::Allocate` packs the block size into the low 41 bits of the boundary tag
(`size = tag & 0x1ffffffffff`, in 4-byte units) and derives the next tag as
`block + (tag & 0x1ffffffffff) * 4`. The faulting registers say exactly what was wrong:

```
RDI = 000001FF_000008A2      ; tag & 0x1ffffffffff  — bits 32..40 all set
R12 = 000001FF_FFFFFFFF      ; the 41-bit size mask
```

`0x1FF << 32`, times 4, is `0x7FC` in the high dword. The corruption is nine set bits in one
qword.

`BANISHED_REDZONE=watch BANISHED_REDZONE_WATCH=1` then dated it precisely — the watchdog was clean
every millisecond until the frame that handled the Load click, and reported the corruption
immediately after `[AutoStart] AUTOLOAD sequence fired`:

```
[REDZONE-HEAP] ==== corruption at checkpoint 'watchdog' (seq=0) ====
[REDZONE-HEAP] range 0: bad block @…8fd8 tag=0x00001fff00000a98
[REDZONE-HEAP] preceding block @…8fb8 raw: tag=…08 user[0]=000000010000000d
               user[1]=7661536c6562616c user[2]=1e2a00656d614e65     ("labelSaveName")
```

Two observed corrupt tags, `0x00001fff…` and `0x000021ff…`, are `0x2000 - 1` and `0x2200 - 1` in
the high dword. **The tag was decremented by one**, and the borrow propagated down through bits
32..40.

### The line

`code/Game/Dialog/saveloaddialog.cpp`, in `DoLoad`'s mod-warning block:

```cpp
uint64_t showPtr = 0;
fn_InternalBasePtr_Ctor(&showPtr, warnDlg);   // warnDlg = MemAlloc(0xC0, 8)
…
fn_String_Dtor(&showPtr);                     // "cleanup the extra ptr we built"
```

`System::String::~String` does `*(int*)(data-4) -= 1; if (== 0) MemFree(data-8)`. For a real
`String` that is the refcount in the string header. For a `MemAlloc` **user** pointer, `data-8` is
the allocator's boundary tag and `data-4` is **its high dword**. So the line decremented a live
tag by one on every mod-warning shown, and left the block's size field claiming ~8.7 TB. The next
allocation that walked past it faulted.

Retail (`0x1400b61e0`) does no such thing — it copies the InternalPtr into a scratch slot, hands
`ShowDialog` the copy (it takes its argument by value and writes through the slot it is given),
and releases the original with `InternalBasePtr::DeletePtr`:

```c
local_c8 = local_res18;
iVar3 = UI::UserInterface::ShowDialog(*(UserInterface **)(param_1 + 0x10), &local_c8);
Object::InternalBasePtr::DeletePtr((InternalBasePtr *)&local_res18);
```

The fix restores that shape and deletes the `String` destructor. This also explains the note's
last paragraph: *"Mounting a `.pkm` made one load succeed. Treat that as heap-layout luck"* — it
was not luck. Mounting the mod makes `CheckMod` return 0, which skips the whole mod-warning block,
which skips this line.

---

## 3. Root cause C — the load path never ran `GameContainer_EnsureLists`

With A and B fixed the world built and `GameUpdateLoop::Update` ran — and faulted `READ of 0` at
Runtime `0x18009E6F4` (`AchievementManager::Update`), from `gameupdateloop.cpp:1470`. That is the
second open recorded in memory ("achievement instances null (mgr+0x18) after a load").

It is a missing init on this path, not a `Persist` defect. Retail calls `GameContainer_Init`
(`0x140099db0`, from `FUN_140001120`) **once at app init**, and that is what hands the achievement
list to the manager (`AchievementManager::SetAchievements`). Standalone has no master
`ExternalList`, so that init was replaced by `GameContainer_EnsureLists()` called from whichever
dialog needs it first — New Game (`newgamedialog.cpp:99`), Scenario (`scenariodialog.cpp:81`) and
Achievements (`mainmenudialog.cpp:299`). **The Load path had no such call**, so a loaded game
reached `GameUpdateLoop::Update` with `mgr+0x18` still null.

Fix: call `GameContainer_EnsureLists()` at the top of `MainMenuDialog_OpenLoad`, exactly as
`MainMenuDialog_OpenAchievement` already does. Verified in the log:

```
[GameContainer] AchievementManager::SetAchievements(mgr=…, list=…) -> count=36
```

---

## 4. The fix

| File | Change |
|---|---|
| `code/Game/Dialog/saveloaddialog.cpp` | **(a)** removed `fn_String_Dtor(&showPtr)` and restored retail's copy-then-`DeletePtr` shape (root cause B). **(b)** added `SaveLoad_ModalShowDialog()`, which answers the save/load modals with 2 when a harness env var says the run is unattended (or `BANISHED_MODAL_ANSWER=<n>` is set), and otherwise logs loudly before entering the modal (root cause A). **(c)** added a `DoLoad` trace: selected index, the save's mod list with each `CheckMod` result, and the installed mod list. |
| `code/Game/Dialog/mainmenudialog.cpp` | `GameContainer_EnsureLists()` at the top of `MainMenuDialog_OpenLoad` (root cause C). |

The unattended predicate is the same six variables as `HarnessGuardUnattendedBoot`
(`standalone/main.cpp:195`): `BANISHED_AUTOSTART`, `BANISHED_AUTOQUIT`, `BANISHED_AUTOLOAD`,
`BANISHED_AUTOTUTORIAL`, `BANISHED_AUTOMENU_BUTTON`, `BANISHED_PAUSEMENU_BUTTON`. An **attended**
run is unchanged: it still gets the real prompt. `BANISHED_MODAL_ANSWER` overrides in either
direction (e.g. `=0` to prove the decline path).

Known cosmetic residue, stated rather than hidden: when the modal is answered without showing it,
the warning `DialogInstance` that was already allocated and constructed is never added to the UI
and never freed. That is a per-declined-mod leak of 0xC0 bytes on an unattended run only, and it
is the same trade `ModDialog::Apply` already makes.

---

## 5. Verification

All runs: debug standalone, one process at a time. The first four were run in the shared game
directory before another lane began deploying into it (see §5c); their logs carry this lane's own
`[SaveLoadDialog::DoLoad]` trace and `(build default 0)`, and their RtDiff site lists contain none
of the concurrent IO ports, so they are this lane's binary. **Runs 3 and 4 of §5b re-establish the
same result on a binary verified by md5 before and after, in an isolated directory** — treat those
as the authoritative numbers.

| Run | Result |
|---|---|
| `BANISHED_AUTOLOAD=Save9` | World built, ran 154 s of simulation (citizens, burials, plague census, 218,463 `StorageDisplayComp::Draw` lines), `standalone_crash.log` empty, process alive at kill. |
| `BANISHED_AUTOLOAD=Auto` | World built, ran 180 s of simulation, `standalone_crash.log` empty, process alive at kill. |
| `BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1` + pause-menu self-quit | Clean exit, diff summary printed: **54 sites, 18,510,887 calls, 60 divergences in 2 sites**. |
| `BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1 BANISHED_RT_INTERNAL_SHADOW=1` + self-quit | Clean exit: **58 sites, 18,864,110 calls, 65 divergences in 2 sites**. Re-run isolated and md5-verified (§5b run 4): **58 sites, 18,728,761 calls, 48 divergences in 3 sites**. |

The divergent sites are the Wave 1 residuals, unchanged in character:
`Math::Vector3::FromYawPitch` (18 / 9) and `Math::Transform::FromYawPitchOffset` (42 / 47 / 38) —
the 1-ULP sin/cos-fed sites WAVE1.md records. The isolated re-run adds a **third** such site,
`Math::Vector3::Heading` at **1 divergence in 47,850 calls**, which is the same trig-fed family
(`Heading` is atan2-based) and was simply below the threshold of appearing in the earlier runs;
the per-site counts move run to run because the call volumes do. Stated as a third site rather
than folded in, because "2 sites" was what the earlier runs showed and the difference should not
be quietly absorbed. Nothing outside that family diverged.

Self-quit knobs used for the diff runs (needed because `RtDiffSummary()` prints from `WinMain`,
not `atexit`): `BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_BUTTON=6`.

### The four Wave 3 functions, now exercised

From the `BANISHED_RT_INTERNAL_SHADOW=1` run's `[RtDiff]` summary:

| Function | `calls=` | `divergences=` |
|---|---|---|
| `Object::ExternalFactory::GetPathForReferenceId` | **89,733** | 0 |
| `Object::ExternalFactory::GetReferenceId` | **1** | 0 |
| `Object::InternalFactory::ClearPersistState [shadow]` | **1** | 0 |
| `Object::InternalFactory::ResetPersistState [shadow]` | **1** | 0 |

All four are non-zero for the first time. Two caveats worth stating plainly rather than rounding
off:

* `GetReferenceId`, `ClearPersistState` and `ResetPersistState` are **one call each**. That is
  enough to retire "registered-but-never-executed"; it is not a volume argument, and it does not
  by itself flip the `BANISHED_RT_INTERNAL_TABLE` tier's NO-GO verdict — WAVE3.md's two residual
  issues with `ResetPersistState` (the stated non-byte-faithful encoding-factory deviation) are
  untouched by this lane.
* The two `PersistState` rows are **shadow-mode** measurements: the DLL still does the real work
  and our port runs alongside for comparison. That is what the shadow oracle is for, and it is the
  same standing WAVE3.md gave `Internal::Internal` / `~Internal` (which this run reproduced at
  84,172 calls each, 0 divergences).

---

## 5a. THE MACHINE-STATE FACT THAT CAUSED THIS — read this before touching save/load or achievements

**Both of this machine's mods live in a holding pen, not in the mods folder:**

```
C:\GOG Games\Banished\WinData\_mods_off\betterFields.pkm   (10101 bytes)
C:\GOG Games\Banished\WinData\_mods_off\DebugMenu.pkm      ( 5789 bytes)
```

The mods folder is `WinData\` itself (loose files -> `.pkg` -> `.pkm`). `_mods_off\` was created
2026-09-05 18:47. The timeline is the whole story:

| when | what |
|---|---|
| Sep 5 18:15 | `Auto.sav` written |
| Sep 5 18:30 | `Save9.sav` written |
| Sep 5 18:44 | `Save10.sav` written |
| Sep 5 18:47 | both `.pkm` moved to `_mods_off\` |

Every save on this machine was made with the mods mounted; every load attempted since 18:47 has
run without them. That single fact explains both properties PLAN.md §5 recorded and could not
account for — the stall is not save-specific (it hits `Save9` and `Auto` alike) and it reproduces
with `BANISHED_RT_OBJECT=0` (it predates the port).

**This is not discoverable from the code.** `find … -maxdepth 2 -iname '*.pkm'` misses these,
because `_mods_off\` is itself at depth 2 and the files at depth 3 — that is exactly how this
lane initially concluded "no `.pkm` anywhere".

### The standing conflict: achievements and save-loading cannot both work on this save set

`docs/waves/achievements_wave.md:20-30`: `FileSystem::_singleton+0x38` is the user-mod count and
`AreUserPackFilesEnabled()` is literally `count > 0`, so **retail silently disables all achievement
evaluation whenever any `.pkm` sits in `WinData`.** So:

* achievement runs REQUIRE the mods out of `WinData\`;
* loading any save written on this machine before Sep 5 18:47 REQUIRES `DebugMenu.pkm` in.

Both cannot hold at once for this save set. This is a standing harness constraint, not a one-off,
and it has now cost the project time twice. **After this lane's fix a mod-mismatched load no longer
stalls** — it logs the mismatch and proceeds — so the achievements configuration (mods out) is the
one that can stay, which is how this lane left the directory. See §7.

### The function that does the comparing

Lane D correctly ruled out `IO::Modification::Find` (no internal callers in the DLL). The
comparing function is **`Save::SaveLoad::CheckMod`, retail `0x1800c1090`**, called only from the
exe's `SaveLoadDialog::DoLoad` (`0x1400b61e0`). Decompiled, it:

1. builds `IO::FileSystem::GetDataDirectory() + <the save's recorded mod path string>` and runs
   `System::Filename::AdjustDelimiters` on it;
2. linear-searches `IO::Modification::_singleton` — count at `+0x10`, array base at `+0x8`, stride
   `0x80` — comparing the `String` at entry `+0x58` by length then `_stricmp`;
3. returns **1** if no entry matches (→ `modsMissing`), else compares entry `+0x2c` against the
   save record's `+0x10` (mismatch → **4**, `modsChanged`) and entry `+0x1c` against `+0x18`
   (mismatch → **3**, `modsVersion`), and finally returns **2** (`modsOutOfDate`) only when the
   save's `+0x14` is below `0x1000400`, else **0**.

Our `DoLoad` trace prints exactly this pairing, which is how the mismatch was identified:

```
[SaveLoadDialog::DoLoad] installed mod count=0
[SaveLoadDialog::DoLoad]   save mod[0] "Debug Menu" CheckMod=1
```

and with `DebugMenu.pkm` restored to `WinData\`:

```
[SaveLoadDialog::DoLoad] installed mod count=1
[SaveLoadDialog::DoLoad]   installed[0] path="C:\GOG Games\Banished\WinData\DebugMenu.pkm"
[SaveLoadDialog::DoLoad]   save mod[0] "Debug Menu" CheckMod=0
```

`betterFields.pkm` is **not** in any of these saves' recorded mod lists — only `"Debug Menu"` is —
so only `DebugMenu.pkm` is needed to match, and mounting `betterFields.pkm` would add content the
saves were never written against.

---

## 5b. The decisive A/B: same binary, mod in vs mod out

Asked for by the team lead after the user pointed out that a save cannot be loaded with a mod
disabled that was enabled when the save was written. **The hypothesis is CONFIRMED** — and it is
the trigger for all three defects above, not an alternative to them.

Run in an ISOLATED game directory (see §5c) with the deployed exe's md5 checked before *and* after
each run, and every `BANISHED_*` variable cleared before each launch. "Control" is my base
`562d2ff` with **none** of this lane's changes; "fixed" is the same tree plus the two files in
commit `ef0debc`. All four runs used `BANISHED_AUTOLOAD=Save9`.

| # | binary | `DebugMenu.pkm` | outcome |
|---|---|---|---|
| 1 | control `397e84fc…` | **out** | `CheckMod=1` → heap-tag corruption crash, `tag=0x000021ff…`, the +0x7FC signature |
| 2 | control `397e84fc…` | **in** | `CheckMod=0` → **loads.** STAGE C, 655 plague censuses, 212,382 storage draws, no crash, alive at 155 s |
| 3 | fixed `e57096aa…` | **out** | `CheckMod=1`, modal answered → **loads.** SetAchievements ran, 747 censuses, 242,155 draws, no crash, alive at 176 s |
| 4 | fixed `e57096aa…` | **out**, `RT_DIFF=1 RT_INTERNAL_SHADOW=1`, self-quit | clean exit; **58 sites, 18,728,761 calls, 48 divergences in 3 sites** |

Run 2 is the lead's test and it settles the question: **putting the mod back makes the save load on
code that has none of this lane's fixes.** Run 1 is the same binary with the mod removed, so the
mod set is the only variable.

### Why this does not make the three defects go away

Mounting the mod does not fix anything; it *skips* the code that is broken. With `CheckMod=0` the
whole mod-warning block is never entered, so the modal (A) is never shown and the `String`
destructor (B) never runs. And defect C is masked for a different reason: `.pkm` present means
`AreUserPackFilesEnabled()` is true, which disables achievement evaluation, so
`AchievementManager::Update` is never called and cannot fault on a null instance list. Run 2's log
has no `SetAchievements` line at all and still runs fine — that is the masking, visible.

So there are two working configurations, and they are not equivalent:

* **mod in, unfixed code** — loads, but achievements are dead by design, and the moment any save
  references a mod you do not have, all three defects are live again.
* **mod out, fixed code** (runs 3 and 4) — loads, achievements initialise (`count=36`), and a
  mod mismatch is now a logged line instead of a stall or a corrupted heap.

The second is the configuration this lane leaves behind, because it is the only one in which the
achievements wave and save-loading can both work.

One more observation from run 1 worth recording, because it explains inconsistent earlier reports:
**the unanswered modal does not always block.** `ShowDialog`'s loop exits as soon as the dialog is
no longer in the UI's list, so depending on whether the warning dialog actually got added it either
spins forever (the reported stall) or returns immediately and then hits defect B (run 1's crash).
Same root cause, two presentations. A repro that "crashes instead of stalling" is not a different
bug.

---

## 5c. Two process-hygiene facts this lane learned the hard way

**1. Another lane was building and running the game in the shared directory.** Between 21:20 and
21:45 `C:\GOG Games\Banished\reimpl-x64.exe` was repeatedly overwritten by a `make sa-stress`
perf build from the main worktree (26,373,976 bytes, `55bbf702…`), and a second process was writing
into the same `standalone_*.log` files — two `STAGE C` lines with different heap bases in one log.
Two of this lane's experiments measured that foreign binary before it was caught. The tells:
`[PerfX] … (build default 1)` instead of `0`, `[FrameLimiter] disabled … via BANISHED_FPS_CAP`,
`BANISHED_SEED=31337`, and an `injecting New Game click` where `BANISHED_AUTOLOAD` was set — all of
which are `stress_harness.cpp`'s `SetIfUnset` block, i.e. a `BANISHED_STRESS` run.

**Never trust the deployed exe's mtime as identity.** Take its md5 immediately before launch and
again after the run; if it changed, discard the result.

**2. Run in a private game directory when other lanes are active.** The whole dependency is four
files plus the packages, ~180 MB, and it gives you your own logs, your own exe and your own
`WinData` (so you can mount or unmount a mod without touching anyone else's achievement runs):

```sh
mkdir -p /c/laneH_game/WinData
cp "/c/GOG Games/Banished/"*.dll                 /c/laneH_game/
cp "/c/GOG Games/Banished/WinData/"*.pkg         /c/laneH_game/WinData/
cp <your build>/standalone/build/reimpl-x64.exe  /c/laneH_game/
# then launch with the working directory set to /c/laneH_game
```

Saves live in `~/Documents/Banished/Save` and are shared, which is what you want. `IO::Persistence`
is HKCU-registry-backed and is also shared — that is the one thing this recipe does not isolate.

---

## 6. What this closes and what it does not

**Closes**

* PLAN.md §5's `Save9.sav` row: the harness works, and both `Save9` and the documented `Auto` load
  to gameplay unattended.
* WAVE3.md finding E.
* The `reference_load_wild_write_high_dword_7fc` open, with a mechanism rather than a signature.
* The "achievement instances null after a load" open.

**Still open, seen but out of this lane's scope**

* **Quit-to-menu teardown after loading `Auto.sav` crashes.** With
  `BANISHED_PAUSEMENU_BUTTON=6` the world teardown faults in the allocator via
  `EntityList_Shutdown → WorkComponent_OnRemoveVSlot → WorkComponent_OnRemove_CancelWork →
  ActCt_TaskCleanup_8e30 → InternalBasePtr::DeletePtr → HuntWork_ScalarDtor_Impl → MemFree`
  (`gameupdateloop_teardown.cpp:243`, `workcomponent.cpp:357/220/312`,
  `action_base_complete.cpp:306`, `huntwork.cpp:915`). The same sequence on `Save9.sav` quit
  cleanly, so it is content-dependent. This is the `EntityList_Shutdown` teardown family already
  in the memory index, not a regression from this lane — but it is now reachable from a save load,
  which makes it cheaper to reproduce than the 800×10 stress town.
* Nothing here touches `runtime/src/io/`.
* **HEAD (`6435a40`) crashed on the save-load path inside a concurrent lane's IO port.** Built and
  run deliberately as a control: `GameUpdateLoop_Load` -> `runtime/src/io/compressedstream.cpp`
  -> `MemoryAllocator::Free` -> `ACCESS_VIOLATION`, WRITE of `00007f2a36a8bc82` (a garbage pointer
  being freed). The source line this lane first quoted (`:375`) was misleading; the lead
  symbolised the two Runtime frames and placed it precisely: `Runtime+0x6A57` is the locked
  `MemoryAllocator::Free` wrapper `FUN_180006a10`, and `Runtime+0xA95E0` is the return address of
  the `CALL` to it inside **`~CompressedMemoryStream`**, freeing `[RDI+0x10]` — the embedded
  `MemoryStream`'s `Stream::_encoding`. So it is a free of an already-freed or corrupted
  `_encoding`, raised by the DLL's own destructor; the destructor is the messenger, not the
  writer. It is intermittent (a later run was clean). **The lead owns it**; the decisive test is
  the same load with `BANISHED_RT_STREAM=0`. Recorded here only because this lane's fixed load
  path is what made it reachable. This lane's commit is `562d2ff` plus two files under
  `code/Game/Dialog/`, so it does not contain that port.

---

## 7. State this lane left behind — deliberate, not incidental

* **`C:\GOG Games\Banished\WinData\` has NO `.pkm` in it.** Both mods are back in
  `WinData\_mods_off\`, where this lane found them. That is the achievements-compatible
  configuration, and after this lane's fix it is also a working save-load configuration — which
  was not true this morning. `DebugMenu.pkm` was mounted there temporarily during investigation and
  has been removed; verified by `ls WinData/*.pkm` returning nothing.
* **The isolated directory `C:\laneH_game\` still exists** and holds the fixed control binary
  (`e57096aa…`, base `562d2ff` + this lane's two files) plus its own `WinData` with NO `.pkm`.
  It is ~180 MB and safe to delete; it is left in place because it is the only isolated harness
  on this machine and §5c's recipe is cheaper to reuse than to re-derive. Its logs are the
  evidence behind §5b runs 1-4.
* **The git worktree `C:/code/GIT/laneH2` still exists**, detached at `562d2ff` with this lane's
  two files checked out from `ef0debc` on top — that is the binary `e57096aa…` was built from.
  Remove it with `git worktree remove --force C:/code/GIT/laneH2` when the isolated harness is
  no longer wanted. (The earlier build worktree `C:/code/GIT/laneH` was already removed.)
* **The shared `reimpl-x64.exe` was NOT restored by this lane**, because another lane was actively
  deploying into it while this work finished. Whoever is running there owns that file; deploy your
  own build before trusting it.
* If you need to load one of this machine's pre-2026-09-05-18:47 saves on a build that does NOT
  contain commit `ef0debc`, copy `WinData\_mods_off\DebugMenu.pkm` into `WinData\` first — and
  remember that doing so silently disables every achievement.
