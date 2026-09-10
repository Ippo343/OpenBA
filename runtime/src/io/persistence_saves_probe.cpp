// ============================================================================
// runtime/src/io/persistence_saves_probe.cpp — Wave 19 lane PERSIST's oracle
// ============================================================================
//
// A harness, not a port: nothing here is registered.
//
// WHY IT EXISTS. Five of the lane's seven rows are only reached from
// runtime/src/save/saveload_update.cpp behind a live save/load menu, and three
// of those five MUTATE the singleton IO::Persistence and then wake a worker
// thread. Ours-then-theirs on the live singleton would issue two requests for
// one call, so the obvious replay is unavailable — which is precisely the
// shape that has been mistaken for "no oracle" four times in this project
// (reference_no_route_is_not_no_oracle.md).
//
// It is not "no oracle". The whole issued state is inside the object: a
// String, three ints and two event handles. So the subject is SYNTHESISED —
// two byte-identical scratch Persistence images, ours run on one and the
// shipped body on the other, then the WHOLE OBJECT compared. That works here
// and did not work for Wave 8's Audio rows for one concrete reason recorded in
// the port's banner: there is no stored completion callback in this cluster,
// so nothing has run into game state by the time the call returns.
//
// WHAT IS COMPARED, STATED SO IT CAN BE CHECKED
// ---------------------------------------------
// Bytes 0x00..0x98 of the object, EXCEPT +0x40..0x48 (`pendingFilename`),
// which is compared by CONTENT instead. That exception is not a convenience:
// both sides build their filename through an independent String allocation, so
// the two pointers are necessarily different and a raw memcmp of that qword
// would fail 100% of calls and read like a catastrophic port failure
// (reference_oracle_must_not_compare_unwritten_bytes.md). Everything else in
// the object IS position-compared, including the bytes neither side is
// expected to touch — because "the port scribbled somewhere it should not" is
// exactly what a whole-object compare is for.
//
// THE PROBE CAN SAY "I MEASURED NOTHING" (WAVE19_IO.md §6.6). Every case
// reports how many of the 0x98 bytes the subject CHANGED relative to the
// seeded pre-state. A case whose subject wrote nothing is reported as
// INERT and does not count towards a green.
//
// CONTROLS. Three, scored separately, on applicable cases only (§6.3). Each is
// a deliberately-wrong local MUTANT run in place of the port on the ours side,
// producing a real comparable divergence through the mechanism the control
// names (§6.4) rather than UB:
//
//   CTRL-OBJ    writes command=99 instead of 3  -> must be caught by the
//               whole-object compare at +0x4c
//   CTRL-STR    drops the ".sav" suffix         -> must be caught by the
//               filename CONTENT compare
//   CTRL-VAL    returns the array entry when flags&1 says refuse -> must be
//               caught by the by-value SaveGameInfo compare
//
// All three MUST read non-zero, and the probe prints them BEFORE any green so
// a zero is never trusted on its own (§6.2).
//
// SAFETY OF THE SYNTHESISED SUBJECT. The scratch images are seeded with NULL
// array pointers, and both Collection clear helpers the subjects reach
// (0x18002a930, 0x180024aa0) are `if (data == null) { zero the fields; return; }`
// on that input — read as disassembly, not assumed. The two event handles are
// real CreateEvent handles this file owns and both sides are given the SAME
// pair, so ResetEvent/SetEvent land somewhere harmless and the handle fields
// still position-compare.
//
// NOT PROBED: ??1Persistence@. Its worker destructor (0x1800b7740) joins and
// closes a real thread; handing it a fabricated worker is not a controlled
// experiment. See the wave doc's "what this does NOT claim".
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

#include "../../funcs/rt_port.h"
#include "../../types/io/Persistence.h"
#include "../system/string.h"

using RtSystem::String;
using RtSystem::StringRefCount;
using RtIo::Persistence;
using RtIo::SaveGameInfo;
using RtIo::SaveBlock;

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The ports under test.
int   RtIo_Persistence_GetSaveCount(const void*);
void* RtIo_Persistence_GetSaveInfo(const void*, void*, int);
void* RtIo_Persistence_GetSaveFilename(const void*, void*, const void*);
bool  RtIo_Persistence_BeginDelete(void*, const void*);
bool  RtIo_Persistence_BeginLoad(void*, const void*, int, const void*);
bool  RtIo_Persistence_BeginGetSaveGameInfo(void*, const void*, int);
void  RtIo_Persistence_dtor(void*);

extern "C" __declspec(dllimport) void* __stdcall CreateEventA(void*, int, int, const char*);
extern "C" __declspec(dllimport) int   __stdcall CloseHandle(void*);
extern "C" __declspec(dllimport) int   __stdcall SetEvent(void*);

namespace {

using FormatFn = void* (*)(void* ret, const char* fmt, ...);

FormatFn Format() {
    // The oracle must be able to build its inputs whether or not String::Format
    // is ported, and it is not the subject here, so the RAW door is correct.
    static auto fn = RtOriginal<FormatFn>("?Format@String@System@@SA?AV12@PEBDZZ");
    return fn;
}

String MakeString(const char* text) {
    String s;
    s.data = nullptr;
    Format()(&s, "%s", text);
    return s;
}

// ---- scoring ---------------------------------------------------------------
struct Screen {
    const char* name;
    int applicable = 0;   // cases this screen could speak about
    int agreed     = 0;
    int diverged   = 0;
    int inert      = 0;   // subject changed nothing -> this case measured nothing
    int ctrlCases  = 0;   // mutant runs attempted
    int ctrlCaught = 0;   // ...of which the screen caught
};

Screen g_obj{"whole-object (0x98 bytes, +0x40 by content)"};
// The whole object is compared, but only a handful of its bytes are ever
// WRITTEN by these subjects. Reporting the compared size alone would overstate
// this screen; the number below is the size of the set it discriminates on.
int g_objBytesWrittenMax = 0;
Screen g_str{"GetSaveFilename content"};
Screen g_val{"GetSaveInfo by-value struct"};
Screen g_int{"GetSaveCount scalar"};
Screen g_dtor{"~Persistence whole-object (handles/Strings by predicate)"};

// ---- the synthesised subject ----------------------------------------------
constexpr size_t kImage = 0x98;

struct Scratch {
    alignas(16) unsigned char bytes[kImage];
    Persistence* p() { return reinterpret_cast<Persistence*>(bytes); }
};

void* g_workEvent = nullptr;
void* g_doneEvent = nullptr;

// Seed one image. `saves`/`saveCount` are the caller's, shared by both sides:
// the subjects only ever READ them.
void Seed(Scratch& s, unsigned flags, const String& savePath,
          SaveGameInfo* saves, int saveCount) {
    std::memset(s.bytes, 0, kImage);
    Persistence* p = s.p();
    p->flags     = flags;
    p->workEvent = g_workEvent;
    p->doneEvent = g_doneEvent;
    p->saves     = saves;
    p->saveCount = saveCount;
    p->savePath  = savePath;      // shared buffer, never released by a subject
}

int BytesChanged(const Scratch& before, const Scratch& after) {
    int n = 0;
    for (size_t i = 0; i < kImage; ++i)
        if (before.bytes[i] != after.bytes[i]) ++n;
    return n;
}

// The whole-object comparison, with the ONE documented exception. Returns the
// byte offset of the first position difference, or -1 for agreement; sets
// `nameDiff` when the content-compared filename disagrees.
int CompareImages(Scratch& ours, Scratch& theirs, bool& nameDiff) {
    const size_t kNameLo = offsetof(Persistence, pendingFilename);
    const size_t kNameHi = kNameLo + sizeof(void*);

    const char* a = ours.p()->pendingFilename.data;
    const char* b = theirs.p()->pendingFilename.data;
    nameDiff = ((a == nullptr) != (b == nullptr)) ||
               (a != nullptr && std::strcmp(a, b) != 0);

    for (size_t i = 0; i < kImage; ++i) {
        if (i >= kNameLo && i < kNameHi) continue;
        if (ours.bytes[i] != theirs.bytes[i]) return (int)i;
    }
    return -1;
}

// Release whatever filename a subject left behind, so a 60-case sweep does not
// leak 60 String buffers into the live allocator.
void ReleaseImage(Scratch& s) {
    RtSystem::RtString_ReleaseInline(&s.p()->pendingFilename);
    s.p()->pendingFilename.data = nullptr;
}

// ---- the three mutants -----------------------------------------------------
// Each is the ported body with ONE compared field wrong. They exist to prove
// the corresponding screen is capable of reading non-zero.

bool Mutant_BeginGetSaveGameInfo(void* self, const void* nameRef, int count) {
    auto* p = static_cast<Persistence*>(self);
    if (p->flags & 1u) return false;
    String fn;
    fn.data = nullptr;
    RtIo_Persistence_GetSaveFilename(self, &fn, nameRef);
    using ClearFn = void (*)(void*);
    reinterpret_cast<ClearFn>(ResolveAddr(Module::Runtime, 0x18002a930))(&p->blockDescData);
    reinterpret_cast<ClearFn>(ResolveAddr(Module::Runtime, 0x180024aa0))(&p->blockPtrData);
    p->command = 99;                       // <-- CTRL-OBJ: should be 3
    if (p->pendingFilename.data != fn.data) {
        RtSystem::RtString_ReleaseInline(&p->pendingFilename);
        p->pendingFilename.data = fn.data;
        if (p->pendingFilename.data) ++StringRefCount(p->pendingFilename.data);
    }
    p->status     = 0;
    p->blockCount = count;
    RtSystem::RtString_ReleaseInline(&fn);
    return true;
}

// CTRL-REF: issues the request even though flags&1 says refuse. Applicable
// ONLY to flags=1 cases -- it is byte-identical to the port on flags=0, which
// is precisely why it must not be scored there
// (reference_union_control_hides_a_blind_screen.md).
bool Mutant_IgnoreRefusal(void* self, const void* nameRef, int count) {
    auto* p = static_cast<Persistence*>(self);
    String fn;
    fn.data = nullptr;
    RtIo_Persistence_GetSaveFilename(self, &fn, nameRef);
    using ClearFn = void (*)(void*);
    reinterpret_cast<ClearFn>(ResolveAddr(Module::Runtime, 0x18002a930))(&p->blockDescData);
    reinterpret_cast<ClearFn>(ResolveAddr(Module::Runtime, 0x180024aa0))(&p->blockPtrData);
    p->command = 3;
    if (p->pendingFilename.data != fn.data) {
        RtSystem::RtString_ReleaseInline(&p->pendingFilename);
        p->pendingFilename.data = fn.data;
        if (p->pendingFilename.data) ++StringRefCount(p->pendingFilename.data);
    }
    p->status     = 0;
    p->blockCount = count;
    RtSystem::RtString_ReleaseInline(&fn);
    return true;
}

// CTRL-CNT: returns the count even when flags&1 says refuse. Applicable ONLY
// to flags=1, same reason as CTRL-REF.
int Mutant_GetSaveCount(const void* self) {
    return static_cast<const Persistence*>(self)->saveCount;
}

void* Mutant_GetSaveFilename(const void* self, void* retval, const void* nameRef) {
    auto* p    = static_cast<const Persistence*>(self);
    auto* ret  = static_cast<String*>(retval);
    auto* name = static_cast<const String*>(nameRef);
    String tmp;
    tmp.data = nullptr;
    Format()(&tmp, "%sSave\\%s", p->savePath.data, name->data);   // <-- CTRL-STR: no ".sav"
    ret->data = nullptr;
    if (tmp.data) { ret->data = tmp.data; ++StringRefCount(ret->data); }
    RtSystem::RtString_ReleaseInline(&tmp);
    return retval;
}

void* Mutant_GetSaveInfo(const void* self, void* retval, int index) {
    auto* p   = static_cast<const Persistence*>(self);
    auto* ret = static_cast<SaveGameInfo*>(retval);
    const SaveGameInfo& e = p->saves[index];        // <-- CTRL-VAL: ignores flags&1
    ret->sortKey   = e.sortKey;
    ret->name.data = nullptr;
    if (e.name.data) { ret->name.data = e.name.data; ++StringRefCount(ret->name.data); }
    return retval;
}

// ---- the shipped bodies ----------------------------------------------------
using BeginInfoFn   = bool  (*)(void*, const void*, int);
using BeginDeleteFn = bool  (*)(void*, const void*);
using BeginLoadFn   = bool  (*)(void*, const void*, int, const void*);
using GetFilenameFn = void* (*)(const void*, void*, const void*);
using GetInfoFn     = void* (*)(const void*, void*, int);
using GetCountFn    = int   (*)(const void*);

// ---- one issue case --------------------------------------------------------
// `mutant` selects the mutant body for the ours side; nullptr means the port.
enum class Issue { Info, Delete, Load };
// Ctrl::None is the port. WrongCommand is applicable only where the port
// ISSUES (flags==0); IgnoreRefusal only where it REFUSES (flags==1).
enum class Ctrl { None, WrongCommand, IgnoreRefusal };

void RunIssueCase(Issue which, unsigned flags, const char* name, int count,
                  const String& savePath, Ctrl ctrl) {
    const bool useMutant = (ctrl != Ctrl::None);
    Scratch a, b, pre;
    Seed(a, flags, savePath, nullptr, 0);
    Seed(b, flags, savePath, nullptr, 0);
    std::memcpy(pre.bytes, a.bytes, kImage);

    String nameStr = MakeString(name);
    SaveBlock blocks[1] = {};
    bool oursRet = false, theirsRet = false;

    switch (which) {
    case Issue::Info:
        oursRet = ctrl == Ctrl::WrongCommand
                      ? Mutant_BeginGetSaveGameInfo(a.p(), &nameStr, count)
                  : ctrl == Ctrl::IgnoreRefusal
                      ? Mutant_IgnoreRefusal(a.p(), &nameStr, count)
                      : RtIo_Persistence_BeginGetSaveGameInfo(a.p(), &nameStr, count);
        theirsRet = RtOriginal<BeginInfoFn>(
            "?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z")(
            b.p(), &nameStr, count);
        break;
    case Issue::Delete:
        oursRet = RtIo_Persistence_BeginDelete(a.p(), &nameStr);
        theirsRet = RtOriginal<BeginDeleteFn>(
            "?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z")(b.p(), &nameStr);
        break;
    case Issue::Load:
        oursRet = RtIo_Persistence_BeginLoad(a.p(), &nameStr, 0, blocks);
        theirsRet = RtOriginal<BeginLoadFn>(
            "?BeginLoad@Persistence@IO@@QEAA_NAEBVString@System@@HPEBUSaveBlock@12@@Z")(
            b.p(), &nameStr, 0, blocks);
        break;
    }

    int changed = BytesChanged(pre, b);          // measured on the SHIPPED side
    bool nameDiff = false;
    int firstDiff = CompareImages(a, b, nameDiff);
    bool ok = (firstDiff < 0) && !nameDiff && (oursRet == theirsRet);

    Screen& sc = g_obj;
    if (useMutant) {
        ++sc.ctrlCases;
        if (!ok) ++sc.ctrlCaught;
        fprintf(stderr, "[PersistProbe]   %-15s %-6s flags=%u -> %s "
                        "(firstDiff=%d nameDiff=%d)\n",
                ctrl == Ctrl::WrongCommand ? "CTRL-OBJ/cmd" : "CTRL-OBJ/refuse",
                which == Issue::Info ? "Info" : which == Issue::Delete ? "Delete" : "Load",
                flags, ok ? "NOT CAUGHT" : "caught", firstDiff, (int)nameDiff);
    } else {
        ++sc.applicable;
        if (changed > g_objBytesWrittenMax) g_objBytesWrittenMax = changed;
        if (changed == 0) {
            ++sc.inert;
        } else if (ok) {
            ++sc.agreed;
        } else {
            ++sc.diverged;
            fprintf(stderr, "[PersistProbe]   DIVERGE %-6s flags=%u name=\"%s\": "
                            "firstDiff=+0x%x nameDiff=%d ours=%d theirs=%d\n",
                    which == Issue::Info ? "Info" : which == Issue::Delete ? "Delete" : "Load",
                    flags, name, firstDiff, (int)nameDiff, (int)oursRet, (int)theirsRet);
        }
        fprintf(stderr, "[PersistProbe]   %-6s flags=%u count=%d -> %s "
                        "(shipped side wrote %d of %d bytes)\n",
                which == Issue::Info ? "Info" : which == Issue::Delete ? "Delete" : "Load",
                flags, count, changed == 0 ? "INERT" : (ok ? "agree" : "DIVERGE"),
                changed, (int)kImage);
    }

    ReleaseImage(a);
    ReleaseImage(b);
    RtSystem::RtString_ReleaseInline(&nameStr);
}

// ---- GetSaveFilename: a pure function, so verify it by INPUT SWEEP ---------
// (project_pure_functions_verify_by_sweep.md — waiting for a caller is the
// slow way to learn a pure body is right.)
void RunFilenameSweep(const String& savePath, bool useMutant) {
    static const char* kNames[] = {
        "Save1", "My Town", "", "a/b\\c", "..", "Very Long Save Name 0123456789",
        "unicode\xc3\xa9", "trailing ", " leading", "dots.in.name",
    };
    Scratch s;
    Seed(s, 0, savePath, nullptr, 0);

    for (const char* n : kNames) {
        String nameStr = MakeString(n);
        String ours, theirs;
        ours.data = nullptr;
        theirs.data = nullptr;

        if (useMutant) Mutant_GetSaveFilename(s.p(), &ours, &nameStr);
        else           RtIo_Persistence_GetSaveFilename(s.p(), &ours, &nameStr);
        RtOriginal<GetFilenameFn>(
            "?GetSaveFilename@Persistence@IO@@AEBA?AVFilename@System@@AEBVString@4@@Z")(
            s.p(), &theirs, &nameStr);

        bool ok = (ours.data == nullptr) == (theirs.data == nullptr) &&
                  (ours.data == nullptr || std::strcmp(ours.data, theirs.data) == 0);

        if (useMutant) {
            ++g_str.ctrlCases;
            if (!ok) ++g_str.ctrlCaught;
        } else {
            ++g_str.applicable;
            // A filename is never empty here (the format string alone is 9
            // chars), so there is no INERT case on this screen; assert that
            // rather than assume it.
            if (theirs.data == nullptr) ++g_str.inert;
            else if (ok)                ++g_str.agreed;
            else {
                ++g_str.diverged;
                fprintf(stderr, "[PersistProbe]   DIVERGE Filename(\"%s\"): "
                                "ours=\"%s\" theirs=\"%s\"\n",
                        n, ours.data ? ours.data : "<null>",
                        theirs.data ? theirs.data : "<null>");
            }
        }
        RtSystem::RtString_ReleaseInline(&ours);
        RtSystem::RtString_ReleaseInline(&theirs);
        RtSystem::RtString_ReleaseInline(&nameStr);
    }
}

// ---- GetSaveCount / GetSaveInfo -------------------------------------------
void RunReaderSweep(const String& savePath, bool useMutant) {
    SaveGameInfo saves[3];
    String n0 = MakeString("Autosave");
    String n1 = MakeString("Town of Testing");
    saves[0].sortKey = 0x0123456789abcdefULL; saves[0].name = n0;
    saves[1].sortKey = 0;                     saves[1].name = n1;
    saves[2].sortKey = 0xffffffffffffffffULL; saves[2].name.data = nullptr;

    for (unsigned flags : {0u, 1u}) {
        Scratch s;
        Seed(s, flags, savePath, saves, 3);

        // CTRL-CNT and CTRL-VAL both differ from the port ONLY on the refusal
        // branch, so flags==0 is an INAPPLICABLE case for them and is not
        // scored. The first version of this probe scored CTRL-VAL on the union
        // and printed "3 of 6 caught", which reads like a half-blind screen and
        // is really a fully effective one asked three impossible questions
        // (reference_union_control_hides_a_blind_screen.md).
        const bool ctrlApplies = (flags == 1u);
        {
            int theirs = RtOriginal<GetCountFn>("?GetSaveCount@Persistence@IO@@QEBAHXZ")(s.p());
            if (useMutant) {
                if (ctrlApplies) {
                    ++g_int.ctrlCases;
                    if (Mutant_GetSaveCount(s.p()) != theirs) ++g_int.ctrlCaught;
                }
            } else {
                int ours = RtIo_Persistence_GetSaveCount(s.p());
                ++g_int.applicable;
                if (ours == theirs) ++g_int.agreed;
                else {
                    ++g_int.diverged;
                    fprintf(stderr, "[PersistProbe]   DIVERGE GetSaveCount flags=%u: "
                                    "ours=%d theirs=%d\n", flags, ours, theirs);
                }
            }
        }

        for (int i = 0; i < 3; ++i) {
            SaveGameInfo ours, theirs;
            ours.sortKey = 0;   ours.name.data = nullptr;
            theirs.sortKey = 0; theirs.name.data = nullptr;

            if (useMutant) Mutant_GetSaveInfo(s.p(), &ours, i);
            else           RtIo_Persistence_GetSaveInfo(s.p(), &ours, i);
            RtOriginal<GetInfoFn>(
                "?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z")(s.p(), &theirs, i);

            bool ok = ours.sortKey == theirs.sortKey &&
                      ours.name.data == theirs.name.data;

            if (useMutant) {
                if (!ctrlApplies) {
                    RtSystem::RtString_ReleaseInline(&ours.name);
                    RtSystem::RtString_ReleaseInline(&theirs.name);
                    continue;
                }
                ++g_val.ctrlCases;
                if (!ok) ++g_val.ctrlCaught;
            } else {
                ++g_val.applicable;
                if (ok) ++g_val.agreed;
                else {
                    ++g_val.diverged;
                    fprintf(stderr, "[PersistProbe]   DIVERGE GetSaveInfo flags=%u i=%d: "
                                    "ours={%llu,%p} theirs={%llu,%p}\n",
                            flags, i, (unsigned long long)ours.sortKey, (void*)ours.name.data,
                            (unsigned long long)theirs.sortKey, (void*)theirs.name.data);
                }
            }
            RtSystem::RtString_ReleaseInline(&ours.name);
            RtSystem::RtString_ReleaseInline(&theirs.name);
        }
    }
    RtSystem::RtString_ReleaseInline(&n0);
    RtSystem::RtString_ReleaseInline(&n1);
}

// ===========================================================================
// ~Persistence -- the row this lane held back on a blocker it never read
// ---------------------------------------------------------------------------
// The holdback said 0x1800b7740 "joins and closes a REAL thread, so handing it
// a fabricated worker is a crash with extra steps". Reading it retired that in
// one objdump: the join is `WaitForSingleObject([this+0x20], INFINITE)` on a
// handle at an offset the CALLER owns, and the four CloseHandle calls close
// four handles the caller owns. Seed four already-signalled events of our own
// and the same scratch route that verified the Begin* applies unchanged.
//
// WHAT IS COMPARED. The whole 0x98-byte image, EXCEPT four ranges that cannot
// position-compare because the two sides legitimately hold different values:
//
//   +0x20/+0x28/+0x30/+0x38  each side owns its OWN four handles, so the raw
//                            values differ. Checked by PREDICATE instead:
//                            unchanged from the seed on both sides (the dtor
//                            reads and closes them, it must not write them).
//   +0x40  pendingFilename   independent String allocations
//   +0x90  savePath          independent String allocations
//
// Both Strings are seeded with refcount 2, so the release takes the SHARING
// arm and both sides end at nullptr -- which means +0x40 and +0x90 DO
// position-compare after all, and the exclusion above is belt-and-braces. Case
// B seeds refcount 1 to drive the FREEING arm, where retail deliberately
// leaves the field holding `buf - 8` rather than null; there the two sides
// hold different dangling pointers and the predicate is `field == ownBuf - 8`.
// Reproducing that asymmetry is the whole point of the second case: a port
// that "cleaned it up" to null would pass case A and fail case B.
//
// The three arrays are seeded NON-NULL with real engine allocations, so the
// three clear helpers do real work (each frees the block, then zeroes the
// pointer and the count) rather than taking their null-safe arm and leaving
// the case inert.
// ===========================================================================

// An engine-allocated block, obtained the only way this file can get one: a
// String allocation's user pointer is `data - 8` (String.h's header layout).
// The array clear helpers MemFree exactly that pointer, so the block must come
// from the engine allocator and must NOT be released again afterwards.
void* EngineBlock(String& keepAlive, const char* filler) {
    keepAlive = MakeString(filler);
    return keepAlive.data ? (void*)(keepAlive.data - 8) : nullptr;
}

struct DtorSeed {
    void* handles[4] = {nullptr, nullptr, nullptr, nullptr};
    String savePath, pending, b0, b1, b2;
};

// Seed one side of a dtor case. Returns false if any resource could not be
// made, so the case is reported as MEASURED NOTHING rather than as a pass.
bool SeedDtor(Scratch& s, DtorSeed& res, int refcount) {
    std::memset(s.bytes, 0, kImage);
    Persistence* p = s.p();

    for (int i = 0; i < 4; ++i) {
        // Manual-reset, created SIGNALLED: the WaitForSingleObject(INFINITE)
        // on handles[0] must return immediately or the probe hangs the game.
        res.handles[i] = CreateEventA(nullptr, 1, 1, nullptr);
        if (!res.handles[i]) return false;
    }
    p->threadHandle = res.handles[0];
    p->workEvent    = res.handles[1];
    p->readyEvent   = res.handles[2];
    p->doneEvent    = res.handles[3];

    res.savePath = MakeString("C:\\PersistProbe\\");
    res.pending  = MakeString("C:\\PersistProbe\\Save\\Save1.sav");
    if (!res.savePath.data || !res.pending.data) return false;
    p->savePath        = res.savePath;
    p->pendingFilename = res.pending;
    // refcount 2 -> the release takes the SHARING arm (field nulled);
    // refcount 1 -> the FREEING arm (field left holding buf - 8).
    StringRefCount(res.savePath.data) = refcount;
    StringRefCount(res.pending.data)  = refcount;

    p->blockDescData = EngineBlock(res.b0, "block-desc-array-filler");
    p->blockPtrData  = EngineBlock(res.b1, "block-ptr-array-filler");
    p->saves         = static_cast<SaveGameInfo*>(EngineBlock(res.b2, "saves-array-filler"));
    if (!p->blockDescData || !p->blockPtrData || !p->saves) return false;
    // Counts the clear helpers will zero. They do not walk the elements on the
    // free path -- 0x18002a930 / 0x180024aa0 / 0x18002ac40 each MemFree the
    // block and zero the pair -- so the contents need not be well-formed.
    p->blockDescCount = 0;
    p->blockPtrCount  = 0;
    p->saveCount      = 0;
    return true;
}

using DtorFn = void (*)(void*);

// CTRL-DTOR: skips the worker destructor entirely, so the three arrays stay
// populated and the four handles stay open. Applicable to every dtor case.
void Mutant_Dtor(void* self) {
    auto* p = static_cast<Persistence*>(self);
    RtSystem::RtString_ReleaseInline(&p->savePath);
    reinterpret_cast<DtorFn>(ResolveAddr(Module::Runtime, 0x18000f7d0))(self);
}

void RunDtorCase(int refcount, bool useMutant) {
    Scratch a, b, preA, preB;
    DtorSeed ra, rb;
    if (!SeedDtor(a, ra, refcount) || !SeedDtor(b, rb, refcount)) {
        fprintf(stderr, "[PersistProbe]   ~Persistence rc=%d -> ABORT, could not seed; "
                        "MEASURED NOTHING\n", refcount);
        return;
    }
    std::memcpy(preA.bytes, a.bytes, kImage);
    std::memcpy(preB.bytes, b.bytes, kImage);

    if (useMutant) Mutant_Dtor(a.p());
    else           RtIo_Persistence_dtor(a.p());
    RtOriginal<DtorFn>("??1Persistence@IO@@QEAA@XZ")(b.p());

    const int changed = BytesChanged(preB, b);

    // (i) position compare, excluding the four handle qwords and the two
    //     String fields, each of which is checked by predicate below.
    int firstDiff = -1;
    for (size_t i = 0; i < kImage; ++i) {
        const bool excluded =
            (i >= offsetof(Persistence, threadHandle) &&
             i <  offsetof(Persistence, threadHandle) + 4 * sizeof(void*)) ||
            (i >= offsetof(Persistence, pendingFilename) &&
             i <  offsetof(Persistence, pendingFilename) + sizeof(void*)) ||
            (i >= offsetof(Persistence, savePath) &&
             i <  offsetof(Persistence, savePath) + sizeof(void*));
        if (excluded) continue;
        if (a.bytes[i] != b.bytes[i]) { firstDiff = (int)i; break; }
    }

    // (ii) the handles must be READ and CLOSED, never written.
    bool handlesIntact = true;
    for (int i = 0; i < 4; ++i) {
        handlesIntact &= (&a.p()->threadHandle)[i] == ra.handles[i];
        handlesIntact &= (&b.p()->threadHandle)[i] == rb.handles[i];
    }

    // (iii) the two String fields took the SAME arm on both sides.
    auto arm = [&](const Persistence* p, const DtorSeed& r) {
        // 0 = nulled (sharing arm), 1 = left at buf-8 (freeing arm), 2 = other
        auto one = [](const char* got, const char* buf) -> int {
            if (got == nullptr)      return 0;
            if (got == buf - 8)      return 1;
            return 2;
        };
        return one(p->savePath.data, r.savePath.data) * 4 +
               one(p->pendingFilename.data, r.pending.data);
    };
    const int armA = arm(a.p(), ra), armB = arm(b.p(), rb);
    const bool armsMatch = (armA == armB);
    const int expectedArm = (refcount == 2) ? 0 : (1 * 4 + 1);

    // (iv) all three arrays cleared, both sides.
    auto cleared = [](const Persistence* p) {
        return p->blockDescData == nullptr && p->blockDescCount == 0 &&
               p->blockPtrData  == nullptr && p->blockPtrCount  == 0 &&
               p->saves         == nullptr && p->saveCount      == 0;
    };
    const bool clearedA = cleared(a.p()), clearedB = cleared(b.p());

    const bool ok = firstDiff < 0 && handlesIntact && armsMatch &&
                    clearedA == clearedB;

    if (useMutant) {
        ++g_dtor.ctrlCases;
        if (!ok) ++g_dtor.ctrlCaught;
        fprintf(stderr, "[PersistProbe]   CTRL-DTOR       rc=%d  -> %s "
                        "(firstDiff=%d handles=%d arms=%d clearedA=%d clearedB=%d)\n",
                refcount, ok ? "NOT CAUGHT" : "caught", firstDiff,
                (int)handlesIntact, (int)armsMatch, (int)clearedA, (int)clearedB);
    } else {
        ++g_dtor.applicable;
        if (changed == 0)   ++g_dtor.inert;
        else if (ok)        ++g_dtor.agreed;
        else              { ++g_dtor.diverged;
            fprintf(stderr, "[PersistProbe]   DIVERGE ~Persistence rc=%d: firstDiff=%d "
                            "handlesIntact=%d armA=%d armB=%d clearedA=%d clearedB=%d\n",
                    refcount, firstDiff, (int)handlesIntact, armA, armB,
                    (int)clearedA, (int)clearedB); }
        fprintf(stderr, "[PersistProbe]   ~Persistence    rc=%d  -> %s "
                        "(shipped side wrote %d of %d bytes; arm=%d expected=%d; "
                        "arrays cleared=%d)\n",
                refcount, changed == 0 ? "INERT" : (ok ? "agree" : "DIVERGE"),
                changed, (int)kImage, armB, expectedArm, (int)clearedB);
        // The arm the SHIPPED side took is reported against what the refcount
        // predicts, so a case that silently stopped exercising the freeing arm
        // announces itself rather than passing quietly.
        if (armB != expectedArm)
            fprintf(stderr, "[PersistProbe]   NOTE ~Persistence rc=%d: the shipped side "
                            "took arm %d, not the predicted %d -- this case is NOT "
                            "exercising the branch it was written for\n",
                    refcount, armB, expectedArm);
    }

    for (int i = 0; i < 4; ++i) { CloseHandle(ra.handles[i]); CloseHandle(rb.handles[i]); }
    // b0/b1/b2 were MemFree'd by the subjects; releasing the String wrappers
    // would be a double free. They are deliberately abandoned -- three small
    // blocks, once per process, under an env-gated probe.
}

void ReportScreen(const Screen& s) {
    fprintf(stderr, "[PersistProbe] %-42s control %d of %d caught | "
                    "%d agreed, %d diverged, %d inert, of %d applicable\n",
            s.name, s.ctrlCaught, s.ctrlCases,
            s.agreed, s.diverged, s.inert, s.applicable);
}

}  // namespace

// A plain global entry point (not namespaced) so the one call site in
// code/Game/Game/camera_input.cpp can declare it in-line without pulling this
// file's headers into the game tree.
extern "C" void RtIo_PersistenceSavesProbeEntry() {
    static bool done = false;
    if (done || !std::getenv("BANISHED_RT_PERSIST_PROBE")) return;
    done = true;

    g_workEvent = CreateEventA(nullptr, 1, 0, nullptr);
    g_doneEvent = CreateEventA(nullptr, 1, 0, nullptr);
    if (!g_workEvent || !g_doneEvent) {
        fprintf(stderr, "[PersistProbe] ABORT: CreateEvent failed; MEASURED NOTHING\n");
        return;
    }

    String savePath = MakeString("C:\\PersistProbe\\");
    fprintf(stderr, "[PersistProbe] ==== synthesised IO::Persistence oracle ====\n");
    fprintf(stderr, "[PersistProbe] savePath=\"%s\"\n", savePath.data);

    // Controls FIRST, so no zero below is trusted before they have printed
    // (WAVE19_IO.md §6.2).
    fprintf(stderr, "[PersistProbe] -- controls --\n");
    RunIssueCase(Issue::Info, 0, "Save1", 2, savePath, Ctrl::WrongCommand);
    RunIssueCase(Issue::Info, 1, "Save1", 2, savePath, Ctrl::IgnoreRefusal);
    RunFilenameSweep(savePath, /*useMutant=*/true);
    RunReaderSweep(savePath, /*useMutant=*/true);
    RunDtorCase(2, /*useMutant=*/true);
    RunDtorCase(1, /*useMutant=*/true);

    fprintf(stderr, "[PersistProbe] -- subjects --\n");
    for (unsigned flags : {0u, 1u}) {
        RunIssueCase(Issue::Info, flags, "Save1", 2, savePath, Ctrl::None);
        RunIssueCase(Issue::Info, flags, "My Town", 0, savePath, Ctrl::None);
        RunIssueCase(Issue::Delete, flags, "Save1", 0, savePath, Ctrl::None);
        RunIssueCase(Issue::Delete, flags, "", 0, savePath, Ctrl::None);
        RunIssueCase(Issue::Load, flags, "Save1", 0, savePath, Ctrl::None);
    }
    RunFilenameSweep(savePath, false);
    RunReaderSweep(savePath, false);
    RunDtorCase(2, false);   // sharing arm: both String fields end nulled
    RunDtorCase(1, false);   // freeing arm: both end holding buf - 8

    fprintf(stderr, "[PersistProbe] ==== results ====\n");
    ReportScreen(g_obj);
    ReportScreen(g_str);
    ReportScreen(g_val);
    ReportScreen(g_int);
    ReportScreen(g_dtor);

    // EVERY screen must carry its own control that reads non-zero. The first
    // version of this predicate omitted g_int, and g_int duly printed
    // "control 0 of 0 caught" beside its two greens without failing the
    // verdict -- a screen with no positive control at all, reported as clean.
    auto green = [](const Screen& sc) {
        return sc.ctrlCases > 0 && sc.ctrlCaught == sc.ctrlCases;
    };
    bool controlsGreen = green(g_obj) && green(g_str) && green(g_val) &&
                     green(g_int) && green(g_dtor);
    int diverged = g_obj.diverged + g_str.diverged + g_val.diverged +
                   g_int.diverged + g_dtor.diverged;
    int inert    = g_obj.inert + g_str.inert + g_val.inert + g_dtor.inert;
    fprintf(stderr, "[PersistProbe] effectiveness: the whole-object screen "
                    "compares %d bytes, of which the subjects write at most %d "
                    "(command, blockCount and the filename pointer). The other "
                    "%d bytes are a SCRIBBLE check, not a behaviour check.\n",
            (int)kImage, g_objBytesWrittenMax, (int)kImage - g_objBytesWrittenMax);
    fprintf(stderr, "[PersistProbe] VERDICT: %s | %d divergences | %d inert cases%s\n",
            !controlsGreen ? "CONTROLS FAILED -- no green is readable"
                           : (diverged ? "DIVERGENCES FOUND" : "clean"),
            diverged, inert,
            inert ? " (those cases MEASURED NOTHING)" : "");
    fflush(stderr);

    RtSystem::RtString_ReleaseInline(&savePath);
    CloseHandle(g_workEvent);
    CloseHandle(g_doneEvent);
    g_workEvent = g_doneEvent = nullptr;
}
