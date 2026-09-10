// ============================================================================
// runtime/src/video/rt_video_port.cpp — RtVideoPortInit()  Wave 6 (Video)
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   BANISHED_RT_VIDEO=0      leave the whole Video namespace on the DLL
//   BANISHED_RT_VIDCONTEXT=0 ... just Video::Context            (lane A)
//   BANISHED_RT_VIDDEVICE=0  ... just Video::Device/Configuration/RenderBuffer (lane B)
//   BANISHED_RT_VIDBUFFERS=0 ... just Video::ImageBuffer/Index/VertexBuffer (lane C)
//   BANISHED_RT_VIDCOMPANION=0 ... just the twelve Video::*Data companion bases,
//                            the buffer Lock/Unlock/Bind trampolines and
//                            Device::DisplayMode          (Wave 9 lane B2)
//   BANISHED_RT_VIDDYNAMIC=0 ... just Device::{Lock,Unlock,Release/RecreateDynamic},
//                            the eight class-static list walkers and
//                            RenderBuffer::Recreate       (Wave 9 lane VID)
//   BANISHED_RT_VIDCTXBIND=0 ... just Context::BindResources/BindTextures, the
//                            two Context methods dx11/ calls once per draw
//                                                        (Wave 9 lane CTX)
//   BANISHED_RT_CTXBIND_PROBE=1 runs lane CTX's 37-case oracle at init.
//
// Each lane owns exactly one .cpp and exposes one Register function returning
// the number of RegisterRuntime* calls it made. A lane with nothing landed yet
// returns 0 from a weak stub below.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"

extern int RtVideoContextRegister();   // context.cpp  (lane A)
extern int RtVideoDeviceRegister();    // device.cpp   (lane B)
extern int RtVideoBuffersRegister();   // buffers.cpp  (lane C)
extern int RtVideoCompanionRegister(); // companion_data.cpp (Wave 9 lane B2)
extern int RtVideoDynamicResourcesRegister();  // dynamic_resources.cpp (Wave 9 lane VID)
extern int RtVideoContextLifecycleRegister();  // context_lifecycle.cpp (Wave 9 lane VID2)
extern "C" void RtVideoContextLifecycleProbe();
extern "C" void RtVideoDeviceStartupProbeArm(bool on);  // device_startup_probe.cpp
extern "C" int  RtVideoShaderConstantsVerify();         // constant_buffer.cpp
extern "C" void RtVideoAssignConstantsProbeArm(bool on);  // constant_buffer.cpp
extern int      RtVideoContextCtorRegister();             // context_ctor.cpp
extern "C" void RtVideoContextCtorProbeArm(bool on);     // context_ctor.cpp
extern int      RtVideoContextBindRegister();            // context_bind.cpp (lane CTX)
extern "C" void RtVideoContextBindProbe();               // context_bind.cpp (lane CTX)
extern int      RtVideoDeviceEnumTypesRegister();          // device_enum_types.cpp (Wave 18 GFXIF)
extern "C" void RtVideoDeviceEnumTypesProbeArm(bool on);   // device_enum_types_probe.cpp

void RtVideoPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_VIDEO")) {
        printf("[RtPort] BANISHED_RT_VIDEO=0 -- Video namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_VIDCONTEXT")) n += RtVideoContextRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDDEVICE"))  n += RtVideoDeviceRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDBUFFERS")) n += RtVideoBuffersRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDCOMPANION")) n += RtVideoCompanionRegister();
    if (RtWaveEnabled("BANISHED_RT_VIDDYNAMIC"))   n += RtVideoDynamicResourcesRegister();
    // Wave 9 lane VID2. The probe runs only when BANISHED_RT_CTXCLEAR_PROBE is
    // set: it makes 513 calls into the shipped ?ClearResources@ and is a
    // verification step, not boot work. It allocates nothing (two .bss
    // Contexts), so it is safe at this point in boot, but it is still opt-in.
    // Wave 9 lane VID2: the Device startup cluster. `?PlatformStartup@` is the
    // next cutover boot wall after the IO chain — Engine::Startup calls it at
    // engine_lifecycle.cpp:719 and a null there ends the boot with
    // g_startupFailAt == 11.
    if (RtWaveEnabled("BANISHED_RT_VIDDEVSTART")) {
        extern int RtVideoDeviceStartupRegister();
        n += RtVideoDeviceStartupRegister();
        // Armed, not run — it allocates. It self-schedules at the top of
        // PlatformStartup, which is the first moment the allocator is live.
        const char* dp = std::getenv("BANISHED_RT_VIDDEVSTART_PROBE");
        RtVideoDeviceStartupProbeArm(dp && *dp && std::strcmp(dp, "0") != 0);
    }
    // Wave 9 lane VID2 phase 3: Video::ConstantBuffer, the first link in the
    // chain to CreateVideoContext. The literal self-check runs here because it
    // only reads the DLL's .rdata — no allocation, no ordering hazard.
    if (RtWaveEnabled("BANISHED_RT_VIDCONSTBUF")) {
        extern int RtVideoConstantBufferRegister();
        n += RtVideoConstantBufferRegister();
        RtVideoShaderConstantsVerify();
        // The AssignConstants oracle: ARMED here, RUN from the top of
        // Video::Device::PlatformStartup. It drives the SHIPPED body once and
        // ours once over two .bss Contexts, which allocates ~124 Strings and 14
        // arrays (all released again) — and at THIS point in boot the
        // MemoryAllocator singleton is constructed but not started, so running
        // it here faults inside the DLL's MemAlloc. Measured, not assumed.
        const char* ap = std::getenv("BANISHED_RT_ASSIGNCONST_PROBE");
        RtVideoAssignConstantsProbeArm(ap && *ap && std::strcmp(ap, "0") != 0);
    }
    // Wave 9 lane VID2 phase 4: the Context ctor and CreateVideoContext. Its
    // probe is armed here and RUN from the top of PlatformStartup for the same
    // allocator reason as the two above.
    if (RtWaveEnabled("BANISHED_RT_VIDCTXCTOR")) {
        n += RtVideoContextCtorRegister();
        const char* cp = std::getenv("BANISHED_RT_CTXCTOR_PROBE");
        RtVideoContextCtorProbeArm(cp && *cp && std::strcmp(cp, "0") != 0);
    }
    // Wave 9 lane CTX: ?BindResources@ / ?BindTextures@ — the two Context
    // methods our OWN dx11/ backend calls back into, once per draw call. The
    // probe allocates nothing (three .bss Contexts plus fixed fakes) and makes
    // no call outside this file, so unlike the three above it can run right
    // here; it does not need PlatformStartup's allocator.
    if (RtWaveEnabled("BANISHED_RT_VIDCTXBIND")) {

        n += RtVideoContextBindRegister();
        const char* bp = std::getenv("BANISHED_RT_CTXBIND_PROBE");
        if (bp && *bp && std::strcmp(bp, "0") != 0)
            RtVideoContextBindProbe();
    }
    if (RtWaveEnabled("BANISHED_RT_VIDCTXLIFE")) {
        n += RtVideoContextLifecycleRegister();
        const char* probe = std::getenv("BANISHED_RT_CTXCLEAR_PROBE");
        if (probe && *probe && std::strcmp(probe, "0") != 0)
            RtVideoContextLifecycleProbe();
    }
    // Wave 9 lane EVM: the plugin-loading half of the
    // EnumerateVideoModes chain. InitializeInterfaces is the body
    // standalone/video_interface.cpp has been asking for in writing since
    // lane ENGIO -- it is the only thing that fills the DX11 slot once the
    // detour cannot exist.
    // ONE switch for the whole lane, so the cutover-residue set-diff can be
    // taken with the SAME BINARY one flag apart rather than against a rebuild.
    if (RtWaveEnabled("BANISHED_RT_EVM")) {
        extern int RtVideoInterfacesRegister();
        n += RtVideoInterfacesRegister();
    }

    // Wave 9 lane EVM: the two introsort instantiations
    // EnumerateVideoModes runs over the enumerated tree, plus their two
    // small-range passes and the DisplayMode deep swap. ADDRESSES ONLY --
    // none of the five is exported in either build.
    if (RtWaveEnabled("BANISHED_RT_EVM")) {
        extern int RtVideoModeSortRegister();
        const int sorts = RtVideoModeSortRegister();
        printf("[LaneEVM] mode/refresh sort addresses registered=%d\n", sorts);
    }

    // Wave 9 lane EVM: attempt 22's wall itself. Registered AFTER the two
    // blocks above, because its body calls both of them.
    if (RtWaveEnabled("BANISHED_RT_EVM")) {
        extern int RtVideoEnumerateModesRegister();
        n += RtVideoEnumerateModesRegister();
    }

    // Wave 18 lane GFXIF: Video::Device::Adapter and ::GraphicsInterface, plus
    // AdjustDisplay / FindOrAddMode / GetDisplayMode. These nine names are the
    // ones VideoDX11's devicedata import table cannot fill with the shipped DLL
    // absent, which is the VCALL-of-0 the cutover boot gate has died on since
    // attempt 23. Registered LAST in this file because the Array<Adapter>
    // helpers it adds call the Array<DisplayMode> and Array<bool> bodies lane
    // VID2 registers above — the calls are by C++ symbol, not through the
    // registry, so the order is documentation rather than a dependency, but it
    // should stay this way so that reading the file in order reads correctly.
    if (RtWaveEnabled("BANISHED_RT_VIDENUMTYPES")) {
        n += RtVideoDeviceEnumTypesRegister();
        // ARMED here, RUN from the top of Video::Device::PlatformStartup: the
        // probe allocates, and the MemoryAllocator singleton is not started yet
        // at this point in boot.
        const char* ep = std::getenv("BANISHED_RT_VIDENUMTYPES_PROBE");
        RtVideoDeviceEnumTypesProbeArm(ep && *ep && std::strcmp(ep, "0") != 0);
    }

    printf("[RtPort] Video: %d exports registered\n", n);
}
