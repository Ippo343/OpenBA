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

// ============================================================
// The original prologues, for the detour's byte check
// ============================================================
//
// 1800011a0  40 57                    PUSH RDI
// 1800011a2  48 83 EC 30              SUB  RSP,0x30
// 1800011a6  48 C7 44 24 20 FE FF FF FF   MOV qword [RSP+0x20],-2
//                                          (the MSVC EH state slot)
// 15 bytes of prologue before anything we would clip; the detour needs 12.
//
// Note the 0x40: a REX prefix with no bits set, which encodes nothing. MSVC
// emits it here anyway, so PUSH RDI is two bytes, not one. Ghidra's listing
// shows the mnemonic and not the encoding, and taking the encoding from the
// mnemonic put every byte after it one position early — the detour's byte check
// refused the patch instead of clipping the function, which is what it is for.
const uint8_t kCreateDeviceDataPrologue[12] = {
    0x40, 0x57,
    0x48, 0x83, 0xEC, 0x30,
    0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE
};

// 180001230  48 8D 05 89 73 01 00     LEA RAX,[0x1800185c0]   ; "DirectX 11"
// 180001237  C3                       RET
// 180001238  CC CC CC CC CC CC CC CC  alignment padding
// The whole function is 8 bytes, so the detour's last 4 bytes land in padding.
// The padding is included in the check precisely so that "did we just clip a
// neighbouring function?" is answered, not assumed.
const uint8_t kGetDeviceNamePrologue[12] = {
    0x48, 0x8D, 0x05, 0x89, 0x73, 0x01, 0x00,
    0xC3,
    0xCC, 0xCC, 0xCC, 0xCC
};

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
// Dx11FuncsInit still cross-checks against 0x1800185c0 when the DLL happens to
// be loaded, which is where a divergence would show up.
// ============================================================
const char* Dx11_GetDeviceName() {
    return "DirectX 11";
}

}  // namespace VideoDX11

DX11_REGISTER_DETOUR(CreateDeviceData, 0x1800011a0,
                     &::VideoDX11::Dx11_CreateDeviceData,
                     ::VideoDX11::kCreateDeviceDataPrologue,
                     "BANISHED_DX11_EXPORTS", true,"CreateDeviceData")

DX11_REGISTER_DETOUR(GetDeviceName, 0x180001230,
                     &::VideoDX11::Dx11_GetDeviceName,
                     ::VideoDX11::kGetDeviceNamePrologue,
                     "BANISHED_DX11_EXPORTS", true,"GetDeviceName")
