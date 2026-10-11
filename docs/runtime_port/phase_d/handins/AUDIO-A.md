# AUDIO-A hand-in (Phase D.4 wave 3, branch `d4-audio`, base `0b11a0f85`)

Plan: `plans/SDL.md` Block 3, steps A1 and A2 only (D4_RULES §2 wave 3). A3 (the xWMA decoder) is the
XWMA block; A4 / A5 (the SDL backend) come later.

Static gate only. The game, `sa-run` and `sa-deploy` were never run. Main did not move during the
block (`0b11a0f85` is still its tip), so there was nothing to merge before the final build.

## Commits

| commit | title | gate |
|---|---|---|
| `2abfb4e81` | refactor: Platform::Audio API, XAudio2 behind it (A1) | debug / perf / unit EXIT 0; unit 971 / 0; 0 new warnings (debug, perf); castmetric src 2; abi_audit OK, three rows lowered |
| `02adbaa8b` | diag: SoundBuffer format census (A2) | same |
| `586ea9f5e` | behaviour: row 1274 -- PlayPlatform LoopCount 0xFF (XAUDIO2_LOOP_INFINITE), not 0xFFFFFFFF | same; PlayPlatform B11 DIFF with A1's call shape, and the loop count is now `neg %cl ; sbb %ecx,%ecx ; movzbl %cl,%ecx`, retail's sequence |

## A1: what landed

- **The API** is `src/platform/common/platform_audio.h`: namespace `Platform`, free functions, no OS,
  COM, XAudio2 or SDL header.
  - The engine, master and voice are pointers to incomplete types, 8 bytes each. On Windows each
    value is the COM interface pointer.
  - `AudioWaveFormat` is WAVEFORMATEX field for field. The Windows backend asserts every offset
    against `<mmreg.h>`.
  - `AudioVoiceCallback` holds the seven IXAudio2VoiceCallback slots in their order.
    `Device::PlatformChannel` derives from it. It adds no data, so the record's layout and vtable
    are unchanged and `singleton_ctors.o` stays IDENTICAL.
  - The callback contract is written in the header. OnBufferEnd calls `EventSet(bufferEvent)` and
    OnStreamEnd sets `channel->flags |= 2` without a lock. The bodies are Banished's; a backend
    only calls them.
- **The Windows backend** is `src/platform/win/audio_xaudio2_win.cpp`. It makes the same COM calls
  on the same slots with the same arguments. `AudioEngineCreate` is `RtAudio_CreateXAudio2`
  (0x180050270) moved verbatim, and B11 pairs the two as RENAMED-IDENTICAL.
- **Submit and GetState** take plain arguments rather than packed mirror structs. The backend builds
  the XAUDIO2_BUFFER / XAUDIO2_BUFFER_WMA pair from the same values the callers stored. This keeps
  `#pragma pack` out of `src/platform/common` (abi_audit would count a new row there).
- **The Stream's events** are `Platform::EventHandle`: `_wakeEvent`, `_bufferEvent` and
  `PlatformChannel::bufferEvent`. The THREADS hand-ins left them to this seam. CreateEventW,
  CloseHandle, SetEvent and WaitForMultipleObjects became EventCreate, EventClose, EventSet and
  EventWaitAny.
- **Two retail bodies are new.** `Device::SetVolumePlatform` (0x1800deab0) and
  `Device::PlatformShutdown` (0x1800de690) were both read in Ghidra. Each is exactly the sequence its
  caller had inlined, and both are public now.
- **Four files moved** from `src/platform/win` to `src/engine/audio`, because no OS header was left in
  them: `audio_platform_startup`, `audio_voice_callback`, `audio_stream` and `audio_stream_play`.
- **No `<xaudio2.h>` and no XAudio2 type is spelled outside `src/platform/win`.** Three comments
  remain in `SoundBuffer.h`, `Stream.h` and `platform_audio.h`.

### The XAudio2 files at the base, and what happened to each

The plan says 13. Eighteen files mention the name: 14 under `src/`, plus the README, `platform_os.h`, one test
and one tool.

| file | at the base | now |
|---|---|---|
| `src/engine/audio/Device.h` | IXAudio27 / mastering / source-voice forward declarations; typed fields | opaque `Platform::` handles; PlatformChannel implements `AudioVoiceCallback`; two members public |
| `src/engine/audio/audio_device.cpp` | `<windows.h>` + `<xaudio2.h>`; DestroyVoice, Stop, SetEvent | primitives + EventSet; no OS header; new SetVolumePlatform body |
| `src/engine/audio/audio_play_platform.cpp` | `<windows.h>` + `<xaudio2.h>`; six voice calls, XAUDIO2_BUFFER locals | primitives; no OS header |
| `src/engine/audio/audio_scene_update.cpp` | `<windows.h>` + `<xaudio2.h>`; a direct SetVolume COM call | `device->SetVolumePlatform`; no OS header |
| `src/engine/engine/engine_lifecycle.cpp` | `<xaudio2.h>`; the inlined PlatformShutdown | `dev->PlatformShutdown()`; `<windows.h>` stays for other reasons |
| `src/game/Game/game_main.cpp` | `<xaudio2.h>`; the inlined PlatformShutdown | `ad->PlatformShutdown()`; `<windows.h>` stays |
| `src/platform/win/audio_platform_startup_win.cpp` | `<windows.h>` + `<objbase.h>` + `<xaudio2.h>`; GUIDs, CoCreateInstance helper, PlatformStartup | helper and GUIDs moved to `audio_xaudio2_win.cpp`; the rest moved to `src/engine/audio/audio_platform_startup.cpp` (+ PlatformShutdown) |
| `src/platform/win/audio_voice_callback_win.cpp` | `<windows.h>`; SetEvent | `src/engine/audio/audio_voice_callback.cpp`, EventSet |
| `src/platform/win/vtbatch7b_stream_run_win.cpp` | `<xaudio2.h>`; GetState x4, SubmitSourceBuffer, WaitForMultipleObjects x2 | primitives + EventWaitAny; keeps `<windows.h>` for SetFilePointer (the file seam's) |
| `src/engine/engine/singleton_ctors.cpp` | comment + `_xaudio2 = nullptr` | unchanged code (a comment re-pointed) |
| `src/engine/audio/SoundBuffer.h` | comment | unchanged (A2 adds a declaration) |
| `src/engine/audio/Stream.h` | comment; `void*` events | `Platform::EventHandle` events |
| `src/engine/audio/wave29_lane_avbody.cpp` | comment | unchanged |
| `src/game/Game/gameupdateloop_serialize.cpp` | comment | unchanged |
| `src/engine/audio/README.md` | prose | a Phase D.4 paragraph added |
| `src/platform/common/platform_os.h` | comment (COM consumers) | unchanged |
| `tests/unit/audio_abi_test.cpp` | `offsetof(_xaudio2)` | unchanged (passes); a comment re-pointed |
| `tools/abi_audit.py` | pattern list | unchanged |

The two stream files had no XAudio2 spelling but moved with the events: `audio_stream_win.cpp` and
`audio_stream_play_win.cpp`.

### A1 B11 verdicts

B11 was run on the -O2 objects against `runs/b11base_d4c/perf/obj`. The moved objects were paired
by hand under their new names, with the map `RtAudio_CreateXAudio2 -> Platform::AudioEngineCreate`.

| object | verdict | shape |
|---|---|---|
| `audio_device.o` | FreeChannelPlatform DIFF, StopPlatform DIFF, SetVolumePlatform ONLY-TIP (new), 9 IDENTICAL | `call *0x90` became AudioVoiceDestroy; `call *0xa0` became AudioVoiceStop; the SetEvent import became EventSet |
| `audio_play_platform.o` | PlayPlatform DIFF, 9 IDENTICAL | the six COM calls `*0x40, *0x60, *0xd0, *0x98, *0xa8, *0x90` became Create, SetVolume, SetFrequencyRatio, Start, Submit, Destroy in the same order; no store or compare change |
| `audio_scene_update.o` | Scene::Update DIFF, 11 IDENTICAL | `call *0x60` became `Device::SetVolumePlatform`; its `0xa88` null compare moved into the callee |
| `engine_lifecycle.o` | Engine::Shutdown DIFF, 14 IDENTICAL | `call *0x90` + `call *0x10` became `Audio::Device::PlatformShutdown`; the `0xa88` / `0xa90` stores moved into it; full call list diffed |
| `game_main.o` | RunGameMain DIFF, 8 IDENTICAL | the same two calls became PlatformShutdown; full call list diffed |
| `vtbatch7b_stream_run_win.o` | Stream::Run DIFF, 8 IDENTICAL, 1 SAME | `*0xc8` became AudioVoiceBuffersQueued, `*0xa8` became AudioVoiceSubmit, WaitForMultipleObjects became EventWaitAny. **Read:** 3 BuffersQueued call sites for the source's 4, because GCC cross-jumps the first lock / null test / GetState / unlock into the identical in-loop tail (`0x33c jmp 0x377`). The `+0x20` compare count goes 2 to 1 because the first test is now `movzbl` + register test; both loads remain |
| `audio_platform_startup.o` | PlatformStartup DIFF, PlatformShutdown ONLY-TIP (new), RtAudio_CreateXAudio2 ONLY-BASE | `*0x50` became AudioMasterCreate, `*0x10` became AudioEngineRelease; the helper is RENAMED-IDENTICAL in `audio_xaudio2_win.o` |
| `audio_stream.o` | ctor DIFF, dtor DIFF, 8 IDENTICAL, 1 RENAMED-IDENTICAL | 2 CreateEventW became 2 EventCreate; 2 CloseHandle became 2 EventClose |
| `audio_stream_play.o` | Stream::Play DIFF, 8 IDENTICAL | SetEvent became EventSet |
| `audio_voice_callback.o` | 12 IDENTICAL, OnBufferEnd SAME | the tail `jmp SetEvent` became a tail `jmp EventSet` |
| every other object | IDENTICAL (17050 functions) | including `singleton_ctors.o` and every includer of `Device.h` / `Stream.h` |

Three code-free objects (`debug.o`, `ui_operator_delete.o`, `tool_base_edit2.o`) differ only by the
length of the worktree's source-path string.

## A2: the census, and what to grep

Every `SoundBuffer::Serialize` read records the buffer's WAVEFORMATEX and its streamed flag. The music
tracks are streamed SoundBuffers, so the music is in the same table. At exit, one line per distinct format
goes to `standalone_debug.log`. It is registered with `std::atexit` right after the `[ExtLoad]` hook in
`RuntimeInit`, so it prints just before the `[ExtLoad] calls=` line. The perf exe compiles printf away;
read it from a debug run.

```
grep '^\[SoundFormat\]' "C:/GOG Games/Banished/standalone_debug.log"
```

Each line has this shape. A run that loaded no SoundBuffer prints `[SoundFormat] none (no SoundBuffer was loaded)`.

```
[SoundFormat] tag=0x0161 channels=2 rate=44100 avgBytesPerSec=... blockAlign=... bits=16 cbSize=0 streamed=0 count=...
```

Plan §0.2 holds if every line reads `tag=0x0161`. `blockAlign` and `avgBytesPerSec` are the two
parameters a WMAv2 decoder needs besides channels and rate, so the XWMA block can size its fixture
from them. `overflow` is non-zero only past 32 distinct formats.

B11 for A2: `runtime_init.o` RuntimeInit DIFF (one added `atexit` call after
RtObjectExternalLoadInstallCounters). In `vtbatch7_soundbuffer_soundeffect.o`, SoundBuffer::Serialize
is a DIFF: the census insert is inlined as table stores and key compares, and its call list is unchanged.
`DumpSoundBufferFormatCensus` is ONLY-TIP, and the other 28 functions are IDENTICAL.

## Deviations from the brief and the plan

- **`PlatformReleaseEngine` is named `Device::PlatformShutdown`.** Retail exports that function at
  0x1800de690, and its body is the inlined sequence instruction for instruction. Both callers call it.
- **The Stream's events moved to `Platform::EventHandle` in A1.** The brief allowed it; the THREADS
  hand-ins assigned them to this seam.
- **Four files moved to `src/engine/audio`**, because no OS header was left in them. The plan does not
  list the moves; the SDL build will need these files, and it does not compile `src/platform/win`.
- **The scene's attendance counter `g_volumeUpdates`** now counts the volume pushes asked for. Before, it
  counted only those that met a live voice. Nothing reads it but its own exit line.

## Finding: a port difference, fixed by `586ea9f5e` (`known_port_issues.md` row 1274)

`PlayPlatform` passes LoopCount `0xFFFFFFFF` for a looping sound, and has since it was ported. Retail
passes `0xFF`, which is `XAUDIO2_LOOP_INFINITE`. The evidence is 0x1800de90f..0x1800de91a:
`MOVZX EAX,[RCX+0x20]; NEG AL; SBB EDX,EDX; MOVZX EDX,DL; MOV [RSP+0x70],EDX`, where `[RSP+0x70]` is
LoopCount. A static_assert against `<xaudio2.h>` exposed it.

XAudio2 accepts 0..254 or 255. If it rejects the value, a looping non-streamed sound never starts: its
SubmitSourceBuffer fails, and PlayPlatform destroys the voice. A1 kept the value because it is a pure
refactor. The coordinator asked for the fix on this branch, and `586ea9f5e` passes
`Platform::kAudioLoopInfinite` (255). Row 1274 is marked FIXED.

### The expected audible difference (not run)

The loop flag is SoundBuffer +0x20, written from the `.rsc` field `bool _looping`. Every
`_looping = true` buffer in `ToolKit/resource` that is not `_streamed` is an ambient loop. Each one
reaches PlayPlatform through `AmbientEmitterComponent` (`src/game/Component/ambientemittercomponent.cpp`).

| ambient | resource | who adds it | route to hear it |
|---|---|---|---|
| map ambience: Field / Forest / Wetland / Winter | `Audio/Effects/Environment.rsc` | `StartConditions.rsc` `_mapAmbient` | any new game, camera over land |
| river | `Audio/Effects/RiverEffect.rsc` | `StartConditions.rsc` `_riverAmbient` | a new game, camera over a river |
| wind | `Audio/Effects/WindEffect.rsc` | `StartConditions.rsc` `_windAmbient` | a new game |
| rain | `Audio/Effects/RainEffect.rsc` | `Environment/Environment.rsc` weather effects | rain weather |
| fire | `Audio/Effects/FireEffect.rsc` | building templates' `ambientemitter` (`_addOnCreate = false`) | a burning building (`disaster_route.sh` fire) |
| tornado | `Audio/Effects/TornadoEffect.rsc` | the tornado | `disaster_route.sh` tornado |
| crowd | `Audio/Effects/CrowdEffect.rsc` | (not traced) | a busy town square |

If XAudio 2.7 rejected 0xFFFFFFFF, as expected, these were silent before the fix and should be audible
now. The plain new-game ambience (field / forest, river, wind) is the quickest listen.

If they were already audible before the fix, XAudio 2.7 accepted the bad value. The fix is then
inaudible, and it still matches retail's bytes.

These paths do not change:
- The streamed music: `Audio/Music/Menu.rsc` loops, but its buffer is never submitted here.
- The one-shots, which pass 0 as before.

Row 1274 was the next free number at the base. Renumber it if another wave-3 block also takes 1274.

## Open items for A4 / A5 (what an SDL backend must provide behind the same API)

1. **Every function in `platform_audio.h`**, with the same results: 0 for success, non-zero for failure.
   The callers test `!= 0`.
   - `AudioEngineCreate` writes `*out` only on success. `AudioMasterCreate` can be the opened device.
   - `AudioVoiceCreate` must write the slot it is given; PlayPlatform passes `&record->voice`.
2. **The callback contract.** Call the record's `OnBufferEnd` once per finished buffer, and
   `OnStreamEnd` after an end-of-stream buffer has played out. Call them on the backend's own thread,
   with no lock the main thread waits on.
3. **`AudioVoiceDestroy` must not return while a callback for that voice is running.**
   XAudio2's DestroyVoice guarantees this. For SDL the plan's recipe is unbind, then
   `SDL_SetAudioStreamGetCallback(s, NULL, NULL)` (it waits), then destroy. The plan marks that
   recipe unverified.
4. **`AudioVoiceBuffersQueued` counts buffers submitted and not yet finished**, the playing one
   included. The Stream worker's high-water mark (3) and its drain (0) depend on it.
5. **`AudioVoiceSubmit` receives xWMA today.** `data` / `bytes` are packets, `packetTable[packetCount]`
   is the cumulative decoded-byte table, and `playBegin` is in samples. Under the Q5 answer (decode at
   load) a SoundBuffer will hold PCM and the table becomes a sample offset. The xWMA arguments then
   change meaning; the submit signature can stay.
6. **The loop count.** Since `586ea9f5e`, "forever" arrives as `kAudioLoopInfinite` (255); any other
   non-zero value is a finite repeat count.
7. **`AudioVoiceSetFrequencyRatio`** receives `2^(pitch/12)`, capped at 2.0 by the create call's max
   ratio. **`AudioVoiceSetVolume`** receives the linear mix (XAudio2 volume).
8. **Still Windows-only in the audio path:** `vtbatch7b_stream_run_win.cpp`'s SetFilePointer, which
   belongs to the file seam. Stream::Run cannot move to `src/engine/audio` until that call is a file
   primitive.

## Shared files (merge with care)

- `src/engine/audio/Device.h` and `src/engine/audio/Stream.h`: field types and includes.
- `src/engine/engine/engine_lifecycle.cpp`: Engine::Shutdown's audio arm and one include.
- `src/game/Game/game_main.cpp`: the teardown's audio arm and one include.
- `src/app/runtime_init.cpp`: one atexit line and one include (A2).
- `src/engine/audio/SoundBuffer.h`: one declaration (A2).
- `tools/abi_audit.baseline`: win_include src/engine 10 -> 3, src/game 5 -> 4; win32_typedef
  src/engine 7 -> 5. Another block that lowers the same rows produces a conflict; take the lower count.
- `docs/known_port_issues.md`: row 1274 after row 1273.
- Makefile / CMakeLists.txt: untouched. Their globs (`src/engine/*/*.cpp`, `src/platform/win/*.cpp`,
  the CMake GLOB_RECURSE) pick up the moved and new files.

## Owed at the merge gate (coordinator)

- `sa-run` with audio audible: the menu music, in-game ambience and one-shot effects. The ambient
  loops (field / forest, river, wind) are the row-1274 listen.
- The `[SoundFormat]` lines from the new-game log.
- The resave gate and `worldcmp.sh`.
- A storm / fire route (`disaster_route.sh`) for 64-voice saturation and the reap path.
- A clean quit with music playing, which exercises PlatformShutdown on both shutdown paths.
