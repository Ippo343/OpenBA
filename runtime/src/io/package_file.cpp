// ============================================================================
// runtime/src/io/package_file.cpp — Wave 9 lane PKG2
// IO::PackageFile — the cutover wall of attempt 14
// ============================================================================
//
// ATTEMPT 14 DIED HERE. `RtIo_FileSystem_LoadPackages +0x184`, EXEC of address
// 0, on a heap block whose first 0x38 bytes were all zero, immediately after
// `[RtResolve] MISS "??0PackageFile@IO@@QEAA@XZ" -> NULL`. The crash handler
// labelled it VCALL-of-0; it is the CONSTRUCTOR CALL itself (RAX=0, RCX=RDX=the
// fresh 0xF0 block, which is why the object reads as all-zero — nothing had run
// on it yet). Lane PKG allocated the block, resolved the constructor to
// nothing, and called it.
//
// This file ports the constructor and the class's leaf accessors. It does NOT
// port `?ReadPackfile@` — see §4.
//
//   | Export                                        | Retail      | Ord  | Size |
//   |-----------------------------------------------|-------------|------|------|
//   | ??0PackageFile@IO@@QEAA@XZ                    | 0x1800b4440 |  289 | 48 i |
//   | ?Construct@PackageFile@IO@@UEAAPEAV…XZ        | 0x1800252f0 | 1834 |  4 i |
//   | ?Type@PackageFile@IO@@UEBAIXZ                 | 0x180025130 | 4210 |  2 i |
//   | ?TypeId@PackageFile@IO@@SAIXZ                 | 0x180025140 | 4314 |  2 i |
//   | ?TypeName@PackageFile@IO@@UEBAPEBDXZ          | 0x180025150 | 4417 |  2 i |
//   | ?GetName@PackageFile@IO@@QEBA?AV…XZ           | 0x180025310 | 2806 | 13 i |
//   | ?GetAuthor@PackageFile@IO@@QEBA?AV…XZ         | 0x180025350 | 2456 | 13 i |
//   | ?GetDescription@PackageFile@IO@@QEBA?AV…XZ    | 0x180025390 | 2566 | 13 i |
//   | ?GetVersion@PackageFile@IO@@QEBAHXZ           | 0x1800253d0 | 3010 |  2 i |
//   | ?GetUserVersion@PackageFile@IO@@QEBAHXZ       | 0x1800253e0 | 3003 |  2 i |
//   | ?GetPlatform@PackageFile@IO@@QEBAHXZ          | 0x1800253f0 | 2848 |  2 i |
//   | ?GetFileCount@PackageFile@IO@@QEBAHXZ         | 0x180025440 | 2600 |  2 i |
//   | ?GetFileSize@PackageFile@IO@@QEBAIH@Z         | 0x1800254a0 | 2605 |  5 i |
//   | ?GetFileOffset@PackageFile@IO@@QEBAIH@Z       | 0x1800254c0 | 2603 |  5 i |
//   | ?GetPriority@PackageFile@IO@@QEBAHXZ          | 0x1800254e0 | 2856 |  2 i |
//   | ?CanOverride@PackageFile@IO@@QEBA_NXZ         | 0x1800254f0 | 1683 |  2 i |
//   | ?GetCRC@PackageFile@IO@@QEBAIXZ               | 0x180025500 | 2473 |  2 i |
//   | ?GetIconWidth@PackageFile@IO@@QEBAHXZ         | 0x180025510 | 2699 |  2 i |
//   | ?GetIconHeight@PackageFile@IO@@QEBAHXZ        | 0x180025520 | 2698 |  2 i |
//   | ?GetFileVersion@PackageFile@IO@@UEBAHXZ       | 0x1800b6080 | 2642 |  2 i |
//   | ?Compile@PackageFile@IO@@UEAA?AW4…@Z          | 0x1800b6090 | 1744 |  2 i |
//
// EVERY NAME AND EVERY ORDINAL IN THAT TABLE WAS PRINTED OUT OF
// Runtime-x64.dll's OWN EXPORT DIRECTORY with `pefile`, not transcribed from a
// decompile and not typed from memory. Lane PKG's header records that its own
// first draft carried eight hand-reconstructed ordinals and SEVEN WERE WRONG;
// an ordinal is exactly the kind of detail that reads as evidence and is never
// checked by the compiler. `tools/export_name_audit.py` re-checks the names.
//
// the sweep probe — and leaves every call on the DLL.
//
// NO NEW WIN32 API AND NO NEW IMPORT LIBRARY.
//
// ---------------------------------------------------------------------------
// 1. THE CONSTRUCTOR, INSTRUCTION BY INSTRUCTION
// ---------------------------------------------------------------------------
// 0x1800b4440, 48 instructions, and it CALLS NOTHING. It is a straight run of
// stores into `this` plus `MOV RAX,RDX` (the saved `this`) on the way out. That
// makes it the rarest thing in this wave: a byte-faithful port with a REAL,
// CHEAP, NON-DESTRUCTIVE ORACLE — run both bodies onto private scratch and
// compare all 0xF0 bytes. See §5.
//
//   1800b4454  LEA RAX,[0x180154788] / MOV [RCX],RAX   ??_7Base@Object@@6B@
//   1800b445e  LEA RAX,[0x1801547a0] / MOV [RCX],RAX   ??_7External@Object@@6B@
//   1800b4468  LEA RAX,[RCX+8] ; MOV [RAX],R8          +0x08 = 0
//   1800b4477  MOV [RCX+0x10],R8                       +0x10 = 0
//   1800b447b  MOV [RCX+0x18],R8                       +0x18 = 0
//   1800b447f  LEA RAX,[0x180155700] / MOV [RCX],RAX   ??_7PackageFile@IO@@6B@
//   1800b4489  +0x20 .. +0x68 = 0                      ten qword stores
//   1800b44b1  MOV qword [RCX+0x70],0x2710             priority=10000, platform=0
//   1800b44b9  +0x80, +0x88 = 0
//   1800b44c7  +0x98, +0xa0 = 0
//   1800b44d5  MOV dword [RCX+0xa8],R8D                the CRC / packfile id
//   1800b44dc  MOV byte  [RCX+0xac],1                  overridable = TRUE
//   1800b44e3  +0xb0, +0xb8 = 0                        the file table
//   1800b44f1  ADD RCX,0xc0 — the embedded FileSystem::FileInfo:
//               +0xc0 dword = 0xFFFFFFFF, +0xc8 = 0, +0xd0 dword = 0,
//               +0xe0 = 0, +0xe8 = 0, and then +0xd8 = -1 (out of order,
//               transcribed in retail's order below).
//
// THE THREE VTABLE STORES. Two of them are dead by inspection: nothing between
// 0x1800b445b and 0x1800b4486 reads [this+0], and there is no call in the
// window, so only ??_7PackageFile survives. They are transcribed anyway (MSVC's
// inlined base constructors; PLAN §6 step 4 says byte-faithful first) and they
// are transcribed THROUGH THE PORT-AWARE DATA DOOR rather than as literal
// 0x180154788 / 0x1801547a0. That matters at cutover: a raw VA would be a
// pointer into a module that is not loaded, and it would be written into every
// package object in the process.
//
// FIELDS THE CONSTRUCTOR DOES NOT WRITE. +0x78/+0x7C (the icon dimensions,
// which ?GetIconWidth@/?GetIconHeight@ read), +0x90, +0xAD..+0xAF, +0xC4,
// +0xD4. The store list jumps 0x70 -> 0x80 and 0x88 -> 0x98; that is retail
// leaving them to ReadPackfile, not a gap in this transcription. The oracle in
// §5 is built so those bytes cannot produce a false failure OR a free pass.
//
// ---------------------------------------------------------------------------
// 2. WHAT THIS UNBLOCKS, AND WHAT IT DOES NOT
// ---------------------------------------------------------------------------
// LoadPackages' inner loop is `MemAlloc(0xF0) -> ??0PackageFile -> ReadPackfile
// -> append`. This file closes the middle step of three. The NEXT rung is
// `?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z` (0x1800b46b0,
// ord 3461, 6,597 BYTES) — the .pkg format parser, which is a lane of its own
// and is NOT attempted here.
//
// So the honest prediction for attempt 15 is: the frame stays in
// RtIo_FileSystem_LoadPackages and the fault MOVES FORWARD from the constructor
// call to the ReadPackfile call, with `[RtResolve] MISS "?ReadPackfile@..."`
// above it. What would falsify it:
//   * A fault still naming ??0PackageFile means the registration did not take —
//     check that `[RtPort] IO: N exports registered` moved by 21.
//   * A fault inside our own constructor body means a minted vtable row is
//     missing: `RT_DATA_SLOT` returns null for a name the mint does not carry,
//     and the store would put 0 at [this+0]. §7 is about exactly that.
//   * A fault naming `?CloseFile@` or `FUN_180008340` would mean the boot got
//     PAST ReadPackfile somehow and is in ClosePackages' teardown — good news,
//     and it would mean lane PKG's §4 list is shorter than it looks.
//
// ---------------------------------------------------------------------------
// 3. THE ACCESSORS, AND WHY THEY ARE IN THIS FILE AT ALL
// ---------------------------------------------------------------------------
// Twenty of the twenty-one rows are leaves — `MOV EAX,[RCX+off] ; RET`, or the
// String AddRef idiom, or a constant. On their own they would be the thing this
// project has been burned by repeatedly: registered ports with no caller,
// reporting `divergences=0` over code no run ever executed
// (reference_uncalled_port_is_not_verified.md). Nothing in our tree calls
// `?GetPriority@` or `?GetCRC@` today, so registering them alone would produce
// exactly that.
//
// They are here because a caller EXISTS and is free: LoadPackages mounts every
// .pkg and .pkm on the install and leaves the objects hanging off the
// FileSystem singleton, fully populated by the DLL's own ReadPackfile. That is
// a large, real, NON-UNIFORM population sitting in memory at the end of a
// function this tree already owns. §6's sweep probe walks it.
//
// ---------------------------------------------------------------------------
// 4. WHAT THIS FILE DELIBERATELY DOES NOT PORT, AND WHY
// ---------------------------------------------------------------------------
//   ?ReadPackfile@PackageFile@IO@@…        0x1800b46b0  ord 3461  6,597 bytes
//       The .pkg parser. THE NEXT WALL. A lane, not a tail-end.
//   ??1PackageFile@IO@@UEAA@XZ             0x1800b4540  ord  639
//       The destructor. It is not hard, it is BLOCKED ON SIX UNPORTED CALLEES:
//       0x18000c180 (~FileInfo at +0xc0), 0x1800269a0 (~Array at +0xb0),
//       0x1800112a0 (+0x98), 0x1800111a0 (+0x80), 0x180010ab0 (+0x30 and
//       +0x20, twice), and a tail JMP to 0x1800bc950 (~External, which Wave 3
//       DID port). Five of those six are unported today, so a "port" of the
//       destructor would be five calls through addresses that are null at
//       cutover — strictly worse than one named MISS on the destructor itself.
//   FUN_1800259d0 (the DELETING destructor, vtable slot 0)
//       Same blocker: it is the MSVC deleting-destructor thunk over
//       ??1PackageFile. Lane K owns that family. ClosePackages calls it, so it
//       is needed at cutover — but only once a package has been mounted, which
//       is downstream of ReadPackfile.
//   ?Reconstruct@ 0x1800252d0 and ?Destruct@ 0x180025300
//       Both are two-instruction bodies AND BOTH DRIVE VTABLE SLOT 0, i.e. the
//       destructor above. Porting them today would put a faithful-looking body
//       over a call that cannot work, and neither has any oracle — running one
//       destroys the object being compared. They belong with the destructor
//       row, not with the leaves, and they are declined for that reason rather
//       than for cost. (This is the row where the first draft of this file was
//       wrong: it grouped Reconstruct/Destruct with Construct because all three
//       are 2-4 instructions. Construct is portable because its only callee is
//       the constructor this file ports; the other two are not, because their
//       only callee is the destructor it does not.)
//   ?Serialize@PackageFile@IO@@UEAA_NAEAVStream@2@@Z   0x1800b60a0
//       ~100 instructions dispatching through the IO::Encoding vtable at
//       +0x18/+0x20/+0x40/+0x78/+0xa0. It is a SAVE-PATH Serialize, which is
//       this project's most-cited failure mode, and the retail-vs-profile
//       Encoding offset trap applies
//       (reference_encoding_vtable_offsets_differ_between_builds.md). It has a
//       real oracle available — the write path replays
//       (reference_write_path_serialize_is_replayable.md) — but only with a
//       Stream to drive it, which this lane does not have. Deferred, not
//       dismissed.
//   ?GetFileInfo@ 0x180025400 — needs the FileInfo copy ctor 0x18000c110.
//   ?GetIconData@ 0x180025530, ?GetPreviewData@ 0x180025580 — return
//       Collection::Array copies; the Array copy machinery is lane Q/D2's.
//   ??0PackageFile@IO@@QEAA@AEBV01@@Z (copy ctor, 0x1800255d0),
//   ??4PackageFile@IO@@ (operator=, 0x180025790),
//   ?WritePackfile@ / ?ExtractPackFile@ / ?BuildPackageFile@ /
//   ?AreResourcesEqual@ / ?BuildFileSet@ / ?ClosePackFile@
//       The TOOLKIT half of the class — building and writing packages. Nothing
//       in the game's boot or run path reaches any of them; they are the
//       resource compiler's side. Not on any cutover path this wave has
//       measured, and listed so the next lane does not re-derive that.
//
// ---------------------------------------------------------------------------
// 5. THE CONSTRUCTOR'S ORACLE
// ---------------------------------------------------------------------------
// Both bodies run onto PRIVATE 0xF0 scratch buffers prefilled with the SAME
// byte (0xCD), and all 0xF0 bytes are compared. Prefilling both identically is
// what makes the comparison sound in both directions:
//
//   * A byte NEITHER side writes holds 0xCD on both and compares equal, so the
//     +0x78/+0x7C/+0x90/+0xC4/+0xD4 holes cannot manufacture a 100%-failure
//     report (reference_oracle_must_not_compare_unwritten_bytes.md, which cost
//     Wave 6 four false gaps).
//   * A byte OURS writes and theirs does not — the mirror defect, and the one a
//     "compare only the fields we modelled" oracle cannot see at all — reads
//     0x00 against 0xCD and FAILS. That is the case worth catching: it is what
//     a transcription with one store too many looks like.
//
// EFFECTIVENESS CONTROL (reference_probe_needs_an_effectiveness_control.md).
// The site's `worked=` column is not `true`; it is "our body changed at least
// one byte away from the prefill". A body that ran and did nothing would report
// `calls=N worked=0` with divergences=0, which is the exact shape of a green
// line over dead code. The count itself is printed on the first call.
//
// The vtable stores are part of the compare. While the DLL is loaded,
// RT_DATA_SLOT hands back the DLL's own tables, so ours and theirs agree
// exactly — the oracle verifies that we wrote the RIGHT vtable, in the right
// slot, at the right offset. It cannot verify the MINTED table, because the
// mint does not exist on a DLL-present run; that is §7's problem and it is
// stated there rather than implied to be covered here.
//
// ---------------------------------------------------------------------------
// 6. THE ACCESSOR SWEEP PROBE
// ---------------------------------------------------------------------------
// `RtIo_PackageFile_SweepProbe()` runs at the end of
// RtIo_FileSystem_LoadPackages, only when the diff harness is on. At that
// moment `fs->packfiles[0 .. packfileCount)` holds one fully-populated
// PackageFile per mounted package, built by the DLL's ReadPackfile. The probe
// calls every ported accessor on every one of them, and the two index-taking
// accessors (GetFileSize, GetFileOffset) on EVERY FILE IN EVERY PACKAGE. Each
// port then runs its own ours-vs-theirs comparison, so the probe's only job is
// to supply inputs.
//
// It also drives ?Construct@ on two cases retail's own callers would not
// produce together: a fresh 0xF0 scratch block (returns it, constructed) and
// NULL (returns null without touching anything). Two branches, both covered.
//
// WHAT THE POPULATION IS AND IS NOT. `packfileCount` is small — a stock install
// mounts a handful of .pkg plus one per enabled mod — so the per-package
// accessors get a handful of calls each. The per-FILE accessors get one call
// per entry, which is tens of thousands. Both numbers are printed; neither is
// dressed up as the other.
//
// AND THE HOLE THAT LEAVES, stated rather than left for a reader to find. The
// per-package population is small AND may be near-uniform: every package on a
// stock install is built by the same tool, so `?GetVersion@`, `?GetPlatform@`
// and `?CanOverride@` may return the same value on every one of them, and
// `reference_uniform_population_is_not_a_verification.md` is explicit that a
// population of identical values is not a verification no matter how many
// comparisons it makes. What actually pins those three is the disassembly —
// each is a single MOV at a named offset — and the probe prints the DISTINCT
// VALUE COUNT per accessor so a reader can see which rows are genuinely
// exercised and which are one value repeated.
//
// ---------------------------------------------------------------------------
// 7. THE MINT ROW — ??_7PackageFile@IO@@6B@
// ---------------------------------------------------------------------------
// The constructor's surviving vtable store needs `??_7PackageFile@IO@@6B@`
// (0x180155700, ord 1471, EXPORTED) to exist after the DLL is gone. Lane C2's
// generator decides its own set: `tools/rt_mint_gen.py` shells out to
// `tools/runtime_census.py` and takes the unported DATA exports our tree
// REFERENCES, so the RT_DATA_SLOT call below is what puts the row in the table.
// The generated file is regenerated as part of this commit; it is not hand
// edited.
//
// Nine slots. Lane PKG established the count by READING THE BYTES at
// 0x180155700 rather than trusting the mangled name: nine consecutive qwords
// are .text addresses (0x1800259d0, 0x180025150, 0x180025130, 0x1800b6080,
// 0x1800b6090, 0x1800b60a0, 0x1800252d0, 0x180025300, 0x1800252f0) and the
// tenth is zero, followed by the ASCII "%08lX%04hX%04hX…" of the next .rdata
// object. This lane ports slots 1, 2, 3, 4 and 8 (TypeName, Type,
// GetFileVersion, Compile, Construct); slots 0, 5, 6 and 7 (the deleting
// destructor, Serialize, Reconstruct, Destruct) stay named traps, for the
// reasons in §4. A trap that fires says which vtable and which slot, which is
// the whole point of the mint never holding a DLL address.
//
// ---------------------------------------------------------------------------
// 8. THE +0x70 CORRECTION
// ---------------------------------------------------------------------------
// runtime/types/io/FileSystem.h modelled +0x70 as a `uint64_t priority`,
// because the constructor's store is one qword. It is TWO dwords: `?GetPriority@`
// reads the low half and `?GetPlatform@` reads `[RCX+0x74]`. The header now
// says so. The single qword store seeds priority=0x2710 AND platform=0, which
// is why the qword reading was never wrong about the BYTES — only about what
// they mean. Nothing in the tree read the field as a qword (grepped before the
// change), and no offset or static_assert moved.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>          // placement new into the retail hidden return slot
#include <atomic>

#include "../../funcs/rt_port.h"
#include "../../types/io/FileSystem.h"
#include "../../types/io/PackageFile.h"   // B.3.4 PACKAGE+FS: the real class
#include "../../types/system/String.h"
#include "../../src/system/string.h"
#include "../../src/system/pod_bridge.h"

namespace RtIO {

using RtSystem::String;
using RtSystem::Sys;   // the transitional POD->class bridge (pod_bridge.h)

namespace {

// ---- sizes ----------------------------------------------------------------
// sizeof(IO::PackageFile). Not guessed: LoadPackages allocates `MOV EDX,0xf0`
// per package (0x1800ab55b) and the deleting destructor at 0x1800259d0 passes
// `MOV EDX,0xf0` to the array-delete helper. Two independent sites agree.
constexpr size_t kPackageFileSize = 0xF0;

// ---- the type-info record -------------------------------------------------
// ?Type@ (0x180025130) reads `dword ptr [0x1801f9ac8]` and ?TypeName@
// (0x180025150) reads `qword ptr [0x1801f9ad0]` — the class's own
// BaseTypeInfo, at +0x00 and +0x08. ENDGAME B.3.9 TI-REST: that record is the
// class's `DEFINE_TYPE` static (io_type_records.cpp) and the three accessors
// are DECLARE_TYPE's; the atomic slot over the minted record is gone.

// ENDGAME B.3.9 IO: the PutQ/PutD/PutB transcription of ??0PackageFile
// (`PackageFileCtorBody`, with its three minted vptr stamps) is gone; the
// stores are `IO::PackageFile`'s real constructor (PackageFile.h), in the same
// order, and the vptrs are the compiler's.

// ---- oracle typedefs ------------------------------------------------------
typedef void*    (*CtorFn)(void*);
typedef void*    (*ConstructFn)(void*);
typedef uint32_t (*U32ThisFn)(const void*);
typedef int32_t  (*I32ThisFn)(const void*);
typedef bool     (*BoolThisFn)(const void*);
typedef const char* (*CStrThisFn)(const void*);
typedef uint32_t (*U32ThisIntFn)(const void*, int);
typedef String*  (*StrGetterFn)(const void*, String*);
typedef uint32_t (*U32VoidFn)();
typedef int32_t  (*I32ThisFilenameFn)(void*, const void*);

// ENDGAME B.3.1 step 2: the shared "null the slot, AddRef if non-null" body the
// three String getters used to hand-roll is exactly `System::String`'s copy
// ctor (0x180006cb0, `Adopt` in string.cpp) -- null-then-AddRef, no freeing
// branch, which is a getter's whole contract. The three call sites below now
// construct that ctor into their hidden return slot directly.
//
//   180025327  MOV qword ptr [RDX],RAX     ; RAX = 0, the return slot
//   18002532a  MOV RAX,qword ptr [RCX+off]
//   18002532e  TEST RAX,RAX / JZ
//   180025333  MOV qword ptr [RDX],RAX
//   180025336  INC dword ptr [RAX + -0x4]  ; the refcount, block+4

}  // namespace

// ============================================================================
// ENDGAME B.3.9 IO: the ??0PackageFile door (its callers -- LoadPackages, the
// FACTREST replay, Construct, Reconstruct -- build the class directly now) and
// the ?Construct@ slot door are gone.
// ============================================================================
// The reflection triple. All three read the exported BaseTypeInfo at
// 0x1801f9ac8 — ?Type@ and ?TypeId@ are the SAME two instructions at two
// addresses (the virtual and the static spelling), which is why they share a
// body here and are still registered separately.
// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS commit 3: every body below is now a member of the
// real `IO::PackageFile` (runtime/types/io/PackageFile.h); these doors keep
// their names and both registrations and delegate.
//
// *** THE QUALIFIED `IO::PackageFile::` IS LOAD-BEARING ON THE EIGHT VIRTUAL
// *** SLOTS AND IS NOT DECORATION. *** A live PackageFile in this build wears
// the MINTED `??_7PackageFile@IO@@6B@`, whose nine rows resolve back to THESE
// VERY DOORS. A plain virtual call in a door would dispatch through that minted
// table straight back into itself -- infinite recursion, not delegation. The
// qualification suppresses virtual dispatch (B34_BRIEFS section A.1, and
// serialize_family.cpp / encoding.cpp established the pattern in blocks 1-2).
// The non-virtual members below do not need it and carry it anyway, so that a
// reader never has to check which is which.
extern "C" uint32_t RtIo_PackageFile_TypeId() {
    return IO::PackageFile::TypeId();
}

// ============================================================================
// ENDGAME B.3.9 IO: the seven slot doors (Type, TypeName, GetFileVersion,
// Compile, Serialize, Reconstruct, Destruct) and Construct's are gone with the
// minted ??_7PackageFile row, their only reader. ?TypeId@ is a static, not a
// slot, and keeps its door.
// ============================================================================
// The three String getters. `self->name/author/description` are RtSystem::String
// MEMBERS of the not-yet-converted PackageFileView, reached through the one
// transitional bridge (pod_bridge.h); `ret` is the raw hidden return slot, so
// the copy ctor is constructed into it -- see the note above.
// ============================================================================
// The three getters return `System::String` BY VALUE, so retail's ABI puts the
// hidden return slot in RDX and the door's `ret` IS that slot. The member's
// by-value return fills it through the same ABI, so the door constructs the
// member's result into it -- one AddRef and one release, exactly as before.
extern "C" String* RtIo_PackageFile_GetName(const PackageFileView* self, String* ret) {
    new (ret) System::String(
        reinterpret_cast<const IO::PackageFile*>(self)->IO::PackageFile::GetName());
    return ret;
}

extern "C" String* RtIo_PackageFile_GetAuthor(const PackageFileView* self, String* ret) {
    new (ret) System::String(
        reinterpret_cast<const IO::PackageFile*>(self)->IO::PackageFile::GetAuthor());
    return ret;
}

extern "C" String* RtIo_PackageFile_GetDescription(const PackageFileView* self, String* ret) {
    new (ret) System::String(
        reinterpret_cast<const IO::PackageFile*>(self)->IO::PackageFile::GetDescription());
    return ret;
}

// ============================================================================
// The scalar field reads. Each is one MOV; the offset IS the function.
// ============================================================================
// Each door delegates to the like-named member. The third macro argument used
// to be the POD FIELD; it is the MEMBER NAME now, because the offset is spelled
// once, in the member body, and nowhere else.
#define RT_PKG2_SCALAR_GETTER(FN, SITE, MEMBER, TYPE, ORACLE_FN, MANGLED)      \
    extern "C" TYPE FN(const PackageFileView* self) {                          \
        return static_cast<TYPE>(                                              \
            reinterpret_cast<const IO::PackageFile*>(self)                     \
                ->IO::PackageFile::MEMBER());                                  \
    }

RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetVersion, "IO::PackageFile::GetVersion",
                      GetVersion, int32_t, I32ThisFn,
                      "?GetVersion@PackageFile@IO@@QEBAHXZ")            // +0x68
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetUserVersion, "IO::PackageFile::GetUserVersion",
                      GetUserVersion, int32_t, I32ThisFn,
                      "?GetUserVersion@PackageFile@IO@@QEBAHXZ")        // +0x6C
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetPlatform, "IO::PackageFile::GetPlatform",
                      GetPlatform, int32_t, I32ThisFn,
                      "?GetPlatform@PackageFile@IO@@QEBAHXZ")           // +0x74
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetPriority, "IO::PackageFile::GetPriority",
                      GetPriority, int32_t, I32ThisFn,
                      "?GetPriority@PackageFile@IO@@QEBAHXZ")           // +0x70
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetFileCount, "IO::PackageFile::GetFileCount",
                      GetFileCount, int32_t, I32ThisFn,
                      "?GetFileCount@PackageFile@IO@@QEBAHXZ")          // +0xB8
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetCRC, "IO::PackageFile::GetCRC",
                      GetCRC, uint32_t, U32ThisFn,
                      "?GetCRC@PackageFile@IO@@QEBAIXZ")                // +0xA8
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetIconWidth, "IO::PackageFile::GetIconWidth",
                      GetIconWidth, int32_t, I32ThisFn,
                      "?GetIconWidth@PackageFile@IO@@QEBAHXZ")          // +0x78
RT_PKG2_SCALAR_GETTER(RtIo_PackageFile_GetIconHeight, "IO::PackageFile::GetIconHeight",
                      GetIconHeight, int32_t, I32ThisFn,
                      "?GetIconHeight@PackageFile@IO@@QEBAHXZ")         // +0x7C

#undef RT_PKG2_SCALAR_GETTER

// 0x1800254f0 is `MOVZX EAX,byte ptr [RCX+0xac]` — a ZERO extension of the
// byte, not a sign extension and not a dword read. The field is the same one
// LoadPackages' index pass tests, so a wrong width here would change package
// precedence, not just a returned bool.
extern "C" bool RtIo_PackageFile_CanOverride(const PackageFileView* self) {
    return reinterpret_cast<const IO::PackageFile*>(self)->IO::PackageFile::CanOverride();
}

// ============================================================================
// The two index-taking entry accessors.
// ============================================================================
//   1800254a0  MOV RAX,qword ptr [RCX+0xb0]
//   1800254a7  MOVSXD R8,EDX          <- SIGN extension of the int index
//   1800254aa  ADD R8,R8              <- *2, and the *8 is in the SIB scale
//   1800254ad  MOV EAX,dword ptr [RAX + R8*0x8 + 0xc]
//
// So the address is `entries + index*16 + 0xc`, and the index is SIGN-extended.
// Ghidra's C prints that MOVSXD as a zero extension
// (reference_ghidra_c_output_vs_disassembly.md, three Wave 6 defects); it is
// written as int64 here because that is what the instruction says. There is no
// bounds check in either body, and none is added — the caller bounds it with
// GetFileCount, and a guard here would skip work rather than restore it.
extern "C" uint32_t RtIo_PackageFile_GetFileSize(const PackageFileView* self, int index) {
    const uint32_t ours =
        reinterpret_cast<const IO::PackageFile*>(self)->IO::PackageFile::GetFileSize(index);
    // the raw DLL door, not bare the raw DLL door: the sweep probe calls this once
    // per FILE in every package — tens of thousands of times — and the raw DLL door
    // is GetProcAddress under the loader lock on every call
    // (reference_rtoriginal_is_getprocaddress.md). Both reach the shipped body;
    // only one of them does it without a syscall per entry.
    return ours;
}

extern "C" uint32_t RtIo_PackageFile_GetFileOffset(const PackageFileView* self, int index) {
    return reinterpret_cast<const IO::PackageFile*>(self)->IO::PackageFile::GetFileOffset(index);
}


}  // namespace RtIO

// ============================================================================
// Registration
// ============================================================================
// EVERY row gets both doors. RegisterRuntimeExport is what our own callers
// reach; RegisterRuntimeAddr is what a call BY ADDRESS reaches, and this tree
// has both shapes (reference_bypassed_port_needs_a_source_checker.md — a port
// reached by address silently uses the DLL, and no run reports it).
//
// Registering these ADDRESSES is safe on a DLL-present run in a way the
// allocator's four were not: none of the 21 is patched, detoured or read as
// shipped bytes by anything in this tree (grepped across runtime/, standalone/,
// engine/, code/ and dx11/ for each address before adding it), so there is no
// `ResolveAddr consults the port registry FIRST` hazard here
// (reference_resolveaddr_consults_port_registry_first.md). That is why this
// file registers unconditionally where runtime/src/system/allocator.cpp
// registers only at cutover.
int RtIoPackageFileRegister() {
    using namespace RtIO;
    int n = 0;

    RegisterRuntimeExport("?TypeId@PackageFile@IO@@SAIXZ",
                          (void*)&RtIo_PackageFile_TypeId); ++n;
    RegisterRuntimeAddr(0x180025140, (void*)&RtIo_PackageFile_TypeId);

    RegisterRuntimeExport("?GetName@PackageFile@IO@@QEBA?AVString@System@@XZ",
                          (void*)&RtIo_PackageFile_GetName); ++n;
    RegisterRuntimeAddr(0x180025310, (void*)&RtIo_PackageFile_GetName);

    RegisterRuntimeExport("?GetAuthor@PackageFile@IO@@QEBA?AVString@System@@XZ",
                          (void*)&RtIo_PackageFile_GetAuthor); ++n;
    RegisterRuntimeAddr(0x180025350, (void*)&RtIo_PackageFile_GetAuthor);

    RegisterRuntimeExport("?GetDescription@PackageFile@IO@@QEBA?AVString@System@@XZ",
                          (void*)&RtIo_PackageFile_GetDescription); ++n;
    RegisterRuntimeAddr(0x180025390, (void*)&RtIo_PackageFile_GetDescription);

    RegisterRuntimeExport("?GetVersion@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetVersion); ++n;
    RegisterRuntimeAddr(0x1800253d0, (void*)&RtIo_PackageFile_GetVersion);

    RegisterRuntimeExport("?GetUserVersion@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetUserVersion); ++n;
    RegisterRuntimeAddr(0x1800253e0, (void*)&RtIo_PackageFile_GetUserVersion);

    RegisterRuntimeExport("?GetPlatform@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetPlatform); ++n;
    RegisterRuntimeAddr(0x1800253f0, (void*)&RtIo_PackageFile_GetPlatform);

    RegisterRuntimeExport("?GetFileCount@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetFileCount); ++n;
    RegisterRuntimeAddr(0x180025440, (void*)&RtIo_PackageFile_GetFileCount);

    RegisterRuntimeExport("?GetFileSize@PackageFile@IO@@QEBAIH@Z",
                          (void*)&RtIo_PackageFile_GetFileSize); ++n;
    RegisterRuntimeAddr(0x1800254a0, (void*)&RtIo_PackageFile_GetFileSize);

    RegisterRuntimeExport("?GetFileOffset@PackageFile@IO@@QEBAIH@Z",
                          (void*)&RtIo_PackageFile_GetFileOffset); ++n;
    RegisterRuntimeAddr(0x1800254c0, (void*)&RtIo_PackageFile_GetFileOffset);

    RegisterRuntimeExport("?GetPriority@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetPriority); ++n;
    RegisterRuntimeAddr(0x1800254e0, (void*)&RtIo_PackageFile_GetPriority);

    RegisterRuntimeExport("?CanOverride@PackageFile@IO@@QEBA_NXZ",
                          (void*)&RtIo_PackageFile_CanOverride); ++n;
    RegisterRuntimeAddr(0x1800254f0, (void*)&RtIo_PackageFile_CanOverride);

    RegisterRuntimeExport("?GetCRC@PackageFile@IO@@QEBAIXZ",
                          (void*)&RtIo_PackageFile_GetCRC); ++n;
    RegisterRuntimeAddr(0x180025500, (void*)&RtIo_PackageFile_GetCRC);

    RegisterRuntimeExport("?GetIconWidth@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetIconWidth); ++n;
    RegisterRuntimeAddr(0x180025510, (void*)&RtIo_PackageFile_GetIconWidth);

    RegisterRuntimeExport("?GetIconHeight@PackageFile@IO@@QEBAHXZ",
                          (void*)&RtIo_PackageFile_GetIconHeight); ++n;
    RegisterRuntimeAddr(0x180025520, (void*)&RtIo_PackageFile_GetIconHeight);

    // ---- the factory pair (B39_RULES D2): the class's OWN statics, both
    // doors, replacing the FACTREST row (rt_factories_io.{cpp,inc}, deleted).
    // The class's own `DEFINE_TYPE` record (io_type_records.cpp) carries
    // both as its factory pointers.
    RegisterRuntimeExport("?CreateNewObject@PackageFile@IO@@SAPEAVBase@Object@@XZ",
                          (void*)&IO::PackageFile::CreateNewObject); ++n;
    RegisterRuntimeAddr(0x180025240, (void*)&IO::PackageFile::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@PackageFile@IO@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&IO::PackageFile::CreateInPlaceObject); ++n;
    RegisterRuntimeAddr(0x1800252c0, (void*)&IO::PackageFile::CreateInPlaceObject);
    // DELIBERATELY ABSENT — full reasons in §4.
    //   ?ReadPackfile@PackageFile@IO@@…   0x1800b46b0  ord 3461  6,597 bytes  <- NEXT WALL
    //   ??1PackageFile@IO@@UEAA@XZ        0x1800b4540  ord  639  (5 unported callees)
    //   FUN_1800259d0 (deleting dtor, vtable slot 0) -- CLOSED Wave 25 VTIO
    //   ?Reconstruct@ 0x1800252d0 / ?Destruct@ 0x180025300  (drive slot 0)
    //       -- CLOSED, Wave 27 lane IOTEXT, registered above.
    //   ?Serialize@   0x1800b60a0  (Encoding vtable, save path)
    //   ?GetFileInfo@ 0x180025400 / ?GetIconData@ / ?GetPreviewData@
    //   the ToolKit half: WritePackfile, ExtractPackFile, BuildPackageFile,
    //   AreResourcesEqual, BuildFileSet, ClosePackFile, copy ctor, operator=

    // A LANE-OWNED IDENTITY MARKER, and it exists because the obvious gate broke.
    // This lane verifies "the exe that RAN is mine" by grepping the log rather
    // than trusting an on-disk md5 (which tells you what is on disk when you
    // look, not what ran). The first version of that gate matched
    // `[RtPort] IO: 77 exports registered` -- and lane MOD then landed
    // IO::Modification on the same RtIoPortInit, which moves the IO total and
    // would have made the gate fire falsely on a perfectly good run.
    //
    // A shared TOTAL is the wrong thing to key an identity check on: it changes
    // whenever any other lane lands in the same namespace. This line changes only
    // when THIS file changes, so the gate keys on it instead.
    printf("[RtPort]   IO::PackageFile (lane PKG2): %d exports registered\n", n);
    return n;
}

// ============================================================================
// ENDGAME B.3.4 PACKAGE+FS — `IO::PackageFile`'s member bodies
// ============================================================================
//
// A member of `IO::X` cannot be DEFINED inside `namespace RtIO` — `IO` and
// `RtIO` are siblings, so a definition has to appear in a namespace ENCLOSING
// the class (B34_BRIEFS section A.2). This block therefore sits after
// `}  // namespace RtIO` and reaches that namespace's file-local helpers as
// `RtIO::X`, which works for its anonymous-namespace members too in this same
// translation unit.
//
// Inside `namespace IO` a bare `PackageFile` binds to the REAL CLASS and a bare
// `String`, `Filename` or `Stream` would bind to whatever `IO` sees first, so
// every POD is spelled `RtIO::` / `RtSystem::` and every real class `System::`
// (B34_RULES section 7.2, THE GREP HAZARD).
//
// ENDGAME B.3.9 IO: EVERYTHING MOVED. The constructor and destructor are the
// class's own (PackageFile.h / package_file_dtor.cpp), slot 0 is concrete, and
// every PackageFile -- LoadPackages, the factory pair below, Construct,
// Reconstruct -- wears the compiler's table. The minted row, its RT_DELDTOR
// row and the FACTREST replay are retired.
// ============================================================================

namespace IO {

// ---- the reflection triple, slots 1 and 2 plus the static companion --------
// `?Type@` (0x180025130), `?TypeId@` (0x180025140) and `?TypeName@`
// (0x180025150) are DECLARE_TYPE's bodies over the class's own record
// (ENDGAME B.3.9 TI-REST; io_type_records.cpp). Neither reads RCX.

// ---- slots 3 and 4: the two constant-returning slots -----------------------
// 0x1800b6080: MOV EAX,0x5 ; RET. The .pkg format version this build writes and
// accepts. A constant in the CODE, not a field -- reading it off the object
// would be wrong even though it would usually agree.
int32_t PackageFile::GetFileVersion() const {
    return 5;
}

// 0x1800b6090: MOV EAX,0x1 ; RET. `External::CompileResult`, and the value is 1
// on every path -- a package is never "compiled", so the slot reports the same
// result whatever Filename it is handed and the parameter is genuinely unread.
Object::External::CompileResult PackageFile::Compile(const System::Filename& path) {
    (void)path;
    return 1;
}

// ---- slots 6 and 7 ---------------------------------------------------------
//   ?Destruct@        180025300  MOV RAX,qword ptr [RCX]   ; this->vtable
//                     180025303  XOR EDX,EDX               ; flags = 0
//                     180025305  JMP  qword ptr [RAX]      ; TAIL, slot 0
//
//   ?Reconstruct@     1800252d6  MOV RAX,qword ptr [RCX]   ; this->vtable
//                     1800252d9  XOR EDX,EDX               ; flags = 0
//                     1800252de  CALL qword ptr [RAX]      ; slot 0
//                     1800252e8  JMP  0x1800b4440          ; TAIL, ??0PackageFile
//
// FLAGS = 0, which in the deleting-destructor family means DESTRUCT IN PLACE
// AND DO NOT FREE: the object's storage belongs to the factory that owns it.
// Passing 1 would free a block the caller still holds. The constant is read off
// `XOR EDX,EDX` at both sites, not assumed.
// ENDGAME B.3.9 IO: the object wears the compiler's table, so the virtual
// slot-0 call is `DeletingDtor(0)` (flags 0: destroy in place, no free).
void PackageFile::Destruct() {
    DeletingDtor(0);                                      // 180025305, flags 0
}

// "Destroy in place, then run the constructor over the same storage." The
// declared return is `Object::External*`; retail returns whatever the
// tail-jumped constructor leaves in RAX, which `??0PackageFile@IO@@` sets to
// `this` -- placement construction over the same storage.
Object::External* PackageFile::Reconstruct() {
    DeletingDtor(0);                                      // 1800252de, flags 0
    return ::new (this) PackageFile();                    // 1800252e8 ??0PackageFile
}

// ---- slot 8: PackageFile's OWN virtual -------------------------------------
//   1800252f0  TEST RCX,RCX
//   1800252f3  JNZ 0x1800b4440      <- tail jump straight into the constructor
//   1800252f9  XOR EAX,EAX / RET
//
// See runtime/types/io/PackageFile.h's banner for the four pieces of retail
// evidence that this virtual is declared on `IO::PackageFile` and not on an
// unmodelled class between `Object::External` and it.
// ENDGAME B.3.9 IO: the retired door held retail's null test; a virtual call
// always has a `this`, so the member is the tail jump alone.
Object::External* PackageFile::Construct() {
    return ::new (this) PackageFile();                    // 1800252f3 ??0PackageFile
}

// ?CreateNewObject@PackageFile@IO@@SAPEAVBase@Object@@XZ  0x180025240
// The FACTREST row this replaces: ?MemAlloc@MemoryAllocator (0x1800c7a50, the
// same function Object::detail::InternalMemAlloc reaches) of 0xf0 bytes,
// align 8; retail's null branch; then the constructor (call at 0x1800252a1).
Object::Base* PackageFile::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PackageFile), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) PackageFile();
}

// ?CreateInPlaceObject@PackageFile@IO@@SAPEAVBase@Object@@PEAX@Z  0x1800252c0
// retail's null test, then the constructor (call at 0x1800252c3).
Object::Base* PackageFile::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) PackageFile();
}

// ---- the three String getters ----------------------------------------------
// Retail's body is the null-then-AddRef idiom, which is `System::String`'s copy
// constructor exactly (0x180006cb0): it nulls the return slot, loads the
// member, and AddRefs only if it is non-null. The members keep the POD spelling
// (PackageFile.h's banner says why), so each is reached through pod_bridge.h's
// `Sys()`.
//
//   180025327  MOV qword ptr [RDX],RAX     ; RAX = 0, the return slot
//   18002532a  MOV RAX,qword ptr [RCX+off]
//   18002532e  TEST RAX,RAX / JZ
//   180025333  MOV qword ptr [RDX],RAX
//   180025336  INC dword ptr [RAX + -0x4]  ; the refcount, block+4
System::String PackageFile::GetName() const {
    return System::String(RtSystem::Sys(_name));               // +0x40
}

System::String PackageFile::GetAuthor() const {
    return System::String(RtSystem::Sys(_author));             // +0x48
}

System::String PackageFile::GetDescription() const {
    return System::String(RtSystem::Sys(_description));        // +0x50
}

// ---- the scalar field reads ------------------------------------------------
// Each is one MOV; the offset IS the function, and these eight bodies are now
// the ONLY place in the tree that spells it.
int32_t  PackageFile::GetVersion() const     { return _version; }      // 0x1800253d0 +0x68
int32_t  PackageFile::GetUserVersion() const { return _userVersion; }  // 0x1800253e0 +0x6c
int32_t  PackageFile::GetPriority() const    { return _priority; }     // 0x1800254e0 +0x70
int32_t  PackageFile::GetPlatform() const    { return _platform; }     // 0x1800253f0 +0x74
int32_t  PackageFile::GetIconWidth() const   { return _iconWidth; }    // 0x180025510 +0x78
int32_t  PackageFile::GetIconHeight() const  { return _iconHeight; }   // 0x180025520 +0x7c
uint32_t PackageFile::GetCRC() const         { return _crc; }          // 0x180025500 +0xa8
int32_t  PackageFile::GetFileCount() const   { return _entryCount; }   // 0x1800254d0 +0xb8

// 0x1800254f0 is `MOVZX EAX,byte ptr [RCX+0xac]` -- a ZERO extension of the
// byte, not a sign extension and not a dword read. The field is the same one
// LoadPackages' index pass tests, so a wrong width here would change package
// precedence, not just a returned bool.
bool PackageFile::CanOverride() const {
    return _overridable != 0;
}

// ---- the two index-taking entry accessors ----------------------------------
//   1800254a0  MOV RAX,qword ptr [RCX+0xb0]
//   1800254a7  MOVSXD R8,EDX          <- SIGN extension of the int index
//   1800254aa  ADD R8,R8              <- *2, and the *8 is in the SIB scale
//   1800254ad  MOV EAX,dword ptr [RAX + R8*0x8 + 0xc]
//
// So the address is `entries + index*16 + 0xc`, and the index is SIGN-extended.
// Ghidra's C prints that MOVSXD as a zero extension
// (reference_ghidra_c_output_vs_disassembly.md, three Wave 6 defects); it is
// written as int64 here because that is what the instruction says. There is NO
// bounds check in either body and none is added -- the caller bounds it with
// GetFileCount, and a guard here would skip work rather than restore it.
uint32_t PackageFile::GetFileSize(int index) const {
    const RtIO::PackageFileEntry* const e =
        static_cast<const RtIO::PackageFileEntry*>(_entries) + static_cast<int64_t>(index);
    return e->size;                                             // +0x0C
}

uint32_t PackageFile::GetFileOffset(int index) const {
    const RtIO::PackageFileEntry* const e =
        static_cast<const RtIO::PackageFileEntry*>(_entries) + static_cast<int64_t>(index);
    return e->offset;                                           // +0x08
}

}  // namespace IO
