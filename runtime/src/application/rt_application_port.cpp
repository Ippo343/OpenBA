// ============================================================================
// runtime/src/application/rt_application_port.cpp — RtApplicationPortInit()
//                                                    Wave 8 lane E
// ============================================================================
//   BANISHED_RT_APP=0   leave the whole Application namespace on the DLL
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"
#include "../collection/set_avl.h"

using RtApplication::Application;

// application.cpp (no Win32)
bool RtApp_IsPaused(const Application* self);
void RtApp_Quit(Application* self);

// application_win.cpp (Win32 platform seam)
bool RtApp_PlatformUpdate(Application* self);
void RtApp_SleepThread(Application* self, int ms);
void RtApp_YieldToThreads(Application* self);
void RtApp_EnableCursor(Application* self, bool enabled);
bool RtApp_LaunchProcess(Application* self, const char* cmd, bool wait);

// application_accessors.cpp (no Win32) -- Wave 12 lane A
Application* RtApp_Get();
void* RtApp_GetInstance(const Application* self);
void* RtApp_GetWindow(const Application* self);
uint8_t RtApp_IsCursorEnabled(Application* self);   // RAW BYTE, not a normalised bool -- see its banner
bool RtApp_PlatformIsPaused(const Application* self);
void RtApp_PlatformDestruct(Application* self);

// application_thunks.cpp -- Wave 12 lane A
bool RtApp_Update(Application* self);
void RtApp_Shutdown(Application* self);
bool RtApp_Startup(Application* self);

extern "C" void RtApp_PlatformConstruct(Application* self, void* platformHandle);

// platform_startup_win.cpp (Win32 platform seam; lives beside PlatformStartup
// because the two share the `kClassName` literal — see its banner)
extern "C" void RtApp_PlatformShutdown(Application* self);

// application_lifecycle.cpp (no Win32) — Wave 12 lane B
extern "C" void          RtApp_Dtor(Application* self);
extern "C" Application*  RtApp_CopyCtor(Application* self, const Application* other);
extern "C" Application*  RtApp_Assign(Application* self, const Application* other);
extern "C" void          RtApp_SetDestroyTree(void* self, rtsetavl::Node* n);
extern "C" rtsetavl::Node* RtApp_SetCloneTree(void* self, const rtsetavl::Node* src);
extern "C" void          RtApp_SetClear(rtsetavl::Set* set);

// application_lifecycle_probe.cpp — Wave 12 lane B's oracle
extern "C" void RtApp_LifecycleProbeArm(bool on);

void RtApplicationPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_APP")) {
        printf("[RtPort] BANISHED_RT_APP=0 -- Application namespace port disabled\n");
        return;
    }
    int n = 0;

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

    // Lane B's oracle. Opt-in, and it ALLOCATES, so it cannot run here — it is
    // self-scheduled from the first Application::PlatformUpdate, by which point
    // System::MemoryAllocator has been started. See its banner.
    {
        const char* p = std::getenv("BANISHED_RT_APPB_PROBE");
        RtApp_LifecycleProbeArm(p && *p && std::strcmp(p, "0") != 0);
    }

    // ?_singleton@Application@1@0PEAV11@EA @ 0x1801fc678 — a DATA export, not
    // a function; the seam maps names to functions, so it cannot be
    // redirected before cutover (same reasoning as Save::SaveLoad::_singleton
    // and Object::InternalFactory::_singleton in earlier waves). It is
    // already wired: standalone/address_registry.cpp registers exe dispatch
    // slot 0x140152368 to ResolveAddr(Module::Runtime, 0x1801fc678), which is
    // this exact address, read directly and confirmed against the ctor's own
    // `MOV qword ptr [0x1801fc678],RCX` store — no change needed here.

    // Wave 9 lane VID2: the message-callback Set. These two are the recorded
    // blocker for ?PlatformStartup@Device@Video@@ (the next cutover boot wall
    // after the IO chain) AND two of the seventeen unsatisfied rows in
    // dx11/src/devicedata.cpp's all-or-nothing import table.
    if (RtWaveEnabled("BANISHED_RT_APPMSGCB")) {
        extern int RtApplicationMessageCallbacksRegister();
        n += RtApplicationMessageCallbacksRegister();
    }

    // Wave 9 lane PLAT: the PlatformStartup family and the WindowProc that one
    // of them installs. CROSS-NAMESPACE ON PURPOSE — Achievement, Object and
    // Engine each contribute one two-instruction body, and what the five share
    // is the export NAME and the cutover boot path, not a namespace (the shape
    // lane X's singleton_ctors.cpp established).
    //
    // `?PlatformStartup@Application@1@AEAA_NXZ` is the live cutover wall:
    // engine_lifecycle.cpp:638 calls it and a null there is EXEC-of-0.
    if (RtWaveEnabled("BANISHED_RT_PLAT")) {
        extern int RtApplicationPlatformStartupRegister();
        n += RtApplicationPlatformStartupRegister();
    }

    printf("[RtPort] Application: %d exports registered\n", n);
}
