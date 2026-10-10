# Phase D.4 seam plan: RENDER (D3D11 over DXVK)

Decision 4 (ENDGAME.md §E, NEXT_SESSION.md §2.2): keep `src/render/dx11/` as the D3D11 implementation and
run it over DXVK (D3D11 -> Vulkan) on non-Windows first; a native Vulkan backend only if DXVK proves
insufficient. Tree survey at main `4e36b7000`; DXVK facts checked against upstream on 2026-10-10 and cited
by URL. **UNKNOWN** marks what neither the tree nor upstream settles, with the step that settles it.

Upstream sources (cited below by tag):
- [README] https://github.com/doitsujin/dxvk (README.md, master)
- [OPTS] https://raw.githubusercontent.com/doitsujin/dxvk/master/meson_options.txt
- [REL] https://api.github.com/repos/doitsujin/dxvk/releases?per_page=5
- [DRV] https://github.com/doitsujin/dxvk/wiki/Driver-support
- [WIN] https://github.com/doitsujin/dxvk/wiki/Windows
- [PKG] https://raw.githubusercontent.com/doitsujin/dxvk/master/package-release.sh
- [PKGN] https://raw.githubusercontent.com/doitsujin/dxvk/master/package-native.sh
- [VP] https://raw.githubusercontent.com/doitsujin/dxvk/master/VP_DXVK_requirements.json
- [DEV] https://raw.githubusercontent.com/doitsujin/dxvk/master/src/d3d11/d3d11_device.cpp
- [FMT] https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxgi/dxgi_format.cpp
- [SC] https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxgi/dxgi_swapchain.cpp
- [FAC] https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxgi/dxgi_factory.cpp
- [OUT] https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxgi/dxgi_output.cpp
- [WB] https://raw.githubusercontent.com/doitsujin/dxvk/master/include/native/windows/windows_base.h
- [DXBC] https://github.com/doitsujin/dxbc-spirv (README)

## 0. What the survey found

1. **There is nothing COM-shaped to make GCC-portable.** `__uuidof`, `IID_PPV_ARGS`, `ComPtr`, `_com_ptr_t`,
   `DEFINE_GUID`, `QueryInterface`: **0** in `src/`. The two IIDs are literal `const GUID`s
   (`contextdata.cpp:314` ID3D11Texture2D, `devicedata.cpp:247` IDXGIFactory1). Spellings that exist:
   `HRESULT` 15, `WINAPI` 12, `GUID` 4, `REFIID` 1 (`devicedata.cpp:205`), `HWND` 36 (19 files, 1 in
   dx11), `HCURSOR` 5, `HMONITOR` 3. dxvk-native's headers define all of them: HWND / HMONITOR are
   `HANDLE` = `void*`, and `WINAPI` / `STDMETHODCALLTYPE` / `__stdcall` expand to nothing [WB].
   (dxvk-native supports `__uuidof(type)` anyway [README]; we never use it.)
2. **d3d11 / dxgi are not linked.** `ResolveGfxImport` (`devicedata.cpp:211-229`) does
   `GetModuleHandleW` -> `LoadLibraryW` -> `GetProcAddress` for two entry points, `D3D11CreateDevice` and
   `CreateDXGIFactory1` (`:231-240`). The link line has no `-ld3d11 -ldxgi` (`Makefile:71`,
   `CMakeLists.txt:99`). So a DXVK `d3d11.dll` + `dxgi.dll` beside the exe is picked up with **zero code**.
   Both DLLs must come from DXVK: [WIN] names "the game loads `dxgi.dll` from `System32`" while using
   DXVK's d3d11 as a common crash.
3. **No WIC, DirectXTex, D3DX, `D3DCompile` or PNG decoder** anywhere in `src/` or `tests/` (grep: 0).
   Textures upload the package bytes as-is (`imagebufferdata.cpp:202`). Shaders are retail DXBC handed to
   `CreateVertexShader` / `CreatePixelShader` (`shaderprograms.cpp:143,209`). Screenshots are a
   hand-written BMP (`src/engine/video/save_image_to_file.cpp`) fed by a staging readback
   (`imagebufferdata.cpp:664-699`).
4. **The non-portable part of dx11 is Win32 window/cursor code, not D3D:** 22 call sites in
   `devicedata.cpp` (§C.2).
5. **The trace oracle reads D3D11 pipeline state only** (`tools/dx11_state_diff/dump_state.py:423`
   `GetD3D11PipelineState()`; shader keys are the DXBC sha1, `:176-186`). Correction to the brief: row 1211
   of known_port_issues is `System::Debug::ForceDump`; `EnumerateVideoModes` is
   `src/engine/video/enumerate_video_modes.cpp`.

## A. The D3D11 / DXGI surface and the DXVK verdicts

Device creation: feature levels 11_0, 10_1, 10_0 (`devicedata.cpp:1190-1193`, `:1659`), driver type
HARDWARE, **flags 0** (no `D3D11_CREATE_DEVICE_DEBUG`, not `SINGLETHREADED`). Not used at all:
queries (`CreateQuery` 0), compute, GS/HS/DS, deferred contexts, `GenerateMips`, UAVs. DXVK's feature-level
ceiling is computed from the device features, with a `d3d11.maxFeatureLevel` override accepting 9_1..12_1
[DEV]; 11_0 needs `drawIndirectFirstInstance`, `multiDrawIndirect`, `tessellationShader` on top of the
D3D11 baseline [VP].

**good** = implemented by DXVK and exercised as we call it. **good, differs** = implemented, with a
documented behaviour that differs from Microsoft's runtime (said in the row). Sites are in
`src/render/dx11/` unless prefixed `app/` (`src/app/main.cpp`, diagnostics).

| # | Interface :: method | Sites | Verdict |
|---|---|---|---|
| 1 | `D3D11CreateDevice` (GetProcAddress) | devicedata 238, 1196, 1665 | good; native: dlsym from the dxvk-native lib (§C.1) |
| 2 | `CreateDXGIFactory1` (GetProcAddress) | devicedata 233, 1116, 1547 | good; native: same |
| 3 | IDXGIFactory1::EnumAdapters1 | devicedata 1123, 1552 | good |
| 4 | IDXGIFactory::MakeWindowAssociation(NO_WINDOW_CHANGES) | devicedata 1226 | **good, differs:** a no-op that logs "Ignoring flags" [FAC] |
| 5 | IDXGIFactory::CreateSwapChain (DISCARD, 2 buffers, B8G8R8A8, ALLOW_MODE_SWITCH) | devicedata 1230; desc 446-485 | good; a failed fullscreen create falls back to windowed [SC] |
| 6 | IDXGIAdapter1::EnumOutputs | devicedata 1130, 1589 | good |
| 7 | IDXGIAdapter1::GetDesc1 (`Description` WCHAR[128]) | devicedata 1556 | good; native: WCHAR is 4-byte `wchar_t` [WB] (§C.4) |
| 8 | IDXGIOutput::GetDesc (`Monitor`, `DeviceName`) | devicedata 653, 1178, 1593 | good: `DeviceName` from `wsi::getDisplayName`, `Monitor` is the WSI's monitor handle [OUT]; native: same WCHAR note |
| 9 | IDXGIOutput::GetDisplayModeList(fmt 28) | devicedata 1633, 1639 | **good, differs:** modes come from the WSI, filtered by bits-per-pixel only, sorted width/height/refresh [OUT] |
| 10 | IDXGISwapChain::GetBuffer (literal IID) | contextdata 465 | good |
| 11 | IDXGISwapChain::Present(vsync?1:0, 0) | contextdata 508 | good; returns `DXGI_STATUS_OCCLUDED` when an occluded fullscreen window drops out of fullscreen [SC] (§E) |
| 12 | IDXGISwapChain::ResizeTarget (retail fmt-28 quirk) | devicedata 834 | **good, differs:** windowed = `wsi::resizeWindow`; fullscreen = `ChangeDisplayMode`; refresh updated only if numerator != 0 [SC] |
| 13 | IDXGISwapChain::ResizeBuffers(1, w, h, 0x57, 2) | devicedata 846 | good |
| 14 | IDXGISwapChain::SetFullscreenState | devicedata 851, 1044 | **good, differs:** implemented [SC], but on Windows exclusive fullscreen is opt-in via `dxvk.allowFse = True` [WIN] |
| 15-17 | IDXGISwapChain::GetContainingOutput / GetFullscreenState / GetDesc | devicedata 651; app 882, 885, 947 | good |
| 18 | ID3D11Device::CreateBuffer (dynamic, immutable, CB, staging) | buffers 380, 442, 723, 739 | good |
| 19 | ID3D11Device::CreateTexture2D (MSAA attachments, textures, staging) | devicedata 525; imagebufferdata 556, 675 | good (D24S8 note below) |
| 20-22 | CreateShaderResourceView / CreateRenderTargetView / CreateDepthStencilView | imagebufferdata 582; 433, 595; 466, 593 | good |
| 23 | ID3D11Device::CreateInputLayout | vertexlayoutdata 209 | good |
| 24-25 | CreateVertexShader / CreatePixelShader (retail DXBC) | shaderprograms 143, 209 | good **if the blobs are SM 4.0, 4.1 or 5.0**: DXVK rejects any other model and any blob whose container hash fails, with `E_INVALIDARG` [DEV]. The compiler is dxbc-spirv ("Shader Models 5.1 and older") since DXVK 3.0 [DXBC] [REL v3.0]. Retail requires FL 10_0, so SM 4.x/5.0 is expected; step 1's log confirms ("Invalid shader model" would appear) |
| 26-29 | CreateBlendState / CreateRasterizerState / CreateDepthStencilState / CreateSamplerState | renderstatedata 416-418; imagebufferdata 377 | good (`samplerAnisotropy` required [VP]) |
| 30 | ID3D11Device::CheckFormatSupport (B8G8R8A8 RT, D24S8 DS) | devicedata 1210, 1218 | good |
| 31 | ID3D11Device::CheckMultisampleQualityLevels (1..32) | devicedata 1697 | good: 1 quality level when supported, 0 (S_OK) when not, for any count 1..32 [DEV]; our test is `quality != 0` |
| 32 | ID3D11Device::GetDeviceRemovedReason | contextdata 527, 548 | **good, differs:** `S_OK`, or `DXGI_ERROR_DEVICE_RESET` for ANY non-success Vulkan status; never `DEVICE_REMOVED` [DEV] (§E) |
| 33-36 | Ctx::IASetInputLayout / IASetVertexBuffers / IASetIndexBuffer(R16_UINT) / IASetPrimitiveTopology | vertexlayoutdata 128; buffers 552, 623; contextdata 811, 827, 871, 957 | good |
| 37-42 | Ctx::VS/PSSetShader, VS/PSSetConstantBuffers, PSSetShaderResources, PSSetSamplers | shaderprograms 176, 226; buffers 771, 773; contextdata 795-796; imagebufferdata 621-622 | good |
| 43-45 | Ctx::Draw / DrawIndexed / DrawIndexedInstanced | contextdata 996; 875, 1146; 879, 1148 | good |
| 46-47 | Ctx::Map / Unmap (WRITE_DISCARD, NO_OVERWRITE, READ_WRITE on STAGING) | buffers 316, 332, 449, 451; imagebufferdata 637, 641, 684, 699 | good |
| 48-53 | Ctx::OMSetRenderTargets / OMSetBlendState / OMSetDepthStencilState / RSSetState / RSSetViewports / RSSetScissorRects | devicedata 725; smallclasses 163; renderstatedata 451-454; contextdata 730, 604 | good |
| 54-56 | Ctx::CopyResource / UpdateSubresource / ResolveSubresource (MSAA B8G8R8A8 -> back buffer, every frame) | buffers 445, 490, 777; imagebufferdata 643, 677; contextdata 468; smallclasses 202 | good. v3.0.1 notes a possible slight perf cost "in a small number of games with suboptimal MSAA resolve patterns" [REL v3.0.1] |
| 57-59 | Ctx::ClearRenderTargetView / ClearDepthStencilView / ClearState | contextdata 658, 666; devicedata 726 | good |
| 60-61 | IUnknown::AddRef / Release | 3 / 29 sites | good |

**Total: 61 API entry points and methods. 56 good, 5 good-with-a-documented-difference (rows 4, 9, 12,
14, 32), 0 gaps, 0 unknown.** The one conditional is the shader model of the retail blobs (rows 24-25),
which step 1 settles. Every difference is in display management or device loss, none in the draw path.

| Formats | DXGI | DXVK, verified |
|---|---|---|
| Textures (`imagebufferdata.cpp:202`) | BC1_UNORM, BC3_UNORM, A8_UNORM, R8_UNORM, B8G8R8A8_UNORM, R32_FLOAT, R24G8_TYPELESS | BC1 -> `VK_FORMAT_BC1_RGBA_UNORM_BLOCK`, BC3 -> `BC3_UNORM_BLOCK` [FMT]; `textureCompressionBC` is a D3D11-baseline requirement [VP], so a GPU without BC cannot run DXVK's D3D11 at all (the Android blocker). A8 -> `VK_FORMAT_A8_UNORM_KHR`, else R8 with swizzle {0,0,0,R} [FMT] |
| Depth | D24_UNORM_S8_UINT DSV, R24_UNORM_X8_TYPELESS SRV (`imagebufferdata.cpp:568,589`) | where D24S8 is unsupported ("AMD do not support 24-bit depth buffers on Vulkan") all four D24 formats remap to `D32_SFLOAT_S8_UINT`, with a log line [FMT] |
| Back buffer / MSAA colour | B8G8R8A8_UNORM, resolved per frame | good |
| Vertex elements (`vertexlayoutdata.cpp:66-74`) | R32x4/x3/x2/x1 FLOAT, R16G16B16A16/R16G16 SINT, B8G8R8A8_UNORM, R8G8B8A8_UINT/UNORM | good on desktop; MoltenVK not checked (macOS is not a step in this plan) |

## B. Build proposal

**Current release: DXVK v3.1.1, 2026-09-15** [REL]. Assets: `dxvk-3.1.1.tar.gz` (sha256
`40565b4a724aadc4433fa4e010b4b23916d9b1f1baeee64e17186db94f54e608`) and
`dxvk-native-3.1.1-steamrt-sniper.tar.gz` (sha256
`506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7`) [REL]. v3.1 shipped no native
tarball; v3.0, v3.0.1, v3.0.2 and v3.1.1 did [REL]. The Windows tarball holds the DLLs under `x64/` and
`x32/`, DLLs only [PKG].

**Driver floor.** DXVK 3.x needs "a driver recent enough to support Vulkan 1.4" [REL v3.0] [DRV]. The
listed minimums are Linux drivers only (RADV 25.0, NVIDIA 575.51.02, NVK 25.1, ANV 25.1) [DRV]. v3.0 tells
RDNA1/2 users on Windows to "stick to DXVK 2.x" [REL v3.0]. DXVK 1.10.3 needs only Vulkan 1.1 [DRV]. The
newest 2.x tag was not fetched; pin it from the releases page if 2.x is needed.

**Step 1 is a DLL drop-in on the existing Windows exe, with no build change.**
- **Windows is not officially supported upstream.** "While using DXVK on Windows may generally work ... we
  do not support it officially". AMD/Intel Windows drivers are "generally" untested with DXVK, while NVIDIA
  is fine because its "Vulkan driver is largely shared" [WIN]. A step-1 failure on AMD/Intel is therefore
  weak evidence against decision 4. Linux is the target that counts.
- **Never `System32`** [WIN]. Our copy is per-directory anyway.
- **Shared game directory.** `C:/GOG Games/Banished` also holds the retail exe, whose `VideoDX11-x64.dll`
  would load the same DLLs. The step-1 script copies `x64/d3d11.dll` + `x64/dxgi.dll` in under
  `tools/game_dir_lock.sh acquire coord`, runs, and removes them on every exit path.
- **Logs and cache.** On Windows DXVK writes `reimpl-x64_d3d11.log` / `_dxgi.log` to the working
  directory, and `DXVK_LOG_PATH` moves them [README]. Since 3.0 there is an on-disk shader cache under
  `%LOCALAPPDATA%/dxvk`, switched off with `DXVK_SHADER_CACHE=0` [README] [REL v3.0]. These are DXVK's
  env vars, set by the script and never read by our code (CLAUDE.md's no-`getenv` rule is about our
  code). Run with `DXVK_SHADER_CACHE=0` so every run compiles the same way. A `dxvk.conf` beside the exe
  carries config options such as `dxvk.allowFse` [WIN] [README `DXVK_CONFIG_FILE`].

**Then dxvk-native for Linux.**
- **What it is.** "DXVK Native is a version of DXVK which allows it to be used natively without Wine",
  meant for ports, and "`HWND`s can become `SDL_Window*`s" through a WSI backend [README].
- **WSI (verified).** "DXVK Native requires a backend to be explicitly set via the `DXVK_WSI_DRIVER`
  environment variable. The current built-in options are `SDL3`, `SDL2`, and `GLFW`" [README]. Meson
  features: `native_sdl3`, `native_sdl2`, `native_glfw`, all `auto` [OPTS]. **SDL3 WSI has landed**, so it
  matches the SDL3 decision with no shim. `auto` silently drops a backend whose dev package is missing,
  so a source build passes `-Dnative_sdl3=enabled`.
- **Obtaining it, in order:**
  1. **The prebuilt `dxvk-native-<ver>-steamrt-sniper.tar.gz`** [REL], built in the Steam Runtime
     [README]. It packs a `usr/lib` + `usr/lib32` tree [PKGN]. Pin a version that ships a native tarball
     (3.1.1). It goes under `third_party/dxvk-native/3.1.1/`, either committed or fetched by a script that
     checks the sha256 above. The `.so` names and the header layout inside are not in the packaging
     script; read them from the tarball (step 4).
  2. **A source build inside Linux/WSL**: `git clone --recursive`; Meson >= 0.58 and glslang [README];
     host gcc with `-m64` / `-m32`, no cross file, `--force-fallback-for=libdisplay-info` [PKGN]. Build
     it with the distro's meson/glslang, not MSYS2: this machine has neither, and `pacman -S` is
     forbidden. pip meson exists, but this is a Linux build.
- **Headers on the Linux build.** dxvk-native ships "a slim set of Windows header definitions ... and
  the MinGW headers for D3D9/11" [README]. Those plus our literal GUIDs are all dx11 needs. The CMake
  build adds them on its non-WIN32 branch. The Makefile stays Windows-only, since it is the gate's build.
- **Seam constraints handed to the SDL plan.** `Video::Device::windowHandle`
  (`src/engine/video/Device.h:590`, already `void*`) carries the `SDL_Window*` that dxvk-native takes as
  `OutputWindow`. The SDL window is created Vulkan-capable (`SDL_WINDOW_VULKAN`; that the SDL3 WSI needs
  the flag is my reading of how it creates a surface, **UNKNOWN** upstream). The launcher, or the
  script on Linux, sets `DXVK_WSI_DRIVER=SDL3`.

## C. Code changes

### C.1 Entry-point resolution (`devicedata.cpp:211-240`)
`ResolveGfxImport` becomes a platform function. `LoadLibraryW` + `GetProcAddress` moves into
`src/platform/win/`. The Linux twin `dlopen`s the dxvk-native d3d11 / dxgi libraries and `dlsym`s the same
two names. The typedefs keep `WINAPI`, which expands to nothing there [WB]. Nothing else in dx11 links
against D3D.

### C.2 The 22 Win32 window/cursor sites in `devicedata.cpp`
These are the real seam. They go through the window API the SDL seam provides. Its first Windows
implementation is today's Win32 call, so behaviour is unchanged. Once SDL3 owns the window, native D3D11
gets the HWND from SDL's window properties.

| Sites | Calls | Becomes |
|---|---|---|
| 713, 721 | `IsWindowVisible`, `IsIconic` (resize helper) | window-state query |
| 1162, 1168 | `BootWindow::ArmDeferredShow`, `ShowWindow(SW_SHOWDEFAULT)` | window show |
| 627, 628, 638 | `GetWindowRect` / `GetClientRect` / `IsWindowVisible` in `TraceGeom` (diagnostic) | window rect query, or Windows-only |
| 1598 | `GetMonitorInfoW(outDesc.Monitor)` for the "(Display N)" suffix | §C.3 |
| 927-933, 1022-1026, 1267-1268, 1311, 1344-1348 | WM_SETCURSOR callback (msg 0x20), `CreateCursor` from the 32x32 ImageBuffer, `SetCursor`, `ShowCursor`, `DestroyCursor` | a platform cursor from the same pixels (`SDL_CreateColorCursor`); `HICON__* cursor` (`VideoDX11Data.h:158`) becomes an opaque handle |

The `Video::Device` side is already behind `_win.cpp` files: `device_adjustdisplay_win.cpp` (`SetWindowPos`,
`SetWindowLongW`, `AdjustWindowRect`, `MonitorFromRect`, `GetMonitorInfoW`) and `device_startup_win.cpp`
(`EnumDisplaySettingsW`, `MoveWindow`, `GetWindowPlacement`). Their non-Windows twins are part of the same
window API.

### C.3 DXGI output enumeration (`EnumerateDxgiInterfaceInto`, `devicedata.cpp:1535-1720`)
The walk is portable apart from `GetMonitorInfoW`, which reads `szDevice` (`\\.\DISPLAYn`) only to build
the "(Display n)" suffix. DXVK fills `DXGI_OUTPUT_DESC::DeviceName` from `wsi::getDisplayName` [OUT]. On
Windows, Microsoft's DXGI fills it with the same `\\.\DISPLAYn` string, so the change reads `DeviceName`
instead. It is a change to a retail call sequence, so it ships as a `behaviour:` commit with the Options
adapter names compared before and after. What the SDL3 WSI's `getDisplayName` returns, and so the
Linux suffix, is **UNKNOWN** until step 4. The mode filter (`m.Width < 800`, `:1643`) and the MSAA probe
stay. Under DXVK the mode list comes from the WSI, sorted width/height/refresh [OUT]. Retail's
`FindOrAddMode` already merges refresh rates per resolution, so ordering differences are harmless.
Count differences show up in the Options combo, which step 1 records.

### C.4 The `WCHAR` width (ABI audit hook)
dxvk-native has `typedef wchar_t WCHAR` [WB]: 4 bytes on Linux. `DXGI_ADAPTER_DESC1::Description` and
`DXGI_OUTPUT_DESC::DeviceName` feed `System::WideString`, the 2-byte retail type
(`devicedata.cpp:1567`, and §C.3). Those two reads need a 4->2 conversion on Linux. The item goes on the
`wchar_t` line of the ABI audit (NEXT_SESSION §2.2), not into a separate change.

### C.5 Unchanged
- `exports.cpp` keeps the two plugin entry points. They are plain functions since B5, and only their names
  recall the DLL.
- `dx9_placeholder.cpp`, the shader / buffer / state / texture classes and the BMP writer stay as they are.
- `video_interfaces_win.cpp` (`SetErrorMode` and the DLL-name table) gets a trivial non-Windows twin.
- `src/app/main.cpp`'s swap-chain probes (`:860-955`) use portable DXGI calls plus four Win32 window calls.
  They are diagnostics, so they move to `src/diag/` or stay Windows-only.

## D. Order and gating

**D.0, the measurement fact that shapes every gate.** The trace oracle compares what *our renderer told
D3D11*. The step-1 drop-in does not change our exe, so the D3D11 command stream is the same by
construction. Step 1 therefore tests DXVK's translation, and the oracle does not apply to it. Steps 2-3
change our code on Microsoft's D3D11, and the oracle is their gate.

Whether RenderDoc records a D3D11 or a Vulkan capture with DXVK loaded is **UNKNOWN**. Searches of
renderdoc.org and the RenderDoc GitHub found no statement either way. RenderDoc supports both APIs on
Windows, and Vulkan capture needs its implicit layer registered (https://github.com/baldurk/renderdoc/wiki/Vulkan).
The renderdoc MCP handles either API (`tools/renderdoc_mcp/README.md:29`). `dump_state.py` does not: it
reads D3D11 state only. Pixel diffs measure nothing (CLAUDE.md).

| Step | Change | Gate |
|---|---|---|
| **1. DXVK drop-in (Windows, no code)** | A script, under the game-dir lock, copies `x64/d3d11.dll` + `x64/dxgi.dll` from the pinned release, runs `sa-run` and `sa-run-menu` with `DXVK_LOG_LEVEL=info DXVK_SHADER_CACHE=0`, collects the DXVK logs, and removes the DLLs. | `sa-run` exit 0, crash log 0, `boot_state` in-game. The DXVK log shows the device and feature level, no `err:` lines, no "Invalid shader model" and no "Shader hash validation failed" (settles rows 24-25), and whether the D24S8 -> D32S8 remap fired. One RenderDoc capture at the camera pin, read with the MCP: its capture API (settles D.0), and a draw count inside native's measured 509-566 at frame 150. `resave_gate.sh Gate` PASS. A human eyeballs one screenshot; it is never diffed. Record the Options adapter and mode list against native. Fullscreen is checked once with and once without `dxvk.allowFse = True`. |
| **2. Entry-point and output-name portability (Windows build, Microsoft D3D11)** | C.1, a pure move of `ResolveGfxImport` into `src/platform/win`. C.3, `DeviceName` replaces `GetMonitorInfoW`, in a `behaviour:` commit. | The pure move: `phase_d/b11pair.py` with a rename map shows RENAMED-IDENTICAL, and the callers' objects are unchanged. C.3: the Options adapter names match before and after. Both: `sa-run` clean, resave PASS, trace oracle against the base exe with a control pair: S3 floored = 0, S4 = 0, S6 = 0. |
| **3. Window / cursor seam (after the SDL seam lands its window API)** | C.2. The Windows implementation is today's Win32 calls; then the SDL3-backed one. | Per commit: `sa-run` + `sa-run-menu` clean, resave PASS, oracle S3/S4/S6 = 0, `worldcmp.sh` identical. Manual: cursor visible over the menu; alt-tab and minimize/restore without a window collapse (`devicedata.cpp:700-860`); windowed and fullscreen. |
| **4. First Linux build (after the settings, SDL and threads seams and the ABI audit)** | CMake non-WIN32 branch against the pinned dxvk-native tarball; C.4; Linux twins of the `_win.cpp` video files; `DXVK_WSI_DRIVER=SDL3`. | Boots to the menu and into a new game with exit 0. `DXVK_HUD=devinfo,fps` or a Vulkan RenderDoc capture shows the frame. Resave and `worldcmp.sh` against the Windows reference. The oracle needs a Vulkan arm (risk 3) or is absent on Linux. |

Step 1 can run now, in parallel with the other seams' planning. Its result decides whether decision 4
holds before any code moves.

## E. Device-lost under DXVK (deferred, no work planned)

Today's route is one injected `DXGI_ERROR_DEVICE_RESET` (`contextdata.cpp:511-517`), followed by a real
`DEVICE_REMOVED` on every frame. The leading hypothesis is that game-owned buffers from the dead device
get bound after the recreate (row 890, `b5_session/DEVLOST_FINDING.md`). DXVK changes the shape in four
ways:

1. **There is no device-lost recovery in DXVK.** `GetDeviceRemovedReason` returns `S_OK`, or
   `DXGI_ERROR_DEVICE_RESET` for any non-success Vulkan status. `RegisterDeviceRemovedEvent` is a stub, and
   swap-chain Present has no device-lost branch [DEV] [SC]. A real loss surfaces as RESET, never REMOVED.
   Our `Finish` accepts both, so the recreate path still runs. Whether a fresh `D3D11CreateDevice` after a
   real `VK_ERROR_DEVICE_LOST` works is **UNKNOWN**. [REL v3.0.2] only adds `DXVK_DEBUG=hang` to locate
   such crashes.
2. **DXVK has no cross-device validation** (none seen in [DEV]). A stale buffer bound into the new
   device's context would not produce a clean removal. It would reach Vulkan as another `VkDevice`'s
   handle: undefined behaviour, more likely a crash than today's loop.
3. **Fullscreen occlusion returns `DXGI_STATUS_OCCLUDED` from Present** [SC]. `Finish` treats any non-zero
   hr as a possible loss and asks `GetDeviceRemovedReason`. That returns `S_OK`, so `Finish` returns
   harmlessly, but it costs a query per occluded frame.
4. **The injection still works,** because `BANISHED_FORCE_DEVICE_LOST` substitutes only the Present
   result. It stays the one way to drive the route over DXVK. Each leaked `ID3D11Device` would hold a
   whole `VkDevice`, so `devlost.sh`'s `DLMAX=5` cap matters more.

Re-read the finding under DXVK when the user lifts the deferral. Step 1 does not run the device-lost route.

## F. Risks and user questions

Risks:
1. **The driver floor.** DXVK 3.x needs Vulkan 1.4 [DRV]; RDNA1/2 on Windows is told to stay on 2.x
   [REL v3.0]; and Windows itself is unsupported upstream, with AMD/Intel Windows drivers untested [WIN].
   This machine's GPU and driver are not recorded in the repo docs.
2. **The Linux test bed (top risk).** WSL2's GPU Vulkan is Mesa's dozen, which prints that it is not a
   conformant Vulkan implementation (https://discourse.libsdl.org/t/sdl-gpu-rank-dzn-above-lavapipe-for-wsl-vulkan-support/65959).
   Its last reported conformance target was Vulkan 1.2 (https://Phoronix.com/news/Microsoft-Dzn-Vulkan-1.2),
   far short of DXVK 3's Vulkan 1.4. Lavapipe (CPU) is conformant to 1.3 and, as of February 2025,
   exposes the 1.4 extensions without having submitted for 1.4 conformance
   (https://www.slideshare.net/slideshow/current-state-of-lavapipe-mesa-s-software-renderer-for-vulkan/276099337).
   DXVK 3.x on lavapipe is **UNKNOWN**. At best it is a slow correctness bed, never perf.
3. **Verification tooling on the Vulkan side.** `dump_state.py` is D3D11-only. A Vulkan arm is new
   tooling, and DIRECTION §2 says "no new instruments", so it needs the user's go-ahead. Without it the
   Linux gate is boot + resave + world comparison.
4. **Display management.** All five "differs" rows are fullscreen, mode-list and device-loss behaviour.
   The Options dialog reads the mode list with no bounds check (`dx9_placeholder.cpp` banner), so a
   different DXVK mode count is the most likely visible difference.
5. **Retail shader models.** If any retail blob is outside SM 4.0/4.1/5.0 or fails its hash check, DXVK
   refuses it [DEV]. Expected not to happen, given the FL 10_0 floor; step 1 proves it.

User questions, only those that change the work:
1. **Which Linux test bed:** WSL2 (dozen likely fails DXVK 3's Vulkan 1.4 floor; lavapipe gives
   correctness only) or a real Linux box with a Vulkan 1.4 driver? The step-4 gate depends on it.
2. **May step 1 download DXVK v3.1.1** (sha256s in §B)? Should the binaries be committed under
   `third_party/` or fetched by a checked script? If this machine's GPU is AMD RDNA1/2, a 2.x pin instead.
3. **Is a Vulkan arm for the trace oracle allowed** (risk 3)?
4. **This machine's GPU and driver version**, if known. Otherwise step 1's DXVK log reports them.

Android stays deferred. DXVK's D3D11 baseline requires `textureCompressionBC` [VP], which most Android
GPUs lack. That is a concrete blocker on top of NEXT_SESSION §2.2's open question.
