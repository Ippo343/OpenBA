// ============================================================================
// runtime/src/object/external_path_stack_probe.cpp — Wave 11 lane OBJEXT
//   The oracle for PushPath / PopPath and the four
//   `Collection::Array<PathStackEntry>` bodies underneath them.
// ============================================================================
//
// TWO PARTS, TWO SUBJECTS, ONE RUN
// --------------------------------
//   PART 1  the four container bodies, lockstep against the shipped ones on a
//           PRIVATE pair of arrays.
//   PART 2  PushPath / PopPath, A/B against the shipped ones on THE LIVE
//           `ExternalFactory` SINGLETON.
//
// PART 2 IS NOT ON A FAKE, AND THAT IS DELIBERATE. Every row in this lane is a
// non-static member on `?_singleton@ExternalFactory@Object@@`, and that object
// is constructed by RETAIL's constructor — an in-DLL call our port never
// intercepts. So the `this` our bodies receive at runtime is retail's object,
// with retail's field layout, and an oracle that constructs its own
// `ExternalFactory` would be measuring an object the game never uses
// (reference_synthetic_subject_beats_a_fake.md — a synthetic subject beats a
// fake, but a REAL subject beats both when one is reachable). Part 2 therefore
// takes the live singleton, and it also VALIDATES THE OFFSET while it is there:
// it cross-checks `_pathStack` at +0x30/+0x38 against retail's own
// `GetPathStackDirectory` / `GetPathStackFilename` accessors reading the same
// object, which is the only independent evidence available that our header's
// `static_assert`s describe the object retail actually built.
//
// PART 1 IS ON A PRIVATE SUBJECT, and that is also deliberate: its subject is a
// `Collection::Array`, not the factory. The array is a plain three-field header
// with no constructor of retail's to respect, both sides run on structurally
// identical private ones, and driving the container arms this way is the only
// way to reach the shrink and shift paths at all — the live stack is EMPTY on
// every one of a normal run's calls (`external_path_win.cpp` measured
// `stack empty/nonempty = 22583/0`).
//
// WHY A LOCKSTEP AND NOT A LIVE REPLAY
// ------------------------------------
// Every entry point here is a DESTRUCTIVE MUTATOR. Running ours and then theirs
// at a live call site pushes the same entry twice, and on the pop side pops an
// element the first call already freed. A live shadow is not merely weak here,
// it is wrong.
//
// WHY IT COMPARES TEXT AND NOT POINTERS
// -------------------------------------
// The two sides allocate their own buffers, so the `char*` in an element is
// never equal across sides and a memcmp of the elements fails 100% of calls —
// which reads like a catastrophic port defect and is nothing of the kind
// (reference_oracle_must_not_compare_unwritten_bytes.md). What must agree is
// the STRING and the SHAPE. Capacity is compared as well as size: the
// `cap = n*3` growth rule is the arm a paraphrase is likeliest to get wrong and
// it is invisible to a contents-only check.
//
// THE NEGATIVE CONTROLS RUN IN THIS SAME PROCESS
// ----------------------------------------------
// All of them, swept by setting `RtObject::g_negCtl` around each pass. A
// control that costs a game launch each is a control that does not get run —
// and this wave has three lanes queueing on one game directory, where a single
// blocking acquire has already timed out. `external_path_win.cpp:170` gives the
// same reason for the same shape.
//
// Each control is scored SEPARATELY, with the mechanism of any miss named. A
// score on the UNION of two screens hides that one of them caught 1 of 3
// (reference_union_control_hides_a_blind_screen.md). Which part each control
// can even reach is stated up front, because a control aimed at a body a screen
// never drives reports NOT CAUGHT and indicts a sound instrument
// (reference_control_must_pick_a_compared_subject.md):
//
//   ctl 1  "-" not "." joining the entry filename   -> PART 2 only
//   ctl 2  entry halves stored in the wrong order   -> PART 1 and PART 2
//   ctl 3  RemoveAt skips the shift-down            -> PART 1 only
//   ctl 4  SetSize grows by 2 instead of 1          -> PART 1 and PART 2
//
// ctl 4 is NOT "ignore the fill", which is what a first draft used. With
// `fill == nullptr` and no `init` arm, retail leaves the new tail
// UNINITIALISED, so the probe's own comparison would dereference a garbage
// `char*` — a crash, not a divergence. Growing by two keeps every element
// initialised (SetSizeCore assigns the fill to every element in
// `[oldSize, newSize)`) and diverges in the SHAPE, which is compared.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "external_path_stack.h"

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../../types/collection/Collection.h"

// Declared locally rather than including standalone/resolve.h, which drags in
// <windows.h> (the same reason external_set_probe.cpp:99 declares it).
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

namespace RtSystem {
Filename* RtSystem_Filename_CtorCStr(Filename* self, const char* src);
}  // namespace RtSystem

namespace {

using RtObject::PathStackEntry;
using RtObject::ExternalFactory;
using RtSystem::Filename;
using RtSystem::String;
using RtSystem::RtString_ReleaseInline;

using Arr = Collection::Array<PathStackEntry>;

bool g_armed = false;

// ---- the shipped bodies -----------------------------------------------------
// PART 1's four have NO export in either build — `Collection::Array<T>` is
// header-only (WAVE9 lane Q) — so `ResolveExportRaw` cannot reach them and
// `ResolveRuntimeRaw` is the only door.
//
// PART 2's two ARE exported, and are STILL reached by address here, for the
// reason external_path_win.cpp:793 reaches them the same way: `ResolveExport`
// consults the port registry and would hand back OUR body, and so would
// `ResolveAddr(Module::Runtime, …)`
// (reference_resolveaddr_consults_port_registry_first.md). `ResolveRuntimeRaw`
// skips the registry by construction, which is what an oracle needs.
typedef void  (*SetSizeFn)(void*, int, const void*);
typedef void* (*RemoveLastFn)(void*, void*);
typedef void  (*RemoveAtFn)(void*, int);
typedef void  (*PushPathFn)(ExternalFactory*, const Filename*);
typedef void  (*PopPathFn)(ExternalFactory*);
typedef Filename* (*StackReaderFn)(const ExternalFactory*, Filename*);

SetSizeFn    TheirSetSize()    { return (SetSizeFn)   ResolveRuntimeRaw(0x18002e220); }
RemoveLastFn TheirRemoveLast() { return (RemoveLastFn)ResolveRuntimeRaw(0x18002ccb0); }
RemoveAtFn   TheirRemoveAt()   { return (RemoveAtFn)  ResolveRuntimeRaw(0x18002de30); }
PushPathFn   TheirPush()       { return (PushPathFn)  ResolveRuntimeRaw(0x1800bd0e0); }
PopPathFn    TheirPop()        { return (PopPathFn)   ResolveRuntimeRaw(0x1800bd040); }
// Retail's own path-stack readers, used ONLY to cross-check the +0x30/+0x38
// offset against the live object. Never as the comparison itself.
StackReaderFn TheirStackDir()  { return (StackReaderFn)ResolveRuntimeRaw(0x1800bcfd0); }
StackReaderFn TheirStackFile() { return (StackReaderFn)ResolveRuntimeRaw(0x1800bcf60); }

// ---- comparison -------------------------------------------------------------
char g_lastOurs[512];
char g_lastTheirs[512];

void Describe(const Arr* a, char* out, size_t n) {
    int w = std::snprintf(out, n, "size=%d cap=%d [", a->_size, a->_capacity);
    for (int i = 0; i < a->_size && w > 0 && (size_t)w < n; ++i) {
        const PathStackEntry* e = a->_data + i;
        w += std::snprintf(out + w, n - (size_t)w, "%s%s|%s",
                           i ? "," : "",
                           e->directory.data ? e->directory.data : "(null)",
                           e->filename.data  ? e->filename.data  : "(null)");
    }
    if (w > 0 && (size_t)w < n) std::snprintf(out + w, n - (size_t)w, "]");
}

struct Score {
    int compared;
    int mismatches;
};

const char* const kScript[] = {
    "Terrain/Grass.rsc",
    "Template/Building/House.rsc",
    "Dialog/MainMenu.rsc",
    "Audio/Music/Theme.rsc",
};
const int kScriptN = (int)(sizeof(kScript) / sizeof(kScript[0]));

void MakeEntry(PathStackEntry* e, const char* dir, const char* file) {
    e->directory.data = nullptr;
    e->filename.data  = nullptr;
    RtSystem::RtSystem_Filename_CtorCStr(&e->directory, dir);
    RtSystem::RtSystem_Filename_CtorCStr(&e->filename,  file);
}

void FreeEntry(PathStackEntry* e) {
    RtString_ReleaseInline(reinterpret_cast<String*>(&e->filename));
    RtString_ReleaseInline(reinterpret_cast<String*>(&e->directory));
}

void DrainOurs(Arr* a) {
    while (a->_size > 0) { PathStackEntry o; RtObject_PathStackRemoveLast_18002ccb0(a, &o); FreeEntry(&o); }
}
void DrainTheirs(Arr* a) {
    while (a->_size > 0) { PathStackEntry o; TheirRemoveLast()(a, &o); FreeEntry(&o); }
}

// ---------------------------------------------------------------------------
// PART 1 — the four container bodies, one pass at one control setting.
//
// The script drives every arm at least once:
//   push #1      capacity 0 -> 1, the `oldSize == 0` arm (NO *3)
//   push #2      the `n == old+1 && old != 0` arm, capacity 2 -> 6
//   push #3/#4   inside capacity: no realloc, no destroy
//   RemoveAt(0)  the shift-down loop, three elements moved
//   pops         the shrink arm with `realloced == false`
// ---------------------------------------------------------------------------
Score RunContainerPass(int ctl) {
    Score s = { 0, 0 };
    Arr ours{nullptr, 0, 0};
    Arr theirs{nullptr, 0, 0};

    const int grow = (ctl == 4) ? 2 : 1;

    for (int i = 0; i < kScriptN; ++i) {
        PathStackEntry eo, et;
        MakeEntry(&eo, "Data/", kScript[i]);
        MakeEntry(&et, "Data/", kScript[i]);
        RtObject::g_negCtl = ctl;
        RtObject_PathStackSetSize_18002e220(&ours, ours._size + grow, &eo);
        RtObject::g_negCtl = 0;
        TheirSetSize()(&theirs, theirs._size + 1, &et);
        FreeEntry(&eo);
        FreeEntry(&et);
        ++s.compared;
        Describe(&ours, g_lastOurs, sizeof(g_lastOurs));
        Describe(&theirs, g_lastTheirs, sizeof(g_lastTheirs));
        if (std::strcmp(g_lastOurs, g_lastTheirs) != 0) ++s.mismatches;
    }

    RtObject::g_negCtl = ctl;
    RtObject_PathStackRemoveAt_18002de30(&ours, 0);
    RtObject::g_negCtl = 0;
    TheirRemoveAt()(&theirs, 0);
    ++s.compared;
    Describe(&ours, g_lastOurs, sizeof(g_lastOurs));
    Describe(&theirs, g_lastTheirs, sizeof(g_lastTheirs));
    if (std::strcmp(g_lastOurs, g_lastTheirs) != 0) ++s.mismatches;

    while (ours._size > 0 && theirs._size > 0) {
        PathStackEntry o, t;
        RtObject::g_negCtl = ctl;
        RtObject_PathStackRemoveLast_18002ccb0(&ours, &o);
        RtObject::g_negCtl = 0;
        TheirRemoveLast()(&theirs, &t);
        const bool textOk =
            o.directory.data && t.directory.data &&
            std::strcmp(o.directory.data, t.directory.data) == 0 &&
            o.filename.data && t.filename.data &&
            std::strcmp(o.filename.data, t.filename.data) == 0;
        FreeEntry(&o);
        FreeEntry(&t);
        ++s.compared;
        Describe(&ours, g_lastOurs, sizeof(g_lastOurs));
        Describe(&theirs, g_lastTheirs, sizeof(g_lastTheirs));
        if (!textOk || std::strcmp(g_lastOurs, g_lastTheirs) != 0) ++s.mismatches;
    }

    RtObject::g_negCtl = 0;
    DrainOurs(&ours);
    DrainTheirs(&theirs);
    return s;
}

// ---------------------------------------------------------------------------
// PART 2 — PushPath / PopPath on the LIVE singleton.
//
// SYMMETRIC AND SELF-CLEANING. Ours pushes, the entry is read back off the live
// object, ours pops. Then theirs pushes, the entry is read back the same way,
// theirs pops. The two readbacks are compared. The stack depth is asserted back
// to where it started after EACH half, so the probe leaves no trace in the
// object the game is about to use — and no trace in the counters either, which
// is why `g_psc` is snapshotted and restored around the whole thing (a script's
// calls are not the game's, and letting them into `calls=` would make the site
// look better exercised than it is).
// ---------------------------------------------------------------------------
bool ReadTop(const ExternalFactory* f, char* out, size_t n, bool useRetailReader) {
    const Arr* a = reinterpret_cast<const Arr*>(
        reinterpret_cast<const uint8_t*>(f) + 0x30);
    if (a->_size <= 0) { std::snprintf(out, n, "(empty)"); return false; }
    if (!useRetailReader) {
        const PathStackEntry* e = a->_data + (a->_size - 1);
        std::snprintf(out, n, "%s|%s",
                      e->directory.data ? e->directory.data : "(null)",
                      e->filename.data  ? e->filename.data  : "(null)");
        return true;
    }
    // The independent offset cross-check: retail's own accessors, on the same
    // object, must see what we see at +0x30/+0x38.
    Filename d{nullptr}, fl{nullptr};
    TheirStackDir()(f, &d);
    TheirStackFile()(f, &fl);
    std::snprintf(out, n, "%s|%s",
                  d.data ? d.data : "(null)", fl.data ? fl.data : "(null)");
    RtString_ReleaseInline(reinterpret_cast<String*>(&fl));
    RtString_ReleaseInline(reinterpret_cast<String*>(&d));
    return true;
}

Score RunLivePushPop(ExternalFactory* f, int ctl, const char* probePath,
                     int* offsetCrossChecks, int* offsetCrossFails) {
    Score s = { 0, 0 };
    const Arr* a = reinterpret_cast<const Arr*>(
        reinterpret_cast<const uint8_t*>(f) + 0x30);
    const int depth0 = a->_size;

    Filename p{nullptr};
    RtSystem::RtSystem_Filename_CtorCStr(&p, probePath);

    char oursTxt[256], theirsTxt[256], oursViaRetail[256];

    RtObject::g_negCtl = ctl;
    RtObject::RtObject_ExternalFactory_PushPath(f, &p);
    RtObject::g_negCtl = 0;
    ReadTop(f, oursTxt, sizeof(oursTxt), /*useRetailReader=*/false);
    // Same element, read through retail's accessors: an offset cross-check, not
    // part of the ours-vs-theirs comparison.
    if (ReadTop(f, oursViaRetail, sizeof(oursViaRetail), /*useRetailReader=*/true)) {
        ++*offsetCrossChecks;
        if (std::strcmp(oursTxt, oursViaRetail) != 0) ++*offsetCrossFails;
    }
    RtObject::RtObject_ExternalFactory_PopPath(f);
    const int depthAfterOurs = a->_size;

    TheirPush()(f, &p);
    ReadTop(f, theirsTxt, sizeof(theirsTxt), /*useRetailReader=*/false);
    TheirPop()(f);
    const int depthAfterTheirs = a->_size;

    RtString_ReleaseInline(reinterpret_cast<String*>(&p));

    ++s.compared;
    if (std::strcmp(oursTxt, theirsTxt) != 0) ++s.mismatches;
    // The balance check is part of the verdict, not decoration: a Push that
    // grows by the wrong amount or a Pop that removes the wrong number of
    // elements is a defect the text comparison alone cannot see.
    if (depthAfterOurs != depth0 || depthAfterTheirs != depth0) ++s.mismatches;

    std::snprintf(g_lastOurs,   sizeof(g_lastOurs),   "%s (depth %d->%d)",
                  oursTxt,   depth0, depthAfterOurs);
    std::snprintf(g_lastTheirs, sizeof(g_lastTheirs), "%s (depth %d->%d)",
                  theirsTxt, depth0, depthAfterTheirs);
    return s;
}

}  // namespace

void RtObjectPathStackProbeArm(bool on) { g_armed = on; }
bool RtObjectPathStackProbeArmed()      { return g_armed; }

void RtObjectPathStackProbeRun(ExternalFactory* live) {
    if (!g_armed) return;

    if (TheirSetSize() == nullptr || TheirRemoveLast() == nullptr ||
        TheirRemoveAt() == nullptr) {
        std::printf("[PathStack] PROBE SKIPPED -- a shipped container body did "
                    "not resolve; NO COMPARISON WAS MADE\n");
        return;
    }

    // Snapshot/restore rather than a guard at each `++`: the script must leave
    // NO trace in the effectiveness counters, or they stop measuring the game
    // and start measuring the probe.
    const RtObject::PathStackCounters saved = RtObject::g_psc;

    // ---- PART 1 -------------------------------------------------------------
    const int ctls1[] = { 0, 2, 3, 4 };
    for (int i = 0; i < 4; ++i) {
        const Score s = RunContainerPass(ctls1[i]);
        const bool isCtl = ctls1[i] != 0;
        std::printf("[PathStack] PART1 ctl=%d compared=%d mismatches=%d  %s\n",
                    ctls1[i], s.compared, s.mismatches,
                    !isCtl ? (s.mismatches == 0 ? "PASS" : "*** PORT DEFECT ***")
                           : (s.mismatches  > 0 ? "control CAUGHT"
                                                : "*** control NOT CAUGHT ***"));
        if (s.compared == 0)
            std::printf("[PathStack] PART1 ctl=%d MADE NO COMPARISONS -- "
                        "this is not a pass\n", ctls1[i]);
        if (s.mismatches && ctls1[i] == 0) {
            std::printf("[PathStack] PART1   ours = %s\n", g_lastOurs);
            std::printf("[PathStack] PART1 theirs = %s\n", g_lastTheirs);
        }
    }

    // ---- PART 2 -------------------------------------------------------------
    if (live == nullptr) {
        std::printf("[PathStack] PART2 SKIPPED -- no live ExternalFactory; "
                    "NO COMPARISON OF PushPath/PopPath WAS MADE\n");
    } else if (TheirPush() == nullptr || TheirPop() == nullptr) {
        std::printf("[PathStack] PART2 SKIPPED -- shipped PushPath/PopPath did "
                    "not resolve; NO COMPARISON WAS MADE\n");
    } else {
        int xChecks = 0, xFails = 0;
        const int ctls2[] = { 0, 1, 2, 4 };
        for (int i = 0; i < 4; ++i) {
            const Score s = RunLivePushPop(live, ctls2[i],
                                           "Template/Building/House.rsc",
                                           &xChecks, &xFails);
            const bool isCtl = ctls2[i] != 0;
            std::printf("[PathStack] PART2 ctl=%d compared=%d mismatches=%d  %s\n",
                        ctls2[i], s.compared, s.mismatches,
                        !isCtl ? (s.mismatches == 0 ? "PASS" : "*** PORT DEFECT ***")
                               : (s.mismatches  > 0 ? "control CAUGHT"
                                                    : "*** control NOT CAUGHT ***"));
            if (s.mismatches && ctls2[i] == 0) {
                std::printf("[PathStack] PART2   ours = %s\n", g_lastOurs);
                std::printf("[PathStack] PART2 theirs = %s\n", g_lastTheirs);
            }
        }
        std::printf("[PathStack] PART2 offset cross-check (+0x30/+0x38 read by "
                    "retail's own accessors on the live object): %d checked, "
                    "%d disagreed\n", xChecks, xFails);
    }

    RtObject::g_psc = saved;
    RtObject::g_negCtl = 0;
}
