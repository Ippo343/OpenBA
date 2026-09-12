// ============================================================================
// runtime/src/audio/rt_audio_port.cpp — RtAudioPortInit()   Wave 8, lane C
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   BANISHED_RT_AUDIO=0   leave the whole Audio namespace on the DLL
//
// One .cpp owns this lane's whole scope (Audio::Scene + Audio::Configuration),
// so there is only one Register function to aggregate, mirroring
// runtime/src/video/rt_video_port.cpp's shape for the integrator.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtAudioSceneRegister();          // audio.cpp
// WAVE 28 lane MISC: ?Update@Scene@Audio@@ (0x1800df6c0, 1816 bytes), the one
// unported Audio BODY row. Its OWN switch, BANISHED_RT_AUDIOSCENEUPDATE
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

void RtAudioPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_AUDIO")) {
        printf("[RtPort] BANISHED_RT_AUDIO=0 -- Audio namespace port disabled\n");
        return;
    }
    int n = 0;
    n += RtAudioSceneRegister();
    n += RtAudioSceneUpdateRegister();   // owns BANISHED_RT_AUDIOSCENEUPDATE
    n += RtAudioConfigurationRegister();
    n += RtAudioDeviceRegister();
    // Wave 24 lane SMALLBODY: Audio::Stream's ctor and dtor (0x1800a0e30 /
    // 0x1800a0d90). Own file + own switch -- both bodies are KERNEL32
    // synchronisation and audio.cpp must stay free of <windows.h>.
    n += RtAudioStreamRegister();
    n += RtAudioVoiceCallbackRegister();
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
