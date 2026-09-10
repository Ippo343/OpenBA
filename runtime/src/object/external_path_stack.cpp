// ============================================================================
// runtime/src/object/external_path_stack.cpp — Wave 11 lane OBJEXT
//   `Object::ExternalFactory::PushPath` / `PopPath`, the two WRITERS of the
//   path stack that Wave 9 lane RRES ported the READERS of.
// ============================================================================
//
//   ?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z  0x1800bd0e0
//   ?PopPath@ExternalFactory@Object@@QEAAXXZ                        0x1800bd040
//
// plus the four `Collection::Array<PathStackEntry>` bodies MSVC emitted for
// this one instantiation, which are what lane RRES named as its blocker:
//
//   0x18002e220  Array<PathStackEntry>::SetSize(n, const T& fill)
//   0x18002e8d0  elem-destroy(range)   — two Filenames per element
//   0x18002ccb0  RemoveLast(&out)      — copy the tail out, then RemoveAt
//   0x18002de30  RemoveAt(index)       — release, shift down, --size
//
// WHY THIS FILE EXISTS
// --------------------
// `runtime/src/object/external_path.h` states the deferral verbatim:
//
//     `PushPath` (0x1800bd0e0) and `PopPath` (0x1800bd040) are the WRITERS of
//     the path stack this file reads, and they are NOT ported. They need
//     `0x18002e220` — `Collection::Array<PathStackEntry>::Resize(n, const T&)`,
//     the 0x10-element instantiation, which exists nowhere in the tree — plus
//     its element copy-ctor and destroy-range.
//
// That blocker is discharged here, and NOT by writing a fourth copy of
// `SetSize`: Wave 9 lane Q already ported that body once, parameterised by
// `rtcoll::ArraySpec` (runtime/src/collection/array_internal.h), and
// `runtime/src/engine/mainloop_video.cpp` already sets the precedent of
// defining a spec OUTSIDE array.cpp for an instantiation that belongs to
// another lane's subject (`kSpec_VideoResource`). `kSpec_PathStack` below is
// that same move. `SetSizeCore` was read against 0x18002e220's disassembly
// arm by arm before it was reused — the `cap = n*3` growth rule when
// `n == old+1 && old != 0`, the `MemAlloc(cap*stride, align)` with align 8,
// the realloc arm's `destroy(old, oldSize)` before `MemFree`, and the
// non-realloc shrink arm's `destroy(old + newSize*stride, oldSize - newSize)`
// — all four agree. The two `Remove*` bodies are NOT SetSize and are written
// out here in full.
//
// A separate translation unit from `external_path_win.cpp` for the reason
// `rt_object_port.cpp` gives at the top: one lane owns one file. Nothing here
// needs <windows.h>, so this is the portable half and carries no `_win`.
//
// EVIDENCE
// --------
// Every body was transcribed from `disassemble_function(address=)` on
// Runtime-x64.dll (image base 0x180000000, read 2026-09-09), not from the
// decompiled C. Ghidra's C for `PushPath` is misleading in the usual way
// (reference_ghidra_c_output_vs_disassembly.md): it DROPS the right-hand
// operand of both `System::operator+` calls, so the two string literals the
// entry filename is built from — "." at 0x180155814 — are invisible in it, and
// it reports four "unreachable" blocks that are the ordinary release idiom.
//
// The two literals are read out of the image:
//   0x180155810 = "_"   0x180155814 = "."
// PushPath uses only the second.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "external_path.h"
#include "external_path_stack.h"

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/system/String.h"
#include "../system/string.h"
#include "../system/setupdir_helpers.h"
#include "../collection/array_internal.h"

namespace RtSystem {
// Declared locally, house style — filename.cpp exposes no header of its own.
// Identical spellings to runtime/src/object/external_path_win.cpp:88-95; the
// linker checks the two against each other.
void      RtSystem_Filename_AdjustDelimiters(Filename* self);              // 0x1800c6970
Filename* RtSystem_Filename_GetPathFilename(const Filename* self, Filename* ret,
                                            bool stripExtension);          // 0x1800c6740
Filename* RtSystem_Filename_CtorCopy(Filename* self, const Filename* other);
                                                                           // 0x1800067d0
}  // namespace RtSystem

namespace RtObject {

using RtSystem::Filename;
using RtSystem::String;
using RtSystem::RtString_AdoptInline;
using RtSystem::RtString_ReleaseInline;
using RtSystem::RtSystem_String_Concat;
using RtSystem::RtSystem_String_ConcatCStr;
using RtSystem::RtSystem_Filename_AdjustDelimiters;
using RtSystem::RtSystem_Filename_GetPathFilename;
using RtSystem::RtSystem_Filename_GetPathDirectory;
using RtSystem::RtSystem_Filename_CtorCopy;

namespace {

inline String*       AsStr(Filename* f)       { return reinterpret_cast<String*>(f); }
inline const String* AsStr(const Filename* f) { return reinterpret_cast<const String*>(f); }

// 0x180155814. A named constant so the negative control below can swap it for
// a token the oracle must reject.
const char* const kDot = ".";

// The release/adopt pair retail inlines at every `Filename x = y` site, with
// retail's own `!=` self-assign guard. Same body as external_path_win.cpp's;
// duplicated rather than shared because that file is another lane's and its
// copy is `static` inside an anonymous namespace.
inline void ReleaseThenAdopt(Filename* dst, char* src) {
    if (dst->data == src) return;
    RtString_ReleaseInline(AsStr(dst));
    RtString_AdoptInline(AsStr(dst), src);
}

// The negative control. `0` is the real body and is what every shipping and
// every measuring run uses. `BANISHED_RT_PATHSTACK_NEGCTL=<n>` forces one.
//   1  entry filename joined with "-" instead of "."
//   2  PushPath stores the entry's two Filenames in the WRONG ORDER
//   3  RemoveAt removes index+1 instead of index (guarded to stay in range)
//   4  SetSize's fill element is ignored (new tail left as the old bytes)
}  // namespace

// NOT in the anonymous namespace above, and not for style: the four container
// bodies at the bottom of this file are `extern "C"` at GLOBAL scope (see the
// note there), and a member of an anonymous namespace nested in `RtObject`
// cannot be named from outside `RtObject` at all.
int g_negCtl = 0;

// The probe MUST NOT run from the register hook. That runs immediately after
// ResolveInit()'s LoadLibrary, and the DLL's MemoryAllocator has been
// CONSTRUCTED by then but not STARTED — allocating from it at that moment is
// the "works on my boot order" hazard array_internal.h warns about, and this
// probe allocates on both sides. So it self-schedules: the flag is armed at
// registration and the first call into a registered entry point runs it, a
// moment that is provably safe because the caller is itself mid-Array.
bool g_probePending = false;


PathStackCounters g_psc = {};

// ---------------------------------------------------------------------------
// THE PROBE'S SCHEDULING SEAM — and why it is not in PushPath.
//
// The obvious place to self-schedule is the first call into a registered entry
// point. It does not work here, for a MEASURED reason: `PushPath` is never
// called on a normal run at all. Lane RRES instrumented the read side and
// reported `stack empty/nonempty = 22583/0` — 22,583 live `ExpandRelativePath`
// calls, every one of them against an EMPTY stack. A gate inside `PushPath`
// would therefore never fire, and the probe would silently never run while the
// registration line still printed "ARMED".
//
// So the gate lives on `FindOrLoad`, which does run (it is what drives those
// 22,583 calls) and which has the LIVE `ExternalFactory` in hand as its `this`
// — the object part 2 of the probe needs and must not fake.
// ---------------------------------------------------------------------------
void PathStackProbeGate(ExternalFactory* live) {
    if (!g_probePending) return;
    g_probePending = false;   // cleared BEFORE the run: the probe calls PushPath
                              // itself, so an un-cleared flag would recurse.
    RtObjectPathStackProbeRun(live);
}

// ============================================================================
// ?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z   0x1800bd0e0
// ============================================================================
// RCX = this, RDX = path. Retail, in listing order:
//
//   0x1800bd108  ext   = path.GetPathExtension()                 [RBP-0x30]
//   0x1800bd11a  fname = path.GetPathFilename(stripExtension=1)  [RBP-0x38]
//   0x1800bd12e  t1    = fname + "."                             [RBP-0x40]
//   0x1800bd13e  t2    = t1 + ext                                [RBP+0x38]
//   0x1800bd146  file  = adopt(t2); AdjustDelimiters(file)       [RBP+0x20]
//   0x1800bd16a  dir   = path.GetPathDirectory()                 [RBP+0x30]
//   0x1800bd177  entry.directory = Filename(dir)                 [RBP-0x18]
//   0x1800bd1bf  entry.filename  = release-then-adopt(file)      [RBP-0x10]
//   0x1800bd1cf  AdjustDelimiters(entry.filename)   TWICE
//   0x1800bd1de  SetSize(&this->_pathStack, size + 1, &entry)
//
// THE DOUBLE AdjustDelimiters IS RETAIL'S, NOT A TRANSCRIPTION SLIP. Two
// separate `LEA RCX,[RBP-0x10] ; CALL 0x1800c6970` at 0x1800bd1cb and
// 0x1800bd1d4. It is idempotent, so it has no observable effect — but it is a
// second pass over the buffer and it is what the bytes say, so it is kept.
// (`ExpandRelativePath` carries the same doubled call, which is corroboration
// that this is a source-level idiom of retail's and not a one-off.)
//
// `ext` is used, unlike in GetCompiledPath where the same call's result is
// computed and DROPPED — see external_compiled.cpp.
void RtObject_ExternalFactory_PushPath(ExternalFactory* self, const Filename* path) {
    RT_DIFF_BEGIN("Object::ExternalFactory::PushPath");
    ++g_psc.pushCalls;

    Filename ext{nullptr};                                       // [RBP-0x30]
    RtSystem_Filename_GetPathExtension(path, &ext);
    Filename fname{nullptr};                                     // [RBP-0x38]
    RtSystem_Filename_GetPathFilename(path, &fname, true);

    String t1{nullptr}, t2{nullptr};                             // -0x40 / +0x38
    RtSystem_String_ConcatCStr(&t1, AsStr(&fname), g_negCtl == 1 ? "-" : kDot);
    RtSystem_String_Concat(&t2, &t1, AsStr(&ext));

    Filename file{nullptr};                                      // [RBP+0x20]
    RtString_AdoptInline(AsStr(&file), t2.data);                 // 0x1800bd14a
    RtSystem_Filename_AdjustDelimiters(&file);                   // 0x1800bd15d

    Filename dir{nullptr};                                       // [RBP+0x30]
    RtSystem_Filename_GetPathDirectory(path, &dir);

    // The 0x10-byte local at [RBP-0x18]: two ADJACENT stack slots, which is
    // the WRITE-side pin for the PathStackEntry field order that
    // runtime/types/object/External.h records.
    PathStackEntry entry;
    RtSystem_Filename_CtorCopy(&entry.directory, &dir);          // 0x1800bd177
    entry.filename.data = nullptr;                               // 0x1800bd185 (RBX == 0)
    ReleaseThenAdopt(&entry.filename, file.data);                // 0x1800bd189-0x1800bd1c8
    RtSystem_Filename_AdjustDelimiters(&entry.filename);         // 0x1800bd1cf
    RtSystem_Filename_AdjustDelimiters(&entry.filename);         // 0x1800bd1d4  (yes, twice)

    RtObject_PathStackSetSize_18002e220(&self->_pathStack,
                                        self->_pathStack._size + 1, &entry);
    g_psc.depthAfterPush = self->_pathStack._size;

    // Retail's release order, reversed construction order.
    RtString_ReleaseInline(AsStr(&entry.filename));
    RtString_ReleaseInline(AsStr(&entry.directory));
    RtString_ReleaseInline(AsStr(&dir));
    RtString_ReleaseInline(AsStr(&file));
    RtString_ReleaseInline(&t2);
    RtString_ReleaseInline(&t1);
    RtString_ReleaseInline(AsStr(&fname));
    RtString_ReleaseInline(AsStr(&ext));

    RT_DIFF_WORK(true);
    // NO LIVE SHADOW. PushPath is a destructive mutator on one shared stack:
    // running theirs after ours would push the same entry TWICE and leave the
    // factory two deep where retail leaves it one. Verification is the
    // lockstep synthetic-subject probe in external_path_stack_probe.cpp — the
    // same instrument, and the same reason, as external_set.h records for the
    // resource tree's twelve Set bodies.
}

// ============================================================================
// ?PopPath@ExternalFactory@Object@@QEAAXXZ                        0x1800bd040
// ============================================================================
// RCX = this. Retail:
//
//   0x1800bd04d  RemoveLast(&this->_pathStack, &out)   out at [RSP+0x28]
//   0x1800bd066  release(out.filename)                      [RSP+0x30]
//   0x1800bd0??  release(out.directory)                     [RSP+0x28]
//
// The entry is copied OUT and then immediately thrown away, which looks
// pointless and is not: `RemoveAt` releases the array's own reference, so
// without the copy the last element's two buffers would be freed while
// `RemoveLast`'s caller still held nothing at all. It is retail's `T tmp =
// arr.RemoveLast();` with an unused `tmp`, and the refcount traffic is
// observable.
//
// NOTE THE EMPTY-STACK BEHAVIOUR IS FAITHFULLY UNGUARDED. On a zero-deep stack
// `idx` is -1 and retail reads `_data[-1]`. We do not "fix" that
// (CLAUDE.md, "Don't 'fix' a faithful crash with a null check"): every caller
// in the tree pops only what it pushed, and a guard here would hide the
// upstream imbalance rather than the crash.
void RtObject_ExternalFactory_PopPath(ExternalFactory* self) {
    RT_DIFF_BEGIN("Object::ExternalFactory::PopPath");
    ++g_psc.popCalls;

    PathStackEntry out;                                          // [RSP+0x28]
    RtObject_PathStackRemoveLast_18002ccb0(&self->_pathStack, &out);
    RtString_ReleaseInline(AsStr(&out.filename));                // 0x1800bd066
    RtString_ReleaseInline(AsStr(&out.directory));
    g_psc.depthAfterPop = self->_pathStack._size;

    RT_DIFF_WORK(true);
    // No live shadow, same reason as PushPath.
}

}  // namespace RtObject

// ============================================================================
// THE FOUR CONTAINER BODIES — GLOBAL SCOPE, ON PURPOSE
// ============================================================================
// They are `extern "C"` and external_path_stack.h declares them at global
// scope. Defining them inside `namespace RtObject` as well would give one
// C-linkage entity two declarations in two scopes; GCC then rejects every
// `&RtObject_PathStack*` in the registration hook as "address of overloaded
// function with no contextual type information". And `rtcoll` must be the REAL
// `rtcoll` — nested inside `RtObject` it is a different namespace with no
// `ArraySpec` and no `SetSizeCore` in it.
//
// Everything they touch is therefore spelled out. No `using` at global scope:
// runtime/types/common.h forward-declares GLOBAL structs called `Filename` and
// `String`, so an unqualified use here is AMBIGUOUS rather than wrong
// (runtime/src/engine/mainloop_video.cpp:261 records the same trap).
// ============================================================================

// ============================================================================
// 0x18002e8d0 — elem-destroy(range) for PathStackEntry
// ============================================================================
// Signature read off the listing, and it is the SAME not-the-obvious-one shape
// `rtcoll::ElemDestroy` already describes: RCX is stored to its home slot and
// never read, so the function takes (ignored, T* base, int32 count). Retail
// releases the SECOND Filename (+0x08, the filename) BEFORE the first (+0x00,
// the directory); the order is observable whenever the two share a buffer.
//
// One function, two jobs, deliberately: it IS `kSpec_PathStack`'s destroy arm
// and it IS the registered entry point for the address. Two spellings would be
// two places to drift (mainloop_video.cpp:249 makes the same call).
extern "C" void RtObject_PathStackDestroyRange_18002e8d0(void* ignoredThis,
                                                          uint8_t* base, int count) {
    (void)ignoredThis;
    for (int i = 0; i < count; ++i) {
        uint8_t* e = base + static_cast<int64_t>(i) * 0x10;
        ++RtObject::g_psc.elemsDestroyed;
        RtSystem::RtString_ReleaseInline(reinterpret_cast<RtSystem::String*>(e + 8));   // filename
        RtSystem::RtString_ReleaseInline(reinterpret_cast<RtSystem::String*>(e));       // directory
    }
}

namespace rtcoll {

// The element assignment retail's SetSize uses is a placement copy-CONSTRUCT of
// both halves (`Filename::Filename(dst, src)` twice, 0x1800067d0), not a
// release-then-adopt: the destination is raw MemAlloc memory on the realloc arm
// and freshly-vacated-by-destroy memory nowhere. It must NOT release `dst`.
static void AssignPathStackEntry(uint8_t* dst, const uint8_t* src) {
    if (RtObject::g_negCtl == 2) {
        // control 2: swap the two halves.
        RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(dst),
                                   reinterpret_cast<const RtSystem::Filename*>(src + 8));
        RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(dst + 8),
                                   reinterpret_cast<const RtSystem::Filename*>(src));
        return;
    }
    RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(dst),
                               reinterpret_cast<const RtSystem::Filename*>(src));
    RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(dst + 8),
                               reinterpret_cast<const RtSystem::Filename*>(src + 8));
}

// stride 0x10, align 8 — the `SHL RDX,0x4` and the `MOV EDX,0x8` at
// 0x18002e220's MemAlloc call site.
//
// byteCopy=false: the assignment bumps two refcounts, so SetSizeCore's
// "copied prefix equals the old buffer" check would be comparing pointers it
// has legitimately re-owned. assign covers the whole stride (both halves are
// written), so there is no unwritten window on either arm; `init` is null
// because retail has no fill-less arm here — PushPath always passes an entry.
// seedZero is the WHOLE element: every byte of it is a `char*` that the
// element ops dereference at `p[-1]` to bump a refcount, so the probe must
// never seed garbage into either half.
const ArraySpec kSpec_PathStack = {                // 0x18002e220
    "Array<Object::PathStackEntry:0x10>", 0x10, 8,
    &AssignPathStackEntry, nullptr, &RtObject_PathStackDestroyRange_18002e8d0,
    /*byteCopy        */ false,
    /*assign unwritten*/ 0, 0,
    /*init   unwritten*/ 0, 0,
    /*seed zero       */ 0x00, 0x10 };

// STATIC STORAGE DURATION IS LOAD-BEARING: RtDiffRegister keeps the POINTER and
// re-reads it at summary time, so a stack Site is reported as harness
// corruption (array_internal.h).
Site g_pathstack_18002e220 = {
    { "Collection::Array<PathStackEntry>::SetSize @0x18002e220", 0, 0, false, false },
    0x18002e220 };

}  // namespace rtcoll

// ============================================================================
// 0x18002e220 — Array<PathStackEntry>::SetSize(n, const T& fill)
// ============================================================================
extern "C" void RtObject_PathStackSetSize_18002e220(void* a, int n, const void* fill) {
    ++RtObject::g_psc.setSizeCalls;
    rtcoll::SetSizeCore(static_cast<rtcoll::ArrayHdr*>(a), n,
                        RtObject::g_negCtl == 4 ? nullptr : static_cast<const uint8_t*>(fill),
                        &rtcoll::kSpec_PathStack, &rtcoll::g_pathstack_18002e220);
}

// ============================================================================
// 0x18002de30 — Array<PathStackEntry>::RemoveAt(index)
// ============================================================================
// NOT a SetSize. Retail:
//
//   if (index < size) {
//       i = index;
//       do {
//           release(e[i].filename); release(e[i].directory);
//           next = i + 1;
//           if (next < size) { copy-construct e[i] from e[next]; }
//           i = next;
//       } while (i < size);
//   }
//   --size;                      // BOTH arms, hence unconditional here
//
// Two details that a paraphrase loses. (1) The `--size` sits in both the taken
// and the not-taken arm of the outer `if`, so a `RemoveAt` past the end still
// decrements — faithful, and load-bearing for `RemoveLast` on an empty stack.
// (2) The shift is a copy-CONSTRUCT into memory the two releases immediately
// above just vacated, not an assignment; using `ReleaseThenAdopt` here would
// double-release.
extern "C" void RtObject_PathStackRemoveAt_18002de30(void* arrVoid, int index) {
    rtcoll::ArrayHdr* a = static_cast<rtcoll::ArrayHdr*>(arrVoid);
    ++RtObject::g_psc.removeAtCalls;

    // CONTROL 3, and note WHERE it perturbs. It removes a DIFFERENT, valid
    // index -- it does not break the body's invariants.
    //
    // Its first form did: it skipped the shift-down, which leaves every
    // shifted-from slot holding pointers the loop had already released, and the
    // next pop then copy-constructs from freed memory and releases it again.
    // That is a DOUBLE FREE, and it took the process down inside
    // System::MemoryAllocator::Free (0x1800c6c00) on the measuring run rather
    // than reporting `control CAUGHT` -- taking control 4 and the whole of the
    // probe's part 2 with it. A negative control has to survive being wrong:
    // it must produce an observable, COMPARABLE divergence, not undefined
    // behaviour. See WAVE11_LANE_OBJEXT.md §10.
    if (RtObject::g_negCtl == 3 && index + 1 < a->_size) index = index + 1;

    if (index < a->_size) {
        int32_t i = index;
        do {
            uint8_t* e = a->_data + static_cast<int64_t>(i) * 0x10;
            RtSystem::RtString_ReleaseInline(reinterpret_cast<RtSystem::String*>(e + 8));
            RtSystem::RtString_ReleaseInline(reinterpret_cast<RtSystem::String*>(e));

            const int32_t next = i + 1;
            if (next < a->_size) {
                uint8_t* dst = a->_data + static_cast<int64_t>(i) * 0x10;
                // Retail tests the COMPUTED address, which can only be null
                // when _data is null and i == 0.
                if (dst != nullptr) {
                    const uint8_t* src = a->_data + static_cast<int64_t>(next) * 0x10;
                    ++RtObject::g_psc.elemsShifted;
                    RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(dst),
                                               reinterpret_cast<const RtSystem::Filename*>(src));
                    RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(dst + 8),
                                               reinterpret_cast<const RtSystem::Filename*>(src + 8));
                }
            }
            i = next;
        } while (i < a->_size);
        ++RtObject::g_psc.removeAtInRange;
    } else {
        ++RtObject::g_psc.removeAtPastEnd;
    }
    a->_size = a->_size - 1;
}

// ============================================================================
// 0x18002ccb0 — Array<PathStackEntry>::RemoveLast(&out)
// ============================================================================
// Copy-constructs the tail element into the caller's raw 0x10-byte slot, then
// RemoveAt(size - 1). Returns the slot in RAX, as every by-value-out helper in
// this DLL does.
extern "C" void* RtObject_PathStackRemoveLast_18002ccb0(void* arrVoid, void* outVoid) {
    rtcoll::ArrayHdr* a = static_cast<rtcoll::ArrayHdr*>(arrVoid);
    uint8_t* out = static_cast<uint8_t*>(outVoid);
    ++RtObject::g_psc.removeLastCalls;

    const int32_t idx = a->_size - 1;
    const uint8_t* e = a->_data + static_cast<int64_t>(idx) * 0x10;
    RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(out),
                               reinterpret_cast<const RtSystem::Filename*>(e));
    RtSystem::RtSystem_Filename_CtorCopy(reinterpret_cast<RtSystem::Filename*>(out + 8),
                               reinterpret_cast<const RtSystem::Filename*>(e + 8));
    RtObject_PathStackRemoveAt_18002de30(a, idx);
    return out;
}


// ============================================================================
// Registration
// ============================================================================
// At GLOBAL scope, matching the other five `RtObject*Register()` hooks
// rt_object_port.cpp declares.
int RtObjectExternalPathStackRegister() {
    using namespace RtObject;
    int n = 0;

    const char* nc = std::getenv("BANISHED_RT_PATHSTACK_NEGCTL");
    g_negCtl = nc ? std::atoi(nc) : 0;

    RegisterRuntimeExport("?PushPath@ExternalFactory@Object@@QEAAXAEBVFilename@System@@@Z",
                          (void*)&RtObject_ExternalFactory_PushPath); ++n;
    RegisterRuntimeAddr(0x1800bd0e0, (void*)&RtObject_ExternalFactory_PushPath);

    RegisterRuntimeExport("?PopPath@ExternalFactory@Object@@QEAAXXZ",
                          (void*)&RtObject_ExternalFactory_PopPath); ++n;
    RegisterRuntimeAddr(0x1800bd040, (void*)&RtObject_ExternalFactory_PopPath);

    // The four container bodies. ADDRESSES ONLY — `Collection::Array<T>` is
    // header-only and has no export in either build (WAVE9 lane Q). Registering
    // them is what keeps this port from being one that CALLS the DLL by
    // address: every address our own bodies jump to is an address we own.
    RegisterRuntimeAddr(0x18002e220, (void*)&RtObject_PathStackSetSize_18002e220);
    RegisterRuntimeAddr(0x18002e8d0, (void*)&RtObject_PathStackDestroyRange_18002e8d0);
    RegisterRuntimeAddr(0x18002ccb0, (void*)&RtObject_PathStackRemoveLast_18002ccb0);
    RegisterRuntimeAddr(0x18002de30, (void*)&RtObject_PathStackRemoveAt_18002de30);

    // Opt-in, not a kill switch: RtWaveEnabled returns true when the variable is
    // UNSET, which is right for a port and wrong for a probe.
    const char* pe = std::getenv("BANISHED_RT_PATHSTACK_PROBE");
    RtObjectPathStackProbeArm(pe != nullptr && std::strcmp(pe, "0") != 0);
    g_probePending = RtObjectPathStackProbeArmed();

    // THE IDENTITY LINE. Nothing else in the tree prints "PathStack".
    std::printf("[PathStack] registered %d exports + 4 container addresses "
                "(negctl=%d, probe=%s)\n",
                n, g_negCtl, RtObjectPathStackProbeArmed() ? "ARMED" : "off");
    return n;
}
