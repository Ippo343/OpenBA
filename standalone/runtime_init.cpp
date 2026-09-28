// standalone/runtime_init.cpp -- the ported Runtime's one-time boot initialisation.
// See runtime_init.h.

#include "runtime_init.h"

#include "rt_libm.h"                  // RtLibm::Init
#include "system/MemoryAllocator.h"   // RtSystemAllocatorArm
#include "object/External.h"          // RtObjectExternalLoadInstallCounters
#include "video/Device.h"             // RtVideoDeviceStartupIdentity
#include "graphics/Mesh.h"            // RtGraphicsWave29SlotsSummary

void RuntimeInit() {
    // The transcendental seam every Math / Volume body calls through.
    RtLibm::Init();
    // The platform seam, the host's cutover armer, `[RtAlloc] ARMED`.
    RtSystemAllocatorArm();
    // The `[ExtLoad] calls=` / `[ExtAdd]` / `[ExtSaveText]` exit lines.
    RtObjectExternalLoadInstallCounters();
    // The run-identity line (device_startup.cpp's own build stamp).
    RtVideoDeviceStartupIdentity();
    // The `[SLOTS29]` attendance line, printed at boot so a run always shows it.
    RtGraphicsWave29SlotsSummary();
}
