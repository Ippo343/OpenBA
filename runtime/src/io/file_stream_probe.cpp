// ============================================================================
// runtime/src/io/file_stream_probe.cpp — Wave 11 follow-up 2, lane IOSTREAM
//   The oracle for runtime/types/io/FileStream.h.
// ============================================================================
//
// THE SUBJECT OF THIS LANE IS A LAYOUT, so the instrument has to be an oracle
// for a layout — not for a body. There are no ported stream bodies to shadow
// here and there is nothing to compare "ours" against, because the header is
// the deliverable and the header is a CLAIM ABOUT RETAIL.
//
// So: THEIRS RUNS FOR REAL, and the claim is checked against what it did.
// Retail's own exported constructors are run on buffers this file allocates,
// and three independent screens then ask whether the header describes the
// object retail actually built. That is the shape rt_predict.h calls a
// predictive oracle, and it is NOT the shape RT_DIFF_SHADOW has: nothing here
// consults the port's own verdict, because the port has no verdict to give
// (reference_shadow_marker_is_not_an_oracle.md).
//
// ---------------------------------------------------------------------------
// THE THREE SCREENS, AND WHAT EACH CAN AND CANNOT SAY
// ---------------------------------------------------------------------------
// SCREEN A — WRITE EXTENT. Prefill sizeof(T)+64 bytes with a pattern, run
//   retail's constructor, and require every byte at index >= sizeof(T) to be
//   untouched. This can prove sizeof is not TOO SMALL. It CANNOT prove it is
//   not too large — nothing a constructor does is evidence against trailing
//   padding — and the header does not lean on it for that: the sizes come from
//   the `_eh_vector_destructor_iterator_` strides, which are a measurement of
//   the array stride, i.e. of sizeof itself.
//
// SCREEN B — CONSTRUCTOR VALUES. Read the fields THROUGH OUR STRUCT and require
//   the values the header says each constructor writes. Catches an offset that
//   is wrong by enough to land on a differently-valued field. Blind to any two
//   fields the constructor sets to the SAME value — which is a real blind spot
//   here and is reported as one (see control 5).
//
// SCREEN C — RETAIL'S OWN ACCESSORS. Read the same field two ways: through our
//   offsets, and through a shipped one-instruction accessor that names it.
//   This is the only screen whose evidence is independent of our reading of the
//   constructor, and it is the one that MATTERS: it already caught a real
//   defect in this header before any of this ran.
//
//   `TextFileStream::objectType` and `objectName` were SWAPPED. Two adjacent
//   refcounted `String`s, written by the same idiom in the same function, with
//   a constructor signature of `(String const&, String const&, int)` that names
//   neither. Inspection cannot settle that; a body that NAMES one of them can,
//   and `?GetObjectType@` (`LEA RAX,[RCX+0x60]`) and `?GetObjectName@`
//   (`LEA RAX,[RCX+0x58]`) do. Screen C is in this file because that class of
//   defect will recur, not because it happened once.
//
// ---------------------------------------------------------------------------
// WHERE IT RUNS, AND WHY NOT AT INIT
// ---------------------------------------------------------------------------
// Four of the five constructors ALLOCATE (each installs an `IO::Encoding` at
// +0x10 through the `MemoryAllocator` singleton). That singleton does not
// exist at `RuntimePortInit` and the first allocation there faults at
// `null+0x640` (WAVE9 lane PKG2; reference_a_green_readiness_screen_is_not_a_
// route.md is the general form — an instrument outside its subject's live
// window prints a plausible ZERO, not an error).
//
// So the probe hangs off the FIRST CALL INTO `ExternalFactory::Add`, which is
// safe for exactly the reason external_set.cpp's probe hangs off the first
// `Insert`: the caller is itself about to allocate. And unlike lane OBJEXT's
// first attempt — which scheduled off `PushPath`, a body nothing calls
// (WAVE11_LANE_OBJEXT.md §10.1) — `Add` is DRIVEN ~22.6k times per run by our
// own ported `FindOrLoad`. That is a measured claim, and `g_ac.calls` in
// external_add.cpp is its positive control: if the probe never reports, that
// counter says whether the reason was "never reached" or "reached and silent".
//
// Re-entry is impossible: the arm flag is cleared before anything is driven.
//
// ---------------------------------------------------------------------------
// NEGATIVE CONTROLS
// ---------------------------------------------------------------------------
// `BANISHED_RT_IOSTREAM_NEGCTL=1..5`. Each control is ONE PARAMETER consumed by
// the SINGLE implementation of the thing it perturbs, and each ATTENDS at the
// site it names (`att=`), so a control that catches for an unrelated reason
// prints as NOT BANKED rather than as coverage. Both requirements come from
// WAVE11_LANE_OBJEXT.md §10.3/§10.5, where a stale second copy of a control
// caught in a screen the control could not reach.
//
// Every control produces a COMPARABLE DIVERGENCE, never undefined behaviour:
// they perturb what the probe READS, never what retail writes, so a wrong
// control cannot corrupt the object or take the run down with the other four
// (§10.2, the control that would have crashed instead of diverging).
//
// Controls are scored SEPARATELY and each one's catch rate is reported with the
// mechanism of its misses. Control 5 is EXPECTED NOT TO CATCH and is kept for
// that reason — see its note.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileStream.h"
#include "../../types/system/String.h"
#include "../system/string.h"

namespace RtIoProbe {

// ---------------------------------------------------------------------------
// The one door shape in this file. `ResolveExportRaw` is GetProcAddress
// against the shipped DLL and NEVER consults the port registry — which is the
// whole point: an oracle that resolved through `ResolveAddr(Module::Runtime,…)`
// or `ResolveExport` would be handed OUR body wherever one is registered and
// would compare ours with ours
// (reference_resolveaddr_consults_port_registry_first.md, and the Wave 5
// Goal::Serialize probe that was dark for five waves).
//
// It is GetProcAddress on every call and the house rule is to resolve once, but
// this probe runs ONCE per process, so the cheap-and-cached shape would only
// add machinery here (reference_rtoriginal_is_getprocaddress.md is about calls
// in a body, not in a one-shot instrument).
void* RtResolveExportForProbe(const char* mangled) {
    void* p = ResolveExportRaw(mangled);
    if (p == nullptr) {
        // A name the DLL does not contain resolves NULL SILENTLY
        // (reference_resolveexport_phantom_names.md). Say so loudly rather than
        // calling through it.
        printf("[IOStream]   FATAL phantom export name: %s\n", mangled);
        std::abort();
    }
    return p;
}

namespace {

using RtSystem::String;

// ---------------------------------------------------------------------------
// The control, read once. `getenv` is cached process-wide
// (standalone/env_cache.h), but the probe runs once so this is a formality.
// ---------------------------------------------------------------------------
int g_ctl = 0;

// Attendance: incremented AT the perturbation site each control names. A catch
// with att=0 is not credited.
uint64_t g_att[6] = {0, 0, 0, 0, 0, 0};

int  g_checked = 0;
int  g_failed  = 0;

void Check(bool ok, const char* what, unsigned long long got, unsigned long long want) {
    ++g_checked;
    if (!ok) {
        ++g_failed;
        printf("[IOStream]   FAIL %-46s got=0x%llx want=0x%llx\n", what, got, want);
    }
}

// ---------------------------------------------------------------------------
// CONTROL 1 — FileStream::file read at +0x24 instead of +0x20.
// One implementation, one parameter. Every screen that reads a FileInfo field
// goes through here, so the control cannot be applied in one place and missed
// in another.
// ---------------------------------------------------------------------------
const RtIO::FileInfo* FileInfoOf(const RtIo::FileStream* fs) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(fs);
    if (g_ctl == 1) { ++g_att[1]; p += 0x24; } else { p += 0x20; }
    return reinterpret_cast<const RtIO::FileInfo*>(p);
}

// ---------------------------------------------------------------------------
// CONTROL 2 — TextFileStream's two Strings swapped, i.e. THE DEFECT THIS
// HEADER ACTUALLY HAD before screen C was written. The control exists to prove
// screen C would have caught it, rather than to assert it after the fact.
// ---------------------------------------------------------------------------
char* const* TextObjectType(const RtIo::TextFileStream* ts) {
    if (g_ctl == 2) { ++g_att[2]; return &ts->objectName; }
    return &ts->objectType;
}
char* const* TextObjectName(const RtIo::TextFileStream* ts) {
    if (g_ctl == 2) { ++g_att[2]; return &ts->objectType; }
    return &ts->objectName;
}

// ---------------------------------------------------------------------------
// CONTROLS 3 and 4 — an UNDERSTATED size, which is the failure mode screen A
// exists to catch, and the one that would corrupt a caller's stack frame.
// 0x98 is not an arbitrary wrong number: it is sizeof(CompressedMemoryStream),
// the sibling class, and is exactly what a lane that reused the neighbouring
// header without re-measuring would write down.
// ---------------------------------------------------------------------------
size_t ClaimedSize(size_t real, int classId) {
    if (g_ctl == 3 && classId == 2) { ++g_att[3]; return 0x98; }  // CompressedFileStream
    if (g_ctl == 4 && classId == 4) { ++g_att[4]; return 0x10; }  // VisitorStream
    return real;
}

// ---------------------------------------------------------------------------
// CONTROL 5 — TextFileStream's +0x68 and +0x6c swapped.
//
// EXPECTED NOT TO CATCH, and kept precisely because of that. Both constructors
// write 6 to BOTH fields, so screen B cannot tell them apart: this is a fact
// about the CONSTRUCTOR, not about the probe's inputs, and enlarging the input
// set cannot change it. What distinguishes them is `?SaveText@External@Object@@`
// branching on +0x6c alone to pick the byte-order mark — a body this probe does
// not drive. Reported as NOT CAUGHT with that mechanism stated, rather than
// dropped so the control table reads 5/5 (reference_union_control_hides_a_
// blind_screen.md).
// ---------------------------------------------------------------------------
const int32_t* TextRequestedFormat(const RtIo::TextFileStream* ts) {
    if (g_ctl == 5) { ++g_att[5]; return &ts->activeEncoding; }
    return &ts->requestedFormat;
}
const int32_t* TextActiveEncoding(const RtIo::TextFileStream* ts) {
    if (g_ctl == 5) { ++g_att[5]; return &ts->requestedFormat; }
    return &ts->activeEncoding;
}

// ---------------------------------------------------------------------------
// Doors. Every constructor, destructor and accessor below is an EXPORTED name,
// so all of these are NAME doors — there is not one address door in this file.
// ---------------------------------------------------------------------------
using CtorFn      = void* (*)(void*);
using TextSetObj  = void  (*)(void* self, const String* type, const String* name, int version);
using GetU32Fn    = uint32_t (*)(const void*);
using GetI32Fn    = int32_t  (*)(const void*);
using GetStrRefFn = char* const* (*)(const void*);

constexpr uint8_t kFill  = 0xA5;
constexpr size_t  kGuard = 64;

// One class's screen A + B. Returns the number of bytes the constructor wrote
// inside the claimed size — the POSITIVE CONTROL for this screen, which must be
// non-zero or the whole cell is vacuous (a constructor that never ran writes
// nothing and trivially respects any guard).
size_t RunCtorScreen(const char* label, size_t realSize, int classId,
                     const char* ctorName, const char* dtorName,
                     void** outObj, uint8_t** outBuf) {
    const size_t claimed = ClaimedSize(realSize, classId);
    const size_t alloc   = realSize + kGuard;

    uint8_t* buf = static_cast<uint8_t*>(std::malloc(alloc));
    std::memset(buf, kFill, alloc);

    void* obj = buf;
    reinterpret_cast<CtorFn>(RtResolveExportForProbe(ctorName))(obj);

    // ---- SCREEN A: nothing past the claimed size may have moved ----------
    size_t guardHits = 0;
    for (size_t i = claimed; i < alloc; ++i)
        if (buf[i] != kFill) ++guardHits;

    // ---- the positive control for this cell -------------------------------
    size_t wrote = 0;
    for (size_t i = 0; i < claimed && i < alloc; ++i)
        if (buf[i] != kFill) ++wrote;

    ++g_checked;
    if (guardHits != 0) {
        ++g_failed;
        printf("[IOStream]   FAIL %-20s wrote %llu byte(s) PAST claimed size 0x%llx\n",
                    label, (unsigned long long)guardHits, (unsigned long long)claimed);
    }
    if (wrote == 0)
        printf("[IOStream]   WARN %-20s constructor wrote NOTHING -- cell is vacuous\n",
                    label);

    *outObj = obj;
    *outBuf = buf;
    (void)dtorName;
    return wrote;
}

}  // namespace

// ============================================================================
// The probe.
// ============================================================================
void RunFileStreamLayoutProbe() {
    const char* e = std::getenv("BANISHED_RT_IOSTREAM_NEGCTL");
    g_ctl = (e && e[0]) ? std::atoi(e) : 0;

    printf("[IOStream] layout probe ctl=%d -- retail constructors vs "
                "runtime/types/io/FileStream.h\n", g_ctl);

    // ---------------------------------------------------------------- FileStream
    {
        void* obj; uint8_t* buf;
        size_t wrote = RunCtorScreen("FileStream", sizeof(RtIo::FileStream), 0,
                                     "??0FileStream@IO@@QEAA@XZ",
                                     "??1FileStream@IO@@UEAA@XZ", &obj, &buf);
        auto* fs = static_cast<RtIo::FileStream*>(obj);
        const RtIO::FileInfo* fi = FileInfoOf(fs);

        // SCREEN B — the values ??0FileStream (0x1800aaa10) writes.
        Check(fs->base.direction == 0,      "FileStream.base.direction == 0",
              (unsigned)fs->base.direction, 0);
        Check(fs->filename == nullptr,      "FileStream.filename == null",
              (uintptr_t)fs->filename, 0);
        Check(fi->handle == -1,             "FileStream.file.handle == -1",
              (uint32_t)fi->handle, 0xffffffffu);
        Check(fi->winHandle == -1,          "FileStream.file.winHandle == -1",
              (uint64_t)fi->winHandle, (uint64_t)-1);

        // SCREEN C — retail's own accessors for the same two FileInfo fields.
        // `?GetSize@FileStream` is `MOV EAX,[RCX+0x28]; RET` and
        // `?GetCurrentPos@FileStream` is `MOV EAX,[RCX+0x2c]; RET`, so they
        // name FileInfo::size and FileInfo::cursor through OUR +0x20 claim.
        auto getSize = reinterpret_cast<GetU32Fn>(
            RtResolveExportForProbe("?GetSize@FileStream@IO@@UEBAIXZ"));
        auto getPos  = reinterpret_cast<GetU32Fn>(
            RtResolveExportForProbe("?GetCurrentPos@FileStream@IO@@UEBAIXZ"));
        Check(getSize(obj) == (uint32_t)fi->size,
              "GetSize()  == our file.size", getSize(obj), (uint32_t)fi->size);
        Check(getPos(obj)  == (uint32_t)fi->cursor,
              "GetCurrentPos() == our file.cursor", getPos(obj), (uint32_t)fi->cursor);

        reinterpret_cast<CtorFn>(
            RtResolveExportForProbe("??1FileStream@IO@@UEAA@XZ"))(obj);
        std::free(buf);
        printf("[IOStream]   FileStream            wrote=%llu/0x%llx\n",
                    (unsigned long long)wrote, (unsigned long long)sizeof(RtIo::FileStream));
    }

    // ---------------------------------------------------------- BinaryFileStream
    {
        void* obj; uint8_t* buf;
        size_t wrote = RunCtorScreen("BinaryFileStream", sizeof(RtIo::BinaryFileStream), 1,
                                     "??0BinaryFileStream@IO@@QEAA@XZ",
                                     "??1BinaryFileStream@IO@@UEAA@XZ", &obj, &buf);
        auto* bs = static_cast<RtIo::BinaryFileStream*>(obj);
        const RtIO::FileInfo* fi = FileInfoOf(&bs->base);

        Check(fi->handle == -1,   "BinaryFileStream.file.handle == -1",
              (uint32_t)fi->handle, 0xffffffffu);
        // Unlike ??0FileStream, this one INSTALLS an Encoding. That is the one
        // observable difference between the two constructors and it is what
        // makes "BinaryFileStream is a FileStream plus a vtable" checkable.
        Check(bs->base.base.encoding != nullptr,
              "BinaryFileStream.encoding installed", (uintptr_t)bs->base.base.encoding, 1);

        reinterpret_cast<CtorFn>(
            RtResolveExportForProbe("??1BinaryFileStream@IO@@UEAA@XZ"))(obj);
        std::free(buf);
        printf("[IOStream]   BinaryFileStream      wrote=%llu/0x%llx\n",
                    (unsigned long long)wrote,
                    (unsigned long long)sizeof(RtIo::BinaryFileStream));
    }

    // ------------------------------------------------------ CompressedFileStream
    {
        void* obj; uint8_t* buf;
        size_t wrote = RunCtorScreen("CompressedFileStream",
                                     sizeof(RtIo::CompressedFileStream), 2,
                                     "??0CompressedFileStream@IO@@QEAA@XZ",
                                     "??1CompressedFileStream@IO@@UEAA@XZ", &obj, &buf);
        auto* cf = static_cast<RtIo::CompressedFileStream*>(obj);

        // The CompressedStream half, from Stream.h's own pins.
        Check(cf->base.activeWindow == -1, "CompressedFileStream.base.activeWindow == -1",
              (uint32_t)cf->base.activeWindow, 0xffffffffu);
        Check(cf->base.source == nullptr,  "CompressedFileStream.base.source == null",
              (uintptr_t)cf->base.source, 0);

        // THE +0x68 MEMBER, which is the claim this class exists to make.
        const RtIO::FileInfo* fi = FileInfoOf(&cf->file.base);
        Check(fi->handle == -1, "CompressedFileStream.file(+0x68).handle == -1",
              (uint32_t)fi->handle, 0xffffffffu);

        // TWO vtables in one object, at two different offsets, each stamped by
        // its own constructor. If +0x68 were wrong, the inner stamp would land
        // somewhere else and this pair could not both hold.
        // THE TABLE THIS MUST COMPARE AGAINST IS THE DLL'S, NOT THE REGISTRY'S.
        // WAVE 26 lane PROMOIO. RunCtorScreen builds the object with
        // RtResolveExportForProbe -> ResolveExportRaw, i.e. RETAIL's constructor,
        // which stamps retail's table by an absolute LEA. `RT_DATA_SLOT` goes
        // through ResolveExport, which consults the PORT REGISTRY FIRST, so the
        // moment a `??_7` row is PROMOTED it hands back OUR minted table and the
        // two sides can no longer be equal by construction.
        //
        // That already happened, twice, and nothing noticed because this probe is
        // opt-in (BANISHED_RT_IOSTREAM_PROBE): `??_7BinaryFileStream@IO@@6B@` was
        // promoted by WAVE 20 lane PROMOTE and `??_7VisitorStream@IO@@6B@` by
        // WAVE 19 lane IOBASE, so two of this file's three vptr checks have been
        // structurally FALSE since those waves. The failure does not read as
        // "a promotion happened" — it reads as a LAYOUT defect, which is the one
        // thing this probe exists to detect.
        //
        // The claim the check is making is about the OFFSET (+0x00 and +0x68 are
        // two separately-stamped vtables in one object), and that claim is
        // preserved exactly by asking the DLL for the address. Use the raw door.
        const void* vtOuter = RtResolveExportForProbe("??_7CompressedFileStream@IO@@6B@");
        const void* vtInner = RtResolveExportForProbe("??_7BinaryFileStream@IO@@6B@");
        Check(cf->base.base.vptr == vtOuter, "vptr(+0x00) == ??_7CompressedFileStream",
              (uintptr_t)cf->base.base.vptr, (uintptr_t)vtOuter);
        Check(cf->file.base.base.vptr == vtInner, "vptr(+0x68) == ??_7BinaryFileStream",
              (uintptr_t)cf->file.base.base.vptr, (uintptr_t)vtInner);

        reinterpret_cast<CtorFn>(
            RtResolveExportForProbe("??1CompressedFileStream@IO@@UEAA@XZ"))(obj);
        std::free(buf);
        printf("[IOStream]   CompressedFileStream  wrote=%llu/0x%llx\n",
                    (unsigned long long)wrote,
                    (unsigned long long)sizeof(RtIo::CompressedFileStream));
    }

    // ------------------------------------------------------------ TextFileStream
    {
        void* obj; uint8_t* buf;
        size_t wrote = RunCtorScreen("TextFileStream", sizeof(RtIo::TextFileStream), 3,
                                     "??0TextFileStream@IO@@QEAA@XZ",
                                     "??1TextFileStream@IO@@UEAA@XZ", &obj, &buf);
        auto* ts = static_cast<RtIo::TextFileStream*>(obj);

        Check(*TextRequestedFormat(ts) == 6, "TextFileStream.requestedFormat == 6",
              (uint32_t)*TextRequestedFormat(ts), 6);
        Check(*TextActiveEncoding(ts)  == 6, "TextFileStream.activeEncoding == 6",
              (uint32_t)*TextActiveEncoding(ts), 6);
        Check(ts->objectVersion == 0,        "TextFileStream.objectVersion == 0",
              (uint32_t)ts->objectVersion, 0);

        // ---- SCREEN C, and the reason this file exists ---------------------
        // Give the two Strings DISTINCT values, then ask retail which is which.
        // With equal values the screen would be vacuously green — the same trap
        // as comparing two fields a constructor sets identically.
        String typeStr{}; typeStr.data = nullptr;
        String nameStr{}; nameStr.data = nullptr;
        RtSystem::RtSystem_String_Set(&typeStr, "IOSTREAM_TYPE");
        RtSystem::RtSystem_String_Set(&nameStr, "IOSTREAM_NAME");

        reinterpret_cast<TextSetObj>(
            RtResolveExportForProbe("?SetObject@TextFileStream@IO@@QEAAXAEBVString@System@@0H@Z"))(
                obj, &typeStr, &nameStr, 7);

        auto getType = reinterpret_cast<GetStrRefFn>(
            RtResolveExportForProbe("?GetObjectType@TextFileStream@IO@@QEBAAEBVString@System@@XZ"));
        auto getName = reinterpret_cast<GetStrRefFn>(
            RtResolveExportForProbe("?GetObjectName@TextFileStream@IO@@QEBAAEBVString@System@@XZ"));
        auto getVer  = reinterpret_cast<GetI32Fn>(
            RtResolveExportForProbe("?GetObjectVersion@TextFileStream@IO@@QEBAHXZ"));

        // Compared as TEXT, not as pointers: SetObject adopts the caller's
        // buffer here so the pointers happen to match, but a comparison that
        // depends on that would break the moment anything copied
        // (reference_oracle_must_snapshot_aliased_inputs.md).
        const char* ourType    = *TextObjectType(ts);
        const char* ourName    = *TextObjectName(ts);
        const char* retailType = *getType(obj);
        const char* retailName = *getName(obj);

        Check(ourType && retailType && std::strcmp(ourType, retailType) == 0,
              "GetObjectType() == our objectType(+0x60)", 0, 0);
        Check(ourName && retailName && std::strcmp(ourName, retailName) == 0,
              "GetObjectName() == our objectName(+0x58)", 0, 0);
        Check(getVer(obj) == ts->objectVersion,
              "GetObjectVersion() == our objectVersion(+0x50)",
              (uint32_t)getVer(obj), (uint32_t)ts->objectVersion);

        // And the strong form: the two must not merely agree with each other
        // but carry the values SetObject was handed in each role. A swap that
        // fooled both our read and retail's would still fail this.
        Check(retailType && std::strcmp(retailType, "IOSTREAM_TYPE") == 0,
              "retail's objectType is SetObject's FIRST arg", 0, 0);
        Check(retailName && std::strcmp(retailName, "IOSTREAM_NAME") == 0,
              "retail's objectName is SetObject's SECOND arg", 0, 0);

        reinterpret_cast<CtorFn>(
            RtResolveExportForProbe("??1TextFileStream@IO@@UEAA@XZ"))(obj);
        RtSystem::RtString_ReleaseInline(&typeStr);
        RtSystem::RtString_ReleaseInline(&nameStr);
        std::free(buf);
        printf("[IOStream]   TextFileStream        wrote=%llu/0x%llx\n",
                    (unsigned long long)wrote,
                    (unsigned long long)sizeof(RtIo::TextFileStream));
    }

    // --------------------------------------------------------------- VisitorStream
    {
        void* obj; uint8_t* buf;
        size_t wrote = RunCtorScreen("VisitorStream", sizeof(RtIo::VisitorStream), 4,
                                     "??0VisitorStream@IO@@QEAA@XZ",
                                     "??1VisitorStream@IO@@UEAA@XZ", &obj, &buf);
        auto* vs = static_cast<RtIo::VisitorStream*>(obj);

        // direction == 1 is the ONE thing ??0VisitorStream does that
        // ??0Stream does not, and it is what says this object is a WRITER.
        Check(vs->base.direction == 1, "VisitorStream.direction == 1",
              (unsigned)vs->base.direction, 1);
        Check(vs->base.encoding != nullptr, "VisitorStream.encoding installed",
              (uintptr_t)vs->base.encoding, 1);
        // Raw, not RT_DATA_SLOT — see the block above the CompressedFileStream
        // pair. This row has been PROMOTED since WAVE 19 lane IOBASE, so the
        // RT_DATA_SLOT spelling of this line has been comparing our minted table
        // against retail's stamp, and failing, ever since.
        const void* vt = RtResolveExportForProbe("??_7VisitorStream@IO@@6B@");
        Check(vs->base.vptr == vt, "VisitorStream.vptr == ??_7VisitorStream",
              (uintptr_t)vs->base.vptr, (uintptr_t)vt);

        reinterpret_cast<CtorFn>(
            RtResolveExportForProbe("??1VisitorStream@IO@@UEAA@XZ"))(obj);
        std::free(buf);
        printf("[IOStream]   VisitorStream         wrote=%llu/0x%llx\n",
                    (unsigned long long)wrote,
                    (unsigned long long)sizeof(RtIo::VisitorStream));
    }

    // ---------------------------------------------------------------- verdict
    const uint64_t att = (g_ctl >= 1 && g_ctl <= 5) ? g_att[g_ctl] : 0;
    if (g_ctl == 0) {
        printf("[IOStream] LAYOUT ctl=0 checked=%d failed=%d  %s\n",
                    g_checked, g_failed, g_failed == 0 ? "PASS" : "*** MISMATCH ***");
    } else if (g_failed == 0) {
        printf("[IOStream] LAYOUT ctl=%d checked=%d failed=0 att=%llu  "
                    "control NOT CAUGHT\n", g_ctl, g_checked, (unsigned long long)att);
    } else if (att == 0) {
        printf("[IOStream] LAYOUT ctl=%d checked=%d failed=%d att=0  "
                    "CAUGHT BUT NOT VIA ITS OWN MECHANISM -- NOT BANKED\n",
                    g_ctl, g_checked, g_failed);
    } else {
        printf("[IOStream] LAYOUT ctl=%d checked=%d failed=%d att=%llu  "
                    "control CAUGHT\n", g_ctl, g_checked, g_failed,
                    (unsigned long long)att);
    }
}

}  // namespace RtIoProbe

// Flat C++ entry point for external_add.cpp's arm site. Declared there as a
// block-scope `extern` so the caller needs no header from this file — the same
// shape rt_object_port.cpp uses for RtObjectFactoryDtorsProbe.
extern "C" void RtIoProbeRunFileStreamLayoutProbe() {
    RtIoProbe::RunFileStreamLayoutProbe();
}
