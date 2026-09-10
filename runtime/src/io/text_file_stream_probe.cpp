// ============================================================================
// runtime/src/io/text_file_stream_probe.cpp — Wave 20, lane SAVETEXT
//   The oracle for the five `??_7TextFileStream@IO@@6B@` slots ported in
//   text_file_stream_win.cpp.
// ============================================================================
//
// NO GAMEPLAY ROUTE REACHES ANY OF THEM, and this file exists because that is
// not the same as "no oracle" (reference_no_route_is_not_no_oracle.md, refuted
// four times in Wave 18 and again in 19).
//
// WHY THERE IS NO ROUTE, stated as a checkable premise rather than as a mood:
// the retail TEXT encoding's two serializer factories are stubs —
//
//     0x1801559d0 +0x18 -> 0x1800b9a80    33 C0 C3   XOR EAX,EAX ; RET
//     0x1801559d0 +0x20 -> 0x1800b9a70    33 C0 C3   XOR EAX,EAX ; RET
//
// while the BINARY encoding every real stream uses (0x1801554a8) has real
// bodies at the same two slots (0x1800a8120 / 0x1800a8080, both
// `40 57 48 83 EC 30`). Text serialization is a Tools-x64.exe path that was
// stripped from the shipped DLL. Re-verified from the shipped bytes at the top
// of this lane; the byte contrast against the binary encoding is the POSITIVE
// CONTROL that the reader can tell a stub from a body at all.
//
// EXPIRY: that premise is about THIS DLL. It is falsified the moment those two
// qwords point at anything but a 3-byte stub, and anyone re-checking it should
// read the bytes rather than this comment.
//
// ---------------------------------------------------------------------------
// THE SUBJECTS ARE SYNTHETIC, AND TWO DIFFERENT KINDS OF SYNTHETIC
// ---------------------------------------------------------------------------
// Screens A, B and C need only the FIELDS the bodies read (+0x20 file.handle,
// +0x28 size, +0x2c cursor, +0x38 winHandle, +0x70 buffer, +0x78 bufSize,
// +0x7c bufCursor). So their subject is a zeroed 0x80-byte block with those
// fields set by hand — NO constructor, NO destructor, NO file. That is not a
// shortcut: it lets each screen put the object into states a constructed stream
// cannot easily be driven into (a negative cursor, a cursor exactly at EOF, a
// packed handle) and it keeps the constructor out of the subject of a screen
// that is not testing the constructor.
//
// Screen D tests `Create`, which really opens a file, so its subject is a REAL
// `TextFileStream` built by the SHIPPED constructor — the same one on both
// sides, deliberately, because the constructor is not what is under test and
// building the two sides differently would put a difference into the subject.
//
// ---------------------------------------------------------------------------
// FOUR SCREENS, EACH REPORTED WITH ITS OWN DENOMINATOR
// ---------------------------------------------------------------------------
//   A  GetSize and GetCurrentPos, ours vs the shipped export, over BOTH arms of
//      the `buffer` branch and several values in each.
//   B  SetCurrentPos, ours vs theirs, on two PARALLEL objects — return value
//      AND post-state (both cursors), over the buffer arm (in range, at EOF, past
//      EOF, negative) and the packed-handle file arm.
//   C  Read, ours vs theirs, on two parallel objects — the destination BYTES and
//      the post-state cursor. The two are scored separately because control 3
//      moves only the second.
//   D  Create, ours vs theirs, writing two real files per encoding value, over
//      {0, 2, 3, 4, 5, 7} — the whole byte-order-mark table plus one value
//      OUTSIDE it — with `_requestedFormat` set to a DIFFERENT value each time
//      so that a body reading the wrong one of the pair is visible. The files
//      are compared BYTE FOR BYTE.
//
// POSITIVE CONTROLS. `checked=` must be non-zero for every screen or the screen
// is VACUOUS, not green; screen D additionally reports `bytes=`, since two empty
// files compare equal. `att=` reports how many times a negative control was
// actually consumed — a control with att=0 tested nothing.
//
// NEGATIVE CONTROLS — `BANISHED_RT_TFS_NEGCTL=1..4`. The perturbations live in
// the BODIES (text_file_stream_win.cpp), not here, so each is consumed by the
// single implementation under test rather than by a copy of it. Each is scored
// separately and each names the screen it is supposed to trip.
// ============================================================================

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

namespace RtIO {

using RtIo::TextFileStream;
using RtSystem::Filename;
using RtSystem::String;

// Set by this probe, read by the bodies. Defined in text_file_stream_win.cpp.
extern int      g_tfsCtl;
extern uint64_t g_tfsAtt;
void RtIoTextFileStreamCounters(unsigned long long* out);

extern "C" uint32_t RtIo_TextFileStream_GetSize(const TextFileStream*);
extern "C" uint32_t RtIo_TextFileStream_GetCurrentPos(const TextFileStream*);
extern "C" bool     RtIo_TextFileStream_Read(TextFileStream*, int32_t, void*);
extern "C" bool     RtIo_TextFileStream_SetCurrentPos(TextFileStream*, int32_t);
extern "C" bool     RtIo_TextFileStream_Create(TextFileStream*, const Filename*,
                                               int32_t, int32_t);

namespace {

using GetU32Fn  = uint32_t (*)(const TextFileStream*);
using ReadFn    = bool (*)(TextFileStream*, int32_t, void*);
using SetPosFn  = bool (*)(TextFileStream*, int32_t);
using CreateFn  = bool (*)(TextFileStream*, const Filename*, int32_t, int32_t);
using TextCtorFn = void* (*)(TextFileStream*, const String*, const String*, int);

const char* const kGetSize =
    "?GetSize@TextFileStream@IO@@UEBAIXZ";
const char* const kGetPos =
    "?GetCurrentPos@TextFileStream@IO@@UEBAIXZ";
const char* const kRead =
    "?Read@TextFileStream@IO@@UEAA_NHPEAX@Z";
const char* const kSetPos =
    "?SetCurrentPos@TextFileStream@IO@@UEAA_NH@Z";
const char* const kCreate =
    "?Create@TextFileStream@IO@@UEAA_NAEBVFilename@System@@W4Format@Text@4@1@Z";
const char* const kTextCtor =
    "??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z";

// ---------------------------------------------------------------------------
// Per-screen scoreboards. A screen that never ran reports checked=0 and is
// declared VACUOUS by the verdict line, not green.
// ---------------------------------------------------------------------------
struct Screen { const char* name; int checked; int failed; };
Screen g_scr[4] = { {"A getters", 0, 0}, {"B setpos", 0, 0},
                    {"C read",    0, 0}, {"D create", 0, 0} };
uint64_t g_bytes = 0;   // screen D's positive control

void Chk(int s, bool ok, const char* what, long long got, long long want) {
    ++g_scr[s].checked;
    if (!ok) {
        ++g_scr[s].failed;
        printf("[TfsProbe]   FAIL [%s] %-30s ours=%lld theirs=%lld\n",
               g_scr[s].name, what, got, want);
    }
}

// ---------------------------------------------------------------------------
// The hand-built subject for screens A, B and C. 0x80 bytes, the size the
// deleting destructor's `MOV EDX,0x80` element stride gives.
// ---------------------------------------------------------------------------
struct RawTfs { unsigned char b[0x80]; };

int32_t* At32(RawTfs& r, size_t off) {
    return reinterpret_cast<int32_t*>(r.b + off);
}
void** AtPtr(RawTfs& r, size_t off) {
    return reinterpret_cast<void**>(r.b + off);
}
TextFileStream* As(RawTfs& r) {
    return reinterpret_cast<TextFileStream*>(r.b);
}

// buffer mode: install `buf`, size and cursor.
void MakeBuffered(RawTfs& r, void* buf, int32_t size, int32_t cursor) {
    std::memset(&r, 0, sizeof(r));
    *AtPtr(r, 0x70) = buf;
    *At32(r, 0x78)  = size;
    *At32(r, 0x7c)  = cursor;
    *At32(r, 0x20)  = -1;      // file.handle: nothing opened
}

// file mode: buffer null, FileInfo carrying a size, a cursor and a handle.
void MakeFileMode(RawTfs& r, int32_t handle, int32_t size, int32_t cursor) {
    std::memset(&r, 0, sizeof(r));
    *AtPtr(r, 0x70) = nullptr;
    *At32(r, 0x20)  = handle;
    *At32(r, 0x28)  = size;
    *At32(r, 0x2c)  = cursor;
}

long ReadAll(const char* path, unsigned char* buf, long cap) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    const long n = static_cast<long>(std::fread(buf, 1, static_cast<size_t>(cap), f));
    std::fclose(f);
    return n;
}

bool g_done = false;

}  // namespace

// ============================================================================
// Driven one-shot from ExternalFactory::Add, the same proven-live host the
// SaveText and FileStream-layout probes use. Scheduling an instrument off a
// body is a CLAIM THAT THE BODY RUNS; here that claim is already measured —
// `Add` runs ~22.6k times per menu-to-gameplay run and external_add.cpp's own
// `g_ac.calls` is the counter that says so.
// ============================================================================
void RtIoTextFileStreamProbeRun() {
    if (g_done) return;
    g_done = true;

    const char* e = std::getenv("BANISHED_RT_TFS_NEGCTL");
    g_tfsCtl = (e && e[0]) ? std::atoi(e) : 0;

    // A name the DLL does not contain resolves NULL SILENTLY
    // (reference_resolveexport_phantom_names.md), so every one is checked.
    auto tGetSize = reinterpret_cast<GetU32Fn>(ResolveExportRaw(kGetSize));
    auto tGetPos  = reinterpret_cast<GetU32Fn>(ResolveExportRaw(kGetPos));
    auto tRead    = reinterpret_cast<ReadFn>(ResolveExportRaw(kRead));
    auto tSetPos  = reinterpret_cast<SetPosFn>(ResolveExportRaw(kSetPos));
    auto tCreate  = reinterpret_cast<CreateFn>(ResolveExportRaw(kCreate));
    auto tCtor    = reinterpret_cast<TextCtorFn>(ResolveExportRaw(kTextCtor));

    const char* missing = nullptr;
    if (!tGetSize) missing = kGetSize;
    else if (!tGetPos)  missing = kGetPos;
    else if (!tRead)    missing = kRead;
    else if (!tSetPos)  missing = kSetPos;
    else if (!tCreate)  missing = kCreate;
    else if (!tCtor)    missing = kTextCtor;
    if (missing) {
        printf("[TfsProbe] FATAL phantom export name: %s\n", missing);
        return;
    }

    printf("[TfsProbe] ctl=%d -- ours vs the shipped DLL, 5 slots, 4 screens\n",
           g_tfsCtl);

    // ---- SCREEN A: GetSize / GetCurrentPos, both arms ----------------------
    // Pure readers, so ONE object can serve both sides: neither call mutates it.
    {
        unsigned char scratch[64];
        std::memset(scratch, 0xAB, sizeof(scratch));

        const int32_t sizes[]   = { 0, 1, 41, 0x7fffffff };
        const int32_t cursors[] = { 0, 7, -3, 41 };

        for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
            RawTfs r;
            // buffer arm
            MakeBuffered(r, scratch, sizes[i], cursors[i]);
            Chk(0, RtIo_TextFileStream_GetSize(As(r)) == tGetSize(As(r)),
                "GetSize buffer arm",
                RtIo_TextFileStream_GetSize(As(r)), tGetSize(As(r)));
            Chk(0, RtIo_TextFileStream_GetCurrentPos(As(r)) == tGetPos(As(r)),
                "GetCurrentPos buffer arm",
                RtIo_TextFileStream_GetCurrentPos(As(r)), tGetPos(As(r)));

            // file arm — DIFFERENT values in +0x28/+0x2c from the ones above, so
            // a body that read the wrong pair would not accidentally agree.
            MakeFileMode(r, /*handle*/ 0x01000005, sizes[i] ^ 0x5a5a, cursors[i] + 11);
            Chk(0, RtIo_TextFileStream_GetSize(As(r)) == tGetSize(As(r)),
                "GetSize file arm",
                RtIo_TextFileStream_GetSize(As(r)), tGetSize(As(r)));
            Chk(0, RtIo_TextFileStream_GetCurrentPos(As(r)) == tGetPos(As(r)),
                "GetCurrentPos file arm",
                RtIo_TextFileStream_GetCurrentPos(As(r)), tGetPos(As(r)));
        }
    }

    // ---- SCREEN B: SetCurrentPos, return value AND post-state --------------
    // MUTATES, so two parallel objects. The `pos` list is chosen around the
    // range test: 0 and 20 are inside, 41 is EXACTLY the size (where retail
    // returns false and still moves the cursor — the only value control 2 can
    // be caught at), 99 is past the end, -1 is negative.
    {
        unsigned char sa[64], sb[64];
        std::memset(sa, 0xCD, sizeof(sa));
        std::memset(sb, 0xCD, sizeof(sb));
        const int32_t positions[] = { 0, 20, 41, 99, -1 };

        for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
            const int32_t pos = positions[i];

            RawTfs ro, rt;
            MakeBuffered(ro, sa, 41, 5);
            MakeBuffered(rt, sb, 41, 5);
            const bool bo = RtIo_TextFileStream_SetCurrentPos(As(ro), pos);
            const bool bt = tSetPos(As(rt), pos);
            Chk(1, bo == bt, "SetCurrentPos buffer ret", bo, bt);
            Chk(1, *At32(ro, 0x7c) == *At32(rt, 0x7c), "SetCurrentPos buffer cursor",
                *At32(ro, 0x7c), *At32(rt, 0x7c));

            // File arm, PACKED handle (pack 1, file 5) — retail returns true
            // without touching Win32, which is the sub-arm the unsigned
            // `CMP ..,-3 ; JA` test selects.
            MakeFileMode(ro, 0x01000005, 41, 5);
            MakeFileMode(rt, 0x01000005, 41, 5);
            const bool fo = RtIo_TextFileStream_SetCurrentPos(As(ro), pos);
            const bool ft = tSetPos(As(rt), pos);
            Chk(1, fo == ft, "SetCurrentPos packed ret", fo, ft);
            Chk(1, *At32(ro, 0x2c) == *At32(rt, 0x2c), "SetCurrentPos packed cursor",
                *At32(ro, 0x2c), *At32(rt, 0x2c));
        }
        // The -1 / -2 sub-arm calls SetFilePointer on +0x38 and is NOT driven
        // here: with no real handle both sides would fail identically on a
        // Win32 error, which is agreement that proves nothing. Screen D opens a
        // real file, and that is where a live handle exists; see WHAT THIS
        // PROBE DOES NOT COVER at the bottom.
    }

    // ---- SCREEN C: Read, destination bytes AND post-state -------------------
    // Scored as TWO separate checks per case on purpose: control 3 suppresses
    // only the cursor advance, so a screen that compared only the copied bytes
    // would pass it.
    {
        static const unsigned char src[64] = {
            0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff,
            0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
            0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
            0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f };

        const int32_t starts[] = { 0, 1, 16, 31 };
        const int32_t lens[]   = { 1, 8, 16, 32 };

        for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
            unsigned char do_[64], dt[64];
            std::memset(do_, 0x5e, sizeof(do_));
            std::memset(dt,  0x5e, sizeof(dt));

            RawTfs ro, rt;
            MakeBuffered(ro, const_cast<unsigned char*>(src), 64, starts[i]);
            MakeBuffered(rt, const_cast<unsigned char*>(src), 64, starts[i]);

            const bool co = RtIo_TextFileStream_Read(As(ro), lens[i], do_);
            const bool ct = tRead(As(rt), lens[i], dt);

            Chk(2, co == ct, "Read ret", co, ct);
            Chk(2, std::memcmp(do_, dt, sizeof(do_)) == 0, "Read destination bytes",
                do_[0], dt[0]);
            Chk(2, *At32(ro, 0x7c) == *At32(rt, 0x7c), "Read post cursor",
                *At32(ro, 0x7c), *At32(rt, 0x7c));
        }
    }

    // ---- SCREEN D: Create, the byte-order-mark table, on disk ---------------
    // THE ONE SCREEN THAT TOUCHES THE FILESYSTEM, and the only one that can see
    // the BOM at all.
    //
    // `_requestedFormat` is deliberately set to a value DIFFERENT from
    // `_activeEncoding` on every case, so that a body selecting the mark from
    // the wrong member of the pair writes different bytes. Without that the two
    // fields are indistinguishable and control 4 could not be caught — which is
    // exactly the gap SaveText's copy of this table has, since it writes 2 to
    // both.
    {
        static const int32_t encodings[] = { 0, 2, 3, 4, 5, 7 };
        // 7 is OUTSIDE {0,2,3,4,5}: retail's `sub 2 / dec / dec / dec` chain
        // falls through to "no mark", and this is the case that says so.

        for (size_t i = 0; i < sizeof(encodings) / sizeof(encodings[0]); ++i) {
            const int32_t enc = encodings[i];
            const int32_t req = (enc == 3) ? 4 : 3;   // always different, and
                                                      // always a value that
                                                      // WOULD write a mark.

            char pOurs[64], pTheirs[64];
            std::snprintf(pOurs,   sizeof(pOurs),   "tfs_probe_ours_%d.txt",   (int)enc);
            std::snprintf(pTheirs, sizeof(pTheirs), "tfs_probe_theirs_%d.txt", (int)enc);
            std::remove(pOurs);
            std::remove(pTheirs);

            Filename fo{}; fo.data = nullptr;
            Filename ft{}; ft.data = nullptr;
            String   ty{}; ty.data = nullptr;
            String   nm{}; nm.data = nullptr;
            RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&fo), pOurs);
            RtSystem::RtSystem_String_Set(reinterpret_cast<String*>(&ft), pTheirs);
            RtSystem::RtSystem_String_Set(&ty, "TfsProbe");
            RtSystem::RtSystem_String_Set(&nm, "tfs_probe");

            // Both subjects built by the SHIPPED constructor — see the header.
            // Deliberately NOT destroyed: this runs once per process and the
            // destructor is a slot this lane did not port (slot 0). Leaking two
            // streams in a one-shot opt-in probe is cheaper than driving an
            // unported destructor and mistaking its faults for these bodies'.
            RawTfs bo, bt;
            std::memset(&bo, 0, sizeof(bo));
            std::memset(&bt, 0, sizeof(bt));
            tCtor(As(bo), &ty, &nm, 0);
            tCtor(As(bt), &ty, &nm, 0);

            const bool ro = RtIo_TextFileStream_Create(As(bo), &fo, req, enc);
            const bool rt = tCreate(As(bt), &ft, req, enc);

            Chk(3, ro == rt, "Create ret", ro, rt);
            Chk(3, *At32(bo, 0x68) == *At32(bt, 0x68), "Create +0x68 requested",
                *At32(bo, 0x68), *At32(bt, 0x68));
            Chk(3, *At32(bo, 0x6c) == *At32(bt, 0x6c), "Create +0x6c active",
                *At32(bo, 0x6c), *At32(bt, 0x6c));
            Chk(3, *At32(bo, 0x2c) == *At32(bt, 0x2c), "Create post file.cursor",
                *At32(bo, 0x2c), *At32(bt, 0x2c));

            // THE ARTEFACT. A write goes to the handle immediately here (unlike
            // a stream that is renamed into place on close), but the bytes are
            // not necessarily flushed while the handle is open — so the handle
            // is closed through the SHIPPED FileSystem before the file is read
            // back. Reading too early is the error
            // WAVE19_LANE_FILE warned about from the other direction.
            using CloseFileFn = void (*)(void*, RtIO::FileInfo*);
            auto closeFile = reinterpret_cast<CloseFileFn>(ResolveExportRaw(
                "?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z"));
            void* fs = *reinterpret_cast<void**>(ResolveExportRaw(
                "?_singleton@FileSystem@IO@@0PEAV12@EA"));
            if (closeFile && fs) {
                closeFile(fs, reinterpret_cast<RtIO::FileInfo*>(bo.b + 0x20));
                closeFile(fs, reinterpret_cast<RtIO::FileInfo*>(bt.b + 0x20));
            }

            unsigned char fbo[64], fbt[64];
            const long no = ReadAll(pOurs,   fbo, (long)sizeof(fbo));
            const long nt = ReadAll(pTheirs, fbt, (long)sizeof(fbt));
            Chk(3, no == nt, "Create file length", no, nt);
            if (no > 0 && no == nt) {
                g_bytes += (uint64_t)no;
                Chk(3, std::memcmp(fbo, fbt, (size_t)no) == 0, "Create file bytes",
                    fbo[0], fbt[0]);
            }
        }
    }

    // ---- VERDICT -----------------------------------------------------------
    int totC = 0, totF = 0;
    for (int i = 0; i < 4; ++i) {
        const char* v = (g_scr[i].checked == 0) ? "VACUOUS"
                      : (g_scr[i].failed  == 0) ? "PASS" : "FAIL";
        printf("[TfsProbe]   screen %-10s checked=%-3d failed=%-3d %s\n",
               g_scr[i].name, g_scr[i].checked, g_scr[i].failed, v);
        totC += g_scr[i].checked;
        totF += g_scr[i].failed;
    }

    unsigned long long c[11] = {0};
    RtIoTextFileStreamCounters(c);
    printf("[TfsProbe]   attendance read=%llu setPos(buf=%llu file=%llu seek=%llu) "
           "getPos(buf=%llu file=%llu) getSize(buf=%llu file=%llu) "
           "create=%llu failed=%llu bom=%llu\n",
           c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10]);

    printf("[TfsProbe] VERDICT ctl=%d att=%llu checked=%d failed=%d bytes=%llu %s\n",
           g_tfsCtl, (unsigned long long)g_tfsAtt, totC, totF, (unsigned long long)g_bytes,
           (totC == 0) ? "VACUOUS" : (totF == 0 ? "PASS" : "FAIL"));
    std::fflush(stdout);
}

}  // namespace RtIO

// ============================================================================
// WHAT THIS PROBE DOES NOT COVER — stated so a green VERDICT is not read as
// more than it is.
//
// * `SetCurrentPos`'s Win32 sub-arm (handle -1 or -2 -> `SetFilePointer` on
//   +0x38) is NOT driven. Screen B uses a packed handle, which is the arm that
//   returns without touching Win32. Driving the other arm needs a live handle,
//   and with no handle both sides fail identically — agreement that proves
//   nothing (reference_probe_needs_an_effectiveness_control.md). `setPosSeek`
//   in the attendance line is the counter that says so: it is EXPECTED to read
//   0, and a future run where it does not is the first evidence for that arm.
// * Slots 0, 2 and 8 are not ported and are not screened.
// * Screen D's `Create` is exercised only with a path in the current working
//   directory, so `FileSystem::CreatePath`'s directory-creating behaviour is
//   not part of the subject.
// * NOTHING HERE MEASURES WHETHER THE MINTED VTABLE DISPATCHES THESE BODIES. It
//   compares our exported body against the DLL's exported body. A green screen
//   is evidence about the BODIES, not about promotion
//   (reference_a_green_readiness_screen_is_not_a_route.md).
// ============================================================================

extern "C" void RtIoProbeRunTextFileStreamProbe() {
    RtIO::RtIoTextFileStreamProbeRun();
}
