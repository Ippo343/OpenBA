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
extern int RtSystemThreadRegister();      // thread_win.cpp   (CriticalSection/Thread)
extern int RtSystemTimerRegister();       // timer.cpp        (also owns the five Timer data exports)
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
extern int RtSystemSetupDirHelpersRegister();  // setupdir_helpers.cpp
// System::Compression::Decompress -- the BOTTOM of the ?ReadPackfile@ stack
// (docs/runtime_port/WAVE9_READPACKFILE_PLAN.md). It lives in System rather
// than IO because that is its namespace, but it is PackageFile work: it is the
// function that decodes every byte of every package the game loads.
extern int RtSystemCompressionRegister();      // compression.cpp
// Cutover closure (2026-09-12): System::CRC::Compute, the leaf the persistence
// jobs reach by address.
extern int RtSystemCrcRegister();              // crc.cpp

// allocator.cpp -- the single owner of the 0x180009c30 registration; see the
// comment at the call site in RtSystemPortInit below.
extern "C" void RtSystemAllocatorRegisterSharedAddresses();

void RtSystemPortInit() {
    // 0x180009c30 -- the allocator-lock Free wrapper -- was registered THREE
    // times by three different lanes, each with its own body; the registry's
    // last binding wins, so two were dead code and which one survived depended
    // on init order. Consolidated to one owner in allocator.cpp.
    RtSystemAllocatorRegisterSharedAddresses();

    // ENDGAME B.3.9 SYS (B39_RULES D7): the minted storage rows ARE the class
    // statics from here on (name door, address door, manifest publish). Before
    // the mint, as the redirect requires. Timer's five go TOGETHER (B34B 4.1).
    RtMintRedirectStorage("?_ticksPerSecond@Timer@System@@0_JA",       &System::Timer::_ticksPerSecond);
    RtMintRedirectStorage("?_secondsPerTick@Timer@System@@0NA",        &System::Timer::_secondsPerTick);
    RtMintRedirectStorage("?_systemTicksPerSecond@Timer@System@@0_JA", &System::Timer::_systemTicksPerSecond);
    RtMintRedirectStorage("?_systemInitialTick@Timer@System@@0_JA",    &System::Timer::_systemInitialTick);
    RtMintRedirectStorage("?_systemSecondsPerTick@Timer@System@@0NA",  &System::Timer::_systemSecondsPerTick);
    // Error's three, together (B39_RULES 1.5). `_errorString` is the 0x2000-byte
    // BUFFER: the cell is the array's own address.

    int n = 0;
    n += RtSystemStringRegister();
    n += RtSystemFilenameRegister();
    n += RtSystemValueRegister();
    n += RtSystemThreadRegister();
    n += RtSystemTimerRegister();
    n += RtSystemConfigurationRegister();
    n += RtSystemErrorRegister();
    n += RtSystemWideStringRegister();
    n += RtSystemSetupDirHelpersRegister();
    n += RtSystemCompressionRegister();
    n += RtSystemCrcRegister();
    n += RtSystemAllocatorRegister();
    printf("[RtPort] System: %d exports registered\n", n);
}
