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

// application.cpp (no Win32)
bool RtApp_IsPaused(const App* self);
void RtApp_Quit(App* self);

// application_win.cpp (Win32 platform seam)
bool RtApp_PlatformUpdate(App* self);
void RtApp_SleepThread(App* self, int ms);
void RtApp_YieldToThreads(App* self);
void RtApp_EnableCursor(App* self, bool enabled);
bool RtApp_LaunchProcess(App* self, const char* cmd, bool wait);

// application_accessors.cpp (no Win32) -- Wave 12 lane A
App* RtApp_Get();
void* RtApp_GetInstance(const App* self);
void* RtApp_GetWindow(const App* self);
uint8_t RtApp_IsCursorEnabled(App* self);   // RAW BYTE, not a normalised bool -- see its banner
bool RtApp_PlatformIsPaused(const App* self);
void RtApp_PlatformDestruct(App* self);

// application_thunks.cpp -- Wave 12 lane A
bool RtApp_Update(App* self);
void RtApp_Shutdown(App* self);
bool RtApp_Startup(App* self);

extern "C" void RtApp_PlatformConstruct(App* self, void* platformHandle);

// platform_startup_win.cpp (Win32 platform seam; lives beside PlatformStartup
// because the two share the `kClassName` literal — see its banner)
extern "C" void RtApp_PlatformShutdown(App* self);

// application_lifecycle.cpp (no Win32) — Wave 12 lane B
extern "C" void          RtApp_Dtor(App* self);
extern "C" App*  RtApp_CopyCtor(App* self, const App* other);
extern "C" App*  RtApp_Assign(App* self, const App* other);
extern "C" void          RtApp_SetDestroyTree(void* self, rtsetavl::Node* n);
extern "C" rtsetavl::Node* RtApp_SetCloneTree(void* self, const rtsetavl::Node* src);
extern "C" void          RtApp_SetClear(rtsetavl::Set* set);

void RtApplicationPortInit() {
    int n = 0;
    // ENDGAME B.3.9 ENG (B39_RULES D7): the minted storage row IS the class
    // static from here on (name door, address door, manifest publish).
    RtMintRedirectStorage("?_singleton@Application@1@0PEAV11@EA", &Application::Application::_singleton);

    RegisterRuntimeExport("?IsPaused@Application@1@QEAA_NXZ", (void*)&RtApp_IsPaused); ++n;
    RegisterRuntimeAddr(0x18009f4e0, (void*)&RtApp_IsPaused);

    RegisterRuntimeExport("?Quit@Application@1@QEAAXXZ", (void*)&RtApp_Quit); ++n;
    RegisterRuntimeAddr(0x18009f4d0, (void*)&RtApp_Quit);

    RegisterRuntimeExport("?PlatformUpdate@Application@1@AEAA_NXZ", (void*)&RtApp_PlatformUpdate); ++n;
    RegisterRuntimeAddr(0x18009fdf0, (void*)&RtApp_PlatformUpdate);

    RegisterRuntimeExport("?SleepThread@Application@1@QEAAXH@Z", (void*)&RtApp_SleepThread); ++n;
    RegisterRuntimeAddr(0x18009f850, (void*)&RtApp_SleepThread);

    RegisterRuntimeExport("?YieldToThreads@Application@1@QEAAXXZ", (void*)&RtApp_YieldToThreads); ++n;
    RegisterRuntimeAddr(0x18009f920, (void*)&RtApp_YieldToThreads);

    // Wave 12 lane A -- the nine trivial-body exports.
    RegisterRuntimeExport("?Get@Application@1@SAPEAV11@XZ", (void*)&RtApp_Get); ++n;
    RegisterRuntimeAddr(0x18000a2f0, (void*)&RtApp_Get);

    RegisterRuntimeExport("?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ", (void*)&RtApp_GetInstance); ++n;
    RegisterRuntimeAddr(0x18009fb90, (void*)&RtApp_GetInstance);

    RegisterRuntimeExport("?GetWindow@Application@1@QEBAPEAUHWND__@@XZ", (void*)&RtApp_GetWindow); ++n;
    RegisterRuntimeAddr(0x18009fba0, (void*)&RtApp_GetWindow);

    RegisterRuntimeExport("?IsCursorEnabled@Application@1@QEAA_NXZ", (void*)&RtApp_IsCursorEnabled); ++n;
    RegisterRuntimeAddr(0x18000a0c0, (void*)&RtApp_IsCursorEnabled);

    RegisterRuntimeExport("?PlatformIsPaused@Application@1@AEAA_NXZ", (void*)&RtApp_PlatformIsPaused); ++n;
    RegisterRuntimeAddr(0x18009fe80, (void*)&RtApp_PlatformIsPaused);

    RegisterRuntimeExport("?PlatformDestruct@Application@1@AEAAXXZ", (void*)&RtApp_PlatformDestruct); ++n;
    RegisterRuntimeAddr(0x18009fc00, (void*)&RtApp_PlatformDestruct);

    RegisterRuntimeExport("?Update@Application@1@QEAA_NXZ", (void*)&RtApp_Update); ++n;
    RegisterRuntimeAddr(0x18009f4f0, (void*)&RtApp_Update);

    RegisterRuntimeExport("?Shutdown@Application@1@QEAAXXZ", (void*)&RtApp_Shutdown); ++n;
    RegisterRuntimeAddr(0x18009f500, (void*)&RtApp_Shutdown);

    RegisterRuntimeExport("?Startup@Application@1@QEAA_NXZ", (void*)&RtApp_Startup); ++n;
    RegisterRuntimeAddr(0x18009f510, (void*)&RtApp_Startup);

    // Wave 12 lane C.
    RegisterRuntimeExport("?EnableCursor@Application@1@QEAAX_N@Z", (void*)&RtApp_EnableCursor); ++n;
    RegisterRuntimeAddr(0x18009f800, (void*)&RtApp_EnableCursor);

    RegisterRuntimeExport("?LaunchProcess@Application@1@QEAA_NPEBD_N@Z", (void*)&RtApp_LaunchProcess); ++n;
    RegisterRuntimeAddr(0x18009f5e0, (void*)&RtApp_LaunchProcess);

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

    RegisterRuntimeExport("?PlatformShutdown@Application@1@AEAAXXZ",   // ord 3393
                          (void*)&RtApp_PlatformShutdown); ++n;
    RegisterRuntimeAddr(0x18009fd70, (void*)&RtApp_PlatformShutdown);

    RegisterRuntimeExport("?PlatformConstruct@Application@1@AEAAXPEAX@Z",  // ord 3331
                          (void*)&RtApp_PlatformConstruct); ++n;
    RegisterRuntimeAddr(0x18009fbe0, (void*)&RtApp_PlatformConstruct);

    RegisterRuntimeExport("??0Application@0@QEAA@AEBV00@@Z",           // ord 48
                          (void*)&RtApp_CopyCtor); ++n;
    RegisterRuntimeAddr(0x18000a0d0, (void*)&RtApp_CopyCtor);

    RegisterRuntimeExport("??4Application@0@QEAAAEAV00@AEBV00@@Z",     // ord 1050
                          (void*)&RtApp_Assign); ++n;
    RegisterRuntimeAddr(0x18000a160, (void*)&RtApp_Assign);

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
    RegisterRuntimeAddr(0x18000a530, (void*)&RtApp_SetCloneTree);
    RegisterRuntimeAddr(0x18000a440, (void*)&RtApp_SetClear);

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

    // Wave 9 lane VID2: the message-callback Set. These two are the recorded
    // blocker for ?PlatformStartup@Device@Video@@ (the next cutover boot wall
    // after the IO chain) AND two of the seventeen unsatisfied rows in
    // dx11/src/devicedata.cpp's all-or-nothing import table.
    extern int RtApplicationMessageCallbacksRegister();
    n += RtApplicationMessageCallbacksRegister();

    // Wave 9 lane PLAT: the PlatformStartup family and the WindowProc that one
    // of them installs. CROSS-NAMESPACE ON PURPOSE — Achievement, Object and
    // Engine each contribute one two-instruction body, and what the five share
    // is the export NAME and the cutover boot path, not a namespace (the shape
    // lane X's singleton_ctors.cpp established).
    //
    // `?PlatformStartup@Application@1@AEAA_NXZ` is the live cutover wall:
    // engine_lifecycle.cpp:638 calls it and a null there is EXEC-of-0.
    extern int RtApplicationPlatformStartupRegister();
    n += RtApplicationPlatformStartupRegister();

    printf("[RtPort] Application: %d exports registered\n", n);
}
