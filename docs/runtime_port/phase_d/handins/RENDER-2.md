# RENDER-2 hand-in (Phase D.4, branch `d4-render`, base `c44b484e3`)

Plan: `plans/RENDER.md` step 2 only, sections C.1 and C.3. Step 3 (C.2, the 22 window / cursor sites) waits
for the SDL seam's window API. Step 4 is the Linux build.
Static gate only: the game was never run. `git merge main` (`0f5b886bf`, THREADS-1) merged cleanly before
the final build; main did not touch `devicedata.cpp`.

## Commits

| commit | title | gate |
|---|---|---|
| `80f7b2e24` | refactor: ResolveGfxImport to src/platform/win | debug 0 / perf 0 / unit 0; unit 933 / 0; 0 new warnings; castmetric 2 |
| `ddebbe8fc` | behaviour: DXGI output name from IDXGIOutput::GetDesc DeviceName, not GetMonitorInfoW | debug 0 / perf 0 / unit 0; unit 933 / 0; 0 new warnings; castmetric 2 |
| `54d19a40d` | Merge branch 'main' into d4-render (THREADS-1) | final build: debug 0 / perf 0 / unit 0; unit 945 / 0 (933 + THREADS-1's 12); 0 new warnings; castmetric 2 |

## B11 verdicts (perf objects against the main checkout's `runs/b11base_d4/perf/obj`)

**C.1 (`80f7b2e24`):** `b11cmp.py` over every object: TOTAL DIFF 2, IDENTICAL 16917, ONLY-BASE 1, ONLY-TIP 1,
SAME 6.

- **The moved body is RENAMED-IDENTICAL.** Base `devicedata.o`'s `VideoDX11::(anon)::ResolveGfxImport
  [clone .isra.0]` was paired by hand with tip `gfx_win.o`'s `(anon)::ResolveGfxImport [clone .isra.0]`.
  Both have 40 lines and the same call list. The one differing line is the format string's offset in its
  object's `.rdata`: base `+0x78`, tip `+0x0`, with the same bytes. `b11pair.py`'s map cannot pair it,
  because the `.isra` clone has no usable bare name.
- **The callers DIFF only by the accepted shape.** In `GetCreateDXGIFactory1` and `GetD3D11CreateDevice`,
  `lea name,%rdx; lea L"dll",%rcx; call ResolveGfxImport.isra` became one `call Platform::Resolve...()`.
  The guard, the store and the return are identical.
- **SAME 6 are offsets only.** Every differing line in `devicedata.o`'s other functions is a rip-relative
  `.rdata` offset, because five strings left the object.
- **SETTINGS-1's objects are not this block's.** `filesystem.o` (ONLY-TIP 1), `settings_file.o` and
  `settings_win.o` differ because the B11 base predates the SETTINGS-1 merge. Every `FileSystem.h`
  includer is IDENTICAL.

**C.3 (`ddebbe8fc`):** TOTAL DIFF 3, IDENTICAL 16917, ONLY-BASE 1, ONLY-TIP 1, SAME 5.

- **`EnumerateDxgiInterfaceInto` loses exactly the retail GDI call.** The inlined memset, the `movl $0x68`
  cbSize store and `call *__imp_GetMonitorInfoW` are gone (690 to 674 objdump lines). `StrLength` now
  takes the `GetDesc` buffer, and the frame is 0x70 smaller.
- **Nothing else moved.** The other two DIFFs are C.1's callers, and SAME 5 are `.rdata` offsets.

## Why `Platform::GfxProc` and not `void*`

`GfxProc` is `std::intptr_t (*)()`, which is exactly `FARPROC` on Win64. `gfx_win.cpp` static_asserts
`std::is_same<FARPROC, Platform::GfxProc>`. So the move adds no cast. The one FARPROC to typed-pointer cast
per entry point stays at the dx11 caller with its existing `// CAST-OK: win32` marker. A `void*` return would
have added a function-to-object cast in `gfx_win.cpp`. Under dxvk-native `WINAPI` expands to nothing, and
the POSIX body turns `dlsym`'s `void*` into a `GfxProc`.

## Expected Options-dialog strings (for the coordinator's check)

**No existing log line prints the adapter names.** `[VideoProbe]` prints interface and adapter indices
only. `[ResizeTrace] swapchain containing output '%ls'` prints a `DeviceName`, but only on the resize path,
and no run log under `runs/` contains it. I did not add a log line, because DIRECTION §2 forbids new
instruments. The check is the Options dialog's adapter combo, read by eye, before and after.

| route | before (base exe) | after (this branch) |
|---|---|---|
| Microsoft D3D11 | `NVIDIA GeForce RTX 5080 (Display n)`, one per output with a usable mode | the same strings, same order |
| DXVK drop-in | DXVK's adapter description + ` (Display n)` | the same: DXVK's Win32 WSI returns the same GDI name |

- **`n` is the last character of `\\.\DISPLAYn`.** `ResizeTrace`'s comment records a DISPLAY3 on this
  machine, so expect three entries if all three outputs list modes.
- **Under DXVK the adapter part may read AMD.** RENDER-1's notes say DXVK spoofs vendor 0x1002 / 0x73df. I
  did not verify whether that changes `Description` as well as the IDs.

## Deviations from the plan

1. **Two named functions, not one generic resolver.** The header declares `ResolveCreateDXGIFactory1()`
   and `ResolveD3D11CreateDevice()`. The DLL names (`d3d11.dll` vs the dxvk-native `.so`) are
   platform-specific, so they live in the platform body. The common header carries no `wchar_t` path
   either (D4_RULES C2).
2. **`ResolveGfxImport` keeps its unused retail-IAT argument.** It is documentation only, and keeping it
   keeps the moved body RENAMED-IDENTICAL.
3. **One retail path is dropped by C.3.** A failed `GetMonitorInfoW` left `szDevice` zeroed. Retail's
   suffix then read `szDevice[-1]`, the tail of `mi.rcWork`. `GetDesc` always fills `DeviceName`. This is
   recorded in the code comment.
4. **One static_assert replaced.** The `MONITORINFOEXW` size assert guarded the cbSize literal, which is
   gone. It is replaced by an assert on `DXGI_OUTPUT_DESC::DeviceName` (offset 0, 64 bytes).

## Open items for the coordinator

- **Run the merge gate.** That means `sa-run`, resave, the trace oracle with a control pair (S3 floored,
  S4, S6 all 0), and `dxvk_route.sh`. Then compare the Options adapter combo against the table above.
- **C.4 is a comment only.** dxvk-native's WCHAR is a 4-byte `wchar_t`, so on Linux `DeviceName` and
  `Description` need a 4-to-2 conversion. It belongs on the ABI audit's `wchar_t` line.
- **The Linux twin of `gfx_win.cpp` is step 4.** It will `dlopen` / `dlsym` the dxvk-native libraries.

## Shared files touched

- None. The build globs for `src/platform/common/*.cpp` and `src/platform/win/*.cpp` and the
  `-Isrc/platform/common` root already existed from SETTINGS-1. `Makefile` and `CMakeLists.txt` are
  untouched.
- New: `src/platform/common/platform_gfx.h`, `src/platform/win/gfx_win.cpp`. Changed:
  `src/render/dx11/devicedata.cpp`.
