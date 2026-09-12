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
// 0x1800c7ad0. This file does NOT arm the allocator: BANISHED_RT_ALLOC is lane
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
// consults the port registry. So the `[RtDiff]` per-address `calls=` will read
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
// KILL SWITCHES
// ---------------------------------------------------------------------------
//   BANISHED_RT_DELDTOR=0            all 37 addresses stay on the DLL
//   BANISHED_RT_DELDTOR_PROBE=1      the byte-comparing cases (default OFF)
//   BANISHED_RT_DELDTOR_PROBE=neg    ... plus the negative control
//   BANISHED_RT_DELDTOR_PROBE=all    ... plus the two allocator cases (see above)
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "deleting_dtor_internal.h"

// Same local declaration array.cpp and array_destroy.cpp carry, and for the
// same reason: standalone/resolve.h drags in <windows.h>, which PLAN.md §4.1
// keeps out of runtime/src/. The linker checks the two spellings match.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
// The registry-FREE door, for the probe and for DtorIsPorted below. ResolveAddr
// consults the port registry first, so it cannot answer "what did the DLL
// ship here" (reference_resolveaddr_consults_port_registry_first.md).
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

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

static const DelDtorSpec kSpecs[] = {
    RT_DELDTOR(0x180001680, 0x18009e180, 0x058, "Achievement::Achievement"),
    RT_DELDTOR(0x180001a90, 0x18009e400, 0x028, "Achievement::AchievementInstance"),
    RT_DELDTOR(0x180001d70, 0x18009edb0, 0x030, "Achievement::Goal"),
    RT_DELDTOR(0x1800020c0, 0x18009ef40, 0x020, "Achievement::GoalInstance"),
    RT_DELDTOR(0x180002d60, 0x180103a00, 0x060, "Graphics::MaterialInstance"),
    RT_DELDTOR(0x1800037b0, 0x18011d670, 0x058, "UI::SpriteSheet"),
    RT_DELDTOR(0x180006440, 0x1800bfdb0, 0x010, "Object::Internal"),
    RT_DELDTOR(0x1800065b0, 0x1800c0360, 0x008, "Object::Base"),   // probe subject
    RT_DELDTOR(0x180006640, 0x1800bc950, 0x020, "Object::External"),
    RT_DELDTOR(0x18000b7e0, 0x1800bb980, 0x020, "Object::CompiledExternal"),
    RT_DELDTOR(0x18000c610, 0x1800b9a40, 0x018, "IO::Stream"),
    RT_DELDTOR(0x180011800, 0x1800a2120, 0x020, "Graphics::AnimationEvent"),
    RT_DELDTOR(0x180015e90, 0x1800a41c0, 0x0c0, "Graphics::Geometry"),
    RT_DELDTOR(0x18001ed20, 0x1800f2610, 0x068, "Video::ImageBuffer"),
    RT_DELDTOR(0x180020160, 0x180103c70, 0x048, "Graphics::Particle"),
    RT_DELDTOR(0x180020310, 0x1801041f0, 0x0d8, "Graphics::ParticleInstance"),
    RT_DELDTOR(0x180020ce0, 0x1800a5520, 0x0e0, "Graphics::ParticleSystemInstance"),
    RT_DELDTOR(0x1800244a0, 0x1800a9560, 0x098, "IO::CompressedMemoryStream"),
    RT_DELDTOR(0x180031900, 0x1800dddb0, 0x028, "Engine::UpdateLoop"),
    RT_DELDTOR(0x180035c10, 0x1801161a0, 0x0c0, "UI::Element"),
    RT_DELDTOR(0x18003b800, 0x1800cd2f0, 0x030, "UI::DialogController"),
    RT_DELDTOR(0x1800458c0, 0x180114d40, 0x0b8, "UI::DialogInstance"),
    RT_DELDTOR(0x18004d4f0, 0x1800dc170, 0x048, "Engine::LoaderLoop"),
    RT_DELDTOR(0x18004d670, 0x1800db060, 0x188, "Engine::DialogLoaderLoopT"),
    RT_DELDTOR(0x18004d8b0, 0x1800db340, 0x168, "Engine::DialogLoopT"),
    RT_DELDTOR(0x180058590, 0x1800f8db0, 0x010, "Video::RenderBufferData"),   // probe subject
    RT_DELDTOR(0x180058610, 0x1800f8d90, 0x010, "Video::VideoProgramData"),   // probe subject
    RT_DELDTOR(0x180058690, 0x1800f8d70, 0x010, "Video::RenderStateData"),   // probe subject
    RT_DELDTOR(0x180058710, 0x1800f8dd0, 0x010, "Video::PixelProgramData"),   // probe subject
    RT_DELDTOR(0x180058790, 0x1800f8d10, 0x010, "Video::VertexProgramData"),   // probe subject
    RT_DELDTOR(0x180058810, 0x1800f8d30, 0x010, "Video::VertexLayoutData"),   // probe subject
    RT_DELDTOR(0x180058890, 0x1800f8d50, 0x010, "Video::VertexBufferData"),   // probe subject
    RT_DELDTOR(0x180058910, 0x1800f8df0, 0x010, "Video::IndexBufferData"),   // probe subject
    RT_DELDTOR(0x180058990, 0x1800f8e50, 0x010, "Video::ConstantBufferData"),   // probe subject
    RT_DELDTOR(0x180058a10, 0x1800f8e10, 0x010, "Video::ImageBufferData"),   // probe subject
    RT_DELDTOR(0x180058a90, 0x1800f8e30, 0x010, "Video::ContextData"),   // probe subject
    RT_DELDTOR(0x180058b10, 0x1800f8cf0, 0x010, "Video::DeviceData"),   // probe subject
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
    RT_DELDTOR(0x180038b20, 0x180112e40, 0x088, "UI::Dialog"),
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
    RT_DELDTOR(0x18004d270, 0x1800dac70, 0x0c0, "UI::ToolTipDialog"),
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
    RT_DELDTOR(0x180023fd0, 0x1800aa650, 0x068, "IO::CompressedStream"),
    RT_DELDTOR(0x180024100, 0x1800a9330, 0x0b8, "IO::CompressedFileStream"),
    // ---- WAVE 21 lane ENC ----------------------------------------------------
    // Slot 0 of `??_7TextFileStream@IO@@6B@` (0x180155980), Wave 20 §8 item 3's
    // named expiry: "unblocks when `??1TextFileStream@` lands." It lands here.
    //
    // Both constants disassembled at 0x18002ba90 for this lane (pefile byte read
    // against Runtime-x64.dll, RETAIL — not the profile build):
    //     4C 8D 0D E1 E9 08 00   LEA R9,[rip+0x8e9e1] -> 0x1800ba490  ~TextFileStream
    //     BA 80 00 00 00         MOV EDX,0x80          sizeof(IO::TextFileStream)
    // 0x1800ba490 resolves in the DLL's export directory (checked by VA, not by
    // name pattern) to `??1TextFileStream@IO@@UEAA@XZ` -- UEAA, matching
    // reference_phantom_dtor_mangling_qeaa_vs_ueaa.md's warning that this class
    // is virtual and QEAA would have resolved NULL silently. NOT yet ported —
    // no RegisterRuntimeExport for that name exists anywhere in the tree at the
    // time of this row, so ResolveAddr hands back the DLL's own body and this
    // row makes that address newly `called`
    // (reference_porting_can_grow_the_cutover_surface.md). 0x80 matches
    // runtime/types/io/FileStream.h's independent `static_assert(sizeof(TextFileStream)
    // == 0x80, ...)`.
    //
    // The array-branch CALL target (0x18007ec98, the shared `eh vector
    // destructor iterator`) and the scalar/array MemFree target (0x180006a10,
    // one of the ten per-TU `operator delete` copies the banner already lists)
    // were both re-derived from the same disassembly and match the family's
    // known constants exactly — this row is not a new shape, just a new address.
    //
    // NOT added to kProbeSubjects: ~TextFileStream frees `buffer` (+0x70) when
    // set and calls into `IO::FileStream`'s own teardown, so running it on a
    // fabricated object would be a manufactured crash, not a measurement.
    //
    // APPENDED rather than inserted in address order (it would sort between
    // Graphics::ParticleSystemInstance and IO::CompressedMemoryStream) for the
    // same reason every other row above is appended: kProbeSubjects is a list of
    // TABLE INDICES and a mid-table insert silently re-points all twelve
    // Video::*Data probe subjects at wrong rows.
    RT_DELDTOR(0x18002ba90, 0x1800ba490, 0x080, "IO::TextFileStream"),
    // ---- WAVE 21 lane ENC, item 12 --------------------------------------------
    // Slot 0 of `??_7ReaderWriter@IO@@6B@` (0x1801553b0) -- the ONE non-purecall
    // slot of that 26-slot table (25 are `_purecall`, re-verified this lane with
    // tools/vtable_dump.py against RETAIL Runtime-x64.dll). This is what the
    // census's "ours 0 / slots 1" line at reg_mint_promoted.cpp:1202 was
    // waiting on: the `?` numerator was an expiry with no addressee
    // (reference_expiry_clause_needs_an_addressee.md); Wave 21's kickoff named
    // lane ENC as that addressee. The answer, re-derived from the shipped
    // bytes rather than assumed, is 0 BEFORE this row and 1 AFTER it.
    //
    // Both constants disassembled at 0x180023bd0 (pefile byte read against
    // Runtime-x64.dll, RETAIL):
    //     4C 8D 0D 61 55 09 00   LEA R9,[rip+0x95561] -> 0x1800b9150  ~ReaderWriter
    //     BA 20 00 00 00         MOV EDX,0x20          sizeof(IO::ReaderWriter)
    // 0x1800b9150 resolves in the export directory (checked by VA) to
    // `??1ReaderWriter@IO@@UEAA@XZ` -- UEAA, virtual, matching
    // reference_phantom_dtor_mangling_qeaa_vs_ueaa.md's warning. NOT ported (no
    // RegisterRuntimeExport for that name exists in the tree), so this row
    // makes that address newly `called`
    // (reference_porting_can_grow_the_cutover_surface.md) -- the same shape as
    // the UI::ToolTipDialog and IO::CompressedFileStream rows above, where the
    // deleting-dtor thunk is ported before its callee. 0x20 matches
    // runtime/types/io/Serializer.h's `struct ReaderWriter` (4 * 8-byte fields,
    // no static_assert on the whole-struct size there, but every FIELD offset
    // through +0x18 is asserted and the struct has no members past it).
    //
    // The array-branch CALL target (0x18007ec98, the shared iterator) and the
    // MemFree target (0x180006a10) both match the family's known constants.
    //
    // THIS DOES NOT PROMOTE THE ROW. `??_7ReaderWriter@IO@@6B@` is tagged
    // RT_UNPROMOTABLE | NONE, EVER in reg_mint_promoted.cpp on independent
    // grounds (20 stampers, all in-DLL RIP-relative LEA, zero calls through the
    // stamped object across every stamp window) that this row does not touch.
    // Filling the one real slot moves the "ours" count truthfully; it does not
    // change the promotion verdict.
    //
    // ADDED to kProbeSubjects: ~ReaderWriter's entire body is
    // `LEA RAX,[??_7ReaderWriter@IO@@6B@]; MOV [RCX],RAX; RET` -- a pure vtable
    // restamp with no member destroys and no frees, the same shape as the
    // Object::Base and Video::*Data subjects already in the list.
    //
    // APPENDED (would sort between IO::CompressedFileStream and
    // IO::TextFileStream) for the same reason every other row above is
    // appended: kProbeSubjects is a list of TABLE INDICES.
    RT_DELDTOR(0x180023bd0, 0x1800b9150, 0x020, "IO::ReaderWriter"),
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
    RT_DELDTOR(0x1800259d0, 0x1800b4540, 0x0f0, "IO::PackageFile"),
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
    RT_DELDTOR(0x180058dd0, 0x180058d20, 0x0c0, "Graphics::Light"),
    RT_DELDTOR(0x180064620, 0x180108160, 0x108, "Graphics::SunLight"),

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
    RT_DELDTOR(0x180035350, 0x180116100, 0x068, "UI::ElementDescription"),
    // 2026-09-12: slot 0 of ??_7Material@Graphics@@6B@. ~Material 0x180102a00 is
    // ported in runtime/src/graphics/material.cpp; sizeof from the
    // eh_vector_destructor_iterator call in 0x180011d20 (0x38).
    RT_DELDTOR(0x180011d20, 0x180102a00, 0x038, "Graphics::Material"),
    // 2026-09-12: slot 0 of ??_7VideoProgram@Video@@6B@ (videoprogram.cpp); 0x48 from
    // CreateNewObject's Allocate(0x48, 8) and the eh_vector iterator stride.
    RT_DELDTOR(0x18004efb0, 0x1800f4650, 0x048, "Video::VideoProgram"),
    RT_DELDTOR(0x18004e000, 0x1800f5df0, 0x048, "Video::VertexLayout"),   // vertexlayout.cpp (2026-09-12)
    // 2026-09-12: the Video shader family, all four slot-0 entries. Each row's two
    // constants were read from the shipped bytes, not from the class header:
    //   0x18004e360  LEA R9,[0x1800f50b0]  MOV EDX,0x50   Video::ShaderProgram
    //   0x18004e810  LEA R9,[0x1800f61b0]  MOV EDX,0x58   Video::VertexProgram
    //   0x18004eb90  LEA R9,[0x1800f3a00]  MOV EDX,0x58   Video::PixelProgram
    //   0x18004b9d0  LEA R9,[0x1800f4c30]  MOV EDX,0x80   Video::RenderState
    // All four destructors are ported in runtime/src/video/.
    //
    // THREE OF THE FOUR INLINE ~Class IN THE SCALAR ARM and call a CLASS-SPECIFIC
    // `operator delete` (??3VertexProgram 0x18004e5f0, ??3PixelProgram 0x18004e970,
    // ??3RenderState 0x18004b600) instead of the shared 0x180006a10. That changes
    // nothing here: all three were disassembled for this commit and are the same
    // `if (p) { Enter(cs); Free(_singleton, p); Leave(cs); }` as the other ten
    // copies this file's banner enumerates, so DeletingDtorCore's single MemFree
    // is retail's body. The `LEA R9` in each VECTOR arm still names the real
    // destructor, which is what tools/rt_deleting_dtor_verify.py reads.
    RT_DELDTOR(0x18004e360, 0x1800f50b0, 0x050, "Video::ShaderProgram"),
    RT_DELDTOR(0x18004e810, 0x1800f61b0, 0x058, "Video::VertexProgram"),
    RT_DELDTOR(0x18004eb90, 0x1800f3a00, 0x058, "Video::PixelProgram"),
    RT_DELDTOR(0x18004b9d0, 0x1800f4c30, 0x080, "Video::RenderState"),
};
static DtorFn g_dtorCache[kSpecCount];
static Site   g_sites[kSpecCount];

static_assert(sizeof(kSpecs) / sizeof(kSpecs[0]) == kSpecCount,
              "kSpecCount must match the table -- the probe and the site array "
              "are sized from it");

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


// Armed at init only when BANISHED_RT_DELDTOR_PROBE=all; see the probe's
// "WHEN IT RUNS". Nothing calls a deleting destructor before cutover, so this
// gate is expected NOT to fire today -- it exists so that the moment lane C2's
// mint is armed and something does, the allocating cases run at a point where
// the Runtime heap is provably started (the caller is destroying an object).
bool g_allocProbePending = false;
static bool g_allocProbeRunning = false;

void* DeletingDtorCore(void* self, unsigned flags, int i) {
    if (g_allocProbePending && !g_allocProbeRunning) {
        g_allocProbeRunning = true;
        g_allocProbePending = false;
        ProbeRunAllocating();
        g_allocProbeRunning = false;
    }
    // Read the array count BEFORE the body runs: on the array path the block is
    // freed, so afterwards the cookie is either gone or reused.
    const int n = (flags & 2u)
        ? *reinterpret_cast<const int32_t*>(static_cast<uint8_t*>(self) - 8) : 1;
    void* ret = DeletingDtorCoreWith(self, flags, i, Dtor(i));
    if (RtDiffEnabled()) {
        Site& st = g_sites[i];
        if (!st.d.name) st.d.name = kSpecs[i].name;
        RtDiffRegister(&st.d);
        char desc[160];
        std::snprintf(desc, sizeof(desc), "%s flags=%u %s destroyed=%d freed=%d",
                      kSpecs[i].name, flags, (flags & 2u) ? "array" : "scalar",
                      n > 0 ? n : 0, (flags & 1u) ? 1 : 0);
        RtDiffReport(&st.d, true, desc, desc);
    }
    return ret;
}

// The probe's "normal" door: the same destructor the live path uses, without
// the diff reporting the wrapper adds.
DtorFn NormalDtor(int i) { return Dtor(i); }

// Whether row `i`'s destructor resolves to OUR port or to the DLL. Reported by
// the probe so a red line names the right lane.
bool DtorIsPorted(int i) {
    return reinterpret_cast<uintptr_t>(Dtor(i)) != ResolveRuntimeRaw(kSpecs[i].dtorVa);
}

DtorFn RawDtor(int i) {
    return reinterpret_cast<DtorFn>(ResolveRuntimeRaw(kSpecs[i].dtorVa));
}

// Achievement::Achievement ~Achievement = 0x18009e180  (PORTED)
void* RtDelDtor_Achievement(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 0);
}
// Achievement::AchievementInstance ~AchievementInstance = 0x18009e400  (still the DLL's)
void* RtDelDtor_AchievementInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 1);
}
// Achievement::Goal        ~Goal = 0x18009edb0  (PORTED)
void* RtDelDtor_Goal(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 2);
}
// Achievement::GoalInstance ~GoalInstance = 0x18009ef40  (PORTED)
void* RtDelDtor_GoalInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 3);
}
// Graphics::MaterialInstance ~MaterialInstance = 0x180103a00  (still the DLL's)
void* RtDelDtor_MaterialInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 4);
}
// UI::SpriteSheet          ~SpriteSheet = 0x18011d670  (still the DLL's)
void* RtDelDtor_SpriteSheet(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 5);
}
// Object::Internal         ~Internal = 0x1800bfdb0  (PORTED)
void* RtDelDtor_Internal(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 6);
}
// Object::Base             ~Base = 0x1800c0360  (still the DLL's)
void* RtDelDtor_Base(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 7);
}
// Object::External         ~External = 0x1800bc950  (PORTED)
void* RtDelDtor_External(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 8);
}
// Object::CompiledExternal ~CompiledExternal = 0x1800bb980  (still the DLL's)
void* RtDelDtor_CompiledExternal(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 9);
}
// IO::Stream               ~Stream = 0x1800b9a40  (still the DLL's)
void* RtDelDtor_Stream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 10);
}
// Graphics::AnimationEvent ~AnimationEvent = 0x1800a2120  (PORTED)
void* RtDelDtor_AnimationEvent(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 11);
}
// Graphics::Geometry       ~Geometry = 0x1800a41c0  (PORTED)
void* RtDelDtor_Geometry(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 12);
}
// Video::ImageBuffer       ~ImageBuffer = 0x1800f2610  (PORTED)
void* RtDelDtor_ImageBuffer(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 13);
}
// Graphics::Particle       ~Particle = 0x180103c70  (still the DLL's)
void* RtDelDtor_Particle(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 14);
}
// Graphics::ParticleInstance ~ParticleInstance = 0x1801041f0  (still the DLL's)
void* RtDelDtor_ParticleInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 15);
}
// Graphics::ParticleSystemInstance ~ParticleSystemInstance = 0x1800a5520  (still the DLL's)
void* RtDelDtor_ParticleSystemInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 16);
}
// IO::CompressedMemoryStream ~CompressedMemoryStream = 0x1800a9560  (PORTED)
void* RtDelDtor_CompressedMemoryStream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 17);
}
// Engine::UpdateLoop       ~UpdateLoop = 0x1800dddb0  (PORTED)
void* RtDelDtor_UpdateLoop(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 18);
}
// UI::Element              ~Element = 0x1801161a0  (still the DLL's)
void* RtDelDtor_Element(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 19);
}
// UI::DialogController     ~DialogController = 0x1800cd2f0  (PORTED)
void* RtDelDtor_DialogController(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 20);
}
// UI::DialogInstance       ~DialogInstance = 0x180114d40  (still the DLL's)
void* RtDelDtor_DialogInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 21);
}
// Engine::LoaderLoop       ~LoaderLoop = 0x1800dc170  (still the DLL's)
void* RtDelDtor_LoaderLoop(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 22);
}
// Engine::DialogLoaderLoopT ~DialogLoaderLoopT = 0x1800db060  (still the DLL's)
void* RtDelDtor_DialogLoaderLoopT(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 23);
}
// Engine::DialogLoopT      ~DialogLoopT = 0x1800db340  (still the DLL's)
void* RtDelDtor_DialogLoopT(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 24);
}
// Video::RenderBufferData  ~RenderBufferData = 0x1800f8db0  (PORTED)
void* RtDelDtor_RenderBufferData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 25);
}
// Video::VideoProgramData  ~VideoProgramData = 0x1800f8d90  (PORTED)
void* RtDelDtor_VideoProgramData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 26);
}
// Video::RenderStateData   ~RenderStateData = 0x1800f8d70  (PORTED)
void* RtDelDtor_RenderStateData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 27);
}
// Video::PixelProgramData  ~PixelProgramData = 0x1800f8dd0  (PORTED)
void* RtDelDtor_PixelProgramData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 28);
}
// Video::VertexProgramData ~VertexProgramData = 0x1800f8d10  (PORTED)
void* RtDelDtor_VertexProgramData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 29);
}
// Video::VertexLayoutData  ~VertexLayoutData = 0x1800f8d30  (PORTED)
void* RtDelDtor_VertexLayoutData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 30);
}
// Video::VertexBufferData  ~VertexBufferData = 0x1800f8d50  (PORTED)
void* RtDelDtor_VertexBufferData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 31);
}
// Video::IndexBufferData   ~IndexBufferData = 0x1800f8df0  (PORTED)
void* RtDelDtor_IndexBufferData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 32);
}
// Video::ConstantBufferData ~ConstantBufferData = 0x1800f8e50  (PORTED)
void* RtDelDtor_ConstantBufferData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 33);
}
// Video::ImageBufferData   ~ImageBufferData = 0x1800f8e10  (PORTED)
void* RtDelDtor_ImageBufferData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 34);
}
// Video::ContextData       ~ContextData = 0x1800f8e30  (PORTED)
void* RtDelDtor_ContextData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 35);
}
// Video::DeviceData        ~DeviceData = 0x1800f8cf0  (PORTED)
void* RtDelDtor_DeviceData(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 36);
}
// UI::Dialog               ~Dialog = 0x180112e40  (PORTED, lane DLG2)  [Wave 10C]
void* RtDelDtor_Dialog(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 37);
}
// UI::ToolTipDialog        ~ToolTipDialog = 0x1800dac70  (NOT ported)  [Wave 10D]
void* RtDelDtor_ToolTipDialog(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 38);
}
// IO::CompressedStream     ~CompressedStream = 0x1800aa650  (PORTED, lane PKG2)  [Wave 19]
void* RtDelDtor_CompressedStream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 39);
}
// IO::CompressedFileStream ~CompressedFileStream = 0x1800a9330  (PORTED, Wave 27 IOTEXT
//                          -- runtime/src/io/file_stream.cpp; this row's address
//                          door is what made porting it worth doing)
void* RtDelDtor_CompressedFileStream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 40);
}
// IO::TextFileStream       ~TextFileStream = 0x1800ba490  (PORTED, Wave 27 IOTEXT --
//                          runtime/src/io/file_stream.cpp; this row's own
//                          address door is what made porting it worth doing)
void* RtDelDtor_TextFileStream(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 41);
}
// IO::ReaderWriter         ~ReaderWriter = 0x1800b9150  (NOT ported)  [Wave 21 ENC, item 12]
void* RtDelDtor_ReaderWriter(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 42);
}
// IO::PackageFile          ~PackageFile = 0x1800b4540  (PORTED -- Wave 25 lane BODY,
//                          runtime/src/io/package_file_dtor.cpp:442. This line read
//                          "NOT ported" until Wave 27 lane IOTEXT; the two waves
//                          landed in the same wave and the comment never caught up.)
void* RtDelDtor_PackageFile(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 43);
}
// Graphics::EdgeMesh          ~EdgeMesh = 0x1800a2780          [Wave 27 GFXMESH]
void* RtDelDtor_EdgeMesh(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 44);
}
// Graphics::EdgeMeshInstance  ~EdgeMeshInstance = 0x1800174d0  [Wave 27 GFXMESH]
void* RtDelDtor_EdgeMeshInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 45);
}
// Graphics::MeshInstance      ~MeshInstance = 0x180017060      [Wave 27 GFXMESH]
void* RtDelDtor_MeshInstance(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 46);
}

// Graphics::Light          ~Light = 0x180058d20  (NOT ported)  [Wave 27 GFXPART]
void* RtDelDtor_Light(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 47);
}
// Graphics::SunLight       ~SunLight = 0x180108160  (NOT ported)  [Wave 27 GFXPART]
void* RtDelDtor_SunLight(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 48);
}

// UI::ElementDescription   ~ElementDescription = 0x180116100  (PORTED)  [Wave 27 IOTEXT]
void* RtDelDtor_ElementDescription(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 49);
}

// Graphics::Material        ~Material = 0x180102a00  (PORTED, material.cpp)
void* RtDelDtor_Material(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 50);
}

// Video::VideoProgram       ~VideoProgram = 0x1800f4650  (PORTED, videoprogram.cpp)
void* RtDelDtor_VideoProgram(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 51);
}

// Video::VertexLayout       ~VertexLayout = 0x1800f5df0  (PORTED, vertexlayout.cpp)
void* RtDelDtor_VertexLayout(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 52);
}

// Video::ShaderProgram      ~ShaderProgram = 0x1800f50b0  (PORTED, shaderprogram.cpp)
void* RtDelDtor_ShaderProgram(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 53);
}
// Video::VertexProgram      ~VertexProgram = 0x1800f61b0  (PORTED, vertexprogram.cpp)
void* RtDelDtor_VertexProgram(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 54);
}
// Video::PixelProgram       ~PixelProgram = 0x1800f3a00   (PORTED, pixelprogram.cpp)
void* RtDelDtor_PixelProgram(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 55);
}
// Video::RenderState        ~RenderState = 0x1800f4c30    (PORTED, renderstate.cpp)
void* RtDelDtor_RenderState(void* self, unsigned flags) {
    return DeletingDtorCore(self, flags, 56);
}

static const int kProbeSubjects[] = { 7, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 42 };
const int* ProbeSubjects(int* count) {
    *count = static_cast<int>(sizeof(kProbeSubjects) / sizeof(kProbeSubjects[0]));
    return kProbeSubjects;
}

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
    if (!RtWaveEnabled("BANISHED_RT_DELDTOR")) {
        // The count is taken from kSpecCount rather than typed, for the same
        // reason the summary at the bottom of this file already does it: two
        // earlier lanes appended rows and left this literal reading 37.
        printf("[RtDelDtor] DISABLED via BANISHED_RT_DELDTOR=0 -- all %d "
                    "deleting destructors stay on the DLL\n", rtdtor::kSpecCount);
        return;
    }

    using namespace rtdtor;

    RegisterRuntimeAddr(0x180001680, (void*)&RtDelDtor_Achievement);
    RegisterRuntimeAddr(0x180001a90, (void*)&RtDelDtor_AchievementInstance);
    RegisterRuntimeAddr(0x180001d70, (void*)&RtDelDtor_Goal);
    RegisterRuntimeAddr(0x1800020c0, (void*)&RtDelDtor_GoalInstance);
    RegisterRuntimeAddr(0x180002d60, (void*)&RtDelDtor_MaterialInstance);
    RegisterRuntimeAddr(0x1800037b0, (void*)&RtDelDtor_SpriteSheet);
    RegisterRuntimeAddr(0x180006440, (void*)&RtDelDtor_Internal);
    RegisterRuntimeAddr(0x1800065b0, (void*)&RtDelDtor_Base);
    RegisterRuntimeAddr(0x180006640, (void*)&RtDelDtor_External);
    RegisterRuntimeAddr(0x18000b7e0, (void*)&RtDelDtor_CompiledExternal);
    RegisterRuntimeAddr(0x18000c610, (void*)&RtDelDtor_Stream);
    RegisterRuntimeAddr(0x180011800, (void*)&RtDelDtor_AnimationEvent);
    RegisterRuntimeAddr(0x180015e90, (void*)&RtDelDtor_Geometry);
    RegisterRuntimeAddr(0x18001ed20, (void*)&RtDelDtor_ImageBuffer);
    RegisterRuntimeAddr(0x180020160, (void*)&RtDelDtor_Particle);
    RegisterRuntimeAddr(0x180020310, (void*)&RtDelDtor_ParticleInstance);
    RegisterRuntimeAddr(0x180020ce0, (void*)&RtDelDtor_ParticleSystemInstance);
    RegisterRuntimeAddr(0x1800244a0, (void*)&RtDelDtor_CompressedMemoryStream);
    RegisterRuntimeAddr(0x180031900, (void*)&RtDelDtor_UpdateLoop);
    RegisterRuntimeAddr(0x180035c10, (void*)&RtDelDtor_Element);
    RegisterRuntimeAddr(0x18003b800, (void*)&RtDelDtor_DialogController);
    RegisterRuntimeAddr(0x1800458c0, (void*)&RtDelDtor_DialogInstance);
    RegisterRuntimeAddr(0x18004d4f0, (void*)&RtDelDtor_LoaderLoop);
    RegisterRuntimeAddr(0x18004d670, (void*)&RtDelDtor_DialogLoaderLoopT);
    RegisterRuntimeAddr(0x18004d8b0, (void*)&RtDelDtor_DialogLoopT);
    RegisterRuntimeAddr(0x180058590, (void*)&RtDelDtor_RenderBufferData);
    RegisterRuntimeAddr(0x180058610, (void*)&RtDelDtor_VideoProgramData);
    RegisterRuntimeAddr(0x180058690, (void*)&RtDelDtor_RenderStateData);
    RegisterRuntimeAddr(0x180058710, (void*)&RtDelDtor_PixelProgramData);
    RegisterRuntimeAddr(0x180058790, (void*)&RtDelDtor_VertexProgramData);
    RegisterRuntimeAddr(0x180058810, (void*)&RtDelDtor_VertexLayoutData);
    RegisterRuntimeAddr(0x180058890, (void*)&RtDelDtor_VertexBufferData);
    RegisterRuntimeAddr(0x180058910, (void*)&RtDelDtor_IndexBufferData);
    RegisterRuntimeAddr(0x180058990, (void*)&RtDelDtor_ConstantBufferData);
    RegisterRuntimeAddr(0x180058a10, (void*)&RtDelDtor_ImageBufferData);
    RegisterRuntimeAddr(0x180058a90, (void*)&RtDelDtor_ContextData);
    RegisterRuntimeAddr(0x180058b10, (void*)&RtDelDtor_DeviceData);
    RegisterRuntimeAddr(0x180038b20, (void*)&RtDelDtor_Dialog);   // Wave 10C DLG8
    RegisterRuntimeAddr(0x18004d270, (void*)&RtDelDtor_ToolTipDialog);  // Wave 10D TIP
    RegisterRuntimeAddr(0x180023fd0, (void*)&RtDelDtor_CompressedStream);      // Wave 19 CSTREAM
    RegisterRuntimeAddr(0x180024100, (void*)&RtDelDtor_CompressedFileStream);  // Wave 19 CSTREAM
    RegisterRuntimeAddr(0x18002ba90, (void*)&RtDelDtor_TextFileStream);        // Wave 21 ENC
    RegisterRuntimeAddr(0x180023bd0, (void*)&RtDelDtor_ReaderWriter);          // Wave 21 ENC item 12
    RegisterRuntimeAddr(0x1800259d0, (void*)&RtDelDtor_PackageFile);           // Wave 25 VTIO
    RegisterRuntimeAddr(0x1800134b0, (void*)&RtDelDtor_EdgeMesh);              // Wave 27 GFXMESH
    RegisterRuntimeAddr(0x1800175d0, (void*)&RtDelDtor_EdgeMeshInstance);      // Wave 27 GFXMESH
    RegisterRuntimeAddr(0x180017330, (void*)&RtDelDtor_MeshInstance);          // Wave 27 GFXMESH

    RegisterRuntimeAddr(0x180058dd0, (void*)&RtDelDtor_Light);                 // Wave 27 GFXPART
    RegisterRuntimeAddr(0x180064620, (void*)&RtDelDtor_SunLight);              // Wave 27 GFXPART

    RegisterRuntimeAddr(0x180035350, (void*)&RtDelDtor_ElementDescription);    // Wave 27 IOTEXT
    RegisterRuntimeAddr(0x180011d20, (void*)&RtDelDtor_Material);
    RegisterRuntimeAddr(0x18004efb0, (void*)&RtDelDtor_VideoProgram);
    RegisterRuntimeAddr(0x18004e000, (void*)&RtDelDtor_VertexLayout);
    RegisterRuntimeAddr(0x18004e360, (void*)&RtDelDtor_ShaderProgram);
    RegisterRuntimeAddr(0x18004e810, (void*)&RtDelDtor_VertexProgram);
    RegisterRuntimeAddr(0x18004eb90, (void*)&RtDelDtor_PixelProgram);
    RegisterRuntimeAddr(0x18004b9d0, (void*)&RtDelDtor_RenderState);

    // The count is taken from kSpecCount rather than typed, so a row added
    // without touching this line cannot make the log say the wrong number --
    // which the hard-coded "38" here would have done for Wave 10D's row.
    printf("[RtDelDtor] %d deleting destructors registered "
                "(one body, %d rows); ~Class resolves lazily\n",
                rtdtor::kSpecCount, rtdtor::kSpecCount);

    // The byte-comparing half of the oracle. It allocates nothing and runs
    // here rather than on first call, because nothing calls these before
    // cutover -- see the banner.
    rtdtor::ProbeRunStatic();

    // The allocating half arms itself for the first real call, which is a
    // moment we know the Runtime heap is started. Today there is no such call.
    {
        const char* m = std::getenv("BANISHED_RT_DELDTOR_PROBE");
        if (m && std::strcmp(m, "all") == 0) rtdtor::g_allocProbePending = true;
    }
}
