// ============================================================================
// runtime/src/io/file_stream_cutover_probe.cpp — Wave 19 lane FILE
// THE SCREENSHOT CUTOVER SCREEN: is every door on ?SaveImageToFile@'s path
// answered by OUR registry, or does one of them still reach the DLL?
// ============================================================================
//
// Armed with BANISHED_RT_FILE_CUTOVER=1. Runs at `atexit`, so the whole port
// registry is populated whatever order the namespaces registered in.
//
// IT IS ARMED FROM RtIoPortInit(), *OUTSIDE* THIS LANE'S TWO KILL SWITCHES, and
// that placement was a correction. It was originally armed from inside
// `RtIoFileStreamRegister()`, so the obvious control — re-run with
// `BANISHED_RT_FILESTREAM=0 BANISHED_RT_CREATEFILE=0` and watch the board go
// red and attendance go to zero — printed NOTHING AT ALL, because the switch
// removed the reporter along with its subject. A control that deletes the
// instrument it is controlling measures nothing and reads as a clean run.
// Found by running that control, not by inspection.
//
// ---------------------------------------------------------------------------
// WHY THIS SCREEN EXISTS AND WHAT IT IS *NOT*
// ---------------------------------------------------------------------------
// Lane FILE's acceptance criterion is "re-run Wave 18 lane VRES's BMP oracle
// WITH THE DLL ABSENT". That oracle is a COMPARISON — ours writes one BMP, the
// shipped body writes another, and the two files are byte-compared. With the
// DLL absent there is no shipped body, so the comparison has no second side.
// The brief's instruction cannot be executed literally, and saying so is part
// of the deliverable rather than a reason to skip the work.
//
// So the claim is split into the two halves the single test was standing in for,
// and BOTH are run:
//
//   A. IS OUR BODY RIGHT?  VRES's oracle, DLL PRESENT, unchanged, with our
//      ports live on every door it used to reach the DLL through. Cases 9-11
//      byte-compare the two files. That half already exists and is not
//      reimplemented here.
//
//   B. DOES OUR BODY STILL NEED THE DLL?  This file. For every door on the
//      screenshot path it asks the PORT REGISTRY directly — RtPortLookupExport
//      / RtPortLookupAddr, the residue-free queries — and prints a verdict per
//      door. A door with no registry answer is one that resolves to the shipped
//      body today and to NULL at cutover.
//
// A alone is what Wave 18 already had, and it passed while the hole was open.
// B alone would pass on a body that was registered and wrong. Neither is the
// asked-for test; together they are what that test was for.
//
// ---------------------------------------------------------------------------
// THIS SCREEN'S SUBJECT, STATED — because six Wave 18 instruments could not
// state theirs (`reference_a_screen_that_cannot_name_its_subject.md`)
// ---------------------------------------------------------------------------
// Its subject is the LIST BELOW: the doors reachable from
// `?SaveImageToFile@ImageBuffer@Video@@`, enumerated BY HAND from
// runtime/src/video/save_image_to_file.cpp's RT_EXPORT_ONCE sites and from the
// disassembly of each callee, transitively, down to Win32.
//
// IT IS A HAND-BUILT LIST AND THAT IS ITS LIMIT. It cannot discover a door
// nobody wrote down. It says nothing about doors reached from anywhere else in
// the process, and nothing about whether the bodies behind those doors are
// CORRECT — that is half A's job. A green board here means "every door I was
// told about is answered by our registry", never "the screenshot works at
// cutover".
//
// It prints `MEASURED NOTHING` rather than a green line when the list is empty
// or when both controls fail, because from outside "no problem found" and "I
// looked at nothing" are indistinguishable and this project has repeatedly
// shipped the second believing the first (WAVE19_IO.md §6.6).
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"

void* RtPortLookupExport(const char* mangled_name);
void* RtPortLookupAddr(uintptr_t ghidra_addr);

// The two lane-FILE attendance blocks. Printed here rather than by their own
// files so a reader cannot see the DOOR verdicts without the CALL counts: a
// board of shut doors over bodies nothing ever entered is the exact shape of a
// green screen that proves nothing.
namespace RtIO {
void RtIoFileStreamCounters(unsigned long long* out);            // 7 slots
void RtIoFileSystemCreateFileCounters(unsigned long long* out);  // 10 slots
void RtIoFileStreamSlotCounters(unsigned long long* out);        // 14 slots
void RtIoBfsPromoteCounters(unsigned long long* out);            // 6 slots
}

namespace {

struct Door {
    const char* name;   // nullptr for an unexported address-only door
    uintptr_t   addr;   // 0 when only the name door exists
    const char* note;
};

// ---------------------------------------------------------------------------
// THE SUBJECT. Every door on the screenshot path, in call order.
//
// Rows marked (18) are the six the Wave 19 integrator's correction C2 measured
// out of save_image_to_file.cpp. Rows marked (19) are the closure this lane
// found underneath them by walking the disassembly — they are NOT on the Wave
// 19 row list and would have been discovered by this screen if not by the walk.
// Rows marked (pre) were already ported before this lane and are listed anyway:
// a door that is someone else's port is still a door, and leaving it out would
// make the denominator flattering.
// ---------------------------------------------------------------------------
const Door kDoors[] = {
    // ---- (18) the six save_image_to_file.cpp reaches directly --------------
    { "?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z", 0x1800ae430, "(18) lane FILE" },
    { "?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z",     0x1800aa8f0, "(18) lane FILE" },
    { "??0BinaryFileStream@IO@@QEAA@XZ",                          0x1800a8200, "(18) lane FILE" },
    { "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z",        0x1800accc0, "(18) lane FILE" },
    { "??1BinaryFileStream@IO@@UEAA@XZ",                          0x180023c80, "(18) lane FILE" },
    { "??1FileStream@IO@@UEAA@XZ",                                0x1800aaab0, "(18) lane FILE" },

    // ---- (19) the closure between those six and Win32 ----------------------
    { "?CreateFileW@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z",
                                                                  0x1800acc50, "(19) closure" },
    { "?PlatformCreateFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z",
                                                                  0x1800ade60, "(19) closure" },
    { "?PlatformCloseFile@FileSystem@IO@@AEAAXAEAUFileInfo@12@@Z", 0x1800ae020, "(19) closure" },
    { nullptr,                                                    0x18000c180, "(19) ~FileInfo, unexported" },

    // ---- (pre) already ported by earlier lanes -----------------------------
    { "?SubString@String@System@@QEBA?AV12@HH@Z",                  0x1800c9230, "(pre) lane String" },
    { "?AdjustDelimiters@Filename@System@@AEAAXXZ",                0x1800c6970, "(pre) lane String" },
    { "??4Filename@System@@QEAAAEAV01@AEBV01@@Z",                  0x180006770, "(pre) lane String" },
    { "??0WideString@System@@QEAA@AEBVString@1@@Z",                0x1800cab50, "(pre) lane WideStr" },
    { "?Set@WideString@System@@QEAAXPEB_W@Z",                      0x1800caf40, "(pre) lane WideStr" },
    { "?AsUTF8@WideString@System@@QEBA?AVString@2@XZ",             0x1800cacd0, "(pre) lane WideStr" },
    { nullptr,                                                    0x18000c200, "(pre) FileInfo::operator=" },
    { nullptr,                                                    0x180006a10, "(pre) guarded object free" },
    // CORRECTED after the first measured run, by the OTHER instrument.
    // This row originally named `?Allocate@MemoryAllocator@System@@QEAAPEAX_KH@Z`
    // (the member, 0x1800c6e60), which is what retail INLINES inside
    // ??0BinaryFileStream. Our port calls RtSystem::RtMemAlloc, which resolves
    // the STATIC locking pair `?MemAlloc@`/`?MemFree@` (0x1800c7a50/0x1800c7ad0)
    // — allocator.cpp:684 records that the static IS lockEnter+Allocate+leave,
    // so the transcription is faithful and the DOOR is a different symbol from
    // the one the disassembly shows.
    //
    // BANISHED_RT_STRICT's residue census named `?MemAlloc@` and not
    // `?Allocate@`, which is how this was found: two instruments looking at the
    // same path from opposite ends, one static and one live, and the live one
    // corrected the static one's subject. Recorded rather than quietly swapped.
    { "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z",             0x1800c7a50, "(pre) allocator" },
    { "?MemFree@MemoryAllocator@System@@SAXPEAX@Z",                0x1800c7ad0, "(pre) allocator" },

    // ---- (pre) the DATA doors — vtables, not code -------------------------
    // Listed with the code doors on purpose: at cutover an unminted vtable is
    // exactly as fatal as an unported function, and separating them is how a
    // board reads green while the object gets a null vptr.
    { "?_singleton@FileSystem@IO@@0PEAV12@EA",                     0x1801fc6a8, "(pre) DATA singleton" },
    { "??_7Stream@IO@@6B@",                                        0x1801549a0, "(pre) DATA vtable" },
    { "??_7FileStream@IO@@6B@",                                    0x180154960, "(19) DATA vtable, lane MINT" },
    { "??_7BinaryFileStream@IO@@6B@",                              0x1801554d0, "(19) DATA vtable, lane MINT" },
    { nullptr,                                                    0x1801554a8, "(pre) DATA encoding vtable" },
};
const int kDoorCount = static_cast<int>(sizeof(kDoors) / sizeof(kDoors[0]));

// ---------------------------------------------------------------------------
// THE TWO CONTROLS, each scored on its OWN applicable case, never on a union
// (WAVE19_IO.md §6.3 — a union denominator overstates a blind control, and
// Wave 18 shipped one).
//
// POSITIVE control, MUST read NOT-PORTED. `?Commit@CompressedStream@IO@@` is
// WAVE19_IO.md §2's worked example of a door that LOOKS ported —
// compressedstream.cpp names it eight times — and is not. If this screen calls
// it PORTED, the screen is answering from something other than the registry and
// every zero below is worthless.
//
// SUBJECT control, MUST read PORTED. `?OpenFile@FileSystem@IO@@` has been
// registered by name AND address since Wave 9 lane PKG2 rung 3. If this reads
// NOT-PORTED the lookup is broken in the other direction and every "not ported"
// below is a false alarm.
//
// A screen with only the first control cannot tell "nothing is ported" from
// "the lookup always says no"; with only the second it cannot tell "everything
// is ported" from "the lookup always says yes". Both, or no verdict.
// ---------------------------------------------------------------------------
const char* const kCtlMustBeUnported = "?Commit@CompressedStream@IO@@QEAAXXZ";
const char* const kCtlMustBePorted   =
    "?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z";

bool g_armed = false;

void Run() {
    printf("[FileCutover] ==== screenshot-path door screen ====\n");
    printf("[FileCutover] subject: %d hand-enumerated doors on the path from "
           "?SaveImageToFile@ImageBuffer@Video@@ down to Win32. It cannot see a "
           "door nobody wrote down, and it says NOTHING about correctness.\n",
           kDoorCount);

    // ---- controls first, and their verdict gates everything below ---------
    const bool ctlNegOk = (RtPortLookupExport(kCtlMustBeUnported) == nullptr);
    const bool ctlPosOk = (RtPortLookupExport(kCtlMustBePorted)   != nullptr);
    printf("[FileCutover] CONTROL positive (must read PORTED)     %-64s : %s\n",
           kCtlMustBePorted, ctlPosOk ? "PORTED -> GREEN" : "not ported -> RED");
    printf("[FileCutover] CONTROL negative (must read NOT-PORTED) %-64s : %s\n",
           kCtlMustBeUnported, ctlNegOk ? "not ported -> GREEN" : "PORTED -> RED");

    if (kDoorCount == 0 || !ctlPosOk || !ctlNegOk) {
        printf("[FileCutover] *** MEASURED NOTHING: %s. No verdict is offered "
               "below, and an absence of red lines here is not evidence. ***\n",
               kDoorCount == 0 ? "the door list is empty"
                               : "a control failed, so the lookup is not "
                                 "distinguishing ported from unported");
        return;
    }

    int nameDoors = 0, nameOpen = 0;
    int addrDoors = 0, addrOpen = 0;
    int openRows  = 0;

    for (int i = 0; i < kDoorCount; ++i) {
        const Door& d = kDoors[i];
        bool byName = false, byAddr = false;
        if (d.name) { ++nameDoors; byName = (RtPortLookupExport(d.name) != nullptr);
                      if (!byName) ++nameOpen; }
        if (d.addr) { ++addrDoors; byAddr = (RtPortLookupAddr(d.addr) != nullptr);
                      if (!byAddr) ++addrOpen; }

        const bool rowOpen = (d.name && !byName) || (d.addr && !byAddr);
        if (rowOpen) ++openRows;

        printf("[FileCutover] %-5s name=%-4s addr=%-4s 0x%09llx %-30s %s\n",
               rowOpen ? "OPEN" : "shut",
               d.name ? (byName ? "yes" : "NO") : "-",
               d.addr ? (byAddr ? "yes" : "NO") : "-",
               (unsigned long long)d.addr, d.note,
               d.name ? d.name : "(unexported)");
    }

    printf("[FileCutover] name doors %d of %d answered by our registry, %d OPEN\n",
           nameDoors - nameOpen, nameDoors, nameOpen);
    printf("[FileCutover] addr doors %d of %d answered by our registry, %d OPEN\n",
           addrDoors - addrOpen, addrDoors, addrOpen);
    printf("[FileCutover] ==== %d of %d rows SHUT, %d OPEN ====\n",
           kDoorCount - openRows, kDoorCount, openRows);

    // ---- attendance, printed WHATEVER the door board said -----------------
    // The board above is a STATIC registry query. It cannot tell a body that
    // ran from one that is merely registered, and `calls=0` has four causes
    // that invert the conclusion (WAVE19_IO.md §6.5). These counts are the
    // other half and they print unconditionally.
    unsigned long long fs[7] = {0}, fc[10] = {0};
    RtIO::RtIoFileStreamCounters(fs);
    RtIO::RtIoFileSystemCreateFileCounters(fc);
    printf("[FileCutover] attendance  ??0BinaryFileStream=%llu  ??1BinaryFileStream=%llu  "
           "??1FileStream=%llu  ??0TextFileStream=%llu  ?Create@FileStream=%llu (true=%llu)  "
           "~FileInfo=%llu\n", fs[0], fs[1], fs[2], fs[3], fs[4], fs[5], fs[6]);
    printf("[FileCutover] attendance  ?CreatePath=%llu (dirs=%llu)  ?CreateFileW=%llu  "
           "?PlatformCreateFile=%llu (ok=%llu fail=%llu)  ?CloseFile=%llu  "
           "?PlatformCloseFile=%llu (renamed=%llu)  ??1FileSystem=%llu\n",
           fc[0], fc[1], fc[2], fc[3], fc[4], fc[5], fc[6], fc[7], fc[8], fc[9]);

    // The nine vtable-slot bodies of the post-report tranche. These are the two
    // minted DATA rows' traps; unlike the rows above they are DISPATCHED
    // through a vtable rather than called through an export door, so their
    // attendance says something the door board structurally cannot: whether a
    // table WE stamped was actually dispatched.
    unsigned long long sl[14] = {0};
    RtIO::RtIoFileStreamSlotCounters(sl);
    printf("[FileCutover] slots       GetSource=%llu (nonNull=%llu)  Write=%llu (full=%llu)  "
           "Read=%llu  SetCurrentPos=%llu (win32=%llu)  GetCurrentPos=%llu  GetSize=%llu  "
           "Open=%llu (true=%llu)  deletingDtor=%llu  arrayArmAbort=%llu  copyCtor=%llu\n",
           sl[0], sl[1], sl[2], sl[3], sl[4], sl[5], sl[6], sl[7], sl[8],
           sl[9], sl[10], sl[11], sl[12], sl[13]);

    // ---- WAVE 20 LANE PROMOTE: the ??_7BinaryFileStream@ promotion ---------
    // Two claims, printed on two lines, deliberately not merged.
    //
    //  EFFECT      what our two stampers actually installed. `stamps` is the
    //              denominator and it MUST read non-zero or the A/B measured
    //              nothing: with no BinaryFileStream constructed, both halves
    //              of the kill-switch comparison are vacuously equal.
    //  ATTENDANCE  what dispatched through it. `viaMintBFS` is the attendance
    //              number for the PROMOTED row; `viaDllBFS` and `other` are the
    //              two ways an entry to these shared bodies can arrive without
    //              being attendance for it, and they are printed rather than
    //              folded away so the tiers stay separable.
    unsigned long long bp[6] = {0};
    RtIO::RtIoBfsPromoteCounters(bp);
    printf("[BfsPromote] EFFECT      stamps=%llu  stampedMinted=%llu  "
           "(promoted <=> the two are EQUAL and non-zero; kill switch <=> "
           "stampedMinted=0 with stamps UNCHANGED)\n", bp[0], bp[1]);
    printf("[BfsPromote] ATTENDANCE  slotEntries=%llu  viaMintBFS=%llu  "
           "viaDllBFS=%llu  other=%llu\n", bp[2], bp[3], bp[4], bp[5]);
    if (bp[0] == 0)
        printf("[BfsPromote] *** I MEASURED NOTHING: no BinaryFileStream was "
               "constructed on this route, so neither the EFFECT nor the "
               "ATTENDANCE line above is evidence of anything. ***\n");
    else if (bp[2] == 0)
        printf("[BfsPromote] attendance is ZERO with stamps=%llu: the table was "
               "installed and never dispatched. That is a finding, not a dead "
               "instrument -- the EFFECT line above is its liveness control.\n",
               bp[0]);

    unsigned long long entered = 0;
    for (int i = 0; i < 7; ++i)  entered += fs[i];
    for (int i = 0; i < 10; ++i) entered += fc[i];
    for (int i = 0; i < 14; ++i) entered += sl[i];
    if (entered == 0)
        printf("[FileCutover] *** NO BODY IN THIS LANE WAS ENTERED IN THIS RUN. "
               "The board above is a static registry query and says nothing about "
               "behaviour; read it as 'registered', never as 'working'. ***\n");

    if (openRows == 0)
        printf("[FileCutover] every door IN THIS LIST is answered by our port "
               "registry. That is not the same statement as 'the screenshot "
               "works with the DLL absent' -- see this file's header.\n");
}

}  // namespace

void RtIoFileCutoverProbeArm() {
    const char* e = std::getenv("BANISHED_RT_FILE_CUTOVER");
    if (!e || !*e || std::strcmp(e, "0") == 0) return;
    if (g_armed) return;
    g_armed = true;
    std::atexit(&Run);
}
