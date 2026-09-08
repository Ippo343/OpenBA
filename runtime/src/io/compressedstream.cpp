// ============================================================================
// runtime/src/io/compressedstream.cpp — Wave 4 lane B: IO::CompressedMemoryStream
// ============================================================================
//
// The six IO stream-family exports our tree calls, all of them on
// IO::CompressedMemoryStream and all of them from ONE caller,
// code/Game/Game/gameupdateloop_serialize.cpp (GameUpdateLoop::SaveRestore,
// exe 0x1400959c0):
//
//   retail      profile     ordinal  export
//   0x1800a9520 0x18005c240  106     ??0CompressedMemoryStream@IO@@QEAA@XZ
//   0x1800a9560 0x18005c280  551     ??1CompressedMemoryStream@IO@@UEAA@XZ
//   0x1800a9600  --          1874    ?Create@CompressedMemoryStream@IO@@QEAA_NXZ
//   0x1800a9500  --          3302    ?Open@CompressedMemoryStream@IO@@QEAA_NXZ
//   0x1800a9620  --          1719    ?Close@CompressedMemoryStream@IO@@UEAAXXZ
//   0x180024350  --          2784    ?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAVMemoryStream@2@XZ
//
// All six manglings are present in the shipped DLL's export directory
// (rt_io_port.cpp warning 2). Authoritative check:
// docs/runtime_port/wave4_export_names.txt — the wave-wide pefile sweep, 39 IO
// symbols, 39 ok / 0 MISSING; the addresses and ordinals above are its rows.
// The three vtable symbols and the seven call-through targets below are NOT in
// that sweep and were checked separately by this lane; all present, see
// WAVE4_LANE_B.md §2. Nothing here is a phantom name.
//
// ?GetMemoryStream sits far from the other five, in the 0x18002xxxx COMDAT pool
// of inline-defined and compiler-generated one-liners. It is NOT identical-
// COMDAT-folded with anything: grouping the whole export directory by RVA gives
// exactly one name at 0x180024350 (and at each of the other eight addresses
// this file registers), so the RegisterRuntimeAddr below binds one function,
// not a folded group.
//
// ORIGINAL SOURCE FILE. The recovered tree (docs/runtime_port/
// original_source_tree.txt) has no Stream.cpp and no
// CompressedMemoryStream.cpp: this class lives in IO\CompressedStream.cpp and
// IO::Stream / IO::MemoryStream in IO\MemoryStream.cpp. This file was
// scaffolded as "stream.cpp" and RENAMED to compressedstream.cpp by the
// orchestrator on lane B's recommendation, because the recovered tree has a
// SEPARATE IO\Stream.cpp for the IO::Stream base -- keeping the old name would
// have collided with it when that base is ported. The old name was the
// lane's file name, not the engine's.
//
// WHAT IS *NOT* PORTED, AND WHY THE CODEC IS SAFE
// -----------------------------------------------
// Every one of these six is a thin shim over IO::CompressedStream, and
// CompressedStream is where the actual compression lives (Commit,
// CompressWindow, DecompressFromSource, SetWindow, and through them
// System::Compression::Compress / Decompress). NONE of that is reimplemented
// here. Each shim calls the same exported CompressedStream / MemoryStream entry
// point retail calls, with the same arguments:
//
//   ??0CompressedStream@IO@@QEAA@XZ                     0x1800aa3d0
//   ??1CompressedStream@IO@@UEAA@XZ                     0x1800aa650
//   ?Commit@CompressedStream@IO@@QEAAXXZ                0x1800a9ee0
//   ?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z 0x1800aa4b0
//   ?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z   0x1800aa560
//   ??0MemoryStream@IO@@QEAA@XZ                         0x1800ae710
//   ??1MemoryStream@IO@@UEAA@XZ                         0x1800ae890
//
// so the compressed byte format is bit-identical by construction. Porting
// CompressedStream itself is a later wave's job.
//
// VTABLE OWNERSHIP — the argument for THIS class
// ----------------------------------------------
// ??1CompressedMemoryStream and ?Close are both virtual (UEAA), so PLAN §4.3
// risk 3 applies and WAVE3.md finding 4 says in capitals that Wave 3's
// exported-vtable argument is PER CLASS. Re-made here from scratch:
//
//  (a) ??_7CompressedMemoryStream@IO@@6B@ IS an exported data symbol
//      (0x180155560, ordinal 1409), as are ??_7MemoryStream@IO@@6B@
//      (0x1801555b0, 1465) and ??_7Stream@IO@@6B@ (0x1801549a0, 1506). Verified
//      with pefile against C:\GOG Games\Banished\Runtime-x64.dll. The ported
//      ctor and dtor therefore stamp the DLL'S OWN vtable, resolved by name.
//      We never build a private copy, so there is exactly ONE vtable and ONE
//      RTTI record for this class in the process. That is the whole of the
//      "two vtables in play" hazard, and it cannot arise.
//
//  (b) Consequence: slot 0 (the deleting-dtor thunk 0x1800244a0) and slot 7
//      (Close, 0x1800a9620) still point at the DLL's bodies. Every VIRTUAL
//      dispatch keeps landing in the DLL; the ports below are reached only
//      through our own direct, resolved-pointer calls. Both bodies are
//      byte-faithful reimplementations of each other over an identical layout,
//      so which copy runs is unobservable — but it does mean the diff oracle
//      only ever sees OUR callers, and the census will overstate coverage if
//      read as "the DLL now uses ours".
//
//  (c) No object crosses the boundary. Our only caller builds its
//      CompressedMemoryStream in a stack buffer (`alignas(16) uint8_t
//      cms[0x100]`) and calls ctor / Create / Open / Close / GetMemoryStream /
//      dtor through resolved function pointers — never `new`, never `delete`,
//      never a virtual call. The DLL's own uses are self-contained: the only
//      internal code xrefs to any of the six are to the dtor, from
//      ReadPackfile (0x1800b6054, IO::PackageFile), from the deleting-dtor
//      thunk, and from four SEH unwind funclets — every one of them on an
//      object the DLL both built and destroys itself.
//
//  (d) Therefore the class is NOT ported "half-way" in the sense PLAN §4.1
//      decision 5 warns about. The unported members (the copy ctor,
//      operator=, Open(void*,int), PreCommit, and the whole CompressedStream
//      base) continue to operate on the same single layout and the same single
//      vtable. Nothing observable depends on which copy of a body runs.
//
// This argument turns entirely on (a). If ??_7CompressedMemoryStream@IO@@6B@
// were not exported the honest move would be to hold the two virtuals back, and
// the lane doc says so.
//
// DIFF ORACLES — see docs/runtime_port/WAVE4_LANE_B.md §5 for the full
// can/cannot table. Two of the six get a real oracle (the ctor, by constructing
// the original into a scratch buffer and comparing all 0x98 bytes; and
// GetMemoryStream, which is pure). The other four allocate or mutate, so
// running the original alongside would construct twice or double-commit; they
// get shadow post-state assertions and are labelled PARTIAL, not proved.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../funcs/rt_port.h"
#include "../../types/io/Stream.h"

using RtIo::CompressedMemoryStream;
using RtIo::CompressedStream;
using RtIo::MemoryStream;
using RtIo::Stream;

namespace {

// ---- The entry points these shims forward to -------------------------------
// Every one is looked up ONCE per call site. ResolveExportRaw/ResolveExport is
// GetProcAddress / a registry probe (rt_port.h): a bare RtOriginal in a body
// cost Wave 2 five points of sim time. These are cold paths — a save happens
// once every few minutes — but the rule is the rule.
//
// WAVE 9 LANE V2, 2026-09-07: the ORACLE sites (the scratch-object ctor/dtor
// at 235/263, both inside `if (rt_diff_on__)`) stay on RT_ORIGINAL_ONCE — that
// is the whole point of them. The sites the LIVE bodies execute — the base and
// member constructors, the two base destructors, and Commit — are REAL
// DEPENDENCIES and moved to RT_EXPORT_ONCE. None of them names the symbol its
// own enclosing port registers (CompressedMemoryStream's ctor/dtor call
// CompressedStream's and MemoryStream's), so there is no self-recursion.
// The Create / Open pair below were NOT among rawdoor_audit's findings and are
// converted anyway: the resolve and its argument list are on separate SOURCE
// LINES, and the checker's immediate-call regex is per-line. They are the same
// live-body real dependency as the rest, found by reading the file rather than
// by the checker — a clean run is a floor, not a ceiling.
// Raw here would be a NULL call the instant Runtime-x64.dll is absent; through
// the registry it is instead a named unresolved import if the neighbour is
// still unported at cutover, which is a work-list row rather than a crash.

using CtorFn      = void*(*)(void*);          // ctor, returns `this` in RAX
using DtorFn      = void (*)(void*);
using VoidThisFn  = void (*)(void*);
using CsCreateFn  = bool (*)(void*, void*, int);
using StreamVFn   = void (*)(void*);          // vtable slot 7, Close()

// The three exported vtable data symbols. Resolved once each; the value is the
// address the retail ctor/dtor store, i.e. slot 0, not the RTTI word before it.
const void* VtblCompressedMemoryStream() {
    static const void* v = ResolveExport("??_7CompressedMemoryStream@IO@@6B@");
    return v;
}
const void* VtblStream() {
    static const void* v = ResolveExport("??_7Stream@IO@@6B@");
    return v;
}

// The embedded MemoryStream's Close(), dispatched through its own vtable at
// +0x38 exactly as retail does (`MOV RAX,[RDI]; CALL [RAX+0x38]`). Deliberately
// NOT a direct call to ?Close@MemoryStream: retail dispatches virtually and a
// subclass could in principle sit there.
void CallStreamCloseVirtual(MemoryStream* ms) {
    const void* const* vtbl = *reinterpret_cast<const void* const* const*>(ms);
    reinterpret_cast<StreamVFn>(
        const_cast<void*>(vtbl[RtIo::kStreamVtblCloseSlot]))(ms);
}

}  // namespace

// ============================================================================
// ??0CompressedMemoryStream@IO@@QEAA@XZ   retail 0x1800a9520, profile 0x18005c240
// ============================================================================
// Retail, disassembled:
//     CALL 0x1800aa3d0            ; CompressedStream::CompressedStream(this)
//     LEA  RAX,[0x180155560]      ; &CompressedMemoryStream::_vftable_
//     MOV  [RBX],RAX
//     LEA  RCX,[RBX+0x68]
//     CALL 0x1800ae710            ; MemoryStream::MemoryStream(this+0x68)
//     MOV  RAX,RBX                ; MSVC ctors return `this`
//
// The order is load-bearing: the base ctor stamps CompressedStream's vtable,
// we overwrite it with ours, and only THEN is the member constructed — so a
// hypothetical virtual call out of MemoryStream's ctor would see the derived
// vtable. The profile build (0x18005c240) has the identical three steps with
// nothing inlined, which is the independent witness for the +0x68 offset.
//
// No new state is introduced here; every field this object owns is written by
// one of the two base/member constructors.
extern "C" CompressedMemoryStream*
RtIo_CompressedMemoryStream_Ctor(CompressedMemoryStream* self) {
    RT_DIFF_BEGIN("IO::CompressedMemoryStream::CompressedMemoryStream");

    RT_EXPORT_ONCE(CtorFn, "??0CompressedStream@IO@@QEAA@XZ")(&self->base);
    *reinterpret_cast<const void**>(self) = VtblCompressedMemoryStream();
    RT_EXPORT_ONCE(CtorFn, "??0MemoryStream@IO@@QEAA@XZ")(&self->memoryStream);

    // ---- FULL ORACLE ------------------------------------------------------
    // Construction is the one member of this family that CAN be diffed against
    // the original for real: build theirs into a private scratch buffer, compare
    // all 0x98 bytes, then destroy the scratch. Nothing live is touched.
    //
    // The two IO::Encoding pointers (+0x10 on the CompressedStream base and
    // +0x78 on the embedded MemoryStream) are fresh heap allocations and CANNOT
    // match, so each is replaced in the comparison copies by the first qword of
    // the object it points at — the Encoding vtable. That still catches the
    // failure that matters (a stream whose encoding is null, or is not the
    // shared 0x1801554a8 BinaryEncoding) while ignoring the address.
    if (rt_diff_on__) {
        // Compare only the bytes the two ctors ACTUALLY WRITE. The rest is
        // padding they leave alone, and comparing it pits a zeroed scratch
        // buffer against the caller's uninitialised stack
        // (gameupdateloop_serialize.cpp's `alignas(16) uint8_t cms[0x100]`),
        // which is an artifact of the harness, not a difference between the two
        // implementations.
        //
        // The set below is READ OFF THE SHIPPED BYTES of both constructors, in
        // full, rather than discovered one divergence at a time:
        //   ??0CompressedStream (0x1800aa3d0) writes +0x00 q, +0x08 d, +0x10 q,
        //     +0x1c d, +0x20 d, then +0x28..+0x60 q  -> gaps 0x0c-0x0f,
        //     0x18-0x1b, 0x24-0x27
        //   ??0MemoryStream    (0x1800ae710) writes +0x00 q, +0x08 d, +0x10 q,
        //     +0x18 q, +0x20 BYTE, +0x24 q        -> gaps rel 0x0c-0x0f,
        //     0x21-0x23, 0x2c-0x2f, i.e. 0x74-0x77, 0x89-0x8b, 0x94-0x97 at
        //     the embedded sub-object's base of 0x68
        //
        // TWO EARLIER ATTEMPTS AT THIS ARE WORTH KNOWING ABOUT. Excluding the
        // gaps one at a time as the integration run reported them fixed 0x0c
        // and then immediately failed at 0x18. Deriving the mask at runtime by
        // constructing into a 0xff-filled buffer as well as a 0x00-filled one
        // looked elegant and CRASHED THE GAME: the ctors and the matching dtor
        // act on fields they never initialise, so handing them 0xff bytes hands
        // them garbage pointers to free. Do not reintroduce either.
        static const struct { unsigned short off, len; } kGaps[] = {
            {0x0c, 4}, {0x18, 4}, {0x24, 4}, {0x74, 4}, {0x89, 3}, {0x94, 4},
        };

        alignas(16) unsigned char theirs[sizeof(CompressedMemoryStream)];
        std::memset(theirs, 0, sizeof(theirs));
        RT_ORIGINAL_ONCE(CtorFn, "??0CompressedMemoryStream@IO@@QEAA@XZ")(theirs);

        alignas(16) unsigned char a[sizeof(CompressedMemoryStream)];
        alignas(16) unsigned char b[sizeof(CompressedMemoryStream)];
        std::memcpy(a, self,   sizeof(a));
        std::memcpy(b, theirs, sizeof(b));

        // The two IO::Encoding pointers (+0x10 on the CompressedStream base and
        // +0x78 on the embedded MemoryStream) are fresh heap allocations and
        // CANNOT match, so each is replaced in the comparison copies by the
        // first qword of the object it points at -- the Encoding vtable. That
        // still catches the failure that matters (an encoding that is null, or
        // is not the shared 0x1801554a8 BinaryEncoding) while ignoring the
        // address.
        for (size_t off : {size_t(0x10), size_t(0x78)}) {
            for (unsigned char* p : {a, b}) {
                void* enc = nullptr;
                std::memcpy(&enc, p + off, sizeof(enc));
                const void* tag = enc ? *reinterpret_cast<const void* const*>(enc) : nullptr;
                std::memcpy(p + off, &tag, sizeof(tag));
            }
        }
        for (const auto& g : kGaps) {
            std::memset(a + g.off, 0, g.len);
            std::memset(b + g.off, 0, g.len);
        }
        RT_DIFF_BYTES(a, b, sizeof(a));

        RT_ORIGINAL_ONCE(DtorFn, "??1CompressedMemoryStream@IO@@UEAA@XZ")(theirs);
    }
    return self;
}

// ============================================================================
// ??1CompressedMemoryStream@IO@@UEAA@XZ   retail 0x1800a9560, profile 0x18005c280
// ============================================================================
// The profile build gives the SOURCE shape, with everything out of line:
//
//     vptr = &CompressedMemoryStream::_vftable_;
//     CompressedStream::Close();          // the base's Close, not ours
//     _memoryStream.Close();              // virtual, vtable +0x38
//     _memoryStream.~MemoryStream();
//     ~CompressedStream();
//
// Retail (0x1800a9560) is the same five steps with two things inlined:
//
//   * `CompressedStream::Close()` (0x1800aa8d0) is exactly
//     `if (_headerSize > 0 && _direction == Writing) <Commit body>`, so MSVC
//     collapsed it to the guard plus a CALL to the out-of-line copy of that
//     body, ?Commit@CompressedStream (0x1800a9ee0). We reproduce retail's
//     literal form — the guard, then Commit — rather than calling the exported
//     Close, so the two fields the guard reads stay visible and asserted.
//
//   * `~MemoryStream` (exported at 0x1800ae890) is inlined. Its four steps are
//     byte-for-byte the sequence at 0x1800a95a4..0x1800a95e0, so calling the
//     exported copy is behaviour-identical.
//
// ONE DELIBERATE DEVIATION, and it is not a behavioural one. Retail frees the
// two IO::Encoding objects through FUN_180006a10, and the MemoryStream buffer
// through 0x1800c7ad0 = ?MemFree@MemoryAllocator@System@@SAXPEAX@Z. Those two
// addresses hold the SAME FUNCTION: 96 bytes read out of each differ only in
// the RIP-relative displacements (`4885c97456574883ec3048c7442420feffffff...`
// at both). FUN_180006a10 is a second, un-exported copy of MemFree that the
// linker did not fold. Since we reach both through the exported ~MemoryStream
// and ~CompressedStream, nothing in this file has to name either address.
//
// NO NULL GUARDS ADDED. Retail dereferences self and self->memoryStream's vptr
// unconditionally and so do we (CLAUDE.md: a faithful crash is upstream's bug).
extern "C" void RtIo_CompressedMemoryStream_Dtor(CompressedMemoryStream* self) {
    RT_DIFF_BEGIN("IO::CompressedMemoryStream::~CompressedMemoryStream");

    *reinterpret_cast<const void**>(self) = VtblCompressedMemoryStream();

    // Inlined CompressedStream::Close(): flush the pending windows if this
    // stream was written to at all. headerSize is Array<Header>::_size at +0x48.
    if (self->base.headerSize > 0 && self->base.base.direction == 1)
        RT_EXPORT_ONCE(VoidThisFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(&self->base);

    CallStreamCloseVirtual(&self->memoryStream);
    RT_EXPORT_ONCE(DtorFn, "??1MemoryStream@IO@@UEAA@XZ")(&self->memoryStream);
    RT_EXPORT_ONCE(DtorFn, "??1CompressedStream@IO@@UEAA@XZ")(&self->base);

    // ---- PARTIAL (shadow) --------------------------------------------------
    // A destructor cannot be diffed by running both: the second run would free
    // the same blocks twice. What IS checkable is the post-state the two base
    // destructors are supposed to leave behind — both sub-objects downgraded to
    // IO::Stream's vtable. That verifies the teardown ran to the end; it does
    // NOT verify the Commit decision, which reads the same two fields in our
    // code and retail's and is therefore tautological here.
    RT_DIFF_SHADOW(*reinterpret_cast<const void**>(self) == VtblStream() &&
                       self->memoryStream.base.vptr == VtblStream(),
                   "both sub-objects left with IO::Stream vtable",
                   "expected IO::Stream vtable");
}

// ============================================================================
// ?Create@CompressedMemoryStream@IO@@QEAA_NXZ    retail 0x1800a9600
// ============================================================================
// Whole body: `return CompressedStream::Create(*this, this->_memoryStream, 2);`
// — a tail call to 0x1800aa4b0 with the embedded MemoryStream as the sink and a
// window count of 2. That base routine sets _direction = 1 (writing), stores
// _source, news up a System::Compression and calls SetWindowCount(2).
extern "C" bool RtIo_CompressedMemoryStream_Create(CompressedMemoryStream* self) {
    RT_DIFF_BEGIN("IO::CompressedMemoryStream::Create");

    const bool ok = RT_EXPORT_ONCE(
        CsCreateFn, "?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z")(
        &self->base, &self->memoryStream, 2);

    // ---- PARTIAL (shadow) --------------------------------------------------
    // Calling the original a second time would allocate a second Compression
    // object, leak the first and re-run SetWindowCount, so there is no
    // call-both oracle here. What the shadow proves is that we handed the base
    // routine the arguments retail hands it: the post-state pins the direction
    // (1 == writing), the sink (the embedded MemoryStream, not some other
    // stream) and the literal window count of 2, which is the only number in
    // this function.
    RT_DIFF_SHADOW(ok && self->base.base.direction == 1 &&
                       self->base.source ==
                           reinterpret_cast<Stream*>(&self->memoryStream) &&
                       self->base.windowSize == 2 &&
                       self->base.compression != nullptr,
                   "dir=1 source=&_memoryStream windows=2 compression!=0",
                   "CompressedStream::Create(*this,_memoryStream,2)");
    return ok;
}

// ============================================================================
// ?Open@CompressedMemoryStream@IO@@QEAA_NXZ      retail 0x1800a9500
// ============================================================================
// Whole body: `return CompressedStream::Open(*this, this->_memoryStream, 1);`
// — 0x1800aa560, window count 1. That routine sets _direction = 0 (reading),
// stores _source, calls SetWindowCount(1) and then parses the window header
// table out of the source stream, returning false if the totals do not add up.
//
// Note the asymmetry with Create and DO NOT "fix" it: writing uses two windows,
// reading one. Both literals are read straight off the two call sites.
extern "C" bool RtIo_CompressedMemoryStream_Open(CompressedMemoryStream* self) {
    RT_DIFF_BEGIN("IO::CompressedMemoryStream::Open");

    const bool ok = RT_EXPORT_ONCE(
        CsCreateFn, "?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z")(
        &self->base, &self->memoryStream, 1);

    // ---- PARTIAL (shadow) --------------------------------------------------
    // Same reason as Create: the base routine consumes bytes from the source
    // stream, so a second call would read from a moved cursor. `ok` is
    // deliberately NOT asserted — Open legitimately returns false on a stream
    // whose header table does not check out, and the caller (SaveRestore) does
    // not test it either. _direction, _source and the window count are set
    // before any early return, so they hold on both outcomes.
    RT_DIFF_SHADOW(self->base.base.direction == 0 &&
                       self->base.source ==
                           reinterpret_cast<Stream*>(&self->memoryStream) &&
                       self->base.windowSize == 1,
                   "dir=0 source=&_memoryStream windows=1",
                   "CompressedStream::Open(*this,_memoryStream,1)");
    return ok;
}

// ============================================================================
// ?Close@CompressedMemoryStream@IO@@UEAAXXZ      retail 0x1800a9620
// ============================================================================
// The first two lines of the destructor, and nothing else:
//     if (_headerSize > 0 && _direction == Writing) Commit();   // inlined
//                                                               // CompressedStream::Close
//     _memoryStream.Close();                                    // virtual, +0x38
// Retail tail-JMPs the virtual call, which is why Ghidra reports "could not
// recover jumptable" at 0x1800a9647; it is an ordinary tail call, not a switch.
//
// This is the function that turns the written windows into the compressed byte
// image sitting in _memoryStream, so SaveRestore must call it before Store.
extern "C" void RtIo_CompressedMemoryStream_Close(CompressedMemoryStream* self) {
    RT_DIFF_BEGIN("IO::CompressedMemoryStream::Close");

    const bool committed =
        self->base.headerSize > 0 && self->base.base.direction == 1;
    if (committed)
        RT_EXPORT_ONCE(VoidThisFn, "?Commit@CompressedStream@IO@@QEAAXXZ")(&self->base);

    CallStreamCloseVirtual(&self->memoryStream);

    // ---- PARTIAL (presence only) -------------------------------------------
    // Be honest about this one: it proves nothing about equivalence. Calling the
    // original afterwards would Commit a second time and corrupt the save, and
    // the Commit decision reads the same two fields in both implementations, so
    // asserting it here is circular. The site exists so the summary's `calls=`
    // column shows the save path was actually taken and reports which branch it
    // took; the equivalence argument for this body rests on the decompile.
    RT_DIFF_SHADOW(true,
                   committed ? "Commit() taken" : "Commit() skipped (empty or reading)",
                   "presence only -- no oracle, see WAVE4_LANE_B.md");
}

// ============================================================================
// ?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAVMemoryStream@2@XZ
//                                                retail 0x180024350
// ============================================================================
// `LEA RAX,[RCX+0x68]; RET`. The mangling is AEAV — a REFERENCE return, which on
// the MSVC x64 ABI is a plain pointer in RAX. It is NOT a by-value class return,
// so there is no hidden return slot in RDX and the fn_* typedef takes exactly
// one argument. (Checked against the mangled name rather than assumed:
// reference_hidden_retval_and_int_pointer_args.md.)
extern "C" MemoryStream*
RtIo_CompressedMemoryStream_GetMemoryStream(CompressedMemoryStream* self) {
    RT_DIFF_BEGIN("IO::CompressedMemoryStream::GetMemoryStream");

    MemoryStream* ours = &self->memoryStream;

    // ---- FULL ORACLE -------------------------------------------------------
    // Pure: no reads of mutable state, no writes, no allocation. Calling the
    // original on the live object is free of side effects.
    using GetMsFn = MemoryStream*(*)(CompressedMemoryStream*);
    RT_DIFF_SCALAR(static_cast<const void*>(ours),
                   static_cast<const void*>(
                       RtOriginal<GetMsFn>(
                           "?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAV"
                           "MemoryStream@2@XZ")(self)));
    return ours;
}

// ============================================================================
// Registration — called from RtIoPortInit() under BANISHED_RT_STREAM.
// ============================================================================
// ============================================================================
// Wave 9 lane W — IO::MemoryStream::Open(int) and IO::MemoryStream::~MemoryStream
//
//   0x1800ae670  ?Open@MemoryStream@IO@@QEAA_NH@Z   ord 3305
//   0x1800ae890  ??1MemoryStream@IO@@UEAA@XZ        ord 631
//
// Both are Wave 9 lane V rows: REAL DEPENDENCY (MIGRATED). Lane V2 moved their
// call sites (saveload.cpp:832, compressedstream.cpp:330) from the raw door to
// RT_EXPORT_ONCE, which is correct and NOT sufficient — with no port and no
// mint entry `ResolveExport` misses the registry and falls through to
// `ResolveExportRawImpl`, so the request only changed which residue list prints
// it. These two close the row for real.
//
// THE THIRD MEMBER OF THE ROW, ??0MemoryStream, IS **NOT** PORTED HERE AND THE
// REASON IS A FACT ABOUT THE BINARY, NOT AN ESTIMATE. Every concrete Stream
// constructor in this family allocates an 8-byte IO::Encoding and stores ONE
// pointer into it: the shared encoding vtable at **0x1801554a8** (see
// RtIo_CompressedMemoryStream_Ctor's oracle, which already special-cases that
// address). That address is **not an exported symbol** — checked against the
// DLL's export directory BY ADDRESS, not by name pattern; 0x1801555b0
// (??_7MemoryStream@IO@@6B@) is exported and 0x1801554a8 has no export at all,
// and it is not merely the interior of a bigger table either: 0x180155488..
// 0x1801554a0 are four _purecall slots belonging to the abstract base, so
// 0x1801554a8 is where the CONCRETE encoding's table begins. A ported ctor must
// obtain that pointer, and at cutover there is no name for `ResolveExport` to
// take and no row the mint can carry, because the mint is keyed by exported
// symbol. So ??0MemoryStream and ??0CompressedStream are blocked on EXTENDING
// THE MINT TO UNEXPORTED VTABLES, or on identifying the concrete Encoding class
// and rebuilding its table slot by slot — lane C2's shape of work, not a port.
// Named rather than defaulted; see WAVE9_LANE_W.md.
//
// Neither function below touches the encoding's CONTENTS, only its address, so
// neither inherits that blocker.
// ============================================================================

// The four fields ~MemoryStream and Open need, plus this class's own vtable.
// ??_7MemoryStream@IO@@6B@ (0x1801555b0) is exported, so it goes through the
// PORT-AWARE door for the reason mesh.cpp's RtVtable() records: the raw door is
// GetProcAddress, which returns null at cutover for a symbol the mint carries
// correctly just as much as for one it has never heard of.
static const void* VtblMemoryStream() {
    static const void* v = ResolveExport("??_7MemoryStream@IO@@6B@");
    return v;
}

using MemAllocFn = void* (*)(uint64_t, int);
using MemFreeFn  = void  (*)(void*);

// ??1MemoryStream's body, factored out so the oracle below can run it on a
// scratch object WITHOUT re-entering the diff guard. Calling the registered
// entry point recursively would recurse forever — the same shape lane V2 caught
// in widgets_layout.cpp's RemoveGraphs, where a blanket door conversion would
// have made a function resolve to itself.
static void MemoryStreamDtorBody(MemoryStream* self) {
    self->base.vptr = VtblMemoryStream();
    if (self->ownsData && self->data != nullptr)
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(self->data);
    self->base.vptr = VtblStream();
    if (self->base.encoding != nullptr)
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(self->base.encoding);
}

// ?Open@MemoryStream@IO@@QEAA_NH@Z — 0x1800ae670.
//
// Retail, in order: MemAlloc(size,4) -> +0x18; size -> +0x28; 0 -> +0x24
// (_cursor); 0 -> +0x08 (_direction, i.e. READING); 1 -> +0x20 (_ownsData);
// return true UNCONDITIONALLY — including when the allocation failed. That last
// part is retail's and it is reproduced: a caller that trusts the `true` and
// writes into a null buffer is retail's behaviour, and a guard here would be
// the one CLAUDE.md warns against rather than a repair.
//
// The lock/Allocate/unlock trio the decompile shows inlined at this call site is
// instruction-for-instruction `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`
// (0x1800c7a50: EnterCriticalSection(_singleton+0x640); Allocate(_singleton, n,
// align); LeaveCriticalSection) — compared, not assumed — so calling MemAlloc
// through the port-aware door is both the faithful spelling and the one that
// follows lane E's allocator when it arms.
//
// ORACLE: FULL, by replay into a scratch object. Open ALLOCATES, so running
// theirs over the live object would leak the buffer ours just made and leave
// the object pointing at a second one. Instead a scratch MemoryStream is built
// with the DLL's own constructor, THEIR Open is run on it with the same size,
// and every field the function writes except the buffer pointer is compared —
// _size, _cursor, _direction, _ownsData, and the return value: all four
// non-pointer stores. Two independent allocations cannot share an address, so
// comparing _data would fail 100% of calls for a reason that is not a defect
// (reference_oracle_must_not_compare_unwritten_bytes.md, from the other side).
// The scratch is destroyed with the DLL's own destructor, so nothing leaks and
// nothing live is touched. Every call reaching the guard does the comparison,
// so `calls=` counts work rather than entry.
extern "C" bool RtIo_MemoryStream_Open(MemoryStream* self, int size) {
    RT_DIFF_BEGIN("IO::MemoryStream::Open");

    self->data = RT_EXPORT_ONCE(MemAllocFn,
        "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z")(static_cast<uint64_t>(size), 4);
    self->size = static_cast<uint32_t>(size);
    self->cursor = 0;
    self->base.direction = 0;
    self->ownsData = 1;

    if (rt_diff_on__) {
        alignas(16) unsigned char scratch[sizeof(MemoryStream)];
        std::memset(scratch, 0, sizeof(scratch));
        RT_ORIGINAL_ONCE(CtorFn, "??0MemoryStream@IO@@QEAA@XZ")(scratch);
        MemoryStream* s = reinterpret_cast<MemoryStream*>(scratch);

        using OpenFn = bool (*)(void*, int);
        const bool theirsOk =
            RT_ORIGINAL_ONCE(OpenFn, "?Open@MemoryStream@IO@@QEAA_NH@Z")(s, size);

        const bool eq = theirsOk &&
                        s->size == self->size &&
                        s->cursor == self->cursor &&
                        s->base.direction == self->base.direction &&
                        s->ownsData == self->ownsData;
        char a[160], b[160];
        std::snprintf(a, sizeof(a), "ret=1 size=%u cursor=%u dir=%d owns=%u",
                      self->size, self->cursor, self->base.direction, self->ownsData);
        std::snprintf(b, sizeof(b), "ret=%d size=%u cursor=%u dir=%d owns=%u",
                      theirsOk ? 1 : 0, s->size, s->cursor, s->base.direction, s->ownsData);
        RT_DIFF_SHADOW(eq, a, b);

        RT_ORIGINAL_ONCE(DtorFn, "??1MemoryStream@IO@@UEAA@XZ")(s);
    }
    return true;
}

// ??1MemoryStream@IO@@UEAA@XZ — 0x1800ae890.
//
// Retail, in order:
//     vptr = &MemoryStream::_vftable_      // the MSVC "run the dtor under this
//                                          // class's own vtable" idiom
//     if (_ownsData && _data) MemFree(_data);
//     vptr = &Stream::_vftable_            // the inlined ~Stream
//     if (_encoding)          MemFree(_encoding);
//
// It writes exactly ONE field — the vptr, twice — and frees at most two blocks.
// _data and _encoding are left holding freed pointers; retail does not null
// them and neither does MemoryStreamDtorBody above. The object is dead, and
// nulling would show up as a divergence against the shipped bytes for no gain.
//
// The two MemFree calls are the inlined `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`
// (0x1800c7ad0), whose whole body is `if (p) { lock; Free(_singleton,p); unlock; }`.
// Retail reaches it through FUN_180006a10, a SECOND un-exported copy of those
// same instructions — this file's comment at :313 already records that pair.
// Calling the exported name is the same code and is the one lane E's allocator
// port replaces.
//
// ORACLE: A/B ON TWO SCRATCH OBJECTS, and why it is not a replay is the point.
// A destructor cannot be replayed: running theirs after ours double-frees both
// blocks, and suppressing the frees (nulling _data and _encoding first, the way
// this wave does for Video::ImageBuffer::PlatformDestruct) would leave nothing
// to compare, because the only field either body writes is the vptr and it is
// written unconditionally. So two identical scratch streams are built with the
// DLL's constructor and OPENED with the DLL's Open — which sets _ownsData, so
// the branch that actually matters runs on both sides. That is the
// EFFECTIVENESS control, not decoration: without the Open, both bodies would
// skip the first free and the comparison would be green over an untaken branch
// (reference_probe_needs_an_effectiveness_control.md). Ours destroys one,
// theirs destroys the other, and the post-states are compared on every byte
// except the two heap pointers.
//
// WHAT THIS DOES NOT PROVE, stated rather than implied: that both bodies freed
// the same blocks. Two frees per side are not observable from here. What IS
// checked is the branch decision that selects them and the vptr sequence — the
// two things a wrong destructor gets wrong, and the pair that
// reference_phantom_dtor_mangling_qeaa_vs_ueaa.md is about.
extern "C" void RtIo_MemoryStream_Dtor(MemoryStream* self) {
    RT_DIFF_BEGIN("IO::MemoryStream::~MemoryStream");

    alignas(16) unsigned char oursScratch[sizeof(MemoryStream)];
    alignas(16) unsigned char theirsScratch[sizeof(MemoryStream)];
    if (rt_diff_on__) {
        using OpenFn = bool (*)(void*, int);
        for (unsigned char* p : {oursScratch, theirsScratch}) {
            std::memset(p, 0, sizeof(MemoryStream));
            RT_ORIGINAL_ONCE(CtorFn, "??0MemoryStream@IO@@QEAA@XZ")(p);
            RT_ORIGINAL_ONCE(OpenFn, "?Open@MemoryStream@IO@@QEAA_NH@Z")(p, 64);
        }
        MemoryStreamDtorBody(reinterpret_cast<MemoryStream*>(oursScratch));
        RT_ORIGINAL_ONCE(DtorFn, "??1MemoryStream@IO@@UEAA@XZ")(theirsScratch);
    }

    // EFFECTIVENESS CONTROL, on the LIVE object rather than the scratch. The
    // scratch pair is deliberately constructed so both frees always run, which
    // is what makes the A/B comparison meaningful -- but it says nothing about
    // whether the live stream this call is actually destroying was ever opened.
    // A run where every live call had _ownsData == false would report the same
    // green number over a branch the real objects never take.
    if (rt_diff_on__) {
        static RtDiffSite brOwned = { "IO::MemoryStream::~MemoryStream[live:owned buffer freed]", 0, 0, false, false };
        static RtDiffSite brNoBuf = { "IO::MemoryStream::~MemoryStream[live:no owned buffer -- that free unexercised]", 0, 0, false, false };
        static RtDiffSite brEnc   = { "IO::MemoryStream::~MemoryStream[live:encoding freed]", 0, 0, false, false };
        // Register FIRST and unconditionally -- RtDiffReport does not register
        // (rt_diff.cpp:82). See buffers.cpp's CountBranch for why that matters.
        RtDiffRegister(&brOwned); RtDiffRegister(&brNoBuf); RtDiffRegister(&brEnc);
        const bool owned = self->ownsData && self->data != nullptr;
        if (owned)               RtDiffReport(&brOwned, true, "taken", "taken");
        if (!owned)              RtDiffReport(&brNoBuf, true, "taken", "taken");
        if (self->base.encoding) RtDiffReport(&brEnc,   true, "taken", "taken");
    }

    MemoryStreamDtorBody(self);

    if (rt_diff_on__) {
        unsigned char a[sizeof(MemoryStream)], b[sizeof(MemoryStream)];
        std::memcpy(a, oursScratch, sizeof(a));
        std::memcpy(b, theirsScratch, sizeof(b));
        std::memset(a + 0x10, 0, 8);  std::memset(b + 0x10, 0, 8);   // _encoding
        std::memset(a + 0x18, 0, 8);  std::memset(b + 0x18, 0, 8);   // _data
        const bool eq = std::memcmp(a, b, sizeof(a)) == 0;
        const MemoryStream* o = reinterpret_cast<const MemoryStream*>(oursScratch);
        const MemoryStream* t = reinterpret_cast<const MemoryStream*>(theirsScratch);
        char da[160], db[160];
        std::snprintf(da, sizeof(da), "vptr=%p owns=%u cursor=%u size=%u",
                      o->base.vptr, o->ownsData, o->cursor, o->size);
        std::snprintf(db, sizeof(db), "vptr=%p owns=%u cursor=%u size=%u",
                      t->base.vptr, t->ownsData, t->cursor, t->size);
        RT_DIFF_SHADOW(eq, da, db);
    }
}

// RegisterRuntimeExport flips every ResolveExport call site in our tree; the
// paired RegisterRuntimeAddr covers anything that asks for the same function by
// Ghidra address. No detour is installed: per the vtable-ownership argument at
// the top of this file the DLL's own callers are self-contained, and a detour on
// a virtual whose vtable slot we deliberately leave pointing at the DLL would
// make the two paths disagree about which body runs for no benefit.
int RtIoCompressedStreamRegister() {
    int n = 0;

    RegisterRuntimeExport("??0CompressedMemoryStream@IO@@QEAA@XZ",
                          (void*)&RtIo_CompressedMemoryStream_Ctor); ++n;
    RegisterRuntimeAddr(0x1800a9520, (void*)&RtIo_CompressedMemoryStream_Ctor);

    RegisterRuntimeExport("??1CompressedMemoryStream@IO@@UEAA@XZ",
                          (void*)&RtIo_CompressedMemoryStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800a9560, (void*)&RtIo_CompressedMemoryStream_Dtor);

    RegisterRuntimeExport("?Create@CompressedMemoryStream@IO@@QEAA_NXZ",
                          (void*)&RtIo_CompressedMemoryStream_Create); ++n;
    RegisterRuntimeAddr(0x1800a9600, (void*)&RtIo_CompressedMemoryStream_Create);

    RegisterRuntimeExport("?Open@CompressedMemoryStream@IO@@QEAA_NXZ",
                          (void*)&RtIo_CompressedMemoryStream_Open); ++n;
    RegisterRuntimeAddr(0x1800a9500, (void*)&RtIo_CompressedMemoryStream_Open);

    RegisterRuntimeExport("?Close@CompressedMemoryStream@IO@@UEAAXXZ",
                          (void*)&RtIo_CompressedMemoryStream_Close); ++n;
    RegisterRuntimeAddr(0x1800a9620, (void*)&RtIo_CompressedMemoryStream_Close);

    RegisterRuntimeExport("?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAVMemoryStream@2@XZ",
                          (void*)&RtIo_CompressedMemoryStream_GetMemoryStream); ++n;
    RegisterRuntimeAddr(0x180024350, (void*)&RtIo_CompressedMemoryStream_GetMemoryStream);

    // ---- Wave 9 lane W ----
    RegisterRuntimeExport("?Open@MemoryStream@IO@@QEAA_NH@Z",
                          (void*)&RtIo_MemoryStream_Open); ++n;
    RegisterRuntimeAddr(0x1800ae670, (void*)&RtIo_MemoryStream_Open);

    RegisterRuntimeExport("??1MemoryStream@IO@@UEAA@XZ",
                          (void*)&RtIo_MemoryStream_Dtor); ++n;
    RegisterRuntimeAddr(0x1800ae890, (void*)&RtIo_MemoryStream_Dtor);
    // NOTE: ??_7MemoryStream@IO@@6B@ slot 0 is the DELETING destructor thunk
    // (0x1800242a0), NOT this body, so registering ??1 does not change what a
    // virtual `delete` dispatches to. That is the same vtable-ownership
    // argument this file makes at the top for CompressedMemoryStream, and it is
    // why no detour is installed here either.

    printf("[RtPort]   IO::CompressedMemoryStream: %d exports registered "
           "(SAVE PATH ONLY -- a plain autostart session never reaches them; "
           "needs a save or a load, see WAVE4_LANE_B.md)\n", n);
    return n;
}
