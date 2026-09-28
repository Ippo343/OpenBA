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
#include "../../types/audio/Device.h"   // ENDGAME B.3.9 ENG: the storage redirect
#include "../../types/audio/Audio.h"   // ENDGAME B.3.9 ENG: the storage redirect

extern int RtAudioSceneRegister();          // audio.cpp

void RtAudioPortInit() {
    // B3R (registry teardown, W1 RT-Core): the other nine register functions
    // registered only rows no code read; they went with their rows.
    int n = 0;
    n += RtAudioSceneRegister();
    printf("[RtPort] Audio: %d exports registered\n", n);
}
