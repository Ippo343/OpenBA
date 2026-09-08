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
extern int RtAudioConfigurationRegister();  // audio.cpp

void RtAudioPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_AUDIO")) {
        printf("[RtPort] BANISHED_RT_AUDIO=0 -- Audio namespace port disabled\n");
        return;
    }
    int n = 0;
    n += RtAudioSceneRegister();
    n += RtAudioConfigurationRegister();
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
