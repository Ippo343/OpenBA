# Phase D.4 block RENDER-3: the devicedata window / cursor sites over Platform::Window

Branch `d4-render3`, base `5f9dcb6d2`; main merged before the final build (main had only
docs commits since the base). RENDER plan step 3, section C.2 only: the Win32 window and
cursor sites in `src/render/dx11/devicedata.cpp`, put behind `Platform::Window` /
`Platform::Cursor` free functions whose Win32 bodies are today's exact call. Windows
behaviour is unchanged by construction. Step 4, the Linux build, is not part of this block.

Static gate only, per commit: `standalone-debug`, `standalone-perf` and `unit` at `-j2`, all
exit 0, unit 953 / 0. Each commit has 0 new warnings against the main checkout's
`runs/warn_baseline_{debug,perf}.txt`. castmetric over `src` is 2, and `tools/abi_audit.py`
is OK. The game was never run.

## Commits

| commit | title | B11 (perf objects vs `runs/b11base_d4b/perf/obj`) |
|---|---|---|
| `b7f48c0c1` | refactor: devicedata window-state queries over Platform::Window (1/3) | devicedata: 2 DIFF in the accepted shape, 3 SAME (.rdata displacements); window_win IDENTICAL + 1 new |
| `1a939889d` | refactor: the window show over Platform::Window::ShowDefault (2/3) | devicedata CreateDevice and boot_window ShowNow DIFF in the accepted shape; ArmDeferredShow RENAMED-IDENTICAL; 7 header includers IDENTICAL |
| `00e2cfe43` | refactor: the dx11 cursor over Platform::Cursor, an opaque CursorHandle (3/3) | devicedata OnSetCursor / Shutdown / SetImageBuffer DIFF in the accepted shape; 18 other includers IDENTICAL |

### The accepted shape, quoted per site

`fdiff.py`, a scratch script over `b11cmp.parse`, printed a unified diff of each function's
instruction text. Every DIFF is an import call that became a direct call to the primitive.
The only other change is a literal argument that moved into the primitive.

| function | base | tip |
|---|---|---|
| DeviceData_Resize | `call *__imp_IsWindowVisible`, `call *__imp_IsIconic` | `call Platform::Window::IsVisible`, `call Platform::Window::IsIconic` |
| TraceGeom.part.0 | `__imp_GetWindowRect`, `__imp_GetClientRect`, `__imp_GetCurrentThreadId`, `__imp_IsWindowVisible` | `Platform::Window::GetWindowRect`, `GetClientRect`, `Platform::CurrentThreadId`, `Platform::Window::IsVisible`, in the same order |
| CreateDevice | `mov $0xa,%edx` + `call *__imp_ShowWindow` | `call Platform::Window::ShowDefault`; SW_SHOWDEFAULT moved into the primitive |
| BootWindow::ShowNow | `mov $0xa,%edx` + `call *__imp_ShowWindow` | `call Platform::Window::ShowDefault` |
| DeviceData_OnSetCursor | `call *__imp_SetCursor`, `mov $1,%ecx`, `call *__imp_ShowCursor` | `call Platform::Cursor::Set`, `mov $1,%ecx`, `call Platform::Cursor::Show` |
| DeviceData::Shutdown | `call *__imp_DestroyCursor` | `call Platform::Cursor::Destroy` |
| DeviceData::SetImageBuffer | `__imp_DestroyCursor`, `__imp_CreateCursor`, `__imp_SetCursor` | `Platform::Cursor::Destroy`, `Create`, `Set`; Create's 7-argument set-up is the same instructions, 3 of them scheduled in a different order |

Also:
- **CreateDevice, CreateAttachment, EnumerateDxgiInterfaceInto SAME:** the only change is
  `.rdata` displacements moved by 0x10, because commit 1 shortened TraceGeom's format
  string. CreateDevice also picks up the ArmDeferredShow relocation's new mangling.
- **SetImageBuffer:** one trailing alignment `nopw` is gone.
- **Map artefact:** with the `__imp_X -> Platform::...` map, window_win.o's existing
  primitives read SAME. With an empty map they are IDENTICAL, as SDL-A noted. The verdicts
  above use the empty map unless they say otherwise.

## Decisions and deviations

1. **TraceGeom is routed, not ifdef'd.** It is a diagnostic gated by
   `ResizeTraceOn()`. Every call in it already had a primitive except IsWindowVisible,
   which DeviceData_Resize needs anyway. So it becomes portable at no cost. Its printf
   formats went `%ld` -> `%d` and `%lu` -> `%u` for the int32_t / uint32_t operands. The
   printed values are the same.
2. **Create's signature** is
   `Create(void* instance, hotX, hotY, width, height, const void* and, const void* xor)`,
   in CreateCursor's argument order plus the HINSTANCE. The brief had
   `Create(and, xor, w, h, hotx, hoty)`. Retail passes `Application::GetInstance()`.
   Keeping the instance as an argument keeps that call in the caller, and keeping the
   order keeps the caller's set-up instructions the same. The planes are `const void*`,
   so the uint32_t row arrays pass without a cast.
3. **BootWindow went to `void*`.** `ArmDeferredShow(Configuration*, void*)` and the
   reveal call ShowDefault, so `boot_window.h` no longer includes `<windows.h>`. Its other
   includer, `splash_loop_stubs.cpp`, compiles without it and is IDENTICAL.
4. **The cursor handle** is `Platform::CursorHandle = Platform::CursorObject*`. It is a
   pointer to a never-defined struct rather than `void*`, so a window handle cannot be
   passed as a cursor by mistake. window_win.cpp asserts its size against HCURSOR. The old
   `HCURSOR == HICON__*` assert in devicedata.cpp and the `struct HICON__;` forward
   declaration in VideoDX11Data.h are gone.
5. **Slot 3's `UnregisterMessageCallback(0x20)` / `(6)`** now spell `kMsgSetCursor` /
   `kMsgActivate`. This is B11 IDENTICAL.
6. **Kept as they were:** `WindowOf()` and `<windows.h>` stay in devicedata.cpp. The
   swap-chain desc's OutputWindow, MakeWindowAssociation and the D3D11 / DXGI types still
   need them, and plan C5 keeps that code. GetMonitorInfoW, C.2 row 1598, is section C.3, a
   `behaviour:` change outside this block.

## Open items for SDL step W5 and the coordinator

- **W5 must provide `Platform::Cursor::{Create, Set, Show, Destroy}` over SDL:**
  - **Create:** turn the AND/XOR planes into ARGB pixels. AND set is transparent, AND clear
    with XOR set is white, both clear is black, and the MSB of a row's first byte is the
    leftmost pixel. Then call `SDL_CreateColorCursor` and ignore `instance`. The
    alternative is `SDL_CreateCursor(data, mask, ...)`. Its bit meaning differs from Win32's
    AND/XOR, so it needs a plane translation, not a pass-through.
  - **Set:** `SDL_SetCursor`.
  - **Show:** `SDL_ShowCursor` / `SDL_HideCursor`. Retail's ShowCursor is a display
    counter. Only `Show(true)` is ever called, from WM_SETCURSOR, so a boolean show is
    enough today.
  - **Destroy:** `SDL_DestroyCursor`.
- **WM_SETCURSOR under SDL:** the callback is registered as `kMsgSetCursor`. SDL owns the
  cursor once it owns the WndProc, so W3 / W5 must decide whether the pump synthesises
  `kMsgSetCursor`. If it does not, `Set` at build time in SetImageBuffer is the only apply.
  The cursor stays set either way, because SDL re-applies its current cursor itself.
- **Slot 3 leaves `cursor` dangling** after Destroy. That is retail and is kept. If slot 8
  ever runs on the same DeviceData after slot 3, it destroys the stale handle again. Win32's
  DestroyCursor fails harmlessly on a dead HCURSOR. `SDL_DestroyCursor` on a freed
  `SDL_Cursor*` is a use-after-free. Before W5 swaps the body, W5 must establish whether
  that order can happen, for example on the device-lost recreate path. If it can, the SDL
  body needs a liveness check of its own. The faithful caller stays unguarded.
- **Coordinator gate at merge:** sa-run, sa-run-menu, resave, the trace oracle with a
  control pair (S3 / S4 / S6 = 0) and worldcmp. Check by eye that the cursor shows over
  the menu, and that alt-tab and minimise / restore work. Minimise / restore exercises
  the routed IsIconic / IsVisible early-outs.

## Shared files (SDL-B also owns these)

- **`src/platform/common/platform_window.h`** adds:
  - `Platform::Window::IsVisible(void*)`
  - `Platform::Window::ShowDefault(void*)`
  - `struct Platform::CursorObject` and `Platform::CursorHandle`
  - `Platform::Cursor::Create(void*, int32_t, int32_t, int32_t, int32_t, const void*, const void*)`
  - `Platform::Cursor::Set(CursorHandle)`
  - `Platform::Cursor::Show(bool)`
  - `Platform::Cursor::Destroy(CursorHandle)`

  The additions are appended. Nothing existing was changed.
- **`src/platform/win/window_win.cpp`** has the bodies for those seven functions and the
  CursorHandle size assert.
- **Also touched:**
  - `src/render/dx11/devicedata.cpp`
  - `src/render/dx11/VideoDX11Data.h`, which now includes `platform_window.h`
  - `src/app/boot_window.h`
  - `src/app/boot_window.cpp`
