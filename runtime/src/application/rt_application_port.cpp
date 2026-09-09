// ============================================================================
// runtime/src/application/rt_application_port.cpp — RtApplicationPortInit()
//                                                    Wave 8 lane E
// ============================================================================
//   BANISHED_RT_APP=0   leave the whole Application namespace on the DLL
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "../../types/application/Application.h"

using RtApplication::Application;

// application.cpp (no Win32)
bool RtApp_IsPaused(const Application* self);
void RtApp_Quit(Application* self);

// application_win.cpp (Win32 platform seam)
bool RtApp_PlatformUpdate(Application* self);
void RtApp_SleepThread(Application* self, int ms);
void RtApp_YieldToThreads(Application* self);

// application_accessors.cpp (no Win32) -- Wave 12 lane A
Application* RtApp_Get();
void* RtApp_GetInstance(const Application* self);
void* RtApp_GetWindow(const Application* self);
bool RtApp_IsCursorEnabled(Application* self);
bool RtApp_PlatformIsPaused(const Application* self);
void RtApp_PlatformDestruct(Application* self);

// application_thunks.cpp -- Wave 12 lane A
bool RtApp_Update(Application* self);
void RtApp_Shutdown(Application* self);
bool RtApp_Startup(Application* self);

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
