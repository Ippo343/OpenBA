# CHATTER-DIAG -- the harsh start / stop of the town chatter (phase 1, diagnose only)

Date 2026-10-16. Worktree branch `worktree-agent-a9739c38307497eb5` (base `7dbcaaf0e`). Nothing committed.
The temporary `[ChatDiag]` stderr traces (`audio_scene_update.cpp`, `audio_play_platform.cpp`,
`audio_sdl.cpp`) were reverted after the run; the patch is kept in the scratchpad
(`chatter/chatdiag_trace.patch`) only.

## Verdict (evidence below)

1. **Most likely: the loop seam of the asset itself (retail does the same).** `Crowd.wav` decodes to
   678,400 frames (15.383 s, stereo 44.1 kHz). Its last **1,088 frames (24.7 ms) are near-silent**
   (64-frame RMS 2..58 against a body whose quietest 10 ms window in 15 s is 312, p1 377, median 565),
   and the loop then re-enters at full level (first frame -132 / -302). So every 15.4 s the murmur
   drops out for ~25 ms and slams back in: a "stop, start" heard at a steady rhythm while the camera
   is still. 10 ms RMS across the wrap: `517 499 412 45 19 | 476 543 660 592 545`.
   The tail is the WMA encoder's padding of the last 2048-sample frame (678,400 / 2048 = 331.25).
   **Retail plays it too**: our decoder matches XAudio2 2.7 on Crowd.wav over every sample (max 1.53e-5,
   `docs/runtime_port/phase_d/handins/XWMA.md` "Measured in scratch"), and XAudio2 loops a single buffer by
   replaying it whole, pass 2 bit-identical to pass 1, measured on Crowd (XWMA.md A4.5). Our feed does the
   same (`audio_feed.cpp` Pull: `cursor = 0; LoopEnd(); continue;` in the same pull, no gap added).
   **It is an asset artefact, not a port defect.**
2. **Secondary, retail behaviour: no ramp anywhere.** Retail starts a voice at its full mixed gain from a
   mid-loop packet boundary and stops it by destroying the voice; volume moves are a per-frame step
   (`Device::SetVolumePlatform`, `SetVolume(mix, 0)`). None of the ported bodies (`Scene::Update`
   0x1800df6c0, `PlayPlatform` 0x1800de7a0, `GetAttenuatedVolume` 0x1800dfde0, `Scene::Pause`
   0x1800df070, `AmbientEmitterComponent` 0x140042a10 / c60 / d70, `ResidenceComponent` AddResident
   0x14012a710 / RemoveResident 0x14012a550) carries a fade; nothing was lost. The audible cases:
   the pause menu (`Scene::Pause(3, true)` releases every ambient voice at once; unpause restarts them at
   full gain), a family moving into / the last one leaving the nearest house, demolition, load.
3. **Ruled out: the distance edge.** Attenuation is linear and continuous:
   `1 - (d - inner) / (max - inner)` with `max = radius + camDist` (crowd: radius 10, inner 5,
   camDist = |camera - target| ~ tens of units), 0 beyond. The voice is stopped only when the
   max-over-emitters gain is exactly 0 and restarted when it becomes > 0, i.e. at near-zero gain.
4. **Ruled out as the cause, but a real difference: SDL applies gain as a flat per-chunk multiply.**
   Read in SDL 3.4.18 (`third_party/SDL` @ `829a65d7`): `SDL_audiocvt.c:327-337` `buf[i] *= gain`,
   gain read once per `SDL_GetAudioStreamDataAdjustGain` call (:1369); no interpolation. XAudio2 may
   smooth a SetVolume over its 10 ms pass -- NOT verified (cannot be measured here). It only matters
   for step changes (a house gaining / losing residents near the listener), not for the steady wrap.
5. **SDL start / underrun: clean.** The data is queued before Start (Submit -> SetVolume -> Ratio ->
   Start, `audio_play_platform.cpp`), the first pull carries the resampler margin (CLICK row 1289),
   and the loop continues inside one pull.

## 1. Which sound is the chatter, and who plays it

- `ToolKit/resource/Audio/Effects/CrowdEffect.rsc`: SoundBuffer `_looping = true`, `Audio\Effects\Crowd.wav`;
  SoundEffect `_radius = 10.0`, `_innerRadius = 5.0`, one sample, volume 1.0.
- Referenced ONLY by the three residences (`Template/WoodHouse.rsc:40-44`, `StoneHouse.rsc:44`,
  `Hostel.rsc:43`): `AmbientEmitterDescription ambientemitter { _addOnCreate = false; _soundEffects
  [ FireEffect, CrowdEffect ] }` -- slot 1. Not the biome bed (that is `AmbientEnvironmentComponent`
  SetSlot day / night, `Environment.rsc`), not the market / town hall (FireEffect only).
- Started by `ResidenceComponent::AddResident` (`src/game/Component/residencecomponent.cpp:392-438`) when
  the FIRST family member is assigned (`CountResidents() == 0`): move the emitters to the model,
  `PlaySlot(1)` -> `FindEffect3D` (one shared effect for every house) or `PlayEffect3D` type 3, then
  `AddEmitter(handle, housePos, volume)`. Stopped by `RemoveResident` (:347-374) when the LAST leaves:
  `RemoveEmitter`, `StopEffect3D` when the effect has no emitter left. Residency is assignment, not
  presence: the loop does NOT start / stop as people walk in and out.
- So the whole town's chatter is ONE looping voice whose gain = max over occupied houses of the
  linear falloff (`wave29_lane_avbody.cpp` GetAttenuatedVolume), pushed per frame.

## 2. Playback path

`Scene::Update` loop 1 (per frame: attenuated gain; 0 -> `StopEffect`; > 0 and no channel ->
`PlayEffect`) -> loop 2 (`Device::PlayPlatform` for a starting sample; else `SetVolumePlatform(mix)`)
-> `Platform::AudioVoiceSubmit(PCM, playBegin, LoopCount 255)` -> `AudioFeed` (loops from byte 0) ->
`SDL_PutAudioStreamData` in the get-callback, gain = `SDL_SetAudioStreamGain`. The PCM is decoded once at
load (`SoundBuffer::DecodeAtLoad`, `vtbatch7_soundbuffer_soundeffect.cpp:301`).

## 3. The asset census (all looping ambiences carry the same tail)

Decoded with the game's `XwmaDecoder` (`tools/audio_tail`, packets cut by `pkg_sounds.py` into the
scratchpad; nothing retail written to the tree). "tail" = trailing run of 64-frame windows under 20 % of
the body's p1 10 ms RMS:

| loop | length | body p1 / median | tail | last64 / first64 RMS | seam step |
|---|---|---|---|---|---|
| Crowd | 15.38 s | 377 / 565 | **24.7 ms** | 2 / 553 | 302 |
| Fire | 30.65 s | 489 / 1200 | 21.8 ms | 15 / 1280 | 23 |
| Rain | 56.19 s | 733 / 1658 | 18.9 ms | 42 / 1297 | 329 |
| River | 16.25 s | 576 / 775 | 21.8 ms | 0 / 1011 | 834 |
| Wind | 15.05 s | 562 / 1039 | 10.2 ms | 82 / 103 | 147 |
| Tornado | 25.27 s | 1847 / 3766 | 21.3 ms | 15 / 1135 | 155 |
| Env field | 15.09 s | 178 / 361 | **43.5 ms** | 1 / 461 | 640 |
| Env forest | 18.27 s | 210 / 357 | 14.7 ms (+5.3 ms head) | 4 / 2 | 3 |
| Env wetland | 15.06 s | 156 / 590 | 11.6 ms (+1.5 ms head) | 3 / 22 | 1 |
| Env winter | 20.48 s | 145 / 510 | 0 | 42 / 86 | 91 |

Crowd stands out because it is dense, steady and short; River and the field bed have the same
defect (the user may hear those once the crowd is fixed).

## 4. In-game trace

Perf exe of this worktree + the trace patch, `BANISHED_STRESS` town (`tools/stress_run.sh --no-build
--speed 1 --frames 3000`, game-dir lock `chatter`, deployed md5 `bea2ae36...` checked before / after,
exit 0, crash log empty, machine load CLEAN). 163 `[ChatDiag]` lines in `standalone_error.log`:

- The crowd is ONE voice, `PlayPlatform slot=1 bytes=2713600 loop=1 playBegin=0 dur=15.383`, started at
  load (`t_sdl=9773 VOICESTART`), scene effect `slot=1 r=10/5 emitters=353` (353 occupied houses on one
  effect). The river is the same shape (`bytes=2867200 dur=16.254`, `r=10/5 emitters=221`).
- It was **never stopped or restarted** during the ~50 s run: no `STOP` / `PLAY` for it after the first,
  `VOICESTOP` / `VOICEDESTROY` only at quit (`t_sdl=59718`).
- Its gain moved only while the camera pin settled (0.264 -> 0.407 -> 0.353 -> 0.298 -> 0.246 -> 0.227
  over the first 2.3 s), then stayed 0.2272 for the rest of the run.
- **Loop wraps:** `LOOPEND` at `t_sdl=25156, 40536, 55916` -- exactly 15.380 s apart = the PCM length.
  So with a still camera the only discontinuities the chatter voice has are its wraps, each carrying the
  24.7 ms dropout of §3. (The river voice wraps on its own 16.25 s cycle with a 21.8 ms tail.)

## 5. Remedies

**A. `ext.seamless_loops` (class A, OFF by default) -- RECOMMENDED.** A load-time transform of the
user's installed data (DESIGN rule 7): in `SoundBuffer::DecodeAtLoad`, for a `_loopForever`
non-streamed buffer, after `DecodePackets`: trim the trailing codec tail (64-frame windows under a
fraction of the body level, bounded to <= 1 WMA frame = 2048 samples) and fold the last X ms into
the first X ms with an equal-power crossfade, shortening the buffer by tail + X. Offline on Crowd
(scratch `seamfix.py`, X = 50 ms): tail 1,088 frames trimmed, new length 15.309 s, 10 ms RMS across
the wrap `630 536 626 647 565 | 542 413 674 609 542` (was `517 499 412 45 19 | 476 543 ...`), seam step
302 -> 138 (inside the body's normal sample-to-sample steps, max 901).
- Seam: an engine-side null-default static data pointer (rule 5), e.g. `const bool*
  Audio::SoundBuffer::_extSeamlessLoops`, set by `src/ext` at boot before the resource load; the faithful
  path pays one null test per buffer load (not hot).
- Toggle takes effect at the next boot (buffers decode once at load); say so in the Enhanced tab text.
- Interactions to check: `ComputePlayPosition` takes `time % _durationSeconds` and a packet index into
  the ORIGINAL table, then `XwmaPcmPlayBegin` clamps to the PCM length -- a 1-2 % shorter buffer only
  moves a resume point, and a resume landing past the new end is clamped (would start at the very end
  and wrap at once; acceptable, or clamp to `start % newLength`). `_durationSeconds` is unchanged, so
  the scene's timing is unchanged. Memory unchanged. Applies to every looping one-shot (all ten above);
  music (streamed) is untouched (its wrap was ruled clean by ear, D4 close).
- Effort: ~0.5 day (one helper in `src/ext/seamless_loops/`, pure and unit-tested on synthetic PCM, the
  registry row + Enhanced tab entry, the seam). Risk low: one-shots only, decoded PCM only, no sim,
  no save, no gate change with the row off.

**B. `ext.audio_declick` (class A, OFF) -- optional, for start / stop / step changes.** A short ramp
in the SDL backend: fade-in over ~10 ms at a voice's first pull, a gain glide (per-frame linear from
the previous to the new gain inside the get-callback, SDL stream gain left at 1.0) and a ~10 ms
fade-out before Stop / Destroy (the stop would need to be deferred to the callback). Covers the
pause-menu cut, a house's emitter appearing / vanishing, and load. This changes the platform layer's
timing of Stop / Destroy (XAudio2's semantics: immediate), so it is more invasive: ~1-1.5 days, medium
risk (the AudioVoiceDestroy-does-not-return-while-a-callback-runs proof in `audio_sdl.cpp` must still
hold; a deferred fade-out cannot outlive the voice record). Only worth it if the user still hears
hard edges after A. A port defect is NOT claimed here: retail has no ramp in any body read.

**C. Not recommended:** fading the loop in the game logic (AmbientEmitter / Residence) -- it would
change faithful bodies and does nothing for the seam.

## 6. What could not be verified

- Retail by ear (the user can: the wrap gap should be audible in `Application-x64.exe` too, every
  ~15 s near an occupied house with the camera still). This is the one check that decides "retail too".
- Whether XAudio2 2.7 ramps `SetVolume` across a pass (would make retail's step changes softer than ours).
- `third_party/SDL` is EMPTY in the main checkout since 18:34 today (main's `build/cmake-debug` was last
  built 18:33; any re-configure will hit CMakeLists' FATAL_ERROR). This worktree initialised its own
  copy (`git submodule update --init --depth 1 third_party/SDL`, the pinned 829a65d7) to build.
  Coordinator: check what emptied main's submodule.
- The game dir's `OpenBA.exe` is now this worktree's TRACE build (md5 `bea2ae36...`, prints `[ChatDiag]`
  to stderr). Before my deploy it held md5 `4477f5e8...` (not mine). Redeploy before the next gate run.
