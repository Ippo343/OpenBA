// VideoDX11 DeviceData — vtable 0x180018bb0, slots 0/1/2/3/8 and the eleven
// resource factories 9..19 (lane L6). Slots 4-7 are the one-liner stubs and
// live in devicedata_stubs.cpp; this file does not touch them.
//
// This is the platform boundary itself: slot 2 is the only place in the process
// that calls D3D11CreateDevice / IDXGIFactory1::CreateSwapChain, and slot 3 is
// the only place that tears them down. Slots 2 and 3 are also the two functions
// that can leave the user with no window, so everything below is a transcription
// of the disassembly rather than a reconstruction of intent — including the
// parts that look like bugs (see "Original quirks reproduced" at the bottom).
//
// ---------------------------------------------------------------------------
// The two-globals trap, and a correction to decomp/videodx11.md
// ---------------------------------------------------------------------------
// Ghidra renders THREE different singletons as `_singleton_exref`:
//
//   VideoDX11 IAT 0x180013310  ?_singleton@Application@1@0PEAV11@EA
//   VideoDX11 IAT 0x180013480  ?_singleton@Configuration@Video@@0PEAV12@EA
//   VideoDX11 IAT 0x1800134b8  ?_singleton@Device@Video@@0PEAV12@EA
//
// decomp/videodx11.md conflated the first two with `Video::Device` in three
// places. All three are corrected here, each against the instruction:
//
//   * "DeviceData+0x40 is mirrored into Application::_singleton+0x40 on
//     shutdown" — it is mirrored into **Video::Configuration+0x40**
//     (0x18000520f loads 0x180013480, not 0x180013310).
//   * "DXGI_SWAP_CHAIN_DESC.Windowed = Device+0x40 == 0" — it is
//     **Configuration+0x40 == 0** (0x1800045af loads 0x180013480).
//     Device+0x40 is not a flag at all: it is the base pointer of the
//     Device::GraphicsInterface array, which slot 1 fills (0x180004dca).
//   * "ResolveMultisampleMode clamps Device+0x24" — it clamps
//     **Configuration+0x24** (0x18000451f loads 0x180013480).
//
// Getting any of these wrong reads a flag out of a pointer field.
//
// ---------------------------------------------------------------------------
// Video::Configuration field map (Runtime), recovered for this lane
// ---------------------------------------------------------------------------
// Every offset below is proven twice: by the registry key it is loaded from in
// Video::Configuration::Startup (Runtime 0x1800f6680, which does
// String::Format("%s%s", "Video", <suffix>) then a Persistence map lookup), and
// by the default the ctor writes (Runtime 0x1800f6f10).
//
//   off    registry key             ctor default
//   -----  -----------------------  ------------
//   +0x00  VideoSettings (bool)     true
//   +0x04  VideoInterface           1
//   +0x08  VideoAdapter             0
//   +0x0c  VideoWidth               -1
//   +0x10  VideoHeight              -1
//   +0x14  VideoWindowX             -1
//   +0x18  VideoWindowY             -1
//   +0x1c  VideoWindowWidth         -1      <- the "skip override" sentinel
//   +0x20  VideoWindowHeight        -1      <- the "skip override" sentinel
//   +0x24  VideoMultisample         1
//   +0x28  VideoTextureFilter       0
//   +0x2c  VideoShadowScalar        1
//   +0x30  VideoShaderDetail        2
//   +0x34  VideoReflectionDetail    1
//   +0x38  VideoRefreshN            60
//   +0x3c  VideoRefreshD            1
//   +0x40  VideoFullscreen (bool)   true
//   +0x41  VideoVSync (bool)        true    (L5 already uses this as SyncInterval)
//   +0x42  not persisted            false
//
// ---------------------------------------------------------------------------
// COM vtable indices used by this file
// ---------------------------------------------------------------------------
// Index counted from QueryInterface = 0, INCLUDING inherited methods. Every one
// was read off the call site's displacement in the disassembly and then checked
// against mingw-w64's d3d11.h / dxgi.h, not assumed from either alone.
//
//   interface           idx  byte   method                        call site
//   ------------------  ---  -----  ----------------------------  ----------
//   IUnknown              2  0x010  Release                       many
//   IDXGIFactory1        12  0x060  EnumAdapters1                 0x180004e72
//   IDXGIFactory          8  0x040  MakeWindowAssociation         0x18000507b
//   IDXGIFactory         10  0x050  CreateSwapChain               0x180005091
//   IDXGIAdapter          7  0x038  EnumOutputs                   0x180004ead
//   IDXGIAdapter1        10  0x050  GetDesc1                      0x180004760
//   IDXGIOutput           7  0x038  GetDesc                       0x180004f52
//   IDXGIOutput           8  0x040  GetDisplayModeList            0x18000491a
//   IDXGISwapChain       10  0x050  SetFullscreenState            0x180005271
//   IDXGISwapChain       13  0x068  ResizeBuffers                 0x180004493
//   IDXGISwapChain       14  0x070  ResizeTarget                  0x180004464
//   ID3D11Device          5  0x028  CreateTexture2D               0x180005332
//   ID3D11Device         29  0x0e8  CheckFormatSupport            0x18000500a
//   ID3D11Device         30  0x0f0  CheckMultisampleQualityLevels 0x180004bac
//   ID3D11DeviceContext  33  0x108  OMSetRenderTargets            0x18000438e
//   ID3D11DeviceContext 111  0x378  ClearState                    0x18000439b
//
// The IDXGIAdapter1 index 10 for GetDesc1 is the one worth spelling out:
// IDXGIAdapter contributes EnumOutputs(7)/GetDesc(8)/CheckInterfaceSupport(9)
// on top of IDXGIObject's 3..6, so GetDesc1 lands at 10 = +0x50. Slot 1 calls
// +0x50 on the object EnumAdapters1 produced, which is consistent.
//
// ---------------------------------------------------------------------------
// Runtime import IAT map
// ---------------------------------------------------------------------------
// VideoDX11's Runtime import thunks are one contiguous run. The base is
// 0x180013218: index N sits at 0x180013218 + N*8, in the order objdump -p
// prints the Runtime-x64.dll import name table. That base was DERIVED, not
// assumed — it is the unique value consistent with all six IAT addresses
// earlier lanes had already pinned independently (?Lock@Device = 0x180013408,
// ?_singleton@Configuration = 0x180013480, ??1ContextData = 0x180013470,
// ?_singleton@Device = 0x1800134b8, ?MemAlloc = 0x1800134c0,
// ?MemFree = 0x1800134d0), and it additionally predicts all eleven base-ctor
// slots this file needs, each of which matches the CALL displacement in the
// corresponding factory. That is 17 independent confirmations of one constant.

#include "dx11_seam.h"
#include "stall_debug.h"
#include "boot_window.h"
#include "dx11_backend_access.h"
#include "VideoDX11Data.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "system/String.h"   // -Iruntime/types
#include "system/Error.h"    // ENDGAME B.3.4b System callers: System::Error
#include "system/WideString.h"  // ENDGAME B.3.4b System callers: System::WideString
// ENDGAME B.3.5 block D2 / B35_FILEMAP §3f: the real `Video::Device` and its four
// nested value types, replacing this file's four local name-and-layout mirrors.
#include "video/Device.h"
// RULE B5-3: the real `Video::DeviceData` abstract base, whose destructor the
// scalar deleting dtor calls by qualified name.
#include "video/CompanionData.h"
#include "application/Application.h"   // -Iruntime/types: ENDGAME B.3.7 callers (K5)
#include <new>

namespace VideoDX11 {
namespace {

// ============================================================
// Runtime-x64.dll imports
//
// Same policy as L3/L5: resolve lazily by mangled name with the VideoDX11 IAT
// slot as a fallback, and if ANY of them is missing, every slot in this file
// forwards to the shipped implementation rather than dereferencing a null.
// ============================================================
struct Imports {
    // --- object lifetime -------------------------------------------------
    void* (*MemAlloc)(size_t, int);                    // 0x1800134c0
    void  (*MemFree)(void*);                           // 0x1800134d0
    // ENDGAME B.3.5 block J: `??1DeviceData@Video@@UEAA@XZ` and its table row are
    // GONE. Block D2 made slot 0 call `~DeviceData()` by qualified non-virtual
    // name and kept the row so `g_imOk` would still fail when the Runtime side was
    // absent. It no longer can be absent for THIS binding: a member call is
    // resolved by the LINKER, which is a stronger guarantee than a run-time null
    // test, so the row was testing a symbol nobody reads. Every row that is still
    // resolved at run time is still in the all-or-nothing test below, and
    // `Dx11SeamInstall()`'s completeness check is untouched (RULE B5-8).

    // --- the eleven base ctors, in DeviceData slot 9..19 order -----------
    void* (*CtorContextData)(void*, void*);            // 0x180013478
    void* (*CtorImageBufferData)(void*, void*);        // 0x1800133e8
    void* (*CtorConstantBufferData)(void*, void*);     // 0x180013490
    void* (*CtorIndexBufferData)(void*, void*);        // 0x180013390
    void* (*CtorVertexBufferData)(void*, void*);       // 0x180013340
    void* (*CtorVertexLayoutData)(void*, void*);       // 0x180013330
    void* (*CtorVertexProgramData)(void*, void*);      // 0x180013320
    void* (*CtorPixelProgramData)(void*, void*);       // 0x180013360
    void* (*CtorRenderStateData)(void*, void*);        // 0x180013350
    void* (*CtorVideoProgramData)(void*, void*);       // 0x1800134a0
    void* (*CtorRenderBufferData)(void*, void*);       // 0x1800134b0

    // --- Video::Device ---------------------------------------------------
    // ENDGAME B.3.5 block J, RULE B5-14: ALL EIGHT ROWS ARE GONE. `Video::Device`
    // is a real class (runtime/types/video/Device.h) and every one of them is a
    // direct member call now. `GetDisplayMode` is the by-value one and is
    // converted in the ONLY shape B5-12 allows for a caller — "the door body
    // moved to the call site": one placement-new of the member's prvalue into the
    // same caller storage the retail retslot pointed at, and nothing else.
    //   ?Lock@                       -> Video::Device::Lock
    //   ?Unlock@                     -> Video::Device::Unlock
    //   ?ReleaseDynamic@             -> Video::Device::ReleaseDynamic
    //   ?RecreateDynamic@            -> Video::Device::RecreateDynamic
    //   ?AdjustDisplay@              -> Video::Device::AdjustDisplay
    //   ?GetDisplayMode@             -> Video::Device::GetDisplayMode  (by value)
    //   ?GetMultisampleModeCount@    -> Video::Device::GetMultisampleModeCount
    //   ?IsMultisampleModeSupported@ -> Video::Device::IsMultisampleModeSupported

    // --- Video::Device::Adapter / GraphicsInterface / DisplayMode --------
    // THE SEVEN CTOR/DTOR ROWS STAY (B4b-2, B35_RULES section 2.4 and 11): none
    // of these four nested value types has a real C++ constructor or destructor —
    // Device.h records why per type — so there is no member to call. Only
    // `Adapter::FindOrAddMode`, which IS a real member, converted.
    void* (*AdapterCtor)(void*);                       // 0x180013250
    void* (*AdapterCopyCtor)(void*, const void*);      // 0x1800132b8
    void  (*AdapterDtor)(void*);                       // 0x1800134c8
    void* (*GfxIfaceCtor)(void*);                      // 0x1800132d0
    void* (*GfxIfaceCopyCtor)(void*, const void*);     // 0x1800132b0
    void  (*GfxIfaceDtor)(void*);                      // 0x1800132c8
    void* (*DisplayModeCtor)(void*, int, int);         // 0x180013308
    void  (*DisplayModeDtor)(void*);                   // 0x1800132f8

    // --- System::String / WideString -------------------------------------
    // ENDGAME B.3.1 step 2: the six System::String imports (0x180013248,
    // 0x180013218, 0x1800132c0, 0x180013260, 0x180013270, 0x180013268) and
    // Format (0x180013288) are gone — System::String is a real class in this
    // build and the plugin calls its members directly.
    // ENDGAME B.3.4b closing: the WideString ctor / dtor / StrLength imports
    // (0x1800132e0, 0x1800132d8, 0x180013278) are gone with their call sites —
    // System::WideString is a real class here too.
    //
    // ENDGAME B.3.5 block J: `?AsUTF8@WideString@System@@` IS GONE TOO, and the
    // reason this row gave for staying was stale. It said the member form "cannot
    // reproduce" a hidden retslot written into a String the caller had already
    // constructed — but the caller only default-constructed that String because
    // the DOOR's ABI needed somewhere to write. `System::WideString::AsUTF8()` is
    // a real const member returning `System::String` by value
    // (runtime/src/system/widestring.cpp), and both sides of the call are
    // GCC-compiled C++, so direct-initialising the local FROM the prvalue is one
    // construction into the same storage — strictly fewer than the two the door
    // shape performed. The retslot hazard RULE B5-12 warns about is a
    // door/virtual-slot hazard; it does not arise for a non-virtual member called
    // from C++.

    // --- Video::ImageBuffer ----------------------------------------------
    // ENDGAME B.3.5 block J: `?CopyFromTexture@ImageBuffer@Video@@` is
    // `Video::ImageBuffer::CopyFromTexture` (runtime/src/video/buffers.cpp).

    // --- Application ------------------------------------------------------
    // ENDGAME B.3.7 callers (K5): the three function imports are GONE —
    // 0x180013290 `?RegisterMessageCallback@Application@1@`, 0x180013298
    // `?UnregisterMessageCallback@Application@1@`, 0x1800132a8
    // `?GetInstance@Application@1@`. `Application::Application` is a real C++
    // class since B.3.7 block B1 (runtime/types/application/Application.h), and
    // the plugin calls the three members directly on the object read through
    // `appSingleton` below. That DATA slot (0x180013310) stays a slot until the
    // class-static conversion at the end of B.3 (carry-over 5).

    // --- the three singleton slots ---------------------------------------
    void** appSingleton;                               // 0x180013310
    void** configSingleton;                            // 0x180013480
    void** deviceSingleton;                            // 0x1800134b8
};

Imports g_im;
bool    g_imResolved = false;
bool    g_imOk       = false;

const Imports* GetImports() {
    if (g_imResolved) return g_imOk ? &g_im : nullptr;
    g_imResolved = true;
    std::memset(&g_im, 0, sizeof(g_im));

    struct Entry { void** dst; const char* mangled; uintptr_t iat; };
    const Entry table[] = {
        { (void**)&g_im.MemAlloc,               "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z",                 0x1800134c0 },
        { (void**)&g_im.MemFree,                "?MemFree@MemoryAllocator@System@@SAXPEAX@Z",                    0x1800134d0 },

        { (void**)&g_im.CtorContextData,        "??0ContextData@Video@@QEAA@PEAVContext@1@@Z",                    0x180013478 },
        { (void**)&g_im.CtorImageBufferData,    "??0ImageBufferData@Video@@QEAA@PEAVImageBuffer@1@@Z",            0x1800133e8 },
        { (void**)&g_im.CtorConstantBufferData, "??0ConstantBufferData@Video@@QEAA@PEAVConstantBuffer@1@@Z",      0x180013490 },
        { (void**)&g_im.CtorIndexBufferData,    "??0IndexBufferData@Video@@QEAA@PEAVIndexBuffer@1@@Z",            0x180013390 },
        { (void**)&g_im.CtorVertexBufferData,   "??0VertexBufferData@Video@@QEAA@PEAVVertexBuffer@1@@Z",          0x180013340 },
        { (void**)&g_im.CtorVertexLayoutData,   "??0VertexLayoutData@Video@@QEAA@PEAVVertexLayout@1@@Z",          0x180013330 },
        { (void**)&g_im.CtorVertexProgramData,  "??0VertexProgramData@Video@@QEAA@PEAVVertexProgram@1@@Z",        0x180013320 },
        { (void**)&g_im.CtorPixelProgramData,   "??0PixelProgramData@Video@@QEAA@PEAVPixelProgram@1@@Z",          0x180013360 },
        { (void**)&g_im.CtorRenderStateData,    "??0RenderStateData@Video@@QEAA@PEAVRenderState@1@@Z",            0x180013350 },
        { (void**)&g_im.CtorVideoProgramData,   "??0VideoProgramData@Video@@QEAA@PEAVVideoProgram@1@@Z",          0x1800134a0 },
        { (void**)&g_im.CtorRenderBufferData,   "??0RenderBufferData@Video@@QEAA@PEAVRenderBuffer@1@@Z",          0x1800134b0 },


        { (void**)&g_im.AdapterCtor,            "??0Adapter@Device@Video@@QEAA@XZ",                              0x180013250 },
        { (void**)&g_im.AdapterCopyCtor,        "??0Adapter@Device@Video@@QEAA@AEBU012@@Z",                      0x1800132b8 },
        { (void**)&g_im.AdapterDtor,            "??1Adapter@Device@Video@@QEAA@XZ",                              0x1800134c8 },
        { (void**)&g_im.GfxIfaceCtor,           "??0GraphicsInterface@Device@Video@@QEAA@XZ",                    0x1800132d0 },
        { (void**)&g_im.GfxIfaceCopyCtor,       "??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z",            0x1800132b0 },
        { (void**)&g_im.GfxIfaceDtor,           "??1GraphicsInterface@Device@Video@@QEAA@XZ",                    0x1800132c8 },
        { (void**)&g_im.DisplayModeCtor,        "??0DisplayMode@Device@Video@@QEAA@HH@Z",                        0x180013308 },
        { (void**)&g_im.DisplayModeDtor,        "??1DisplayMode@Device@Video@@QEAA@XZ",                          0x1800132f8 },


        { (void**)&g_im.appSingleton,           "?_singleton@Application@1@0PEAV11@EA",                          0x180013310 },
        { (void**)&g_im.configSingleton,        "?_singleton@Configuration@Video@@0PEAV12@EA",                   0x180013480 },
        { (void**)&g_im.deviceSingleton,        "?_singleton@Device@Video@@0PEAV12@EA",                          0x1800134b8 },
    };
    g_imOk = true;
    for (const Entry& e : table) {
        *e.dst = Dx11ResolveRuntimeImport(e.mangled, e.iat);
        // Name the casualty. The whole-struct scan below can only say "some
        // field is null", which cost a debugging session when Standalone mode
        // removed the VideoDX11 IAT fallback and one import started failing:
        // the symptom was DeviceData::EnumerateInterfaces silently declining to
        // enumerate, three layers away from the cause.
        if (!*e.dst) {
            fprintf(stderr, "[Dx11] devicedata import UNRESOLVED: %s\n", e.mangled);
            g_imOk = false;
        }
    }

    // Also catch a field that has no table entry at all — that one is a
    // straightforward coding error and shows up as a permanent null.
    const void* const* p = reinterpret_cast<const void* const*>(&g_im);
    for (size_t i = 0; i < sizeof(Imports) / sizeof(void*); ++i) {
        if (p[i] == nullptr) {
            fprintf(stderr, "[Dx11] devicedata Imports field #%zu is null "
                            "(no table entry?)\n", i);
            g_imOk = false;
        }
    }
    if (!g_imOk) fflush(stderr);
    return g_imOk ? &g_im : nullptr;
}

// ============================================================
// d3d11.dll / dxgi.dll entry points
//
// These are the only two non-COM graphics imports the DLL has.
//
// They used to be read out of VideoDX11's OWN import address table — the exact
// pointers the shipped code CALLs through — with GetProcAddress as a fallback
// that the comment claimed was unreachable "because VideoDX11 could not have
// loaded without these imports resolving". Standalone mode made it not merely
// reachable but the only path: the DLL is never loaded, so there is no IAT.
//
// The order is therefore inverted. LoadLibrary (not GetModuleHandle) is what
// makes this work standalone: with VideoDX11 absent, nothing else in the
// process has pulled d3d11.dll or dxgi.dll in yet, so asking for a handle to an
// already-loaded module returns null and the renderer never comes up.
// ============================================================
typedef HRESULT (WINAPI *CreateDXGIFactory1_t)(REFIID, void**);
typedef HRESULT (WINAPI *D3D11CreateDevice_t)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
                                              const D3D_FEATURE_LEVEL*, UINT, UINT,
                                              ID3D11Device**, D3D_FEATURE_LEVEL*,
                                              ID3D11DeviceContext**);

void* ResolveGfxImport(uintptr_t iatGhidra, const wchar_t* dll, const char* name) {
    // Primary: the system DLL, loading it if necessary. Same function either
    // way — the shipped DLL's IAT slot holds exactly what GetProcAddress
    // returns, since that is what the loader wrote into it.
    HMODULE m = GetModuleHandleW(dll);
    if (!m) m = LoadLibraryW(dll);
    if (m) {
        if (void* fn = reinterpret_cast<void*>(GetProcAddress(m, name)))
            return fn;
    }

    // There is no second source: the old fallback read VideoDX11-x64.dll's own
    // IAT slot, and that DLL is never mapped (ENDGAME.md A.1).
    (void)iatGhidra;

    fprintf(stderr, "[Dx11] FAILED to resolve %ls!%s — the renderer cannot start\n",
            dll, name);
    return nullptr;
}

CreateDXGIFactory1_t GetCreateDXGIFactory1() {
    static CreateDXGIFactory1_t fn = reinterpret_cast<CreateDXGIFactory1_t>(
        ResolveGfxImport(0x180013530, L"dxgi.dll", "CreateDXGIFactory1"));
    return fn;
}
D3D11CreateDevice_t GetD3D11CreateDevice() {
    static D3D11CreateDevice_t fn = reinterpret_cast<D3D11CreateDevice_t>(
        ResolveGfxImport(0x180013520, L"d3d11.dll", "D3D11CreateDevice"));
    return fn;
}

// IID_IDXGIFactory1, transcribed from the 16 bytes at VideoDX11 0x180018b98
// that slots 1 and 2 hand to CreateDXGIFactory1:
//   78 ae 0a 77  6f f2  ba 4d  a8 29 25 3c 83 d1 b3 87
// Spelled out rather than using the dxguid.lib symbol so this file does not add
// a library to the link line, and so the shipped bytes sit next to their use.
const GUID kIID_IDXGIFactory1 =
    { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };

// ============================================================
// Runtime-side field offsets
//
// Raw offsets rather than invented structs: Video::Device is at least 0x108
// bytes and only ~14 of them are understood, so a struct would be mostly
// fabricated padding. Each constant carries its proof.
// ============================================================
namespace Dev {
// +0x04 / +0x08  the swap chain's back-buffer size. Written by slot 2 from the
// display-mode table and then again by Video::Device::AdjustDisplay
// (Runtime 0x1800f7e20) from GetClientRect; read by FillSwapChainDesc
// (0x1800045f6 / 0x1800045ff) and by the buffer creators.
constexpr size_t kWidth            = 0x004;
constexpr size_t kHeight           = 0x008;

// +0x0c  index into the mode table at +0x20. 0x180004edc uses it with an
// effective stride of 0x18 (`LEA RCX,[RAX+RAX*2]` then scale 8).
constexpr size_t kModeIndex        = 0x00c;

// +0x10  index into the current DisplayMode's refresh-rate array (stride 8).
// 0x180004609 / 0x18000461c.
constexpr size_t kRefreshIndex     = 0x010;

// +0x14 / +0x18  the DXGI adapter and output indices slot 2 re-enumerates with
// (0x180004e6f -> EnumAdapters1, 0x180004eaa -> EnumOutputs).
constexpr size_t kAdapterIndex     = 0x014;
constexpr size_t kOutputIndex      = 0x018;

// +0x20  DisplayMode[] for the selected adapter, stride 0x18 (0x180004ee4).
constexpr size_t kModeTable        = 0x020;

// +0x40 / +0x48 / +0x4c  the Device::GraphicsInterface array {data,count,cap}.
// Slot 1's tail passes `Device+0x40` and `count+1` to the array grower at
// 0x180005b00 (0x180004dca-0x180004ddc). NOT a bool — see the header note.
constexpr size_t kInterfaces       = 0x040;
constexpr size_t kInterfaceCount   = 0x048;

// +0x90  HWND. Used as DXGI_SWAP_CHAIN_DESC::OutputWindow (0x18000464e) and by
// ShowWindow / IsWindowVisible / MakeWindowAssociation.
constexpr size_t kHwnd             = 0x090;

// +0x9c  "swap chain needs a rebuild". Set by the resize helper before it does
// anything and cleared only on its success path. L5 found the reader:
// ContextData::BeginCommands bails to the resize helper while it is set.
constexpr size_t kResetPending     = 0x09c;

// +0xa0  HMONITOR, taken from DXGI_OUTPUT_DESC::Monitor (0x180004f5d).
constexpr size_t kMonitor          = 0x0a0;

// +0x100  the active Video::DeviceData*.
constexpr size_t kActiveDeviceData = 0x100;
}  // namespace Dev

namespace Cfg {
constexpr size_t kWindowWidth  = 0x1c;  // VideoWindowWidth,  -1 = skip override
constexpr size_t kWindowHeight = 0x20;  // VideoWindowHeight, -1 = skip override
constexpr size_t kMultisample  = 0x24;  // VideoMultisample
constexpr size_t kFullscreen   = 0x40;  // VideoFullscreen
constexpr size_t kSkipShow     = 0x42;  // not persisted; suppresses the ShowWindow
}  // namespace Cfg

template <typename T> inline T& Field(void* base, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + off);
}
template <typename T> inline T Field(const void* base, size_t off) {
    return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + off);
}

inline void* DeviceOf(DeviceData* self) { return self->device; }

// ENDGAME B.3.5 block J — the two bridge casts every converted call goes through.
// `DeviceData::device` (+0x08) is a `Video::Device*` and the cursor slot is handed
// a `Video::ImageBuffer*`, but both arrive on this side of the seam as `void*`
// (RULE B5-8's declared leave: the concrete `*Data` classes mirror the base's
// fields rather than deriving in C++). Named once each instead of at ten sites.
inline ::Video::Device* VDev(void* device) {
    return reinterpret_cast< ::Video::Device*>(device);            // B.3.5 bridge
}
inline ::Video::ImageBuffer* VImg(void* imageBuffer) {
    return reinterpret_cast< ::Video::ImageBuffer*>(imageBuffer);  // B.3.5 bridge
}
inline void* ConfigOf(const Imports* im) { return *im->configSingleton; }
inline void* AppOf(const Imports* im)    { return *im->appSingleton; }

// ============================================================
// Runtime value types this file constructs on the stack
//
// ENDGAME B.3.5 block D2, B35_FILEMAP §3f — THE FOUR LOCAL MIRRORS ARE GONE.
// This file used to re-declare `DisplayMode`, `RefreshRate`, `Adapter` and
// `GraphicsInterface` as byte-for-byte name-and-layout duplicates of
// `runtime/types/video/Device.h`'s, each with its own `static_assert`s repeating
// the Runtime-side ones. That is B34_RULES §1.5.2's duplicate-POD hazard four
// times over: two independent descriptions of one layout, either of which could
// drift. They are now ALIASES of the real nested classes
// `Video::Device::{DisplayMode,RefreshRate,Adapter,GraphicsInterface}`, so there
// is ONE description and dx11 gets the class's own field names. RULE B5-1.2 /
// B35_RULES §1.2: dx11 compiles with the shared include set, so this needs no
// Makefile change and is simply another `runtime/types` header included here.
//
// WHAT THIS FILE PROVED THAT THE RUNTIME SIDE COULD NOT, and it is now recorded
// in the header instead of here: `Adapter+0x2c`/`+0x30`, which Wave 6 lane B
// could only describe as "ctor = -1, meaning not recovered", are the DXGI
// enumeration coordinates. `DeviceData_EnumerateInterfaces` below is the WRITER
// (`EnumAdapters1`'s loop counter and `EnumOutputs`'s), and `Dev::kAdapterIndex`
// / `Dev::kOutputIndex` (Device+0x14/+0x18, the copies `ValidateConfiguration`
// makes) are where this file READS them back. Header fields renamed to
// `dxgiAdapterIndex` / `dxgiOutputIndex` accordingly.
//
// The sizes and every offset are still asserted — by Device.h, once, against the
// Runtime bodies AND against this file's strides (0x180004c43 for Adapter,
// 0x180005b00 for GraphicsInterface). The two strides agree with the Runtime
// side's, which is what made the collapse safe rather than hopeful.
// ============================================================

using DisplayMode       = ::Video::Device::DisplayMode;
using RefreshRate       = ::Video::Device::RefreshRate;
using Adapter           = ::Video::Device::Adapter;
using GraphicsInterface = ::Video::Device::GraphicsInterface;

// Re-stated here, from THIS file's own instruments, so the collapse is checked
// rather than assumed: if Device.h ever disagreed with the strides dx11 walks,
// the build breaks here.
static_assert(sizeof(Adapter) == 0x38,
              "Video::Device::Adapter stride — dx11's array grower at 0x180004c43");
static_assert(sizeof(GraphicsInterface) == 0x18,
              "GraphicsInterface stride — dx11 at 0x180005b00");
static_assert(sizeof(DisplayMode) == 0x18, "Video::Device::DisplayMode stride");
static_assert(sizeof(RefreshRate) == 8, "RefreshRate stride (Device+0x10 scales by 8)");

// System::String is a single pointer to a refcounted char buffer (refcount at
// [-4], allocation base at [-8] — visible in Video::Configuration::Startup's
// inlined destructor). We never touch the interior; 8 bytes of storage plus the
// Runtime ctor/dtor is all this file needs.
struct WideStringStorage { void* p; };

// The three OS/DXGI structure offsets slot 1 and slot 2 depend on. The original
// hard-codes them as displacements, so if the SDK headers ever disagreed the
// port would read the wrong field silently. Pinned here instead.
//
//   DXGI_OUTPUT_DESC::Monitor at +0x58   — slot 2 stores it to Device+0xa0
//                                          (`MOV RAX,[RBP+0x18]` where the desc
//                                          base is RBP-0x40, i.e. +0x58)
//   MONITORINFOEXW size 0x68             — slot 1 writes cbSize = 0x68 literally
//
// MONITORINFOEXW::szDevice sits at +0x28 in the original (`LEA RCX,[RBP+0x88]`
// against a struct based at RBP+0x60). There is no static_assert for it because
// mingw-w64 declares MONITORINFOEXW as deriving from MONITORINFO in C++ mode,
// which makes offsetof on it only conditionally supported and produces a
// -Winvalid-offsetof diagnostic. This file reaches the field BY NAME, so the
// compiler places it correctly regardless; the size assert below is what
// actually guards the cbSize literal the OS validates.
static_assert(offsetof(DXGI_OUTPUT_DESC, Monitor) == 0x58,
              "DXGI_OUTPUT_DESC::Monitor offset (0x180004f59)");
static_assert(sizeof(MONITORINFOEXW) == 0x68,
              "MONITORINFOEXW cbSize literal 0x68 (0x180004811)");
static_assert(sizeof(DXGI_SWAP_CHAIN_DESC) == 0x48,
              "DXGI_SWAP_CHAIN_DESC is the 9 qwords FillSwapChainDesc zeroes");
static_assert(sizeof(DXGI_MODE_DESC) == 0x1c,
              "DXGI_MODE_DESC stride — slot 1 allocates count*0x1c (0x180004941)");

// Forward decl — the WM_ACTIVATE handler and the resize helper are mutually
// referenced through RegisterMessageCallback.
int  DeviceData_Resize(DeviceData* self);

// ============================================================
// Original slot forwarding
// ============================================================
template <typename Fn> Fn Orig(int slot) {
    return reinterpret_cast<Fn>(Dx11OriginalSlot(Vt::DeviceData, slot));
}

// ============================================================
// 0x180004510 — ResolveMultisampleMode() -> sampleCount
//
// Clamp Video::Configuration's multisample INDEX against the adapter's mode
// count, then walk downwards until the device reports the mode supported, write
// the (possibly lowered) index back into the configuration, and return
// index + 1 as the D3D11 sample count.
//
// Two faithfulness details:
//   * The downward walk is unbounded. If index 0 were unsupported it would run
//     off the bottom into negative indices. The shipped code has no floor and
//     neither does this; Video::Device::IsMultisampleModeSupported(0) is always
//     true for a device that got this far (1x MSAA), so the loop terminates.
//   * The write-back is guarded by a compare (0x180004575) that makes it a
//     no-op when nothing changed. Kept, because it is the difference between
//     touching and not touching the Configuration singleton.
// ============================================================
int ResolveMultisampleMode(DeviceData* self, const Imports* im) {
    void* dev = DeviceOf(self);
    void* cfg = ConfigOf(im);

    int index = Field<int32_t>(cfg, Cfg::kMultisample);
    if (index >= VDev(dev)->GetMultisampleModeCount())
        index = VDev(dev)->GetMultisampleModeCount() - 1;

    while (!VDev(dev)->IsMultisampleModeSupported(index))
        --index;

    if (Field<int32_t>(cfg, Cfg::kMultisample) != index)
        Field<int32_t>(cfg, Cfg::kMultisample) = index;

    return index + 1;
}

// ============================================================
// 0x180004590 — FillSwapChainDesc(DXGI_SWAP_CHAIN_DESC*)
//
// The whole fullscreen story is these 72 bytes. Field by field, with the store
// that proves each:
//
//   BufferDesc.Width / .Height        Device+0x04 / +0x08          0x1800045f9 / 602
//   BufferDesc.RefreshRate            mode.rates[Device+0x10]      0x180004615 / 624
//   BufferDesc.Format                 0x57 = B8G8R8A8_UNORM        0x180004627
//   BufferDesc.ScanlineOrdering       0  (upper half of that qword store)
//   BufferDesc.Scaling                0                            0x180004631
//   SampleDesc                        {1, 0}                       0x180004634 (one qword)
//   BufferUsage                       0x20 = RENDER_TARGET_OUTPUT  0x18000463c
//   BufferCount                       2                            0x180004643
//   OutputWindow                      Device+0x90                  0x180004655
//   Windowed                          Configuration+0x40 == 0      0x18000465b-661
//   SwapEffect                        0 = DISCARD                  0x180004664
//   Flags                             2 = ALLOW_MODE_SWITCH        0x180004667
//
// The swap chain is single-sampled on purpose: MSAA lives on the +0x38 colour
// texture, which EndCommands resolves into the back buffer.
//
// The DisplayMode temporary is fetched for its refresh-rate table alone; the
// width/height in it are ignored here (Device+0x04/+0x08 already hold the
// post-AdjustDisplay client size).
// ============================================================
void FillSwapChainDesc(DeviceData* self, DXGI_SWAP_CHAIN_DESC* desc, const Imports* im) {
    void* dev = DeviceOf(self);
    void* cfg = ConfigOf(im);

    // ENDGAME B.3.5 block J, RULE B5-12 / CALLER_BATCH_BRIEF "by-value returns":
    // the door body moved to the call site. `Video::Device::GetDisplayMode` returns
    // the mode BY VALUE; retail's caller hands it a pointer to this very stack slot
    // and the door placement-constructs into it. Written as ONE placement-new of
    // the prvalue into that same uninitialised storage, which is what the door did,
    // and no copy (C++17 guarantees the prvalue is built in place).
    DisplayMode mode;
    ::new (static_cast<void*>(&mode))
        DisplayMode(VDev(dev)->GetDisplayMode(Field<int32_t>(dev, Dev::kModeIndex)));

    std::memset(desc, 0, sizeof(*desc));

    desc->BufferDesc.Width  = static_cast<UINT>(Field<int32_t>(dev, Dev::kWidth));
    desc->BufferDesc.Height = static_cast<UINT>(Field<int32_t>(dev, Dev::kHeight));

    // ENDGAME B.3.5: `mode` is the real `Video::Device::DisplayMode` now, so its
    // refresh array is already typed and the cast is gone.
    const RefreshRate* rates = mode.refreshData;
    const int rateIndex = Field<int32_t>(dev, Dev::kRefreshIndex);
    desc->BufferDesc.RefreshRate.Numerator   = static_cast<UINT>(rates[rateIndex].numerator);
    desc->BufferDesc.RefreshRate.Denominator = static_cast<UINT>(rates[rateIndex].denominator);

    desc->BufferDesc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;   // 0x57
    desc->BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    desc->BufferDesc.Scaling          = DXGI_MODE_SCALING_UNSPECIFIED;

    desc->SampleDesc.Count   = 1;
    desc->SampleDesc.Quality = 0;

    desc->BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;             // 0x20
    desc->BufferCount  = 2;
    desc->OutputWindow = Field<HWND>(dev, Dev::kHwnd);
    desc->Windowed     = (Field<uint8_t>(cfg, Cfg::kFullscreen) == 0) ? TRUE : FALSE;
    desc->SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
    desc->Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;      // 2

    im->DisplayModeDtor(&mode);
}

// ============================================================
// 0x1800052b0 / 0x180005350 — CreateColorBuffer / CreateDepthBuffer
//
// Identical except for three constants, so they share one body here exactly as
// the shipped code shares one shape. Both release-and-null the previous texture
// first, size themselves from Device+0x04/+0x08, and take their sample count
// from ResolveMultisampleMode.
//
//   colour  field +0x38  format 0x57 (B8G8R8A8_UNORM)   bind 0x20 (RENDER_TARGET)
//   depth   field +0x30  format 0x2d (D24_UNORM_S8_UINT) bind 0x40 (DEPTH_STENCIL)
//
// Neither checks the HRESULT beyond "== 0"; both return that as a bool. Note
// they are MULTISAMPLED while the swap chain is not — that is the whole reason
// EndCommands does a ResolveSubresource.
// ============================================================
bool CreateAttachment(DeviceData* self, void** slot, DXGI_FORMAT format,
                      UINT bindFlags, const Imports* im) {
    Dx11Release(*slot);
    *slot = nullptr;

    void* dev = DeviceOf(self);

    D3D11_TEXTURE2D_DESC desc;
    desc.Width              = static_cast<UINT>(Field<int32_t>(dev, Dev::kWidth));
    desc.Height             = static_cast<UINT>(Field<int32_t>(dev, Dev::kHeight));
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
    desc.Format             = format;
    desc.SampleDesc.Count   = static_cast<UINT>(ResolveMultisampleMode(self, im));
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = bindFlags;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;

    ID3D11Device* d3d = static_cast<ID3D11Device*>(self->d3dDevice);
    HRESULT hr = d3d->CreateTexture2D(&desc, nullptr,
                                      reinterpret_cast<ID3D11Texture2D**>(slot));
    return hr == 0;
}

bool CreateColorBuffer(DeviceData* self, const Imports* im) {
    return CreateAttachment(self, &self->colorBuffer, DXGI_FORMAT_B8G8R8A8_UNORM,
                            D3D11_BIND_RENDER_TARGET, im);
}
bool CreateDepthBuffer(DeviceData* self, const Imports* im) {
    return CreateAttachment(self, &self->depthBuffer, DXGI_FORMAT_D24_UNORM_S8_UINT,
                            D3D11_BIND_DEPTH_STENCIL, im);
}

// ============================================================
// Window-collapse hunt (temporary diagnostic)
//
// The window collapses to 136x39 about 1.6s into a run. 136x39 is exactly
// SM_CXMIN x SM_CYMIN on this desktop — the OS *minimum tracking size*, i.e.
// what Windows clamps a request to when a caller asks for a smaller (or 0x0)
// window. It is NOT the metrics of a minimized frame (SM_CXMINIMIZED is
// 160x28), and the WM_SIZE that follows carries type=0 (SIZE_RESTORED), so the
// window is never iconic. Something asks for a degenerate size and the OS
// rounds it up.
//
// CAUSE (proven 2026-07-24): PowerToys FancyZones, an EXTERNAL process. It is
// not a bug in this port or in the engine. FancyZones has no app exclusions
// configured and has openWindowOnActiveMonitor/appLastZone_moveWindows set, so
// it snaps the freshly created game window into a zone on another monitor. The
// cross-process SetWindowPos carries SWP_ASYNCWINDOWPOS, which is why it is
// invisible to every in-process hook and why the request is *posted*: it lands
// only when the main thread resumes pumping after the sync loader wait, making
// it look like the loader caused it. Adding reimpl-x64.exe to
// fancyzones_excluded_apps takes the collapse from 240 occurrences to 0 with no
// code change, and DeviceData_Resize is then never called at all.
//
// Everything on our side is downstream of that. This code and
// Video::Device::AdjustDisplay only *react*: AdjustDisplay unconditionally
// re-reads GetClientRect into Device+0x04/+0x08 and Configuration+0x1c/+0x20,
// so once an external actor has collapsed the window the bad size latches and
// each frame re-applies it. That latch is 100% original Runtime code, so retail
// Banished is vulnerable to exactly the same thing.
//
// The trace below is kept because it cleanly separates "someone else collapsed
// the window" from "we collapsed it": if entry already reports a degenerate
// client rect, the caller is external. Bounded to the first few resizes —
// once the swap chain is wedged BeginCommands retries every frame.
// ============================================================
namespace {
int g_resizeTraceCalls = 0;
constexpr int kResizeTraceMax = 6;

inline bool ResizeTraceOn() { return g_resizeTraceCalls <= kResizeTraceMax; }

// ------------------------------------------------------------------
// Degenerate-size guard (deliberate deviation from retail — see below)
//
// Retail latches whatever GetClientRect returns, with no floor. That was safe
// in 2013: nothing on a normal desktop reshaped a window behind the app's back.
// It is not safe now — FancyZones (2019) and every tiling/zone manager since do
// exactly that, and one collapsed frame poisons Device+0x04/+0x08 and
// Configuration+0x1c/+0x20 for the rest of the run, then gets persisted.
//
// The guard rejects impossible values rather than overriding real ones: no
// window this small can be a render target, and no user ever chose one. 64
// matches the sentinel repair in standalone/main.cpp (HarnessGuardUnattendedBoot).
//
// Kill-switch for A/B: BANISHED_WINDOW_GUARD=0 restores verbatim retail
// behaviour (window collapses and stays collapsed).
// ------------------------------------------------------------------
constexpr int kMinSaneWindowDim = 64;

inline bool DegenerateSize(int w, int h) {
    return w < kMinSaneWindowDim || h < kMinSaneWindowDim;
}

// Last size that could plausibly be a render target. The guard restores this
// when it catches a collapse, so it must only ever hold a sane pair.
int g_lastGoodW = 0;
int g_lastGoodH = 0;

inline void RememberGoodSize(int w, int h) {
    if (!DegenerateSize(w, h)) { g_lastGoodW = w; g_lastGoodH = h; }
}

// Best available sane size, in descending order of trustworthiness: what we
// last saw working, then the configured window override, then the display mode
// (which is always sane because it comes from the adapter's mode table).
void PickGoodSize(void* cfg, const DisplayMode& mode, int* w, int* h) {
    if (!DegenerateSize(g_lastGoodW, g_lastGoodH)) {
        *w = g_lastGoodW; *h = g_lastGoodH; return;
    }
    const int cw = Field<int32_t>(cfg, Cfg::kWindowWidth);
    const int ch = Field<int32_t>(cfg, Cfg::kWindowHeight);
    if (!DegenerateSize(cw, ch)) { *w = cw; *h = ch; return; }
    *w = mode.width; *h = mode.height;
}

void TraceGeom(const char* stage, void* dev, const Imports* im) {
    if (!ResizeTraceOn()) return;
    HWND hwnd = Field<HWND>(dev, Dev::kHwnd);
    void* cfg = ConfigOf(im);
    RECT wr{}, cr{};
    GetWindowRect(hwnd, &wr);
    GetClientRect(hwnd, &cr);
    fprintf(stderr,
            "[ResizeTrace #%d] %-18s win=(%ld,%ld %ldx%ld) client=%ldx%ld "
            "dev=%dx%d cfgWin=%dx%d vis=%d tid=%lu\n",
            g_resizeTraceCalls, stage,
            wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top,
            cr.right - cr.left, cr.bottom - cr.top,
            Field<int32_t>(dev, Dev::kWidth), Field<int32_t>(dev, Dev::kHeight),
            Field<int32_t>(cfg, Cfg::kWindowWidth),
            Field<int32_t>(cfg, Cfg::kWindowHeight),
            IsWindowVisible(hwnd) ? 1 : 0, GetCurrentThreadId());
    fflush(stderr);
}

// Which monitor is the swap chain bound to? The collapsed window lands at
// (-2456,104), which is (104,104) on DISPLAY3 (desktop x=-2560). If the
// containing output is that monitor, a DXGI-side reposition explains both the
// move and the resize.
void TraceContainingOutput(IDXGISwapChain* sc) {
    static bool done = false;
    if (done) return;
    done = true;
    IDXGIOutput* out = nullptr;
    if (SUCCEEDED(sc->GetContainingOutput(&out)) && out) {
        DXGI_OUTPUT_DESC od{};
        if (SUCCEEDED(out->GetDesc(&od))) {
            fprintf(stderr, "[ResizeTrace] swapchain containing output '%ls' "
                    "desktop=(%ld,%ld)-(%ld,%ld) attached=%d\n",
                    od.DeviceName,
                    od.DesktopCoordinates.left, od.DesktopCoordinates.top,
                    od.DesktopCoordinates.right, od.DesktopCoordinates.bottom,
                    od.AttachedToDesktop ? 1 : 0);
            fflush(stderr);
        }
        out->Release();
    }
}
}  // namespace

// ============================================================
// 0x180004340 — the swap-chain resize / recreate helper
//
// Not a vtable slot: a plain function called from the WM_ACTIVATE handler
// (twice, one call per transition direction) and directly by
// ContextData::BeginCommands when Device+0x9c is set. Both callers run this
// body, which is a transcription of the shipped one.
//
// Sequence, with the early-out first:
//
//   Device+0x9c = 1                       <- set BEFORE the visibility test
//   if (!IsWindowVisible(Device+0x90)) return 0;   <- Device+0x9c STAYS SET
//   context->OMSetRenderTargets(0, NULL, NULL)
//   context->ClearState()
//   Video::Device::ReleaseDynamic(device)
//   Release depth (+0x30), Release colour (+0x38), null both
//   GetDisplayMode(device, &mode, Device+0x0c)
//   if (Configuration+0x40 && !DeviceData+0x40)   <- entering/staying fullscreen
//       Device+0x04/+0x08 = mode.width/height
//   Video::Device::AdjustDisplay(device)          <- moves/resizes the HWND and,
//                                                    in windowed mode, rewrites
//                                                    Device+0x04/+0x08 from
//                                                    GetClientRect
//   swapChain->ResizeTarget({Device+0x04, Device+0x08, mode.rates[Device+0x10],
//                            format 28, 0, 0})
//   swapChain->ResizeBuffers(1, Device+0x04, Device+0x08, 0x57, 2)
//   swapChain->SetFullscreenState(Configuration+0x40 != 0, NULL)
//   CreateColorBuffer() && CreateDepthBuffer()
//   Video::Device::RecreateDynamic(device)
//   Device+0x9c = 0
//   return 1
//
// Any failure returns 0 and LEAVES Device+0x9c set, which is what makes
// BeginCommands retry the whole thing on the next frame.
//
// The `format 28` in the ResizeTarget mode desc is DXGI_FORMAT_R8G8B8A8_UNORM,
// not the B8G8R8A8_UNORM (0x57) the swap chain actually uses. That mismatch is
// in the shipped code (`MOV dword ptr [RSP+0x60],0x1c` at 0x18000441d) and is
// reproduced verbatim — see the quirks list.
// ============================================================
int DeviceData_Resize(DeviceData* self) {
    const Imports* im = GetImports();
    // No fallback to the shipped 0x180004340: VideoDX11-x64.dll is never mapped
    // (ENDGAME.md A.1). An unresolved import table is a real failure.
    if (!im) return 0;

    void* dev = DeviceOf(self);
    ++g_resizeTraceCalls;
    TraceGeom("entry", dev, im);
    Field<uint8_t>(dev, Dev::kResetPending) = 1;

    if (!IsWindowVisible(Field<HWND>(dev, Dev::kHwnd)))
        return 0;

    // A minimized window has a 0x0 client rect, and IsWindowVisible() is still
    // TRUE for it — so retail cheerfully latches that 0x0 the first time the
    // player minimizes. Treat it like the not-visible case: leave
    // kResetPending set and rebuild when the window comes back. Doing this
    // before the guard below also stops us fighting a deliberate minimize.
    if (IsIconic(Field<HWND>(dev, Dev::kHwnd)))
        return 0;

    ID3D11DeviceContext* ctx = static_cast<ID3D11DeviceContext*>(self->d3dContext);
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    ctx->ClearState();

    // ClearState() has just unbound every constant buffer. ConstantBufferData
    // caches which rung it has bound and would otherwise never re-bind, leaving
    // every shader reading a null constant buffer — a permanently black window
    // after any resize. Drop those caches here, where the bindings actually
    // died. Deliberate deviation from the shipped DLL; see the note above
    // Dx11InvalidateConstantBufferBindings in buffers.cpp.
    Dx11InvalidateConstantBufferBindings();

    VDev(dev)->ReleaseDynamic();

    Dx11Release(self->depthBuffer);
    Dx11Release(self->colorBuffer);
    self->depthBuffer = nullptr;
    self->colorBuffer = nullptr;

    void* cfg = ConfigOf(im);

    // The same by-value shape as FillSwapChainDesc's — see the note there.
    DisplayMode mode;
    ::new (static_cast<void*>(&mode))
        DisplayMode(VDev(dev)->GetDisplayMode(Field<int32_t>(dev, Dev::kModeIndex)));

    if (Field<uint8_t>(cfg, Cfg::kFullscreen) != 0 && self->wasFullscreen == 0) {
        Field<int32_t>(dev, Dev::kWidth)  = mode.width;
        Field<int32_t>(dev, Dev::kHeight) = mode.height;
    }

    // AdjustDisplay's windowed branch re-reads GetClientRect and writes the
    // result into BOTH Device+0x04/+0x08 and Video::Configuration+0x1c/+0x20,
    // with no floor. Left alone, one collapsed frame latches for the whole run
    // and then gets persisted. The guard brackets that call.
    //
    // Guard, first half. Device+0x04/+0x08 arrives here from the WM_SIZE path,
    // so an external collapse is already sitting in it before AdjustDisplay
    // runs. Replacing it with a sane size now means AdjustDisplay's SetWindowPos
    // pushes the window back open instead of nailing it shut, and the
    // GetClientRect it latches afterwards is the good value.
    if (DegenerateSize(Field<int32_t>(dev, Dev::kWidth),
                       Field<int32_t>(dev, Dev::kHeight))) {
        int gw = 0, gh = 0;
        PickGoodSize(cfg, mode, &gw, &gh);
        static bool reported = false;
        if (!reported) {
            reported = true;
            fprintf(stderr, "[WindowGuard] refusing collapsed size %dx%d -> "
                    "restoring %dx%d (an external window manager reshaped the "
                    "window; see the note above DeviceData_Resize)\n",
                    Field<int32_t>(dev, Dev::kWidth),
                    Field<int32_t>(dev, Dev::kHeight), gw, gh);
            fflush(stderr);
        }
        Field<int32_t>(dev, Dev::kWidth)  = gw;
        Field<int32_t>(dev, Dev::kHeight) = gh;
    }

    TraceGeom("pre-AdjustDisplay", dev, im);
    VDev(dev)->AdjustDisplay();
    TraceGeom("post-AdjustDisplay", dev, im);

    // Guard, second half. If the external actor re-collapsed the window inside
    // that call, AdjustDisplay has just latched the bad size into both the
    // device and the configuration. Scrub both: the swap chain must never be
    // built at a zero dimension (CreateColorBuffer fails with "[0x0 4xAA]" and
    // Engine::Startup returns 0), and Configuration must never carry a value
    // that Video::Configuration::Save would persist. The window itself may stay
    // collapsed — we do not keep fighting for it — but the renderer survives
    // and the settings stay clean.
    {
        int gw = 0, gh = 0;
        PickGoodSize(cfg, mode, &gw, &gh);
        if (DegenerateSize(Field<int32_t>(dev, Dev::kWidth),
                           Field<int32_t>(dev, Dev::kHeight))) {
            Field<int32_t>(dev, Dev::kWidth)  = gw;
            Field<int32_t>(dev, Dev::kHeight) = gh;
        }
        if (DegenerateSize(Field<int32_t>(cfg, Cfg::kWindowWidth),
                           Field<int32_t>(cfg, Cfg::kWindowHeight))) {
            Field<int32_t>(cfg, Cfg::kWindowWidth)  = gw;
            Field<int32_t>(cfg, Cfg::kWindowHeight) = gh;
        }
    }

    // Whatever we ended up with, if it is sane it becomes the restore point.
    RememberGoodSize(Field<int32_t>(dev, Dev::kWidth),
                     Field<int32_t>(dev, Dev::kHeight));

    const RefreshRate* rates = mode.refreshData;   // ENDGAME B.3.5: typed, no cast
    const int rateIndex = Field<int32_t>(dev, Dev::kRefreshIndex);

    DXGI_MODE_DESC target;
    target.Width                   = static_cast<UINT>(Field<int32_t>(dev, Dev::kWidth));
    target.Height                  = static_cast<UINT>(Field<int32_t>(dev, Dev::kHeight));
    target.RefreshRate.Numerator   = static_cast<UINT>(rates[rateIndex].numerator);
    target.RefreshRate.Denominator = static_cast<UINT>(rates[rateIndex].denominator);
    target.Format                  = static_cast<DXGI_FORMAT>(28);  // sic — see quirks
    target.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    target.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;

    IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(self->swapChain);

    bool ok = false;

    // ResizeTarget is called first and unconditionally in the shipped order, so
    // hoisting it out of the short-circuit chain preserves semantics exactly.
    TraceContainingOutput(sc);
    TraceGeom("pre-ResizeTarget", dev, im);
    const HRESULT hrTarget = sc->ResizeTarget(&target);
    TraceGeom("post-ResizeTarget", dev, im);
    if (ResizeTraceOn()) {
        fprintf(stderr, "[ResizeTrace #%d] ResizeTarget(%ux%u @ %u/%u fmt=%u) "
                "-> 0x%08lX\n", g_resizeTraceCalls, target.Width, target.Height,
                target.RefreshRate.Numerator, target.RefreshRate.Denominator,
                static_cast<unsigned>(target.Format),
                static_cast<unsigned long>(hrTarget));
        fflush(stderr);
    }

    if (hrTarget == 0 &&
        sc->ResizeBuffers(1,
                          static_cast<UINT>(Field<int32_t>(dev, Dev::kWidth)),
                          static_cast<UINT>(Field<int32_t>(dev, Dev::kHeight)),
                          DXGI_FORMAT_B8G8R8A8_UNORM,
                          DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH) == 0 &&
        sc->SetFullscreenState(Field<uint8_t>(cfg, Cfg::kFullscreen) != 0 ? TRUE : FALSE,
                               nullptr) == 0 &&
        CreateColorBuffer(self, im) &&
        CreateDepthBuffer(self, im)) {
        VDev(dev)->RecreateDynamic();
        Field<uint8_t>(dev, Dev::kResetPending) = 0;
        ok = true;
    }

    im->DisplayModeDtor(&mode);
    return ok ? 1 : 0;
}

// ============================================================
// 0x1800042d0 — WM_ACTIVATE callback (registered as message id 6)
//
// The fullscreen save/restore across focus changes. Registered by slot 2 and
// unregistered by slot 3.
//
//   cfg = Video::Configuration::_singleton
//   dd  = Video::Device::_singleton->[0x100]        (the DeviceData)
//   if (cfg->fullscreen == 0 && dd->wasFullscreen == 0) return 1;   // nothing to do
//   if (LOWORD(wParam) == WA_INACTIVE) {
//       dd->wasFullscreen = 1;    cfg->fullscreen = 0;    Resize(dd);
//   } else if (dd->wasFullscreen) {
//       dd->wasFullscreen = 0;    cfg->fullscreen = 1;    Resize(dd);
//   }
//   return 1;
//
// So DeviceData+0x40 is not "is fullscreen" — it is "was fullscreen, restore it
// when focus comes back". Naming it `wasFullscreen` (which the type header
// already does) is exactly right.
//
// Note the outer guard: when the game is configured windowed AND has never been
// fullscreen, both flags are zero and this handler does nothing at all. It
// cannot resize a window that was never fullscreen.
//
// The callback signature comes from the Runtime export it is handed to:
//   ?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z
//     -> void RegisterMessageCallback(unsigned, int (*)(HWND, unsigned,
//                                                       unsigned __int64, __int64))
// The original reads only the low word of wParam (`TEST R8W,R8W`).
// ============================================================
int __cdecl DeviceData_OnActivate(HWND /*hwnd*/, unsigned /*msg*/,
                                  unsigned long long wParam, long long /*lParam*/) {
    const Imports* im = GetImports();
    if (!im) return 1;

    void* cfg = ConfigOf(im);
    void* dev = *im->deviceSingleton;
    DeviceData* dd = Field<DeviceData*>(dev, Dev::kActiveDeviceData);

    if (Field<uint8_t>(cfg, Cfg::kFullscreen) == 0 && dd->wasFullscreen == 0)
        return 1;

    if (static_cast<unsigned short>(wParam) == 0) {      // WA_INACTIVE
        dd->wasFullscreen = 1;
        Field<uint8_t>(cfg, Cfg::kFullscreen) = 0;
        DeviceData_Resize(dd);
        return 1;
    }

    if (dd->wasFullscreen != 0) {
        dd->wasFullscreen = 0;
        Field<uint8_t>(cfg, Cfg::kFullscreen) = 1;
        DeviceData_Resize(dd);
    }
    return 1;
}

// ============================================================
// 0x1800053f0 — WM_SETCURSOR callback (registered as message id 0x20)
//
// Three instructions of substance: re-apply the cursor slot 8 built, and force
// it visible. Reads the DeviceData through Video::Device::_singleton rather
// than capturing it, because a message callback gets no user pointer.
// ============================================================
int __cdecl DeviceData_OnSetCursor(HWND /*hwnd*/, unsigned /*msg*/,
                                   unsigned long long /*w*/, long long /*l*/) {
    const Imports* im = GetImports();
    if (!im) return 1;

    void* dev = *im->deviceSingleton;
    DeviceData* dd = Field<DeviceData*>(dev, Dev::kActiveDeviceData);

    SetCursor(static_cast<HCURSOR>(dd->cursor));
    ShowCursor(TRUE);
    return 1;
}

// ============================================================
// Slot 0 — 0x180004270  scalar deleting destructor
//
//   this->vptr = DeviceData vtable
//   MemFree(this->trackedImages)
//   this->trackedImages = NULL; this->trackedCount = this->trackedCapacity = 0
//   Video::DeviceData::~DeviceData(this)
//   if (flags & 1) MemFree(this)
//   return this
//
// The count/capacity clear is a SINGLE 8-byte store at +0x68 (0x1800042a7), so
// it zeroes both halves; writing them separately is equivalent and clearer.
//
// The vptr store re-seats `this` on the LIVE vtable — the same array the seam
// patches — so a partially-ported class stays consistent. Dx11VtableAddress
// hands back that live address rather than a copy.
//
// Note what is NOT freed here: the swap chain, device, context and buffers are
// slot 3's job, and the cursor is destroyed by slot 3 too. Calling slot 0
// without slot 3 leaks every COM object. That is the shipped contract.
// ============================================================
DeviceData* DeviceData_Dtor(DeviceData* self, unsigned flags) {
    const Imports* im = GetImports();
    if (!im) return Orig<DeviceData* (*)(DeviceData*, unsigned)>(0)(self, flags);

    self->vptr = Dx11VtableAddress(Vt::DeviceData);

    im->MemFree(self->trackedImages);
    self->trackedImages   = nullptr;
    self->trackedCount    = 0;
    self->trackedCapacity = 0;

    // ENDGAME B.3.5 block D2, RULE B5-3: `Video::DeviceData` is a real abstract
    // class (block A), so the base-dtor call is a QUALIFIED NON-VIRTUAL member
    // call, not a resolved import and never `DeletingDtor(0)` — that would
    // re-enter this slot. The cast is the bridge the declared leave (RULE B5-8)
    // requires: this class mirrors the base's two fields rather than deriving.
    // ENDGAME B.3.5 block J: `Imports::DeviceDataBaseDtor` and its table row are
    // GONE — see the note at the struct. The readiness test above still guards
    // `MemFree`, which this body really does resolve at run time.
    reinterpret_cast<Video::DeviceData*>(self)
        ->Video::DeviceData::~DeviceData();   // B.3.5 bridge

    if (flags & 1)
        im->MemFree(self);

    return self;
}

// ============================================================
// Slot 3 — 0x1800051c0  shutdown
//
//   UnregisterMessageCallback(app, 0x20)      // WM_SETCURSOR
//   UnregisterMessageCallback(app, 6)         // WM_ACTIVATE
//   if (cursor) DestroyCursor(cursor)
//   if (this->wasFullscreen) Configuration->fullscreen = true
//   Release(depth +0x30);  depth = NULL
//   Release(colour +0x38); colour = NULL
//   Release(context +0x28); context = NULL
//   if (swapChain) {
//       if (Configuration->fullscreen) swapChain->SetFullscreenState(FALSE, NULL)
//       Release(swapChain)
//   }
//   swapChain = NULL
//   Release(device +0x20); device = NULL
//
// Three things about this function are deliberate and reproduced:
//
//   * The wasFullscreen -> Configuration write happens FIRST, and the
//     SetFullscreenState guard then reads the value it just wrote. So quitting
//     while alt-tabbed out of fullscreen still restores VideoFullscreen=true in
//     the saved settings AND still leaves DXGI exclusive mode properly.
//   * +0x10 (the IDXGIFactory1) is NOT released here. Slot 2 already released
//     it as soon as the swap chain existed (0x1800050c2) and never nulled the
//     field, so DeviceData+0x10 is a dangling pointer for the whole session.
//     Releasing it here would be a double-release.
//   * +0x48 (the HCURSOR) is not nulled after DestroyCursor, so it too dangles.
//     Slot 8's identity check would then compare against a stale handle. Left
//     as-is; "fixing" it changes observable behaviour.
// ============================================================
void DeviceData_Shutdown(DeviceData* self) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(DeviceData*)>(3)(self); return; }

    void* app = AppOf(im);
    reinterpret_cast< ::Application::Application*>(app)->UnregisterMessageCallback(0x20);  // B.3.7 bridge
    reinterpret_cast< ::Application::Application*>(app)->UnregisterMessageCallback(6);     // B.3.7 bridge

    if (self->cursor)
        DestroyCursor(static_cast<HCURSOR>(self->cursor));

    void* cfg = ConfigOf(im);
    if (self->wasFullscreen != 0)
        Field<uint8_t>(cfg, Cfg::kFullscreen) = 1;

    Dx11Release(self->depthBuffer);
    self->depthBuffer = nullptr;

    Dx11Release(self->colorBuffer);
    self->colorBuffer = nullptr;

    Dx11Release(self->d3dContext);
    self->d3dContext = nullptr;

    if (self->swapChain) {
        IDXGISwapChain* sc = static_cast<IDXGISwapChain*>(self->swapChain);
        if (Field<uint8_t>(cfg, Cfg::kFullscreen) != 0)
            sc->SetFullscreenState(FALSE, nullptr);
        sc->Release();
    }
    self->swapChain = nullptr;

    Dx11Release(self->d3dDevice);
    self->d3dDevice = nullptr;
}

// ============================================================
// Slot 2 — 0x180004e10  create device + swap chain + buffers
//
// The riskiest function in the DLL: get it wrong and there is no window. Every
// step below is in the shipped order, with the shipped error strings (they are
// what the user sees when startup fails, so they are observable).
//
//  1. CreateDXGIFactory1(IID_IDXGIFactory1, &this->+0x10)
//  2. factory->EnumAdapters1(Device+0x14, &adapter)
//  3. adapter->EnumOutputs(Device+0x18, &output)
//  4. Device+0x04/+0x08 = modeTable[Device+0x0c].{width,height}
//  5. if (!Configuration->fullscreen) {
//         if (Configuration->windowWidth  >= 0) Device+0x04 = it
//         if (Configuration->windowHeight >= 0) Device+0x08 = it
//     }
//  6. if (!Configuration+0x42) ShowWindow(Device+0x90, SW_SHOWDEFAULT)
//  7. output->GetDesc(&od);  Device+0xa0 = od.Monitor
//  8. Video::Device::AdjustDisplay(device)
//  9. FillSwapChainDesc(&desc)
// 10. D3D11CreateDevice(adapter, UNKNOWN, NULL, 0, {11_0,10_1,10_0}, 3,
//                       D3D11_SDK_VERSION, &this->+0x20, &fl, &this->+0x28)
// 11. CheckFormatSupport(B8G8R8A8_UNORM) must have RENDER_TARGET (0x4000)
// 12. CheckFormatSupport(D24_UNORM_S8_UINT) must have DEPTH_STENCIL (0x10000)
// 13. factory->MakeWindowAssociation(Device+0x90, DXGI_MWA_NO_WINDOW_CHANGES)
// 14. factory->CreateSwapChain(d3dDevice, &desc, &this->+0x18)
// 15. Release adapter, output, factory
// 16. CreateColorBuffer() then CreateDepthBuffer()
// 17. RegisterMessageCallback(0x20, OnSetCursor)
//     RegisterMessageCallback(6,    OnActivate)
//
// Step 5 is the "-1 means don't override" sentinel: the test is a SIGN test
// (`TEST ECX,ECX; JS skip` at 0x180004f12 and 0x180004f21), and
// Video::Configuration's constructor really does default both fields to -1
// (Runtime 0x1800f6f10 stores 0xffffffffffffffff at +0x1c). Confirmed on both
// sides, not inferred from one.
//
// Step 8 is why the swap chain and the window always agree at creation time:
// AdjustDisplay physically SetWindowPos'es the HWND and then, in windowed mode,
// re-reads Device+0x04/+0x08 from GetClientRect. FillSwapChainDesc runs after
// it, so it sees the settled size.
//
// Failure handling, reproduced exactly:
//   * Only DXGI_ERROR_NOT_FOUND (0x887a0002) is treated as an enumeration
//     failure. Any OTHER non-zero HRESULT from EnumAdapters1/EnumOutputs falls
//     through and the code proceeds with whatever the out-pointer holds.
//   * Every failure path returns false WITHOUT releasing anything already
//     created. The factory, adapter, output, device and context all leak on a
//     failed startup. Reproduced: adding cleanup would change what a
//     post-failure DumpVideoState() sees.
// ============================================================
bool DeviceData_CreateDevice(DeviceData* self) {
    const Imports* im = GetImports();
    if (!im) return Orig<bool (*)(DeviceData*)>(2)(self);

    CreateDXGIFactory1_t createFactory = GetCreateDXGIFactory1();
    D3D11CreateDevice_t  createDevice  = GetD3D11CreateDevice();
    if (!createFactory || !createDevice) return Orig<bool (*)(DeviceData*)>(2)(self);

    void* dev = DeviceOf(self);
    StallDebug::Mark("  DX11 CreateDevice: entry");

    if (createFactory(kIID_IDXGIFactory1, &self->unk10) != 0) {
        System::Error::SetText("VideoDX11: Couldn't create a dxgi factory!", false);   // B.3.4 bridge
        return false;
    }
    IDXGIFactory1* factory = static_cast<IDXGIFactory1*>(self->unk10);

    IDXGIAdapter1* adapter = nullptr;
    if (factory->EnumAdapters1(static_cast<UINT>(Field<int32_t>(dev, Dev::kAdapterIndex)),
                               &adapter) == DXGI_ERROR_NOT_FOUND) {
        System::Error::SetText("VideoDX11: Unable to query adapter for dx11 interface!", false);   // B.3.4 bridge
        return false;
    }

    IDXGIOutput* output = nullptr;
    if (adapter->EnumOutputs(static_cast<UINT>(Field<int32_t>(dev, Dev::kOutputIndex)),
                             &output) == DXGI_ERROR_NOT_FOUND) {
        System::Error::SetText("VideoDX11: Unable to query output for dx11 display!", false);   // B.3.4 bridge
        return false;
    }

    StallDebug::Mark("  DX11 DXGI factory + adapter + output enum");

    void* cfg = ConfigOf(im);

    // 4 — start from the selected display mode.
    const DisplayMode* modeTable =
        static_cast<const DisplayMode*>(Field<void*>(dev, Dev::kModeTable));
    const int modeIndex = Field<int32_t>(dev, Dev::kModeIndex);
    Field<int32_t>(dev, Dev::kWidth)  = modeTable[modeIndex].width;
    Field<int32_t>(dev, Dev::kHeight) = modeTable[modeIndex].height;

    // 5 — windowed override, skipped for any negative value.
    if (Field<uint8_t>(cfg, Cfg::kFullscreen) == 0) {
        const int ww = Field<int32_t>(cfg, Cfg::kWindowWidth);
        if (ww >= 0) Field<int32_t>(dev, Dev::kWidth) = ww;
        const int wh = Field<int32_t>(cfg, Cfg::kWindowHeight);
        if (wh >= 0) Field<int32_t>(dev, Dev::kHeight) = wh;
    }

    // Arm the deferred reveal BEFORE step 6 reads the flag. This does not
    // change step 6's logic at all — it sets Configuration+0x42, which the
    // shipped branch below already honours, so CreateDevice takes the engine's
    // own skip-show path and standalone/boot_window.cpp reveals the window once
    // the splash has a frame to show. Without this the window is on screen and
    // empty for ~450ms every boot; see boot_window.h for the measurements.
    // No-op (and step 6 shows the window inline, as shipped) when
    // BANISHED_DEFER_WINDOW_SHOW=0.
    BootWindow::ArmDeferredShow(cfg, Field<HWND>(dev, Dev::kHwnd));

    // 6 — SW_SHOWDEFAULT (10) honours the STARTUPINFO the process was launched
    // with, so the resulting show-state is a property of the launcher, not of
    // this code. See the Task B note in the lane report.
    if (Field<uint8_t>(cfg, Cfg::kSkipShow) == 0) {
        ShowWindow(Field<HWND>(dev, Dev::kHwnd), SW_SHOWDEFAULT);
        // From here the engine window is ON SCREEN. Everything between this
        // point and the first splash Present is a visible, unpainted window,
        // so this mark is the start of the boot blank-window budget.
        StallDebug::Mark("  DX11 ShowWindow  <== WINDOW NOW VISIBLE");
    }

    // 7 — remember which monitor the chosen output is on; AdjustDisplay uses it
    // to position a fullscreen window.
    DXGI_OUTPUT_DESC od;
    output->GetDesc(&od);
    Field<HMONITOR>(dev, Dev::kMonitor) = od.Monitor;

    // 8 — settle the window, which may rewrite Device+0x04/+0x08.
    VDev(dev)->AdjustDisplay();
    StallDebug::Mark("  DX11 AdjustDisplay (window sized)");

    // 9
    DXGI_SWAP_CHAIN_DESC desc;
    FillSwapChainDesc(self, &desc, im);

    // 10
    const D3D_FEATURE_LEVEL levels[3] = {
        D3D_FEATURE_LEVEL_11_0,   // 0xb000
        D3D_FEATURE_LEVEL_10_1,   // 0xa100
        D3D_FEATURE_LEVEL_10_0,   // 0xa000
    };
    D3D_FEATURE_LEVEL granted = static_cast<D3D_FEATURE_LEVEL>(0);
    if (createDevice(adapter, static_cast<D3D_DRIVER_TYPE>(0), nullptr, 0,
                     levels, 3, D3D11_SDK_VERSION,
                     reinterpret_cast<ID3D11Device**>(&self->d3dDevice), &granted,
                     reinterpret_cast<ID3D11DeviceContext**>(&self->d3dContext)) != 0) {
        System::Error::SetText("VideoDX11: Could not initialize the DX11 device! "
                         "D3D_FEATURE_LEVEL_10_0 is required!", false);   // B.3.4 bridge
        return false;
    }

    StallDebug::Mark("  DX11 D3D11CreateDevice");

    ID3D11Device* d3d = static_cast<ID3D11Device*>(self->d3dDevice);

    // 11
    UINT support = 0;
    d3d->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM, &support);
    if ((support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) == 0) {
        System::Error::SetText("VideoDX11: Graphics hardware doesn't support "
                         "DXGI_FORMAT_B8G8R8A8_UNORM as a render target format!", false);   // B.3.4 bridge
        return false;
    }

    // 12
    d3d->CheckFormatSupport(DXGI_FORMAT_D24_UNORM_S8_UINT, &support);
    if ((support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) == 0) {
        System::Error::SetText("VideoDX11: Graphics hardware doesn't support "
                         "DXGI_FORMAT_D24_UNORM_S8_UINT as a depth stencil format!", false);   // B.3.4 bridge
        return false;
    }

    // 13
    factory->MakeWindowAssociation(Field<HWND>(dev, Dev::kHwnd),
                                   DXGI_MWA_NO_WINDOW_CHANGES);

    // 14
    if (factory->CreateSwapChain(d3d, &desc,
                                 reinterpret_cast<IDXGISwapChain**>(&self->swapChain)) != 0) {
        System::Error::SetText("VideoDX11: Could not initialize the swap chain!", false);   // B.3.4 bridge
        return false;
    }

    // 15 — adapter, output and factory are done with. The factory field is left
    // pointing at the released object (see slot 3's note).
    adapter->Release();
    output->Release();
    factory->Release();

    // 16
    if (!CreateColorBuffer(self, im)) {
        int aa = ResolveMultisampleMode(self, im);
        const System::String msg = System::String::Format(
            "VideoDX11: Failed to create color buffer with format "
            "DXGI_FORMAT_B8G8R8A8_UNORM [%ix%i %ixAA]",
            Field<int32_t>(dev, Dev::kWidth),
            Field<int32_t>(dev, Dev::kHeight), aa);
        System::Error::SetText(msg._data, false);   // B.3.4 bridge
        return false;
    }
    if (!CreateDepthBuffer(self, im)) {
        int aa = ResolveMultisampleMode(self, im);
        const System::String msg = System::String::Format(
            "VideoDX11: Failed to create a valid depth buffer format "
            "DXGI_FORMAT_D24_UNORM_S8_UINT [%ix%i %AA]!",
            Field<int32_t>(dev, Dev::kWidth),
            Field<int32_t>(dev, Dev::kHeight), aa);
        System::Error::SetText(msg._data, false);   // B.3.4 bridge
        return false;
    }

    StallDebug::Mark("  DX11 swapchain + color/depth buffers");

    // 17
    void* app = AppOf(im);
    reinterpret_cast< ::Application::Application*>(app)->RegisterMessageCallback(  // B.3.7 bridge
        0x20, reinterpret_cast< ::Application::Application::MessageCallback>(&DeviceData_OnSetCursor));
    reinterpret_cast< ::Application::Application*>(app)->RegisterMessageCallback(  // B.3.7 bridge
        6, reinterpret_cast< ::Application::Application::MessageCallback>(&DeviceData_OnActivate));
    return true;
}

// ============================================================
// Slot 8 — 0x180005450  SetImageBuffer(ImageBuffer*, int hotX, int hotY)
//
// Builds a Win32 HCURSOR from a 32x32 Video::ImageBuffer. No D3D11 at all.
//
// Early-out: if (imageBuffer, hotX, hotY) all match what is already cached in
// +0x50/+0x58/+0x5c, do nothing. Otherwise destroy the old cursor and rebuild.
//
// The pixel classification (0x180005520-0x180005549) is:
//
//   alpha == 0            -> set the bit in the AND plane  (transparent)
//   alpha != 0 && B != 0  -> set the bit in the XOR plane  (white)
//   alpha != 0 && B == 0  -> neither bit                   (black)
//
// It tests the BLUE channel, not luminance: `TEST DL,DL` on the low byte of a
// BGRA texel. A cursor drawn in pure red would come out black.
//
// Bit order: within each 32-pixel row the first pixel is bit 31 and the last is
// bit 0, then the row's dword is byte-reversed (0x180005553-0x1800055b4 is a
// hand-expanded bswap). Net effect: the leftmost pixel is the MSB of the first
// byte, which is what CreateCursor wants.
//
// The masks are 32 rows x 4 bytes = 128 bytes each, zeroed as one dword plus a
// 124-byte memset (0x1800054e2 / 0x1800054e6) — a 128-byte memset is the same
// thing.
//
// Note the source read is bracketed by Video::Device::Lock/Unlock, because
// CopyFromTexture stages through the immediate context.
// ============================================================
void DeviceData_SetImageBuffer(DeviceData* self, void* imageBuffer, int hotX, int hotY) {
    const Imports* im = GetImports();
    if (!im) { Orig<void (*)(DeviceData*, void*, int, int)>(8)(self, imageBuffer, hotX, hotY); return; }

    if (imageBuffer == self->cursorImage &&
        hotX == self->cursorHotspotX &&
        hotY == self->cursorHotspotY)
        return;

    if (self->cursor)
        DestroyCursor(static_cast<HCURSOR>(self->cursor));

    void* dev = DeviceOf(self);

    uint32_t pixels[32 * 32];
    VDev(dev)->Lock();
    VImg(imageBuffer)->CopyFromTexture(pixels, 32, 32);
    VDev(dev)->Unlock();

    uint32_t andPlane[32];
    uint32_t xorPlane[32];
    std::memset(andPlane, 0, sizeof(andPlane));
    std::memset(xorPlane, 0, sizeof(xorPlane));

    int flat = 0;
    for (int row = 0; row < 32; ++row) {
        for (int bit = 31; bit >= 0; --bit, ++flat) {
            const uint32_t texel = pixels[flat];
            const uint32_t mask  = 1u << bit;
            if ((texel & 0xff000000u) == 0)
                andPlane[row] |= mask;                 // transparent
            else if ((texel & 0xffu) != 0)
                xorPlane[row] |= mask;                 // white
        }
        // The hand-expanded byte reversal at 0x180005553.
        const uint32_t a = andPlane[row];
        andPlane[row] = (a >> 24) | ((a >> 8) & 0xff00u) |
                        ((a << 8) & 0xff0000u) | (a << 24);
        const uint32_t x = xorPlane[row];
        xorPlane[row] = (x >> 24) | ((x >> 8) & 0xff00u) |
                        ((x << 8) & 0xff0000u) | (x << 24);
    }

    HCURSOR cursor = CreateCursor(
        static_cast<HINSTANCE>(
            reinterpret_cast<const ::Application::Application*>(AppOf(im))->GetInstance()),  // B.3.7 bridge
        hotX, hotY, 32, 32, andPlane, xorPlane);
    self->cursor = cursor;
    SetCursor(cursor);

    self->cursorImage    = imageBuffer;
    self->cursorHotspotX = hotX;
    self->cursorHotspotY = hotY;
}

// ============================================================
// 0x180005b00 — Collection::Array<GraphicsInterface>::SetLength(int)
//
// The generic grower, specialised for GraphicsInterface (stride 0x18). Growth
// rule, transcribed: when the request exceeds capacity, capacity becomes the
// request — EXCEPT when the request is exactly oldCount+1 and oldCount was
// non-zero, in which case it becomes request*3. So the first append allocates
// exactly 1 and every later append triples. Reproduced because the allocation
// sizes are observable in a heap trace.
//
// New elements are copy-constructed from `proto`; carried-over elements are
// copy-constructed from the old block, which is then destroyed and freed. When
// the block did NOT move, only the tail past the new length is destroyed.
// ============================================================
void GfxInterfaceArraySetLength(void* arrayBase, int newCount,
                                const GraphicsInterface* proto, const Imports* im) {
    GraphicsInterface** dataP = reinterpret_cast<GraphicsInterface**>(arrayBase);
    int32_t* countP = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(arrayBase) + 8);
    int32_t* capP   = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(arrayBase) + 12);

    const int oldCount = *countP;
    if (newCount == oldCount) return;

    GraphicsInterface* oldBase = *dataP;
    *countP = newCount;

    if (newCount > *capP) {
        int cap = newCount;
        *capP = cap;
        if (newCount == oldCount + 1 && oldCount != 0) {
            cap = newCount * 3;
            *capP = cap;
        }
        *dataP = static_cast<GraphicsInterface*>(
            im->MemAlloc(static_cast<size_t>(cap) * sizeof(GraphicsInterface), 8));
    }

    GraphicsInterface* base = *dataP;
    int i = (oldBase == base) ? oldCount : 0;
    for (; i < newCount; ++i) {
        GraphicsInterface* dst = base + i;
        if (!dst) continue;
        const GraphicsInterface* src = (i < oldCount) ? (oldBase + i) : proto;
        im->GfxIfaceCopyCtor(dst, src);
    }

    if (oldBase != base) {
        for (int j = 0; j < oldCount; ++j)
            im->GfxIfaceDtor(oldBase + j);
        im->MemFree(oldBase);
    } else {
        for (int j = newCount; j < oldCount; ++j)
            im->GfxIfaceDtor(oldBase + j);
    }
}

// The same grower, inlined into slot 1 for Adapter (stride 0x38) with the
// identical growth rule. The shipped code has two separate expansions of the
// same template; they are byte-for-byte the same algorithm.
void AdapterArraySetLength(GraphicsInterface* gi, int newCount,
                           const Adapter* proto, const Imports* im) {
    const int oldCount = gi->adapterCount;
    if (newCount == oldCount) return;

    Adapter* oldBase = gi->adapterData;
    gi->adapterCount = newCount;

    if (newCount > gi->adapterCapacity) {
        int cap = newCount;
        gi->adapterCapacity = cap;
        if (newCount == oldCount + 1 && oldCount != 0) {
            cap = newCount * 3;
            gi->adapterCapacity = cap;
        }
        gi->adapterData = static_cast<Adapter*>(
            im->MemAlloc(static_cast<size_t>(cap) * sizeof(Adapter), 8));
    }

    Adapter* base = gi->adapterData;
    int i = (oldBase == base) ? oldCount : 0;
    for (; i < newCount; ++i) {
        Adapter* dst = base + i;
        if (!dst) continue;
        const Adapter* src = (i < oldCount) ? (oldBase + i) : proto;
        im->AdapterCopyCtor(dst, src);
    }

    if (oldBase != base) {
        for (int j = 0; j < oldCount; ++j)
            im->AdapterDtor(oldBase + j);
        im->MemFree(oldBase);
    } else {
        for (int j = newCount; j < oldCount; ++j)
            im->AdapterDtor(oldBase + j);
    }
}

// ============================================================
// Slot 1 — 0x180004690  enumerate adapters / outputs / display modes
//
// The largest function in the DLL (494 instructions). It builds ONE
// Video::Device::GraphicsInterface named "DirectX 11" holding one
// Video::Device::Adapter per (DXGI adapter, DXGI output) PAIR that has at least
// one usable display mode, and appends it to Device+0x40.
//
// Per output it records: a display name, the filtered mode list, a 32-entry
// MSAA support table, the max anisotropy, and the (adapterIndex, outputIndex)
// that slot 2 will later re-enumerate with.
//
//   for (a = 0; factory->EnumAdapters1(a, &ad) != NOT_FOUND; ++a) {
//       ad->GetDesc1(&d1)
//       for (o = 0; ad->EnumOutputs(o, &out) != NOT_FOUND; ++o) {
//           Adapter rec;                             // fresh each iteration
//           rec.name = WideString(d1.Description).AsUTF8()
//           rec.maxAnisotropy = 16
//           rec.adapterIndex = a; rec.outputIndex = o
//           out->GetDesc(&od)
//           GetMonitorInfoW(od.Monitor, &mi)         // MONITORINFOEXW, cbSize 0x68
//           rec.name += " (Display " + AsUTF8(&mi.szDevice[len-1]) + ")"
//           out->GetDisplayModeList(28, 0, &n, NULL)
//           modes = MemAlloc(n * 0x1c, 4)
//           out->GetDisplayModeList(28, 0, &n, modes)
//           for each mode with Width >= 800:
//               rec.FindOrAddMode(DisplayMode(w,h), RefreshRate{num,den})
//           out->Release()
//           if (rec.modeCount != 0) {
//               D3D11CreateDevice(ad, ...)           // probe device, discarded
//               resize rec.msaaSupported to 32 bytes
//               for (s = 1; s <= 32; ++s)
//                   rec.msaaSupported[s-1] =
//                       CheckMultisampleQualityLevels(0x57, s, &q), q != 0
//               ctx->Release(); dev->Release()
//               gi.adapters.SetLength(gi.adapterCount + 1)   // appends rec
//           }
//           MemFree(modes)
//       }
//       ad->Release()
//   }
//   factory->Release()
//   if (gi.adapterCount > 0)
//       Device.interfaces.SetLength(Device.interfaceCount + 1, &gi)
//   return gi.adapterCount > 0
//
// Notes on fidelity:
//
//   * The display-mode format is 28 (R8G8B8A8_UNORM), NOT the 0x57
//     (B8G8R8A8_UNORM) everything else in the DLL uses (`LEA EDX,[R8+0x1c]` at
//     0x180004916 with R8 = 0). Same mismatch as the resize helper's
//     ResizeTarget. Reproduced.
//   * The MSAA probe uses 0x57, so the two formats really are used side by side
//     in the same function.
//   * The 800-pixel floor is an unsigned compare (`CMP dword ptr [RBX],0x320;
//     JC`) against the WIDTH only; height is unfiltered.
//   * The name suffix takes `&szDevice[len-1]` — the LAST wide character of the
//     device name, i.e. the display number off the end of "\\.\DISPLAY3". That
//     is why the string reads " (Display 3)".
//   * The shipped code contains an inlined Array<DXGI_MODE_DESC>::SetLength
//     whose copy loop is provably dead (its guard is `index >= 0`, always true
//     for a loop counting up from zero) and whose MemFree argument is provably
//     the null old base. Both are omitted here; neither has an observable
//     effect, and writing them out would be transcribing decompiler noise
//     rather than behaviour.
//   * The probe D3D11CreateDevice's HRESULT failure path SKIPS the append but
//     still frees the mode list and destroys the record. Reproduced.
//   * On the probe-device path the original does NOT check the ID3D11Device
//     out-pointer for null before calling through it.
// ============================================================
// The adapter/output/mode walk, factored out of DeviceData_EnumerateInterfaces
// so the cutover-only DX9 placeholder below can list the SAME adapters under
// its own name (on a retail machine the DX9 plugin enumerates the same GPUs and
// modes through d3d9). Byte-for-byte the shipped walk; only `dev` and the
// interface name are parameters now.
static bool EnumerateDxgiInterfaceInto(void* dev, const char* interfaceName,
                                       const Imports* im,
                                       CreateDXGIFactory1_t createFactory,
                                       D3D11CreateDevice_t  createDevice) {
    GraphicsInterface gi;
    im->GfxIfaceCtor(&gi);
    {
        // B.3.1 bridge: GraphicsInterface::name is a POD slot on a struct this
        // plugin shares with the engine and that is not converted yet.
        const System::String name(interfaceName);
        reinterpret_cast<System::String&>(gi) = name;
    }

    IDXGIFactory1* factory = nullptr;
    if (createFactory(kIID_IDXGIFactory1, reinterpret_cast<void**>(&factory)) != 0) {
        im->GfxIfaceDtor(&gi);
        return false;
    }

    for (UINT adapterIndex = 0; ; ++adapterIndex) {
        IDXGIAdapter1* dxgiAdapter = nullptr;
        if (factory->EnumAdapters1(adapterIndex, &dxgiAdapter) == DXGI_ERROR_NOT_FOUND)
            break;

        DXGI_ADAPTER_DESC1 adapterDesc;
        dxgiAdapter->GetDesc1(&adapterDesc);

        for (UINT outputIndex = 0; ; ++outputIndex) {
            // A fresh record per output, seeded from the adapter description.
            Adapter rec;
            im->AdapterCtor(&rec);
            {
                WideStringStorage w;
                ::new (static_cast<void*>(&w)) System::WideString(adapterDesc.Description);   // B.3.4 bridge
                {
                    // ENDGAME B.3.5 block J: `?AsUTF8@WideString@System@@` is a
                    // real const member returning `System::String` by value, so
                    // the local is DIRECT-INITIALISED from the prvalue — one
                    // construction into the storage the door used to be handed as
                    // a retslot, instead of a default construction plus that
                    // write. The inner scope still releases it before the
                    // WideString, which is the shipped order.
                    const System::String utf8 =
                        reinterpret_cast<const System::WideString*>(&w)->AsUTF8();  // B.3.4 bridge
                    reinterpret_cast<System::String&>(rec) = utf8;  // B.3.1 bridge
                }
                reinterpret_cast<System::WideString*>(&w)->~WideString();   // B.3.4 bridge
            }
            // ENDGAME B.3.5 block D2: the real class's field names. These three
            // stores are the EVIDENCE that Adapter+0x28/+0x2c/+0x30 are the
            // anisotropy cap and the two DXGI enumeration coordinates; Device.h
            // records that and renames its two "meaning not recovered" fields.
            rec.maxFilterLevel   = 0x10;
            rec.dxgiAdapterIndex = static_cast<int32_t>(adapterIndex);
            rec.dxgiOutputIndex  = static_cast<int32_t>(outputIndex);

            IDXGIOutput* output = nullptr;
            if (dxgiAdapter->EnumOutputs(outputIndex, &output) == DXGI_ERROR_NOT_FOUND) {
                im->AdapterDtor(&rec);
                break;
            }

            DXGI_OUTPUT_DESC outDesc;
            output->GetDesc(&outDesc);

            MONITORINFOEXW mi;
            std::memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(mi);                       // 0x68
            GetMonitorInfoW(outDesc.Monitor, &mi);

            // rec.name += " (Display " + last-char-of(szDevice) + ")"
            // Temporary construction and destruction order is the shipped one
            // (0x180004829-0x1800048e8): prefix, wide, utf8, t1, t2, t3, then
            // destroyed t3, t2, t1, utf8, wide, prefix.
            {
                // The nested scopes below reproduce the shipped destruction
                // order exactly — t3, t2, t1, tailU, tailW, prefix — with each
                // System::String released by its own destructor. `tailW` is a
                // System::WideString, which is not a B.3.1 class, so it keeps
                // its explicit dtor call, placed where the shipped one is.
                const System::String prefix(" (Display ");
                {
                    const int len = System::WideString::StrLength(mi.szDevice);   // B.3.4 bridge
                    WideStringStorage tailW;
                    ::new (static_cast<void*>(&tailW)) System::WideString(mi.szDevice + (len - 1));   // B.3.4 bridge
                    {
                        // Same shape as the adapter description above.
                        const System::String tailU =
                            reinterpret_cast<const System::WideString*>(&tailW)->AsUTF8();  // B.3.4 bridge
                        {
                            const System::String t1 = prefix + tailU;
                            const System::String t2 = t1 + ")";
                            // operator+= returns the new value BY VALUE and the
                            // caller discards the copy; that copy is retail's t3.
                            const System::String t3 =
                                (reinterpret_cast<System::String&>(rec) += t2);
                            (void)t3;
                        }                       // t3, t2, t1
                    }                           // tailU
                    reinterpret_cast<System::WideString*>(&tailW)->~WideString();   // B.3.4 bridge
                }
                                                // prefix
            }

            // Display modes. Format 28 is what the shipped code asks for.
            UINT modeCount = 0;
            DXGI_MODE_DESC* modes = nullptr;
            if (output->GetDisplayModeList(static_cast<DXGI_FORMAT>(28), 0,
                                           &modeCount, nullptr) == 0) {
                if (modeCount != 0)
                    modes = static_cast<DXGI_MODE_DESC*>(
                        im->MemAlloc(static_cast<size_t>(modeCount) * sizeof(DXGI_MODE_DESC), 4));

                if (output->GetDisplayModeList(static_cast<DXGI_FORMAT>(28), 0,
                                               &modeCount, modes) == 0) {
                    for (UINT i = 0; i < modeCount; ++i) {
                        const DXGI_MODE_DESC& m = modes[i];
                        if (m.Width < 800) continue;
                        DisplayMode dm;
                        im->DisplayModeCtor(&dm, static_cast<int>(m.Width),
                                            static_cast<int>(m.Height));
                        RefreshRate rr;
                        rr.numerator   = static_cast<int32_t>(m.RefreshRate.Numerator);
                        rr.denominator = static_cast<int32_t>(m.RefreshRate.Denominator);
                        rec.FindOrAddMode(dm, rr);
                        im->DisplayModeDtor(&dm);
                    }
                }
            }
            output->Release();

            if (rec.modeCount != 0) {
                const D3D_FEATURE_LEVEL levels[3] = {
                    D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
                };
                ID3D11Device*        probeDev = nullptr;
                ID3D11DeviceContext* probeCtx = nullptr;
                D3D_FEATURE_LEVEL    granted  = static_cast<D3D_FEATURE_LEVEL>(0);

                if (createDevice(dxgiAdapter, static_cast<D3D_DRIVER_TYPE>(0), nullptr, 0,
                                 levels, 3, D3D11_SDK_VERSION,
                                 &probeDev, &granted, &probeCtx) == 0) {
                    // Grow the MSAA table to 32 bytes. Same growth rule as the
                    // other two arrays; from a fresh Adapter this is a plain
                    // MemAlloc(32, 1) with nothing to carry over.
                    if (rec.multisampleCount != 0x20) {
                        uint8_t* oldBase = rec.multisampleData;
                        const int oldCount = rec.multisampleCount;
                        rec.multisampleCount = 0x20;
                        if (rec.multisampleCap < 0x20) {
                            int cap = 0x20;
                            rec.multisampleCap = cap;
                            if (oldCount + 1 == 0x20 && oldCount != 0) {
                                cap = 0x60;
                                rec.multisampleCap = cap;
                            }
                            rec.multisampleData = static_cast<uint8_t*>(
                                im->MemAlloc(static_cast<size_t>(cap), 1));
                        }
                        unsigned char* base = rec.multisampleData;
                        int i = (oldBase == rec.multisampleData) ? oldCount : 0;
                        for (; i < 0x20; ++i)
                            if (i < oldCount)
                                base[i] = oldBase[i];
                        if (oldBase != rec.multisampleData)
                            im->MemFree(oldBase);
                    }

                    unsigned char* msaa = rec.multisampleData;
                    for (int sampleCount = 1; sampleCount <= 0x20; ++sampleCount) {
                        UINT quality = 0;
                        probeDev->CheckMultisampleQualityLevels(
                            DXGI_FORMAT_B8G8R8A8_UNORM,
                            static_cast<UINT>(sampleCount), &quality);
                        msaa[sampleCount - 1] = (quality != 0) ? 1 : 0;
                    }

                    probeCtx->Release();
                    probeDev->Release();

                    AdapterArraySetLength(&gi, gi.adapterCount + 1, &rec, im);
                }
            }

            im->MemFree(modes);
            im->AdapterDtor(&rec);
        }

        dxgiAdapter->Release();
    }

    factory->Release();

    const bool any = gi.adapterCount > 0;
    if (any) {
        void* arrayBase = static_cast<uint8_t*>(dev) + Dev::kInterfaces;
        GfxInterfaceArraySetLength(arrayBase,
                                   Field<int32_t>(dev, Dev::kInterfaceCount) + 1,
                                   &gi, im);
    }

    im->GfxIfaceDtor(&gi);
    return any;
}

bool DeviceData_EnumerateInterfaces(DeviceData* self) {
    const Imports* im = GetImports();
    if (!im) return Orig<bool (*)(DeviceData*)>(1)(self);

    CreateDXGIFactory1_t createFactory = GetCreateDXGIFactory1();
    D3D11CreateDevice_t  createDevice  = GetD3D11CreateDevice();
    if (!createFactory || !createDevice) return Orig<bool (*)(DeviceData*)>(1)(self);

    return EnumerateDxgiInterfaceInto(DeviceOf(self), "DirectX 11", im,
                                      createFactory, createDevice);
}

// ============================================================
// The DX9 PLACEHOLDER slot -- cutover only
//
// VideoDX9-x64.dll imports Runtime-x64.dll, so with the Runtime DLL absent it
// cannot load and slot 0 stays empty. Retail then appends nothing for it and
// the Device's GraphicsInterface array holds ONE entry (ours, DX11) while the
// configuration's interface index -- a SLOT index, 1 -- stays 1: the Options
// dialog's PopulateResolutionCombo (retail 0x140073ec0) does
// GetInterface(cfgIndex) with no bounds check and reads past the array. Retail
// never meets this because both plugin DLLs always load and each appends one
// entry, so array index == slot index. This placeholder keeps that invariant:
// it appends a "DirectX 9" interface listing the SAME adapters and modes the
// DX11 walk finds (which is what the real plugin reports on a machine that has
// both). Its Create() returns false, so choosing it in Options leaves the
// fallback loop in Device::PlatformStartup to step back to DX11. Filled only by
// VideoInterfaceFillDx9Placeholder (standalone/video_interface.cpp), which the
// ported InitializeInterfaces calls only when the DLL is absent.
// ============================================================
namespace {
struct Dx9Placeholder { void* vptr; void* device; };

void Dx9P_DeletingDtor(Dx9Placeholder* self, int flag) {   // vtable +0x00
    if (flag & 1) delete self;
}
bool Dx9P_EnumerateInterfaces(Dx9Placeholder* self) {       // vtable +0x08
    const Imports* im = GetImports();
    CreateDXGIFactory1_t createFactory = GetCreateDXGIFactory1();
    D3D11CreateDevice_t  createDevice  = GetD3D11CreateDevice();
    if (!im || !createFactory || !createDevice) return false;
    // The same adapters and modes DX11 lists, under the DX9 name: what the
    // real plugin reports on this machine. A zero-adapter entry was tried
    // first and selecting it in Options died in UserInterface::Draw.
    return EnumerateDxgiInterfaceInto(self->device, "DirectX 9", im,
                                      createFactory, createDevice);
}
bool Dx9P_Create(Dx9Placeholder*)  { return false; }        // vtable +0x10
void Dx9P_Destroy(Dx9Placeholder*) {}                       // vtable +0x18

void* g_dx9PlaceholderVtable[4] = {
    reinterpret_cast<void*>(&Dx9P_DeletingDtor),
    reinterpret_cast<void*>(&Dx9P_EnumerateInterfaces),
    reinterpret_cast<void*>(&Dx9P_Create),
    reinterpret_cast<void*>(&Dx9P_Destroy),
};
}  // namespace

void*       Dx9P_CreateImpl(void* device) {
    return new Dx9Placeholder{ g_dx9PlaceholderVtable, device };
}
const char* Dx9P_NameImpl() { return "DirectX 9"; }

// ============================================================
// Slots 9..19 — the eleven resource factories
//
// Every one is the same five steps:
//     p = MemAlloc(sizeof, 4)
//     if (!p) return NULL
//     Video::<Class>Data::<Class>Data(p, owner)     // the Runtime base ctor
//     p->vptr = <the DX11 vtable>                   // overwrite the base's
//     zero the DX11-private tail
//     return p
//
// The address <-> class pairing was verified TWICE, independently:
//   1. Ghidra's recovered signature for each factory names both the return type
//      and the Runtime base ctor it calls.
//   2. The CALL displacement in each factory resolves through the derived IAT
//      base (0x180013218) to exactly the matching ??0<Class>Data mangled name
//      in objdump's import listing.
// Both agree with the order dx11/README.md already recorded. Table:
//
//   slot  factory       size  vtable       base ctor IAT  class
//   ----  ------------  ----  -----------  -------------  ------------------
//    9    0x180005650   0x20  0x1800186f0  0x180013478    ContextData
//   10    0x1800056c0   0x38  0x180018790  0x1800133e8    ImageBufferData
//   11    0x180005730   0x28  0x1800185d0  0x180013490    ConstantBufferData
//   12    0x1800057a0   0x20  0x1800187c8  0x180013390    IndexBufferData
//   13    0x180005810   0x20  0x180018830  0x180013340    VertexBufferData
//   14    0x180005880   0x18  0x180018878  0x180013330    VertexLayoutData
//   15    0x1800058f0   0x30  0x180018890  0x180013320    VertexProgramData
//   16    0x180005960   0x30  0x180018800  0x180013360    PixelProgramData
//   17    0x1800059d0   0x28  0x180018818  0x180013350    RenderStateData
//   18    0x180005a40   0x10  0x1800185a8  0x1800134a0    VideoProgramData
//   19    0x180005aa0   0x10  0x180018588  0x1800134b0    RenderBufferData
//
// Two initialisation values are NOT zero and matter:
//   * ConstantBufferData+0x10 is set to -1 (0x180005758), the "no rung bound
//     yet" sentinel L1 documented.
//   * ContextData+0x10 is set to 0 as a DWORD, which is
//     D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED and therefore never equal to a
//     topology the class would set — L5 relies on that.
// Index/VertexBufferData write +0x18 as a dword and +0x1c as a single byte
// (0x1800057d8 region), leaving +0x1d..+0x1f untouched; that is reproduced by
// assigning the named fields rather than memsetting the tail.
//
// The `this` argument (RCX) is the DeviceData and is ignored by all eleven.
// ============================================================
template <typename T>
T* AllocData(size_t size, void* (*baseCtor)(void*, void*), void* owner,
             Vt vt, const Imports* im) {
    T* p = static_cast<T*>(im->MemAlloc(size, 4));
    if (!p) return nullptr;
    baseCtor(p, owner);
    p->vptr = Dx11VtableAddress(vt);
    return p;
}

#define DX11_FACTORY_PROLOGUE(slotIndex)                                   \
    const Imports* im = GetImports();                                      \
    if (!im) return Orig<void* (*)(DeviceData*, void*)>(slotIndex)(self, owner)

void* Factory_ContextData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(9);
    ContextData* p = AllocData<ContextData>(0x20, im->CtorContextData, owner,
                                            Vt::ContextData, im);
    if (!p) return nullptr;
    p->topology   = 0;
    p->pad14      = 0;
    p->d3dContext = nullptr;
    return p;
}

void* Factory_ImageBufferData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(10);
    ImageBufferData* p = AllocData<ImageBufferData>(0x38, im->CtorImageBufferData, owner,
                                                    Vt::ImageBufferData, im);
    if (!p) return nullptr;
    p->sampler            = nullptr;
    p->texture            = nullptr;
    p->renderTargetView   = nullptr;
    p->depthStencilView   = nullptr;
    p->shaderResourceView = nullptr;
    return p;
}

void* Factory_ConstantBufferData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(11);
    ConstantBufferData* p = AllocData<ConstantBufferData>(0x28, im->CtorConstantBufferData,
                                                          owner, Vt::ConstantBufferData, im);
    if (!p) return nullptr;
    p->activeIndex    = -1;          // sentinel: no rung bound yet
    p->pad14          = 0;
    p->buffers        = nullptr;
    p->bufferCount    = 0;
    p->bufferCapacity = 0;
    // Join the reset registry so DeviceData_Resize can drop this object's
    // "already bound" cache when it calls ClearState(). See the note above
    // Dx11InvalidateConstantBufferBindings in buffers.cpp.
    Dx11TrackConstantBuffer(p);
    return p;
}

void* Factory_IndexBufferData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(12);
    IndexBufferData* p = AllocData<IndexBufferData>(0x20, im->CtorIndexBufferData, owner,
                                                    Vt::IndexBufferData, im);
    if (!p) return nullptr;
    p->buffer      = nullptr;
    p->writeCursor = 0;
    p->locked      = 0;
    return p;
}

void* Factory_VertexBufferData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(13);
    VertexBufferData* p = AllocData<VertexBufferData>(0x20, im->CtorVertexBufferData, owner,
                                                      Vt::VertexBufferData, im);
    if (!p) return nullptr;
    p->buffer      = nullptr;
    p->writeCursor = 0;
    p->locked      = 0;
    return p;
}

void* Factory_VertexLayoutData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(14);
    VertexLayoutData* p = AllocData<VertexLayoutData>(0x18, im->CtorVertexLayoutData, owner,
                                                      Vt::VertexLayoutData, im);
    if (!p) return nullptr;
    p->inputLayout = nullptr;
    return p;
}

void* Factory_VertexProgramData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(15);
    VertexProgramData* p = AllocData<VertexProgramData>(0x30, im->CtorVertexProgramData, owner,
                                                        Vt::VertexProgramData, im);
    if (!p) return nullptr;
    for (int i = 0; i < 4; ++i) p->variants[i] = nullptr;
    return p;
}

void* Factory_PixelProgramData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(16);
    PixelProgramData* p = AllocData<PixelProgramData>(0x30, im->CtorPixelProgramData, owner,
                                                      Vt::PixelProgramData, im);
    if (!p) return nullptr;
    for (int i = 0; i < 4; ++i) p->variants[i] = nullptr;
    return p;
}

void* Factory_RenderStateData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(17);
    RenderStateData* p = AllocData<RenderStateData>(0x28, im->CtorRenderStateData, owner,
                                                    Vt::RenderStateData, im);
    if (!p) return nullptr;
    p->blendState        = nullptr;
    p->depthStencilState = nullptr;
    p->rasterizerState   = nullptr;
    return p;
}

void* Factory_VideoProgramData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(18);
    return AllocData<VideoProgramData>(0x10, im->CtorVideoProgramData, owner,
                                       Vt::VideoProgramData, im);
}

void* Factory_RenderBufferData(DeviceData* self, void* owner) {
    DX11_FACTORY_PROLOGUE(19);
    return AllocData<RenderBufferData>(0x10, im->CtorRenderBufferData, owner,
                                       Vt::RenderBufferData, im);
}

#undef DX11_FACTORY_PROLOGUE

}  // namespace

// The one function in this lane with a caller outside it. 0x180004340 is a near
// CALL from ContextData::BeginCommands, not a vtable dispatch, so the seam has
// no way to redirect it — L5 has to name our port directly, and it can only do
// that through a declaration with external linkage. Everything else here stays
// in the anonymous namespace.
int Dx11_DeviceData_Resize(void* deviceData) {
    return DeviceData_Resize(static_cast<DeviceData*>(deviceData));
}

}  // namespace VideoDX11

// ============================================================
// Original quirks reproduced (do not "fix" these)
//
//  1. Slot 1 and the resize helper ask DXGI for format 28
//     (R8G8B8A8_UNORM) when enumerating display modes and when calling
//     ResizeTarget, while every buffer the backend creates is 0x57
//     (B8G8R8A8_UNORM). Two different formats, same code paths.
//  2. Slot 2 releases the IDXGIFactory1 but never nulls DeviceData+0x10, so
//     that field dangles for the whole session; slot 3 correctly does not
//     release it again.
//  3. Slot 3 destroys the cursor without nulling DeviceData+0x48.
//  4. Slot 2's failure paths leak the factory, adapter, output, D3D11 device
//     and context.
//  5. Slot 2 and slot 1 treat ONLY DXGI_ERROR_NOT_FOUND as an enumeration
//     failure; any other non-zero HRESULT proceeds with an unvalidated pointer.
//  6. ResolveMultisampleMode's downward walk has no lower bound.
//  7. Slot 8 classifies cursor pixels off the BLUE channel alone.
//  7b. The depth-buffer failure message's format string is
//     "... [%ix%i %AA]!" — `%A` is a real printf conversion (hex float), so it
//     consumes the int sample count as a double. The colour-buffer message
//     next to it correctly says "%ixAA". Transcribed verbatim: it is a shipped
//     string on a shipped failure path.
//  8. Video::Device::AdjustDisplay (Runtime, not ours) builds its off-screen
//     probe rectangle with `bottom = rc.bottom + X` — the X offset, not Y.
//     Noted here because it affects where a restored window lands; not our code
//     to change.
//
// ============================================================
// Registration — all 16 slots, graduated default-on.
//
// Slots 1, 2 and 3 are the three that can end a session with no window. They
// were verified in-game and RenderDoc-diffed against the shipped DLL before
// graduating; see dx11/README.md §"Per-draw pipeline-state equality".
// ============================================================

DX11_REGISTER_SLOT(DeviceData, 0, &::VideoDX11::DeviceData_Dtor,
                   "DeviceData::~DeviceData (scalar deleting)");
DX11_REGISTER_SLOT(DeviceData, 1, &::VideoDX11::DeviceData_EnumerateInterfaces,
                   "DeviceData::EnumerateInterfaces");
DX11_REGISTER_SLOT(DeviceData, 2, &::VideoDX11::DeviceData_CreateDevice,
                   "DeviceData::CreateDevice");
DX11_REGISTER_SLOT(DeviceData, 3, &::VideoDX11::DeviceData_Shutdown, "DeviceData::Shutdown");
DX11_REGISTER_SLOT(DeviceData, 8, &::VideoDX11::DeviceData_SetImageBuffer,
                   "DeviceData::SetImageBuffer (HCURSOR)");
DX11_REGISTER_SLOT(DeviceData, 9, &::VideoDX11::Factory_ContextData,
                   "DeviceData::CreateContextData");
DX11_REGISTER_SLOT(DeviceData, 10, &::VideoDX11::Factory_ImageBufferData,
                   "DeviceData::CreateImageBufferData");
DX11_REGISTER_SLOT(DeviceData, 11, &::VideoDX11::Factory_ConstantBufferData,
                   "DeviceData::CreateConstantBufferData");
DX11_REGISTER_SLOT(DeviceData, 12, &::VideoDX11::Factory_IndexBufferData,
                   "DeviceData::CreateIndexBufferData");
DX11_REGISTER_SLOT(DeviceData, 13, &::VideoDX11::Factory_VertexBufferData,
                   "DeviceData::CreateVertexBufferData");
DX11_REGISTER_SLOT(DeviceData, 14, &::VideoDX11::Factory_VertexLayoutData,
                   "DeviceData::CreateVertexLayoutData");
DX11_REGISTER_SLOT(DeviceData, 15, &::VideoDX11::Factory_VertexProgramData,
                   "DeviceData::CreateVertexProgramData");
DX11_REGISTER_SLOT(DeviceData, 16, &::VideoDX11::Factory_PixelProgramData,
                   "DeviceData::CreatePixelProgramData");
DX11_REGISTER_SLOT(DeviceData, 17, &::VideoDX11::Factory_RenderStateData,
                   "DeviceData::CreateRenderStateData");
DX11_REGISTER_SLOT(DeviceData, 18, &::VideoDX11::Factory_VideoProgramData,
                   "DeviceData::CreateVideoProgramData");
DX11_REGISTER_SLOT(DeviceData, 19, &::VideoDX11::Factory_RenderBufferData,
                   "DeviceData::CreateRenderBufferData");

// The two placeholder entry points, with EXTERNAL linkage (the bodies above sit
// in this file's unnamed namespace, as every DX11 body does).
namespace VideoDX11 {
void*       Dx9Placeholder_CreateDeviceData(void* device) { return Dx9P_CreateImpl(device); }
const char* Dx9Placeholder_GetDeviceName()                { return Dx9P_NameImpl(); }
}  // namespace VideoDX11
