#pragma once
// ============================================================================
// runtime/src/collection/array_internal.h — shared between array.cpp and its
// probe (Wave 9 lane Q).
// ============================================================================
//
// Only two files include this. It exists so array_probe.cpp can drive the SAME
// `SetSizeCore` and `ArrayDtorImpl` the registered entry points drive, against
// the SAME `ArraySpec` rows — a probe that re-derived either would be testing a
// copy of the port rather than the port.
//
// The full commentary — what each spec was read out of, why the arity varies,
// which instantiations leave bytes unwritten — is the banner of array.cpp.
// ============================================================================

#include <cstdint>

#include "../../funcs/rt_diff.h"
#include "../../types/collection/Collection.h"

namespace rtcoll {

using ArrayHdr = Collection::Array<uint8_t>;

using ElemAssign  = void (*)(uint8_t* dst, const uint8_t* src);
using ElemInit    = void (*)(uint8_t* dst);
using ElemDestroy = void (*)(void* ignoredThis, uint8_t* base, int count);

struct ArraySpec {
    const char* name;
    uint32_t    stride;
    int         align;
    ElemAssign  assign;    // never null
    ElemInit    init;      // null => retail leaves the new tail untouched
    ElemDestroy destroy;   // null => trivially destructible T
    bool        byteCopy;  // true when `assign` is a pure byte copy

    // ---- Written extents, PER ARM ------------------------------------------
    // A written-extent constant belongs to a WRITER, not to a type
    // (reference_never_written_needs_by_whom.md -- Wave 7 got this wrong in
    // BOTH directions inside one hour). `assign` and `init` are two different
    // code paths with two different written extents, and an element in
    // [oldSize, newSize) takes `assign` when the caller passed a `fill` and
    // `init` when it did not. So there are two windows, and the oracle has to
    // know which arm produced the element it is looking at.
    //
    // Each pair is a half-open byte range WITHIN one element that the arm does
    // NOT write. Equal (both 0) means the arm covers the whole stride.
    // Comparing an unwritten byte fails 100% of cases and reads like a
    // catastrophic port defect (reference_oracle_must_not_compare_unwritten_
    // bytes.md, four such gaps in Wave 6).
    uint32_t    assignUnwrittenLo, assignUnwrittenHi;
    uint32_t    initUnwrittenLo,   initUnwrittenHi;

    // ---- Probe seeding ------------------------------------------------------
    // Half-open byte range within one element that the PROBE must leave ZERO,
    // because the element operations DEREFERENCE it (a String pointer whose
    // refcount at `str-4` gets bumped, or a sub-object handed to another
    // helper). Everything outside this range is seeded with a varying non-zero
    // pattern -- which is the whole point: all-zero elements are the one seed
    // under which an unwritten-bytes bug is invisible, because both sides read
    // zero there and agree by accident.
    uint32_t    seedZeroLo, seedZeroHi;
};

// One diff site per REGISTERED ADDRESS. See array.cpp's "Live instrumentation".
//
// *** A Site MUST HAVE STATIC STORAGE DURATION. *** RtDiffRegister stores the
// POINTER and rt_diff.cpp re-reads it at summary time, so a stack Site dangles
// and the summary reports it as "sites were CORRUPTED -- a stray write hit the
// harness's own statics". The probe's first draft did exactly that 144 times;
// the full account is in array.cpp above SetSizeCore.
struct Site { RtDiffSite d; uintptr_t addr; };

// The allocator boundary (by address; see array.cpp's banner and the lane E
// dependency stated there).
void* MemAlloc(uint64_t bytes, int align);
void  MemFree(void* p);

// ---------------------------------------------------------------------------
// Probe scheduling.
//
// The probe MUST NOT run from RtCollectionPortInit(). That runs immediately
// after ResolveInit()'s LoadLibrary (standalone/main.cpp:1655), and the DLL's
// MemoryAllocator has been CONSTRUCTED by then but not STARTED — Engine::Startup
// does that later. Allocating from it at that moment is exactly the kind of
// "works on my boot order" hazard this project keeps paying for.
//
// So the probe self-schedules: `g_probePending` is armed at init and the first
// call into ANY registered entry point runs it. That moment is provably safe,
// because the caller is itself in the middle of using an Array.
extern bool g_probePending;

// The two ported bodies.
// `site` may be NULL, which means "run the body, touch nothing in the diff
// registry". The probe passes NULL.
void* ArrayDtorImpl(ArrayHdr* a);
void  SetSizeCore(ArrayHdr* a, int newSize, const uint8_t* fill,
                  const ArraySpec* spec, Site* site);

// The per-address specs, in the order of the parameterization table in
// docs/runtime_port/WAVE9_LANE_Q.md.
extern const ArraySpec kSpec_Bool;        // 0x180008b90   tail UNINITIALISED
extern const ArraySpec kSpec_I32;         // 0x18002fea0   tail UNINITIALISED
extern const ArraySpec kSpec_U64Zero;     // 0x180008ff0
extern const ArraySpec kSpec_PtrZero;     // 0x180008d60, 0x1800764c0
extern const ArraySpec kSpec_Transform;   // 0x180059140
extern const ArraySpec kSpec_I32Neg1;     // 0x18007b960
extern const ArraySpec kSpec_Fill8;       // 0x18001f990
extern const ArraySpec kSpec_Fill8Z;      // 0x180076900, 0x18007a0d0
extern const ArraySpec kSpec_Fill4;       // 0x180023830
extern const ArraySpec kSpec_Fill16;      // 0x18004f790, 0x18004f8e0
extern const ArraySpec kSpec_Sprite38;    // 0x180004e30   NON-TRIVIAL
extern const ArraySpec kSpec_EffectSlot;  // 0x180051390   NON-TRIVIAL
extern const ArraySpec kSpec_StringExt;   // 0x180061480   NON-TRIVIAL

}  // namespace rtcoll

// Runs the synthetic-subject replay probe. Driven by rtcoll::g_probePending —
// see the note above for why it is not called from RtCollectionPortInit().
void RtCollectionProbeRun();
// True when BANISHED_RT_ARRAY_PROBE selects a mode. Read once, at init.
bool RtCollectionProbeArmed();
