// Resolved function pointers into the shipped VideoDX11-x64.dll.
// See dx11_funcs.h for what these are for.

#include "dx11_funcs.h"

#include "resolve.h"

#include <cstdio>

fn_DX11_CreateDeviceData_t fn_DX11_CreateDeviceData = nullptr;
fn_DX11_GetDeviceName_t    fn_DX11_GetDeviceName    = nullptr;

void** g_DX11_DeviceDataVtable         = nullptr;
void** g_DX11_ContextDataVtable        = nullptr;
void** g_DX11_ImageBufferDataVtable    = nullptr;
void** g_DX11_ConstantBufferDataVtable = nullptr;
void** g_DX11_IndexBufferDataVtable    = nullptr;
void** g_DX11_VertexBufferDataVtable   = nullptr;
void** g_DX11_VertexLayoutDataVtable   = nullptr;
void** g_DX11_VertexProgramDataVtable  = nullptr;
void** g_DX11_PixelProgramDataVtable   = nullptr;
void** g_DX11_RenderStateDataVtable    = nullptr;
void** g_DX11_VideoProgramDataVtable   = nullptr;
void** g_DX11_RenderBufferDataVtable   = nullptr;

static bool s_ready = false;

bool Dx11FuncsReady() { return s_ready; }

// Resolve a Ghidra address in VideoDX11-x64.dll. Returns null if the module
// isn't loaded (ResolveInit treats it as optional).
template <typename T>
static T Dx11Addr(uintptr_t ghidra_addr) {
    const ModuleInfo* info = GetModuleInfo(Module::VideoDX11);
    if (!info || !info->handle) return nullptr;
    return reinterpret_cast<T>(ResolveAddr(Module::VideoDX11, ghidra_addr));
}

bool Dx11FuncsInit() {
    const ModuleInfo* info = GetModuleInfo(Module::VideoDX11);
    if (!info || !info->handle) {
        printf("[Dx11Funcs] VideoDX11-x64.dll not loaded — nothing resolved\n");
        return false;
    }

    fn_DX11_CreateDeviceData =
        Dx11Addr<fn_DX11_CreateDeviceData_t>(0x1800011a0);
    fn_DX11_GetDeviceName =
        Dx11Addr<fn_DX11_GetDeviceName_t>(0x180001230);

    g_DX11_DeviceDataVtable         = Dx11Addr<void**>(0x180018bb0);
    g_DX11_ContextDataVtable        = Dx11Addr<void**>(0x1800186f0);
    g_DX11_ImageBufferDataVtable    = Dx11Addr<void**>(0x180018790);
    g_DX11_ConstantBufferDataVtable = Dx11Addr<void**>(0x1800185d0);
    g_DX11_IndexBufferDataVtable    = Dx11Addr<void**>(0x1800187c8);
    g_DX11_VertexBufferDataVtable   = Dx11Addr<void**>(0x180018830);
    g_DX11_VertexLayoutDataVtable   = Dx11Addr<void**>(0x180018878);
    g_DX11_VertexProgramDataVtable  = Dx11Addr<void**>(0x180018890);
    g_DX11_PixelProgramDataVtable   = Dx11Addr<void**>(0x180018800);
    g_DX11_RenderStateDataVtable    = Dx11Addr<void**>(0x180018818);
    g_DX11_VideoProgramDataVtable   = Dx11Addr<void**>(0x1800185a8);
    g_DX11_RenderBufferDataVtable   = Dx11Addr<void**>(0x180018588);

    s_ready = (fn_DX11_CreateDeviceData != nullptr);

    // Sanity check the resolution: GetDeviceName is a two-instruction function
    // returning a literal, so calling it is safe and proves the ASLR delta.
    if (s_ready && fn_DX11_GetDeviceName) {
        const char* name = fn_DX11_GetDeviceName();
        printf("[Dx11Funcs] Resolved VideoDX11-x64.dll @ %p (delta %+lld), "
               "GetDeviceName() = \"%s\"\n",
               reinterpret_cast<void*>(info->runtimeBase),
               static_cast<long long>(info->delta),
               name ? name : "(null)");
    }

    return s_ready;
}
