// ============================================================================
// runtime/src/object/deleting_dtor.cpp — Wave 9 lane K
// The unnamed deleting-destructor family: 37 vtable slot-0 entries, ONE body
// ============================================================================
//
// WHAT THESE ARE, AND WHAT THE BRIEF ASSUMED THEY WERE
//
// Lane C2's mint found that slot 0 of every minted `??_7` vtable but one is an
// UNNAMED function — no export, no symbol — and that none of them was ported.
// 37 slots, the most-dispatched slot in the whole minted surface, every one a
// trap that aborts. Nothing that destroys a Runtime object can work after
// cutover until they exist.
//
// The brief priced them as MSVC's SCALAR deleting destructor:
//
//     ??_G<Class>@@UEAAPEAXI@Z(this, flags):
//         ~Class(this); if (flags & 1) operator delete(this); return this;
//
// **They are not.** Every one of the 37 is the VECTOR deleting destructor,
// `??_E<Class>@@UEAAPEAXI@Z`, which carries a second flag bit and a whole
// second branch the scalar form does not have:
//
//     ??_E<Class>(this, flags):
//         if (flags & 2) {                               // delete[] arrived here
//             int n = *(const int32_t*)((char*)this - 8); // the array cookie
//             `eh vector destructor iterator`(this, sizeof(Class), n, ~Class);
//             if (flags & 1) operator delete((char*)this - 8);
//             return (char*)this - 8;                     // ... and returns the BLOCK
//         }
//         ~Class(this);
//         if (flags & 1) operator delete(this);
//         return this;
//
// The difference is not cosmetic. Porting the assumed `??_G` shape would have
// produced a function that, on a `delete[]`, destroys ONE element instead of n,
// hands `operator delete` a pointer eight bytes past the block it must free —
// which in this allocator is a write through a wrong boundary tag, this
// project's worst failure family (reference_load_wild_write_high_dword_7fc.md,
// reference_internalfactory_swapremove_wild_write.md) — and returns the wrong
// pointer to the caller. It would have looked correct in every scalar `delete`,
// which is most of them.
//
// Read out of the disassembly, never Ghidra's decompiled C
// (reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// THE CLUSTER: 37 ADDRESSES, ONE BODY, TWO CONSTANTS
// ---------------------------------------------------------------------------
// All 37 were disassembled. They are ONE compiler-generated body parameterized
// by exactly two values — the per-class destructor and `sizeof(Class)` — and by
// nothing else:
//
//   * `operator delete` is NOT a parameter. Ten distinct addresses appear
//     across the 37 (0x180006a10, 0x1800010d0, 0x180001850, 0x180001ef0,
//     0x1800028a0, 0x180002ee0, 0x18001e990, 0x180020b70, 0x180045490, and
//     0x1800c7ad0 reached from inside two of the inlined destructors). All ten
//     were disassembled and all ten are INSTRUCTION FOR INSTRUCTION the same
//     function: `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }` — the
//     per-translation-unit copies of `operator delete`, and byte-identical to
//     the exported `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` (0x1800c7ad0)
//     that lane Q already reaches. One constant, not a parameter.
//
//     CORRECTED 2026-09-12 (UI text-pipeline batch): "ten" and
//     "per-translation-unit" were both too small a claim. Many of these are
//     per-CLASS `operator delete`s with NAMES in the export directory
//     (`??3<Class>@UI@@SAXPEAX@Z`), and SIXTY of the UI ones were read out of
//     the shipped DLL and compared byte for byte for
//     runtime/src/ui/ui_operator_delete.cpp: all sixty are 93 bytes, every
//     difference between them is inside a RIP-relative displacement, and all
//     sixty resolve to the same four targets (the allocator singleton,
//     EnterCriticalSection, MemoryAllocator::Free, LeaveCriticalSection).
//     So the conclusion this paragraph draws is unchanged and now rests on
//     seventy measurements rather than ten — but the DESCRIPTION was wrong in
//     the way reference_grep_cannot_tell_internal_from_unported_export.md is
//     about, and the dialog_vtable.cpp banner already had to correct it once
//     for `??3Dialog@UI@@`.
//   * The ARITY does not vary. Lane Q's correction on `SetSize` — where eight
//     instantiations took two arguments behind a three-argument typedef — was
//     checked for here address by address: all 37 read RCX and EDX and nothing
//     else. `(void* this, unsigned flags)`, uniformly.
//   * The three prologue variants (with and without the MSVC unwind marker
//     `MOV [RSP+0x20],-2`, and the two orders of `LEA R9` / `MOV R8D`) are
//     register-allocation noise. They have no effect on the object.
//
// What DOES vary between addresses is how the SCALAR branch reaches `~Class`:
//
//   * 15 of 37 CALL it (`CALL <the same address the array branch puts in R9>`).
//   * 22 of 37 INLINE it. Every one of those 22 was checked against the
//     destructor the array branch names, and in all 22 the inlined code IS that
//     destructor's body: same vtable restamp, same member destroys, same tail
//     call. e.g. 0x18000b7e0's scalar branch is `LEA RAX,[0x180154a30]; MOV
//     [RCX],RAX; CALL 0x1800bc950`, and 0x1800bb980 (`~CompiledExternal`, the
//     address its own array branch passes in R9) is `LEA RAX,[0x180154a30];
//     MOV [RCX],RAX; JMP 0x1800bc950`.
//
// So the port calls `~Class` in both branches, and the inlining is not
// reproduced. That is a deliberate DEPARTURE from byte-faithfulness and it is
// the only one in this file: what retail computes is identical, what it costs
// is one extra call in 22 of the 37.
//
// ---------------------------------------------------------------------------
// WHY `~Class` IS CALLED BY ADDRESS, AND WHAT THAT COSTS THE CENSUS
// ---------------------------------------------------------------------------
// `ResolveAddr(Module::Runtime, dtor)` consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so a destructor
// another lane has already ported is reached as OUR code with no change here.
// 23 of the 37 are in that state today — the twelve `Video::*Data` destructors
// lane B2 ported, plus ~Achievement, ~Goal, ~GoalInstance, ~Internal,
// ~External, ~AnimationEvent, ~Geometry, ~ImageBuffer,
// ~CompressedMemoryStream, ~UpdateLoop and ~DialogController. The other 14 are
// still the DLL's, and this file makes them newly `called`:
// reference_porting_can_grow_the_cutover_surface.md. They are named in
// docs/runtime_port/WAVE9_LANE_K.md as lane B's remaining worklist, and each
// becomes ours the moment lane B registers it — no source change here.
//
// The resolve is LAZY, cached per row on first use, for two reasons: so a port
// registered after this file's init is still picked up, and because
// ResolveAddr is not a cheap accessor (reference_rtoriginal_is_getprocaddress.md
// is the same lesson one door over).
//
// ---------------------------------------------------------------------------
// THE ARRAY BRANCH: MSVC's `eh vector destructor iterator`, REPRODUCED
// ---------------------------------------------------------------------------
// 0x18007ec98, 32 instructions, disassembled for this lane and already
// disassembled once for lane D2 (runtime/src/collection/array_destroy.cpp says
// the same thing about it):
//
//     MOVSXD RDI,R8D            ; count is SIGNED
//     RBX = base + count*size
//     DEC EDI / JS out          ; a count of 0 (or negative) destroys nothing
//     SUB RBX,RSI / CALL R14    ; ... and it walks DOWNWARD
//
// It is reproduced here rather than called, so the array branch adds no CRT
// dependency that has to survive cutover. What is NOT reproduced is its
// exception bookkeeping (the `[RSP+0x20]` unwind state that lets a throwing
// element destructor continue the sweep). No destructor in this family throws —
// every one ends in a vtable store, a member destroy or a free — so the
// funclet is unreachable, and saying "unreachable" here is a claim about these
// 37 callees, which were all read, and not about callers.
//
// DESTRUCTION ORDER IS DESCENDING and the port preserves it, but note that a
// comparator CANNOT SEE it: the elements do not alias, so any order leaves the
// same bytes. It is preserved because it reaches the allocator, not because the
// probe proves it — the same statement lane D2 had to make about J2b.
//
// ---------------------------------------------------------------------------
// THE ALLOCATOR — NOT TAKEN
// ---------------------------------------------------------------------------
// Every free goes through `rtcoll::MemFree`, lane Q's by-address call to
// 0x1800c7ad0. This file does NOT arm the allocator: the old DLL-era switch is lane
// E's and carries a standing NO-GO, and a heap cannot be half ours. Lane Q's
// LANE E DEPENDENCY note in runtime/src/collection/array.cpp covers this file
// too — repointing rtcoll::MemFree at lane E's allocator repoints these bodies
// with it. Deliberately reused rather than re-derived so there is ONE place to
// repoint, not three.
//
// ---------------------------------------------------------------------------
// ~Internal IS IN THIS SET, AND THAT MATTERS
// ---------------------------------------------------------------------------
// 0x180006440 is `Object::Internal`'s deleting destructor, and the destructor
// it calls (0x1800bfdb0) is the SWAP-REMOVE through the handle table that
// reference_internalfactory_swapremove_wild_write.md is about: an object freed
// without it leaves a stale handle-table entry that a later swap-remove writes
// through. This file does not change that in either direction — it calls
// ~Internal exactly where retail calls it, on both branches — but it is now on
// the path, so a defect here would present as that failure family. The
// destructor itself is Wave 3's port (runtime/src/object/internal.cpp), which
// registers 0x1800bfdb0 under a shadow variant or the plain one depending on
// its own switches; ResolveAddr returns whichever is live.
//
// ---------------------------------------------------------------------------
// NO EXPORT NAMES ARE ADDED BY THIS FILE
// ---------------------------------------------------------------------------
// All 37 are registered by ADDRESS. They are unnamed in the DLL's export
// directory — that is why lane C2 could only trap them — so there is no
// mangled name to get wrong, and reference_phantom_dtor_mangling_qeaa_vs_ueaa.md
// (a `??1` spelled `QEAA` when it is `UEAA` resolves NULL silently) cannot
// apply. It would apply the moment anyone tried to register these by name:
// the spelling is `??_E<Class>@@UEAAPEAXI@Z`, NOT `??_G` and NOT `??1`, and the
// DLL does not export it under any of the three. tools/export_name_audit.py has
// nothing to check here, which is itself the finding.
//
// THREE FUNCTIONS, AND ONLY ONE OF THEM IS SLOT 0. Worth spelling out, because
// the project has now been bitten by this distinction twice:
//
//   ??1<Class>@@UEAA@XZ       the REAL destructor. No flags argument. EXPORTED
//                             -- lane B2 ported twelve of them.
//   ??_G<Class>@@UEAAPEAXI@Z  SCALAR deleting destructor. One flag bit, no
//                             cookie, no iterator. NOT what is here.
//   ??_E<Class>@@UEAAPEAXI@Z  VECTOR deleting destructor: `TEST DL,2`, the
//                             cookie at [this-8], the iterator. <-- slot 0.
//
// The discriminator is `TEST DL,0x2`: it does not occur in a `??_G` at all,
// because a scalar deleting destructor has no array form and nothing to test
// bit 1 for. Its presence in every one of these 37 IS the identification.
//
// This is also why lane B2 porting twelve `??1` moved lane C2's filled-slot
// count by EXACTLY ZERO, which is correct arithmetic and not a measurement
// error: the `??1` is the CALLEE, and slot 0 holds the `??_E` thunk that calls
// it. Registering the twelve thunks here is what fills those slots -- and
// because lane B2 already owns the callees, both halves are ours for all
// twelve, which takes those twelve vtables from "one trap" to COMPLETE.
//
// ---------------------------------------------------------------------------
// THE ORACLE — TWO HALVES, BECAUSE ONE CANNOT COVER BOTH RISKS
// ---------------------------------------------------------------------------
// A destructor cannot be replayed at a live call site: ours then theirs frees
// twice, and run on the post-state it takes the null branch, compares nothing
// and REPORTS GREEN — lane B2 hit exactly that with ~DisplayMode, the
// `Scene::Pause` early-out shape. So the risk is split:
//
//   1. THE BODY is verified by deleting_dtor_probe.cpp: a synthetic subject
//      built and owned by the probe, ours run on one copy and the SHIPPED body
//      on an identical second copy, bytes compared. With an effectiveness
//      control and a negative control. It covers 13 of the 37 addresses — the
//      ones whose `~Class` is a pure vtable restamp and can therefore be run on
//      a fabricated object without touching engine state. Since all 37 share
//      one body, 13 subjects verify the body; what they do not verify is the
//      other 24 ROWS, which is risk 2.
//
//   2. THE 37 ROWS are verified by tools/rt_deleting_dtor_verify.py, which
//      re-reads all 37 bodies out of the shipped DLL, extracts the destructor
//      and the element size from the machine code, and checks them against the
//      table below. It needs no build and no game run. A transcription error in
//      any row is the realistic per-address failure, and it is exactly what a
//      runtime probe on 13 subjects cannot see.
//
// Both must be clean. Neither alone means the lane is verified.
//
// ---------------------------------------------------------------------------
// EXPECT calls=0 ON ALL 37 BEFORE CUTOVER — AND THAT IS NOT A DEFECT
// ---------------------------------------------------------------------------
// Nothing in our tree calls a deleting destructor today. They live in the DLL's
// own vtables and the DLL dispatches through them directly, which never
// consults the port registry. So the `[the diff oracle]` per-address `calls=` will read
// 0 for all 37, and reference_uncalled_port_is_not_verified.md would normally
// stop the lane there. It does not here, because the probe does not depend on a
// live caller: it runs its own subjects from this file's init. The `calls=`
// column is a CUTOVER READINESS counter — it starts moving when lane C2's mint
// is armed and our vtables are the only ones there are.
//
// That also settles the probe's schedule. Lane Q's probe self-schedules on the
// first call into a registered entry point; here that moment never arrives, so
// the byte-comparing cases run from RtObjectDeletingDtorPortInit() instead.
// They are safe there because they allocate NOTHING: the subjects are
// file-scope arrays, and the 13 destructors involved write one pointer each.
// The two cases that do let the body FREE need the Runtime heap — which is
// constructed but NOT STARTED at init (lane Q's finding) — so they are opt-in
// and self-scheduled, and today they will not run at all. Said plainly rather
// than left for the integrator to discover from a silent counter.
//
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//   the old DLL-era switch            all 37 addresses stay on the DLL
//   the old DLL-era switch      the byte-comparing cases (default OFF)
//   the old DLL-era switch    ... plus the negative control
//   the old DLL-era switch    ... plus the two allocator cases (see above)
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "deleting_dtor_internal.h"

int RtObjectDeletingDtorLeavesRegister();  // deleting_dtor_leaves.cpp

// Same local declaration array.cpp and array_destroy.cpp carry, and for the
// same reason: standalone/resolve.h drags in <windows.h>, which PLAN.md §4.1
// keeps out of runtime/src/. The linker checks the two spellings match.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtdtor {

// ---------------------------------------------------------------------------
// The table. RT_DELDTOR(deleting-dtor VA, ~Class VA, sizeof(Class), name).
//
// tools/rt_deleting_dtor_verify.py parses these four fields out of THIS file
// and re-derives the middle two from the DLL's machine code. Keep the macro
// spelling on one line or the verifier's regex stops seeing the row — and a row
// it cannot see is a row nobody checks.
// ---------------------------------------------------------------------------
#define RT_DELDTOR(ADDR, DTOR, SIZE, NAME) { (ADDR), (DTOR), (SIZE), NAME }

static constexpr DelDtorSpec kSpecs[] = {
    RT_DELDTOR(0x180002d60, 0x180103a00, 0x060, "Graphics::MaterialInstance"),
    RT_DELDTOR(0x180006440, 0x1800bfdb0, 0x010, "Object::Internal"),
    RT_DELDTOR(0x1800065b0, 0x1800c0360, 0x008, "Object::Base"),   // probe subject
    RT_DELDTOR(0x180006640, 0x1800bc950, 0x020, "Object::External"),
    RT_DELDTOR(0x18000b7e0, 0x1800bb980, 0x020, "Object::CompiledExternal"),
    RT_DELDTOR(0x18000c610, 0x1800b9a40, 0x018, "IO::Stream"),
    RT_DELDTOR(0x180015e90, 0x1800a41c0, 0x0c0, "Graphics::Geometry"),
    RT_DELDTOR(0x18001ed20, 0x1800f2610, 0x068, "Video::ImageBuffer"),
    RT_DELDTOR(0x180020310, 0x1801041f0, 0x0d8, "Graphics::ParticleInstance"),
    RT_DELDTOR(0x180020ce0, 0x1800a5520, 0x0e0, "Graphics::ParticleSystemInstance"),
    RT_DELDTOR(0x180031900, 0x1800dddb0, 0x028, "Engine::UpdateLoop"),
    RT_DELDTOR(0x18004d4f0, 0x1800dc170, 0x048, "Engine::LoaderLoop"),
    RT_DELDTOR(0x18004d670, 0x1800db060, 0x188, "Engine::DialogLoaderLoopT"),
    RT_DELDTOR(0x18004d8b0, 0x1800db340, 0x168, "Engine::DialogLoopT"),
    // ---- WAVE 10C lane DLG8 -------------------------------------------------
    // Slot 0 of `??_7Dialog@UI@@6B@`, the last of that vtable's eight abort
    // traps and the only one that is not a UI body. It is this family's shape
    // exactly — a K1 "call the dtor" row — and the two constants come out of the
    // shipped bytes at 0x180038b20:
    //     LEA R9,[0x180112e40]   ~Dialog  (EXPORTED as ??1Dialog@UI@@UEAA@XZ,
    //                                      and PORTED by lane DLG2, so
    //                                      ResolveAddr hands back OUR body)
    //     MOV EDX,0x88           sizeof(UI::Dialog)
    // tools/rt_deleting_dtor_verify.py re-derives both from the DLL, which is
    // the whole reason this row lives here rather than as a 38th hand-written
    // copy in runtime/src/ui/dialog_vtable.cpp.
    //
    // APPENDED rather than inserted in address order (it would sort between
    // UI::Element and UI::DialogController): kProbeSubjects below is a list of
    // TABLE INDICES, and inserting mid-table would silently re-point all twelve
    // Video::*Data probe subjects at the wrong rows.
    //
    // Its scalar-branch `operator delete` is 0x1800384d0, which — unlike the ten
    // this file's banner lists — is EXPORTED, as `??3Dialog@UI@@SAXPEAX@Z`, a
    // class-scoped operator delete. Disassembled: byte-identical to the other
    // ten and to ?MemFree@MemoryAllocator@System@@SAXPEAX@Z. Nothing about the
    // port changes; the banner's "per-translation-unit copies" is just an
    // incomplete description of the set.
    // ---- WAVE 10D lane TIP --------------------------------------------------
    // Slot 0 of `??_7ToolTipDialog@UI@@6B@` (0x180157880), the only one of that
    // vtable's three CODE slots that is not a UI body — the other two,
    // ?TypeName@ and ?Type@, are in runtime/src/ui/tooltipdialog.cpp. Slots 3..9
    // needed nothing: compared BY ADDRESS against `??_7DialogInstance@UI@@6B@`
    // (0x180156d70), all seven are that vtable's own targets and it is already
    // 10/10 and PROMOTED.
    //
    // The two constants come out of the shipped bytes at 0x18004d270:
    //     LEA R9,[0x1800dac70]   ~ToolTipDialog (EXPORTED as
    //                            ??1ToolTipDialog@UI@@UEAA@XZ, and NOT yet
    //                            ported -- so ResolveAddr hands back the DLL's,
    //                            and this row makes that address newly `called`:
    //                            reference_porting_can_grow_the_cutover_surface)
    //     MOV EDX,0xc0           sizeof(UI::ToolTipDialog), and the SAME 0xc0
    //                            BeginToolTip passes to operator new
    //                            (runtime/types/ui/DialogInstance.h:273 records
    //                            that number from the other end)
    // tools/rt_deleting_dtor_verify.py re-derives both from the DLL, which is
    // why this row lives here rather than as a 39th hand-written copy in
    // runtime/src/ui/tooltipdialog.cpp.
    //
    // A K1 "call the dtor" row: 0x1800dac70 disassembles to `LEA
    // RAX,[0x180157880]; MOV [RCX],RAX; JMP 0x180114d40` — a vtable restamp and
    // a tail call to ~DialogInstance — and the deleting dtor's SCALAR branch
    // inlines exactly those three instructions, so calling ~Class in both
    // branches is this family's documented, deliberate departure and nothing
    // more.
    //
    // NOT added to kProbeSubjects: ~DialogInstance destroys members, so running
    // this on a fabricated object would be a manufactured crash, not a
    // measurement.
    //
    // APPENDED rather than inserted in address order (it would sort between
    // UI::DialogController and UI::DialogInstance) for the same reason the
    // UI::Dialog row above is appended: kProbeSubjects is a list of TABLE
    // INDICES and a mid-table insert silently re-points all twelve
    // Video::*Data probe subjects at the wrong rows.
    // ---- WAVE 19 lane CSTREAM -----------------------------------------------
    // Slot 0 of `??_7CompressedStream@IO@@6B@` (0x180155640) and of
    // `??_7CompressedFileStream@IO@@6B@` (0x1801555f0). The THIRD member of that
    // family, `??_7CompressedMemoryStream@`'s 0x1800244a0, is already row 17 —
    // which is exactly why these two are two TABLE ROWS and not two hand-written
    // bodies in compressed_stream_write.cpp. One body, three constants each.
    //
    // Both constants come out of the shipped bytes, disassembled for this lane:
    //     0x180023fd0:  LEA R9,[0x1800aa650]  ~CompressedStream
    //                   MOV EDX,0x68          sizeof(IO::CompressedStream)
    //     0x180024100:  LEA R9,[0x1800a9330]  ~CompressedFileStream
    //                   MOV EDX,0xb8          sizeof(IO::CompressedFileStream)
    // and BOTH sizes are already independently pinned in the type headers:
    // runtime/types/io/Stream.h asserts sizeof(CompressedStream) == 0x68 from
    // the ctor's store pattern, and runtime/types/io/FileStream.h:55 records the
    // 0xb8 stride from THIS VERY THUNK. That second one is a circular witness
    // and is named as such rather than counted twice — the independent one for
    // CompressedFileStream is its ctor at 0x1800a93b0, which places the
    // BinaryFileStream member at +0x68 and BinaryFileStream is 0x50: 0x68 + 0x50
    // = 0xb8. tools/rt_deleting_dtor_verify.py re-derives both from the DLL.
    //
    // ~CompressedStream (0x1800aa650) is PORTED — compressed_stream_release.cpp
    // registers it by name AND address — so ResolveAddr hands back OUR body.
    // ~CompressedFileStream (0x1800a9330) is NOT ported and is EXPORTED as
    // ??1CompressedFileStream@IO@@UEAA@XZ; this row makes that address newly
    // `called` (reference_porting_can_grow_the_cutover_surface.md). It is NOT
    // one of the plan's 36 IO rows and it is NOT claimed by this lane; it is
    // named here so the next census does not have to discover it.
    //
    // NOT added to kProbeSubjects: both destructors free the window array, the
    // block array and an Encoding, so running either on a fabricated object
    // would be a manufactured crash, not a measurement.
    //
    // APPENDED rather than inserted in address order (0x180023fd0 and
    // 0x180024100 would sort between Graphics::ParticleSystemInstance and
    // IO::CompressedMemoryStream) for the reason the two rows above are
    // appended: kProbeSubjects is a list of TABLE INDICES and a mid-table insert
    // silently re-points all twelve Video::*Data probe subjects at wrong rows.
    //
    // ENDGAME B.3.9 IO: the IO::CompressedFileStream row (0x180024100) is
    // retired -- the class's inline DeletingDtor is its slot 0.
    RT_DELDTOR(0x180023fd0, 0x1800aa650, 0x068, "IO::CompressedStream"),
    // ---- WAVE 25 lane VTIO ----------------------------------------------------
    // Slot 0 of `??_7PackageFile@IO@@6B@` (0x180155700). WAVE23_LANE_IOPORT.md
    // §8 item 2 is the residual this discharges: that lane measured the table at
    // 5/9 and the stamper split, and recorded plainly that it "did not enumerate
    // which four slots are unported". They are slots 0, 5, 6 and 7 --
    // this thunk, `?Serialize@PackageFile@IO@@`, `?Reconstruct@PackageFile@IO@@`
    // and `?Destruct@PackageFile@IO@@`. This row closes slot 0, taking the table
    // to 6/9.
    //
    // I NEARLY RECORDED THIS ROW AS NEEDING A DIFFERENT RECIPE, AND THAT WOULD
    // HAVE BEEN WRONG. Reading 0x1800259d0 on its own, the `test $0x2,%dl` /
    // `mov -0x8(%rcx),%r8d` array branch looked like a vector-deleting form
    // distinct from this family. Disassembling the SIBLING row 0x18002ba90
    // (IO::TextFileStream, above) refuted that in one step: the two are the same
    // instruction sequence, operand for operand, differing only in the two
    // constants this macro carries. The family already handles the flags
    // argument; the branch is what `??_E`/`??_G` always look like.
    //
    // Both constants read from RETAIL Runtime-x64.dll with objdump:
    //     4C 8D 0D 51 EB 08 00   LEA R9,[rip+0x8eb51] -> 0x1800b4540  ~PackageFile
    //     BA F0 00 00 00         MOV EDX,0xf0          sizeof(IO::PackageFile)
    // 0x1800b4540 is `??1PackageFile@IO@@UEAA@XZ` in the export directory --
    // UEAA, the VIRTUAL spelling; the QEAA form resolves NULL silently
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). The array-branch CALL
    // (0x18007ec98, the shared iterator) and the MemFree CALL (0x180006a10) are
    // the family's known constants.
    //
    // sizeof == 0xf0 (240). WAVE23_LANE_IOPORT §2.4 estimated "0xc8+ bytes";
    // 0xf0 is the shipped figure and is larger, which matters because that
    // lane's stated reason for not porting `??1PackageFile@` was that a
    // fabricated subject would be a fake rather than a synthetic one. 240 bytes
    // makes that reason stronger, not weaker.
    //
    // DECLARED SURFACE GROWTH: `??1PackageFile@IO@@UEAA@XZ` is NOT ported, so
    // this row makes 0x1800b4540 newly reachable through our thunk -- the same
    // accepted growth as the UI::ToolTipDialog, IO::CompressedFileStream and
    // IO::ReaderWriter rows above (reference_porting_can_grow_the_cutover_surface.md).
    // MEASURED: that growth is REAL but INVISIBLE to `tools/runtime_census.py`.
    // A row-set diff across this change adds NO row for 0x1800b4540, because
    // DeletingDtorCore reaches the dtor through a table field rather than a
    // literal at a resolve call site, which is the census's known non-literal
    // blind spot (reference_census_cannot_see_nonliteral_registrations.md).
    // So the cutover surface grew by one function and the board will not say so
    // -- recorded here rather than left to be rediscovered.
    //
    // NOT ADDED to kProbeSubjects, deliberately, and this is the difference from
    // the IO::ReaderWriter row directly above. That one qualified because
    // ~ReaderWriter's whole body is a vtable restamp -- safe on a fabricated
    // object. ~PackageFile is not remotely that: WAVE23_LANE_IOPORT §2.4 read it
    // and it destroys a FileInfo, two Arrays, five separately-inlined Strings
    // and two lists, then chains to ~External. Running that over a fabricated
    // 240-byte subject frees garbage pointers. A probe subject must be safe to
    // run, and this one is not.
    //
    // THIS DOES NOT PROMOTE THE ROW, and the table is still 6/9.
    // ENDGAME B.3.9 IO: the IO::PackageFile row (0x1800259d0) is retired --
    // the class's inline DeletingDtor is its slot 0.
    // ---- WAVE 27 lane GFXMESH ------------------------------------------------
    // Slot 0 of the three Graphics mesh vtables this lane owns:
    //     ??_7EdgeMesh@Graphics@@6B@          0x180154c70   -> 0x1800134b0
    //     ??_7EdgeMeshInstance@Graphics@@6B@  0x180154d70   -> 0x1800175d0
    //     ??_7MeshInstance@Graphics@@6B@      0x180154dc0   -> 0x180017330
    // All three are the family's exact shape and none is a new one; they are
    // three ADDRESSES, not three bodies, which is the whole point of this table.
    //
    // Every constant below was disassembled from the RETAIL Runtime-x64.dll for
    // this lane, and EVERY ~Class VA was then confirmed against the export
    // directory BY VA with pefile -- not by name pattern, and not against the
    // profile build:
    //
    //   0x1800134b0  LEA R9,[0x1800a2780]   ~EdgeMesh
    //                MOV EDX,0x180          sizeof(Graphics::EdgeMesh)
    //     0x1800a2780 = ordinal 580, ??1EdgeMesh@Graphics@@UEAA@XZ. UEAA, the
    //     VIRTUAL spelling -- QEAA would have resolved NULL silently
    //     (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
    //
    //   0x1800175d0  LEA R9,[0x1800174d0]   ~EdgeMeshInstance
    //                MOV EDX,0xd0           sizeof(Graphics::EdgeMeshInstance)
    //     0x1800174d0 = ordinal 581, ??1EdgeMeshInstance@Graphics@@UEAA@XZ.
    //
    //   0x180017330  LEA R9,[0x180017060]   ~MeshInstance
    //                MOV EDX,0x120          sizeof(Graphics::MeshInstance)
    //     0x180017060 = ordinal 632, ??1MeshInstance@Graphics@@UEAA@XZ.
    //
    // ALL THREE SIZES ARE INDEPENDENTLY PINNED, by a witness that is not this
    // thunk: runtime/types/graphics/Mesh.h carries static_assert(sizeof(EdgeMesh)
    // == 0x180), (sizeof(EdgeMeshInstance) == 0xd0) and (sizeof(MeshInstance)
    // == 0x120), each derived from that class's CONSTRUCTOR store pattern in
    // Wave 6 lane E, years of waves before this row existed. Two independent
    // derivations, same three numbers.
    //
    // THE SCALAR BRANCH INLINES ~Class IN ALL THREE, and all three were checked
    // against the destructor their own array branch names, which is the check
    // the banner says was done for the original 22 of 37:
    //   * 0x1800174d0 (~EdgeMeshInstance) is 0x1a bytes and is a bare tail
    //     `JMP 0x1800a41c0` (~Geometry) with NO vtable restamp -- and the
    //     scalar branch at 0x180017621 is exactly `CALL 0x1800a41c0`. Identical.
    //   * 0x180017060 (~MeshInstance) destroys the node array (+0xf0), the
    //     animation layer array (+0xd8+8) and the material array (+0xc8), then
    //     tail-jumps ~Geometry -- and the scalar branch at 0x180017381 emits
    //     those same four calls on those same four offsets, in that same order.
    //   * 0x1800a2780 (~EdgeMesh) restamps ??_7EdgeMesh@ , destroys the two
    //     lists (+0x158, +0x148) and the eight Video buffers in DESCENDING
    //     address order, then tail-jumps ~External -- and the EdgeMesh thunk's
    //     scalar branch is a plain `CALL 0x1800a2780`, so nothing is inlined
    //     there at all. It is a K1 "call the dtor" row.
    // So calling ~Class in both branches -- this family's one deliberate
    // departure -- costs one extra call on two of the three and nothing on the
    // third, and changes no byte.
    //
    // THREE MORE `operator delete` COPIES, and the banner's list of ten is
    // therefore incomplete rather than wrong. The scalar branches free through
    // 0x180012d20 (EdgeMesh), 0x180017460 (EdgeMeshInstance) and 0x1800169c0
    // (MeshInstance); the array branches all use 0x180006a10, which IS on the
    // banner's list. All three new addresses were disassembled and all three are
    // instruction-for-instruction `if (p) { Enter(cs); Free(_singleton, p);
    // Leave(cs); }` -- the same per-translation-unit copy, and Ghidra names all
    // three `operator_delete`. Nothing about the port changes: `operator delete`
    // is a CONSTANT in this family, not a parameter, and these three confirm
    // that rather than complicate it.
    //
    // ONE OF THE THREE ~Class BODIES IS OURS, TWO ARE THE DLL'S, and the split
    // is stated rather than glossed. Dtor(i) above resolves BY ADDRESS, and
    // ResolveAddr consults the port registry first, so what each row reaches is
    // decided by whether that ADDRESS is registered -- a name registration is
    // not enough:
    //   * ??1MeshInstance@Graphics@@UEAA@XZ IS ported
    //     (mesh.cpp, RtGraphics_MeshInstance_Dtor) but was registered BY NAME
    //     ONLY, so ResolveAddr(0x180017060) handed back the DLL's. This lane
    //     adds the matching RegisterRuntimeAddr in mesh.cpp; row 46 is now ours.
    //     That is reference_half_registered_port_reads_as_absent.md seen from
    //     the other side: the body existed, the door did not.
    //   * ??1EdgeMesh@ (0x1800a2780) and ??1EdgeMeshInstance@ (0x1800174d0) are
    //     NOT ported anywhere in this tree -- grepped by mangled name and by
    //     address before this row was written. Rows 44 and 45 therefore make
    //     those two addresses newly `called`
    //     (reference_porting_can_grow_the_cutover_surface.md). Neither is
    //     claimed as ported by this lane; both are named in
    //     docs/runtime_port/WAVE27_LANE_GFXMESH.md §6 as inherited cutover work,
    //     so the next census does not have to rediscover them.
    //
    // NOT added to kProbeSubjects. ~MeshInstance frees three arrays and
    // ~EdgeMesh frees two lists and tears down eight Video buffer objects;
    // running either over a fabricated subject frees garbage pointers. A probe
    // subject must be safe to run and neither is. ~EdgeMeshInstance would be
    // safe on its own (it is a bare tail call) but its callee ~Geometry is not
    // read by this lane, so it is left out rather than assumed.
    //
    // APPENDED rather than inserted in address order (all three would sort into
    // the middle of the table) for the reason every row since Wave 10C gives:
    // kProbeSubjects is a list of TABLE INDICES and a mid-table insert silently
    // re-points all twelve Video::*Data probe subjects at wrong rows.
    //
    // THESE ROWS DO NOT PROMOTE ANYTHING. They make slot 0 of each of the three
    // tables ported, which is what lets ??_7EdgeMesh@ reach 9/9 COMPLETE; see
    // docs/runtime_port/WAVE27_LANE_GFXMESH.md §5 for the promotion verdicts.
    //
    // *** tools/rt_deleting_dtor_verify.py PRINTS `BAD` FOR THE SECOND AND THIRD
    // *** OF THESE THREE ROWS. THE ROWS ARE RIGHT AND THE TOOL HAS A BLIND SPOT.
    // Read this before "fixing" the rows, because its verdict is the first thing
    // the next person will see:
    //
    //     BAD 0x1800175d0 ... scalar branch neither calls 0x1800174d0 nor
    //                         restamps a vtable
    //     BAD 0x180017330 ... scalar branch neither calls 0x180017060 nor
    //                         restamps a vtable
    //
    // The checker (tools/rt_deleting_dtor_verify.py:260-273) knows exactly TWO
    // shapes for the scalar branch -- K1, it CALLS ~Class; K2/K3, it INLINES a
    // ~Class that begins with a vtable RESTAMP, which the tool matches on -- and
    // its third arm is a bare `else` that reports anything else as a problem.
    // These two rows are a THIRD shape: the scalar branch inlines a ~Class whose
    // own body contains NO restamp at all, so there is nothing for the tool to
    // match and it cannot tell an inlined-correct row from a wrong one. It
    // announces a verdict without being able to examine its subject
    // (reference_a_screen_that_cannot_name_its_subject.md), and it INFLATES:
    // two false positives, no false negatives.
    //
    // BOTH WERE THEREFORE CHECKED BY HAND, instruction by instruction, which is
    // the check the tool is standing in for:
    //   0x1800175d0 scalar branch (at 0x180017621) is `CALL 0x1800a41c0` and
    //       nothing else, and ~EdgeMeshInstance (0x1800174d0) is 0x1a bytes
    //       whose whole body is `JMP 0x1800a41c0`. Identical.
    //   0x180017330 scalar branch (at 0x180017381) is, in order,
    //       CALL 0x18001c550 on this+0xf0, CALL 0x18001c480 on this+0xd8+8,
    //       CALL 0x180014d00 on this+0xc8, CALL 0x1800a41c0 on this -- and
    //       ~MeshInstance (0x180017060) is those same four calls on those same
    //       four offsets in that same order, the last as a tail JMP. Identical.
    // The row's `dtor` field is what DeletingDtorCore CALLS, and calling ~Class
    // in both branches is this family's one documented deliberate departure, so
    // both rows compute what retail computes.
    //
    // NOT FIXED IN THE TOOL, deliberately: WAVE27_KICKOFF.md §4 #1 forbids this
    // wave's lanes from touching instruments. Recorded with file and line in
    // docs/runtime_port/WAVE27_LANE_GFXMESH.md under "INSTRUMENT DEFECT
    // OBSERVED, NOT FIXED". [ADDRESSEE: whoever scopes a Wave 28 instruments
    // lane -- the fix is a K4 arm that accepts a scalar branch whose call set
    // equals the call set of the named ~Class.]
    RT_DELDTOR(0x1800134b0, 0x1800a2780, 0x180, "Graphics::EdgeMesh"),
    RT_DELDTOR(0x1800175d0, 0x1800174d0, 0x0d0, "Graphics::EdgeMeshInstance"),
    RT_DELDTOR(0x180017330, 0x180017060, 0x120, "Graphics::MeshInstance"),

    // ---- WAVE 27 lane GFXPART -------------------------------------------------
    // Slot 0 of `??_7Light@Graphics@@6B@` (0x180158d30) and slot 0 of
    // `??_7SunLight@Graphics@@6B@` (0x180158e10) -- the ONE unported slot of the
    // Light table (taking it 5/6 -> 6/6 COMPLETE) and the first of SunLight's six.
    //
    // Both constants disassembled from RETAIL Runtime-x64.dll for this lane:
    //     0x180058dd0:  LEA R9,[0x180058d20]  ~Light     MOV EDX,0xc0   sizeof(Light)
    //     0x180064620:  LEA R9,[0x180108160]  ~SunLight  MOV EDX,0x108  sizeof(SunLight)
    // Both dtor VAs resolve in the export directory BY VA (not by name pattern) to
    // `??1Light@Graphics@@UEAA@XZ` / `??1SunLight@Graphics@@UEAA@XZ` -- UEAA, the
    // VIRTUAL spelling; QEAA would resolve NULL silently
    // (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md). Both deleting-dtor
    // addresses themselves are `<UNNAMED>` in that same export table, checked
    // rather than inherited (`AEAA` means PRIVATE, not unexported --
    // reference_aeaa_is_private_not_unexported.md).
    //
    // ONE DEPARTURE FROM THE FAMILY, IN THE Light ROW, AND IT IS BENIGN. This
    // family's K1 shape has the scalar branch CALL the same address the array
    // branch puts in R9. 0x180058dd0's scalar branch instead calls 0x1800a41c0
    // (`??1Geometry@Graphics@@UEAA@XZ`) directly. That is not a second shape:
    // ~Light (0x180058d20) disassembles to a bare frame set-up and
    // `JMP 0x1800a41c0` -- Light adds no members over Geometry, so MSVC devirtualised
    // the tail call in one branch and not the other. Calling ~Light in both
    // branches, as DeletingDtorCore does, lands on the identical code. The
    // sizes agree with that reading: 0xc0 here is exactly the Geometry row's 0xc0.
    // SunLight's row has no such departure -- its scalar branch calls 0x180108160,
    // the address its own array branch names.
    //
    // SunLight's scalar-branch `operator delete` is 0x1800643b0, an ELEVENTH
    // per-TU copy not in this file's banner list of ten. Disassembled for this
    // lane: `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }` -- the same
    // body as the other ten and as ?MemFree@MemoryAllocator@System@@SAXPEAX@Z.
    // Its ARRAY branch uses 0x180006a10, already in the banner's list. Nothing
    // about the port changes; the banner's "ten" is an incomplete enumeration,
    // as the UI::Dialog row above already noted for an exported twelfth.
    //
    // DECLARED SURFACE GROWTH: neither `??1Light@Graphics@@UEAA@XZ` nor
    // `??1SunLight@Graphics@@UEAA@XZ` is ported, so these rows make 0x180058d20
    // and 0x180108160 newly reachable through our thunks -- the same accepted
    // growth as the UI::ToolTipDialog, IO::CompressedFileStream, IO::ReaderWriter
    // and IO::PackageFile rows above
    // (reference_porting_can_grow_the_cutover_surface.md). As the PackageFile row
    // records, that growth is INVISIBLE to tools/runtime_census.py, because
    // DeletingDtorCore reaches ~Class through a table field rather than a literal
    // at a resolve call site. Recorded here rather than left to be rediscovered.
    //
    // NOT added to kProbeSubjects. ~Geometry (which both rows reach, Light
    // directly and SunLight through ~SunLight's chain) destroys the Geometry
    // member arrays; running it over a fabricated 0xc0/0x108-byte subject would
    // free garbage pointers. A probe subject must be safe to run.
    //
    // APPENDED rather than inserted in address order (0x180058dd0 would sort
    // among the twelve Video::*Data rows, and 0x180064620 just after them) for
    // the reason every row above is appended: kProbeSubjects is a list of TABLE
    // INDICES and a mid-table insert silently re-points those twelve subjects at
    // the wrong rows.
    //
    // THIS COMPLETES ??_7Light@Graphics@@6B@ STATICALLY. It does not promote it;
    // see WAVE27_LANE_GFXPART.md section 3 for the stamper question.
    //
    // EXPECTED RED, READ THIS BEFORE "FIXING" THE ROW: tools/rt_deleting_dtor_verify.py
    // prints for the Light row (and only the Light row)
    //     BAD 0x180058dd0 ... scalar branch neither calls 0x180058d20 nor restamps a vtable
    // and ends `problems: 1`. That verdict is CORRECT ABOUT THE BYTES and WRONG
    // ABOUT THE PORT. The verifier knows two shapes, K1 (scalar branch CALLs the
    // R9 dtor) and K2/K3 (scalar branch INLINEs it, detected by the vtable
    // restamp). Light is a third: the scalar branch calls ~Geometry directly
    // because ~Light is nothing but `JMP ~Geometry` and there is no restamp to
    // pattern-match. Per Wave 27's no-tooling rule the verifier was NOT extended;
    // the defect is filed in WAVE27_LANE_GFXPART.md under INSTRUMENT DEFECT
    // OBSERVED, NOT FIXED, with this file:line. ADDRESSEE for the fix: Wave 28.
    // (Graphics::SunLight's row 0x180064620 is RETIRED -- ENDGAME B.3.9 pilot
    // c2: every SunLight is exe-built by `::new (p) Graphics::SunLight()` and
    // wears the class's own table, whose slot 0 is the exe's scalar deleting
    // dtor; the minted ??_7SunLight row this filled is retired with it.)

    // ---- WAVE 27 lane IOTEXT --------------------------------------------------
    // Slot 0 of `??_7ElementDescription@UI@@6B@` (0x1801561c0). This is the row
    // runtime/src/ui/dialog.cpp:1471 has asserted since Wave 9 was ALREADY
    // ported ("one of the 37 Wave 9 lane K ported") -- IT WAS NOT. Grepping
    // 0x180035350 across runtime/, standalone/ and code/ at wave27/base finds it
    // only in that comment and in standalone/rt_data_mint_tables.inc:1016, i.e.
    // in the MINT (which is where the trap lives), never in a registration. A
    // body ported but never registered and a body never written look identical
    // from the outside (reference_half_registered_port_reads_as_absent.md); here
    // the prose claimed the port and neither existed. The comment at dialog.cpp
    // has been corrected in place rather than left to be rediscovered.
    //
    // DISASSEMBLED AT 0x180035350 (Runtime-x64.dll, not the profile build) and it
    // is this family, operand for operand: `TEST DL,0x2` -> array arm
    // `LEA R9,[0x180116100]; MOV R8D,[RCX-0x8]; MOV EDX,0x68; CALL 0x18007ec98`,
    // then `TEST SIL,0x1` / free / return `this-8`. One of the 22 rows whose
    // SCALAR arm is INLINED (it open-codes the vtable restamp, the +0x60 String
    // release and the `CALL 0x1800bc950` tail), which is exactly the case this
    // family handles by calling `~Class` on BOTH arms.
    //
    // sizeof == 0x68 THREE WAYS: the `MOV EDX,0x68` here, the ctor's last store
    // at +0x60, and runtime/types/ui/ElementDescription.h's static_assert.
    //
    // EVERY CALLEE IS OURS -- this row closes rather than deepening:
    //   0x180116100 ~ElementDescription  PORTED and registered BY ADDRESS
    //                                    (dialog.cpp, Wave 9 lane UIF)
    //   0x18007ec98 eh vector iterator   reproduced by this file
    //   operator delete                  this file's MemFree
    //
    // NOT ADDED to kProbeSubjects, on the IO::PackageFile row's test rather than
    // the IO::ReaderWriter one: ~ElementDescription releases a refcounted String
    // at +0x60 and chains to ~External. Run over a fabricated subject that
    // decrements a refcount that was never incremented and frees a garbage
    // pointer. A probe subject must be SAFE TO RUN; this one is not.
    // (UI::GroupDescription's row -- 0x180046af0: LEA R9,[0x180117c00]
    // ~GroupDescription, MOV EDX,0x78; a scalar arm whose class-specific
    // ??3GroupDescription (0x1800466f0) is the same single MemFree -- was retired
    // by ENDGAME B.3.9 UI-A: slot 0 is the class's own.)
    // (UI::ImageDescription / UI::AnimatedImageDescription's two rows were
    // retired by ENDGAME B.3.9 UI-A: slot 0 is each class's own.)
    // MERGE 2026-09-12: batch 9's six rows come FIRST so their indices
    // (61..66), already baked into their entry points below, are unchanged;
    // VTABLE BATCH 1's ten were renumbered 67..76 after the merge, exactly as
    // the kSpecCount comment in deleting_dtor_internal.h demands.

    // 2026-09-12 (VTABLE BATCH 9). Six rows for the object-misc batch, each
    // read straight from the deleting destructor's own bytes:
    //   0x180074a10  LEA R9,[0x180111cd0]  MOV EDX,0xd0   ~ComboDialog
    //     (own body: stamp own vtable, tail into ~DialogInstance -- already
    //     ported, runtime/src/ui/dialog.cpp)
    //   0x18003b600  LEA R9,[0x18003b4f0]  MOV EDX,0x20   ~DialogControllerConfig
    //     (own body: stamp own vtable, tail into ~External -- already ported,
    //     runtime/src/object/external.cpp)
    // (ENDGAME B.3.9: the Object::ExtensionCompiler 0x18002c300 and
    // Object::ExternalCompiler 0x18002c180 rows retired with their minted
    // tables -- retail's factories for both return null, nothing built one.)
    //   0x180007800  LEA R9,[0x1800076c0]  MOV EDX,0x30   ~ExternalList
    //     (own body: Array<ExternalPtr<External>>::~Array at +0x20 (0x180009460,
    //     already ported), then ~External -- already ported)
    RT_DELDTOR(0x180007800, 0x1800076c0, 0x030, "Object::ExternalList"),
    // (ENDGAME B.3.9: the IO::CompressedCopyFileStream 0x1800245f0 row retired
    // with its minted table: no class, no stamper, and its ~Class 0x1800a92a0
    // was never registered, so the row called a NULL dtor -- INVENTORY_B
    // finding 2.)
    // Vtable batch 8 (2026-09-12), slot 0 of the four spatial classes' vtables.
    // Both constants read from each deleting dtor's own bytes:
    //     0x1800656b0  ~CollisionMesh 0x180065400  MOV EDX,0x70
    //     0x180064ea0  ~Surface       0x18010ca00  MOV EDX,0x30 (scalar arm only; no vector arm)
    //     0x1800713b0  ~PointList     0x18010f8e0  MOV EDX,0x50
    //     0x180072050  ~PickingMesh   0x1801228a0  MOV EDX,0x48
    // All four destructors are ported in runtime/src/collision2d/, pathfinding/, picking/.
    // None is a probe subject: each releases owned Array buffers and/or refcounted
    // Strings, so a fabricated subject would free/decrement garbage.
    // (ENDGAME B.3.9 PILOT: the Picking::PickingMesh row 0x180072050 retired --
    // the class's own DeletingDtor (PickingMesh.h) carries both arms.)

    // 2026-09-12, VTABLE BATCH 1 (the first half of the UI::*Description
    // family): ten more slot-0 rows, indices 71..80. Each row's two constants
    // were read from that deleting destructor's OWN bytes -- the `LEA R9` that
    // names ~Class and the `MOV EDX` that is sizeof(Class) -- never from the
    // class header, and each was cross-checked against the `Allocate(size, 8)`
    // in that class's `?CreateNewObject@` (already transcribed in
    // runtime/src/ui/factory_ui1_tables.inc). All ten agree.
    //
    //     0x180040ff0  LEA R9,[0x18010fb20]  MOV EDX,0xb8   BorderDescription
    //     0x1800481a0  LEA R9,[0x180110bd0]  MOV EDX,0xa0   ButtonDescription
    //     0x1800489c0  LEA R9,[0x180111620]  MOV EDX,0xa8   CheckDescription
    //     0x180037eb0  LEA R9,[0x1800cc0d0]  MOV EDX,0x78   ClipRegionDescription
    //     0x180074e70  LEA R9,[0x180112370]  MOV EDX,0xb8   ComboDescription
    //     0x18003fa20  LEA R9,[0x1800d11f0]  MOV EDX,0x88   EditDescription
    //     0x180042730  LEA R9,[0x1800d2960]  MOV EDX,0xb8   LabelDescription
    //
    // None of the ten is a probe subject: every one of these destructors
    // releases at least one refcounted String, and ~DocumentTextDescription
    // also destroys four arrays, so running one on a fabricated object would
    // be a manufactured crash rather than a measurement.

    // VTABLE BATCH 7 (2026-09-12), appended AFTER batch 8's four rows above --
    // indices 71-74. Four more classes' slot0, read the same way as every row
    // above (LEA R9,[~Class] / MOV EDX,sizeof from the deleting dtor's own
    // bytes, objdump against retail):
    //   0x1800167f0  LEA R9,[0x1800fb620]  MOV EDX,0x58   Graphics::AnimationGroup
    //   0x180020a70  LEA R9,[0x1800a51c0]  MOV EDX,0x108  Graphics::ParticleSystem
    //   0x18000bef0  LEA R9,[0x1800e03b0]  MOV EDX,0x80   Audio::SoundBuffer
    //   0x180050820  LEA R9,[0x1800e0880]  MOV EDX,0x40   Audio::SoundEffect
    // None of the four ~Class bodies is ported by this batch; all four reach
    // the DLL through ResolveAddr, exactly like most of the rows above.
    // 2026-09-12, the UI TEXT PIPELINE batch. APPENDED AFTER the batch-8 and
    // batch-9 rows above at merge time: kSpecs is POSITIONAL and three batches
    // each started appending at the same index on their own branch, so the
    // wrapper indices below were re-derived from the merged table, not before.
    // 2026-09-12, the UI TEXT PIPELINE batch: the four classes of
    // runtime/src/ui/{font,characterset,cursor,spritecapture_class}.cpp. Each
    // row's two constants were read from the deleting destructor's OWN bytes
    // (the `LEA R9` and `MOV EDX` of its array arm), not from the class header:
    //     0x180032130  LEA R9,[0x1801175a0]  MOV EDX,0x90   UI::Font
    //     0x1800378a0  LEA R9,[0x1800cb840]  MOV EDX,0x28   UI::CharacterSet
    //     0x18003b3a0  LEA R9,[0x1800cd1c0]  MOV EDX,0x30   UI::Cursor
    //     0x18003ccf0  LEA R9,[0x1800d91b0]  MOV EDX,0x58   UI::SpriteCapture
    // All four destructors are ported and registered in runtime/src/ui/.
    //
    // THREE OF THE FOUR INLINE ~Class IN THE SCALAR ARM (CharacterSet, Cursor
    // and SpriteCapture open-code the restamp, the member release and the
    // `CALL ??1External`); UI::Font is the one that CALLS it (0x180032181).
    // That is exactly the split this family handles by calling `~Class` on
    // BOTH arms, and it cost nothing to check: the inlined code in each of the
    // three IS the body of the destructor its own array arm names.
    //
    // Each also calls its CLASS-SPECIFIC `operator delete` in the scalar arm
    // (??3Font 0x180031a50, ??3CharacterSet 0x1800374e0, ??3Cursor 0x18003b010,
    // ??3SpriteCapture 0x18003c8a0) rather than the shared 0x180006a10. All
    // four are now ported in runtime/src/ui/ui_operator_delete.cpp, which
    // proves by byte comparison that all sixty UI class operator deletes are
    // one function — so DeletingDtorCore's single MemFree IS retail's body
    // here too, as it is for the GroupDescription and Video rows above.
    //
    // NOT ADDED to kProbeSubjects, on the UI::ElementDescription row's test:
    // ~CharacterSet releases a refcounted String, ~SpriteCapture destroys
    // three Arrays whose buffers came from the Runtime heap, and ~Font does
    // both. A fabricated subject would decrement a refcount nothing
    // incremented and free garbage pointers.
    // ---- CUTOVER VTABLE BATCH 4 "elem_B" (2026-09-12), rows 85..98 ---------
    // Slot 0 of the fourteen UI widget vtables ported by
    // runtime/src/ui/<class>element.cpp. APPENDED, never inserted: the table is
    // POSITIONAL and kProbeSubjects is a list of INDICES.
    //
    // Both constants per row were read out of RETAIL Runtime-x64.dll's own
    // disassembly at the deleting destructor's address — the `LEA R9,[~Class]`
    // and the `MOV EDX,sizeof` that the shared `eh vector destructor iterator`
    // at 0x18007ec98 is handed:
    //
    //   0x180046540  LEA R9,[0x1800d4c40]  MOV EDX,0x100   UI::NumberEditElement
    //   0x18004ab80  LEA R9,[0x180118ce0]  MOV EDX,0xe8    UI::NumberElement
    //   0x180047540  LEA R9,[0x1800d5220]  MOV EDX,0xc8    UI::PageElement
    //   0x180047d50  LEA R9,[0x1800d5820]  MOV EDX,0xd8    UI::RadioElement
    //   0x1800422f0  LEA R9,[0x18011eda0]  MOV EDX,0x100   UI::TextElement
    //
    // ALL FOURTEEN ~Class bodies are PORTED by that batch and registered by
    // name AND address, so ResolveAddr hands each row OUR destructor rather
    // than the DLL's — these rows add nothing to the cutover surface.
    //
    // NOT added to kProbeSubjects: every one of them tail-calls ~Element,
    // which releases a System::String and destroys the child Array, so running
    // one on a fabricated object would be a manufactured crash, not a
    // measurement.
    //
    // TWO OF THEM READ "UNREC" IN tools/rt_deleting_dtor_verify.py, AND THAT
    // IS THE READER, NOT THE ROW. ProgressElement (0x180078150) and
    // SliderElement (0x18004a180) are the two whose ~Class is a BARE TAIL JUMP
    // to ~Element with no vtable restamp -- five instructions, e.g.
    // 0x180077ef0: `MOV [RSP+8],RCX; SUB RSP,0x38; MOV [RSP+0x20],-2;
    // ADD RSP,0x38; JMP 0x1801161a0`. Inlining that into the scalar branch
    // leaves a plain `CALL 0x1801161a0` (checked at 0x1800781a1), which is
    // neither of the reader's two known shapes ("calls the dtor" /
    // "restamps then inlines"), so it declines to classify. The two constants
    // it DOES check -- dtor VA and sizeof -- are re-derived and correct for
    // both rows.
    // ---- Vtable batch 6 (2026-09-12): graphics buffers, meshes and lights ----
    // INDICES 103-108, appended after batch 4 elem_B's 89-102. The table is
    // POSITIONAL and these six have been RE-COUNTED TWICE: 71-76 -> 89-94 ->
    // 103-108, once for batches 1/5/7 and once for batch 4. That is the merge
    // hazard the banner and kSpecCount both record, met twice by one batch.
    //
    // Every row's two constants were read from that deleting destructor's OWN
    // bytes — the `LEA R9` of the vector arm and its `MOV EDX,imm`:
    //     0x180059080  LEA R9,[0x180058f30]  MOV EDX,0xd8   Graphics::AmbientLight
    //     0x18001eee0  LEA R9,[0x1800a3e40]  MOV EDX,0x70   Graphics::GeneratedImageBuffer
    //     0x18001f260  LEA R9,[0x1800a3c80]  MOV EDX,0x88   Graphics::FrameCopyImageBuffer
    //     0x180022600  LEA R9,[0x1800a70b0]  MOV EDX,0x90   Graphics::ReflectionImageBuffer
    //     0x180012bd0  LEA R9,[0x1800ff270]  MOV EDX,0x98   Graphics::GraphicsMesh
    //     0x1800642b0  LEA R9,[0x180106270]  MOV EDX,0x168  Graphics::SpotLight
    //
    // *** THE AmbientLight ROW'S dtorVa IS A THUNK, AND THAT IS DELIBERATE. ***
    // `??1AmbientLight@Graphics@@UEAA@XZ` at 0x180058f30 is five instructions
    // that tail-jump `??1Geometry@Graphics@@UEAA@XZ` (0x1800a41c0) — the linker
    // folded the two — and 0x180059080's SCALAR arm calls 0x1800a41c0 directly
    // while its VECTOR arm names the thunk. The row carries the thunk, because
    // that is what `tools/rt_deleting_dtor_verify.py` reads out of the shipped
    // bytes, and runtime/src/graphics/lights_vtable.cpp registers our port at
    // BOTH addresses so either door lands on our body.
    //
    // Four of the five remaining destructors are ported by this batch
    // (runtime/src/graphics/imagebuffer_derived.cpp, graphicsmesh_vtable.cpp,
    // lights_vtable.cpp); ~FrameCopyImageBuffer was ported by Wave 23.
    //
    // NONE is a probe subject, on the UI::ElementDescription row's test: every
    // one of them either releases a refcounted System::String (GraphicsMesh's
    // name, TriangleList's name) or destroys GPU objects through a platform
    // companion pointer. A fabricated subject would decrement a refcount nothing
    // incremented and hand the driver a garbage handle.
    RT_DELDTOR(0x18001eee0, 0x1800a3e40, 0x070, "Graphics::GeneratedImageBuffer"),
    RT_DELDTOR(0x18001f260, 0x1800a3c80, 0x088, "Graphics::FrameCopyImageBuffer"),
    // VTABLE BATCH 3 (2026-09-12): the UI::*Element widget runtime, first
    // half. Thirteen slot-0 rows, appended AFTER every batch on main at
    // merge time -- the table is POSITIONAL and each wrapper below passes
    // its OWN index, re-derived after the merge, not before.
    // Each row's two constants come from the deleting destructor's own bytes
    // (LEA R9,[~Class] and MOV EDX,sizeof), objdump against retail:
    //   0x180036db0  LEA R9,[0x1800cb3a0]  MOV EDX,0x100  UI::AnimatedImageElement
    //   0x1800414f0  LEA R9,[0x180041230]  MOV EDX,0x150  UI::BorderElement
    //   0x1800485b0  LEA R9,[0x180110c90]  MOV EDX,0x0d8  UI::ButtonElement
    //   0x180048d80  LEA R9,[0x180111700]  MOV EDX,0x0e0  UI::CheckElement
    //   0x180038370  LEA R9,[0x1800cbe30]  MOV EDX,0x0d8  UI::ClipRegionElement
    //   0x180075390  LEA R9,[0x180112490]  MOV EDX,0x128  UI::ComboElement
    //   0x18003fed0  LEA R9,[0x1800d0e50]  MOV EDX,0x0f8  UI::EditElement
    //   0x180046e10  LEA R9,[0x180117cc0]  MOV EDX,0x0c0  UI::GroupElement
    //   0x180036600  LEA R9,[0x180118470]  MOV EDX,0x0e0  UI::ImageElement
    //   0x180042bb0  LEA R9,[0x1800d25f0]  MOV EDX,0x130  UI::LabelElement
    // NONE of the thirteen is a probe subject: every one of these ~Class
    // bodies either destroys Arrays whose buffers came from the Runtime heap
    // (Combo, DocumentText, Graph, AnimatedImage), releases refcounted
    // Strings (Edit, DocumentText) or closes a live dialog (Combo). A
    // fabricated subject would free garbage. The other seven are pure vtable
    // restamps but chain into ~Element, which swap-removes a handle-table
    // entry -- the same disqualification runtime/types/ui/Element.h records.
    // All thirteen ~Class bodies ARE ported (runtime/src/ui/<class>element.cpp),
    // so ResolveAddr hands this file OUR code for every row.
    // 2026-09-12, VTABLE BATCH 2 (the UI::*Description second half), rows
    // 103..115 -- APPENDED AFTER every row main already had, and the entry
    // points below were renumbered to match at the merge (the table is
    // POSITIONAL; see the kSpecCount banner). Thirteen
    // rows, one per class in that batch. Each pair of constants was read out of
    // THAT deleting destructor's own bytes — the `LEA R9,[~Class]` and the
    // `MOV EDX,sizeof` that the eh_vector iterator receives — and not from any
    // class header or from a sibling that "looks the same": the sizes below
    // range over 0x78..0xc8 and four of the thirteen would have been wrong if
    // carried across from a neighbour.
    //
    // NONE of them is a probe subject. Every one of these destructors either
    // releases a refcounted System::String or destroys an Array whose buffer
    // came from the Runtime heap (most do both), so a fabricated subject would
    // decrement a refcount nothing incremented and free a garbage pointer —
    // the same test the UI::ElementDescription row applies.
};
// ENDGAME B.3 step 9 (B39_RULES.md D4): the count is the table's, and every
// wrapper finds its row BY ADDRESS at compile time -- deleting a row renumbers
// nothing, and a wrapper whose row is gone does not compile.
constexpr int kSpecCount = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));

constexpr int RowOf(uintptr_t addr) {
    for (int i = 0; i < kSpecCount; ++i)
        if (kSpecs[i].addr == addr) return i;
    return -1;
}
constexpr bool RowsAreUnique() {
    for (int i = 0; i < kSpecCount; ++i)
        for (int j = i + 1; j < kSpecCount; ++j)
            if (kSpecs[i].addr == kSpecs[j].addr) return false;
    return true;
}
static_assert(RowsAreUnique(), "two RT_DELDTOR rows share a deleting-destructor address");

template <uintptr_t Addr>
constexpr int Row() {
    constexpr int i = RowOf(Addr);
    static_assert(i >= 0, "this wrapper's RT_DELDTOR row is gone: delete the wrapper "
                          "and its RegisterRuntimeAddr line with it");
    return i;
}

static DtorFn g_dtorCache[kSpecCount];

// ---------------------------------------------------------------------------
// The allocator boundary. ONE call, reusing lane Q's, so lane E has one place
// to repoint rather than three (see the banner).
// ---------------------------------------------------------------------------
static void MemFree(void* p) { rtcoll::MemFree(p); }

// ---------------------------------------------------------------------------
// `~Class`, resolved lazily and cached per row.
// ---------------------------------------------------------------------------
static DtorFn Dtor(int i) {
    if (!g_dtorCache[i])
        g_dtorCache[i] = reinterpret_cast<DtorFn>(
            ResolveAddr(Module::Runtime, kSpecs[i].dtorVa));
    return g_dtorCache[i];
}

// ---------------------------------------------------------------------------
// MSVC's `eh vector destructor iterator` (0x18007ec98), reproduced.
//
// `count` is SIGNED (`MOVSXD RDI,R8D`) and the loop is `DEC EDI; JS out`, so a
// count of zero or less destroys nothing and the walk is DOWNWARD. All three
// details are load-bearing and none is observable in the resulting bytes; see
// the banner.
// ---------------------------------------------------------------------------
void EhVectorDtor(uint8_t* base, uint32_t elemSize, int count, DtorFn dtor) {
    uint8_t* p = base + static_cast<int64_t>(count) * elemSize;
    while (--count >= 0) {
        p -= elemSize;
        dtor(p);
    }
}

// ---------------------------------------------------------------------------
// THE BODY. One function; 37 registered entry points differ only in `i`.
//
// `site` reporting is folded in here rather than in each entry point so the
// `calls=` column counts entries into the BODY per address. It reports which
// branch ran and how many elements were destroyed, so a green line cannot be
// read as "did work" when it was a zero-count array destroy
// (reference_probe_needs_an_effectiveness_control.md).
// ---------------------------------------------------------------------------
// `dtor` is a parameter rather than `Dtor(i)` for ONE reason, and it is a
// diagnostic one. 12 of the 13 addresses the probe uses as subjects have a
// ~Class that is ALREADY OUR PORT (lane B2's Video::*Data destructors), so a
// red comparison could be that port rather than this envelope. The probe's
// `iso` mode re-runs the same case with the SHIPPED destructor forced in here,
// which separates the two in one run instead of leaving the integrator to
// bisect a lane boundary.
void* DeletingDtorCoreWith(void* self, unsigned flags, int i, DtorFn dtor) {
    uint8_t* p = static_cast<uint8_t*>(self);
    const DelDtorSpec& s = kSpecs[i];

    // NOTE: the diff site is reported by the WRAPPER below, not here. The probe
    // drives this body directly and its traffic must not reach the registry:
    // `calls=` is this lane's cutover-readiness signal, and a probe that
    // inflated it would manufacture the very number it exists to let someone
    // read (reference_truncated_tool_output_manufactures_calls_zero.md is the
    // same shape -- a tool inventing the signal it is there to measure).
    if (flags & 2u) {
        // `delete[]`: the count sits in the eight-byte cookie the array form of
        // operator new placed BEFORE the first element, and everything from
        // here on is expressed against the BLOCK, not against `this`.
        const int n = *reinterpret_cast<const int32_t*>(p - 8);
        EhVectorDtor(p, s.elemSize, n, dtor);
        if (flags & 1u) MemFree(p - 8);
        return p - 8;
    }
    dtor(p);
    if (flags & 1u) MemFree(p);
    return p;
}


void* DeletingDtorCore(void* self, unsigned flags, int i) {
    return DeletingDtorCoreWith(self, flags, i, Dtor(i));
}

// Graphics::MaterialInstance ~MaterialInstance = 0x180103a00  (PORTED 2026-09-12, deleting_dtor_leaves.cpp)
void* RtDelDtor_MaterialInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180002d60>());
}
// Object::Internal         ~Internal = 0x1800bfdb0  (PORTED)
void* RtDelDtor_Internal(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180006440>());
}
// Object::Base             ~Base = 0x1800c0360  (PORTED 2026-09-12, deleting_dtor_leaves.cpp)
void* RtDelDtor_Base(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x1800065b0>());
}
// Object::External         ~External = 0x1800bc950  (PORTED)
void* RtDelDtor_External(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180006640>());
}
// Object::CompiledExternal ~CompiledExternal = 0x1800bb980  (PORTED, batch D2)
void* RtDelDtor_CompiledExternal(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18000b7e0>());
}
// IO::Stream               ~Stream = 0x1800b9a40  (PORTED 2026-09-12, deleting_dtor_leaves.cpp)
void* RtDelDtor_Stream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18000c610>());
}
// Graphics::Geometry       ~Geometry = 0x1800a41c0  (PORTED)
void* RtDelDtor_Geometry(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180015e90>());
}
// Video::ImageBuffer       ~ImageBuffer = 0x1800f2610  (PORTED)
void* RtDelDtor_ImageBuffer(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18001ed20>());
}
// Graphics::ParticleInstance ~ParticleInstance = 0x1801041f0  (PORTED 2026-09-12, deleting_dtor_leaves.cpp)
void* RtDelDtor_ParticleInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180020310>());
}
// Graphics::ParticleSystemInstance ~ParticleSystemInstance = 0x1800a5520  (PORTED,
//                          BATCH C 2026-09-12, graphics/batchc_gfx_leaves.cpp;
//                          was "still the DLL's")
void* RtDelDtor_ParticleSystemInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180020ce0>());
}
// Engine::UpdateLoop       ~UpdateLoop = 0x1800dddb0  (PORTED)
void* RtDelDtor_UpdateLoop(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180031900>());
}
// Engine::LoaderLoop       ~LoaderLoop = 0x1800dc170  (PORTED, BATCH 8)
void* RtDelDtor_LoaderLoop(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18004d4f0>());
}
// Engine::DialogLoaderLoopT ~DialogLoaderLoopT = 0x1800db060  (PORTED, BATCH 8)
void* RtDelDtor_DialogLoaderLoopT(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18004d670>());
}
// Engine::DialogLoopT      ~DialogLoopT = 0x1800db340  (PORTED, BATCH 8)
void* RtDelDtor_DialogLoopT(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18004d8b0>());
}
}
}
// IO::CompressedStream     ~CompressedStream = 0x1800aa650  (PORTED, lane PKG2)  [Wave 19]
void* RtDelDtor_CompressedStream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180023fd0>());
}
// Graphics::EdgeMesh          ~EdgeMesh = 0x1800a2780          [Wave 27 GFXMESH]
void* RtDelDtor_EdgeMesh(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x1800134b0>());
}
// Graphics::EdgeMeshInstance  ~EdgeMeshInstance = 0x1800174d0  [Wave 27 GFXMESH]
void* RtDelDtor_EdgeMeshInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x1800175d0>());
}
// Graphics::MeshInstance      ~MeshInstance = 0x180017060      [Wave 27 GFXMESH]
void* RtDelDtor_MeshInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180017330>());
}




// Rows 57-60 (2026-09-12): the four UI::*Description classes the cutover boot
// walked that afternoon. Their table rows landed with the class ports but the
// wrapper + RegisterRuntimeAddr pair did not, so slot 0 of each minted vtable
// stayed a trap; wired here by the coordinator when batch 9's merge exposed it.

// VTABLE BATCH 9 (2026-09-12). Indices 61-66, appended after rows 57-60
// (wired above, after this batch's merge found them without wrappers).
// Object::ExternalList      ~ExternalList = 0x1800076c0           (PORTED, runtime/src/object/external_list.cpp)
void* RtDelDtor_ExternalList(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x180007800>());
}
// Vtable batch 8 (2026-09-12): Collision2D::CollisionMesh/Surface,
// Pathfinding::PointList, Picking::PickingMesh. Indices 67-70 (renumbered at
// the merge: batch 9 took 61-66 on main first).
// UI TEXT PIPELINE batch (2026-09-12). Indices 71-74, counted off the MERGED
// table rather than assumed: this is the fourth batch to append here today and
// the previous three each started at the same index on their own branch.

// ---------------------------------------------------------------------------
// 2026-09-12, VTABLE BATCH 1 -- ten UI::*Description classes, indices 71..80.
//
// The indices were RE-DERIVED from the table after merging main twice (batch 9
// took 61..66, batch 8 took 67..70), not carried over from either append: the
// merge hazard documented at kSpecCount in deleting_dtor_internal.h is an
// index collision the static_assert cannot see, and this batch hit it twice.
//
// Rows 57..60 (Group/Ribbon/Image/AnimatedImageDescription) are NOT wired here:
// they were unwired when this batch started and are wired above, on main, by
// the coordinator. A second wrapper would give one row two owners.
// ---------------------------------------------------------------------------

// VTABLE BATCH 7 (2026-09-12). Indices 81-84 (renumbered at this merge: batch
// 8 took 67-70 on main first, concurrently with this batch's own claim on the
// same range on this branch).
// Vtable batch 6 (2026-09-12): the graphics buffer / mesh / light slot-0 rows.
// INDICES 103-108, RE-COUNTED OFF THE TABLE AFTER THE THIRD MERGE OF main.
// These six were 71-76, then 89-94, and are now 103-108: batches 1, 5 and 7
// took 71-88 and batch 4 elem_B took 89-102 while this branch held them. That
// is the positional-merge hazard the banner and kSpecCount describe, and it has
// now bitten this one batch twice. RE-COUNT after every merge, never before.
void* RtDelDtor_GeneratedImageBuffer(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18001eee0>());
}
void* RtDelDtor_FrameCopyImageBuffer(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, Row<0x18001f260>());
}


// ---- CUTOVER VTABLE BATCH 4 "elem_B" (2026-09-12), rows 85..98 ------------
// All fourteen ~Class bodies are PORTED (runtime/src/ui/<class>element.cpp).

// ---------------------------------------------------------------------------
// VTABLE BATCH 2 (the UI::*Description second half), table rows 103..115.
// The indices below were RE-DERIVED after merging main, not carried over
// from the branch: main had grown the table to 103 rows in the meantime and
// a stale index dispatches the WRONG destructor with no diagnostic.

const DelDtorSpec* Specs() { return kSpecs; }

}  // namespace rtdtor

// ============================================================================
// Registration
//
// By ADDRESS only: every one of them is unnamed in the export directory (see
// the banner). The table has grown past the banner's "37" — Wave 10C added
// UI::Dialog, Wave 10D UI::ToolTipDialog, and Wave 19 lane CSTREAM the two
// IO::Compressed{,File}Stream thunks — so the running count is kSpecCount and
// the standing figures in the banner above are a HISTORICAL record of Wave 9
// lane K's original cluster, not a current census. Lane CSTREAM re-checked the
// "unnamed" claim for its two additions against the DLL's export table rather
// than inheriting it (both `<UNNAMED>`; `AEAA` means private, not unexported,
// and this DLL exports its private members, so that check is not free —
// reference_aeaa_is_private_not_unexported.md).
// Every address below is a LITERAL, deliberately —
// reference_census_cannot_see_nonliteral_registrations.md: a registration built
// from a loop index or a named constant is invisible to tools/runtime_census.py,
// and twelve live ports were missing from the census for exactly that reason.
// Thirty-seven of them behind a fold expression would have been the largest
// instance of that bug the project has had.
// ============================================================================
void RtObjectDeletingDtorPortInit() {
    using namespace rtdtor;

    RegisterRuntimeAddr(0x180002d60, (void*)&RtDelDtor_MaterialInstance);
    RegisterRuntimeAddr(0x180006440, (void*)&RtDelDtor_Internal);
    RegisterRuntimeAddr(0x1800065b0, (void*)&RtDelDtor_Base);
    RegisterRuntimeAddr(0x180006640, (void*)&RtDelDtor_External);
    RegisterRuntimeAddr(0x18000b7e0, (void*)&RtDelDtor_CompiledExternal);
    RegisterRuntimeAddr(0x18000c610, (void*)&RtDelDtor_Stream);
    RegisterRuntimeAddr(0x180015e90, (void*)&RtDelDtor_Geometry);
    RegisterRuntimeAddr(0x18001ed20, (void*)&RtDelDtor_ImageBuffer);
    RegisterRuntimeAddr(0x180020310, (void*)&RtDelDtor_ParticleInstance);
    RegisterRuntimeAddr(0x180020ce0, (void*)&RtDelDtor_ParticleSystemInstance);
    RegisterRuntimeAddr(0x180031900, (void*)&RtDelDtor_UpdateLoop);
    RegisterRuntimeAddr(0x18004d4f0, (void*)&RtDelDtor_LoaderLoop);
    RegisterRuntimeAddr(0x18004d670, (void*)&RtDelDtor_DialogLoaderLoopT);
    RegisterRuntimeAddr(0x18004d8b0, (void*)&RtDelDtor_DialogLoopT);
    RegisterRuntimeAddr(0x180023fd0, (void*)&RtDelDtor_CompressedStream);      // Wave 19 CSTREAM
    RegisterRuntimeAddr(0x1800134b0, (void*)&RtDelDtor_EdgeMesh);              // Wave 27 GFXMESH
    RegisterRuntimeAddr(0x1800175d0, (void*)&RtDelDtor_EdgeMeshInstance);      // Wave 27 GFXMESH
    RegisterRuntimeAddr(0x180017330, (void*)&RtDelDtor_MeshInstance);          // Wave 27 GFXMESH




    // VTABLE BATCH 9
    // Rows 57-60, the UI::*Description four (wrappers above).
    RegisterRuntimeAddr(0x180007800, (void*)&RtDelDtor_ExternalList);

    // 2026-09-12 VTABLE BATCH 1 — ten UI::*Description classes, rows 71..80.
    // Every address a LITERAL, for the reason the banner above gives.

    // VTABLE BATCH 7 (2026-09-12).
    // 2026-09-12 cutover walk: the six scalar dtors this table still resolved
    // to NULL (deleting_dtor_leaves.cpp). Registered here so the address door
    // is closed before the first Dtor(i) lookup can cache a null.
    RtObjectDeletingDtorLeavesRegister();
    // UI TEXT PIPELINE batch, rows 71-74 (wrappers above).
    RegisterRuntimeAddr(0x18001eee0, (void*)&RtDelDtor_GeneratedImageBuffer);  // vtable batch 6
    RegisterRuntimeAddr(0x18001f260, (void*)&RtDelDtor_FrameCopyImageBuffer);  // vtable batch 6

    // ---- CUTOVER VTABLE BATCH 4 "elem_B" (2026-09-12) --------------------

    // VTABLE BATCH 3 (2026-09-12), UI::*Element first half.
    // ---- VTABLE BATCH 2 (the UI::*Description second half), rows 103..115 --

    // The count is taken from kSpecCount rather than typed, so a row added
    // without touching this line cannot make the log say the wrong number --
    // which the hard-coded "38" here would have done for Wave 10D's row.
    printf("[RtDelDtor] %d deleting destructors registered "
                "(one body, %d rows); ~Class resolves lazily\n",
                rtdtor::kSpecCount, rtdtor::kSpecCount);

}
