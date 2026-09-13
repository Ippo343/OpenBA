// The two VideoDX11-x64.dll exports — the entire plugin ABI.
//
//     Ordinal 1  CreateDeviceData  0x1800011a0  Video::DeviceData* (Video::Device*)
//     Ordinal 2  GetDeviceName     0x180001230  const char* ()  -> "DirectX 11"
//
// These are the last two pieces of shipped renderer code that ran in the
// process. They are not reachable from any vtable: Runtime's
// Video::Device::InitializeInterfaces GetProcAddress'es them into
// Video::Device+0xd8 / +0xd0 and Video::Device::CreateVideoInterface calls
// through those fields, both inside a single Engine::Startup call. So they are
// replaced at the function entry instead, by a seam detour — see the
// "Detours" section of dx11/funcs/dx11_seam.h.
//
// CreateDeviceData in particular ran on EVERY boot, because it is what
// allocates the DeviceData whose vtable the rest of the seam then patches.
// Porting it moves that allocation to us; the object it produces is
// byte-identical, so the vtable patching is unaffected either way.

#include "dx11_backend_access.h"
#include "dx11_seam.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace VideoDX11 {
namespace {

// ============================================================
// Imports (two of them — this file is deliberately self-contained)
// ============================================================
struct ExportImports {
    void* (*MemAlloc)(size_t, int);            // IAT 0x1800134c0
    void* (*CtorDeviceData)(void*, void*);     // IAT 0x180013220
};

ExportImports g_im;
bool g_imResolved = false;
bool g_imOk       = false;

const ExportImports* GetExportImports() {
    if (g_imResolved) return g_imOk ? &g_im : nullptr;
    g_imResolved = true;

    g_im.MemAlloc = reinterpret_cast<void* (*)(size_t, int)>(
        Dx11ResolveRuntimeImport("?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z",
                                 0x1800134c0));
    // Slot 0x180013220 -> import-by-name RVA 0x1c602, confirmed against
    // objdump's listing for the shipped DLL. Its neighbour 0x180013228 is
    // ??1DeviceData, which devicedata.cpp already resolves.
    g_im.CtorDeviceData = reinterpret_cast<void* (*)(void*, void*)>(
        Dx11ResolveRuntimeImport("??0DeviceData@Video@@QEAA@PEAVDevice@1@@Z",
                                 0x180013220));

    g_imOk = g_im.MemAlloc && g_im.CtorDeviceData;
    if (!g_imOk) {
        fprintf(stderr, "[Dx11] exports: Runtime imports unresolved "
                        "(MemAlloc=%p CtorDeviceData=%p)\n",
                reinterpret_cast<void*>(g_im.MemAlloc),
                reinterpret_cast<void*>(g_im.CtorDeviceData));
    }
    return g_imOk ? &g_im : nullptr;
}

}  // namespace

// ============================================================
// 0x1800011a0 — CreateDeviceData(Video::Device* device) -> Video::DeviceData*
//
//     ECX = 0x70, EDX = 4   (LEA ECX,[RDX+0x6c] after MOV EDX,4)
//     p = System::MemoryAllocator::MemAlloc(0x70, 4)
//     if (!p) return NULL                         ; JZ over everything
//     Video::DeviceData::DeviceData(p, device)    ; Runtime base ctor, fills
//                                                 ; vptr and +0x08 owner
//     p->vptr = 0x180018bb0                       ; overwrite with the DX11 one
//     zero the DX11-private tail
//     return p
//
// Two details the field-by-field assignment below preserves and a memset would
// not:
//   * +0x40 is written as a single BYTE (MOV byte [RBX+0x40],AL). +0x41..+0x47
//     keep whatever the base ctor left there.
//   * +0x58 is written as a QWORD -1 (MOV qword [RBX+0x58],-1), which sets
//     BOTH cursorHotspotX and cursorHotspotY to -1 — not just the first.
// ============================================================
DeviceData* Dx11_CreateDeviceData(void* device) {
    const ExportImports* im = GetExportImports();
    if (!im) {
        // No forwarding target exists here: the detour has already replaced the
        // original, so there is nothing to fall back to. Failing to allocate is
        // what the original does when MemAlloc returns null, and Runtime
        // handles it (Video::Device::CreateVideoInterface null-checks).
        fprintf(stderr, "[Dx11] CreateDeviceData: imports unresolved, returning null\n");
        return nullptr;
    }

    DeviceData* p = static_cast<DeviceData*>(im->MemAlloc(0x70, 4));
    if (!p) return nullptr;

    im->CtorDeviceData(p, device);
    p->vptr = Dx11VtableAddress(Vt::DeviceData);

    p->unk10          = nullptr;
    p->swapChain      = nullptr;
    p->d3dDevice      = nullptr;
    p->d3dContext     = nullptr;
    p->depthBuffer    = nullptr;
    p->colorBuffer    = nullptr;
    p->wasFullscreen  = 0;             // byte only — pad41 untouched, as shipped
    p->cursor         = nullptr;
    p->cursorImage    = nullptr;
    p->cursorHotspotX = -1;            // the qword -1 at +0x58 covers both
    p->cursorHotspotY = -1;
    p->trackedImages  = nullptr;
    p->trackedCount   = 0;
    p->trackedCapacity = 0;

    return p;
}

// ============================================================
// 0x180001230 — GetDeviceName() -> "DirectX 11"
//
// Our own literal. An earlier revision returned the shipped DLL's .rdata string
// at 0x1800185c0 to keep the returned POINTER identical as well as the bytes;
// that is not available in Standalone mode, where the DLL is never mapped.
// Nothing compares the address — Runtime copies the result into a System::String
// — so the bytes are the whole of the contract.
//
// The retired Dx11FuncsInit used to cross-check against 0x1800185c0 when the DLL happened to
// be loaded, which is where a divergence would show up.
// ============================================================
const char* Dx11_GetDeviceName() {
    return "DirectX 11";
}

}  // namespace VideoDX11

// Neither export is registered with the seam. Runtime reaches both through
// Video::Device+0xd8 / +0xd0, and standalone/video_interface.cpp writes our
// function pointers straight into those fields before Runtime would otherwise
// LoadLibrary the shipped DLL. They used to be installed as inline detours over
// the DLL's own entry points; that was the AB-mode arrangement and went with it.
