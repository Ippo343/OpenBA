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
// IT IS A HAND-BUILT LIST AND THAT IS ITS LIMIT — and the limit is not
// theoretical: WAVE 20 LANE DOORS found FOUR unconditional doors in
// save_image_to_file.cpp itself that this array had never listed (see the (20)
// rows). `tools/screenshot_door_walk.py` now re-derives the DIRECT half of the
// list from the source and diffs it against the array; the TRANSITIVE half is
// still hand-built and still cannot discover a door nobody wrote down. It says nothing about doors reached from anywhere else in
// the process, and nothing about whether the bodies behind those doors are
// CORRECT — that is half A's job. A green board here means "every door I was
// told about is answered by our registry", never "the screenshot works at
// cutover".
//
// It prints `MEASURED NOTHING` rather than a green line when the list is empty
// or when EITHER control fails, because from outside "no problem found" and "I
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
// The DLL's own export directory, walked without any resolver (resolve.h).
int RtEnumRuntimeExports(bool (*visit)(const char* name, void* addr,
                                       uintptr_t ghidraAddr, void* ctx),
                         void* ctx);

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
    // WAVE 20 LANE DOORS. True for a row whose ADDRESS door is registered only
    // on the DLL-ABSENT path, which a screen running with the DLL present can
    // never observe. Such a row reads OPEN here and that is not evidence of a
    // gap; it is this screen being unable to see the registration. It is
    // counted OPEN anyway -- a board that quietly re-scored the rows it cannot
    // check would be flattering itself -- and named on its own line instead.
    bool cutoverOnlyAddr;
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
    { "?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z", 0x1800ae430, "(18) lane FILE", false },
    { "?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z",     0x1800aa8f0, "(18) lane FILE", false },
    { "??0BinaryFileStream@IO@@QEAA@XZ",                          0x1800a8200, "(18) lane FILE", false },
    { "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z",        0x1800accc0, "(18) lane FILE", false },
    { "??1BinaryFileStream@IO@@UEAA@XZ",                          0x180023c80, "(18) lane FILE", false },
    { "??1FileStream@IO@@UEAA@XZ",                                0x1800aaab0, "(18) lane FILE", false },

    // ---- (20) FOUR DOORS THE BOARD NEVER LISTED, found by lane DOORS -------
    //
    // These four are `RT_EXPORT_ONCE` sites in `Video_ImageBuffer_SaveImageToFile`
    // ITSELF — save_image_to_file.cpp:227-233, unconditional, in the first
    // statement block, before anything the board did list. The row above says
    // C2 "measured six out of save_image_to_file.cpp"; the file has TEN
    // RT_EXPORT_ONCE sites and one RT_DATA_SLOT. Six of the ten were carried
    // forward and four were not, and nothing noticed, because a hand-built list
    // has no derivation to disagree with it.
    //
    // This is the THIRD time this debt has been found deeper than stated (the
    // Wave 19 plan said four exports, C2 said six, a call-graph walk found ten,
    // and this walk finds four more). `tools/screenshot_door_walk.py` now
    // re-derives the DIRECT half of this list from the source on every run and
    // diffs it against the array, so a fourth instance is a tool failure rather
    // than a reading failure.
    //
    // All four are ported by other lanes, so they land SHUT — the denominator
    // moves and the open set does not. That is why they were survivable, and it
    // is not a reason to have left them out: an unlisted door is unlisted
    // whether or not it happens to be shut today.
    { "?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z",         0x1800c6740, "(20) walk, lane Filename", false },
    { "?GetPathDirectory@Filename@System@@QEBA?AV12@XZ",          0x1800c6880, "(20) walk, lane SetupDir", false },
    { "??HSystem@@YA?AVString@0@AEBV10@0@Z",                      0x1800c9440, "(20) walk, lane String", false },
    { "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z",                   0x1800c92d0, "(20) walk, lane String", false },

    // ---- (19) the closure between those six and Win32 ----------------------
    { "?CreateFileW@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z",
                                                                  0x1800acc50, "(19) closure", false },
    { "?PlatformCreateFile@FileSystem@IO@@AEAA_NAEAUFileInfo@12@AEBVFilename@System@@@Z",
                                                                  0x1800ade60, "(19) closure", false },
    { "?PlatformCloseFile@FileSystem@IO@@AEAAXAEAUFileInfo@12@@Z", 0x1800ae020, "(19) closure", false },
    { nullptr,                                                    0x18000c180, "(19) ~FileInfo, unexported", false },

    // ---- (pre) already ported by earlier lanes -----------------------------
    { "?SubString@String@System@@QEBA?AV12@HH@Z",                  0x1800c9230, "(pre) lane String", false },
    { "?AdjustDelimiters@Filename@System@@AEAAXXZ",                0x1800c6970, "(pre) lane String", false },
    { "??4Filename@System@@QEAAAEAV01@AEBV01@@Z",                  0x180006770, "(pre) lane String", false },
    { "??0WideString@System@@QEAA@AEBVString@1@@Z",                0x1800cab50, "(pre) lane WideStr", false },
    { "?Set@WideString@System@@QEAAXPEB_W@Z",                      0x1800caf40, "(pre) lane WideStr", false },
    { "?AsUTF8@WideString@System@@QEBA?AVString@2@XZ",             0x1800cacd0, "(pre) lane WideStr", false },
    { nullptr,                                                    0x18000c200, "(pre) FileInfo::operator=", false },
    { nullptr,                                                    0x180006a10, "(pre) guarded object free", false },
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
    // `cutoverOnlyAddr` on both: allocator.cpp's RegisterAllocatorAddresses()
    // — the ONLY registrar of 0x1800c7a50/0x1800c7ad0 — is called from the
    // `if (!RuntimeDllPresent())` arm alone (allocator.cpp:1343-1372). With the
    // DLL present, `BANISHED_RT_ALLOC=1` registers the EXPORTS only, which is
    // exactly what this screen measured: armed, the two rows read
    // `name=yes addr=NO`. Their by-address callers (rtcoll::MemAlloc/MemFree in
    // collection/array.cpp:166,176 and application/message_callbacks.cpp:104)
    // have NO name fallback, so the address door is the only door they use.
    { "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z",             0x1800c7a50, "(pre) allocator", true },
    { "?MemFree@MemoryAllocator@System@@SAXPEAX@Z",                0x1800c7ad0, "(pre) allocator", true },

    // ---- (pre) the DATA doors — vtables, not code -------------------------
    // Listed with the code doors on purpose: at cutover an unminted vtable is
    // exactly as fatal as an unported function, and separating them is how a
    // board reads green while the object gets a null vptr.
    { "?_singleton@FileSystem@IO@@0PEAV12@EA",                     0x1801fc6a8, "(pre) DATA singleton", false },
    { "??_7Stream@IO@@6B@",                                        0x1801549a0, "(pre) DATA vtable", false },
    { "??_7FileStream@IO@@6B@",                                    0x180154960, "(19) DATA vtable, lane MINT", false },
    { "??_7BinaryFileStream@IO@@6B@",                              0x1801554d0, "(19) DATA vtable, lane MINT", false },
    { nullptr,                                                    0x1801554a8, "(pre) DATA encoding vtable", false },
};
const int kDoorCount = static_cast<int>(sizeof(kDoors) / sizeof(kDoors[0]));

// ---------------------------------------------------------------------------
// THE TWO CONTROLS, each scored on its OWN applicable case, never on a union
// (WAVE19_IO.md §6.3 — a union denominator overstates a blind control, and
// Wave 18 shipped one).
//
// WHAT THEY GUARD. This screen's whole verdict is `RtPortLookupExport(x) !=
// nullptr`. Two ways that can be worthless, and they invert each other:
//
//   * the lookup says YES to everything (it answered from the DLL, or from a
//     fall-through resolver) — then every `shut` below is a lie;
//   * the lookup says NO to everything (the registry is empty, unbuilt, or
//     BANISHED_RT_PORT=0) — then every `OPEN` below is a false alarm.
//
// One control each, or no verdict.
//
// ---------------------------------------------------------------------------
// WHY BOTH ARE NOW DERIVED — WAVE 20 LANE DOORS
// ---------------------------------------------------------------------------
// Until this wave the negative control was a HARD-CODED SYMBOL LITERAL,
// `?Commit@CompressedStream@IO@@QEAAXXZ`, chosen because it was a door that
// LOOKED ported and was not. Then a lane ported it, the control read PORTED,
// and this screen — correctly — refused to report. It stayed dark, and the
// board's last number (18 of 25 SHUT) went on being quoted from a document
// while nothing could re-derive it.
//
// The screen was not broken. The CONTROL'S SUBJECT was a free parameter that
// DECAYS AS LANES DO THEIR JOB: every symbol that qualifies today is one a lane
// is trying to disqualify. Picking a different literal only resets that clock,
// and the next lane to darken the board would have no way to know why.
//
// WAVE19_CLOSED.md §7.1: *remove the free parameter; failing that, PRINT IT
// BESIDE THE VERDICT.* Both are done here, in that order:
//
//   REMOVED   the subject is SELECTED AT RUN TIME from Runtime-x64.dll's own
//             export directory (`RtEnumRuntimeExports`, which consults no
//             registry and no resolver): the first named export the port
//             registry does not answer. There is no symbol to go stale,
//             because there is no symbol in the source.
//   PRINTED   the name it happened to pick is printed beside the verdict
//             anyway, with the denominators, so a reader can see the aim.
//
// AND IT CAN STILL FAIL — which is the part a derived control can quietly lose.
// The failure mode is not "the picked symbol turned out to be ported": it is
// `candidates == 0`, i.e. the registry answered YES to all ~4,800 exports.
// That is exactly the hazard the literal was guarding, reached from the other
// side, and it is a real reachable state, not a `static_assert(true)`:
// `BANISHED_RT_FILE_CUTOVER_CTLBREAK=yes` forces the lookup to answer non-null
// and drives it, and `=no` drives the positive control red the same way.
// Demonstrated red-then-green in docs/runtime_port/WAVE20_LANE_DOORS.md §1.
//
// The POSITIVE control is derived from the same single walk: `portedSeen`, the
// number of exported names the registry DOES answer, which must be non-zero
// over the same denominator. Two counters, one scan, opposite directions.
//
// `?OpenFile@FileSystem@IO@@` — the old named positive control, registered
// since Wave 9 lane PKG2 rung 3 — is still printed, but as an INFORMATIVE line
// that GATES NOTHING. It is a literal, and a literal in a gate is what darkened
// this screen; kept because a change in it is worth seeing, demoted because a
// change in it must not silence the board.
// ---------------------------------------------------------------------------
const char* const kCtlInformativePorted =
    "?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBVFilename@System@@@Z";

// ---- the break switch, which exists so the controls can be shown to fail ---
// Not a kill switch for the port: it only lies to THIS FILE'S copy of the
// lookup, so the screen sees a registry that always says yes / always says no.
enum class CtlBreak { None, AlwaysPorted, NeverPorted };

CtlBreak ReadBreak() {
    const char* e = std::getenv("BANISHED_RT_FILE_CUTOVER_CTLBREAK");
    if (!e || !*e) return CtlBreak::None;
    if (std::strcmp(e, "yes") == 0) return CtlBreak::AlwaysPorted;
    if (std::strcmp(e, "no")  == 0) return CtlBreak::NeverPorted;
    return CtlBreak::None;
}
CtlBreak g_break = CtlBreak::None;

// A non-null address that is NOT a function: the break must never be able to
// hand a caller something callable.
int g_breakSentinel = 0;

void* PortLookupName(const char* n) {
    if (g_break == CtlBreak::AlwaysPorted) return &g_breakSentinel;
    if (g_break == CtlBreak::NeverPorted)  return nullptr;
    return RtPortLookupExport(n);
}
void* PortLookupAddr(uintptr_t a) {
    if (g_break == CtlBreak::AlwaysPorted) return &g_breakSentinel;
    if (g_break == CtlBreak::NeverPorted)  return nullptr;
    return RtPortLookupAddr(a);
}

// ---- one walk of the DLL's export table, three products -------------------
struct Walk {
    int         scanned    = 0;   // named exports visited      (the denominator)
    int         withAddr   = 0;   // ... that resolve to an address in the module
    int         portedSeen = 0;   // ... the port registry answers  (POSITIVE ctl)
    int         candidates = 0;   // ... it does not              (NEGATIVE ctl)
    const char* pick       = nullptr;   // the negative control's derived subject
    uintptr_t   pickGhidra = 0;
    uintptr_t   dllAddr[kDoorCount];    // where the DLL says each named row lives
};

bool WalkVisit(const char* name, void* addr, uintptr_t ghidra, void* ctx) {
    Walk* w = static_cast<Walk*>(ctx);
    ++w->scanned;
    if (addr) ++w->withAddr;

    // Address provenance for the board's own rows. A row's `addr` field was
    // copied out of some lane's RegisterRuntimeAddr call, which makes the addr
    // column trivially green for the row that copied it; this asks the SHIPPED
    // BINARY where the symbol actually is.
    for (int i = 0; i < kDoorCount; ++i)
        if (kDoors[i].name && std::strcmp(kDoors[i].name, name) == 0)
            w->dllAddr[i] = ghidra;

    if (!addr) return true;                       // no body to be ported to
    if (PortLookupName(name)) { ++w->portedSeen; return true; }
    ++w->candidates;
    if (!w->pick) { w->pick = name; w->pickGhidra = ghidra; }
    return true;                                  // keep going: the counts ARE the control
}

bool g_armed = false;

void Run() {
    printf("[FileCutover] ==== screenshot-path door screen ====\n");
    printf("[FileCutover] subject: %d hand-enumerated doors on the path from "
           "?SaveImageToFile@ImageBuffer@Video@@ down to Win32. It cannot see a "
           "door nobody wrote down, and it says NOTHING about correctness.\n",
           kDoorCount);

    // ---- controls first, and their verdict gates everything below ---------
    //
    // ONE walk of Runtime-x64.dll's export directory produces both. Neither
    // names a symbol in this source; see the control block above for why the
    // literal that used to sit here had to go.
    g_break = ReadBreak();
    if (g_break != CtlBreak::None)
        printf("[FileCutover] *** BANISHED_RT_FILE_CUTOVER_CTLBREAK=%s IS SET: this "
               "run's registry lookup is DELIBERATELY LYING (%s). Nothing below is "
               "a measurement of the tree; this arm exists only to show the "
               "controls can go red. ***\n",
               g_break == CtlBreak::AlwaysPorted ? "yes" : "no",
               g_break == CtlBreak::AlwaysPorted ? "always answers PORTED"
                                                 : "never answers PORTED");

    Walk w;
    for (int i = 0; i < kDoorCount; ++i) w.dllAddr[i] = 0;
    const int walked = RtEnumRuntimeExports(&WalkVisit, &w);

    const bool ctlPosOk = (w.portedSeen > 0);
    const bool ctlNegOk = (w.candidates > 0) && (w.pick != nullptr);

    printf("[FileCutover] CONTROL walk: %d named exports in Runtime-x64.dll, "
           "%d with an address (the denominator for both controls below)\n",
           walked, w.withAddr);
    printf("[FileCutover] CONTROL positive (registry MUST be able to say YES): "
           "%d of %d exports answered -> %s\n",
           w.portedSeen, w.withAddr, ctlPosOk ? "GREEN" : "RED");
    printf("[FileCutover] CONTROL negative (registry MUST be able to say NO):  "
           "%d of %d exports unanswered -> %s\n",
           w.candidates, w.withAddr, ctlNegOk ? "GREEN" : "RED");
    printf("[FileCutover] CONTROL negative SUBJECT, derived not named: %s "
           "(0x%09llx) -- the first export the registry does not answer. Printed "
           "so the aim is visible; nothing in this file selects it.\n",
           w.pick ? w.pick : "(none -- and that IS the red)",
           (unsigned long long)w.pickGhidra);

    // Informative only, and deliberately NOT part of the gate: it is a literal,
    // and a literal in a gate is what darkened this screen for a whole wave.
    printf("[FileCutover] informative (GATES NOTHING) %-64s : %s\n",
           kCtlInformativePorted,
           PortLookupName(kCtlInformativePorted) ? "ported" : "NOT ported");

    if (kDoorCount == 0 || !ctlPosOk || !ctlNegOk) {
        printf("[FileCutover] *** MEASURED NOTHING: %s. No verdict is offered "
               "below, and an absence of red lines here is not evidence. ***\n",
               kDoorCount == 0     ? "the door list is empty"
               : walked == 0       ? "the export walk visited nothing, so neither "
                                     "control had a case to be scored on"
               : !ctlPosOk         ? "the registry answered NO to every export, so "
                                     "every OPEN below would be a false alarm"
                                   : "the registry answered YES to every export, so "
                                     "every shut below would be a lie");
        return;
    }

    int nameDoors = 0, nameOpen = 0;
    int addrDoors = 0, addrOpen = 0;
    int openRows  = 0;

    for (int i = 0; i < kDoorCount; ++i) {
        const Door& d = kDoors[i];
        bool byName = false, byAddr = false;
        if (d.name) { ++nameDoors; byName = (PortLookupName(d.name) != nullptr);
                      if (!byName) ++nameOpen; }
        if (d.addr) { ++addrDoors; byAddr = (PortLookupAddr(d.addr) != nullptr);
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

    // ---- rows this screen CANNOT SCORE on this route ----------------------
    int cutoverOnly = 0, cutoverOnlyOpen = 0;
    for (int i = 0; i < kDoorCount; ++i) {
        if (!kDoors[i].cutoverOnlyAddr) continue;
        ++cutoverOnly;
        if (kDoors[i].addr && !PortLookupAddr(kDoors[i].addr)) {
            ++cutoverOnlyOpen;
            printf("[FileCutover] UNSCOREABLE ON THIS ROUTE (counted OPEN above) "
                   "0x%09llx %s -- its address door is registered only by the "
                   "DLL-ABSENT arm of the allocator's arming path, which cannot "
                   "run while the DLL is loaded.\n",
                   (unsigned long long)kDoors[i].addr, kDoors[i].name);
        }
    }
    if (cutoverOnly)
        printf("[FileCutover] %d of %d rows are cutover-only by construction; %d of "
               "them read OPEN in this run. Subtracting them would be a claim this "
               "screen cannot make, so the count above does NOT.\n",
               cutoverOnly, kDoorCount, cutoverOnlyOpen);

    // ---- ADDRESS PROVENANCE, derived from the export table ----------------
    // Every `addr` in the array above was copied out of some lane's
    // RegisterRuntimeAddr call, which makes the addr column trivially green for
    // whoever copied it: the row and the registration share one typo. This asks
    // the SHIPPED BINARY where each named symbol actually lives and compares.
    // It cannot check the address-only rows -- they have no name to look up --
    // and that inapplicable set is stated, not folded in.
    int provChecked = 0, provOk = 0, provSkipped = 0;
    for (int i = 0; i < kDoorCount; ++i) {
        if (!kDoors[i].name || !kDoors[i].addr || w.dllAddr[i] == 0) {
            ++provSkipped;
            continue;
        }
        ++provChecked;
        if (w.dllAddr[i] == kDoors[i].addr) { ++provOk; continue; }
        printf("[FileCutover] PROVENANCE MISMATCH row says 0x%09llx, the DLL's "
               "export table says 0x%09llx : %s\n",
               (unsigned long long)kDoors[i].addr,
               (unsigned long long)w.dllAddr[i], kDoors[i].name);
    }
    printf("[FileCutover] address provenance %d of %d checkable rows match the "
           "DLL's own export table (%d rows have no exported name to check "
           "against and are NOT scored)\n", provOk, provChecked, provSkipped);


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

// ---------------------------------------------------------------------------
// WAVE 20 LANE PROMOTE -- a SEPARATE atexit, deliberately.
//
// This block first sat inside Run(), AFTER the door screen's control gate,
// and on its first measurement run it printed NOTHING: the door screen's own
// negative control (?Commit@CompressedStream@IO@@, "must read NOT-PORTED")
// now reads PORTED -- another lane ported it -- so Run() correctly refused to
// report and took this instrument down with it. A control failing for the
// DOOR BOARD says nothing about the counters, which are a different subject
// with their own liveness control (`stamps`). Separated so one screen's
// red cannot silence the other.
// ---------------------------------------------------------------------------
void RunBfsPromote() {
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
    // The per-body breakdown, printed HERE as well as in the door screen: the
    // door screen's own control is red on this tree, so its copy of this line
    // never prints, and "attendance" with no body named is the weakest possible
    // form of the claim.
    unsigned long long sb[14] = {0};
    RtIO::RtIoFileStreamSlotCounters(sb);
    printf("[BfsPromote] BODIES      GetSource=%llu Write=%llu Read=%llu "
           "SetCurrentPos=%llu GetCurrentPos=%llu GetSize=%llu Open=%llu "
           "deletingDtor=%llu copyCtor=%llu\n",
           sb[0], sb[2], sb[4], sb[5], sb[7], sb[8], sb[9], sb[11], sb[13]);

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

}

}  // namespace

void RtIoFileCutoverProbeArm() {
    const char* e = std::getenv("BANISHED_RT_FILE_CUTOVER");
    if (!e || !*e || std::strcmp(e, "0") == 0) return;
    if (g_armed) return;
    g_armed = true;
    std::atexit(&Run);
    std::atexit(&RunBfsPromote);
}
