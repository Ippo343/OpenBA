// ============================================================================
// runtime/src/system/rt_system_port.cpp — RtSystemPortInit()
// ============================================================================
// One entry point for the System namespace, mirroring RtMathPortInit(). Each
// source file exposes exactly ONE Register function; this file and rt_port.h
// are the only shared surface.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "../../types/system/Timer.h"
#include "../../types/system/Error.h"
#include "../../types/system/Configuration.h"

extern int RtSystemStringRegister();      // string.cpp
extern int RtSystemFilenameRegister();    // filename.cpp
extern int RtSystemValueRegister();       // value_types.cpp  (Random/Time/DateTime/Color)
extern int RtSystemAllocatorRegister();   // allocator.cpp    (System::MemoryAllocator)

void RtSystemPortInit() {
    // B3R RT-Core: 0x180009c30 (the allocator-lock Free wrapper) is no longer
    // registered -- every reader calls rtalloc::FreeLockedIgnoringSelf by name --
    // and the Thread, SetupDir, Timer, Configuration, Error, WideString,
    // Compression and CRC registrars went with their rows.

    // ENDGAME B.3.9: Timer's five (close.txt, together, B34B 4.1), Error's three
    // and Configuration's singleton are RETIRED mint rows; every reader uses the
    // class statics in Timer.h / Error.h / Configuration.h.

    int n = 0;
    n += RtSystemStringRegister();
    n += RtSystemFilenameRegister();
    n += RtSystemValueRegister();
    n += RtSystemAllocatorRegister();
    printf("[RtPort] System: %d exports registered\n", n);
}
