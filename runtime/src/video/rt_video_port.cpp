// ============================================================================
// runtime/src/video/rt_video_port.cpp — RtVideoPortInit()  Wave 6 (Video)
// ============================================================================
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
extern int RtVideoImageBufferVtableRegister();  // imagebuffer_vtable.cpp (Wave 18 lane VDATA)
extern int RtVideoCompanionRegister(); // companion_data.cpp (Wave 9 lane B2)
extern int RtVideoDynamicResourcesRegister();  // dynamic_resources.cpp (Wave 9 lane VID)
extern int RtVideoContextLifecycleRegister();  // context_lifecycle.cpp (Wave 9 lane VID2)
extern "C" int  RtVideoShaderConstantsVerify();         // constant_buffer.cpp
extern int      RtVideoContextCtorRegister();             // context_ctor.cpp
extern int      RtVideoContextBindRegister();            // context_bind.cpp (lane CTX)
extern int      RtVideoDeviceEnumTypesRegister();          // device_enum_types.cpp (Wave 18 GFXIF)
extern int      RtVideoVideoProgramRegister();              // video_program.cpp (Wave 23 lane VIDVT)

// BATCH C registration entry point (batchc_*.cpp).
extern "C" int RtVideoBatchCSetLightRegister();

void RtVideoPortInit() {
    // BATCH C — Video::Context::SetLight. Unconditional, see DIRECTION.md §2.3.
    {
        printf("[RtPort] BATCH C: %d video leaf registered\n",
               RtVideoBatchCSetLightRegister());
    }
    int n = 0;
    n += RtVideoContextRegister();
    n += RtVideoDeviceRegister();
    n += RtVideoBuffersRegister();
    // Wave 18 lane VDATA: the seven abort-trap slots of
    // ??_7ImageBuffer@Video@@6B@. Its own switch rather than VIDBUFFERS' so the
    // vtable row's promotion can be A/B'd with the SAME BINARY one flag apart.
    n += RtVideoImageBufferVtableRegister();
    n += RtVideoCompanionRegister();
    n += RtVideoDynamicResourcesRegister();
    // Wave 9 lane VID2: the Device startup cluster. `?PlatformStartup@` is the
    // next cutover boot wall after the IO chain — Engine::Startup calls it at
    // engine_lifecycle.cpp:719 and a null there ends the boot with
    // g_startupFailAt == 11.
    extern int RtVideoDeviceStartupRegister();
    n += RtVideoDeviceStartupRegister();
    // Wave 9 lane VID2 phase 3: Video::ConstantBuffer, the first link in the
    // chain to CreateVideoContext. The literal self-check runs here because it
    // only reads the DLL's .rdata — no allocation, no ordering hazard.
    extern int RtVideoConstantBufferRegister();
    n += RtVideoConstantBufferRegister();
    RtVideoShaderConstantsVerify();
    // Wave 9 lane VID2 phase 4: the Context ctor and CreateVideoContext. Its
    // probe is armed here and RUN from the top of PlatformStartup for the same
    // allocator reason as the two above.
    n += RtVideoContextCtorRegister();
    // Wave 9 lane CTX: ?BindResources@ / ?BindTextures@ — the two Context
    // methods our OWN dx11/ backend calls back into, once per draw call. The
    // probe allocates nothing (three .bss Contexts plus fixed fakes) and makes
    // no call outside this file, so unlike the three above it can run right
    // here; it does not need PlatformStartup's allocator.
    n += RtVideoContextBindRegister();
    // WAVE 29 lane AVBODY: ?GetConstantName@ShaderProgram@Video@@ and
    // ?GetTextureName@ShaderProgram@Video@@ -- Video's ENTIRE remaining body
    // surface (re-derived: the other 18 unported Video rows are 12 NO-OP
    // vtables + 6 HARMFUL data slots, all tagged UNPROMOTABLE). Both return
    // System::String BY VALUE, i.e. through a hidden RDX slot.
    extern int RtVideoShaderProgramNamesRegister();
    n += RtVideoShaderProgramNamesRegister();
    n += RtVideoContextLifecycleRegister();
    // Wave 9 lane EVM: the plugin-loading half of the
    // EnumerateVideoModes chain. InitializeInterfaces is the body
    // standalone/video_interface.cpp has been asking for in writing since
    // lane ENGIO -- it is the only thing that fills the DX11 slot once the
    // detour cannot exist.
    // ONE switch for the whole lane, so the cutover-residue set-diff can be
    // taken with the SAME BINARY one flag apart rather than against a rebuild.
    extern int RtVideoInterfacesRegister();
    n += RtVideoInterfacesRegister();

    // Wave 9 lane EVM: the two introsort instantiations
    // EnumerateVideoModes runs over the enumerated tree, plus their two
    // small-range passes and the DisplayMode deep swap. ADDRESSES ONLY --
    // none of the five is exported in either build.
    extern int RtVideoModeSortRegister();
    const int sorts = RtVideoModeSortRegister();
    printf("[LaneEVM] mode/refresh sort addresses registered=%d\n", sorts);

    // Wave 9 lane EVM: attempt 22's wall itself. Registered AFTER the two
    // blocks above, because its body calls both of them.
    extern int RtVideoEnumerateModesRegister();
    n += RtVideoEnumerateModesRegister();

    // Wave 18 lane GFXIF: Video::Device::Adapter and ::GraphicsInterface, plus
    // AdjustDisplay / FindOrAddMode / GetDisplayMode. These nine names are the
    // ones VideoDX11's devicedata import table cannot fill with the shipped DLL
    // absent, which is the VCALL-of-0 the cutover boot gate has died on since
    // attempt 23. Registered LAST in this file because the Array<Adapter>
    // helpers it adds call the Array<DisplayMode> and Array<bool> bodies lane
    // VID2 registers above — the calls are by C++ symbol, not through the
    // registry, so the order is documentation rather than a dependency, but it
    // should stay this way so that reading the file in order reads correctly.
    n += RtVideoDeviceEnumTypesRegister();

    // Wave 23 lane VIDVT: ?SetupShaders@VideoProgram@Video@@, the single BODY
    // row in Video's nineteen-row gap. It has NO dependency on anything above
    // -- no calls at all, five compare-guarded field writes -- so its position
    // here is arbitrary and its own switch exists only so the row can be A/B'd
    // with the same binary one flag apart. Its ONLY caller in this tree is
    // Wave 21's ?Setup@MaterialInstance@Graphics@@, which reaches it by name
    // through ResolveExport.
    n += RtVideoVideoProgramRegister();
    // The VERIFICATION, because this row has no caller: an input sweep
    // against the shipped export. Opt-in because it calls into the DLL 128
    // times and is a check, not boot work. It allocates NOTHING (two .bss
    // Contexts and a stack VideoProgram), so it is safe this early --
    // unlike the enum-types and device-startup probes above, which is why
    // it runs here rather than being armed for later.

    // Wave 18 lane VRES: the Video resource lifecycle — RenderBuffer
    // ctor/dtor/Bind, ImageBuffer::Recreate, and the Configuration and Device
    // destructors. Registered LAST of the Video lanes because
    // ??1Device@Video@@'s body reaches the two exported DATA singletons through
    // RT_DATA_SLOT and destroys three Arrays through lane D2's bodies; nothing
    // earlier in this init depends on it, and nothing it needs is registered
    // here (Collection's inits run after all of Video's, and this body resolves
    // through function-local statics on first USE, not at init).
    // 2026-09-12: Video::VideoProgram, the whole class (videoprogram.cpp). No switch.
    { extern int RtVideoProgramRegister(); n += RtVideoProgramRegister(); }
    { extern int RtVideoVertexLayoutRegister(); n += RtVideoVertexLayoutRegister(); }
    // 2026-09-12: the Video shader family, whole classes, no switch. ORDER
    // MATTERS ONLY FOR READING: each register call is independent, but
    // ShaderProgram is listed first because VertexProgram and PixelProgram
    // reuse its ctor/dtor/Serialize bodies at link time.
    { extern int RtVideoShaderProgramRegister(); n += RtVideoShaderProgramRegister(); }
    { extern int RtVideoVertexProgramRegister(); n += RtVideoVertexProgramRegister(); }
    { extern int RtVideoPixelProgramRegister();  n += RtVideoPixelProgramRegister(); }
    { extern int RtVideoRenderStateRegister();   n += RtVideoRenderStateRegister(); }

    extern int RtVideoResourcesRegister();
    n += RtVideoResourcesRegister();
    // ARMED here, RUN from the first ?Bind@RenderBuffer@ — it allocates
    // ~40 blocks and at THIS point in boot the MemoryAllocator singleton is
    // constructed but not started. `=neg` additionally runs the four
    // negative controls. See video_resources_probe.cpp.
    // Wave 18 lane VRES row 7. Separate from the six above because it is
    // the only row that depends on unported IO exports, so an A/B that
    // needs the rest of the lane on and this one off has a switch for it.
    extern int RtVideoSaveImageRegister();
    n += RtVideoSaveImageRegister();
    // Wave 18 lane VRES tranche 2. Its own switch for the same reason: it
    // used to be the only row that reached an unported Collection helper
    // (0x180053f00), so an A/B that needs the rest of the lane on and this
    // one off has a switch for it.
    // *** 2026-09-12: 0x180053f00 IS NOW PORTED *** —
    // runtime/src/collection/array.cpp `kSpec_InnerArrayU8`, landed with
    // Video::ShaderProgram, whose PlatformSerialize is its other caller.
    // The switch is kept (it still isolates this row's OWN body) but the
    // reason quoted above no longer applies.
    extern int RtVideoImageBufferSerializeRegister();
    n += RtVideoImageBufferSerializeRegister();

    printf("[RtPort] Video: %d exports registered\n", n);
}
