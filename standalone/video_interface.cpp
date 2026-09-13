// Stop Runtime from loading VideoDX11-x64.dll, and hand it our renderer instead.
//
// This is the piece that makes Standalone mode a real replacement rather than a
// patch of the shipped DLL. Everything else in dx11/ replaces *code*; this
// replaces the DLL's role in Runtime's plugin protocol.
//
// The protocol
// ------------
// Video::Device::InitializeInterfaces (Runtime 0x1800f79f0) walks three
// interface slots on the Device singleton — DX9, DX11, GL at +0xb0, +0xc8,
// +0xe0, stride 0x18 — and for each one:
//
//     if (*(HMODULE*)(this + 0xb0 + idx*0x18) == 0) {        <-- the opening
//         LoadLibraryW(GetDLLForInterface(idx));
//         GetProcAddress("CreateDeviceData") -> slot + 0x10;
//         GetProcAddress("GetDeviceName")    -> slot + 0x08;
//         if (either is null) { FreeLibrary; zero all three; }
//     }
//
// **It skips a slot that is already populated.** So we do not have to
// reimplement it, intercept LoadLibrary, or ship a stub DLL: we only have to
// get there first. Video::Device::CreateVideoInterface (0x1800f7930) then
// requires all three words to be non-zero, sets Device+0xf8 = idx, and calls
// the CreateDeviceData pointer, storing the result at Device+0x100.
//
// Note what the HMODULE word is used for: nothing but a non-zero test, plus a
// FreeLibrary on the GetProcAddress-failed path. It is never called through.
//
// Why a detour rather than writing the fields directly
// ----------------------------------------------------
// The Device singleton does not exist before Engine::Startup — Runtime
// 0x1801fe7a8 still reads null when the address registry is being built — and
// InitializeInterfaces runs inside Startup. There is no window, which is the
// same reason the DX11 seam is the vtables and not Device+0xd0/+0xd8. So we
// fill slot 1 from inside InitializeInterfaces itself, which then skips DX11
// and handles DX9/GL exactly as before. Our ported
// ?InitializeInterfaces@Device@Video@@QEAAXXZ (0x1800f79f0,
// runtime/src/video/video_interfaces_win.cpp) calls VideoInterfaceFillDx11Slot
// as its first statement; the old byte-patch detour on the DLL's own body went
// with the DLL (ENDGAME.md A.1/A.3).

#include "resolve.h"
#include "dx11_seam.h"

#include <windows.h>
#include <cstdint>
#include <cstdio>

namespace VideoDX11 {
// dx11/src/exports.cpp — our ports of the two DLL exports.
struct DeviceData;
DeviceData* Dx11_CreateDeviceData(void* device);
const char* Dx11_GetDeviceName();
// dx11/src/devicedata.cpp -- the cutover-only DX9 placeholder (see there).
void*       Dx9Placeholder_CreateDeviceData(void* device);
const char* Dx9Placeholder_GetDeviceName();
}  // namespace VideoDX11

namespace {

// Video::Device interface-slot layout (decomp/videodx11.md "How Runtime loads it").
constexpr uintptr_t kSlot0        = 0xb0;   // DX9
constexpr uintptr_t kSlotStride   = 0x18;
constexpr uintptr_t kOffModule    = 0x00;
constexpr uintptr_t kOffGetName   = 0x08;
constexpr uintptr_t kOffCreate    = 0x10;
constexpr int       kInterfaceDX11 = 1;

bool g_filled = false;

// The non-zero word Runtime tests, and would FreeLibrary on its error path.
// A fabricated value would make that FreeLibrary undefined; our own module
// handle, with an extra reference taken for the purpose, makes it a harmless
// refcount decrement on a module that is never going away.
HMODULE SentinelModule() {
    static HMODULE h = nullptr;
    if (!h) {
        wchar_t self[MAX_PATH] = {0};
        GetModuleFileNameW(GetModuleHandleW(nullptr), self, MAX_PATH);
        h = LoadLibraryW(self);                       // +1 ref on reimpl-x64.exe
        if (!h) h = GetModuleHandleW(nullptr);        // still non-zero
    }
    return h;
}

void FillDx11Slot(void* device);

}  // namespace

// Wave 9 lane ENGIO. The slot fill, reachable WITHOUT the detour.
//
// See the correction on InstallVideoInterfacePrefill below: at cutover the
// detour is the ONLY path that reaches this, and the detour cannot exist. Whoever
// ports Video::Device::InitializeInterfaces (0x1800F79F0) must call this from
// the ported body -- that is what turns the pre-fill from a DLL-present hack
// into the real thing.
void VideoInterfaceFillDx11Slot(void* device) { FillDx11Slot(device); }

// Cutover only: slot 0 (DX9) with the placeholder plugin, so the Device's
// interface array keeps one entry per slot exactly as it does on a retail
// machine. Called by the ported InitializeInterfaces after its LoadLibraryW of
// VideoDX9-x64.dll failed, and only with the Runtime DLL absent.
void VideoInterfaceFillDx9Placeholder(void* device) {
    uint8_t* slot = static_cast<uint8_t*>(device) + kSlot0;   // slot 0 = DX9
    *reinterpret_cast<HMODULE*>(slot + kOffModule)  = SentinelModule();
    *reinterpret_cast<void**>  (slot + kOffGetName) =
        reinterpret_cast<void*>(&VideoDX11::Dx9Placeholder_GetDeviceName);
    *reinterpret_cast<void**>  (slot + kOffCreate)  =
        reinterpret_cast<void*>(&VideoDX11::Dx9Placeholder_CreateDeviceData);
    printf("[VideoInterface] DX9 slot filled with the cutover PLACEHOLDER on Device %p "
           "(VideoDX9-x64.dll cannot load without Runtime-x64.dll)\n", device);
}

namespace {

void FillDx11Slot(void* device) {
    uint8_t* slot = static_cast<uint8_t*>(device) + kSlot0
                  + kInterfaceDX11 * kSlotStride;

    *reinterpret_cast<HMODULE*>(slot + kOffModule)  = SentinelModule();
    *reinterpret_cast<void**>  (slot + kOffGetName) =
        reinterpret_cast<void*>(&VideoDX11::Dx11_GetDeviceName);
    *reinterpret_cast<void**>  (slot + kOffCreate)  =
        reinterpret_cast<void*>(&VideoDX11::Dx11_CreateDeviceData);

    g_filled = true;
    printf("[VideoInterface] DX11 slot pre-filled on Device %p: "
           "module=%p GetDeviceName=%p CreateDeviceData=%p — Runtime will not "
           "LoadLibrary VideoDX11-x64.dll\n",
           device, (void*)SentinelModule(),
           reinterpret_cast<void*>(&VideoDX11::Dx11_GetDeviceName),
           reinterpret_cast<void*>(&VideoDX11::Dx11_CreateDeviceData));
    fflush(stdout);
}

}  // namespace

// True once our pointers are actually on the Device. Reported by DumpVideoState.
bool VideoInterfacePrefilled() { return g_filled; }
