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
// interpose InitializeInterfaces itself: fill slot 1, then chain to the
// original, which now skips DX11 and handles DX9/GL exactly as before.
//
// Chaining rather than replacing is deliberate. It keeps the delta to "DX11 is
// no longer loaded" instead of also changing whether DX9 and GL are probed,
// which they are today.

#include "runtime_detour.h"
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
}  // namespace VideoDX11

namespace {

// Video::Device interface-slot layout (decomp/videodx11.md "How Runtime loads it").
constexpr uintptr_t kSlot0        = 0xb0;   // DX9
constexpr uintptr_t kSlotStride   = 0x18;
constexpr uintptr_t kOffModule    = 0x00;
constexpr uintptr_t kOffGetName   = 0x08;
constexpr uintptr_t kOffCreate    = 0x10;
constexpr int       kInterfaceDX11 = 1;

// ?InitializeInterfaces@Device@Video@@QEAAXXZ, ordinal 3073.
constexpr uintptr_t kInitializeInterfaces = 0x1800f79f0;

// Prologue, read out of the mapped image (NOT transcribed from a mnemonic
// listing — MSVC emits a redundant REX prefix, so PUSH RBP is 40 55):
//
//   1800f79f0  40 55                 PUSH RBP
//   1800f79f2  56                    PUSH RSI
//   1800f79f3  57                    PUSH RDI
//   1800f79f4  48 83 EC 30           SUB  RSP,0x30
//   1800f79f8  48 C7 44 24 20 FE FF FF FF   MOV qword [RSP+0x20],-2
//
// Boundaries at +0, +2, +3, +4, +8, +17. 17 is the first at or past the 12
// bytes an absolute jump needs, so that is the patch length.
const uint8_t kInitializeInterfacesPrologue[] = {
    0x40, 0x55,
    0x56,
    0x57,
    0x48, 0x83, 0xEC, 0x30,
    0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF
};
constexpr int kInitializeInterfacesPatchLen =
    static_cast<int>(sizeof(kInitializeInterfacesPrologue));

typedef void (*InitializeInterfacesFn)(void* device);
InitializeInterfacesFn g_origInitializeInterfaces = nullptr;

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

void WrapInitializeInterfaces(void* device) {
    FillDx11Slot(device);
    if (g_origInitializeInterfaces) g_origInitializeInterfaces(device);
}

}  // namespace

// Install the pre-fill. Call after Dx11SeamInstall() (so our vtables exist)
// and before Engine::Startup (so we beat InitializeInterfaces).
void InstallVideoInterfacePrefill() {
    // RuntimeDetourInstall takes the GHIDRA address and resolves it RAW itself
    // (Wave 9 lane FNHOOK) — handing it a ResolveAddr result aims the patch at
    // our own body the day 0x1800f79f0 gets ported, and the byte guard then
    // refuses silently.
    g_origInitializeInterfaces = reinterpret_cast<InitializeInterfacesFn>(
        RuntimeDetourInstall(kInitializeInterfaces, reinterpret_cast<void*>(&WrapInitializeInterfaces),
                             kInitializeInterfacesPatchLen,
                             kInitializeInterfacesPrologue,
                             kInitializeInterfacesPatchLen,
                             "Video::Device::InitializeInterfaces"));

    if (!g_origInitializeInterfaces) {
        fprintf(stderr,
                "[VideoInterface] *** DETOUR FAILED *** Runtime will LoadLibrary "
                "VideoDX11-x64.dll and, if it is missing, silently fall back to "
                "VideoDX9-x64.dll. See dx11/README.md.\n");
        fflush(stderr);
    }
}

// True once our pointers are actually on the Device. Reported by DumpVideoState.
bool VideoInterfacePrefilled() { return g_filled; }
