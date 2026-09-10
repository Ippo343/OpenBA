// ============================================================================
// runtime/src/io/file_stream_slots_probe.cpp — Wave 19 lane FILE
// An ORACLE for the ten vtable-slot bodies, because attendance is ZERO
// ============================================================================
//
// Armed with BANISHED_RT_FSSLOTS_PROBE=1 (`=neg1`..`=neg4` for the controls).
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS: A GREEN READINESS SCREEN IS NOT A ROUTE
// ---------------------------------------------------------------------------
// The post-report tranche cleared `??_7FileStream@IO@@6B@` and
// `??_7BinaryFileStream@IO@@6B@` to **8 slots / 8 ported / TRAP=0, READY**, and
// on the same run **every one of the ten bodies read attendance ZERO**.
//
// That is not a contradiction, it is the whole lesson
// (`reference_a_green_readiness_screen_is_not_a_route.md`), and the cause is
// measured rather than guessed: the two tables are MINTED but NOT PROMOTED
// (`[RtMint] promoted 31` of 62 minted rows), so this lane's own cutover board
// reads their name doors `NO`. `RT_DATA_SLOT("??_7FileStream@IO@@6B@")` inside
// our own constructors therefore falls through to the DLL, our objects get
// RETAIL's table, and retail's slot bodies run. Ten registered bodies, nothing
// dispatching them.
//
// Promotion is not this lane's gate to open. So the bodies are verified HERE
// instead, directly, without needing a vtable to dispatch them.
//
// ---------------------------------------------------------------------------
// THE SHAPE: TWO STREAMS, IDENTICAL OPERATIONS, COMPARE THE ARTEFACT
// ---------------------------------------------------------------------------
// Wave 18 lane VRES could not diff `SaveImageToFile` by replay and byte-compared
// the two BMPs instead. Same trick, and it fits this family even better because
// a stream's whole purpose is the file it leaves behind.
//
//   * subject A is driven ENTIRELY through OUR ten bodies;
//   * subject B is driven ENTIRELY through RETAIL's, reached with
//     `ResolveRuntimeRaw` so the port registry cannot answer;
//   * every return value is compared pairwise, AND the two files are
//     byte-compared at the end.
//
// **Both subjects are CONSTRUCTED and CREATED through RETAIL's bodies**
// (`ResolveRuntimeRaw(0x1800a8200)` / `(0x1800aa8f0)`), deliberately: the
// constructor and `Create` are already verified by attendance (2850 and 3 on the
// gameplay route), and using ours here would put two variables in one
// comparison. The ONLY difference between A and B is the slot body under test.
//
// A probe whose ours-side reaches the body BY ADDRESS compares retail with
// itself (`reference_a_probe_whose_ours_side_reaches_the_body_by_address...`),
// so every ours-side call below is a DIRECT C++ call to the named function, and
// every theirs-side call goes through `ResolveRuntimeRaw`. Neither side uses
// `ResolveExport`, which is registry-first and would hand both sides ours.
//
// ---------------------------------------------------------------------------
// WHAT IT CANNOT DO
// ---------------------------------------------------------------------------
// It does NOT exercise the two deleting-destructor thunks' `flags & 2` arm —
// that arm is a declared abort (see file_stream.cpp) and driving it would
// terminate the process. `deletingDtor` is exercised on the scalar path only.
// It also says nothing about whether these bodies are ever DISPATCHED in play;
// that needs promotion, and the attendance counters remain the instrument for it.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileStream.h"
#include "../../types/io/FileSystem.h"
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../system/string.h"

uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);
void*     RtResolveExportForProbe(const char* name);

namespace RtIO {

using RtSystem::String;
using RtSystem::Filename;

// Our bodies, called DIRECTLY.
extern "C" bool     RtIo_FileStream_Write(RtIo::FileStream*, int32_t, const void*);
extern "C" bool     RtIo_FileStream_Read(RtIo::FileStream*, int32_t, void*);
extern "C" bool     RtIo_FileStream_SetCurrentPos(RtIo::FileStream*, int32_t);
extern "C" uint32_t RtIo_FileStream_GetCurrentPos(const RtIo::FileStream*);
extern "C" uint32_t RtIo_FileStream_GetSize(const RtIo::FileStream*);
extern "C" bool     RtIo_FileStream_Open(RtIo::FileStream*, const Filename*);
extern "C" String*  RtIo_FileStream_GetSource(const RtIo::FileStream*, String*);
extern "C" RtIo::BinaryFileStream* RtIo_BinaryFileStream_CopyCtor(
        RtIo::BinaryFileStream*, const RtIo::BinaryFileStream*);
extern "C" void*    RtIo_BinaryFileStream_DeletingDtor(void*, unsigned);

namespace {

using CtorFn    = RtIo::BinaryFileStream* (*)(void*);
using CreateFn  = bool     (*)(void*, const Filename*);
using WriteFn   = bool     (*)(void*, int32_t, const void*);
using ReadFn    = bool     (*)(void*, int32_t, void*);
using SeekFn    = bool     (*)(void*, int32_t);
using GetU32Fn  = uint32_t (*)(const void*);
using OpenFn    = bool     (*)(void*, const Filename*);
using SrcFn     = String*  (*)(const void*, String*);
using CopyFn    = void*    (*)(void*, const void*);
using DelDtorFn = void*    (*)(void*, unsigned);

int      g_ctl = 0;      // 0 = none, 1..4 = a named perturbation
uint64_t g_att = 0;      // how many times a control actually fired
int      g_cases = 0, g_bad = 0;
long     g_bytesCompared = 0;
int      g_compares = 0;   // EFFECTIVENESS: pairwise value comparisons made

void Fail(const char* what, const char* detail) {
    ++g_bad;
    std::printf("[FSSlots] MISMATCH %-22s %s\n", what, detail);
}

void CmpU(const char* what, uint64_t ours, uint64_t theirs) {
    ++g_compares;
    if (ours == theirs) return;
    char m[128];
    std::snprintf(m, sizeof(m), "ours=%llu theirs=%llu",
                  (unsigned long long)ours, (unsigned long long)theirs);
    Fail(what, m);
}

// Build "<saveDir><stem>" as a Filename, the way the VRES probe does — nothing
// here invents a directory.
bool MakePath(const char* stem, Filename* out, char** joinedOut) {
    using GetSaveDirFn  = void* (*)(const void*, void*);
    using StrPlusCStrFn = void* (*)(void*, const void*, const char*);
    using FnCtorStrFn   = void* (*)(void*, const void*);

    void** fsSlot = RT_DATA_SLOT(void**, "?_singleton@FileSystem@IO@@0PEAV12@EA");
    void* fs = fsSlot ? *fsSlot : nullptr;
    if (!fs) return false;

    auto getDir = RT_EXPORT_ONCE(GetSaveDirFn,
        "?GetSaveDirectory@FileSystem@IO@@QEBA?AVFilename@System@@XZ");
    auto plusC  = RT_EXPORT_ONCE(StrPlusCStrFn, "??HSystem@@YA?AVString@0@AEBV10@PEBD@Z");
    auto fnCtor = RT_EXPORT_ONCE(FnCtorStrFn, "??0Filename@System@@QEAA@AEBVString@1@@Z");
    if (!getDir || !plusC || !fnCtor) return false;

    char* dir = nullptr;
    getDir(fs, &dir);
    char* joined = nullptr;
    plusC(&joined, &dir, stem);
    reinterpret_cast<String*>(out)->data = nullptr;
    fnCtor(out, &joined);
    *joinedOut = joined;
    return true;
}

long ReadWhole(const char* path, uint8_t* out, long cap) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    const long n = static_cast<long>(std::fread(out, 1, static_cast<size_t>(cap), f));
    std::fclose(f);
    return n;
}

// ---------------------------------------------------------------------------
// The driven sequence. `ours` selects which family of bodies drives it.
//
// It is the same sequence on both sides, and it is chosen to touch every slot:
// Write twice (so the cursor advances and the second write lands at a SEEKED
// position), SetCurrentPos in between, the two accessors after each write, and
// GetSource at the end. Then the stream is closed and re-Opened and Read back.
// ---------------------------------------------------------------------------
struct Trace {
    bool     write1, write2, seek, open, read;
    uint32_t posAfter1, posAfterSeek, posAfter2, sizeAfter2;
    bool     sourceIsOwn, sourceAddRef, sourceNull;
    uint8_t  readback[32];
};

void Drive(void* s, bool ours, const Filename* path, Trace* t) {
    const char* msg1 = "hello world";     // 11 bytes
    const char* msg2 = "THERE";           //  5 bytes

    auto W = [&](int32_t n, const void* p) -> bool {
        if (ours) return RtIo_FileStream_Write(static_cast<RtIo::FileStream*>(s), n, p);
        return reinterpret_cast<WriteFn>(ResolveRuntimeRaw(0x1800aab90))(s, n, p);
    };
    auto Seek = [&](int32_t n) -> bool {
        if (ours) return RtIo_FileStream_SetCurrentPos(static_cast<RtIo::FileStream*>(s), n);
        return reinterpret_cast<SeekFn>(ResolveRuntimeRaw(0x1800aac10))(s, n);
    };
    auto Pos = [&]() -> uint32_t {
        if (ours) return RtIo_FileStream_GetCurrentPos(static_cast<RtIo::FileStream*>(s));
        return reinterpret_cast<GetU32Fn>(ResolveRuntimeRaw(0x1800aac50))(s);
    };
    auto Size = [&]() -> uint32_t {
        if (ours) return RtIo_FileStream_GetSize(static_cast<RtIo::FileStream*>(s));
        return reinterpret_cast<GetU32Fn>(ResolveRuntimeRaw(0x1800aac60))(s);
    };

    t->write1    = W(11, msg1);
    t->posAfter1 = Pos();

    // CONTROL 1 seeks to the wrong place on OUR side only. It produces a
    // COMPARABLE DIVERGENCE — a different file — never undefined behaviour.
    int32_t seekTo = 6;
    if (ours && g_ctl == 1) { ++g_att; seekTo = 7; }
    t->seek         = Seek(seekTo);
    t->posAfterSeek = Pos();

    t->write2     = W(5, msg2);
    t->posAfter2  = Pos();
    t->sizeAfter2 = Size();

    // GetSource. *** THE TWO SIDES MUST NOT BE COMPARED BY VALUE HERE, and the
    // first version of this probe did exactly that. *** Subject A's filename is
    // "…fsslots_ours.bin" and subject B's is "…fsslots_theirs.bin": they DIFFER
    // BY CONSTRUCTION, so a value comparison reports a mismatch on a correct
    // port, every single run. It did, and the evidence pointed at the probe
    // rather than the body for the same reason Wave 18's case 9 did — the
    // "defect" was exactly the difference the subject was built to have.
    //
    // What is actually common to both sides is the BEHAVIOUR: return this
    // stream's OWN filename buffer, and AddRef it rather than copy it. Both are
    // captured as booleans and those are what get compared.
    char* const ownFilename = static_cast<RtIo::FileStream*>(s)->filename;
    const int32_t refBefore =
        ownFilename ? RtSystem::StringRefCount(ownFilename) : 0;

    String src{}; src.data = nullptr;
    if (ours) RtIo_FileStream_GetSource(static_cast<RtIo::FileStream*>(s), &src);
    else      reinterpret_cast<SrcFn>(ResolveRuntimeRaw(0x1800aab50))(s, &src);

    t->sourceIsOwn  = (src.data == ownFilename);
    t->sourceAddRef = ownFilename &&
                      (RtSystem::StringRefCount(ownFilename) == refBefore + 1);
    t->sourceNull   = (src.data == nullptr);
    if (src.data) RtSystem::RtString_ReleaseInline(&src);

    // Close (scalar deleting destructor, flags=0 so nothing is freed), then
    // re-Open the same path and read it back.
    if (ours) RtIo_BinaryFileStream_DeletingDtor(s, 0);
    else      reinterpret_cast<DelDtorFn>(ResolveRuntimeRaw(0x180023d80))(s, 0);

    reinterpret_cast<CtorFn>(ResolveRuntimeRaw(0x1800a8200))(s);

    if (ours) t->open = RtIo_FileStream_Open(static_cast<RtIo::FileStream*>(s), path);
    else      t->open = reinterpret_cast<OpenFn>(ResolveRuntimeRaw(0x1800aac70))(s, path);

    std::memset(t->readback, 0, sizeof(t->readback));
    if (ours) t->read = RtIo_FileStream_Read(static_cast<RtIo::FileStream*>(s), 16, t->readback);
    else      t->read = reinterpret_cast<ReadFn>(ResolveRuntimeRaw(0x1800aabf0))(s, 16, t->readback);

    if (ours) RtIo_BinaryFileStream_DeletingDtor(s, 0);
    else      reinterpret_cast<DelDtorFn>(ResolveRuntimeRaw(0x180023d80))(s, 0);
}

// ---------------------------------------------------------------------------
// Case 1 — the driven sequence, both sides, values AND the two files compared.
// ---------------------------------------------------------------------------
void CaseDriven() {
    ++g_cases;

    Filename pathO{}, pathT{};
    char *joinedO = nullptr, *joinedT = nullptr;
    if (!MakePath("fsslots_ours.bin", &pathO, &joinedO) ||
        !MakePath("fsslots_theirs.bin", &pathT, &joinedT)) {
        Fail("case1/driven", "could not build the two paths -- PROVES NOTHING");
        return;
    }

    auto rawCtor   = reinterpret_cast<CtorFn>(ResolveRuntimeRaw(0x1800a8200));
    auto rawCreate = reinterpret_cast<CreateFn>(ResolveRuntimeRaw(0x1800aa8f0));
    if (!rawCtor || !rawCreate) {
        Fail("case1/driven", "shipped ctor/Create did not resolve -- PROVES NOTHING");
        return;
    }

    // 0x50 bytes each, and NOT zero-initialised: the constructor writes every
    // field either side reads, and +0x24 is one neither writes.
    alignas(8) uint8_t bufO[0x50], bufT[0x50];
    rawCtor(bufO); rawCtor(bufT);

    const bool createdO = rawCreate(bufO, &pathO);
    const bool createdT = rawCreate(bufT, &pathT);
    if (!createdO || !createdT) {
        Fail("case1/driven", "shipped Create failed on one side -- PROVES NOTHING");
        return;
    }

    Trace to{}, tt{};
    Drive(bufO, true,  &pathO, &to);
    Drive(bufT, false, &pathT, &tt);

    CmpU("Write#1",        to.write1,      tt.write1);
    CmpU("GetCurrentPos#1",to.posAfter1,   tt.posAfter1);
    CmpU("SetCurrentPos",  to.seek,        tt.seek);
    CmpU("GetCurrentPos#2",to.posAfterSeek,tt.posAfterSeek);
    CmpU("Write#2",        to.write2,      tt.write2);
    CmpU("GetCurrentPos#3",to.posAfter2,   tt.posAfter2);
    CmpU("GetSize",        to.sizeAfter2,  tt.sizeAfter2);
    CmpU("Open",           to.open,        tt.open);
    CmpU("Read",           to.read,        tt.read);

    // GetSource, compared by BEHAVIOUR not by value -- see the note in Drive().
    CmpU("GetSource own",   to.sourceIsOwn,  tt.sourceIsOwn);
    CmpU("GetSource addref",to.sourceAddRef, tt.sourceAddRef);
    CmpU("GetSource null",  to.sourceNull,   tt.sourceNull);
    // ... and an ABSOLUTE assertion, because two sides that both returned the
    // WRONG thing in the same way would agree. A stream with a filename must
    // hand back that buffer, AddRef'd.
    ++g_compares;
    if (!to.sourceIsOwn || !to.sourceAddRef || to.sourceNull)
        Fail("GetSource absolute", "ours did not return its own filename AddRef'd");

    ++g_compares;
    if (std::memcmp(to.readback, tt.readback, sizeof(to.readback)) != 0)
        Fail("Read buffer", "the 16 bytes read back differ");

    // The artefact: byte-compare the two files the sequences produced.
    if (joinedO && joinedT) {
        static uint8_t fO[4096], fT[4096];
        const long nO = ReadWhole(joinedO, fO, sizeof(fO));
        const long nT = ReadWhole(joinedT, fT, sizeof(fT));
        if (nO < 0 || nT < 0) {
            Fail("file readback", "one of the two files could not be read "
                                  "-- this case PROVES NOTHING about the artefact");
        } else if (nO == 0 && nT == 0) {
            Fail("file readback", "BOTH files are EMPTY -- they compare equal "
                                  "and prove nothing");
        } else if (nO != nT) {
            char m[96];
            std::snprintf(m, sizeof(m), "file SIZE ours=%ld theirs=%ld", nO, nT);
            Fail("file bytes", m);
        } else {
            for (long b = 0; b < nO; ++b) {
                if (fO[b] != fT[b]) {
                    char m[96];
                    std::snprintf(m, sizeof(m), "file byte +0x%lx of %ld: %02x vs %02x",
                                  b, nO, fO[b], fT[b]);
                    Fail("file bytes", m);
                    break;
                }
            }
            g_bytesCompared += nO;
        }
        std::remove(joinedO);
        std::remove(joinedT);
    }
}

// ---------------------------------------------------------------------------
// Case 2 — the COPY CONSTRUCTOR, field by field.
//
// It is the second stamper of ??_7BinaryFileStream@ and has no caller in either
// binary, so this is its only exercise. All 0x50 bytes are compared EXCEPT
// +0x24 (`FileInfo::_unk04`), which NEITHER constructor writes: comparing bytes
// neither side writes fails 100% of calls and reads like a catastrophic port
// defect (`reference_oracle_must_not_compare_unwritten_bytes.md`).
// ---------------------------------------------------------------------------
void CaseCopyCtor() {
    ++g_cases;

    auto rawCtor = reinterpret_cast<CtorFn>(ResolveRuntimeRaw(0x1800a8200));
    auto rawCopy = reinterpret_cast<CopyFn>(ResolveRuntimeRaw(0x180023ca0));
    if (!rawCtor || !rawCopy) {
        Fail("case2/copyCtor", "shipped bodies did not resolve -- PROVES NOTHING");
        return;
    }

    alignas(8) uint8_t src[0x50], dstO[0x50], dstT[0x50];
    rawCtor(src);

    // Give the source a filename so the refcounted branch is exercised rather
    // than the null one — otherwise the interesting half never runs.
    Filename p{}; char* joined = nullptr;
    if (MakePath("fsslots_copy.bin", &p, &joined)) {
        auto rawCreate = reinterpret_cast<CreateFn>(ResolveRuntimeRaw(0x1800aa8f0));
        if (rawCreate) rawCreate(src, &p);
    }

    std::memset(dstO, 0xCD, sizeof(dstO));
    std::memset(dstT, 0xCD, sizeof(dstT));

    RtIo_BinaryFileStream_CopyCtor(reinterpret_cast<RtIo::BinaryFileStream*>(dstO),
                                   reinterpret_cast<const RtIo::BinaryFileStream*>(src));
    if (g_ctl == 2) {   // CONTROL 2: skip the third vtable store on our side
        ++g_att;
        *reinterpret_cast<void**>(dstO) = nullptr;
    }
    rawCopy(dstT, src);

    for (size_t b = 0; b < 0x50; ++b) {
        if (b >= 0x24 && b < 0x28) continue;   // FileInfo::_unk04 — never written
        ++g_compares;
        if (dstO[b] != dstT[b]) {
            char m[96];
            std::snprintf(m, sizeof(m), "byte +0x%02zx ours=%02x theirs=%02x",
                          b, dstO[b], dstT[b]);
            Fail("copyCtor bytes", m);
            break;
        }
    }

    if (joined) std::remove(joined);
}

}  // namespace

void RunFileStreamSlotsProbe() {
    const char* e = std::getenv("BANISHED_RT_FSSLOTS_PROBE");
    if (!e || !*e || std::strcmp(e, "0") == 0) return;
    g_ctl = 0;
    if (std::strncmp(e, "neg", 3) == 0) g_ctl = std::atoi(e + 3);

    g_cases = g_bad = g_compares = 0; g_bytesCompared = 0; g_att = 0;

    std::printf("[FSSlots] ==== ten slot bodies, ours vs the shipped ones ====\n");
    std::printf("[FSSlots] subject: two BinaryFileStreams built by RETAIL's ctor and "
                "Create, driven through identical operations, one side entirely "
                "through OUR bodies and one entirely through the shipped ones "
                "(ResolveRuntimeRaw). ctl=%d\n", g_ctl);

    CaseDriven();
    CaseCopyCtor();

    // EFFECTIVENESS GUARD. A run that compared nothing reads exactly like a run
    // that found no difference (WAVE19_IO.md §6.6), so say which this was.
    std::printf("[FSSlots] cases=%d mismatches=%d comparisons=%d file-bytes-compared=%ld "
                "controlFired=%llu\n",
                g_cases, g_bad, g_compares, g_bytesCompared,
                (unsigned long long)g_att);
    // ORDER MATTERS HERE AND THE FIRST VERSION HAD IT WRONG. The effectiveness
    // guard used to run before the control verdict, so a control run that was
    // CAUGHT — and caught so decisively that the two files ended up different
    // sizes and the byte comparison was skipped — printed "THIS RUN PROVES
    // NOTHING" and nothing else. That reads as a failed control when it is the
    // opposite. A fired-and-caught control is reported first, and the
    // effectiveness note follows as a qualifier rather than a verdict.
    if (g_ctl != 0 && g_att > 0 && g_bad > 0) {
        std::printf("[FSSlots] control %d CAUGHT (%d mismatch(es)) -- the screen can "
                    "see a wrong body.%s\n", g_ctl, g_bad,
                    g_bytesCompared == 0
                        ? "  (It diverged enough that the two files differed in SIZE, "
                          "so the byte comparison was skipped -- caught by the value "
                          "comparisons alone.)"
                        : "");
    } else if (g_compares == 0 || g_bytesCompared == 0) {
        std::printf("[FSSlots] *** THIS RUN PROVES NOTHING: %s. ***\n",
                    g_compares == 0 ? "no value comparison was made"
                                    : "no file bytes were compared");
    } else if (g_ctl != 0 && g_att == 0) {
        std::printf("[FSSlots] *** CONTROL %d NEVER FIRED -- it is not a control on "
                    "this run and a clean board below means nothing. ***\n", g_ctl);
    } else if (g_bad == 0 && g_ctl == 0) {
        std::printf("[FSSlots] VERDICT: ours matches the shipped bodies on every "
                    "comparison above.\n");
    } else if (g_bad == 0 && g_ctl != 0) {
        std::printf("[FSSlots] *** CONTROL %d FIRED AND WAS NOT CAUGHT -- the screen "
                    "is blind to it. ***\n", g_ctl);
    } else if (g_ctl != 0) {
        std::printf("[FSSlots] control %d CAUGHT (%d mismatch(es)) -- the screen can "
                    "see a wrong body.\n", g_ctl, g_bad);
    }
}

}  // namespace RtIO
