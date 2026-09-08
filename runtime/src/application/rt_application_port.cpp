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

    printf("[RtPort] Application: %d exports registered\n", n);
}
