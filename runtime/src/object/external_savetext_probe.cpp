// ============================================================================
// runtime/src/object/external_savetext_probe.cpp — Wave 11 follow-up 2, lane IOSTREAM
//   The oracle for `Object::External::SaveText` (retail 0x1800bb9d0).
// ============================================================================
//
// NO GAMEPLAY ROUTE REACHES SaveText. It is an authoring entry point: nothing
// in Application-x64.exe imports it and nothing in the DLL calls it, so a live
// run reports `calls=0` for exactly the reason
// reference_uncalled_port_is_not_verified.md warns about — and "no route" is
// NOT "no oracle" (reference_no_route_is_not_no_oracle.md).
//
// ---------------------------------------------------------------------------
// WHAT THE FIRST TWO RUNS OF THIS PROBE ESTABLISHED, AND WHY THE SUBJECT IS
// SYNTHETIC
// ---------------------------------------------------------------------------
// The obvious oracle is a byte comparison: ask ours and theirs each to write
// the SAME live `External` to a DIFFERENT file and compare the results
// (reference_write_path_serialize_is_replayable.md). That was written first and
// it FAULTED — inside `Runtime-x64.dll`, at
// `?Serialize@StringTable@UI@@UEAA_NAEAVStream@IO@@@Z + 0x53`
// (0x18011d993), on `MOV RAX,[RCX]` with RCX = 0.
//
// A crash in a run that just registered a new port reads as a port defect. It
// is not one, and the way that was settled was to REORDER the probe so THEIRS
// runs first. The second run faulted at the identical address, from the shipped
// export, with our body never entered — the log stops at
// `subject 1: THEIRS...` and never reaches the line after it.
//
// THE UPSTREAM NULL, NAMED (the rule is find it, not guard it). Every
// `External::Serialize` starts by asking the stream's `IO::Encoding` for a
// `Serializer` — `encoding->vtbl[+0x18]` when reading, `[+0x20]` when writing.
// In the SHIPPED Runtime-x64.dll the TEXT encoding's two factory slots are
// stubs:
//
//     ??_7 text encoding  0x1801559d0   (installed by ??0TextFileStream, 1800ba431)
//         +0x18 -> 0x1800b9a80   XOR EAX,EAX ; RET
//         +0x20 -> 0x1800b9a70   XOR EAX,EAX ; RET
//
// while the BINARY encoding used by every stream the game actually loads is
// real:
//
//     ??_7 binary encoding 0x1801554a8  (installed by ??0CompressedStream, 1800aa475)
//         +0x18 -> 0x1800a8120   allocates through the MemoryAllocator singleton
//         +0x20 -> 0x1800a8080   likewise
//
// So the text serializer classes are STRIPPED FROM THE RETAIL BUILD — text I/O
// is a Tools-x64.exe path — and `External::SaveText` cannot complete against
// any real object in this DLL, no matter who calls it. That is a fact about
// retail, established by running retail, and it is why `calls=0` is the only
// thing a live run could ever have reported for this row.
//
// ---------------------------------------------------------------------------
// THE SYNTHETIC SUBJECT, AND WHAT IT MAKES CHECKABLE
// ---------------------------------------------------------------------------
// SaveText touches exactly two of its argument's virtuals: `+0x10 GetTypeId`
// and `+0x28 Serialize`. So the subject is an object with a hand-built vtable
// whose `Serialize` is OURS: it records what the stream looked like when it was
// handed over and writes a fixed payload through the stream's OWN `Write`
// virtual (`?Write@TextFileStream@IO@@`, 0x1800ba9f0, a real retail body). Its
// `GetTypeId` returns a REAL registered type id lifted from a live `External`,
// so the Factory lookup is a real hit and the type NAME really is written.
//
// That is reference_synthetic_subject_beats_a_fake.md's shape: the fake part is
// only the one virtual that the shipped DLL can no longer service, and
// everything the function under test actually does — the AVL lookup, the
// String, the constructor, `FileStream::Create`, the byte-order-mark branch,
// the file write, the three-level destructor chain — runs for real on BOTH
// sides.
//
// Three screens, each reported separately:
//   A  the two return values agree;
//   B  the STREAM STATE each side handed to Serialize agrees, field by field
//      (objectType and objectName as TEXT, objectVersion, direction, the
//      Encoding's vtable pointer, requestedFormat, activeEncoding, and whether
//      the file was open);
//   C  the two files agree BYTE FOR BYTE.
//
// POSITIVE CONTROL: `bytes=`, the number of bytes actually compared in screen
// C, and `serCalls=`, the number of times each side reached Serialize. Two
// empty files compare equal, so a cell with `bytes=0` is declared VACUOUS
// rather than green.
//
// ---------------------------------------------------------------------------
// NEGATIVE CONTROLS — `BANISHED_RT_SAVETEXT_NEGCTL=1..3`
// ---------------------------------------------------------------------------
// The perturbations live in the BODY (external_savetext_win.cpp), not here, so
// each is consumed by the single implementation under test rather than by a
// copy of it. Each attends at its own site (`att=`) and each produces a
// comparable divergence, never UB. Scored separately.
//
//   1  the ctor's `type` and `name` arguments swapped   -> screen B, and C
//   2  objectVersion 1 instead of retail's 0            -> screen B
//   3  _activeEncoding forced to 3 (the UTF-8 BOM)      -> screen C, +3 bytes
//
// CONTROL 3 IS ALSO THE ONLY THING THAT EVER EXECUTES THE BOM TRANSCRIPTION.
// No caller in either shipped binary sets `_activeEncoding` to anything but 2,
// so without it those lines would be verified by reading them.
//
// ---------------------------------------------------------------------------
// `BANISHED_RT_SAVETEXT_REAL=1` re-runs the ORIGINAL real-object attempt, so
// the retail fault above is reproducible rather than merely reported. It ends
// the process in the DLL; that is the point.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../system/string.h"
#include "../../funcs/rt_port.h"
#include "../../types/object/TypeInfo.h"
#include "../../types/object/External.h"
#include "../../types/io/FileStream.h"

namespace RtObject {

// Set by this probe, read by the body. Defined in external_savetext_win.cpp.
extern int      g_stCtl;
extern uint64_t g_stAtt;

extern "C" bool RtObject_External_SaveText(External*, const RtSystem::Filename*,
                                           const RtSystem::Filename*);

namespace {

using RtSystem::Filename;
using RtSystem::String;

using SaveTextFn = bool (*)(External*, const Filename*, const Filename*);

const char* const kSaveText =
    "?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z";

const char* const kOursPath   = "savetext_probe_ours.txt";
const char* const kTheirsPath = "savetext_probe_theirs.txt";

// The payload the synthetic Serialize writes through the stream's own Write.
// Chosen long enough that a length mismatch cannot hide inside a BOM.
const char kPayload[] = "IOSTREAM-SAVETEXT-PROBE-PAYLOAD-0123456789";

// ---------------------------------------------------------------------------
// What the synthetic Serialize saw. One of these per side.
// ---------------------------------------------------------------------------
struct Observed {
    int      serCalls;
    char     objectType[128];
    char     objectName[128];
    int32_t  objectVersion;
    int32_t  direction;
    const void* encodingVptr;
    int32_t  requestedFormat;
    int32_t  activeEncoding;
    int32_t  fileOpen;       // file.handle != -1
    int32_t  writeOk;
};

Observed  g_obs[2];   // [0] = theirs, [1] = ours
int       g_side = 0;

void CopyStr(char* dst, size_t cap, const char* src) {
    if (!src) { std::snprintf(dst, cap, "<null>"); return; }
    std::snprintf(dst, cap, "%s", src);
}

// ---------------------------------------------------------------------------
// The synthetic subject's two virtuals.
// ---------------------------------------------------------------------------
uint32_t g_fakeTypeId = 0;

uint32_t FakeGetTypeId(void*) { return g_fakeTypeId; }

bool FakeSerialize(void* /*self*/, void* stream) {
    Observed& o = g_obs[g_side];
    ++o.serCalls;

    auto* ts = static_cast<RtIo::TextFileStream*>(stream);
    CopyStr(o.objectType, sizeof(o.objectType), ts->objectType);
    CopyStr(o.objectName, sizeof(o.objectName), ts->objectName);
    o.objectVersion   = ts->objectVersion;
    o.direction       = ts->base.base.direction;
    o.encodingVptr    = ts->base.base.encoding ? *static_cast<void**>(ts->base.base.encoding)
                                               : nullptr;
    o.requestedFormat = ts->requestedFormat;
    o.activeEncoding  = ts->activeEncoding;
    o.fileOpen        = (ts->base.file.handle != -1) ? 1 : 0;

    // Through the stream's OWN Write virtual (+0x10), which on a TextFileStream
    // is retail's 0x1800ba9f0. The payload therefore reaches the file the same
    // way a real Serialize's bytes would.
    void** vtbl = *reinterpret_cast<void***>(stream);
    using WriteFn = bool (*)(void*, int32_t, const void*);
    o.writeOk = reinterpret_cast<WriteFn>(vtbl[0x10 / sizeof(void*)])(
                    stream, static_cast<int32_t>(sizeof(kPayload) - 1), kPayload) ? 1 : 0;
    return true;
}

// Eight slots is more than SaveText reads (it touches +0x10 and +0x28); the
// tail is filled with a trap so a dispatch this probe did not predict dies
// loudly instead of running into whatever follows in .rdata.
void FakeTrap() {
    std::printf("[SaveTextProbe] FATAL: the subject's vtable was dispatched at a "
                "slot SaveText is not supposed to touch\n");
    std::abort();
}

void*  g_fakeVtbl[8];
struct FakeExternal { void** vptr; uint8_t tail[0x20]; };
FakeExternal g_fake;

void BuildFakeSubject(uint32_t typeId) {
    for (int i = 0; i < 8; ++i) g_fakeVtbl[i] = reinterpret_cast<void*>(&FakeTrap);
    g_fakeVtbl[0x10 / sizeof(void*)] = reinterpret_cast<void*>(&FakeGetTypeId);
    g_fakeVtbl[0x28 / sizeof(void*)] = reinterpret_cast<void*>(&FakeSerialize);
    std::memset(&g_fake, 0, sizeof(g_fake));
    g_fake.vptr  = g_fakeVtbl;
    g_fakeTypeId = typeId;
}

// ---------------------------------------------------------------------------
int      g_checked  = 0;
int      g_failed   = 0;
uint64_t g_bytes    = 0;
bool     g_done     = false;

void Check(bool ok, const char* what, const char* got, const char* want) {
    ++g_checked;
    if (!ok) {
        ++g_failed;
        std::printf("[SaveTextProbe]   FAIL %-34s ours=%s theirs=%s\n", what, got, want);
    }
}
void CheckI(bool ok, const char* what, long long got, long long want) {
    ++g_checked;
    if (!ok) {
        ++g_failed;
        std::printf("[SaveTextProbe]   FAIL %-34s ours=%lld theirs=%lld\n", what, got, want);
    }
}

long ReadAll(const char* path, unsigned char* buf, long cap) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    const long n = static_cast<long>(std::fread(buf, 1, static_cast<size_t>(cap), f));
    std::fclose(f);
    return n;
}

}  // namespace

// ============================================================================
// Driven from ExternalFactory::Add — which is only where the LIVE type id comes
// from. Everything else is this file's own.
// ============================================================================
void RtObjectSaveTextProbeStep(External* liveSubject) {
    if (g_done || liveSubject == nullptr) return;
    g_done = true;

    const char* e = std::getenv("BANISHED_RT_SAVETEXT_NEGCTL");
    g_stCtl = (e && e[0]) ? std::atoi(e) : 0;

    // The one thing borrowed from the live object: a type id that is really in
    // Factory's tree, so the lookup under test is a HIT and the name it copies
    // out is a real BaseTypeInfo::name.
    void** liveVtbl = *reinterpret_cast<void***>(liveSubject);
    using GetIdFn = uint32_t (*)(void*);
    const uint32_t typeId =
        reinterpret_cast<GetIdFn>(liveVtbl[0x10 / sizeof(void*)])(liveSubject);

    auto theirs = reinterpret_cast<SaveTextFn>(ResolveExportRaw(kSaveText));
    if (theirs == nullptr) {
        // A name the DLL does not contain resolves NULL SILENTLY
        // (reference_resolveexport_phantom_names.md).
        std::printf("[SaveTextProbe] FATAL phantom export name: %s\n", kSaveText);
        return;
    }

    std::printf("[SaveTextProbe] ctl=%d typeId=0x%08x -- ours vs the shipped DLL\n",
                g_stCtl, typeId);

    // ---- the REAL-object reproduction of the retail fault ------------------
    if (const char* r = std::getenv("BANISHED_RT_SAVETEXT_REAL")) {
        if (r[0] && std::strcmp(r, "0") != 0) {
            Filename p{}; p.data = nullptr;
            Filename n{}; n.data = nullptr;
            RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&p), kTheirsPath);
            RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&n), "savetext_probe");
            std::printf("[SaveTextProbe] REAL-object mode: calling the SHIPPED export on a "
                        "live External. It is expected to fault in the DLL at "
                        "?Serialize@StringTable@UI@@+0x53 -- see this file's header.\n");
            std::fflush(stdout);
            (void)theirs(liveSubject, &p, &n);
            std::printf("[SaveTextProbe] REAL-object mode: it RETURNED. That contradicts "
                        "this file's header and must be investigated.\n");
            std::fflush(stdout);
            return;
        }
    }

    BuildFakeSubject(typeId);
    std::memset(g_obs, 0, sizeof(g_obs));

    Filename pathOurs{};   pathOurs.data   = nullptr;
    Filename pathTheirs{}; pathTheirs.data = nullptr;
    Filename objName{};    objName.data    = nullptr;
    RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&pathOurs),   kOursPath);
    RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&pathTheirs), kTheirsPath);
    RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&objName),    "savetext_probe");

    std::remove(kOursPath);
    std::remove(kTheirsPath);

    // THEIRS FIRST, deliberately: if this half ever faults again, the log line
    // above it says which side we were in before anything of ours had run.
    g_side = 0;
    std::printf("[SaveTextProbe]   THEIRS...\n"); std::fflush(stdout);
    const bool rTheirs = theirs(reinterpret_cast<External*>(&g_fake), &pathTheirs, &objName);

    g_side = 1;
    std::printf("[SaveTextProbe]   OURS...\n"); std::fflush(stdout);
    const bool rOurs = RtObject_External_SaveText(
        reinterpret_cast<External*>(&g_fake), &pathOurs, &objName);

    const Observed& T = g_obs[0];
    const Observed& O = g_obs[1];

    // ---- SCREEN A ----------------------------------------------------------
    CheckI(rOurs == rTheirs, "return value", (int)rOurs, (int)rTheirs);

    // ---- SCREEN B ----------------------------------------------------------
    CheckI(O.serCalls == T.serCalls, "Serialize entered", O.serCalls, T.serCalls);
    Check(std::strcmp(O.objectType, T.objectType) == 0, "objectType (+0x60)",
          O.objectType, T.objectType);
    Check(std::strcmp(O.objectName, T.objectName) == 0, "objectName (+0x58)",
          O.objectName, T.objectName);
    CheckI(O.objectVersion   == T.objectVersion,   "objectVersion (+0x50)",
           O.objectVersion, T.objectVersion);
    CheckI(O.direction       == T.direction,       "direction (+0x08)",
           O.direction, T.direction);
    CheckI(O.encodingVptr    == T.encodingVptr,    "encoding vtable (+0x10)",
           (long long)(uintptr_t)O.encodingVptr, (long long)(uintptr_t)T.encodingVptr);
    CheckI(O.requestedFormat == T.requestedFormat, "requestedFormat (+0x68)",
           O.requestedFormat, T.requestedFormat);
    CheckI(O.activeEncoding  == T.activeEncoding,  "activeEncoding (+0x6c)",
           O.activeEncoding, T.activeEncoding);
    CheckI(O.fileOpen        == T.fileOpen,        "file.handle open (+0x20)",
           O.fileOpen, T.fileOpen);
    CheckI(O.writeOk         == T.writeOk,         "Stream::Write returned",
           O.writeOk, T.writeOk);

    // The strong form of screen B: the type name must be the one the Factory
    // really holds for this id, not merely the same on both sides. Two sides
    // that agree on the WRONG field would still pass the comparison above.
    {
        const Factory* f = *RT_DATA_SLOT(Factory**, "?_singleton@Factory@Object@@0PEAV12@EA");
        const TypeNode* n = f->root;
        while (n) {
            if (typeId < n->key)      n = n->left;
            else if (n->key < typeId) n = n->right;
            else break;
        }
        const char* expect = n ? n->value.info->name : nullptr;
        const bool swapped = (g_stCtl == 1);
        const char* mine = swapped ? T.objectType : O.objectType;
        Check(expect && std::strcmp(mine, expect) == 0,
              "objectType IS BaseTypeInfo::name", mine, expect ? expect : "<null>");
    }

    // ---- SCREEN C ----------------------------------------------------------
    static unsigned char bufO[1 << 16];
    static unsigned char bufT[1 << 16];
    const long nO = ReadAll(kOursPath,   bufO, sizeof(bufO));
    const long nT = ReadAll(kTheirsPath, bufT, sizeof(bufT));

    if (nO != nT) {
        ++g_checked; ++g_failed;
        std::printf("[SaveTextProbe]   FAIL file SIZE ours=%ld theirs=%ld\n", nO, nT);
        // NAME THE DIFFERENCE, do not just count it. A control that catches has
        // to be shown to catch VIA THE MECHANISM IT NAMES, and for control 3
        // that means the extra bytes are the UTF-8 byte-order mark and not
        // three bytes of anything else. Printing the head of each file is what
        // turns "it grew by 3" into evidence about the transcription.
        std::printf("[SaveTextProbe]        ours head:");
        for (long i = 0; i < nO && i < 6; ++i) std::printf(" %02x", bufO[i]);
        std::printf("\n[SaveTextProbe]        theirs head:");
        for (long i = 0; i < nT && i < 6; ++i) std::printf(" %02x", bufT[i]);
        std::printf("\n");
    } else if (nO > 0) {
        ++g_checked;
        g_bytes += static_cast<uint64_t>(nO);
        if (std::memcmp(bufO, bufT, static_cast<size_t>(nO)) != 0) {
            ++g_failed;
            long at = 0;
            while (at < nO && bufO[at] == bufT[at]) ++at;
            std::printf("[SaveTextProbe]   FAIL file BYTES differ at %ld of %ld "
                        "(ours=0x%02x theirs=0x%02x)\n",
                        at, nO, bufO[at], bufT[at]);
        }
    }

    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&pathOurs));
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&pathTheirs));
    RtSystem::RtString_ReleaseInline(reinterpret_cast<String*>(&objName));
    std::remove(kOursPath);
    std::remove(kTheirsPath);

    // ---- verdict -----------------------------------------------------------
    // THE VACUITY RULE APPLIES ONLY TO A RUN THAT FOUND NOTHING. Its first
    // spelling put this branch ahead of the control branches, and control 3 --
    // which catches by making the two files DIFFERENT SIZES, so no byte
    // comparison happens -- was reported as VACUOUS while its own two failures
    // sat two lines above. A verdict line that contradicts the failures printed
    // above it is worse than no verdict line.
    if (g_failed == 0 && (g_bytes == 0 || O.serCalls == 0 || T.serCalls == 0)) {
        std::printf("[SaveTextProbe] VERDICT ctl=%d checked=%d failed=%d bytes=%llu "
                    "serCalls=%d/%d -- VACUOUS: %s\n",
                    g_stCtl, g_checked, g_failed, (unsigned long long)g_bytes,
                    O.serCalls, T.serCalls,
                    (O.serCalls == 0 || T.serCalls == 0)
                        ? "a side never reached Serialize"
                        : "no file bytes were compared");
    } else if (g_stCtl == 0) {
        std::printf("[SaveTextProbe] VERDICT ctl=0 checked=%d failed=%d bytes=%llu "
                    "serCalls=%d/%d  %s\n",
                    g_checked, g_failed, (unsigned long long)g_bytes,
                    O.serCalls, T.serCalls, g_failed == 0 ? "PASS" : "*** MISMATCH ***");
    } else if (g_failed == 0) {
        std::printf("[SaveTextProbe] VERDICT ctl=%d checked=%d failed=0 att=%llu  "
                    "control NOT CAUGHT\n", g_stCtl, g_checked,
                    (unsigned long long)g_stAtt);
    } else if (g_stAtt == 0) {
        std::printf("[SaveTextProbe] VERDICT ctl=%d checked=%d failed=%d att=0  "
                    "CAUGHT BUT NOT VIA ITS OWN MECHANISM -- NOT BANKED\n",
                    g_stCtl, g_checked, g_failed);
    } else {
        std::printf("[SaveTextProbe] VERDICT ctl=%d checked=%d failed=%d att=%llu  "
                    "control CAUGHT\n", g_stCtl, g_checked, g_failed,
                    (unsigned long long)g_stAtt);
    }
    std::fflush(stdout);
    g_stCtl = 0;   // leave the body unperturbed for the rest of the run
}

}  // namespace RtObject

// Flat entry point for external_add.cpp's arm site, the same shape
// RtIoProbeRunFileStreamLayoutProbe uses.
extern "C" void RtObjectSaveTextProbeStepC(void* subject) {
    RtObject::RtObjectSaveTextProbeStep(static_cast<RtObject::External*>(subject));
}
