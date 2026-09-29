// ============================================================================
// runtime/src/video/device_adjustdisplay_win.cpp — Wave 18 lane GFXIF
//   ?AdjustDisplay@Device@Video@@QEAAXXZ    retail 0x1800f7e20
// ============================================================================
//
// The ninth of lane GFXIF's nine rows, and the only one that is pure Win32.
// PLAN §4.1 decision 1 keeps runtime/src/ free of <windows.h> outside a `_win`
// twin, so it lives here and device_enum_types.cpp stays portable.
//
// Offsets are repeated as literals rather than by including
// runtime/types/video/Device.h, for the reason buffers_win.cpp and
// device_startup_win.cpp both give: pulling an engine header in behind
// <windows.h> is the collision that decision exists for (<wingdi.h> declares a
// function called `Rectangle`). The static_asserts that bind these offsets live
// in the portable header.
//
// WHAT IT DOES. Re-lays the window out for the current Video::Configuration,
// then reads back the client area and publishes it as the new backbuffer size:
//
//   device->field9D = 1                      (a re-entrancy flag; cleared at the end)
//   style = GetWindowLongW(hwnd, GWL_STYLE)
//   MONITORINFO mi = {}; mi.cbSize = 0x28
//   if (device+0xA0) GetMonitorInfoW(device+0xA0, &mi)
//   if (config->fullscreen) {
//       style = (style & 0xFF30FFFF) | WS_POPUP
//       SetWindowLongW(hwnd, GWL_STYLE, style)
//       SetWindowPos(hwnd, NULL, mi.rcMonitor.left, mi.rcMonitor.top,
//                    device->backBufferWidth, device->backBufferHeight,
//                    SWP_FRAMECHANGED | SWP_NOACTIVATE)
//   } else {
//       ... the windowed branch, below ...
//   }
//   device->field9D = 0
//
// EVERY IMPORT SLOT WAS RESOLVED BY NAME, NOT GUESSED FROM THE CALL SHAPE.
// pefile against the shipped DLL's import table:
//   0x18014b510 GetWindowLongW      0x18014b500 SetWindowLongW
//   0x18014b508 GetMonitorInfoW     0x18014b4f8 SetWindowPos
//   0x18014b4f0 GetWindowPlacement  0x18014b4e8 AdjustWindowRect
//   0x18014b4e0 MonitorFromRect     0x18014b4d8 GetDesktopWindow
//   0x18014b5a0 GetWindowRect       0x18014b558 GetClientRect
// Note GetWindowLong**W**, not GetWindowLongPtrW: the style travels as a 32-bit
// LONG throughout, which is why every mask below is 32-bit.
//
// TWO CORRECTIONS TO runtime/types/video/Device.h, BOTH FROM THIS BODY
// -------------------------------------------------------------------
//  * Device+0xA0 is an HMONITOR. Device.h currently covers it with
//    `gap9E[0x12]` and records only "the ctor zeroes +0xa0 and +0xa8". This
//    body loads it and passes it straight to GetMonitorInfoW (0x1800f7e68),
//    which pins it.
//  * Adapter+0x2C and +0x30 are NOT unrecovered. Device.h says "meaning not
//    recovered" for both; dx11/src/devicedata.cpp:485-486 already names them
//    dxgiAdapterIndex and dxgiOutputIndex, written by the shipped DX11
//    EnumerateInterfaces. Recorded here because this lane read both files.
// Neither correction changes a byte of layout, so nothing is edited in the
// header by this lane; see docs/runtime_port/WAVE18_LANE_GFXIF.md.
//
// A RETAIL DEFECT, REPRODUCED DELIBERATELY
// ----------------------------------------
// In the windowed branch the adjusted RECT is offset by the persisted window
// origin, and retail adds the X offset to THREE of the four edges:
//
//   0x1800f7f84  [rect2.left]   = rect.left   + EDI      (EDI = windowX)
//   0x1800f7f8c  [rect2.right]  = rect.right  + EDI
//   0x1800f7f94  [rect2.top]    = rect.top    + ESI      (ESI = windowY)
//   0x1800f7f9c  [rect2.bottom] = rect.bottom + EDI      <-- EDI, not ESI
//
// `bottom` is displaced horizontally. The rectangle is used only as the
// argument to MonitorFromRect, so the consequence is a monitor lookup against a
// slightly wrong box — harmless whenever windowX and windowY are close, and
// invisible on a single-monitor desktop. It is reproduced rather than fixed:
// this port's job is to be the shipped behaviour, and a "corrected" version
// would pick a different monitor from retail in exactly the multi-monitor case
// where anyone would notice.
// ============================================================================

#include <windows.h>
#include <cstdint>
#include <cstring>

#include "../../types/video/Device.h"   // ENDGAME B.3.9 VID: Video::Configuration::_singleton

static const size_t kDeviceBackBufferWidth  = 0x04;
static const size_t kDeviceBackBufferHeight = 0x08;
static const size_t kDeviceWindowHandle     = 0x90;
static const size_t kDeviceMonitor          = 0xA0;
static const size_t kDeviceField9D          = 0x9D;

static const size_t kConfigWindowX      = 0x14;
static const size_t kConfigWindowY      = 0x18;
static const size_t kConfigWindowWidth  = 0x1C;
static const size_t kConfigWindowHeight = 0x20;
static const size_t kConfigFullscreen   = 0x40;

namespace {
uint8_t* VideoConfig() {
    void** slot = reinterpret_cast<void**>(&::Video::Configuration::_singleton);
    return slot ? static_cast<uint8_t*>(*slot) : nullptr;
}
}  // namespace

extern "C" void RtVideo_Device_AdjustDisplay(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);

    // The re-entrancy flag. SizeWindow/MoveWindow read it to tell an
    // application-driven resize from this one; set before the first Win32 call
    // and cleared on BOTH exits (0x1800f7e37 and 0x1800f8088).
    *(self + kDeviceField9D) = 1;

    HWND hwnd = *reinterpret_cast<HWND*>(self + kDeviceWindowHandle);
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);

    MONITORINFO mi;
    std::memset(&mi, 0, 0x28);
    mi.cbSize = 0x28;
    HMONITOR mon = *reinterpret_cast<HMONITOR*>(self + kDeviceMonitor);
    if (mon) GetMonitorInfoW(mon, &mi);

    uint8_t* cfg = VideoConfig();

    if (*(cfg + kConfigFullscreen) != 0) {
        // ---- fullscreen: borderless, pinned to the monitor origin ----------
        // 0xFF30FFFF clears the whole WS_* style nibble pair that carries the
        // caption and border bits; WS_POPUP (0x80000000) is then set.
        style = static_cast<LONG>(
            (static_cast<uint32_t>(style) & 0xFF30FFFFu) | 0x80000000u);
        SetWindowLongW(hwnd, GWL_STYLE, style);

        SetWindowPos(hwnd, nullptr,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     *reinterpret_cast<int32_t*>(self + kDeviceBackBufferWidth),
                     *reinterpret_cast<int32_t*>(self + kDeviceBackBufferHeight),
                     0x30 /* SWP_FRAMECHANGED | SWP_NOACTIVATE */);
        *(self + kDeviceField9D) = 0;
        return;
    }

    // ---- windowed ---------------------------------------------------------
    // 0xCF0000 == WS_OVERLAPPEDWINDOW. OR'd in, not assigned: whatever else the
    // style carries survives.
    style = static_cast<LONG>(static_cast<uint32_t>(style) | 0x00CF0000u);
    SetWindowLongW(hwnd, GWL_STYLE, style);

    // The desired CLIENT rect, at the origin.
    RECT rect;
    rect.left   = 0;
    rect.top    = 0;
    rect.right  = *reinterpret_cast<int32_t*>(self + kDeviceBackBufferWidth);
    rect.bottom = *reinterpret_cast<int32_t*>(self + kDeviceBackBufferHeight);

    WINDOWPLACEMENT wp;
    wp.length = 0x2C;
    GetWindowPlacement(hwnd, &wp);

    // Retail tests showCmd against two ordinary SW_ values and two large
    // constants; all four are kept because only the DLL knows why the large
    // ones are there, and dropping a disjunct changes which branch runs.
    const UINT showCmd = wp.showCmd;
    const bool maximized = (showCmd == 0x1000000u || showCmd == 3u);   // SW_MAXIMIZE
    const bool minimized = (showCmd == 0x20000000u || showCmd == 2u);  // SW_MINIMIZE

    // Grow the client rect into a window rect for this style.
    AdjustWindowRect(&rect, static_cast<DWORD>(style), FALSE);

    // A maximized window is placed at the origin; otherwise at the persisted
    // window position.
    LONG offX = 0, offY = 0;
    if (!maximized) {
        offX = *reinterpret_cast<int32_t*>(cfg + kConfigWindowX);
        offY = *reinterpret_cast<int32_t*>(cfg + kConfigWindowY);
    }

    // The probe rectangle handed to MonitorFromRect. `bottom` takes offX, not
    // offY — retail's, see the banner.
    RECT probe;
    probe.left   = rect.left   + offX;
    probe.right  = rect.right  + offX;
    probe.top    = rect.top    + offY;
    probe.bottom = rect.bottom + offX;

    if (MonitorFromRect(&probe, 0 /* MONITOR_DEFAULTTONULL */) == nullptr) {
        // The window would land off every monitor: centre it on the desktop.
        RECT desk;
        GetWindowRect(GetDesktopWindow(), &desk);

        // Retail's halving is SAR 1 on a CDQ-corrected difference, i.e. a
        // signed divide by two that rounds toward zero.
        offX = ((desk.right - desk.left) / 2) - ((rect.right - rect.left) / 2);
        offY = ((desk.bottom - desk.top) / 2) - ((rect.bottom - rect.top) / 2);

        // A minimized window keeps its persisted origin — the centred values
        // are used for THIS placement but are not written back.
        if (!minimized) {
            *reinterpret_cast<int32_t*>(cfg + kConfigWindowX) = offX;
            *reinterpret_cast<int32_t*>(cfg + kConfigWindowY) = offY;
        }
    }

    SetWindowPos(hwnd, HWND_NOTOPMOST,
                 offX, offY,
                 rect.right - rect.left,
                 rect.bottom - rect.top,
                 0x30 /* SWP_FRAMECHANGED | SWP_NOACTIVATE */);

    // Read the client area BACK — SetWindowPos may not have granted what was
    // asked — and publish it as the backbuffer size and the persisted window
    // size. This readback is why AdjustDisplay is on the boot path at all.
    RECT client;
    GetClientRect(hwnd, &client);
    const int32_t cw = client.right - client.left;
    const int32_t ch = client.bottom - client.top;

    *reinterpret_cast<int32_t*>(self + kDeviceBackBufferHeight) = ch;
    *reinterpret_cast<int32_t*>(self + kDeviceBackBufferWidth)  = cw;
    *reinterpret_cast<int32_t*>(cfg + kConfigWindowWidth)  = cw;
    *reinterpret_cast<int32_t*>(cfg + kConfigWindowHeight) = ch;

    *(self + kDeviceField9D) = 0;
}
