// ============================================================================
// runtime/src/ui/dialog_ctor_dtor.cpp — Wave 10 lane DLG2
//   `??0Dialog@UI@@QEAA@XZ`  0x180112db0  — the default UI::Dialog constructor
//   `??1Dialog@UI@@UEAA@XZ`  0x180112e40  — the virtual UI::Dialog destructor
// ============================================================================
//
// WHY THESE TWO ROWS EXIST AT ALL, AND WHY THAT IS PROGRESS
// ---------------------------------------------------------------------------
// Neither name was in the UI census before 2026-09-08. They ENTERED it as the
// direct consequence of lane UI3 closing the `UI::DialogInstance` family:
// retail INLINES both bodies into the 6-argument
// `?AddController@DialogInstance@UI@@` (its stack temporary is a `UI::Dialog`,
// proved by the `??_7Dialog@UI@@6B@` stamp at 0x180112deb), and UI3's port calls
// the out-of-line exports by name instead of re-inlining them.
//
// So the port replaced an INLINED, UNNAMED dependency with a NAMED, SCOPEABLE
// export. That is `reference_porting_can_grow_the_cutover_surface.md` working
// as intended, not a regression: the two bodies always ran on this path, and
// the census could not see them only because nothing in our tree named them.
// This file closes both.
//
// ---------------------------------------------------------------------------
// THE TWO MANGLINGS, READ OUT OF THE SHIPPED EXPORT DIRECTORY FIRST
// ---------------------------------------------------------------------------
// `tools/pe_exports.py` against C:\GOG Games\Banished\Runtime-x64.dll
// (4,926 exports), BEFORE a line of body was written — a name the DLL does not
// contain resolves to NULL in silence, the port still registers, and the census
// still counts it (reference_resolveexport_phantom_names.md):
//
//   0x180112db0  ??0Dialog@UI@@QEAA@XZ    present
//   0x180112e40  ??1Dialog@UI@@UEAA@XZ    present
//
// Both at exactly the addresses the disassembly gives. NO PHANTOM.
//
// *** THE TWO SPELLINGS DIFFER AND BOTH ARE LOAD-BEARING. *** The ctor is
// `QEAA` (public, non-virtual). The dtor is `UEAA` — the VIRTUAL spelling.
// `??1Dialog@UI@@QEAA@XZ` is NOT in the export table and would resolve NULL
// without a word of complaint (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md,
// which is exactly this defect one class over).
//
// The three vtables the ctor stamps were checked the same way and are exported
// at exactly the addresses the shipped `LEA`s name:
//   ??_7Base@Object@@6B@      0x180154788
//   ??_7External@Object@@6B@  0x1801547a0
//   ??_7Dialog@UI@@6B@        0x1801562a0
// They are read through `ResolveExport` rather than minted. A ported ctor that
// mints its own vtable puts a SECOND vtable in play and the object then fails
// every `IsDerivedFrom` the DLL performs on it — the `??_7Geometry@Graphics@@6B@`
// lever this project has already paid for once.
//
// ---------------------------------------------------------------------------
// `??0Dialog@UI@@QEAA@XZ` — the shipped body, 0x2b instructions
// ---------------------------------------------------------------------------
//   180112dc1  LEA RAX,[0x180154788] ; MOV [RCX],RAX      <- Base vtable
//   180112dcb  LEA RAX,[0x1801547a0] ; MOV [RCX],RAX      <- External vtable
//   180112dd5  LEA RAX,[RCX+0x8]     ; MOV [RSP+0x28],RAX <- SEH-visible temp
//   180112dde  XOR EDX,EDX
//   180112de0  MOV [RAX],RDX                              <- +0x08 THROUGH RAX
//   180112de3  MOV [RCX+0x10],RDX
//   180112de7  MOV [RCX+0x18],RDX
//   180112deb  LEA RAX,[0x1801562a0] ; MOV [RCX],RAX      <- Dialog vtable
//   180112df5  +0x20,+0x28 = 0 (qwords)
//   180112dfd  MOV dword [RCX+0x30],0xffffffff            <- -1, NOT zero
//   180112e04  MOV dword [RCX+0x34],EDX
//   180112e07  MOV qword [RCX+0x38],RDX
//   180112e0b  MOV dword [RCX+0x40],EDX                   <- DWORD; +0x44 UNTOUCHED
//   180112e0e  +0x48 … +0x80 = 0 (qwords, ten of them)
//   180112e31  MOV RAX,RCX ; RET                          <- returns `this`
//
// THREE THINGS ONLY THE DISASSEMBLY SHOWS
// (reference_ghidra_c_output_vs_disassembly.md):
//
//  (a) THE FIRST QWORD IS WRITTEN THREE TIMES. Base, then External, then Dialog
//      — the ordinary MSVC base-ctor chain, fully inlined. The first two stores
//      are dead in the shipped binary, and they are TRANSCRIBED ANYWAY. "It is
//      provably overwritten in this build" is a claim a port should not bake in,
//      and a debugger single-stepping our body should see what it sees in retail.
//
//  (b) `+0x44` IS NEVER WRITTEN. 0x180112e0b is a DWORD store to +0x40 and the
//      next store is a QWORD at +0x48. A "tidy" port that zeroes +0x40 as a
//      qword would write four bytes retail leaves holding whatever the caller's
//      buffer held — and on the 6-arg AddController's path that is UNINITIALISED
//      STACK. The oracle below therefore EXCLUDES +0x44..+0x47 from its compare,
//      counts the exclusion, and prints it. Comparing bytes neither side writes
//      fails 100 % of calls and reads as a catastrophic port defect
//      (reference_oracle_must_not_compare_unwritten_bytes.md).
//
//  (c) `+0x30` IS -1, NOT 0. The single most likely transcription slip in a
//      body that is otherwise 16 zero stores, and it is what negative control
//      (i) below corrupts.
//
// THE CTOR'S WRITTEN EXTENT IS +0x00..+0x87 (the last store is a QWORD at
// +0x80). `runtime/types/ui/Dialog.h` declares out to +0x7f and carries NO
// `sizeof` assert, deliberately. This lane does not invent one either: it adds
// the one member the ctor proves is there (+0x80) and says the size is still
// unknown, because a ctor's written extent is a LOWER BOUND on a class size,
// never the size itself.
//
// ---------------------------------------------------------------------------
// `??1Dialog@UI@@UEAA@XZ` — the shipped body
// ---------------------------------------------------------------------------
//   180112e56  LEA RAX,[0x1801562a0] ; MOV [RCX],RAX      <- re-stamp Dialog vtbl
//   180112e60  ADD RCX,0x78 ; CALL 0x180009460            <- ~Array on +0x78
//   180112e6a  LEA RCX,[RBX+0x68] ; CALL 0x180039440      <- ~Array on +0x68
//   180112e74  LEA RCX,[RBX+0x58] ; CALL 0x180039360      <- ~Array on +0x58
//   180112e7e  LEA RCX,[RBX+0x48] ; CALL 0x180039280      <- ~Array on +0x48
//   180112e88  the System::String release at +0x28 (see below)
//   180112eb2  MOV RCX,RBX ; JMP 0x1800bc950              <- TAIL-JUMP ~External
//
// THE STRING RELEASE IS RETAIL'S ASYMMETRIC ONE, transcribed rather than
// tidied. `_spriteName` at +0x28 is a refcounted `char*` whose count sits at
// `data - 4`:
//   * count reaches zero -> the FIELD is first rewritten to `data - 8`
//     (`ADD qword [RBX+0x28],-0x8`) and THAT is what is handed to `MemFree`.
//     The field is LEFT holding `data - 8`; it is not nulled.
//   * count is still positive -> the field IS nulled.
// The two branches leave the field in different states and that difference is
// observable. `??1DialogInstance@` and `?CreateInstance@ElementDescription@`
// both already reproduce it; this is the third instance of the same shape.
//
// ---------------------------------------------------------------------------
// EVERY CALLEE, AND ITS DISPOSITION — the honest version
// ---------------------------------------------------------------------------
//   0x180009460  ~Array  (+0x78)   PORTED  J1_ENTRY, array.cpp (lane UIC)
//   0x180039440  ~Array  (+0x68)   PORTED  J1_ENTRY, array.cpp (lane UIC)
//   0x180039360  ~Array  (+0x58)   *** NOT PORTED — DLL, by address ***
//   0x180039280  ~Array  (+0x48)   *** NOT PORTED — DLL, by address ***
//   0x1800c7ad0  ?MemFree@         PORTED  (Wave 2 / lane ALLOC), by address
//   0x1800bc950  ~External         PORTED  (external.cpp), by address
//
// THE TWO UNPORTED ONES ARE STATED, NOT GLOSSED. WAVE10_LANE_UI3.md §7b called
// all four "the same family" and suggested two more `J1_ENTRY` lines. **THAT IS
// WRONG AND THIS LANE MEASURED IT RATHER THAN INHERITING IT**
// (reference_holdback_blockers_are_stale_check_them_first.md). All 115 leading
// bytes of each were read with Ghidra `read_memory` and compared against the
// registered J1 reference 0x180033960:
//
//   0x180009460 / 0x180039440   J1 shape: `MOV RDI,RCX ; MOV RSI,[RCX] ; TEST`
//   0x180039360 / 0x180039280   `MOV RDI,RCX ; MOV R8D,[RCX+8] ; MOV RDX,[RCX]
//                                ; CALL rel32` — an EXTRA element-destroy pass
//                                before the free (targets 0x180039641 and
//                                0x180039581 respectively, two DIFFERENT helpers)
//
// They are `~Array<T>` for a T that HAS a destructor. Registering them as J1
// would have silently replaced a body that destroys every element with one that
// only frees the block — a leak, landed under a comment claiming a byte-level
// identity that does not hold. They are called BY ADDRESS through
// `ResolveAddr(Module::Runtime, …)`, which reaches the DLL today and our port
// the moment someone lands one, and they are recorded in the lane doc as this
// port's OWED cutover residue.
//
// Neither is exported (checked against all 4,926 export names), so the address
// door here is a genuine unexported internal and not the "the comment said
// unexported and it was merely private" mistake
// (reference_aeaa_is_private_not_unexported.md).
//
// ---------------------------------------------------------------------------
// THE ALLOCATOR, AND WHY IT IS NOT A BLOCKER
// ---------------------------------------------------------------------------
// The dtor frees. `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` is reached BY
// ADDRESS (0x1800c7ad0), which is the discipline
// `runtime/src/collection/array.cpp` set three waves ago: that address is the
// ported allocator. THERE IS NO CONFIGURATION IN WHICH THE TWO SIDES SEE
// DIFFERENT HEAPS.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1 decision 1.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "../../types/collection/Collection.h" // ENDGAME B.3.2 step 2 — Array<void*> bridge
#include "../../types/system/String.h"
#include "../../types/ui/Dialog.h"

// ============================================================================
// ENDGAME B.3.9 block UI-D: the two DOORS this file carried are RETIRED. The
// constructor above is `UI::Dialog::Dialog()` (Dialog.h, the three vptr stores
// are the compiler's); the destructor is `UI::Dialog::~Dialog()` below, which
// the retired `RtUiDialog_Dtor` door ran by hand. Their callers -- the factory
// pair, Construct/Reconstruct and the four stack temporaries retail inlines --
// spell `::new (p) UI::Dialog()` and `~T()` (slot 0, flags 0). With the doors go the
// `[DLG2] FIRST LIVE CALL` lines.
// ============================================================================
// ============================================================================
// `??1Dialog@UI@@UEAA@XZ`  0x180112e40
// ============================================================================
namespace UI {

Dialog::~Dialog() {
    // 180112e56: the re-stamp of the Dialog vtable is the compiler's.

    // THE FOUR ARRAYS, DESCENDING: +0x78, +0x68, +0x58, +0x48.
    // ENDGAME B.3.2 step 2 — the first two (+0x78 `_references`, +0x68
    // `_templates`) are `array.cpp`'s J1 `~Array` (destroy == null, i.e.
    // trait-equivalent for a trivially destructible element): `_references`/
    // `_templates` are `void**` + count + capacity, laid out contiguously
    // exactly like `Collection::Array<void*>` (data/size/capacity), so the
    // conversion bridges at the use site with a `reinterpret_cast`.
    // LEFT: 0x180009460 has no named body; the triple is viewed as the Array.
    reinterpret_cast<Collection::Array<void*>*>(&_references)->~Array();  // +0x78 retail 0x180009460  // LEFT: H5 Dialog::_references is a raw {ptr,count,cap} triple, not an Array member (its dtor would start to run)
    rtcoll::RtColl_ArrayDtor_180039440(&_templates);                      // +0x68 retail 0x180039440

    // The last two (+0x58 `_namedTemplates`, +0x48 `_controllerConfigs`) are
    // `array_destroy.cpp`'s J2 entries (`RtColl_ArrayDtorElem_180039360` /
    // `_180039280`): they destroy each element's embedded `System::String`
    // via a `DestroySpec` — a different mechanism from `rtcoll::ArraySpec` —
    // so they stay free-function calls, by name.
    rtcoll::RtColl_ArrayDtorElem_180039360(&_namedTemplates);          // 180112e78  +0x58
    rtcoll::RtColl_ArrayDtorElem_180039280(&_controllerConfigs);       // 180112e82  +0x48

    // ---- the System::String release at +0x28, ASYMMETRY AND ALL ------------
    // String's own Release (0x1800c9870): the freeing branch leaves the field
    // at `data - 8`, the sharing branch nulls it (180112e88..180112eaa).
    System::StringPrivateAccess::Release(
        *reinterpret_cast<System::String*>(&_spriteName));   // LEFT: H5 a System::String held as raw char* storage (class not converted this pass)

    // 180112eba: the TAIL-JMP to ~External is the compiler's chain.
}

}  // namespace UI
