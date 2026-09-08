// ============================================================================
// runtime/src/io/rt_io_port.cpp — RtIoPortInit()  (Wave 4)
// ============================================================================
// One entry point for the IO namespace: the Serializer forwarder family and
// its Encoding, the CompressedMemoryStream / MemoryStream / Stream family,
// registry-backed Persistence, and FileSystem / Modification lookups.
//
// Same shape as RtObjectPortInit(): each lane owns exactly ONE source file
// (plus its own types header and wave doc) and exposes exactly ONE Register
// function, so no two lanes ever edit the same file. This file and
// runtime/funcs/rt_port.h are the only shared surface and they are already
// written.
//
// Switches, outermost first:
//   BANISHED_RT_IO=0            leave the whole IO namespace on the DLL
//   BANISHED_RT_SERIALIZER=0    ... just Serializer + Encoding    (lane A)
//   BANISHED_RT_STREAM=0        ... just the Stream family        (lane B)
//   BANISHED_RT_PERSISTENCE=0   ... just Persistence + Registry   (lane C)
//   BANISHED_RT_FILESYSTEM=0    ... just FileSystem + Modification(lane D)
//   BANISHED_RT_FSSTARTUP=0     ... just FileSystem::Startup       (lane ENG)
//   BANISHED_RT_PERSSTARTUP=0   ... just Persistence::Startup      (lane ENG)
//   BANISHED_RT_REGREAD=0       ... just ReadSettings + Registry() (lane BOOT2)
//   BANISHED_RT_FSDIRS=0        ... just SetupDirectories + friends (lane FS)
//   BANISHED_RT_PKG=0           ... just LoadPackages + friends     (lane PKG)
//
// THREE WARNINGS THIS WAVE INHERITS, each already paid for once:
//
//  1. ENCODING VTABLE OFFSETS DIFFER BETWEEN RETAIL AND THE PROFILE BUILD.
//     bool +0x78 / uint +0x98 / int +0xa0 in retail; +0x90 / +0xb0 / +0xb8 in
//     the profile DLL. PLAN §6 step 1 ("locate it in the profile build first")
//     is about NAMES only. A body transcribed from a profile decompile writes
//     the right values through the WRONG slots: it does not crash, it desyncs
//     the stream, and the damage surfaces later on an innocent record.
//     (reference_encoding_vtable_offsets_differ_between_builds.md)
//
//  2. A ResolveExport name the DLL does not contain returns NULL silently and
//     the address fallback hides it, so the port registers, the census counts
//     it and the diff oracle says calls=0. Check every mangled name against
//     the shipped DLL's export directory BEFORE writing the body
//     (tools/export_name_audit.py).
//
//  3. Read the diff oracle's calls= column, not just divergences=. A port with
//     zero callers reports clean.
//
// PLATFORM SEAM (PLAN §4.1 decision 1): Persistence is registry-backed and
// FileSystem touches Win32 directly. Windows-only code goes in a *_win.cpp
// beside the portable half; <windows.h> must never appear in a non-_win file
// under runtime/src/.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtIoSerializerRegister();    // serializer.cpp    (lane A)
extern int RtIoCompressedStreamRegister();  // compressedstream.cpp (lane B)
extern int RtIoPersistenceRegister();   // persistence.cpp   (lane C)
extern int RtIoFileSystemRegister();    // filesystem.cpp    (lane D)
// Wave 9 lane ENG, iteration 2. Its own switch, not folded into
// BANISHED_RT_FILESYSTEM: this one is on the process's single boot path
// (Engine::Startup step 3), so the integrator must be able to A/B it without
// giving up lane D's five read-only accessors.
extern int RtIoFileSystemStartupRegister();  // filesystem_startup.cpp (lane ENG)
// Wave 9 lane ENG, iteration 3. Engine::Startup step 4. Own switch, own file,
// same reasoning as the row above.
extern int RtIoPersistenceStartupRegister(); // persistence_startup.cpp (lane ENG)
// Wave 9 lane BOOT2. IO::Persistence::ReadSettings + IO::Registry's ctor — the
// registry READ half of Engine::Startup step 4, i.e. the first thing lane ENG's
// Persistence::Startup calls. Own switch for the same reason as the two rows
// above: it is on the process's single boot path, and it is also the first port
// in this tree that reads the player's real Windows registry, so the integrator
// must be able to take it out on its own. WriteSettings is NOT in it.
extern int RtIoPersistenceRegistryRegister(); // persistence_registry_win.cpp (lane BOOT2)
// Wave 9 lane FS: FileSystem::SetupDirectories + GetRootDirectory, the row
// immediately behind FileSystem::Startup. BANISHED_RT_FSDIRS also gates the
// lane's System half (WideString and the three System leaves) in
// rt_system_port.cpp — one variable removes the whole lane.
extern int RtIoSetupDirectoriesRegister();   // filesystem_setupdirs_win.cpp (lane FS)
// Wave 9 lane PKG: FileSystem::LoadPackages + ClosePackages + GetPackDirectory
// + PackageFile::GetFileName — the row immediately behind lane FS's
// SetupDirectories, and the LAST call in FileSystem::Startup. Own switch, own
// file, same reasoning as the rows above: it is on the process's single boot
// path and the integrator must be able to A/B it alone. It is also the first
// port in this tree that MOUNTS THE GAME'S PACKAGE FILES, so taking it out is
// the fastest way to tell a packaging fault from a FileSystem fault.
extern int RtIoFileSystemPackagesRegister(); // filesystem_packages.cpp (lane PKG)
// Wave 9 lane FIND: FileSystem::FindFiles — the row lane PKG's LoadPackages
// exposed — plus the TWO NULL DOORS under the already-ported FileSystem::Exists
// (ExistsInPackfile and PlatformExists). Own switch for the reason above and
// one more: the two Exists arms are NOT on the LoadPackages path, they are on
// EVERY path once FileSystem::Startup sets the packfile gate, so an A/B that
// removed them together with lane PKG would confound two very different
// blast radii.
extern int RtIoFileSystemFindRegister();     // filesystem_find_win.cpp (lane FIND)
// Wave 9 lane PKG2: IO::PackageFile — the constructor lane PKG's LoadPackages
// dies on (attempt 14), plus the class's twenty leaf accessors. Own switch for
// the same reason as the rows above, and one more that is specific to this
// file: the constructor is the ONLY row on the boot path, and the other twenty
// are exercised only by a probe. If the boot regresses, BANISHED_RT_PKG2=0
// removes both halves; if the probe is the problem, it is already gated on the
// diff harness and a normal run does not run it.
extern int RtIoPackageFileRegister();        // package_file.cpp (lane PKG2)
// Wave 9 lane PKG2 rung 2: the IO::CompressedStream READ path (Read, SetWindow,
// GetAvailableWindow, DecompressFromSource) — the stack ?ReadPackfile@ stands
// on, and the thing that gives rung 1's Compression::Decompress its first real
// caller. Own switch: it is on the package-mount path AND on the save/load path
// (a CompressedMemoryStream is how saves are written), so the integrator must be
// able to take exactly it out without disturbing Wave 4's stream ports.
extern int RtIoCompressedStreamReadRegister(); // compressed_stream_read.cpp (lane PKG2)
// Wave 9 lane PKG2 rung 3: FileSystem::OpenFile / ReadFile / PlatformOpenFile
// + PackageFile::GetFileInfo. Own switch: OpenFile is how EVERY resource in the
// game is reached once packages are mounted, so it has the widest blast radius
// of anything this lane has landed and the integrator must be able to remove
// exactly it.
extern int RtIoFileSystemOpenFileRegister();  // filesystem_openfile_win.cpp (lane PKG2)
// Wave 9 lane PKG2 rung 4b-ii: IO::MemoryStream's six virtual bodies. Its own
// switch because it sits UNDER CompressedStream::Open -- every stream read in
// the packfile header path goes through it, so the integrator must be able to
// remove exactly it if a boot regresses.
extern int RtIoMemoryStreamRegister();        // memory_stream.cpp (lane PKG2)
// Wave 9 lane PKG2 rung 4b-iii: CompressedStream::Release and ~CompressedStream.
// LANE W'S ROWS. ~CompressedStream is a LIVE blocker today, not just at
// cutover: lane W's ported ~CompressedMemoryStream tail-jumps into it.
extern int RtIoCompressedStreamReleaseRegister();  // compressed_stream_release.cpp
// Wave 9 lane PKG2 rung 4b-iii: the last two bodies between ReadPackfile and a
// mounted package. Own switch -- Open is what decides whether a packfile header
// is well-formed, so a boot regression must be attributable to exactly it.
extern int RtIoCompressedStreamOpenRegister();     // compressed_stream_open.cpp
// Wave 9 lane PKG3: ?ReadPackfile@PackageFile@IO@@ itself -- the .pkg parser,
// and the last body between LoadPackages and a mounted package. Its own switch
// because it is all-or-nothing (a 660-instruction transcription that cannot be
// half-registered), so if a boot regresses the integrator must be able to take
// exactly it out and leave lane PKG2's five rungs of callees standing.
extern int RtIoPackageFileReadRegister();          // package_file_read_win.cpp
namespace RtIO { void RtIo_CompressedStreamReadProbe(); }

// Wave 9 lane MOD: the IO::Modification mods-menu cluster (GetCount,
// GetModification, Enable, Swap, SaveList) plus the unexported Mod-entry
// lifecycle triple and the container internals it reaches. Its own switch
// because the whole cluster was found by the ADDRESS-door census rather than by
// a name-based instrument, so an A/B on it answers a question no other lane's
// kill switch can: whether a by-address door that resolves through the port
// registry behaves the same as one that falls through to the DLL.
namespace RtIO { int RtIoModificationRegister(); }   // modification.cpp (lane MOD)

void RtIoPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_IO")) {
        printf("[RtPort] BANISHED_RT_IO=0 -- IO namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_SERIALIZER"))  n += RtIoSerializerRegister();
    if (RtWaveEnabled("BANISHED_RT_STREAM"))      n += RtIoCompressedStreamRegister();
    if (RtWaveEnabled("BANISHED_RT_PERSISTENCE")) n += RtIoPersistenceRegister();
    if (RtWaveEnabled("BANISHED_RT_FILESYSTEM"))  n += RtIoFileSystemRegister();
    if (RtWaveEnabled("BANISHED_RT_FSSTARTUP"))   n += RtIoFileSystemStartupRegister();
    if (RtWaveEnabled("BANISHED_RT_PERSSTARTUP")) n += RtIoPersistenceStartupRegister();
    if (RtWaveEnabled("BANISHED_RT_REGREAD"))     n += RtIoPersistenceRegistryRegister();
    if (RtWaveEnabled("BANISHED_RT_FSDIRS"))      n += RtIoSetupDirectoriesRegister();
    if (RtWaveEnabled("BANISHED_RT_PKG"))         n += RtIoFileSystemPackagesRegister();
    if (RtWaveEnabled("BANISHED_RT_FIND"))        n += RtIoFileSystemFindRegister();
    if (RtWaveEnabled("BANISHED_RT_PKG2"))        n += RtIoPackageFileRegister();
    if (RtWaveEnabled("BANISHED_RT_CSREAD"))      n += RtIoCompressedStreamReadRegister();
    if (RtWaveEnabled("BANISHED_RT_OPENFILE"))    n += RtIoFileSystemOpenFileRegister();
    if (RtWaveEnabled("BANISHED_RT_MEMSTREAM"))   n += RtIoMemoryStreamRegister();
    if (RtWaveEnabled("BANISHED_RT_CSRELEASE"))   n += RtIoCompressedStreamReleaseRegister();
    if (RtWaveEnabled("BANISHED_RT_CSOPEN"))      n += RtIoCompressedStreamOpenRegister();
    if (RtWaveEnabled("BANISHED_RT_PKG3"))        n += RtIoPackageFileReadRegister();
    if (RtWaveEnabled("BANISHED_RT_MOD"))         n += RtIO::RtIoModificationRegister();
    printf("[RtPort] IO: %d exports registered\n", n);
}
