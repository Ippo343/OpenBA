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
// System::Configuration::SetCommandLine plus the unexported string-keyed BST
// find its map and IO::FileSystem both walk. On the process's single boot path
// (Engine::Startup step 2).
extern int RtSystemConfigurationRegister();  // configuration.cpp
// System::Error::SetText + String::StrAppend -- the diagnostic every remaining
// Engine::Startup failure needs (game_main.cpp reads _errorString to say WHICH
// step bombed). See runtime/src/system/error.cpp §2 for the storage note.
extern int RtSystemErrorRegister();          // error.cpp
// System::WideString (4 rows) plus the three System leaves of
// IO::FileSystem::SetupDirectories; the IO half lives in rt_io_port.cpp.
extern int RtSystemWideStringRegister();       // widestring.cpp
// System::Compression::Decompress -- the BOTTOM of the ?ReadPackfile@ stack
// (docs/runtime_port/WAVE9_READPACKFILE_PLAN.md). It lives in System rather
// than IO because that is its namespace, but it is PackageFile work: it is the
// function that decodes every byte of every package the game loads.
extern int RtSystemCompressionRegister();      // compression.cpp
// Cutover closure (2026-09-12): System::CRC::Compute, the leaf the persistence
// jobs reach by address.
extern int RtSystemCrcRegister();              // crc.cpp

void RtSystemPortInit() {
    // B3R RT-Core: 0x180009c30 (the allocator-lock Free wrapper) is no longer
    // registered -- every reader calls rtalloc::FreeLockedIgnoringSelf by name --
    // and the Thread and SetupDir registrars went with their rows.

    // ENDGAME B.3.9: Timer's five (close.txt, together, B34B 4.1), Error's three
    // and Configuration's singleton are RETIRED mint rows; every reader uses the
    // class statics in Timer.h / Error.h / Configuration.h.

    int n = 0;
    n += RtSystemStringRegister();
    n += RtSystemFilenameRegister();
    n += RtSystemValueRegister();
    n += RtSystemConfigurationRegister();
    n += RtSystemErrorRegister();
    n += RtSystemWideStringRegister();
    n += RtSystemCompressionRegister();
    n += RtSystemCrcRegister();
    n += RtSystemAllocatorRegister();
    printf("[RtPort] System: %d exports registered\n", n);
}
