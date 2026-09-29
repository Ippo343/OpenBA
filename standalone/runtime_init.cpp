// standalone/runtime_init.cpp -- the ported Runtime's one-time boot initialisation.
// See runtime_init.h.

#include "runtime_init.h"

#include "rt_libm.h"                  // RtLibm::Init
#include "system/MemoryAllocator.h"   // RtSystemAllocatorArm
#include "object/External.h"          // RtObjectExternalLoadInstallCounters
#include "video/Device.h"             // RtVideoDeviceStartupIdentity
#include "graphics/Mesh.h"            // RtGraphicsWave29SlotsSummary

#include <cstdio>
#include <windows.h>   // AFTER the runtime headers: <wingdi.h> declares a function
                       // named Rectangle (Volume::Rectangle, via graphics/Mesh.h)

bool RuntimeDllNotMapped() {
    // Nothing in the process may map the retail Runtime DLL: our ports and our
    // allocator are the runtime. A mapped copy means something in the build is
    // still pulling the DLL in, and that is worth stopping on.
    if (GetModuleHandleA("Runtime-x64.dll")) {
        fprintf(stderr, "[Standalone] FATAL: Runtime-x64.dll is mapped in the "
                        "process -- the exe must never load it (ENDGAME A.1)\n");
        fflush(stderr);
        return false;
    }
    fprintf(stderr, "[Standalone] Runtime-x64.dll: never loaded (ENDGAME A.1)\n");
    fflush(stderr);
    return true;
}

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
