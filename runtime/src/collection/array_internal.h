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

#include "../../types/collection/Collection.h"

namespace rtcoll {

// ENDGAME B.3.2 step 1 — `ArrayHdr`, `ArraySpec`, the Elem* typedefs and the
// four core declarations MOVED to runtime/types/collection/Collection.h, where
// `Collection::Array<T>`'s inline bodies can name them. Nothing about them
// changed; this header still owns the `kSpec_*` rows below and the commentary
// on what each spec was read out of, and the per-field commentary that used to
// sit on `ArraySpec` stays HERE, next to those rows:
//
//   * a written-extent constant belongs to a WRITER, not to a type
//     (reference_never_written_needs_by_whom.md) — `assign` and `init` are two
//     code paths with two different written extents, so there are two windows
//     and the oracle has to know which arm produced the element it is looking
//     at. Each pair is a half-open byte range WITHIN one element that the arm
//     does NOT write; equal (both 0) means the arm covers the whole stride.
//     Comparing an unwritten byte fails 100% of cases and reads like a
//     catastrophic port defect
//     (reference_oracle_must_not_compare_unwritten_bytes.md).
//   * `seedZeroLo/Hi` is the range the PROBE must leave ZERO because the
//     element operations DEREFERENCE it (a String pointer whose refcount at
//     `str-4` gets bumped). Everything outside it is seeded with a varying
//     non-zero pattern — all-zero elements are the one seed under which an
//     unwritten-bytes bug is invisible.

// ---------------------------------------------------------------------------
// ENDGAME B.3.2 step 1 — the instantiation a registered address maps onto
// ---------------------------------------------------------------------------
// A hand-derived `kSpec_*` row is not expressible from an element type's traits
// (Collection.h's Array banner lists the twelve kinds that are not), so every
// registered `SetSize`/`~Array` shim names its own row and gets the
// instantiation that carries it:
//
//     Collection::Array<uint8_t, DefaultAllocator, ExplicitSpec<&kSpec_X>>
//
// The element type is `uint8_t` for the same reason `ArrayHdr`'s is: the shim
// is handed a `void*` and the stride lives in the spec, not in the C++ type.
// A converted caller in step 2 uses `Collection::Array<RealElement>` instead
// and gets `rtcoll::SpecFor<RealElement>()`.
template <const ArraySpec* SPEC>
using ArrayWithSpec = Collection::Array<uint8_t, Collection::DefaultAllocator,
                                        Collection::ExplicitSpec<SPEC>>;

template <const ArraySpec* SPEC>
inline ArrayWithSpec<SPEC>* AsArray(void* a) {
    return reinterpret_cast<ArrayWithSpec<SPEC>*>(a);
}

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
extern const ArraySpec kSpec_Node18;      // 0x18005fb00   (Wave 9 lane GFX3)
extern const ArraySpec kSpec_Sprite38;    // 0x180004e30   NON-TRIVIAL
extern const ArraySpec kSpec_EffectSlot;  // 0x180051390   NON-TRIVIAL
extern const ArraySpec kSpec_StringExt;   // 0x180061480   NON-TRIVIAL
// Wave 9 lane UIW. Both NON-TRIVIAL (element assign and a shrink-path destroy),
// but unlike the three above, every element operation they use is PORTED rather
// than called by address into the DLL — see array.cpp's registration note.
extern const ArraySpec kSpec_StringRef;   // 0x180034860   NON-TRIVIAL
extern const ArraySpec kSpec_StringRefZ;  // 0x180029a80  tail ZEROED
extern const ArraySpec kSpec_GraphSeries; // 0x1800417d0   NON-TRIVIAL

// Wave 9 lane DESTROY. Declared here so array_destroy_probe.cpp can drive
// SetSizeCore's SHRINK-TAIL arm for this spec: a live boot only ever GROWS the
// packfile entry array, so that arm has zero live coverage and the probe is the
// only place it gets driven at all.
extern const ArraySpec kSpec_PkgEntry;    // 0x1800298f0   NON-TRIVIAL

// Wave 9 lane VIDL. DEFINED IN runtime/src/engine/mainloop_video.cpp, not in
// array.cpp — the only spec in this table that lives outside the Collection
// port. It is declared here for one reason: so array_probe.cpp can drive
// `Array<MainLoop::VideoResource>::SetSize` (0x18004f620) against the SHIPPED
// body through the same machinery as the other sixteen. Its element assign
// calls System::Filename's copy ctor and its destroy is the 0x180034440 entry
// point that lane registers, so the spec sits next to them rather than next to
// this declaration.
extern const ArraySpec kSpec_VideoResource;  // 0x18004f620   NON-TRIVIAL

// Wave 27 lane FRONTIER. NON-TRIVIAL and TWO-ARGUMENT: the element is
// {System::Filename, int32, int32}, the assign is a CALL to Filename's copy
// ctor and the destroy runs on BOTH the realloc and the shrink arm. Declared
// here so array_probe.cpp can drive it against the shipped 0x18002c6a0 through
// the same machinery as the other rows; no probe case exists yet (residual in
// docs/runtime_port/WAVE27_LANE_FRONTIER.md, addressee named there).
extern const ArraySpec kSpec_ExtArrayElem;   // 0x18002c6a0   NON-TRIVIAL
extern const ArraySpec kSpec_StringPair10;    // 0x180078c90   NON-TRIVIAL

}  // namespace rtcoll
