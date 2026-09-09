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

static const int kProbeSubjects[] = { 7, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36 };
const int* ProbeSubjects(int* count) {
    *count = static_cast<int>(sizeof(kProbeSubjects) / sizeof(kProbeSubjects[0]));
    return kProbeSubjects;
}

const DelDtorSpec* Specs() { return kSpecs; }

}  // namespace rtdtor

// ============================================================================
// Registration
//
// By ADDRESS only: all 37 are unnamed in the export directory (see the banner).
// Every address below is a LITERAL, deliberately —
// reference_census_cannot_see_nonliteral_registrations.md: a registration built
// from a loop index or a named constant is invisible to tools/runtime_census.py,
// and twelve live ports were missing from the census for exactly that reason.
// Thirty-seven of them behind a fold expression would have been the largest
// instance of that bug the project has had.
// ============================================================================
void RtObjectDeletingDtorPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_DELDTOR")) {
        printf("[RtDelDtor] DISABLED via BANISHED_RT_DELDTOR=0 -- all 37 "
                    "deleting destructors stay on the DLL\n");
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

    printf("[RtDelDtor] 38 deleting destructors registered "
                "(one body, 37 rows); ~Class resolves lazily\n");

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
