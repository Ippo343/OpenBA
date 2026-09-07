#pragma once
// ============================================================================
// runtime/types/video/Device.h — Wave 6 lane B
//   Video::Device, its four nested value types, Video::Configuration and
//   Video::RenderBuffer.
// ============================================================================
//
// Every offset below is transcribed from a retail body decompiled at its real
// address in Runtime-x64.dll; the function that pins each one is named next to
// it. Where a field is only pinned by "the ctor zeroes this qword" the comment
// says so, because that is weaker evidence than a read with a known meaning.
//
// AGREEMENT WITH standalone/video_interface.cpp
// ---------------------------------------------
// That file writes DIRECTLY into the live Device object at hardcoded offsets
// (`kSlot0 = 0xb0`, `kSlotStride = 0x18`, module/+0x00, GetDeviceName/+0x08,
// CreateDeviceData/+0x10) to pre-fill the DX11 plugin slot. `Device::dllSlots`
// below is that same array, at the same offset, with the same stride and the
// same member order, and the static_asserts pin it. `standalone/main.cpp`
// (+0xf8 active interface index, +0x100 DeviceData*) and `dx11/src/*.cpp`
// (+0x100) agree too. Nothing in this header contradicts a hardcoded offset
// anywhere else in the tree — see docs/runtime_port/WAVE6_LANE_B.md §3.
//
// TWO ARRAYS, NOT ONE. Video::Device carries an `Array<GraphicsInterface>` at
// +0x40 (what GetInterface/GetInterfaceCount serve) AND a separate raw
// three-element array of DLL plugin slots at +0xb0. Both have stride 0x18 and
// it is easy to conflate them: the first is the ENUMERATED renderers the
// options dialog lists, the second is the LOADED plugin modules. They are
// unrelated memory.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtVideo {

// ----------------------------------------------------------------------------
// Video::Device::RefreshRate — 8 bytes
// Pinned by Video::Device::ValidateConfiguration (0x1800f7370), which walks a
// display mode's refresh list with `piVar2 += 2` over int pairs, and by
// ??0RefreshRate@Device@Video@@QEAA@HH@Z (0x18000d270).
// ----------------------------------------------------------------------------
struct RefreshRate {
    int32_t numerator;    // +0x00
    int32_t denominator;  // +0x04
};
static_assert(sizeof(RefreshRate) == 0x08, "RefreshRate is 8 bytes");
static_assert(offsetof(RefreshRate, numerator) == 0x00, "RefreshRate::numerator");
static_assert(offsetof(RefreshRate, denominator) == 0x04, "RefreshRate::denominator");

// ----------------------------------------------------------------------------
// Video::Device::DisplayMode — 0x18 bytes
// ??0DisplayMode@Device@Video@@QEAA@HH@Z (0x18000d300) writes width, height and
// zeroes +0x08 and +0x10; ValidateConfiguration strides the mode list by 0x18
// and reads +0x00/+0x04 as width/height, +0x08 as the refresh array data and
// +0x10 as its count. That {data, size, capacity} triple is the standard
// 16-byte Collection::Array (runtime/types/collection/Collection.h).
// ----------------------------------------------------------------------------
struct DisplayMode {
    int32_t      width;             // +0x00  ValidateConfiguration, ApplyChangedSettings
    int32_t      height;            // +0x04  ditto
    RefreshRate* refreshData;       // +0x08  Array<RefreshRate>::_data
    int32_t      refreshCount;      // +0x10  Array<RefreshRate>::_size
    int32_t      refreshCapacity;   // +0x14  Array<RefreshRate>::_capacity (inferred from the Array shape)
};
static_assert(sizeof(DisplayMode) == 0x18, "DisplayMode is 0x18 bytes (GetInterface/ValidateConfiguration stride)");
static_assert(offsetof(DisplayMode, width) == 0x00, "DisplayMode::width");
static_assert(offsetof(DisplayMode, height) == 0x04, "DisplayMode::height");
static_assert(offsetof(DisplayMode, refreshData) == 0x08, "DisplayMode::refreshData");
static_assert(offsetof(DisplayMode, refreshCount) == 0x10, "DisplayMode::refreshCount");
static_assert(offsetof(DisplayMode, refreshCapacity) == 0x14, "DisplayMode::refreshCapacity");

// ----------------------------------------------------------------------------
// Video::Device::Adapter — 0x38 bytes
// ??0Adapter@Device@Video@@QEAA@XZ (0x1800f7100) zeroes +0x00..+0x27, writes
// -1 as a qword over +0x28..+0x2f and -1 as a dword at +0x30. ValidateConfiguration
// reads the mode array at +0x08/+0x10, indexes a byte array through +0x18, and
// clamps its 8th out-parameter against the int at +0x28. The 0x38 stride is
// ValidateConfiguration's `*param_2 * 0x38`.
// ----------------------------------------------------------------------------
struct Adapter {
    void*        name;              // +0x00  System::String (data pointer); ctor zeroes it
    DisplayMode* modeData;          // +0x08  Array<DisplayMode>::_data       (ValidateConfiguration)
    int32_t      modeCount;         // +0x10  Array<DisplayMode>::_size       (ValidateConfiguration)
    int32_t      modeCapacity;      // +0x14  Array<DisplayMode>::_capacity   (inferred from the Array shape)
    uint8_t*     multisampleData;   // +0x18  Array<bool>::_data              (ValidateConfiguration: *(char*)(idx + adapter[0x18]))
    int32_t      multisampleCount;  // +0x20  Array<bool>::_size              (inferred; ctor zeroes)
    int32_t      multisampleCap;    // +0x24  Array<bool>::_capacity          (inferred; ctor zeroes)
    int32_t      maxFilterLevel;    // +0x28  ctor = -1; ValidateConfiguration clamps out-param 8 to it
    int32_t      field2C;           // +0x2C  ctor = -1 (same qword store); meaning not recovered
    int32_t      field30;           // +0x30  ctor = -1; meaning not recovered
    int32_t      field34;           // +0x34  not written by the ctor; tail padding or an unrecovered field
};
static_assert(sizeof(Adapter) == 0x38, "Adapter is 0x38 bytes (ValidateConfiguration stride)");
static_assert(offsetof(Adapter, name) == 0x00, "Adapter::name");
static_assert(offsetof(Adapter, modeData) == 0x08, "Adapter::modeData");
static_assert(offsetof(Adapter, modeCount) == 0x10, "Adapter::modeCount");
static_assert(offsetof(Adapter, modeCapacity) == 0x14, "Adapter::modeCapacity");
static_assert(offsetof(Adapter, multisampleData) == 0x18, "Adapter::multisampleData");
static_assert(offsetof(Adapter, multisampleCount) == 0x20, "Adapter::multisampleCount");
static_assert(offsetof(Adapter, multisampleCap) == 0x24, "Adapter::multisampleCap");
static_assert(offsetof(Adapter, maxFilterLevel) == 0x28, "Adapter::maxFilterLevel");
static_assert(offsetof(Adapter, field2C) == 0x2C, "Adapter::field2C");
static_assert(offsetof(Adapter, field30) == 0x30, "Adapter::field30");
static_assert(offsetof(Adapter, field34) == 0x34, "Adapter::field34");

// ----------------------------------------------------------------------------
// Video::Device::GraphicsInterface — 0x18 bytes
// ??0GraphicsInterface@Device@Video@@QEAA@XZ (0x18000cd90) zeroes exactly three
// qwords. GetInterface (0x18000d3c0) strides by 0x18. ValidateConfiguration
// reads the adapter array at +0x08/+0x10, and
// code/Game/Dialog/optionsdialog.cpp:462 does the same by hand
// ("GraphicsInterface starts with a System::String (the interface name)").
// ----------------------------------------------------------------------------
struct GraphicsInterface {
    void*    name;            // +0x00  System::String data pointer — the renderer name shown in the combo
    Adapter* adapterData;     // +0x08  Array<Adapter>::_data
    int32_t  adapterCount;    // +0x10  Array<Adapter>::_size
    int32_t  adapterCapacity; // +0x14  Array<Adapter>::_capacity (inferred from the Array shape)
};
static_assert(sizeof(GraphicsInterface) == 0x18, "GraphicsInterface is 0x18 bytes (GetInterface stride)");
static_assert(offsetof(GraphicsInterface, name) == 0x00, "GraphicsInterface::name");
static_assert(offsetof(GraphicsInterface, adapterData) == 0x08, "GraphicsInterface::adapterData");
static_assert(offsetof(GraphicsInterface, adapterCount) == 0x10, "GraphicsInterface::adapterCount");
static_assert(offsetof(GraphicsInterface, adapterCapacity) == 0x14, "GraphicsInterface::adapterCapacity");

// ----------------------------------------------------------------------------
// One loaded renderer plugin DLL — 0x18 bytes, three of them inline at
// Device+0xb0 (DX9, DX11, GL).
//
// This IS the struct standalone/video_interface.cpp writes into by hand;
// see this header's banner. Video::Device::InitializeInterfaces (0x1800f79f0)
// is the body that defines it: it tests the module word for zero, LoadLibraryW's
// GetDLLForInterface(idx), then stores GetProcAddress("CreateDeviceData") at
// +0x10 and GetProcAddress("GetDeviceName") at +0x08 — note the reversed order
// relative to the field order.
// ----------------------------------------------------------------------------
struct InterfaceSlot {
    void* module;            // +0x00  HMODULE. Only ever tested against zero and FreeLibrary'd.
    void* getDeviceName;     // +0x08  const char* (*)()
    void* createDeviceData;  // +0x10  DeviceData* (*)(Device*)
};
static_assert(sizeof(InterfaceSlot) == 0x18, "InterfaceSlot is 0x18 bytes (InitializeInterfaces stride)");
static_assert(offsetof(InterfaceSlot, module) == 0x00, "InterfaceSlot::module");
static_assert(offsetof(InterfaceSlot, getDeviceName) == 0x08, "InterfaceSlot::getDeviceName");
static_assert(offsetof(InterfaceSlot, createDeviceData) == 0x10, "InterfaceSlot::createDeviceData");

// ----------------------------------------------------------------------------
// Video::Device — 0x108 bytes
//
// Pinned by ??0Device@Video@@QEAA@W4DeviceType@01@@Z (0x1800f7160), which
// writes every field below except the two critical-section bookkeeping words at
// +0x88/+0x8c (Lock() seeds those), plus the four getters this lane ports,
// ApplyChangedSettings (0x1800f8110), ValidateConfiguration (0x1800f7370),
// Startup (0x1800f7270) and RenderBuffer::PlatformConstruct (0x1800f4320).
//
// The critical section is declared as an opaque byte block on purpose: this
// header is included from runtime/src/, which PLAN §4.1 decision 1 keeps free
// of <windows.h>. Nothing in this lane touches it.
// ----------------------------------------------------------------------------
struct Device {
    int32_t           type;                // +0x00  DeviceType ctor argument; 0 == the singleton
    int32_t           backBufferWidth;     // +0x04  ApplyChangedSettings writes; RenderBuffer::Create reads
    int32_t           backBufferHeight;    // +0x08  ApplyChangedSettings writes; RenderBuffer::Create reads
    int32_t           displayModeIndex;    // +0x0C  ApplyChangedSettings writes on a width/height match
    uint8_t           gap10[0x10];         // +0x10..+0x1F  ctor zeroes +0x10..+0x17 and +0x14..+0x1B; unread by this lane
    DisplayMode*      modeData;            // +0x20  Array<DisplayMode>::_data      (ApplyChangedSettings)
    int32_t           modeCount;           // +0x28  Array<DisplayMode>::_size      (ApplyChangedSettings)
    int32_t           modeCapacity;        // +0x2C  Array<DisplayMode>::_capacity  (inferred from the Array shape)
    uint8_t*          multisampleData;     // +0x30  Array<bool>::_data             (IsMultisampleModeSupported)
    int32_t           multisampleCount;    // +0x38  Array<bool>::_size             (GetMultisampleModeCount)
    int32_t           multisampleCap;      // +0x3C  Array<bool>::_capacity         (inferred from the Array shape)
    GraphicsInterface* interfaceData;      // +0x40  Array<GraphicsInterface>::_data (GetInterface)
    int32_t           interfaceCount;      // +0x48  Array<GraphicsInterface>::_size (GetInterfaceCount)
    int32_t           interfaceCapacity;   // +0x4C  Array<GraphicsInterface>::_capacity (inferred)
    uint8_t           gap50[0x10];         // +0x50..+0x5F  ctor zeroes; unread by this lane
    uint8_t           lock[0x28];          // +0x60  CRITICAL_SECTION (InitializeCriticalSection + spin 5000)
    uint32_t          lockOwnerThreadId;   // +0x88  RenderBuffer::PlatformConstruct / ~RenderBuffer
    int32_t           lockRecursionCount;  // +0x8C  ditto
    void*             windowHandle;        // +0x90  HWND, copied from Application::_singleton[0] by Startup
    int32_t           maxAnisotropy;       // +0x98  max anisotropic filter level; read by
                                           //        code/Game/Dialog/optionsdialog.cpp:359 as
                                           //        `*(int*)(vidDev + 0x98)`. Not written by the
                                           //        ctor — adapter enumeration fills it.
    uint8_t           settingsChanged;     // +0x9C  ApplyChangedSettings sets it to 1
    uint8_t           field9D;             // +0x9D  ctor zeroes it in the same 2-byte store
    uint8_t           gap9E[0x12];         // +0x9E..+0xAF  ctor zeroes +0xa0 and +0xa8
    InterfaceSlot     dllSlots[3];         // +0xB0  DX9, DX11, GL — standalone/video_interface.cpp writes slot 1 here
    int32_t           activeInterface;     // +0xF8  ctor = -1; Startup copies it back into Configuration::interfaceIndex
    int32_t           gapFC;               // +0xFC
    void*             deviceData;          // +0x100 DeviceData* — dx11/ reads this at exactly this offset
};
static_assert(sizeof(Device) == 0x108, "Device is 0x108 bytes");
static_assert(offsetof(Device, type) == 0x00, "Device::type");
static_assert(offsetof(Device, backBufferWidth) == 0x04, "Device::backBufferWidth");
static_assert(offsetof(Device, backBufferHeight) == 0x08, "Device::backBufferHeight");
static_assert(offsetof(Device, displayModeIndex) == 0x0C, "Device::displayModeIndex");
static_assert(offsetof(Device, modeData) == 0x20, "Device::modeData");
static_assert(offsetof(Device, modeCount) == 0x28, "Device::modeCount");
static_assert(offsetof(Device, modeCapacity) == 0x2C, "Device::modeCapacity");
static_assert(offsetof(Device, multisampleData) == 0x30, "Device::multisampleData");
static_assert(offsetof(Device, multisampleCount) == 0x38, "Device::multisampleCount");
static_assert(offsetof(Device, multisampleCap) == 0x3C, "Device::multisampleCap");
static_assert(offsetof(Device, interfaceData) == 0x40, "Device::interfaceData");
static_assert(offsetof(Device, interfaceCount) == 0x48, "Device::interfaceCount");
static_assert(offsetof(Device, interfaceCapacity) == 0x4C, "Device::interfaceCapacity");
static_assert(offsetof(Device, lock) == 0x60, "Device::lock");
static_assert(offsetof(Device, lockOwnerThreadId) == 0x88, "Device::lockOwnerThreadId");
static_assert(offsetof(Device, lockRecursionCount) == 0x8C, "Device::lockRecursionCount");
static_assert(offsetof(Device, windowHandle) == 0x90, "Device::windowHandle");
static_assert(offsetof(Device, maxAnisotropy) == 0x98, "Device::maxAnisotropy — must match optionsdialog.cpp:359");
static_assert(offsetof(Device, settingsChanged) == 0x9C, "Device::settingsChanged");
static_assert(offsetof(Device, field9D) == 0x9D, "Device::field9D");
static_assert(offsetof(Device, dllSlots) == 0xB0, "Device::dllSlots — must match standalone/video_interface.cpp kSlot0");
static_assert(offsetof(Device, activeInterface) == 0xF8, "Device::activeInterface — must match standalone/main.cpp:1068");
static_assert(offsetof(Device, deviceData) == 0x100, "Device::deviceData — must match dx11/src/buffers.cpp:209");

// ----------------------------------------------------------------------------
// Video::Configuration — the video settings singleton (0x1801fe7a0)
//
// Every offset is pinned twice: by ??0Configuration@Video@@QEAA@XZ (0x1800f6f10),
// which gives each field its default, and by Video::Configuration::Save
// (0x1800f6d10), which gives each field its persisted NAME. The two agree.
//
// sizeof is 0x44 by natural alignment (last field at +0x42, alignment 4). No
// body this lane read allocates a Configuration, so 0x44 is INFERRED, not
// measured — that is why there is no sizeof static_assert here.
// ----------------------------------------------------------------------------
struct Configuration {
    uint8_t  settingsValid;     // +0x00  ctor = 1; Save -> "Settings"          (bool)
    uint8_t  pad01[3];          // +0x01
    int32_t  interfaceIndex;    // +0x04  ctor = 1; Save -> "Interface"
    int32_t  adapterIndex;      // +0x08  ctor = 0; Save -> "Adapter"
    int32_t  width;             // +0x0C  ctor = -1; Save -> "Width"
    int32_t  height;            // +0x10  ctor = -1; Save -> "Height"
    int32_t  windowX;           // +0x14  ctor = -1; Save -> "WindowX"
    int32_t  windowY;           // +0x18  ctor = -1; Save -> "WindowY"
    int32_t  windowWidth;       // +0x1C  ctor = -1; Save -> "WindowWidth"
    int32_t  windowHeight;      // +0x20  ctor = -1; Save -> "WindowHeight"
    int32_t  multisample;       // +0x24  ctor = 1;  Save -> "Multisample"
    int32_t  textureFilter;     // +0x28  ctor = 0;  Save -> "TextureFilter"
    int32_t  shadowScalar;      // +0x2C  ctor = 1;  Save -> "ShadowScalar"; RenderBuffer::Create shifts by it
    int32_t  shaderDetail;      // +0x30  ctor = 2;  Save -> "ShaderDetail"
    int32_t  reflectionDetail;  // +0x34  ctor = 1;  Save -> "ReflectionDetail"
    int32_t  refreshNumerator;  // +0x38  ctor = 60; Save -> "RefreshN"
    int32_t  refreshDenominator;// +0x3C  ctor = 1;  Save -> "RefreshD"
    uint8_t  fullscreen;        // +0x40  ctor = 1;  Save -> "Fullscreen"      (bool)
    uint8_t  vsync;             // +0x41  ctor = 1;  Save -> "VSync"           (bool)
    uint8_t  field42;           // +0x42  ctor = 0;  not persisted
};
static_assert(offsetof(Configuration, settingsValid) == 0x00, "Configuration::settingsValid");
static_assert(offsetof(Configuration, interfaceIndex) == 0x04, "Configuration::interfaceIndex");
static_assert(offsetof(Configuration, adapterIndex) == 0x08, "Configuration::adapterIndex");
static_assert(offsetof(Configuration, width) == 0x0C, "Configuration::width");
static_assert(offsetof(Configuration, height) == 0x10, "Configuration::height");
static_assert(offsetof(Configuration, windowX) == 0x14, "Configuration::windowX");
static_assert(offsetof(Configuration, windowY) == 0x18, "Configuration::windowY");
static_assert(offsetof(Configuration, windowWidth) == 0x1C, "Configuration::windowWidth");
static_assert(offsetof(Configuration, windowHeight) == 0x20, "Configuration::windowHeight");
static_assert(offsetof(Configuration, multisample) == 0x24, "Configuration::multisample");
static_assert(offsetof(Configuration, textureFilter) == 0x28, "Configuration::textureFilter");
static_assert(offsetof(Configuration, shadowScalar) == 0x2C, "Configuration::shadowScalar");
static_assert(offsetof(Configuration, shaderDetail) == 0x30, "Configuration::shaderDetail");
static_assert(offsetof(Configuration, reflectionDetail) == 0x34, "Configuration::reflectionDetail");
static_assert(offsetof(Configuration, refreshNumerator) == 0x38, "Configuration::refreshNumerator");
static_assert(offsetof(Configuration, refreshDenominator) == 0x3C, "Configuration::refreshDenominator");
static_assert(offsetof(Configuration, fullscreen) == 0x40, "Configuration::fullscreen");
static_assert(offsetof(Configuration, vsync) == 0x41, "Configuration::vsync");
static_assert(offsetof(Configuration, field42) == 0x42, "Configuration::field42");

// ----------------------------------------------------------------------------
// Video::RenderBuffer — 0x30 bytes
//
// The ctor (0x1800f3ee0) zeroes +0x00..+0x17 and calls PlatformConstruct
// (0x1800f4320), which fills the intrusive list links and the platform object.
// Create (0x1800f3f10) writes +0x00, +0x08, +0x10 and +0x14 then tail-calls
// slot 1 of the object at +0x28. The size is confirmed independently by
// code/Game/Game/minimap_paint.cpp:193, which MemAllocs exactly 0x30 bytes for
// one and then calls the retail ctor on it.
//
// +0x18/+0x20 are next/prev of a CIRCULAR doubly linked list of every live
// RenderBuffer, whose head is the DLL-internal `_firstRenderBuffer`. That list
// is why this lane does not port the ctor or the dtor — see
// docs/runtime_port/WAVE6_LANE_B.md §2.
// ----------------------------------------------------------------------------
struct RenderBuffer {
    void*    colorBuffer;   // +0x00  const Video::ImageBuffer*   (Create writes)
    void*    depthBuffer;   // +0x08  const Video::ImageBuffer*   (Create writes)
    int32_t  width;         // +0x10  (Create writes)
    int32_t  height;        // +0x14  (Create writes)
    void*    next;          // +0x18  PlatformConstruct / ~RenderBuffer
    void*    prev;          // +0x20  PlatformConstruct / ~RenderBuffer
    void*    platformData;  // +0x28  RenderBufferData* — Create tail-calls its vtable slot 1
};
static_assert(sizeof(RenderBuffer) == 0x30, "RenderBuffer is 0x30 bytes (minimap_paint.cpp:193 allocates 0x30)");
static_assert(offsetof(RenderBuffer, colorBuffer) == 0x00, "RenderBuffer::colorBuffer");
static_assert(offsetof(RenderBuffer, depthBuffer) == 0x08, "RenderBuffer::depthBuffer");
static_assert(offsetof(RenderBuffer, width) == 0x10, "RenderBuffer::width");
static_assert(offsetof(RenderBuffer, height) == 0x14, "RenderBuffer::height");
static_assert(offsetof(RenderBuffer, next) == 0x18, "RenderBuffer::next");
static_assert(offsetof(RenderBuffer, prev) == 0x20, "RenderBuffer::prev");
static_assert(offsetof(RenderBuffer, platformData) == 0x28, "RenderBuffer::platformData");

// ----------------------------------------------------------------------------
// Video::ImageBuffer flag bits that RenderBuffer::Create tests (+0x34).
//
// runtime/types/video/ImageBuffer.h documents bit 0 (mip maps) only; these
// three are new to this lane and are NOT added there because that header is
// shared with Wave 6 lane C. See WAVE6_LANE_B.md §4.
// ----------------------------------------------------------------------------
enum ImageBufferSizeFlag : uint32_t {
    kImageBufferShadowScaled  = 1u << 6,  // size >>= Configuration::shadowScalar
    kImageBufferBackBuffer    = 1u << 8,  // size == the back buffer's
    kImageBufferHalfBackBuffer= 1u << 9,  // size == the back buffer's / 2
};

}  // namespace RtVideo
