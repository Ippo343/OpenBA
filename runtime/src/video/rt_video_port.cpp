// ============================================================================
// runtime/src/video/rt_video_port.cpp — RtVideoPortInit()  Wave 6 (Video)
// ============================================================================
// Each file that still carries a registration exposes one Register function
// returning the number of RegisterRuntime* calls it made. B3R RT-VG: a file
// whose every row went DEAD lost its Register function and its call here.
// B3R W2-RT: the companion-data, context ctor / lifecycle, device enum types,
// interfaces, resources, SaveImage and ImageBuffer-serialize register
// functions went with their last rows (none had a reader).
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int  RtVideoBuffersRegister();        // buffers.cpp  (lane C)
extern void RtVideoDeviceStartupIdentity();  // device_startup.cpp (Wave 9 lane VID2)

void RtVideoPortInit() {
    int n = 0;
    n += RtVideoBuffersRegister();
    // Wave 9 lane VID2's run-identity line, printed where its (now deleted)
    // register function printed it.
    RtVideoDeviceStartupIdentity();

    printf("[RtPort] Video: %d exports registered\n", n);
}
