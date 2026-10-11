# src/engine/audio — original `code\Runtime\Audio\`

Port target for the Runtime-x64.dll `Audio` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/history/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `src/engine/audio/<file>.cpp`, with binary-compatible types in
`src/engine/audio/<Class>.h` (`static_assert` on every field offset).

A `_win` file is a PLATFORM SEAM (PLAN.md §4.1 decision 1): keep `<windows.h>` out of
every other file in this directory so a `_posix`/`_sdl` twin can be written later.
A `_compile` file is the offline asset compiler and is likely absent from retail —
confirm against the retail DLL before porting one.

## Original files (5 cpp, 5 h)

- `AudioConfiguration.cpp`
- `AudioConfiguration.h`
- `AudioDevice.cpp`
- `AudioDevice.h`
- `AudioDevice_win.cpp`
- `AudioScene.cpp`
- `AudioScene.h`
- `SoundBuffer.h`
- `SoundBuffer_win.cpp`
- `SoundEffect.h`

## Ported here

`audio.cpp` -- `Audio::Scene` and `Audio::Configuration`, 17 of the 22 Audio exports our tree
calls. Wave 8 lane C landed 13; the Wave 8.5 close-out added the channel-teardown family
(`Pause`, `PauseEffect`, `StopEffect`, `StopEffect3D`, `StopAll`). Its oracle is
`ProbeAudioScene` in `standalone/rt_orphan_probe.cpp`, which steps two synthetic Scenes side by
side -- see the block comment above those bodies for why that works without a Device fake.

`Scene::Update`, `Device::Update` and `Device::FreeChannelPlatform`, once listed here as still on
the DLL, are ported (`audio_scene_update.cpp`, `audio_device.cpp`). The file names have NOT been
split to match the original tree above yet -- that is a mechanical follow-up, not a port.

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

`Audio::SoundBuffer` (CompiledExternal, 0x80), `SoundEffect` (External, 0x40), `Stream` (a `System::Thread` subclass, 2 slots, 0x88), and the non-polymorphic `Scene` (0x38), `Configuration` (0x10) and `Device` (0x1298, with its nested `AudioChannel` and `PlatformChannel` — the IXAudio2VoiceCallback record whose table is the unnamed 0x180157c60 mint row) are REAL C++ classes in `src/engine/audio/`. The XAudio2 callback bodies stay in `audio_voice_callback.cpp`; the `Stream` / `Device` ctors stay doors (`singleton_ctors.cpp`), and no lock was added anywhere (B37_RULES 5). Rules and history: `docs/history/endgame_b/B37_RULES.md`; numbers: `docs/history/endgame_b/b37_session/CLOSE_NUMBERS.md`.

## The platform seam (Phase D.4 A1, 2026-10-10)

No file in this directory includes `<windows.h>` or `<xaudio2.h>`. The XAudio 2.7 device, master voice and
source voices are the opaque handles of `src/platform/common/platform_audio.h`, and every COM call lives in
`src/platform/win/audio_xaudio2_win.cpp`. `Device::PlatformChannel` implements `Platform::AudioVoiceCallback`,
the seven-slot contract a backend calls from its audio thread. The Stream's two events are
`Platform::EventHandle`. `PlatformStartup` / `PlatformShutdown`, the voice callbacks and the Stream ctor, dtor
and `Play` moved here from `src/platform/win/`. `Stream::Run` stays in
`src/platform/win/vtbatch7b_stream_run_win.cpp` for its `SetFilePointer` (the file seam's).

## SDL3 audio and decode at load (Phase D.4 SDL-4, 2026-10-11)

The XAudio 2.7 backend is gone: `src/platform/win/audio_xaudio2_win.cpp` was deleted, and every function of
`platform_audio.h` is implemented over SDL3 audio streams in `src/platform/sdl/audio_sdl.cpp`, on every
platform. One stream per voice, bound to the default playback device while the voice is started; SDL
converts and resamples to the device. The XAudio2 source-voice queue SDL lacks (PlayBegin, loop counts,
END_OF_STREAM, BuffersQueued, one OnBufferEnd per buffer) is `src/platform/common/audio_feed.{h,cpp}`, OS-free
and unit tested. The backend's banner holds the proof, from SDL's source, that `AudioVoiceDestroy` does not
return while a callback for the voice runs.

SDL cannot decode xWMA, which every shipped sound is. Per the user's answer to D4_RULES §3 Q5:

- **One-shots decode at load.** `SoundBuffer::Serialize`'s read path ends with `DecodeAtLoad`: a non-streamed
  0x0161 buffer is decoded whole to S16 PCM with `XwmaDecoder` (`xwma_decode.h`), the PCM replaces the packets
  in `_data`, `_format` is rewritten as PCM, and retail's pad at +0x44 keeps the xWMA packet size and the frame
  length. PlayPlatform's PlayBegin arithmetic stays retail's; `audio_pcm.h` turns its result into the PCM sample
  XAudio2 was measured to start at. Memory: the 25 SoundBuffers found in the clear take 44.4 MB as PCM against
  1.87 MB as xWMA; `Rain.wav` alone is 9.9 MB. The exit line `[SoundDecode]` in `standalone_debug.log` has the
  run's totals.
- **The music decodes chunk-ahead on the Stream worker.** `Stream::Run` (now `audio_stream_run.cpp`, its
  `SetFilePointer` is `Platform::FileSeek32`) decodes each 16-packet block with `XwmaDecoder::DecodeChunk`
  and submits the PCM. Nothing is decoded on the audio thread.

Recorded differences: `docs/known_port_issues.md` row 1276 (Tornado's invalid packets); the hand-in
`docs/runtime_port/phase_d/handins/SDL-4.md` lists the rest.
