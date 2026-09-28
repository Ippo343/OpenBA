// ============================================================================
// runtime/src/io/package_file_dtor.cpp — ~IO::PackageFile
//                                        Wave 25 lane BODY
// ============================================================================
//
//   | Export                     | Retail      | Ord | Bytes | Instrs |
//   |----------------------------|-------------|-----|-------|--------|
//   | ??1PackageFile@IO@@UEAA@XZ | 0x1800b4540 | 639 |  318  |   87   |
//
// `UEAA` is the VIRTUAL spelling and it is the one the DLL exports — the
// non-virtual `QEAA` form of this name does not exist, and asking for it would
// have resolved NULL with `ResolveExport` hiding the miss
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). The name above was printed
// out of the shipped export directory by ADDRESS, not typed from the pattern.
//
// ---------------------------------------------------------------------------
// 1. THE STATED BLOCKERS, AND WHAT RE-DERIVING THEM FOUND
// ---------------------------------------------------------------------------
// Two blockers were on the record for this row, one of each shape. Kickoff §1a:
// dependency-shaped claims decay, oracle-shaped ones do not. Here BOTH failed,
// which is itself the news.
//
// (a) DEPENDENCY — `package_file.cpp:925` lists this row as deliberately absent
//     with the parenthetical **"(5 unported callees)"**.
//     **RE-DERIVED: FALSE. The dtor has SEVEN distinct callees and SIX of them
//     are ported AND registered in this tree today.** Measured by disassembling
//     0x1800b4540..0x1800b467e and grepping each target for a
//     `RegisterRuntimeAddr`/`RegisterRuntimeExport`:
//
//       0x18000c180  ~FileSystem::FileInfo   PORTED  io/file_stream.cpp:886
//       0x1800269a0  ~Array<T> (elem loop)   NOT PORTED  <- the only one
//       0x1800112a0  ~Array<uint8>           PORTED  io/modification.cpp:909
//       0x1800111a0  ~Array<int32>           PORTED  io/modification.cpp:907
//       0x1800c7ad0  ?MemFree@MemoryAllocator PORTED  system/allocator.cpp:1701
//       0x180010ab0  ~Array<T> (elem dtor)   PORTED  collection/array_destroy.cpp:902
//       0x1800bc950  ~Object::External       PORTED  object/external.cpp:762
//
//     The count was wrong by four and, more to the point, **an unported callee
//     was never a blocker in the first place**: retail reaches all seven by
//     rel32, this port reaches them through
//     `ResolveAddr(Module::Runtime, addr)`, and that consults the PORT REGISTRY
//     FIRST and falls through to the DLL. So 0x1800269a0 lands on the DLL's own
//     body today and on whatever registers it later, with no change here. That
//     is the kickoff's worked specimen — "not exported" is not "no door" — in
//     its dependency form.
//
//     ENDGAME B.3.2 step 2 (callers, 2026-09-13): both 0x1800269a0 and
//     0x180010ab0 are now `Collection::Array<T>::~Array()` member calls at
//     their call sites in §2's body, not `ResolveAddr` forwarders — see the
//     comments there for the instantiation each one names.
//
// (b) ORACLE — `WAVE23_LANE_IOPORT.md §2.4`, carried forward by Wave 24 lane
//     SMALLBODY as "the strongest-held of the nine, and correctly scoped":
//     *"it has no oracle... a fabricated subject is a fake rather than a
//     synthetic one"*.
//     **RE-DERIVED: FALSE, and §3 is the oracle.** The objection is real but it
//     is an objection to ONE construction (fabricating a PackageFile by hand),
//     not to the existence of an oracle. The subject below is not fabricated:
//     it is built by **retail's own exported constructor**
//     (`??0PackageFile@IO@@QEAA@XZ`, which this tree already ports and which
//     `package_file.cpp` §5 already byte-verifies against the DLL), then filled
//     with **real refcounted System::String blocks from the real allocator**.
//     That is a SYNTHETIC SUBJECT, which beats a fake precisely because every
//     part of it was produced by the shipped code
//     (reference_synthetic_subject_beats_a_fake.md).
//
// ---------------------------------------------------------------------------
// 2. THE BODY — WHAT THE BYTES DO
// ---------------------------------------------------------------------------
// Stamp the vtable, destroy five embedded members, release five Strings, then
// TAIL-JUMP to `~Object::External`.
//
//   1800b455b  LEA RAX,[0x180155700] / MOV [RCX],RAX   ??_7PackageFile@IO@@6B@
//   1800b4565  ADD RCX,0xc0 / CALL 0x18000c180         ~FileInfo        (+0xC0)
//   1800b4572  LEA RCX,[RBX+0xb0] / CALL 0x1800269a0   ~Array           (+0xB0)
//   1800b457f  LEA RCX,[RBX+0x98] / CALL 0x1800112a0   ~Array<uint8>    (+0x98)
//   1800b458c  LEA RCX,[RBX+0x80] / CALL 0x1800111a0   ~Array<int32>    (+0x80)
//   1800b4599  the String release idiom at +0x60, +0x58, +0x50, +0x48, +0x40
//   1800b4658  LEA RCX,[RBX+0x30] / CALL 0x180010ab0   ~Array           (+0x30)
//   1800b4662  LEA RCX,[RBX+0x20] / CALL 0x180010ab0   ~Array           (+0x20)
//   1800b466c  MOV RCX,RBX / JMP 0x1800bc950           ~External, TAIL
//
// FOUR TRANSCRIPTION POINTS THAT MATTER:
//
// (i)   **THE FIVE STRINGS ARE RELEASED IN DESCENDING ORDER** — +0x60 first,
//       +0x40 last. That is reverse construction order, which is what a dtor
//       must do, and it reaches the allocator in that order. A port that
//       released them ascending would produce an identical object and a
//       different free sequence, and no comparator on the object bytes could
//       see it (array_destroy_internal.h records the same trap on three rows).
//
// (ii)  **THE RELEASE IDIOM'S TWO ARMS LEAVE THE SLOT HOLDING DIFFERENT
//       THINGS.** On the freeing arm the slot keeps `data - 8`; on the sharing
//       arm it is NULLED. That asymmetry is retail's and it is the ONLY part of
//       this function a byte comparator can see, which is why §3's subject
//       deliberately contains both kinds. `RtSystem::RtString_ReleaseInline`
//       already is this idiom, instruction for instruction (string.cpp:132),
//       so this file calls it rather than re-transcribing it — the fifth and
//       sixth copies of a refcount idiom are how one of them drifts.
//
// (iii) **TWO MORE STRING FIELDS THAN FileSystem.h NAMES.** The header models
//       +0x40 `name`, +0x48 `author`, +0x50 `description` and then
//       `_unknown58[0x58..0x67]`. The dtor releases **+0x58 and +0x60 with the
//       same String idiom**, so both are `System::String`. They are recorded in
//       the header as strings-of-unknown-purpose; they are NOT given invented
//       names, because a destructor pins the TYPE and says nothing about the
//       MEANING (reference_a_writer_beats_readers_agreeing_on_a_sign.md is the
//       same discipline one step over).
//
// (iv)  **THE LAST CALL IS A TAIL JUMP, SO ITS RETURN VALUE IS THIS
//       FUNCTION'S.** `JMP 0x1800bc950` — whatever `~Object::External` leaves
//       in RAX is what `~PackageFile` returns. Retail's callers are free to use
//       it. Declaring this `void` would leave RAX unset, which works with the
//       DLL present and is exactly the shape §1c of the kickoff describes for
//       ctors; so the signature returns what the tail call returns rather than
//       discarding it. That is a transcription of the jump, not a guess about
//       a caller.
//
// ---------------------------------------------------------------------------
// 3. THE ORACLE — PROBE SHAPE: **SYNTHESISED ORACLE**, named up front
// ---------------------------------------------------------------------------
// Why not the others: an INPUT SWEEP needs purity and this mutates an object
// and the heap; a SHADOW SITE alone cannot work, and the reason is the whole
// difficulty of this row — the two arms would share the same String blocks and
// the second run would DOUBLE FREE; a WRITE-PATH REPLAY has no stream; the
// fallback is TWO-SIDED DISASSEMBLY and it was not needed.
//
// The construction, and every part of it is shipped code:
//
//   1. two arenas of 0xF0 bytes, prefilled 0xCD
//   2. `the raw DLL door<CtorFn>("??0PackageFile@IO@@QEAA@XZ")` on BOTH — the DLL's
//      own constructor, so the starting states are retail's and identical
//   3. five String slots filled per arena with **SEPARATE** blocks from
//      `RtString_SetLength` (the real allocator), with a deliberately MIXED
//      refcount pattern: refcount 1 on +0x60/+0x50/+0x40 (the FREEING arm) and
//      refcount 2 on +0x58/+0x48 (the SHARING arm). Separate blocks are what
//      makes two runs safe; the mixed pattern is what makes the comparison
//      mean anything (reference_uniform_population_is_not_a_verification.md —
//      a population where every case takes the same branch verifies one branch
//      and reports as if it verified the function).
//   4. `the raw DLL door<DtorFn>("??1PackageFile@IO@@UEAA@XZ")` on arena B — **this
//      is a real consultation of the DLL**, which is what makes the site an
//      `[ORACLE]` rather than one of the 133 NO-COMPARISON sites the kickoff
//      counts. `ResolveAddr(Module::Runtime, …)` would NOT do: it checks the
//      port registry first and would compare this port with itself
//      (reference_resolveaddr_consults_port_registry_first.md).
//   5. our body on arena A
//   6. compare all 0xF0 bytes — EXCEPT the two qwords holding the String
//      pointers whose arms FREED, because those hold `data - 8` for two
//      DIFFERENT heap blocks and must differ. Comparing them would fail 100%
//      of calls and read like a catastrophic port
//      (reference_oracle_must_not_compare_unwritten_bytes.md). They are checked
//      by PREDICATE instead — "freed slot holds its own block minus 8" — which
//      is strictly stronger than a byte compare and is stated as its own row.
//
// CONTROLS (kickoff §3.2: a control must corrupt something the probe COMPARES):
//   C1  release the Strings in ASCENDING order         -> must ABSTAIN (agree):
//                                                         the object bytes
//                                                         cannot see order. It
//                                                         is the EFFECTIVENESS
//                                                         control and its green
//                                                         is the honest report
//                                                         of a real blind spot.
//   C2  null the slot on the FREEING arm too           -> must be CAUGHT
//   C3  skip the +0x58 release                         -> must be CAUGHT
//   C4  skip the vtable stamp                          -> must be CAUGHT
//   C5  skip the ~Array at +0x20                       -> must be CAUGHT
// Each control's verdict is reported on ITS OWN ROW and never unioned
// (reference_union_control_hides_a_blind_screen.md).
//
// **WHAT THE ORACLE DOES NOT ESTABLISH, PLAINLY:** it cannot see the ORDER of
// the seven callee invocations (C1 is the standing proof of that, not a
// suspicion), and it cannot see anything the five member destructors do to
// memory OUTSIDE the 0xF0 arena. It establishes that the object is left in the
// same state, with the same slots freed and the same slots shared.
//
// ---------------------------------------------------------------------------
// 4. DOOR ARITHMETIC — DECLARED
// ---------------------------------------------------------------------------
// CLOSES: the NAME door `??1PackageFile@IO@@UEAA@XZ` and the ADDRESS door
//         0x1800b4540.
// OPENS:  **0x1800269a0** — an ADDRESS door, and it is genuinely one: the
//         function is not in the export directory under any name (checked by
//         address with pefile; `AEAA` would not have settled it, since this DLL
//         exports its private members —
//         reference_aeaa_is_private_not_unexported.md). The other six callees
//         are already registered, so they add no rows.
// NET DECLARED SURFACE GROWTH: **+1 census row** (0x1800269a0, address-keyed,
// which lands in the `internal` bucket), against 1 row moved into `ported`.
//
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

#include "../../types/system/String.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/PackageFile.h"     // ENDGAME B.3.9 IO: the member dtor
#include "../../types/io/Modification.h"    // the two Array dtor bodies
#include "../system/string.h"
#include "../collection/array_internal.h"   // ENDGAME B.3.2 step 2 -- kSpec_PkgEntry, AsArray

using RtSystem::String;

namespace {

// The five String slots, in RETAIL'S RELEASE ORDER (descending). Reproduced,
// not sorted — see §2(i).
constexpr uint32_t kStringOffsets[5] = { 0x60, 0x58, 0x50, 0x48, 0x40 };

inline uint8_t* Bytes(void* p) { return reinterpret_cast<uint8_t*>(p); }

}  // namespace

// ============================================================================
// ??1PackageFile@IO@@UEAA@XZ   retail 0x1800b4540  ord 639
// ============================================================================
// ENDGAME B.3.9 IO: the class's own destructor. The door that stood here, its
// by-address/by-name registration and `StampVtable` are gone: the entry stamp
// at 0x1800b455b is the compiler's, and the tail JMP into ~External
// (0x1800b4679) is the compiler's base-destructor call. The order in between is
// retail's, unchanged.
IO::PackageFile::~PackageFile() {
    uint8_t* p = Bytes(this);

    RtIO::RtIo_FileInfo_Dtor(reinterpret_cast<RtIO::FileInfo*>(p + 0xC0));  // 0x1800b456c  ~FileInfo 0x18000c180

    // retail 0x1800269a0 — `~Array<IO::PackageFile::Entry>` at +0xB0.
    // array_destroy.cpp's kD_Str_10 / array.cpp's kSpec_PkgEntry: stride 0x10,
    // an assign that calls Filename::AdjustDelimiters and a destroy that
    // releases the embedded String — neither is trait-expressible (Collection.h's
    // Array banner), so this stays the hand-derived spec, reached with `AsArray`.
    rtcoll::AsArray<&rtcoll::kSpec_PkgEntry>(p + 0xB0)->~Array();

    RtIO::RtColl_ArrayU8Dtor_1800112a0(p + 0x98);   // 0x1800b4586  ~Array<uint8> 0x1800112a0
    RtIO::RtColl_ArrayI32Dtor_1800111a0(p + 0x80);  // 0x1800b4593  ~Array<int32> 0x1800111a0

    // 0x1800b4599..0x1800b4658 — five copies of the release idiom, descending.
    for (int i = 0; i < 5; ++i) {
        String* s = reinterpret_cast<String*>(p + kStringOffsets[i]);
        RtSystem::RtString_ReleaseInline(s);
    }

    // retail 0x180010ab0 — `~Array<System::String>` at +0x30 and +0x20 (stride
    // 8, array_destroy.cpp's kD_Str_08): the trait-derived
    // `Collection::Array<System::String>` over the two POD headers.
    reinterpret_cast<Collection::Array<System::String>*>(p + 0x30)->~Array();  // B.3.2 bridge
    reinterpret_cast<Collection::Array<System::String>*>(p + 0x20)->~Array();  // B.3.2 bridge

    // 0x1800b4679 — the tail into ~External is the compiler's.
}
