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
//   BANISHED_RT_TIMER=0    ... System::Timer (Wave 9 lane B3). Note that the
//                              port also OWNS the five Timer data exports
//                              (runtime/src/system/timer.h); turning the
//                              registrations off does NOT send those data
//                              reads back to the DLL, because our storage
//                              holds the same constants either way.
//   BANISHED_RT_FSDIRS=0   ... Wave 9 lane FS: System::WideString + the three
//                              System leaves of FileSystem::SetupDirectories.
//                              The SAME variable also gates the IO half in
//                              rt_io_port.cpp, so =0 removes the whole lane.
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
extern int RtSystemTimerRegister();       // timer.cpp  (Wave 9 lane B3)
extern int RtSystemAllocatorRegister();   // allocator.cpp
// Wave 9 lane ENG, iteration 2. System::Configuration::SetCommandLine plus the
// unexported string-keyed BST find its map and IO::FileSystem both walk. Its
// own switch because it is on the process's single boot path (Engine::Startup
// step 2), so it must be A/B-able on its own.
extern int RtSystemConfigurationRegister();  // configuration.cpp (lane ENG)
// Wave 9 lane ENG, iteration 3. System::Error::SetText + String::StrAppend --
// the diagnostic every remaining Engine::Startup failure needs (game_main.cpp
// reads _errorString to say WHICH step bombed).
//
// ⛔ ITS OWN SWITCH IS LOAD-BEARING, NOT COSMETIC. rt_data_mint_tables.inc:1538
// mints ?_errorString@Error@System@@ as EIGHT BYTES when the buffer is 0x2000,
// so at cutover the first SetText call overflows kStorageBytes. Hold this with
// BANISHED_RT_ERROR=0 until that row is widened. See runtime/src/system/error.cpp §2.
extern int RtSystemErrorRegister();          // error.cpp (lane ENG)
// Wave 9 lane FS. System::WideString (4 rows) plus the three System leaves of
// IO::FileSystem::SetupDirectories. ONE switch for both, shared with the IO
// half in rt_io_port.cpp, so the integrator can A/B the whole wall in a single
// run rather than bisecting five files.
extern int RtSystemWideStringRegister();       // widestring.cpp     (lane FS)
extern int RtSystemSetupDirHelpersRegister();  // setupdir_helpers.cpp (lane FS)
// Wave 9 lane PKG2 iteration 2: System::Compression::Decompress — the BOTTOM of
// the ?ReadPackfile@ stack (docs/runtime_port/WAVE9_READPACKFILE_PLAN.md). It
// lives in System rather than IO because that is its namespace, but it is
// PackageFile work: it is the function that decodes every byte of every package
// the game loads. Its own switch, because a defect here is silent wrong data
// rather than a crash and the integrator must be able to remove exactly it.
extern int RtSystemCompressionRegister();      // compression.cpp    (lane PKG2)
namespace RtSystem { void RtSystem_CompressionProbe(); }

// allocator.cpp -- the single owner of the 0x180009c30 registration; see the
// comment at the call site in RtSystemPortInit below.
extern "C" void RtSystemAllocatorRegisterSharedAddresses();

void RtSystemPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_SYSTEM")) {
        printf("[RtPort] BANISHED_RT_SYSTEM=0 -- System namespace port disabled\n");
        return;
    }
    // UNCONDITIONAL, and deliberately outside every wave switch. 0x180009c30 --
    // the allocator-lock Free wrapper -- was registered THREE times by three
    // different lanes, each with its own body; the registry's last binding wins,
    // so two were dead code and which one survived depended on init order. Lane
    // VID2 found it. Consolidated to one owner in allocator.cpp; it is not
    // gated because the two registrations it replaces were not gated either, and
    // because its callers span the DLL-present path as well as cutover.
    RtSystemAllocatorRegisterSharedAddresses();

    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_STRING"))   n += RtSystemStringRegister();
    if (RtWaveEnabled("BANISHED_RT_FILENAME")) n += RtSystemFilenameRegister();
    if (RtWaveEnabled("BANISHED_RT_VALUE"))    n += RtSystemValueRegister();
    if (RtWaveEnabled("BANISHED_RT_THREAD"))   n += RtSystemThreadRegister();
    if (RtWaveEnabled("BANISHED_RT_TIMER"))    n += RtSystemTimerRegister();
    if (RtWaveEnabled("BANISHED_RT_CONFIG"))   n += RtSystemConfigurationRegister();
    if (RtWaveEnabled("BANISHED_RT_ERROR"))    n += RtSystemErrorRegister();
    if (RtWaveEnabled("BANISHED_RT_FSDIRS")) {
        n += RtSystemWideStringRegister();
        n += RtSystemSetupDirHelpersRegister();
    }
    if (RtWaveEnabled("BANISHED_RT_COMPRESSION")) n += RtSystemCompressionRegister();
    // NOTE: RtSystem_CompressionProbe() is NOT called here, and the first draft
    // of this lane calling it here is a defect the run found rather than
    // reasoning did. **RtSystemPortInit runs before System::MemoryAllocator has
    // published its singleton.** The probe builds a System::Compression through
    // the DLL's ctor, that ctor takes the allocator lock at
    // `[0x1801fc718] + 0x640`, and with the singleton still null that is a WRITE
    // to 0x648 — an ACCESS_VIOLATION whose top frame is inside ntdll and whose
    // only clue is `[REDZONE-DUMP] no allocator singleton`. Nothing in the crash
    // names the probe. The probe now runs from LoadPackages' tail
    // (runtime/src/io/filesystem_packages.cpp), which is a point the heap is
    // provably live because the surrounding body allocates.
    //
    // The RULE, for the next lane that adds one: a probe that ALLOCATES cannot
    // run from a *PortInit. Registration is fine there; execution is not.
    // Opt-in, not a kill switch: absent env means OFF.
    { const char* e = std::getenv("BANISHED_RT_ALLOC");
      if (e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0)
          n += RtSystemAllocatorRegister(); }
    printf("[RtPort] System: %d exports registered\n", n);
}
