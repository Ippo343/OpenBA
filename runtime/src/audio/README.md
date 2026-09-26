# runtime/src/audio — original `code\Runtime\Audio\`

Port target for the Runtime-x64.dll `Audio` directory. The file list below is the
ORIGINAL source tree recovered from `Runtime-x64-profile.dll` debug strings
(`docs/runtime_port/original_source_tree.txt`); mirror these names here, lowercased,
as `runtime/src/audio/<file>.cpp`, with binary-compatible types in
`runtime/types/audio/<Class>.h` (`static_assert` on every field offset).

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

`rt_audio_port.cpp` -- the namespace aggregator (`RtAudioPortInit`).

`Scene::Update`, `Device::Update` and `Device::FreeChannelPlatform`, once listed here as still on
the DLL, are ported (`audio_scene_update.cpp`, `audio_device.cpp`). The file names have NOT been
split to match the original tree above yet -- that is a mechanical follow-up, not a port.

## Status (ENDGAME B.3.7 closing pass, 2026-09-26)

`Audio::SoundBuffer` (CompiledExternal, 0x80), `SoundEffect` (External, 0x40), `Stream` (a `System::Thread` subclass, 2 slots, 0x88), and the non-polymorphic `Scene` (0x38), `Configuration` (0x10) and `Device` (0x1298, with its nested `AudioChannel` and `PlatformChannel` — the IXAudio2VoiceCallback record whose table is the unnamed 0x180157c60 mint row) are REAL C++ classes in `runtime/types/audio/`. The XAudio2 callback bodies stay in `audio_voice_callback_win.cpp`; the `Stream` / `Device` ctors stay doors (`singleton_ctors.cpp`), and no lock was added anywhere (B37_RULES 5). Rules and history: `docs/runtime_port/endgame_b/B37_RULES.md`; numbers: `docs/runtime_port/endgame_b/b37_session/CLOSE_NUMBERS.md`.
