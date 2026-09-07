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

Still on the DLL, each with a stated reason in `audio.cpp`'s header: `Scene::Update`,
`Device::Update`, `Device::FreeChannelPlatform`. The file names have NOT been split to match the
original tree above yet -- that is a mechanical follow-up, not a port.
