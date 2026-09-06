// ============================================================================
// runtime/src/system/rt_system_port.cpp — RtSystemPortInit()  (Wave 2)
// ============================================================================
// One entry point for the System namespace, mirroring RtMathPortInit(). Each
// lane owns exactly ONE file and exposes exactly ONE Register function, so no
// two lanes ever edit the same source; this file and rt_port.h are the only
// shared surface and they are already written.
//
// Switches, outermost first:
//   BANISHED_RT_SYSTEM=0   leave the whole System namespace on the DLL
//   BANISHED_RT_STRING=0   ... just System::String
//   BANISHED_RT_FILENAME=0 ... just System::Filename
//   BANISHED_RT_VALUE=0    ... Random / Time / DateTime / Color / Configuration
//   BANISHED_RT_THREAD=0   ... CriticalSection / Thread
//   BANISHED_RT_ALLOC=1    ... MemoryAllocator: OPT-IN, see allocator.cpp.
//                              Never default-on: registering MemAlloc without a
//                              detour means we allocate and the DLL frees.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtSystemStringRegister();      // string.cpp
extern int RtSystemFilenameRegister();    // filename.cpp
extern int RtSystemValueRegister();       // value_types.cpp
extern int RtSystemThreadRegister();      // thread_win.cpp
extern int RtSystemAllocatorRegister();   // allocator.cpp

void RtSystemPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_SYSTEM")) {
        printf("[RtPort] BANISHED_RT_SYSTEM=0 -- System namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_STRING"))   n += RtSystemStringRegister();
    if (RtWaveEnabled("BANISHED_RT_FILENAME")) n += RtSystemFilenameRegister();
    if (RtWaveEnabled("BANISHED_RT_VALUE"))    n += RtSystemValueRegister();
    if (RtWaveEnabled("BANISHED_RT_THREAD"))   n += RtSystemThreadRegister();
    // Opt-in, not a kill switch: absent env means OFF.
    { const char* e = std::getenv("BANISHED_RT_ALLOC");
      if (e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0)
          n += RtSystemAllocatorRegister(); }
    printf("[RtPort] System: %d exports registered\n", n);
}
