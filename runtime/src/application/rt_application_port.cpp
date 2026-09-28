// ============================================================================
// runtime/src/application/rt_application_port.cpp — RtApplicationPortInit()
//                                                    Wave 8 lane E
// ============================================================================
//   RT_APP=0   leave the whole Application namespace on the DLL
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../collection/set_avl.h"

// ENDGAME B.3.7: `Application::Application` is the real class; a global
// `using RtApplication::Application;` is ill-formed next to the namespace.
using App = Application::Application;

// application_lifecycle.cpp (no Win32) — Wave 12 lane B
extern "C" void          RtApp_Dtor(App* self);
extern "C" void          RtApp_SetDestroyTree(void* self, rtsetavl::Node* n);

void RtApplicationPortInit() {
    int n = 0;
    // B3R (registry teardown, W1 RT-Core): the rows no code read by name or
    // by address are gone; what is left has a reader elsewhere.

    // ---- Wave 12 lane B: the lifecycle family -----------------------------
    //
    // Every mangled name below was read out of the SHIPPED Runtime-x64.dll
    // export directory with pefile before the bodies were written, and the
    // ordinals are quoted so a future reader can re-check without re-deriving
    // (reference_resolveexport_phantom_names.md). Literals, never named
    // constants (reference_census_cannot_see_nonliteral_registrations.md).
    //
    // `AEAA` on the two Platform* rows means PRIVATE, not unexported — this DLL
    // exports its private members, and the export table is what settled it
    // rather than the spelling (reference_aeaa_means_private_not_unexported.md).
    //
    // BOTH DOORS for all five, per the project rule: the name door flips our
    // ~1,180 call sites, the address door catches anything that still resolves
    // by address (the exe's dispatch slots and dx11's import table both do).

    RegisterRuntimeExport("??1Application@0@QEAA@XZ",                  // ord 530
                          (void*)&RtApp_Dtor); ++n;
    RegisterRuntimeAddr(0x18009f520, (void*)&RtApp_Dtor);

    // The three `Collection::Set` helpers the five bodies above are built out
    // of. ADDRESS DOORS ONLY, and that is a MEASUREMENT, not an assumption: the
    // pefile sweep over the shipped export directory returns nothing at any of
    // these three addresses, so `RegisterRuntimeExport` has no name to register
    // them under. Said explicitly rather than left implied (lane PICK §3).
    //
    // They are registered because retail reaches all three by direct near CALL
    // from bodies we replace wholesale, so anything left going by address —
    // another DLL body we have not ported, or a future port of `Set::Assign`
    // at 0x18000a4a0, which calls two of the three — arrives here.
    //
    // 0x180009c30 (the allocator-lock Free wrapper these three end in) is
    // DELIBERATELY NOT registered here: runtime/src/system/allocator.cpp is its
    // single owner and registers it unconditionally. Wave 9 found it registered
    // three times, with last-binding-wins deciding which body survived; adding
    // a fourth would reopen exactly that hazard.
    RegisterRuntimeAddr(0x18000a4f0, (void*)&RtApp_SetDestroyTree);

    // ?_singleton@Application@1@0PEAV11@EA @ 0x1801fc678 — a DATA export, not
    // a function. THE REASON RECORDED HERE UNTIL WAVE 12 WAS WRONG. It used to
    // read "the seam maps names to functions, so it cannot be redirected before
    // cutover". That was true when written and stopped being true in Wave 10,
    // which built exactly that machinery: standalone/rt_data_mint.cpp and
    // reg_mint_promoted.cpp mint storage for DATA exports and register them by
    // name, and 17 such rows are promoted and default-on today. So "we cannot"
    // is false; the real question is "should we", and the answer is no.
    //
    // Ghidra get_xrefs_to(0x1801fc678), run 2026-09-09: TWENTY-SIX in-DLL
    // RIP-relative references, 2 WRITE and 24 READ, across at least four
    // namespaces — the Application ctor (WRITE 0x18009f5b8) and a Startup
    // (WRITE 0x1800dbbe2); WindowProc, Get, EnableCursor (0x18012258d),
    // Input::PlatformStartup / PlatformShutdown / PlatformUpdate /
    // PlatformGetKeyboardState / PlatformGetMouseState, and two Update bodies.
    // WE INTERCEPT ONLY AT DOORS, AND A RIP-RELATIVE MOV IS NOT A DOOR. Minting
    // our own storage would split the brain: our readers on our copy, those 26
    // sites on theirs, the ctor publishing into only one. Strictly worse than
    // what we have.
    //
    // What we have is correct. RtApp_Get (application_accessors.cpp) resolves
    // the EXPORTED NAME through RT_DATA_SLOT, i.e. it reads the very slot
    // retail writes, and Wave 12 lane VERIFY swept it against the shipped ?Get@
    // through 6 sentinels written into that slot: 6/6 identical. That probe
    // refuses to run under RT_MINT=1 and reports SPLIT instead — which
    // is the guard against someone later minting this row without reading this.
    //
    // This row is therefore the Application namespace's CEILING on the census
    // called side (15/16 = 93.8%), for a structural reason, not for want of
    // work. It is
    // already wired: standalone/address_registry.cpp registers exe dispatch
    // slot 0x140152368 to ResolveAddr(Module::Runtime, 0x1801fc678), which is
    // this exact address, read directly and confirmed against the ctor's own
    // `MOV qword ptr [0x1801fc678],RCX` store — no change needed here.

    printf("[RtPort] Application: %d exports registered\n", n);
}
