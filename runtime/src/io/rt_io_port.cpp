// =====================================================================// runtime/src/io/rt_io_port.cpp — RtIoPortInit()  (Wave 4)
// =====================================================================// One entry point for the IO namespace: the Serializer forwarder family and
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
//   BANISHED_RT_PERSAVES=0      ... just the Persistence save rows (lane PERSIST)
//   BANISHED_RT_FILESYSTEM=0    ... just FileSystem + Modification(lane D)
//   BANISHED_RT_FSSTARTUP=0     ... just FileSystem::Startup       (lane ENG)
//   BANISHED_RT_PERSSTARTUP=0   ... just Persistence::Startup      (lane ENG)
//   BANISHED_RT_REGREAD=0       ... just ReadSettings + Registry() (lane BOOT2)
//   BANISHED_RT_FSDIRS=0        ... just SetupDirectories + friends (lane FS)
//   BANISHED_RT_PKG=0           ... just LoadPackages + friends     (lane PKG)
//   BANISHED_RT_IOBASE=0        ... just the IOBASE slot-0 dtors,
//                                   GetSource and VisitorStream   (lane IOBASE)
//   BANISHED_RT_FILESTREAM=0    ... just the FileStream family      (lane FILE)
//   BANISHED_RT_CREATEFILE=0    ... just FileSystem create/close   (lane FILE)
//   BANISHED_RT_FSSLOTS=0       ... just the FileStream vtable slots (lane FILE)
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
// =====================================================================
#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtIoSerializerRegister();    // serializer.cpp    (lane A)
extern int RtIoCompressedStreamRegister();  // compressedstream.cpp (lane B)
extern int RtIoPersistenceRegister();   // persistence.cpp   (lane C)
extern int RtIoFileSystemRegister();    // filesystem.cpp    (lane D)
// Wave 19 lane PERSIST: the save-ENUMERATION and request-ISSUE half of
// IO::Persistence — GetSaveCount / GetSaveInfo / GetSaveFilename, the three
// Begin* request issuers, and ~Persistence. Own switch, separate from
// BANISHED_RT_PERSISTENCE, because that one gates the SETTINGS half (Get/Set/
// GetSavePath) which is on the boot path; these seven are on the save/load
// menu path and must be removable without taking the settings with them.
extern int RtIoPersistenceSavesRegister();  // persistence_saves{,_win}.cpp (lane PERSIST)
// Wave 24 lane INTERNAL8: the four UNEXPORTED bodies of the worker Wave 19
// left as address doors — the three enqueue helpers and the worker destructor.
// OWN switch, deliberately NOT folded into BANISHED_RT_PERSAVES: these four
// are reached by ADDRESS from Wave 19's already-ported Begin* bodies, so with
// this switch off those callers fall straight back through ResolveAddr into
// the shipped DLL and the A/B is exactly this lane's four bodies and nothing
// else. The dtor in particular joins a real thread, and "the shutdown hangs"
// has to be separable from "the save menu is wrong".
extern int RtIoPersistenceWorkerRegister(); // persistence_worker_win.cpp (Wave 24 INTERNAL8)
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
extern int RtIoPackageFileDtorRegister();    // package_file_dtor.cpp (W25 BODY)
extern int RtIoPackageFileSerializeRegister(); // package_file_serialize.cpp (W27 IOTEXT)
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

// Wave 9 lane ENC: the two IO::Encoding factories (slots 3 and 4 of the shared
// vtable 0x1801554a8, both UNEXPORTED) and the two BinaryReader/BinaryWriter
// constructors they call. Its own switch because it is the first lane to make
// the shared Encoding vtable partly FUNCTIONAL rather than merely minted, and
// an A/B on it isolates the save/load header path from every other IO row.
extern int RtIoEncodingRegister();          // encoding.cpp (lane ENC)

// Wave 9 lane ENC-2: the four BinaryWriter bodies SaveSettings dispatches into
// on the write path (two Serialize slots, the back-patching destructor, and the
// unexported deleting destructor at vtable slot 0). Separate switch from ENC so
// the framing tail can be A/B tested without turning the factories off.
extern int RtIoBinaryWriterRegister();      // binary_writer.cpp (lane ENC-2)

// Wave 9 lane SERIAL: the other 73 slots of the IO scalar-serialization
// dispatch family -- IO::BinaryReader (25), IO::BinaryWriter (23; the two
// ENC-2 owns are excluded) and IO::Visitor (25). Its own switch so the family
// can be turned off without turning off ENC-2's framing tail underneath it and
// vice versa: the two files register DISJOINT slots of the SAME vtables, so an
// A/B that could only move both together could not attribute anything.
extern int RtIoSerializeFamilyRegister();   // serialize_family.cpp (lane SERIAL)

// Wave 19 lane CSTREAM: the IO::CompressedStream WRITE path -- the twelve
// bodies compressed_stream_read.cpp explicitly listed as owed (?Commit,
// ?CompressWindow, ?Create, ?Write, ?SetCurrentPos, ?GetCurrentPos, ?GetSize,
// both ?PreCommit, ?Close on CompressedStream and CompressedFileStream).
//
// ?GetSource@Stream@IO@@ WAS in this file and is NOT any more: lane IOBASE
// landed the same body first, and two RegisterRuntimeExport calls on one name
// are a SHADOWED registration where the later binding silently wins. It is
// registered by iobase_vtables.cpp and consumed here.
//
// ITS OWN SWITCH, and the reason is sharper than "blast radius": this is the
// half of the class that PRODUCES bytes. Every save the game writes goes
// through ?Commit, so an A/B that could only move the read and write halves
// together could not attribute a corrupt save to either. BANISHED_RT_CSREAD
// stays independent of it for the same reason it is independent of
// BANISHED_RT_STREAM.
//
// NOTE it does NOT make the writer cutover-clean: ?Write reaches ?SetWindow,
// whose direction!=0 GROW branch is still DELEGATED to the DLL and counted by
// compressed_stream_read.cpp. That remains a named cutover blocker.
extern int RtIoCompressedStreamWriteRegister();    // compressed_stream_write.cpp

// Wave 19 lane IOBASE, tranche 1: the four IO vector deleting destructors that
// are slot 0 of their rows, the three out-of-line destructors they inline,
// ?GetSource@Stream@IO@@ (slot 1 of SIX minted rows), and the whole
// IO::VisitorStream null-sink family. Its own switch because it is the first
// lane to make four minted DATA rows PROMOTABLE at once, so a boot regression
// must be attributable to exactly it and not to the promotions that follow.
extern int RtIoBaseVtablesRegister();       // iobase_vtables.cpp (lane IOBASE)
// Wave 19 lane FILE: the IO::FileStream family lifecycle (both constructors,
// both destructors, Create) and the FileSystem create/close pair underneath it.
// TWO switches, not one, and deliberately so: the FileStream half is pure
// object lifecycle while the FileSystem half is the only code in this tree that
// creates a file in %TEMP% and RENAMES it over the target. If a write goes
// wrong the integrator needs to be able to remove exactly the platform half and
// leave the lifecycle standing, and vice versa.
//
// THIS LANE'S HEADLINE IS THE SCREENSHOT CUTOVER HOLE, NOT ITS ROW COUNT.
// Wave 18 lane VRES ported ?SaveImageToFile@ImageBuffer@Video@@ and left it
// reaching six unported IO exports through RT_EXPORT_ONCE doors, which resolve
// to the shipped DLL today and to NULL at cutover. These two files are those
// six plus the three-body closure between them and Win32.
extern int RtIoFileStreamRegister();            // file_stream.cpp               (lane FILE)
extern int RtIoFileSystemCreateFileRegister();  // filesystem_createfile_win.cpp (lane FILE)
// Wave 19 lane CSTREAM: the IO::CompressedStream WRITE path -- the twelve
// bodies compressed_stream_read.cpp explicitly listed as owed (?Commit,
// ?CompressWindow, ?Create, ?Write, ?SetCurrentPos, ?GetCurrentPos, ?GetSize,
// both ?PreCommit, ?Close on CompressedStream and CompressedFileStream) plus
// ?GetSource@Stream@IO@@, which is slot 1 of FIVE minted IO vtables.
//
// ITS OWN SWITCH, and the reason is sharper than "blast radius": this is the
// half of the class that PRODUCES bytes. Every save the game writes goes
// through ?Commit, so an A/B that could only move the read and write halves
// together could not attribute a corrupt save to either. BANISHED_RT_CSREAD
// stays independent of it for the same reason it is independent of
// BANISHED_RT_STREAM.
//
// NOTE it does NOT make the writer cutover-clean: ?Write reaches ?SetWindow,
// whose direction!=0 GROW branch is still DELEGATED to the DLL and counted by
// compressed_stream_read.cpp. That remains a named cutover blocker.
extern int RtIoCompressedStreamWriteRegister();    // compressed_stream_write.cpp

// Wave 19 lane FILE, POST-REPORT TRANCHE. The nine bodies behind the eight
// vtable slots of BOTH ??_7FileStream@ and ??_7BinaryFileStream@ (their slots
// 1..7 are byte-identical, so nine functions cover sixteen trap slots). Own
// switch because it is the only part of this lane that changes what a
// DISPATCHED vtable slot does, as opposed to what a called export does.
extern int RtIoFileStreamSlotsRegister();       // file_stream.cpp + file_stream_win.cpp

// Wave 20 lane SAVETEXT. Five of the eight unported slots of
// ??_7TextFileStream@IO@@6B@ (Read, SetCurrentPos, GetCurrentPos, GetSize,
// Create). Its OWN switch, separate from BANISHED_RT_FSSLOTS, because
// TextFileStream's slots and FileStream's slots are reachable by different
// callers -- text I/O is stripped from the shipped DLL (both TEXT serializer
// factories at 0x1801559d0+0x18/+0x20 are XOR EAX,EAX;RET), so these five have
// no gameplay route and a run that moves FileStream's slots must not be read as
// having moved these.
extern int RtIoTextFileStreamSlotsRegister();   // text_file_stream_win.cpp

void RtIoPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_IO")) {
        printf("[RtPort] BANISHED_RT_IO=0 -- IO namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_SERIALIZER"))  n += RtIoSerializerRegister();
    if (RtWaveEnabled("BANISHED_RT_STREAM"))      n += RtIoCompressedStreamRegister();
    if (RtWaveEnabled("BANISHED_RT_PERSISTENCE")) n += RtIoPersistenceRegister();
    if (RtWaveEnabled("BANISHED_RT_PERSAVES"))    n += RtIoPersistenceSavesRegister();
    if (RtWaveEnabled("BANISHED_RT_PERSWORKER")) n += RtIoPersistenceWorkerRegister();
    if (RtWaveEnabled("BANISHED_RT_FILESYSTEM"))  n += RtIoFileSystemRegister();
    if (RtWaveEnabled("BANISHED_RT_FSSTARTUP"))   n += RtIoFileSystemStartupRegister();
    if (RtWaveEnabled("BANISHED_RT_PERSSTARTUP")) n += RtIoPersistenceStartupRegister();
    if (RtWaveEnabled("BANISHED_RT_REGREAD"))     n += RtIoPersistenceRegistryRegister();
    if (RtWaveEnabled("BANISHED_RT_FSDIRS"))      n += RtIoSetupDirectoriesRegister();
    if (RtWaveEnabled("BANISHED_RT_PKG"))         n += RtIoFileSystemPackagesRegister();
    if (RtWaveEnabled("BANISHED_RT_FIND"))        n += RtIoFileSystemFindRegister();
    if (RtWaveEnabled("BANISHED_RT_PKG2"))        n += RtIoPackageFileRegister();
    // Wave 25 lane BODY. ~PackageFile on its OWN switch, NOT folded into PKG2:
    // it is the first body in this class that FREES, so a defect here is a
    // double free or a leak rather than a wrong value, and the integrator must
    // be able to A/B it while keeping the ctor and the twenty accessors.
    if (RtWaveEnabled("BANISHED_RT_PKGDTOR"))     n += RtIoPackageFileDtorRegister();
    // Wave 27 lane IOTEXT. Its OWN switch, not folded into PKG2, for the reason
    // above and one more: it is the only PackageFile body that decides a FILE
    // FORMAT, so a defect is a desynced .pkg rather than a wrong value, and it
    // opens one new unported address door (0x180029bb0). The integrator must be
    // able to take the table's ninth slot out without losing the other eight.
    if (RtWaveEnabled("BANISHED_RT_PKGSER"))      n += RtIoPackageFileSerializeRegister();
    if (RtWaveEnabled("BANISHED_RT_CSREAD"))      n += RtIoCompressedStreamReadRegister();
    if (RtWaveEnabled("BANISHED_RT_OPENFILE"))    n += RtIoFileSystemOpenFileRegister();
    if (RtWaveEnabled("BANISHED_RT_MEMSTREAM"))   n += RtIoMemoryStreamRegister();
    if (RtWaveEnabled("BANISHED_RT_CSRELEASE"))   n += RtIoCompressedStreamReleaseRegister();
    if (RtWaveEnabled("BANISHED_RT_CSOPEN"))      n += RtIoCompressedStreamOpenRegister();
    if (RtWaveEnabled("BANISHED_RT_PKG3"))        n += RtIoPackageFileReadRegister();
    if (RtWaveEnabled("BANISHED_RT_MOD"))         n += RtIO::RtIoModificationRegister();
    if (RtWaveEnabled("BANISHED_RT_ENCODING"))    n += RtIoEncodingRegister();
    if (RtWaveEnabled("BANISHED_RT_BINWRITER"))   n += RtIoBinaryWriterRegister();
    if (RtWaveEnabled("BANISHED_RT_SERIALFAM"))   n += RtIoSerializeFamilyRegister();
    if (RtWaveEnabled("BANISHED_RT_CSWRITE"))     n += RtIoCompressedStreamWriteRegister();

    if (RtWaveEnabled("BANISHED_RT_IOBASE"))      n += RtIoBaseVtablesRegister();
    if (RtWaveEnabled("BANISHED_RT_FILESTREAM"))  n += RtIoFileStreamRegister();
    if (RtWaveEnabled("BANISHED_RT_CREATEFILE"))  n += RtIoFileSystemCreateFileRegister();
    if (RtWaveEnabled("BANISHED_RT_FSSLOTS"))    n += RtIoFileStreamSlotsRegister();
    if (RtWaveEnabled("BANISHED_RT_TFSSLOTS"))   n += RtIoTextFileStreamSlotsRegister();

    // Lane FILE's screenshot-path door screen. Armed OUTSIDE its two switches
    // on purpose: the control for this lane is a run with both of them at 0,
    // and a screen that the control switches off cannot be that control.
    extern void RtIoFileCutoverProbeArm();
    RtIoFileCutoverProbeArm();

    // ---- BATCH 5 (2026-09-12) ---------------------------------------------
    // Registered UNCONDITIONALLY (DIRECTION.md §2 — a port registers
    // unconditionally, and no new BANISHED_RT_* flag is minted for one):
    //
    //   filesystem_timestamp_win.cpp    ?GetFileTimeStamp@FileSystem@  0x1800ae270
    //   persistence_savelist_job.cpp    worker command 4 (unexported)  0x1800b6290
    //   modification_setupfilesystem.cpp ?SetupFileSystem@Modification@ 0x1800aebb0
    //
    // 0x1800b6290 pays one of the five address doors
    // persistence_worker_win.cpp's `Run` opened and declared; the other four
    // (SAVE / LOAD / DELETE / GET-SAVE-INFO) are still outstanding.
    extern int RtIoFileSystemTimeStampRegister();
    extern int RtIoPersistenceSaveListRegister();
    extern int RtIoModificationSetupFileSystemRegister();
    n += RtIoFileSystemTimeStampRegister();
    n += RtIoPersistenceSaveListRegister();
    n += RtIoModificationSetupFileSystemRegister();
    if (RtWaveEnabled("BANISHED_RT_CSWRITE"))     n += RtIoCompressedStreamWriteRegister();
    printf("[RtPort] IO: %d exports registered\n", n);
}
