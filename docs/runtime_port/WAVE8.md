# Wave 8 — Top (Engine, Audio, Input, Application, Save remainder, Utility)

In progress on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2 wave 8
("Top — Audio, Input, Application, Engine, Save; integrators; mostly wiring").

## Scope

`tools/runtime_census.py` (2026-09-07, after Wave 7, before any Wave 8 registration) —
`docs/runtime_port/census_20260907_prewave8.csv`:

| Namespace | in DLL | called by our tree | ported |
|---|---|---|---|
| Engine | 108 | 25 | 0 |
| Audio | 137 | 22 | 0 |
| Input | 63 | 21 | 0 |
| Application | 26 | 6 | 0 |
| Save | 37 | 20 | 15 (Wave 5) |
| Utility | 16 | 2 | 0 |
| **Wave 8 denominator** | | **81 unported** | |

Whole-tree at the wave boundary: **766 called, 458 ported (59.8%)**.
Per-function lists with retail addresses, ordinals and call sites:
`docs/runtime_port/wave8_lanes.txt` — generated from the census plus the shipped export
directory **before any body was written**.

## Why this wave is different from 1–7

**1. This is the integrator wave, and the "DLL's own internal callers" problem is finally
unavoidable in principle.** PLAN §4.2 records that the detour count is still **0 across all
4,632 functions** and that the mechanism is unproven in anger. `Engine::MainLoop::Run` is the
DLL's own frame driver and `Engine::Engine` is the object every DLL subsystem reaches through.
Waves 6 and 7 each expected to be the first detour user and each was not, for a specific
measured reason (no vtable / the vtable is exported). **Every lane must state, from the ctor's
first store or from the export table, which of those two reasons applies to its classes — or
install the detour and say so.** Do not assume the answer from Wave 6/7's luck.

**2. Vtable exports, measured before the wave opened** (pefile over the shipped
`Runtime-x64.dll`, 147 `??_7` exports in total):

| Class | `??_7` exported? |
|---|---|
| `Engine::UpdateLoop`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`, `Loader` | **yes, all five** |
| `Engine::Engine`, `Engine::MainLoop` | **no** |
| `Audio::SoundBuffer`, `SoundEffect`, `Stream` | yes (none of the three is in this wave) |
| `Audio::Scene`, `Audio::Device`, `Audio::Configuration` | **no** |
| `Input::Device`, `Input::KeyMap`, `Input::Configuration` | **no** |
| `Application::Application`, `Save::SaveLoad`, `Utility::PerlinNoise` | **no** |

An absent `??_7` export is *consistent with* a non-polymorphic class (`Video::Context`,
`UI::UserInterface`) but is **not proof** — WAVE7.md §2 says so explicitly and made lane C
verify it. Lanes B, C, D and E each verify it from the ctor's first store, not from the
absence of an export.

**3. `Engine::Engine` embeds the MemoryAllocator at offset 0** (PLAN §4.3 risk 2:
`sizeof(MemoryAllocator) == 0x670`, `Engine::GetMemoryAllocator` is a bare `return this`).
The allocator port exists, is opt-in, and carries a **NO-GO** verdict (WAVE2_ALLOCATOR.md).
So `??0Engine@0@QEAA@PEAX@Z` and `??1Engine@0@QEAA@XZ` construct and destroy global state we
have deliberately not taken over. Lane B is expected to **hold these back with a written
justification**, not to port them because they are on the list. An unverified port is not a
port (the Wave 2 allocator verdict); a port with no possible oracle is worse.

**4. All three remaining phantom export names in the whole tree are Wave 8 names.**
`tools/export_name_audit.py` at the wave boundary: **644 distinct `ResolveExport` names,
3 not in the shipped DLL** — and all three belong to this wave:

| Phantom name | Lane |
|---|---|
| `?CheckMod@SaveLoad@Save@@QEAAHHI@Z` | E |
| `?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z` | E |
| `?PlayEffect@Scene@Audio@@QEAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@VTime@System@@@Z` | C |

The Audio one spells `QEAI` where every other member spells `QEAA`, and the census lists a
second, correctly-spelled `PlayEffect` alongside it. Apply the Wave 7 lane-C test before
claiming a functional gap: `ResolveExport` has no address fallback, but the **call site in
`engine_funcs.cpp` may have its own** `if (!fn) fn = ResolveAddr(Module::Runtime, <addr>)`.
Check that first. Whatever the verdict, **a Wave 8 port of a phantom name must register under
BOTH the DLL's real name and the address** — otherwise the port is dead at its only call site
while the oracle reports `calls=0 divergences=0` (WAVE7.md §3, and
`reference_uncalled_port_is_not_verified.md`).

**5. Almost nothing here is pure, and three lanes touch the OS.** `Input::Device` is
DirectInput8, `Audio::Device` is the platform mixer, `Application::PlatformUpdate` pumps the
Win32 message queue, and `Application::SleepThread`/`YieldToThreads` are scheduler calls. The
Wave 1/2 replay oracle (call ours, call theirs, compare) is **invalid** for any of them:
running both drains the input queue twice, plays the sound twice, or pumps two message loops.
Lanes argue their oracle **per function** — replay for the const accessors
(`GetValue`, `Pressed`, `GetEmitterCount`, `IsEffectPlaying`, `IsRunning`, `IsPaused`,
`GetIdForDeviceInput`, …), byte comparison of a private scratch copy for out-param writers,
shadow for mutators, and a **written NO-ORACLE holdback** for anything that cannot be checked.
`Utility::PerlinNoise::Generate` is the one genuinely pure function in the wave and gets a
full replay oracle to the bit.

Two oracle traps this project has already paid for, both live here:
  * **Never memcmp bytes neither side writes** — 0xCD surviving in THEIRS means retail never
    wrote there, and the site then reports 100% divergent
    (`reference_oracle_must_not_compare_unwritten_bytes.md`).
  * **A written-extent constant belongs to a WRITER, not a type** — the same field is
    correctly excluded from one oracle and included in another
    (`reference_never_written_needs_by_whom.md`).

**6. `Audio::Scene::Serialize` and `Save::SaveLoad` sit on the save stream.** The Wave 4
warning stands: the IO `Encoding` vtable **offsets differ between retail and the profile
build** (`reference_encoding_vtable_offsets_differ_between_builds.md`). The profile build's
NAMES win; its OFFSETS must not.

**7. Read `reference_ghidra_c_output_vs_disassembly.md` before trusting a decompile.**
Ghidra's C drops dereferences and prints `MOVSXD` as a zero extension; three Wave 6 defects
including the crash came from that. Check the disassembly for anything load-bearing.

## Lanes

| Lane | Scope | Fns | File | Kill switch |
|---|---|---|---|---|
| A | `Engine::UpdateLoop` + `Loader` + `LoaderLoop` + `DialogLoopT` + `DialogLoaderLoopT` | 14 | `runtime/src/engine/loops.cpp` | `BANISHED_RT_ENGINELOOP` |
| B | `Engine::Engine` + `Engine::MainLoop` | 11 | `runtime/src/engine/engine.cpp` | `BANISHED_RT_ENGINECORE` |
| C | `Audio::Scene` + `Audio::Device` + `Audio::Configuration` | 22 | `runtime/src/audio/audio.cpp` | `BANISHED_RT_AUDIO` |
| D | `Input::KeyMap` + `Input::Device` + `Input::Configuration` | 21 | `runtime/src/input/input.cpp` | `BANISHED_RT_INPUT` |
| E | `Application` + `Save::SaveLoad` remainder + `Utility::PerlinNoise` | 13 | `runtime/src/application/application.cpp`, `runtime/src/save/saveload_rest.cpp`, `runtime/src/utility/perlin.cpp` | `BANISHED_RT_APP`, `BANISHED_RT_SAVELOAD`, `BANISHED_RT_UTILITY` |

Namespace-level switches: `BANISHED_RT_ENGINE=0`, `BANISHED_RT_AUDIO=0`, `BANISHED_RT_INPUT=0`,
`BANISHED_RT_APP=0`, `BANISHED_RT_UTILITY=0`. Registration aggregators:
`runtime/src/engine/rt_engine_port.cpp` (`RtEnginePortInit`, lanes A+B),
`runtime/src/audio/rt_audio_port.cpp`, `runtime/src/input/rt_input_port.cpp`,
`runtime/src/application/rt_application_port.cpp`, `runtime/src/utility/rt_utility_port.cpp`
— mirroring `runtime/src/video/rt_video_port.cpp`. Lane E extends the existing
`runtime/src/save/rt_save_port.cpp` rather than adding a second Save aggregator.
The integrator wires the new `*PortInit` declarations into `runtime/funcs/rt_port.h` and the
call list in `standalone/resolve_standalone.cpp`.

### The cross-lane ownership contract

* **Lane B owns `runtime/types/engine/Engine.h` and `runtime/types/engine/MainLoop.h` and
  publishes them FIRST**, before writing any body. Lane A measures `UpdateLoop`'s relationship
  to `MainLoop` against them. Note `engine/types/MainLoop.h` already exists from the hook era —
  lane B reconciles it and fixes it **at its source** rather than minting a competing layout
  (PLAN §6 step 8).
* **Lane A owns `runtime/types/engine/UpdateLoop.h`** and the loop-family layouts.
* Nobody edits a header another lane owns. If a lane needs a field added to someone else's
  header, it says so in its doc and asks.
* Nobody edits `runtime/funcs/rt_port.h` or `standalone/resolve_standalone.cpp` — the
  integrator does, once, at merge.

## Process (unchanged from Waves 6–7)

Lanes write code, headers and their lane doc. **Lanes do NOT run `make`, the game,
`tools/rt_run.sh`, `sa-run` or `git commit`** — the integrator serialises the build and every
diff run, because one deployed exe and one log directory cannot serve five lanes. Every
runtime number in a lane doc is therefore a *prediction of what the oracle will report* until
the integration run measures it.

## Pre-wave control (2026-09-07, exe stamp 15:32:19, scaffolding only, 0 Wave 8 ports)

`tools/rt_run.sh wave8-control --timeout 900 -- BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1
BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6`
→ `docs/runtime_port/wave8_control_autostart.txt`

Clean exit rc=0 after 35s, empty crash log.

**CORRECTION, entered at integration: the site and call counts this section first recorded
("59 sites, 1,160,663 calls, 0 divergences") WERE WRONG, and the tool was lying, not the run.**
`tools/rt_run.sh` ended its report with `| tail -80`. A full diffed run emits ~1,200 `[RtDiff]`
lines, so only the last 80 survived and **every site before them was invisible — reported as if
it had never been called.** The true control is the same shape as the integration run below
(~250 sites, ~28M calls). The `tail` is now removed, with the reason recorded at the call site
in `tools/rt_run.sh`.

**This is worth more than the correction.** `calls=0` is the exact signal this project uses to
detect a dead port (`reference_uncalled_port_is_not_verified.md` — a wrong `Quaternion::operator*`
survived all of Wave 1 behind it). A reporting cap that MANUFACTURES `calls=0` attacks the one
number every wave is read on. At this integration it presented as 16 diff sites "disappearing"
between the control and the integration run — including `Graphics::Scene::RemoveObject` at
15,842 calls — which is a catastrophic-looking result, and it cost seven bisection runs before
the raw log showed the sites had been there the whole time. **Read the raw
`standalone_error.log` before believing any site vanished.**

## Results — WAVE 8 LANDED 2026-09-07

**49 of the 81 unported functions ported, 32 held back with written reasons.** Census after the
wave: **772 called, 507 ported (65.7%)**, up from 766 / 458 (59.8%).

| Lane | Scope | Ported | Held back |
|---|---|---|---|
| A | Engine loop family | 12 of 14 | `UpdateLoop::LoadResources` (no oracle, ~250 boot-path instructions); `LoaderLoop::_loader` (DATA — the DLL's own static `Engine::Loader`, a live parked OS thread; registering it would stall the loading screen forever) |
| B | `Engine::Engine` + `MainLoop` | 3 of 11 | 8: four to the allocator NO-GO, two to a detour requirement, `Run`, `_singleton` |
| C | Audio | 13 of 22 | 7, all reaching `Audio::Device`'s real platform mixer, plus `Scene::Serialize` |
| D | Input | 15 of 21 | 6: `Device::Update`, `EnableMouseClipping`, `KeyMap::RegisterInput`, `KeyMap::Load`, 2 DATA singletons |
| E | Application + Save remainder + Utility | 6 of 13 | `~SaveLoad`, `Draw` (Wave 5 verdicts, unchanged); `PerlinNoise` ctor (no live caller) |

### Runs

| Run | Sites with calls | Diffed calls | Divergences |
|---|---|---|---|
| `wave8_integration_autostart.txt` | 250 | **28,191,797** | 99 in 2 sites — Wave 1 family only |
| `wave8_final_save9.txt` (load path) | 265 | **50,870,488** | 61 in 3 sites — Wave 1 family only |
| `wave8_libm_dll.txt` (`BANISHED_RT_LIBM=dll`) | 250 | — | **0, tree-wide** |

**Zero Wave 8 divergences in 79 million diffed calls across the two runs**, both with a
zero-byte `standalone_crash.log` and a clean self-quit. Every residual is the pre-existing Wave 1
1-ULP family at `Vector3::FromYawPitch`, `Transform::FromYawPitchOffset` and `Vector3::Heading`,
and the reported bytes confirm it (`e2` vs `e3`, `5a` vs `5b` — one ULP in the low byte).

The `BANISHED_RT_LIBM=dll` run drives the whole tree to **0 divergences**, which is the proof
that the residual is libm rounding and not port logic — and it also settles lane E's
`PerlinNoise::Generate` tolerance: 2.2M calls clean under both libms, so the ~4-ULP tolerance is
not masking a formula bug. (Not run: `Generate` with its tolerance forced to 0 under DLL libm,
which would be the strictest form of that test.)

### The detour question, answered at last

PLAN §4.2 has recorded since Wave 5 that the detour count is **0 across all 4,632 functions** and
that the mechanism is unproven in anger. Wave 8 is the first wave to find a **genuine detour
requirement**, and lane B found it by looking rather than assuming: `MainLoop::OnVideoError` and
`OnVideoRestart` each have exactly ONE internal caller — the static thunks the `MainLoop` ctor
installs into `Video::Device::_singleton` +0x50/+0x58, reached only on device lost/restore. A
port registering the export and stopping would be **dead code reporting `calls=0 divergences=0`**.
Both are held back and the ported ctor keeps installing the DLL's own thunks, so **the detour
count is still 0** — but for the first time the reason is "we found the case and declined it",
not "no case arose". A detour there could not have been exercised by any unattended run anyway.

### What the allocator NO-GO actually covers

This document predicted the `Engine` ctor and dtor. Lane B proved it reaches **four** functions:
`Engine::Startup` writes three allocator fields directly (`minRangeSize`, `maxTotalSize`,
`pageSize`) then calls the allocator's own init at 0x1800c7690, and `Shutdown` calls its shutdown
at 0x1800c75b0. `GetMemoryAllocator` is literally `MOV RAX,RCX; RET`, confirming the offset-0
embedding.

Lane B also checked the `VideoSettings` arming bit that every unattended run in this project
depends on: it is written by `Engine::Shutdown`, **not** `Startup`. Both are held back, so
`HarnessGuardUnattendedBoot` is unaffected.

### Vtable verdicts (all measured, none assumed)

* **Lane A**: all five loop classes export `??_7` **and** each ctor's `LEA` stamps exactly the
  exported address — checked instruction by instruction, five for five. The Wave 6
  `??_7Geometry` lever holds.
* **Lane B**: neither `Engine` nor `MainLoop` is polymorphic, proven from the ctors (`Engine`'s
  first store into `this` is a zero at +0x670, after the allocator ctor on unmodified RCX;
  `MainLoop`'s is `XOR EAX,EAX; MOV [RCX],RAX`, a zeroed Array pointer).
* **Lanes C, D, E**: no `??_7` export and no vtable store in any ctor examined. Lane D notes
  honestly that no `Input` ctor was in its scope, so it claims consistency, not proof.

### Phantom export names: the tree is CLEAN for the first time

`tools/export_name_audit.py` went **3 → 0**. All three phantoms in the whole tree were Wave 8
names: lane E fixed `Save::CheckMod` and `SetSaveName` (diagnosed by Wave 5 but never fixed at
the source, and registered by name only), and lane C fixed `Audio::Scene::PlayEffect`, spelled
`QEAI` where the DLL spells `QEAA`. Neither was a functional gap — both call sites had their own
address fallback, which is the Wave 7 lane-C answer. All three ports now register under **both**
the real name and the address.
Current: **651 distinct `ResolveExport` names, 0 missing from the shipped DLL.**

### Defects found and fixed at source

1. **`fn_UpdateLoop_Ctor`'s fallback address was not a function entry** (lane A) —
   `ResolveAddr(Module::Runtime, 0x1800dcc00)` points *inside* `MainLoop::OnVideoError`
   (0x1800dc5e0–0x1800dcd12). The correct entry is 0x1800ddd30. Latent, because `ResolveExport`
   always succeeded — but a fallback is only ever taken when something is already wrong.
2. **`ExternalFactory+0x04` is not padding** (lane A) — it is the 32-slot clean-bit allocation
   mask, scanned by `UpdateLoop::UpdateLoop` and cleared by `~UpdateLoop`. Wave 3 saw no writer
   because both writers live in `Engine`. Renamed `_cleanBitMask` in
   `runtime/types/object/External.h`. **Any later lane touching `ExternalFactory` needs this.**
3. **`engine/types/MainLoop.h` had three "padding" dwords that are `Collection::Array`
   capacities** (lane B), plus a wrong `IsRunning` signature note. Corrected in place with three
   new `static_assert`s. Lane B deliberately did NOT "fix" a fourth suspect field whose writer it
   had not read, and recorded the limit instead — the right call.
4. **`LoaderLoop+0x40` is not `m_finished`** (lane A) — the ctor sets it, nothing else writes it,
   and `IsFinished` never reads it. Renamed `m_autoAdvance`; three `code/Game/` call sites were
   correct for a mis-stated reason. The two `g_pLoader*` globals are `System::Thread::runFlags`
   and `hFinished`, not `Loader` fields.
5. **`fn_AudioScene_PlayEffect` was a vestigial resolve under a WRONG name** (integrator, from a
   lane C flag). Dispatch slot 0x140152830 is `??0Goal@Achievement@@QEAA@XZ`
   (`Achievement::Goal::Goal`) straight out of the exe's PE import table — the unregistered-slots
   wave had already identified the mislabel and deliberately left the slot unregistered. The
   typedef had ZERO call sites so it never misbehaved, but it was armed: the moment anyone
   registered that slot it would have become `Goal`'s ctor called through a guessed 4-argument
   signature. Deleted, with the reason recorded where it stood.
6. **`tools/rt_run.sh` truncated every oracle report to the last 80 lines** (integrator) — see
   the control section above.

### Coverage: what the runs did NOT exercise

27 of the 49 ports produced a live diff site. **The other ~22 have no runtime evidence**, and per
`reference_uncalled_port_is_not_verified.md` that is not the same as verified:

* **The whole of `Input::Device`** (`GetValue`, `Pressed`, `Released`, `ResetState`) and most of
  `KeyMap` (`ClearInput` ×2, `GetDeviceForId`, `GetInputForId`, `GetIdForDeviceInput`,
  `SetInput`). Lane D predicted this and was **half right**: it expected the unattended harness to
  miss `KeyMap` entirely, but `KeyMap::Pressed` (12,724 calls) and `KeyMap::GetValue` (7,200) did
  fire from camera input. `Device` got nothing, because `KeyMap::Pressed` does not route through
  it. **These need an ATTENDED run with real keyboard and mouse input before they count as
  exercised** — the harness injects dialog clicks by calling handlers directly and never passes
  through the input layer.
* `Audio::Scene` ctor/dtor, `MoveEmitter`, `RemoveEmitter`, `GetEmitterCount`,
  `Configuration::Startup`.
* `UpdateLoop::IsRunning`, `Application::IsPaused`, and lane E's three shadow-marker ports
  (`PlatformUpdate`, `SleepThread`, `YieldToThreads`), which have no oracle by construction.

### Still open after this wave

* The 32 held-back functions above, each with a written reason in its lane doc.
* The attended input run.
* `tools/runtime_census.py` still cannot see 18 Wave 7 `RegisterRuntimeExport` calls that pass a
  named constant instead of a literal (`runtime/src/ui/widgets_input.cpp`), so every UI number is
  an undercount. Pre-existing, not a Wave 8 regression.
* The 5% perf gate remains BREACHED and DEFERRED BY THE USER to after the final stage. Wave 8 was
  not measured against it.
