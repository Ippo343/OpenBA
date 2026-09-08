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
//   BANISHED_RT_VIDDYNAMIC=0 ... just Device::{Lock,Unlock,Release/RecreateDynamic},
//                            the eight class-static list walkers and
//                            RenderBuffer::Recreate       (Wave 9 lane VID)
//
// Each lane owns exactly one .cpp and exposes one Register function returning
// the number of RegisterRuntime* calls it made. A lane with nothing landed yet
// returns 0 from a weak stub below.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"

extern int RtVideoContextRegister();   // context.cpp  (lane A)
extern int RtVideoDeviceRegister();    // device.cpp   (lane B)
extern int RtVideoBuffersRegister();   // buffers.cpp  (lane C)
extern int RtVideoCompanionRegister(); // companion_data.cpp (Wave 9 lane B2)
extern int RtVideoDynamicResourcesRegister();  // dynamic_resources.cpp (Wave 9 lane VID)
extern int RtVideoContextLifecycleRegister();  // context_lifecycle.cpp (Wave 9 lane VID2)
extern "C" void RtVideoContextLifecycleProbe();

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
    if (RtWaveEnabled("BANISHED_RT_VIDDYNAMIC"))   n += RtVideoDynamicResourcesRegister();
    // Wave 9 lane VID2. The probe runs only when BANISHED_RT_CTXCLEAR_PROBE is
    // set: it makes 513 calls into the shipped ?ClearResources@ and is a
    // verification step, not boot work. It allocates nothing (two .bss
    // Contexts), so it is safe at this point in boot, but it is still opt-in.
    if (RtWaveEnabled("BANISHED_RT_VIDCTXLIFE")) {
        n += RtVideoContextLifecycleRegister();
        const char* probe = std::getenv("BANISHED_RT_CTXCLEAR_PROBE");
        if (probe && *probe && std::strcmp(probe, "0") != 0)
            RtVideoContextLifecycleProbe();
    }
    printf("[RtPort] Video: %d exports registered\n", n);
}
