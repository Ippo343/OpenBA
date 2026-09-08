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
// Wave 9 lane FS: FileSystem::SetupDirectories + GetRootDirectory, the row
// immediately behind FileSystem::Startup. BANISHED_RT_FSDIRS also gates the
// lane's System half (WideString and the three System leaves) in
// rt_system_port.cpp — one variable removes the whole lane.
extern int RtIoSetupDirectoriesRegister();   // filesystem_setupdirs_win.cpp (lane FS)

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
    if (RtWaveEnabled("BANISHED_RT_FSDIRS"))      n += RtIoSetupDirectoriesRegister();
    printf("[RtPort] IO: %d exports registered\n", n);
}
