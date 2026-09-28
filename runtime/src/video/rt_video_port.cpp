// ============================================================================
// runtime/src/video/rt_video_port.cpp — RtVideoPortInit()  Wave 6 (Video)
// ============================================================================
// Each file that still carries a registration exposes one Register function
// returning the number of RegisterRuntime* calls it made. B3R RT-VG: a file
// whose every row went DEAD lost its Register function and its call here.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"   // ENDGAME B.3.9 VID: the storage class statics

extern int RtVideoBuffersRegister();   // buffers.cpp  (lane C)
extern int RtVideoCompanionRegister(); // companion_data.cpp (Wave 9 lane B2)
extern int RtVideoContextLifecycleRegister();  // context_lifecycle.cpp (Wave 9 lane VID2)
extern int      RtVideoContextCtorRegister();             // context_ctor.cpp
extern int      RtVideoDeviceEnumTypesRegister();          // device_enum_types.cpp (Wave 18 GFXIF)

void RtVideoPortInit() {
    // ENDGAME B.3.9 close: `?_singleton@Device@Video@@` is RETIRED (close.txt);
    // every site reads the class static Video::Device::_singleton.
    int n = 0;
    n += RtVideoBuffersRegister();
    n += RtVideoCompanionRegister();
    // Wave 9 lane VID2: the Device startup cluster. `?PlatformStartup@` is the
    // next cutover boot wall after the IO chain — Engine::Startup calls it at
    // engine_lifecycle.cpp:719 and a null there ends the boot with
    // g_startupFailAt == 11.
    extern int RtVideoDeviceStartupRegister();
    n += RtVideoDeviceStartupRegister();
    // Wave 9 lane VID2 phase 4: the Context ctor and CreateVideoContext. Its
    // probe is armed here and RUN from the top of PlatformStartup for the same
    // allocator reason as the one above.
    n += RtVideoContextCtorRegister();
    n += RtVideoContextLifecycleRegister();
    // Wave 9 lane EVM: the plugin-loading half of the
    // EnumerateVideoModes chain. InitializeInterfaces is the body
    // standalone/video_interface.cpp has been asking for in writing since
    // lane ENGIO -- it is the only thing that fills the DX11 slot once the
    // detour cannot exist.
    extern int RtVideoInterfacesRegister();
    n += RtVideoInterfacesRegister();

    // Wave 18 lane GFXIF: Video::Device::Adapter and ::GraphicsInterface, plus
    // AdjustDisplay / FindOrAddMode / GetDisplayMode. These nine names are the
    // ones VideoDX11's devicedata import table cannot fill with the shipped DLL
    // absent, which is the VCALL-of-0 the cutover boot gate has died on since
    // attempt 23.
    n += RtVideoDeviceEnumTypesRegister();

    // Wave 18 lane VRES: the Video resource lifecycle — RenderBuffer
    // ctor/dtor/Bind, ImageBuffer::Recreate, and the Configuration and Device
    // destructors. Nothing earlier in this init depends on it, and nothing it
    // needs is registered here (Collection's inits run after all of Video's,
    // and this body resolves through function-local statics on first USE, not
    // at init).
    extern int RtVideoResourcesRegister();
    n += RtVideoResourcesRegister();
    // Wave 18 lane VRES row 7.
    extern int RtVideoSaveImageRegister();
    n += RtVideoSaveImageRegister();
    // Wave 18 lane VRES tranche 2.
    extern int RtVideoImageBufferSerializeRegister();
    n += RtVideoImageBufferSerializeRegister();

    printf("[RtPort] Video: %d exports registered\n", n);
}
