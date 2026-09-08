// ============================================================================
// runtime/src/video/device.cpp — Wave 6 lane B
//   Video::Device + Video::Configuration + Video::RenderBuffer +
//   Video::RenderStateData
// ============================================================================
//
// Ports 9 of the 15 FUNCTIONS this lane owns (the 17th and 16th rows of
// docs/runtime_port/wave6_lanes.txt are DATA symbols, see below). Layouts and
// the evidence for every offset are in runtime/types/video/Device.h; the
// per-function write-up, the oracle arguments and the six hold-backs are in
// docs/runtime_port/WAVE6_LANE_B.md.
//
//   | Export                                    | Retail      | Ord  | Oracle              |
//   |-------------------------------------------|-------------|------|---------------------|
//   | ?GetInterfaceCount@Device@Video@@...      | 0x18000d3b0 | 2729 | FULL scalar         |
//   | ?GetInterface@Device@Video@@...           | 0x18000d3c0 | 2728 | FULL scalar (ptr)   |
//   | ?GetMultisampleModeCount@Device@Video@@...| 0x18000d450 | 2803 | FULL scalar         |
//   | ?IsMultisampleModeSupported@Device@...    | 0x18000d460 | 3145 | FULL scalar         |
//   | ?ValidateConfiguration@Device@Video@@...  | 0x1800f7370 | 4532 | FULL (scratch args) |
//   | ?ApplyChangedSettings@Device@Video@@...   | 0x1800f8110 | 1604 | FULL (scratch this) |
//   | ?Create@RenderBuffer@Video@@...           | 0x1800f3f10 | 1882 | FULL (scratch this) |
//   | ??1RenderStateData@Video@@UEAA@XZ         | 0x1800f8d70 |  662 | FULL (scratch this) |
//   | ?Save@Configuration@Video@@QEAAXXZ        | 0x1800f6d10 | 3605 | poison + readback   |
//
// HELD BACK (reasons in full in WAVE6_LANE_B.md §2):
//   ?Startup@Device@Video@@QEAA_NXZ            0x1800f7270  ord 4091
//   ?Shutdown@Device@Video@@QEAAXXZ            0x1800f7280  ord 4044
//   ?RecreateDynamic@Device@Video@@QEAAXXZ     0x1800f80b0  ord 3522
//   ?ReleaseDynamic@Device@Video@@QEAAXXZ      0x1800f80e0  ord 3536
//   ??0RenderBuffer@Video@@QEAA@XZ             0x1800f3ee0  ord  346
//   ??1RenderBuffer@Video@@QEAA@XZ             0x1800f4020  ord  659
//
// NOT FUNCTIONS, so not registrable before cutover (PLAN §2.3 / the Wave 4
// FileSystem::_singleton precedent):
//   ?_singleton@Configuration@Video@@0PEAV12@EA  0x1801fe7a0  ord 4804
//   ?_singleton@Device@Video@@0PEAV12@EA         0x1801fe7a8  ord 4807
// They are DATA exports. `RegisterRuntimeExport` installs a function pointer
// that ResolveExport hands back to a caller which then TAKES THE ADDRESS of the
// variable — registering one would hand out a pointer to our code as if it were
// the Device slot. Both stay on the DLL and both are counted as UNPORTED.
//
// All 17 mangled names were checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was written
// (reference_resolveexport_phantom_names.md: a name the DLL does not contain
// resolves to NULL in silence, the port still registers, and the census still
// counts it). The retail addresses and ordinals above are that query's output.
//
// WHAT ELSE STAYS IN THE DLL, AND WHY
// -----------------------------------
// `Video::Device` is a singleton our own code already reaches into by hand:
// standalone/video_interface.cpp DETOURS Video::Device::InitializeInterfaces
// and writes the DX11 plugin slot directly at Device+0xb0, and dx11/ reads
// Device+0x100. Every one of those offsets is asserted in
// runtime/types/video/Device.h, so this port and those writers agree by
// construction. None of the nine bodies below allocates, frees, stamps a
// vtable, or changes any layout — they read the Device and write only fields
// retail writes — so the "two implementations on one live object" hazard of
// WAVE6.md's opening section does not arise for this lane.
//
// PLATFORM. Nothing here includes <windows.h>. The Win32 surfaces of this
// class — the Device critical section at +0x60, LoadLibraryW in
// InitializeInterfaces, the Application message callbacks in Startup — all sit
// inside functions this lane holds back, so no `device_win.cpp` twin was needed
// (PLAN §4.1 decision 1).
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/video/Device.h"
#include "../../types/video/ImageBuffer.h"

using RtVideo::Adapter;
using RtVideo::Configuration;
using RtVideo::Device;
using RtVideo::DisplayMode;
using RtVideo::GraphicsInterface;
using RtVideo::RefreshRate;
using RtVideo::RenderBuffer;

// ----------------------------------------------------------------------------
// ResolveExport, forward-declared rather than pulled in from standalone/resolve.h
//
// Same reasoning rt_port.h gives for declaring RegisterRuntimeExport /
// ResolveExportRaw locally, and the same shape runtime/src/io/persistence.cpp
// uses for ResolveAddr: resolve.h drags in <windows.h>, and PLAN §4.1 decision 1
// requires runtime/src/ to stay platform-neutral outside a `_win` twin. This
// declaration must stay byte-identical to resolve.h's; the two are checked
// against each other by the linker.
//
// Note this is the REGISTRY-AWARE resolver, not ResolveExportRaw. That is
// deliberate for IO::Persistence::Set below — see Video_Configuration_Save.
// ----------------------------------------------------------------------------
void* ResolveExport(const char* mangled_name);

namespace {

// ---- Mangled names, spelled once ------------------------------------------
// These are used by RtOriginal inside diff guards. The REGISTRATION site at the
// bottom of the file spells every name as a literal a second time, on purpose:
// tools/runtime_census.py matches `RegisterRuntimeExport("<literal>"` and
// cannot see through a named constant
// (reference_census_cannot_see_nonliteral_registrations.md).
constexpr const char* kGetInterfaceCount        = "?GetInterfaceCount@Device@Video@@QEBAHXZ";
constexpr const char* kGetInterface             = "?GetInterface@Device@Video@@QEBAAEBUGraphicsInterface@12@H@Z";
constexpr const char* kGetMultisampleModeCount  = "?GetMultisampleModeCount@Device@Video@@QEBAHXZ";
constexpr const char* kIsMultisampleSupported   = "?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z";
constexpr const char* kValidateConfiguration    = "?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z";
constexpr const char* kApplyChangedSettings     = "?ApplyChangedSettings@Device@Video@@QEAA_NXZ";
constexpr const char* kRenderBufferCreate       = "?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z";
constexpr const char* kRenderStateDataDtor      = "??1RenderStateData@Video@@UEAA@XZ";
constexpr const char* kConfigurationSave        = "?Save@Configuration@Video@@QEAAXXZ";
// NOT ported by this lane, only CALLED — see the banner above
// ValidateConfigurationBody.
constexpr const char* kGetDefaultResolution     = "?GetDefaultResolution@Device@Video@@QEBAHAEBUAdapter@12@@Z";

// ---- The three singletons these bodies read --------------------------------
// Resolved through ResolveExportRaw (the DLL's export table) because they are
// DATA symbols: the registry holds function redirections only, and nothing has
// registered — or can register — a data slot before cutover.
Device** DeviceSingletonSlot() {
    static Device** p = reinterpret_cast<Device**>(
        ResolveExportRaw("?_singleton@Device@Video@@0PEAV12@EA"));
    return p;
}
Configuration** ConfigSingletonSlot() {
    static Configuration** p = reinterpret_cast<Configuration**>(
        ResolveExportRaw("?_singleton@Configuration@Video@@0PEAV12@EA"));
    return p;
}
void** PersistenceSingletonSlot() {
    static void** p = reinterpret_cast<void**>(
        ResolveExportRaw("?_singleton@Persistence@IO@@0PEAV12@EA"));
    return p;
}

}  // namespace

// ============================================================================
// Video::Device::GetInterfaceCount  —  retail 0x18000d3b0, ordinal 2729
//
//   1800d3b0  MOV EAX, dword ptr [RCX + 0x48]
//   1800d3b4  RET
// ============================================================================
int Video_Device_GetInterfaceCount(const Device* self) {
    RT_DIFF_BEGIN("Video::Device::GetInterfaceCount");
    const int ours = self->interfaceCount;
    using Fn = int (*)(const Device*);
    RT_DIFF_SCALAR(ours, RtOriginal<Fn>(kGetInterfaceCount)(self));
    return ours;
}

// ============================================================================
// Video::Device::GetInterface  —  retail 0x18000d3c0, ordinal 2728
//
// Returns a reference, i.e. an interior pointer into the Array's storage. No
// bounds check: retail indexes straight off Array::_data, and
// code/Game/Dialog/optionsdialog.cpp relies on that (it feeds the result of
// GetInterfaceCount straight back in). A guard here would not be faithful and
// would not help — see CLAUDE.md "Don't 'fix' a faithful crash with a null
// check".
// ============================================================================
const GraphicsInterface* Video_Device_GetInterface(const Device* self, int index) {
    RT_DIFF_BEGIN("Video::Device::GetInterface");
    const GraphicsInterface* ours = self->interfaceData + index;
    using Fn = const GraphicsInterface* (*)(const Device*, int);
    const void* oursAsPtr = ours;
    RT_DIFF_SCALAR(oursAsPtr,
                   static_cast<const void*>(RtOriginal<Fn>(kGetInterface)(self, index)));
    return ours;
}

// ============================================================================
// Video::Device::GetMultisampleModeCount  —  retail 0x18000d450, ordinal 2803
// ============================================================================
int Video_Device_GetMultisampleModeCount(const Device* self) {
    RT_DIFF_BEGIN("Video::Device::GetMultisampleModeCount");
    const int ours = self->multisampleCount;
    using Fn = int (*)(const Device*);
    RT_DIFF_SCALAR(ours, RtOriginal<Fn>(kGetMultisampleModeCount)(self));
    return ours;
}

// ============================================================================
// Video::Device::IsMultisampleModeSupported  —  retail 0x18000d460, ordinal 3145
//
//   return (bool)((char*)self->multisampleData)[mode];
//
// Retail loads ONE BYTE and returns it as bool, so a stored value other than
// 0/1 would propagate. Reproduced with uint8_t + `!= 0` — MSVC's `bool` return
// is the low byte either way, and every producer of this array stores 0 or 1.
// ============================================================================
bool Video_Device_IsMultisampleModeSupported(const Device* self, int mode) {
    RT_DIFF_BEGIN("Video::Device::IsMultisampleModeSupported");
    const bool ours = self->multisampleData[mode] != 0;
    using Fn = bool (*)(const Device*, int);
    RT_DIFF_SCALAR(ours, RtOriginal<Fn>(kIsMultisampleSupported)(self, mode));
    return ours;
}

// ============================================================================
// Video::Device::ValidateConfiguration  —  retail 0x1800f7370, ordinal 4532
//
// Clamps a renderer/adapter/resolution/refresh/antialias/filter combination to
// something the hardware actually offers. Reads only `this`; writes only
// through the eight int& out-parameters, which is what makes it fully diffable.
//
// TWO RETAIL ODDITIES, both reproduced:
//
//  1. The stack local at 0x1800f7417 (`FUN_180010760(&local_28)`) is an
//     inlined Collection::Array destructor on a local that was zeroed at
//     0x1800f73f6 and never filled. FUN_180010760 begins `if (*p == 0) goto
//     clear`, so on a zero pair it frees nothing. It is a dead
//     scope-exit call from an inlined helper, not part of the algorithm, and it
//     is omitted here rather than emulated: emulating it would mean calling the
//     DLL allocator to free a null pointer.
//
//  2. The first refresh-rate loop compares the match index against -1 (
//     `if (iVar3 != -1)`), but the index it compares starts at 0 and only ever
//     increments, so the test is always true. That is a compiler artifact of a
//     `matchIndex != -1` guard written against a variable the optimiser proved
//     non-negative. The consequence is real and is kept: an EXACT refresh match
//     leaves *refreshNum / *refreshDen ALONE and skips the ratio fallback
//     entirely.
//
// CALLER NOTE: code/Game/Dialog/optionsdialog.cpp:315 passes `&savedAdapter`
// as BOTH the 2nd and the 8th argument (retail does the same at exe
// 0x140072eb0). The diff harness below therefore rebuilds the caller's aliasing
// exactly rather than assuming eight distinct slots — with eight distinct
// scratch ints, the original would see a different world from ours and report
// a divergence that is an artifact of the harness.
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
// Video::Device::GetDefaultResolution — retail 0x1800f7540, ordinal 2559.
//
// ValidateConfiguration falls back to it when the requested resolution is not
// in the adapter's list. It is EXPORTED and public, so it is reachable by name;
// it is simply not one of the 17 functions this lane owns, and nothing in our
// tree calls it directly. Called at the DLL exactly as retail's own body does,
// the shape Wave 5 lane D used for Picking::Scene's internals and Wave 4 lane C
// used for Map_Find / Map_Insert.
//
// WAVE 9 LANE V2, 2026-09-07 — the door was wrong and the stated reason for it
// does not survive being checked. It read "RtOriginal (not ResolveExport) on
// purpose: this is the DLL's implementation, unaffected by whether some later
// wave registers a port of it, which keeps this lane's oracle comparing like
// with like." But this helper has exactly ONE caller
// (ValidateConfigurationBody's resolution fallback), and that caller is the
// LIVE ported body, not the oracle — so it is a REAL DEPENDENCY, and pinning it
// to the DLL was pinning the live path, not an oracle. If a later wave ports
// GetDefaultResolution, the live path SHOULD pick that up; the diff harness
// further down still calls retail's ValidateConfiguration whole, which is where
// like-for-like actually comes from.
//
// The concrete hazard is cutover: ResolveExportRaw returns NULL with
// Runtime-x64.dll absent, so raw here is a null call on the fallback path.
// GetDefaultResolution is unported today, so nothing about which body runs
// changes with this edit.
// ----------------------------------------------------------------------------
int CallGetDefaultResolution(const Device* self, const Adapter* adapter) {
    using Fn = int (*)(const Device*, const Adapter*);
    return RT_EXPORT_ONCE(Fn, kGetDefaultResolution)(self, adapter);
}

void ValidateConfigurationBody(const Device* self,
                               int* renderer, int* adapterIdx,
                               int* width, int* height,
                               int* refreshNum, int* refreshDen,
                               int* antialias, int* filter) {
    if (*renderer < 0 || self->interfaceCount <= *renderer)
        *renderer = 0;
    const GraphicsInterface* iface = self->interfaceData + *renderer;

    if (*adapterIdx < 0 || iface->adapterCount <= *adapterIdx)
        *adapterIdx = 0;
    const Adapter* adapter = iface->adapterData + *adapterIdx;

    // Find the display mode matching the requested width/height.
    int modeIndex = -1;
    for (int i = 0; i < adapter->modeCount; ++i) {
        const DisplayMode* m = adapter->modeData + i;
        if (*width == m->width && *height == m->height) { modeIndex = i; break; }
    }
    if (modeIndex == -1) {
        modeIndex = CallGetDefaultResolution(self, adapter);
        *width  = adapter->modeData[modeIndex].width;
        *height = adapter->modeData[modeIndex].height;
    }

    const DisplayMode* mode = adapter->modeData + modeIndex;

    // Exact refresh match wins outright and leaves the pair untouched
    // (see oddity 2 in the banner).
    bool exact = false;
    for (int i = 0; i < mode->refreshCount; ++i) {
        const RefreshRate* r = mode->refreshData + i;
        if (r->numerator == *refreshNum && r->denominator == *refreshDen) { exact = true; break; }
    }
    if (!exact) {
        // Otherwise take the first entry with the same integer Hz, else entry 0.
        int pick = 0;
        for (int i = 0; i < mode->refreshCount; ++i) {
            const RefreshRate* r = mode->refreshData + i;
            if (r->numerator / r->denominator == *refreshNum / *refreshDen) { pick = i; break; }
            pick = 0;
        }
        *refreshNum = mode->refreshData[pick].numerator;
        *refreshDen = mode->refreshData[pick].denominator;
    }

    if (adapter->multisampleData[*antialias] == 0)
        *antialias = 0;
    if (adapter->maxFilterLevel < *filter)
        *filter = adapter->maxFilterLevel;
}

}  // namespace

void Video_Device_ValidateConfiguration(Device* self,
                                        int* renderer, int* adapterIdx,
                                        int* width, int* height,
                                        int* refreshNum, int* refreshDen,
                                        int* antialias, int* filter) {
    RT_DIFF_BEGIN("Video::Device::ValidateConfiguration");

    int* args[8] = { renderer, adapterIdx, width, height,
                     refreshNum, refreshDen, antialias, filter };

    // Snapshot the inputs, and the caller's aliasing, BEFORE ours runs.
    int  scratchStore[8] = {0,0,0,0,0,0,0,0};
    int* scratchArgs[8] = {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
    if (rt_diff_on__) {
        for (int i = 0; i < 8; ++i) {
            scratchStore[i] = *args[i];
            scratchArgs[i]  = &scratchStore[i];
            for (int j = 0; j < i; ++j) {
                if (args[j] == args[i]) { scratchArgs[i] = scratchArgs[j]; break; }
            }
        }
    }

    ValidateConfigurationBody(self, renderer, adapterIdx, width, height,
                              refreshNum, refreshDen, antialias, filter);

    if (rt_diff_on__) {
        using Fn = void (*)(Device*, int*, int*, int*, int*, int*, int*, int*, int*);
        RtOriginal<Fn>(kValidateConfiguration)(
            self, scratchArgs[0], scratchArgs[1], scratchArgs[2], scratchArgs[3],
            scratchArgs[4], scratchArgs[5], scratchArgs[6], scratchArgs[7]);

        bool equal = true;
        int  badIndex = -1;
        for (int i = 0; i < 8; ++i) {
            if (*args[i] != *scratchArgs[i]) { equal = false; badIndex = i; break; }
        }
        static const char* const kArgNames[8] = {
            "renderer", "adapter", "width", "height",
            "refreshNum", "refreshDen", "antialias", "filter"
        };
        char oursDesc[96], theirsDesc[96];
        if (equal) {
            std::snprintf(oursDesc, sizeof(oursDesc), "all 8 out-params agree");
            std::snprintf(theirsDesc, sizeof(theirsDesc), "all 8 out-params agree");
        } else {
            std::snprintf(oursDesc, sizeof(oursDesc), "%s=%d",
                          kArgNames[badIndex], *args[badIndex]);
            std::snprintf(theirsDesc, sizeof(theirsDesc), "%s=%d",
                          kArgNames[badIndex], *scratchArgs[badIndex]);
        }
        RT_DIFF_SHADOW(equal, oursDesc, theirsDesc);
    }
}

// ============================================================================
// Video::Device::ApplyChangedSettings  —  retail 0x1800f8110, ordinal 1604
//
// Copies the resolution the player chose out of Video::Configuration into the
// Device's back-buffer dimensions, re-finds the matching display mode index,
// raises the "settings changed" byte and returns true unconditionally
// (`MOV AL,1` at 0x1800f816a — there is no failure path).
//
// Which pair it copies depends on Configuration::fullscreen: fullscreen takes
// width/height (+0x0c/+0x10), windowed takes windowWidth/windowHeight
// (+0x1c/+0x20). Retail re-tests the fullscreen byte for each of the two,
// which is a codegen artifact; one test is equivalent because nothing between
// them writes it.
//
// The mode search does NOT break on a match and does NOT reset the index when
// nothing matches: displayModeIndex keeps whatever it had. Both are retail.
// ============================================================================
bool Video_Device_ApplyChangedSettings(Device* self) {
    RT_DIFF_BEGIN("Video::Device::ApplyChangedSettings");

    // Snapshot the whole object BEFORE ours runs, so the original can be run
    // on a private replica afterwards. The replica shares the mode array by
    // pointer, which is safe: this function only reads it.
    Device scratch{};
    if (rt_diff_on__) std::memcpy(&scratch, self, sizeof(Device));

    const Configuration* cfg = *ConfigSingletonSlot();

    const int width  = cfg->fullscreen ? cfg->width  : cfg->windowWidth;
    self->backBufferWidth = width;
    const int height = cfg->fullscreen ? cfg->height : cfg->windowHeight;
    self->backBufferHeight = height;

    for (int i = 0; i < self->modeCount; ++i) {
        const DisplayMode* m = self->modeData + i;
        if (width == m->width && height == m->height)
            self->displayModeIndex = i;
    }

    self->settingsChanged = 1;

    if (rt_diff_on__) {
        using Fn = bool (*)(Device*);
        const bool theirsRet = RtOriginal<Fn>(kApplyChangedSettings)(&scratch);
        const bool equal =
            self->backBufferWidth  == scratch.backBufferWidth  &&
            self->backBufferHeight == scratch.backBufferHeight &&
            self->displayModeIndex == scratch.displayModeIndex &&
            self->settingsChanged  == scratch.settingsChanged  &&
            theirsRet == true;
        char oursDesc[128], theirsDesc[128];
        std::snprintf(oursDesc, sizeof(oursDesc), "w=%d h=%d mode=%d changed=%u ret=1",
                      self->backBufferWidth, self->backBufferHeight,
                      self->displayModeIndex, (unsigned)self->settingsChanged);
        std::snprintf(theirsDesc, sizeof(theirsDesc), "w=%d h=%d mode=%d changed=%u ret=%d",
                      scratch.backBufferWidth, scratch.backBufferHeight,
                      scratch.displayModeIndex, (unsigned)scratch.settingsChanged,
                      (int)theirsRet);
        RT_DIFF_SHADOW(equal, oursDesc, theirsDesc);
    }

    return true;
}

// ============================================================================
// Video::RenderBuffer::Create  —  retail 0x1800f3f10, ordinal 1882
//
// Records the colour and depth surfaces, derives the buffer's dimensions from
// whichever of them is non-null (colour wins), and tail-calls slot 1 of the
// platform object at +0x28 — which in this process is
// VideoDX11 RenderBufferData::Stub1, `mov al,1; ret`
// (dx11/src/smallclasses.cpp:12).
//
// The dimension rule is the same for width and height, driven by three bits of
// ImageBuffer::flags (+0x34) that runtime/types/video/ImageBuffer.h did not
// previously document:
//
//   bit 8  -> the back buffer's size          (Device+0x04 / +0x08)
//   bit 9  -> half the back buffer's size     (signed /2, not >>1)
//   bit 6  -> the image's own size, shifted right by Configuration::shadowScalar
//   none   -> the image's own size
//
// Retail emits the width and height paths as two separately-scheduled copies
// of that rule with a shared tail; they compute the same thing.
// ============================================================================
namespace {

int RenderBufferExtent(uint32_t flags,
                       int32_t ownExtent, int32_t backBufferExtent,
                       const Configuration* cfg) {
    if (flags & RtVideo::kImageBufferBackBuffer)     return backBufferExtent;
    if (flags & RtVideo::kImageBufferHalfBackBuffer) return backBufferExtent / 2;
    int v = ownExtent;
    if (flags & RtVideo::kImageBufferShadowScaled)
        v >>= (cfg->shadowScalar & 31);   // retail: SAR EDX,CL with CL = scalar & 0x1f
    return v;
}

bool CallPlatformCreate(RenderBuffer* self) {
    void* data = self->platformData;
    void** vtbl = *reinterpret_cast<void***>(data);
    using Slot1 = bool (*)(void*);
    return reinterpret_cast<Slot1>(vtbl[1])(data);
}

// The stub platform object the diff harness hands the original, so that
// running it a second time cannot create a second GPU resource. Slot 1 is the
// only slot Create touches.
bool DiffStubCreate(void*) { return true; }

}  // namespace

bool Video_RenderBuffer_Create(RenderBuffer* self,
                               const ImageBuffer* color, const ImageBuffer* depth) {
    RT_DIFF_BEGIN("Video::RenderBuffer::Create");

    RenderBuffer scratch{};
    void*        stubVtbl[2] = { nullptr, reinterpret_cast<void*>(&DiffStubCreate) };
    void*        stubData    = &stubVtbl[0];
    if (rt_diff_on__) {
        std::memcpy(&scratch, self, sizeof(RenderBuffer));
        scratch.platformData = &stubData;
    }

    self->colorBuffer = const_cast<ImageBuffer*>(color);
    self->depthBuffer = const_cast<ImageBuffer*>(depth);

    const Device*        dev = *DeviceSingletonSlot();
    const Configuration* cfg = *ConfigSingletonSlot();
    const ImageBuffer*   src = color ? color : depth;
    const uint32_t       flags = src->flags;

    self->width  = RenderBufferExtent(flags, src->width,  dev->backBufferWidth,  cfg);
    self->height = RenderBufferExtent(flags, src->height, dev->backBufferHeight, cfg);

    if (rt_diff_on__) {
        using Fn = bool (*)(RenderBuffer*, const ImageBuffer*, const ImageBuffer*);
        RtOriginal<Fn>(kRenderBufferCreate)(&scratch, color, depth);
        // Compare only the 0x18 bytes Create writes. The RETURN VALUE is not
        // compared: theirs came from DiffStubCreate, not from the real
        // RenderBufferData, so it carries no information.
        RT_DIFF_BYTES(self, &scratch, 0x18);
    }

    return CallPlatformCreate(self);
}

// ============================================================================
// Video::RenderStateData::~RenderStateData  —  retail 0x1800f8d70, ordinal 662
//
//   1800f8d70  LEA RAX,[0x180158a60]      ; ??_7RenderStateData@Video@@6B@
//   1800f8d77  MOV qword ptr [RCX],RAX
//   1800f8d7a  RET
//
// The entire body is the base-class vtable re-stamp. The vtable is an EXPORTED
// symbol (ordinal 1490), so the port resolves it by name rather than hardcoding
// 0x180158a60 — PLAN §4.3 risk 3's caveat applies but is satisfied here for the
// narrowest possible reason: this is not us CHOOSING to stamp a DLL vtable on
// an object we built, it is us reproducing the one store retail's own dtor
// makes on an object VideoDX11 built (dx11/src/renderstatedata.cpp calls this
// as `d.baseDtor(self)` from its subclass dtor).
//
// Registered by NAME and by ADDRESS: dx11/src/renderstatedata.cpp:317-322
// falls back to ResolveAddr(Module::Runtime, 0x1800f8d70) when the name does
// not resolve, and a name-only registration would leave that fallback on the
// DLL (WAVE3_LANE_A.md §6's lesson).
// ============================================================================
void Video_RenderStateData_Dtor(void* self) {
    RT_DIFF_BEGIN("Video::RenderStateData::~RenderStateData");

    static void* vtable = ResolveExportRaw("??_7RenderStateData@Video@@6B@");
    *reinterpret_cast<void**>(self) = vtable;

    if (rt_diff_on__) {
        void* scratch = nullptr;
        using Fn = void (*)(void*);
        RtOriginal<Fn>(kRenderStateDataDtor)(&scratch);
        const void* ours   = *reinterpret_cast<void**>(self);
        const void* theirs = scratch;
        RT_DIFF_SCALAR(ours, theirs);
    }
}

// ============================================================================
// Video::Configuration::Save  —  retail 0x1800f6d10, ordinal 3605
//
// Eighteen IO::Persistence::Set calls under the section "Video". Each key
// string below was read out of the retail body's LEA operands, so the names are
// the DLL's own, not reconstructed.
//
// THIS IS NOT A REGISTRY WRITE. The wave brief for this lane said Save "writes
// the real HKCU registry" and should be held back on the Wave 4
// Persistence::WriteSettings precedent. That is wrong, and the correction is
// what makes this function portable: IO::Persistence::Set (0x1800b80c0 /
// 0x1800b8380) is an in-memory Collection::Set insert — Wave 4 lane C ported
// both overloads for exactly that reason and held back only ReadSettings /
// WriteSettings, which are the registry pair. Save touches no registry key;
// IO::Persistence::WriteSettings, called from elsewhere at shutdown, does.
//
// Set is reached through ResolveExport, NOT RtOriginal, so these 18 calls go to
// Wave 4's port when it is registered and to the DLL when it is not — which is
// what retail's own body does relative to its class.
//
// ORACLE. Ours writes; then the harness reads all 18 keys back through the
// DLL's Persistence::Get and requires each to equal the field it came from.
// A readback alone would not catch a WRONG KEY NAME (we would read back the
// same wrong key we wrote), so before ours runs the harness POISONS all 18
// retail keys with a value that differs from the one about to be written. A
// port that writes the wrong key, the wrong field, or nothing at all then
// leaves a poisoned value behind and the predicate names the offending key
// (reference_shadow_predicate_must_name_its_failing_term.md). The original is
// run afterwards, which restores every key to the correct value whether ours
// was right or not.
// ============================================================================
namespace {

using PersistSetIntFn  = void (*)(void*, const char*, const char*, int);
using PersistSetBoolFn = void (*)(void*, const char*, const char*, bool);
using PersistGetIntFn  = bool (*)(const void*, const char*, const char*, int*);
using PersistGetBoolFn = bool (*)(const void*, const char*, const char*, bool*);

PersistSetIntFn  PersistSetInt() {
    static auto fn = reinterpret_cast<PersistSetIntFn>(
        ResolveExport("?Set@Persistence@IO@@QEAAXPEBD0H@Z"));
    return fn;
}
PersistSetBoolFn PersistSetBool() {
    static auto fn = reinterpret_cast<PersistSetBoolFn>(
        ResolveExport("?Set@Persistence@IO@@QEAAXPEBD0_N@Z"));
    return fn;
}

constexpr const char* kSectionVideo = "Video";

// Retail's 18 stores, in retail's order. `isBool` picks the Set overload.
struct SaveField {
    const char* key;
    bool        isBool;
    uint16_t    offset;   // byte offset into Video::Configuration
};

constexpr SaveField kSaveFields[] = {
    { "Interface",        false, 0x04 },
    { "Adapter",          false, 0x08 },
    { "Width",            false, 0x0C },
    { "Height",           false, 0x10 },
    { "WindowWidth",      false, 0x1C },
    { "WindowHeight",     false, 0x20 },
    { "WindowX",          false, 0x14 },
    { "WindowY",          false, 0x18 },
    { "RefreshN",         false, 0x38 },
    { "RefreshD",         false, 0x3C },
    { "Multisample",      false, 0x24 },
    { "Fullscreen",       true,  0x40 },
    { "VSync",            true,  0x41 },
    { "TextureFilter",    false, 0x28 },
    { "ShadowScalar",     false, 0x2C },
    { "ShaderDetail",     false, 0x30 },
    { "ReflectionDetail", false, 0x34 },
    { "Settings",         true,  0x00 },
};
constexpr int kSaveFieldCount = (int)(sizeof(kSaveFields) / sizeof(kSaveFields[0]));

int  FieldInt (const Configuration* c, uint16_t off) {
    return *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(c) + off);
}
bool FieldBool(const Configuration* c, uint16_t off) {
    return *(reinterpret_cast<const uint8_t*>(c) + off) != 0;
}

}  // namespace

void Video_Configuration_Save(Configuration* self) {
    RT_DIFF_BEGIN("Video::Configuration::Save");

    void* persistence = *PersistenceSingletonSlot();

    if (rt_diff_on__) {
        // Poison, so that a key we never wrote cannot pass the readback.
        for (int i = 0; i < kSaveFieldCount; ++i) {
            const SaveField& f = kSaveFields[i];
            if (f.isBool)
                RT_ORIGINAL_ONCE(PersistSetBoolFn, "?Set@Persistence@IO@@QEAAXPEBD0_N@Z")(
                    persistence, kSectionVideo, f.key, !FieldBool(self, f.offset));
            else
                RT_ORIGINAL_ONCE(PersistSetIntFn, "?Set@Persistence@IO@@QEAAXPEBD0H@Z")(
                    persistence, kSectionVideo, f.key, FieldInt(self, f.offset) + 1);
        }
    }

    for (int i = 0; i < kSaveFieldCount; ++i) {
        const SaveField& f = kSaveFields[i];
        if (f.isBool)
            PersistSetBool()(persistence, kSectionVideo, f.key, FieldBool(self, f.offset));
        else
            PersistSetInt()(persistence, kSectionVideo, f.key, FieldInt(self, f.offset));
    }

    if (rt_diff_on__) {
        bool        equal = true;
        char        oursDesc[96], theirsDesc[96];
        oursDesc[0] = theirsDesc[0] = '\0';

        for (int i = 0; i < kSaveFieldCount && equal; ++i) {
            const SaveField& f = kSaveFields[i];
            if (f.isBool) {
                const bool want = FieldBool(self, f.offset);
                bool got = !want;
                const bool found = RT_ORIGINAL_ONCE(PersistGetBoolFn,
                    "?Get@Persistence@IO@@QEBA_NPEBD0AEA_N@Z")(
                        persistence, kSectionVideo, f.key, &got);
                if (!found || got != want) {
                    equal = false;
                    std::snprintf(oursDesc, sizeof(oursDesc), "%s=%d", f.key, (int)want);
                    std::snprintf(theirsDesc, sizeof(theirsDesc), "%s readback found=%d value=%d",
                                  f.key, (int)found, (int)got);
                }
            } else {
                const int want = FieldInt(self, f.offset);
                int got = want + 1;
                const bool found = RT_ORIGINAL_ONCE(PersistGetIntFn,
                    "?Get@Persistence@IO@@QEBA_NPEBD0AEAH@Z")(
                        persistence, kSectionVideo, f.key, &got);
                if (!found || got != want) {
                    equal = false;
                    std::snprintf(oursDesc, sizeof(oursDesc), "%s=%d", f.key, want);
                    std::snprintf(theirsDesc, sizeof(theirsDesc), "%s readback found=%d value=%d",
                                  f.key, (int)found, got);
                }
            }
        }
        if (equal) {
            std::snprintf(oursDesc, sizeof(oursDesc), "all %d keys read back", kSaveFieldCount);
            std::snprintf(theirsDesc, sizeof(theirsDesc), "all %d keys read back", kSaveFieldCount);
        }
        RT_DIFF_SHADOW(equal, oursDesc, theirsDesc);

        // Let the original run too, so the settings map ends the call in
        // exactly the state retail would leave it in even if ours was wrong.
        using Fn = void (*)(Configuration*);
        RtOriginal<Fn>(kConfigurationSave)(self);
    }
}

// ============================================================================
// Registration
// ============================================================================
int RtVideoDeviceRegister() {
    int n = 0;

    RegisterRuntimeExport("?GetInterfaceCount@Device@Video@@QEBAHXZ",
                          (void*)&Video_Device_GetInterfaceCount); ++n;
    RegisterRuntimeExport("?GetInterface@Device@Video@@QEBAAEBUGraphicsInterface@12@H@Z",
                          (void*)&Video_Device_GetInterface); ++n;
    RegisterRuntimeExport("?GetMultisampleModeCount@Device@Video@@QEBAHXZ",
                          (void*)&Video_Device_GetMultisampleModeCount); ++n;
    RegisterRuntimeExport("?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z",
                          (void*)&Video_Device_IsMultisampleModeSupported); ++n;
    RegisterRuntimeExport("?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z",
                          (void*)&Video_Device_ValidateConfiguration); ++n;
    RegisterRuntimeExport("?ApplyChangedSettings@Device@Video@@QEAA_NXZ",
                          (void*)&Video_Device_ApplyChangedSettings); ++n;
    RegisterRuntimeExport("?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z",
                          (void*)&Video_RenderBuffer_Create); ++n;
    RegisterRuntimeExport("??1RenderStateData@Video@@UEAA@XZ",
                          (void*)&Video_RenderStateData_Dtor); ++n;
    RegisterRuntimeExport("?Save@Configuration@Video@@QEAAXXZ",
                          (void*)&Video_Configuration_Save); ++n;

    // Address door for the one call site that has an address FALLBACK:
    // dx11/src/renderstatedata.cpp:317-322. Not counted again — it is the same
    // function reached through the other registry
    // (reference_census_three_blind_spots.md: counting both doors double-counts).
    RegisterRuntimeAddr(0x1800f8d70, (void*)&Video_RenderStateData_Dtor);

    return n;
}
