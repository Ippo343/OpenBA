# dxvk-native 3.1.1 (the Linux renderer's D3D11 / DXGI)

Fetched, never committed (D4_RULES section 3 Q6): `tools/fetch_dxvk_native.sh` downloads
`dxvk-native-3.1.1-steamrt-sniper.tar.gz` from the doitsujin/dxvk v3.1.1 release, checks its sha256
(`506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7`, the pin in
`docs/runtime_port/phase_d/plans/RENDER.md` section B) and extracts it here as `3.1.1/`
(gitignored). Run it on Linux (WSL): the tarball holds symlinks, which an MSYS extraction on NTFS
cannot create. The script stamps `3.1.1/.sha256` and is a no-op when the stamp matches.

## Layout (read from the tarball, 2026-10-11)

```
3.1.1/usr/include/dxvk/          the headers: MinGW-w64's d3d11*.h, dxgi*.h, d3dcommon.h, ...
                                 plus dxvk's slim windows.h / windows_base.h / unknwn.h / objbase.h
3.1.1/usr/include/dxvk/wsi/      native_wsi.h -> native_sdl3.h when DXVK_WSI_SDL3 is defined
                                 (HWND <-> SDL_Window*, HMONITOR <-> SDL_DisplayID)
3.1.1/usr/lib/                   x86-64: libdxvk_{d3d11,dxgi,d3d10core,d3d9,d3d8}.so.0.30101,
                                 the SONAME links .so.0 and the development links .so
3.1.1/usr/lib32/                 the same, i386 (unused)
3.1.1/usr/lib/pkgconfig/         dxvk-d3d11.pc etc. (Cflags: -I${includedir}/dxvk)
```

What the build uses:

- **Headers only.** CMake's non-WIN32 `reimpl-x64` adds `usr/include/dxvk` (after our own
  include directories) and defines `DXVK_WSI_SDL3`. Nothing links against the libraries:
  `src/platform/posix/gfx_posix.cpp` dlopens `libdxvk_dxgi.so.0` / `libdxvk_d3d11.so.0` (then the
  unversioned names) at run time, as `gfx_win.cpp` resolves `dxgi.dll` / `d3d11.dll`.
- **`WCHAR` is `wchar_t`, 4 bytes** (`windows_base.h`). `DXGI_ADAPTER_DESC1::Description` and
  `DXGI_OUTPUT_DESC::DeviceName` are read through a 4 -> 2 conversion in
  `src/render/dx11/devicedata.cpp` (RENDER.md C.4).
- **SDL3 is dlopened by DXVK.** The SDL3 WSI loads `libSDL3.so.0` and resolves every SDL call
  through that handle (strings in `libdxvk_d3d11.so.0`: "SDL3 WSI: Failed to load SDL3 DLL."). So
  the Linux exe links SDL3 SHARED, built from the same pinned submodule and installed beside the
  exe: DXVK's dlopen then returns the copy the exe already loaded, the one that owns the
  `SDL_Window*` it is handed as an HWND. The Windows exe keeps SDL3 static.
- `DXVK_WSI_DRIVER=SDL3` is an environment variable for DXVK, set by the run script, never read
  by our code (D4_RULES C11).

The libraries depend only on glibc (libdl, libm, libpthread, libc) and on the Vulkan loader
(dlopened: `libvulkan.so.1`).
