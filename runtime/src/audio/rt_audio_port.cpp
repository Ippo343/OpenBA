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
    printf("[RtPort] Audio: %d/22 exports registered "
           "(7 held back, 1 phantom name fixed, 1 data export -- see WAVE8_LANE_C.md)\n", n);
}
