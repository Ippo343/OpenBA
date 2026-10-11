# SDL-4 hand-in (Phase D.4 wave 4, branch `d4-sdl4`, base `a125c96bb`)

Plan: `plans/SDL.md` Block 4 steps A4 and A5, re-shaped by D4_RULES §3 Q5 (decode at load; the music
chunk-ahead on the Stream worker). Plus Stream::Run's `SetFilePointer`, the Tornado row and the README.

**Static gate only.** The game, `sa-run` and `sa-deploy` were never run. Nothing in this block has been
heard. The listen checklist below is the coordinator's.

**Final gate** (tip after the second `Merge branch 'main'`, main with GAPS and SDL-5 D1 merged in):

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0 |
| unit | 1000 / 0 (990 + 4 `audio_pcm_*` + 6 `audio_feed_*`) |
| CMake twin, `-t unit` (Ninja, debug, `-j 2`) | built from an empty build dir; 1000 / 0 |
| warnings | 0 new against the main checkout's `warn_baseline_{debug,perf}` |
| castmetric (`src`) | 2; the two new casts are `CAST-OK: byte_io` and `CAST-OK: user_data` |
| `tools/abi_audit.py` | OK; no row moved |
| exe imports (`objdump -p`, perf) | no XAudio2, no new DLL; SDL3's build config has WASAPI and DSOUND |

## Commits

| commit | title | B11 (-O2, vs `runs/b11base_d4d/perf/obj`) |
|---|---|---|
| `882bf97e5` | refactor: Stream::Run portable -- SetFilePointer -> Platform::FileSeek32, file to src/engine/audio | `audio_stream_run.o` (paired with the base `vtbatch7b_stream_run_win.o`): Run DIFF in the accepted shape, the import call `call *0x0(%REG)` became `Platform::FileSeek32`, nothing else; 8 IDENTICAL, 1 RENAMED-SAME (the static init, named after the file). The rest of the tree: 17147 IDENTICAL |
| `f63e6bc61` | behaviour: decode xWMA SoundBuffers to S16 PCM at load (D4_RULES Q5); PlayBegin becomes a PCM sample | PlayPlatform DIFF: one added call `XwmaPcmPlayBegin` and one added 16-bit compare of +0x44; the voice-call order unchanged. Serialize DIFF: one added call `DecodeAtLoad`; the inlined census row's three stores all moved by +0x1c, same count. `DecodeAtLoad` ONLY-TIP. `audio_xaudio2_win.o` Submit SAME. `audio_pcm.o` new. The rest: 17152 IDENTICAL |
| `1fe6d87dd` | behaviour: SDL3 audio backend (one-shots A4 + music stream A5); the XAudio2 backend deleted | Run DIFF: added `XwmaDecoder::Open`, `DecodeChunk`, `MaxSamplesFor`, `~XwmaDecoder`, the ring's one-time static init; Submit's arguments; a compare against 0x79000 (the slot size); every lock, wait, BuffersQueued, Submit and CloseFile in retail's order. `__tcf_0` DIFF and `__tcf_1` ONLY-TIP are the ring's static destructor. PlayPlatform: Submit has 6 arguments. `audio_sdl.o` and `audio_feed.o` are new; `audio_xaudio2_win.o` is gone. The rest: 17140 IDENTICAL |
| `cb5876377` | docs: known_port_issues row 1276 (Tornado's invalid packets); audio README for SDL-4 | docs only |
| `70e24171a` | Merge branch 'main' into d4-sdl4 | keep-both in `known_port_issues.md` (main's 1275 text, then 1276). After the merge, B11 shows GAPS' own DIFFs in `mainmenudialog.o` and `game_main.o` and mine unchanged |
| `e502549aa` | Merge branch 'main' into d4-sdl4 (SDL-5 D1, `d5c90621c`) | keep-both in `known_port_issues.md`: rows 1276, 1277, 1278 in order. Re-gated on the merged tree: debug, perf and unit EXIT 0; unit 1000 / 0; 0 new warnings; castmetric 2; abi_audit OK. B11 on the audio objects unchanged from `1fe6d87dd`'s verdicts. No audio file is left in `src/platform/win`: `audio_xaudio2_win.cpp` is deleted, so SDL-5 D2 (the COM init) is unblocked once this merges |

## Design decisions

### SDL locking: why AudioVoiceDestroy cannot return while a callback runs

This was read in SDL 3.4.18's source (`third_party/SDL/src/audio/SDL_audio.c`, `SDL_audiocvt.c`). The plan
had marked it UNVERIFIED.

1. The get-callback runs inside `SDL_GetAudioStreamDataAdjustGain` with the stream's mutex held.
2. The device thread calls that for every bound stream inside `SDL_PlaybackAudioThreadIterate`. It holds the
   physical device's mutex for the whole mixing pass.
3. `SDL_UnbindAudioStreams` locks the physical device mutex first, then the stream's, and only then unlinks.
   An unbind therefore waits out a running callback. Afterwards the device thread no longer sees the stream.
4. `SDL_SetAudioStreamGetCallback(s, NULL, NULL)` takes the stream mutex. That is a second wait, and no
   callback can start after it.
5. `SDL_DestroyAudioStream` then unbinds, which is now a no-op, and frees. On its own it would also wait,
   because its unbind takes the device lock. The explicit order does not rely on that.

`AudioVoiceDestroy` is exactly unbind, then clear the callback, then destroy. The proof is in
`audio_sdl.cpp`'s banner.

SDL mutexes are recursive (`src/thread/windows/SDL_sysmutex.c`: owner plus count). The callback can
therefore call `SDL_PutAudioStreamData`, `SDL_FlushAudioStream` and `SDL_GetAudioStreamAvailable` on its own
stream, which SDL's header documents as intended.

**The callback contract.** The record's virtuals run under SDL's stream mutex. Two of them do anything:
`OnBufferEnd` is an `EventSet` and `OnStreamEnd` is a non-atomic `flags |= 2` (retail's race, kept). Neither
takes a lock, so no deadlock can arise. The main thread's gain and ratio calls wait at most one callback.
No lock was added to engine code.

### The buffer queue (`src/platform/common/audio_feed.{h,cpp}`, OS-free, unit tested)

- **On-demand puts.** Bytes go to SDL only when the get-callback asks for them (`additional`). A buffer is
  finished when its last byte is handed over. That is at most one device period before XAudio2 would call it
  played.
- **BuffersQueued** counts buffers submitted and not finished, the playing one included. The Stream worker's
  high-water mark 3 and its drain to 0 work unchanged.
- **OnStreamEnd** fires once. The END_OF_STREAM buffer must have been handed over and the stream flushed. Then
  a later pass must find `SDL_GetAudioStreamAvailable == 0`, which is the output side.
  `SDL_GetAudioStreamQueued` counts input and would fire early.
- **Loops** restart at byte 0, as measured in XWMA A4.5. 255 means forever, and n means n extra passes. A
  zero-length buffer never loops.
- At most 64 buffers can be queued, as XAudio2 allows. The 65th submit is refused, as XAudio2 refuses it.

### The PCM start rule (decode at load)

PlayPlatform's seek-table search and `nBlockAlign * idx` stay retail's. For a decoded buffer, `nBlockAlign`
is the xWMA packet size, because the format's blockAlign is the PCM's now. `DecodeAtLoad` records the packet
size and the WMA frame length in retail's never-written pad at SoundBuffer +0x44 (`_decodedXwma`; the
offset and size are asserted).

`Audio::XwmaPcmPlayBegin` (`audio_pcm.h`) then applies the rule measured in XWMA A4.3. Let k be the packet
whose table range holds PlayBegin. The start is PlayBegin when k = 0, and PlayBegin + FrameLength when
k > 0, clamped to the PCM length. The seek table stays in memory, because the search reads it. The packets
are freed.

### The music stream

- **Decoder.** `Stream::Run` creates one `XwmaDecoder` per Run, that is per track, with
  `Open(format, nullptr, 0, table, segCount)`. Each 16-packet Block it reads is decoded with
  `DecodeChunk(curSeg, nPackets, block->packets, …)` on the worker. The PCM is then submitted.
- **Fresh passes.** A resumed track (`curSeg > 0`) and the loop wrap to packet 0 are both fresh decoder
  passes, per XWMA A5.7.
- **The PCM ring is not a Stream member.** The backend references a submitted buffer until it finishes, as
  XAudio2 referenced `Block::packets`. A track change returns from Run with buffers still queued on the old
  voice, so the ring must not be freed or moved. The Stream's layout is retail's: 0x88 bytes at
  Device+0xa00, and `_pad81` is 7 bytes, too small for a pointer. There is one Stream, so the ring belongs to
  `audio_stream_run.cpp`. It has 4 slots, the Block ring's shape, sized once at the largest
  `MaxSamplesFor(16)` for the formats the decoder accepts: **7.9 MB**, allocated on the first music play.
- **A known hazard, the same as retail's.** After a track change the new Run overwrites slots the old voice
  may still have queued. The old voice is being stopped, and retail's Block ring has the same overlap.

### Stop and Start

Start is `SDL_BindAudioStream` and Stop is `SDL_UnbindAudioStream`. Stop returns after the device thread's
current pass, so consumption stops promptly. Up to one period SDL already converted stays in the stream, and
it would play if the voice were started again. Banished never restarts a stopped voice: StopPlatform marks
it for the reap.

### Engine, master, shutdown

- **Engine** is `SDL_InitSubSystem(SDL_INIT_AUDIO)`.
- **Master** is `SDL_OpenAudioDevice(DEFAULT_PLAYBACK, NULL)`.
- **Failure.** Either failure returns non-zero and leaves `*out` untouched. That is a silent game with one
  stderr line, `[Audio] SDL_... failed: …`.
- **Shutdown.** `PlatformShutdown` closes the device, which unbinds every stream, then quits the subsystem.
  Voices set `SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN` false, so the quit does not free them. The Stream
  worker may call BuffersQueued until `~Stream` joins it. Retail's XAudio2 voices die with the engine
  instead.

## Recorded behaviour differences (Windows-observable)

1. **Tornado.wav** ends 5,632 samples earlier and silent on each loop pass. Retail plays about 5,000 frames
   of full-scale noise there. This is row 1276. **Hoe2.wav**'s near-silent tail differs by up to 12 LSB; it
   is in the same row's notes.
2. **The music loop wrap is a fresh pass.** XAudio2 dropped the first 512 samples after the wrap on 2 of 3
   measured sounds (XWMA A5.8). Three data points are not a rule, so it was not copied. **This is a
   measure-on-Windows candidate:** the coordinator decides whether to measure the music tracks.
3. **OnBufferEnd fires up to one device period early.** The Stream worker refills that much sooner.
4. **Volume** is applied at SDL's next pull, not at XAudio2's next 10 ms pass.
5. **The frequency ratio** is clamped to [0.01, 2.0]. XAudio2's floor is 1/1024. The game's ratios are
   `2^(pitch/12)` for small pitches, so the floor is never reached.
6. **The output device.** SDL plays through WASAPI, with DirectSound as fallback, at the device's own
   format, and it follows a default-device change. XAudio 2.7 needed the DirectX June 2010 runtime, and a
   machine without it was silent.
7. **Memory.** Decoded one-shots use about 44.4 MB of PCM against 1.87 MB of xWMA for the 25 SoundBuffers
   found in the clear; Rain.wav alone is 9.9 MB. The music ring adds 7.9 MB. The user decided the PCM figure
   under Q5.
8. **A refused xWMA one-shot** (none ships) becomes an empty PCM buffer and plays nothing. A refused
   streamed track skips its pass. The counters for both are below.
9. **A decoded SoundBuffer in memory is PCM.** If anything wrote one back through Serialize's write path, it
   would write PCM. The game never writes SoundBuffers: they are resources, not save data. That is a tool
   path only.

## Deviations from the plan / brief

- **New files beyond the owned list:** `src/platform/common/audio_feed.{h,cpp}` (the OS-free queue, so it
  could be unit tested without SDL), `src/engine/audio/audio_pcm.{h,cpp}`, and two unit test files.
  Makefile and CMakeLists.txt were not touched; their globs pick the files up.
- **The PCM ring is per process,** not per Stream object, for the layout reason above. It is allocated once
  and never per block.
- **The decoder is per Run (per track),** as the brief says ("Open per track").
- **`SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES` is not set.** The plan called it optional, and many platforms
  ignore it.
- **`Platform::ComInit` stays.** Dropping it is SDL-5 D2. SDL's WASAPI driver does its own COM init on its
  threads.
- **The XAudio2 backend passed through one interim commit.** In `f63e6bc61` it learned to submit a null
  XAUDIO2_BUFFER_WMA for PCM. Step 2 is runnable on XAudio2 on its own, and it can be bisected.
- **A stale comment outside my files:** `src/engine/graphics/batchc_gfx_leaves.cpp:166` still names
  `vtbatch7b_stream_run_win.cpp`. It was left alone.

## Open items for the coordinator

### LISTEN checklist (debug or perf exe, after the merge)

1. **Menu music** (`sa-run-menu`): it starts, and it is not pitched or sped up, because SDL resamples to the
   device. Leave it running past the end of the track. The menu track loops: listen at the wrap for a click
   or a gap (item 2 above).
2. **New game** (`sa-run`): the ambient loops (field or forest, river, wind) are audible and loop without a
   gap. This also covers row 1274's listen.
3. **One-shots:** UI clicks, tool sounds (the hoe), animals. Each should play once and stop, with no tail
   and no repeat.
4. **Track change:** from menu music to in-game music, and between in-game tracks. The old track stops, the
   new one starts, and nothing stutters or plays twice.
5. **The pause menu** (Esc). GAPS' OnOpened now pauses the audio scene. Check that ambience and music pause
   and resume.
6. **The disaster route** (`disaster_route.sh`): the tornado loop (row 1276: no noise burst at the loop
   end), the fire loop, and rain. Check 64-voice saturation: no crash, voices reaped (the
   `voicesCreated` / `placed` counts climb).
7. **Quit with music playing,** from the pause menu and with the window close. Expect exit code 0, no hang
   (`standalone_hang.log` absent), and no crash log. This covers PlatformShutdown, then ~Stream joining
   the worker.
8. Optional: switch the default Windows output device during play. SDL should follow it, where retail did
   not.

### What to grep

All of these are in `C:/GOG Games/Banished/`.

```
grep '^\[SoundDecode\]' standalone_debug.log      # decoded=N refused=0 short=1 (Tornado) xwmaBytes / pcmBytes = the memory figure
grep '^\[SoundFormat\]' standalone_debug.log      # AUDIO-A census, unchanged: the formats as stored (xWMA)
grep 'Audio::Stream::Run ATTENDANCE' standalone_debug.log   # opened / chunksSubmitted > 0, undecodable=0
grep 'PlayPlatform ATTENDANCE' standalone_debug.log         # voicesCreated == placed (a gap = a failed SDL call)
grep '^\[Audio\]' standalone_error.log            # must be empty (SDL init / device open failures)
```

The perf exe compiles printf away, so read these lines from a debug run.

### To measure on Windows (not done: no game run)

- The music loop wrap: whether XAudio2 drops 512 samples at the wrap on the shipped music tracks (XWMA
  A5.8).
- A looping one-shot with a non-zero PlayBegin on XAudio2 (XWMA A4.5 measured PlayBegin 0 only).

### Shared files touched

- `docs/known_port_issues.md`: row 1276 after 1275 (keep-both resolved at the main merge).
- `tests/unit/sources.mk`: two families appended. `tests/unit/sources.cmake` was regenerated with
  `tools/restructure/sources_mk_to_cmake.py`.
- `src/platform/common/platform_audio.h`: the Submit signature changed (6 arguments), plus comments.
  LINUX-1 and SDL-5 consumers should merge main after this lands.
- `src/engine/audio/SoundBuffer.h`: `_pad44` became `_decodedXwma` (same offset and size, asserted).
