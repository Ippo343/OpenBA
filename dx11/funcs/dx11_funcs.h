#pragma once
// Resolved function pointers into the shipped VideoDX11-x64.dll.
//
// These exist so partially-ported code can call back into the original
// implementation (the "delegating DeviceData" pattern — see ../README.md).
// Every slot we have not reimplemented yet forwards here.
//
// Runtime-side calls (Video::Device::Lock, Video::ImageBuffer::Width, ...) do
// NOT belong in this file — they go in engine/funcs/engine_funcs.h per the
// existing project convention.
//
// Resolved by Dx11FuncsInit() after ResolveInit() succeeds. VideoDX11-x64.dll
// is optional in ResolveInit(), so Dx11FuncsInit() returns false (rather than
// crashing) when the DLL is absent.
//
// NOTE: Runtime-x64.dll and VideoDX11-x64.dll share preferred base
// 0x180000000. Always resolve through Module::VideoDX11 — a bare address is
// ambiguous between the two.

#include <cstdint>

// ============================================================
// The two exports — the entire plugin ABI
// ============================================================

// CreateDeviceData(Video::Device*) -> Video::DeviceData*   (ordinal 1, 0x1800011a0)
typedef void* (*fn_DX11_CreateDeviceData_t)(void* device);
extern fn_DX11_CreateDeviceData_t fn_DX11_CreateDeviceData;

// GetDeviceName() -> "DirectX 11"                          (ordinal 2, 0x180001230)
typedef const char* (*fn_DX11_GetDeviceName_t)(void);
extern fn_DX11_GetDeviceName_t fn_DX11_GetDeviceName;

// ============================================================
// The original DeviceData vtable (20 slots @ 0x180018bb0)
//
// Resolved as a pointer to the vtable array itself, so a delegating
// DeviceData can forward any slot by index without naming each one:
//
//     ((void(*)(void*))g_DX11_DeviceDataVtable[kSlot_Shutdown])(realDeviceData);
//
// Slot names are in dx11/types/VideoDX11Data.h (enum DeviceDataSlot).
// ============================================================
extern void** g_DX11_DeviceDataVtable;

// ============================================================
// The 11 resource-class vtables, for the same forwarding trick.
// Indexed by DeviceDataSlot order (slot 9..19). Null until Dx11FuncsInit().
// ============================================================
extern void** g_DX11_ContextDataVtable;         // 0x1800186f0
extern void** g_DX11_ImageBufferDataVtable;     // 0x180018790
extern void** g_DX11_ConstantBufferDataVtable;  // 0x1800185d0
extern void** g_DX11_IndexBufferDataVtable;     // 0x1800187c8
extern void** g_DX11_VertexBufferDataVtable;    // 0x180018830
extern void** g_DX11_VertexLayoutDataVtable;    // 0x180018878
extern void** g_DX11_VertexProgramDataVtable;   // 0x180018890
extern void** g_DX11_PixelProgramDataVtable;    // 0x180018800
extern void** g_DX11_RenderStateDataVtable;     // 0x180018818
extern void** g_DX11_VideoProgramDataVtable;    // 0x1800185a8
extern void** g_DX11_RenderBufferDataVtable;    // 0x180018588

// Resolve everything above. Call after ResolveInit().
// Returns false if VideoDX11-x64.dll is not loaded (all pointers stay null).
bool Dx11FuncsInit();

// True once Dx11FuncsInit() has succeeded.
bool Dx11FuncsReady();
