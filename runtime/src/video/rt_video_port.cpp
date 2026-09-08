// ============================================================================
// runtime/src/video/rt_video_port.cpp — RtVideoPortInit()  Wave 6 (Video)
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   BANISHED_RT_VIDEO=0      leave the whole Video namespace on the DLL
//   BANISHED_RT_VIDCONTEXT=0 ... just Video::Context            (lane A)
//   BANISHED_RT_VIDDEVICE=0  ... just Video::Device/Configuration/RenderBuffer (lane B)
//   BANISHED_RT_VIDBUFFERS=0 ... just Video::ImageBuffer/Index/VertexBuffer (lane C)
//   BANISHED_RT_VIDCOMPANION=0 ... just the twelve Video::*Data companion bases,
//                            the buffer Lock/Unlock/Bind trampolines and
//                            Device::DisplayMode          (Wave 9 lane B2)
//
// Each lane owns exactly one .cpp and exposes one Register function returning
// the number of RegisterRuntime* calls it made. A lane with nothing landed yet
// returns 0 from a weak stub below.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtVideoContextRegister();   // context.cpp  (lane A)
extern int RtVideoDeviceRegister();    // device.cpp   (lane B)
extern int RtVideoBuffersRegister();   // buffers.cpp  (lane C)
extern int RtVideoCompanionRegister(); // companion_data.cpp (Wave 9 lane B2)

void RtVideoPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_VIDEO")) {
        printf("[RtPort] BANISHED_RT_VIDEO=0 -- Video namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_VIDCONTEXT")) n += RtVideoContextRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDDEVICE"))  n += RtVideoDeviceRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDBUFFERS")) n += RtVideoBuffersRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDCOMPANION")) n += RtVideoCompanionRegister();
    printf("[RtPort] Video: %d exports registered\n", n);
}
