// ============================================================================
// runtime/src/audio/rt_audio_port.cpp — RtAudioPortInit()   Wave 8, lane C
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   RT_AUDIO=0   leave the whole Audio namespace on the DLL
//
// One .cpp owns this lane's whole scope (Audio::Scene + Audio::Configuration),
// so there is only one Register function to aggregate, mirroring
// runtime/src/video/rt_video_port.cpp's shape for the integrator.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtAudioSceneRegister();          // audio.cpp
// WAVE 28 lane MISC: ?Update@Scene@Audio@@ (0x1800df6c0, 1816 bytes), the one
// unported Audio BODY row. Its OWN switch, RT_AUDIOSCENEUPDATE
// (default ON, owned inside the file), so this single large per-frame body can
// be bisected out without taking audio.cpp's sixteen ports with it.
extern int RtAudioSceneUpdateRegister();     // audio_scene_update.cpp
extern int RtAudioConfigurationRegister();  // audio.cpp
// Wave 9 lane IOA: Audio::Device::Update + ::FreeChannelPlatform. Their own
// .cpp because they need <windows.h> for the Device+0xA50 critical section and
// audio.cpp must stay free of it (PLAN.md §4.1 decision 1).
extern int RtAudioDeviceRegister();         // audio_device.cpp
extern int RtAudioStreamRegister();         // audio_stream_win.cpp (Wave 24)
// Wave 28 lane SLOTS: the 7 slots of the UNEXPORTED XAudio2 voice-callback
// vtable 0x180157c60, one record per channel inside Audio::Device. Own file and
// own switch: address doors only (nothing here is exported) and it needs
// <windows.h> for SetEvent, which audio.cpp must stay free of.
extern int RtAudioVoiceCallbackRegister();  // audio_voice_callback_win.cpp
// WAVE 29 lane AVBODY: Scene::GetAttenuatedVolume + Scene::OnSoundEnded (two of
// Audio's three remaining BODY rows), plus Scene::GetVolumeForPosition -- which
// was NOT on the work list. It is the helper GetAttenuatedVolume calls twice,
// and it is EXPORTED (ord 3027): `AEBA` means PRIVATE, not unexported. Porting
// it by name rather than inlining it keeps this lane's address-door count at
// ZERO. Own switch so the three can be bisected out together.
extern int RtAudioWave29AvbodyRegister();   // wave29_lane_avbody.cpp
// batch 7 (Input/Audio platform leaves, 2026-09-12): Audio's last two
// unported BODY rows. `Stream::Play` is its own `_win.cpp` (KERNEL32 sync
// primitives, same <windows.h> seam as audio_stream_win.cpp);
// `Device::PlayPlatform` is platform-neutral (pure XAudio2 vtable calls
// through function pointers) and needs no <windows.h> of its own, matching
// audio_scene_update.cpp's own file split.
extern int RtAudioStreamPlayRegister();     // audio_stream_play_win.cpp
extern int RtAudioPlayPlatformRegister();   // audio_play_platform.cpp

// BATCH C registration entry point (batchc_*.cpp).
extern "C" int RtAudioBatchCLeavesRegister();

void RtAudioPortInit() {
    // BATCH C — ~SoundBuffer and Device::OnSoundBufferDeleted. Unconditional,
    // see DIRECTION.md §2.3.
    {
        printf("[RtPort] BATCH C: %d audio leaves registered\n",
               RtAudioBatchCLeavesRegister());
    }
    int n = 0;
    n += RtAudioSceneRegister();
    n += RtAudioSceneUpdateRegister();   // owns RT_AUDIOSCENEUPDATE
    n += RtAudioConfigurationRegister();
    n += RtAudioDeviceRegister();
    // Wave 24 lane SMALLBODY: Audio::Stream's ctor and dtor (0x1800a0e30 /
    // 0x1800a0d90). Own file + own switch -- both bodies are KERNEL32
    // synchronisation and audio.cpp must stay free of <windows.h>.
    n += RtAudioStreamRegister();
    n += RtAudioVoiceCallbackRegister();
    // batch 7: Audio's last two unported BODY rows. Registered unconditionally
    // (DIRECTION.md: a port registers unconditionally, no new kill switches).
    n += RtAudioStreamPlayRegister();
    n += RtAudioPlayPlatformRegister();
    // BATCH 8b: the EffectSlot serialize pair (0x180052640 / 0x180051e00), the
    // two address doors audio.cpp's Scene::Serialize banner left open.
    // Unconditional, same reason as the two rows above.
    extern int RtAudioEffectSlotSerializeRegister();  // audio_effectslot_serialize.cpp
    n += RtAudioEffectSlotSerializeRegister();
    // VTABLE BATCH 7 (2026-09-12): Audio::SoundBuffer + Audio::SoundEffect
    // (vtbatch7_soundbuffer_soundeffect.cpp). Unconditional, same reason.
    extern int RtAudioBatch7SoundBufferSoundEffectRegister();
    n += RtAudioBatch7SoundBufferSoundEffectRegister();
    // VTABLE BATCH 7b (2026-09-12): ?Run@Stream@Audio@@ (0x1800a0f60), the
    // streaming worker's thread body — the last unported member of the
    // Audio::Stream trio whose ctor/dtor (audio_stream_win.cpp) and Play
    // (audio_stream_play_win.cpp) were already ours. Unconditional.
    { extern int RtAudioStreamRunRegister();
      n += RtAudioStreamRunRegister(); }
    // Wave 29 lane AVBODY.
    n += RtAudioWave29AvbodyRegister();
    // The denominator was a hardcoded 22 -- Wave 8's own scope -- and Wave 9
    // lane W adding Scene::FindFree3DEffect made "20/22" arithmetic that no
    // longer resolves against the three-held-back / one-phantom / one-data
    // breakdown beside it. Dropping it rather than bumping it: a count of what
    // registered is a fact this line can state, and a denominator that has to be
    // re-derived every time a lane lands is a comment on its way to becoming a
    // lie (commit e9d81d9 is this project fixing three of those).
    printf("[RtPort] Audio: %d exports registered "
           "(WAVE8_LANE_C.md for Wave 8's set, audio.cpp's header for the four "
           "that moved in Wave 8.5 and for Wave 9 lane W's FindFree3DEffect; the "
           "held-back set is listed in audio.cpp, not counted here)\n", n);
}
