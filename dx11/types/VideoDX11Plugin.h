#pragma once
// The renderer plugin's entry points — what Runtime's plugin protocol reaches
// (`Video::Device::InterfaceSlot`, runtime/types/video/Device.h) and what
// standalone/video_interface.cpp writes into it.
//
// Retail VideoDX11-x64.dll exports exactly two functions (ordinal 1
// `CreateDeviceData` 0x1800011a0, ordinal 2 `GetDeviceName` 0x180001230); their
// ports are dx11/src/exports.cpp. The other pair is the cutover-only DX9
// PLACEHOLDER (dx11/src/dx9_placeholder.cpp), which stands in for
// VideoDX9-x64.dll so the Device's GraphicsInterface array keeps one entry per
// slot (B5_RULES D5; the invariant is in that file's banner).
//
// B5 DEV-A: was a local `extern` block in standalone/video_interface.cpp (CLAUDE.md:
// the declaring header, never a local extern). Kept free of <windows.h> /
// <d3d11.h>: everything below is a pointer to a forward-declared class.

namespace Video {
class Device;
class DeviceData;
}  // namespace Video

namespace VideoDX11 {

// ---- VideoDX11-x64.dll's two exports (dx11/src/exports.cpp) ---------------
// 0x1800011a0 — CreateDeviceData(Video::Device*) -> a new VideoDX11::DeviceData,
// returned as its base: exactly `Video::Device::CreateDeviceDataFn`.
::Video::DeviceData* Dx11_CreateDeviceData(::Video::Device* device);
// 0x180001230 — GetDeviceName() -> "DirectX 11".
const char* Dx11_GetDeviceName();

// ---- the cutover-only DX9 placeholder (dx11/src/dx9_placeholder.cpp) -------
::Video::DeviceData* Dx9Placeholder_CreateDeviceData(::Video::Device* device);
const char*          Dx9Placeholder_GetDeviceName();

// ---- shared by DeviceData::EnumerateInterfaces and the placeholder ---------
// The DXGI adapter / output / display-mode walk of retail slot 1 (0x180004690),
// appending ONE `Video::Device::GraphicsInterface` named `interfaceName` to
// `device`'s interface array. Defined once, in dx11/src/devicedata.cpp; false
// when dxgi.dll / d3d11.dll's two entry points cannot be resolved (B5_RULES D9:
// the "run the original" arm it replaces had no original to run).
bool Dx11EnumerateDxgiInterfaces(::Video::Device* device, const char* interfaceName);

}  // namespace VideoDX11
