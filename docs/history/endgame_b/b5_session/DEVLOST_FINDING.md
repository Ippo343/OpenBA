# The forced device-lost route: why one injection becomes a round trip per frame and 20 GB

Read-only investigation, 2026-10-05, for `docs/known_port_issues.md` row 890. No build, no run,
no Ghidra: the six S4 logs under `b4_session/runs/*devlost*/` and the source tree only. Every
claim is tagged **MEASURED** (from a log), **READ** (from code or a code comment that quotes the
retail listing) or **HYPOTHESIS**.

**Summary.** The harness does not loop: `INJECTING` appears exactly once in all six runs and the
`fired` latch in `ForceDeviceLostThisFrame` (src/render/dx11/contextdata.cpp:610-627) cannot fire twice.
Every later `[DeviceLost] hr=0x887a0005` is the real swap chain's `Present` reporting a real
`DXGI_ERROR_DEVICE_REMOVED` on a device that `DeviceData_CreateDevice` had just built
(`D3D11CreateDevice` is logged once per round trip: 221 times in `s4_GFX_devlost`). So the
recreated device is a genuinely new `ID3D11Device`, not a resized swap chain on a dead one, and
it gets removed again — after 906 frames the first time, then after 9, 26, 63 … frames, and
finally every frame (0-1 frames between trips). The code does not distinguish RESET from
REMOVED (retail's `Finish` treats both identically and never asks `GetDeviceRemovedReason` for
either), never logs the removal reason, and — the one defect the tree already names — the game's
half of the recovery, `GameUpdateLoop::OnVideoError` / `OnVideoRestart` (retail 0x140095970 /
0x140095930), is a no-op `PORT GAP` (src/game/Game/gameupdateloop.cpp:2345-2352), so every
game-owned static vertex/index buffer and generated texture keeps a companion created on a device
that no longer exists and is bound to the new context on the next frame. That cross-device use
is the leading hypothesis for the recurring removal, and it predicts the measured degradation
(each round trip strands another device's worth of objects). The ~100 MB per round trip is not
measurable from these logs; the ranked suspects are (1) the old `ID3D11Device` is never
`Release`d on the lost path — retail's own `dd->d3dDevice = nullptr` before `Shutdown`
(contextdata.cpp:668-671) — so each round trip leaves one more device alive holding whatever was
destroyed on it (the whole reloaded External set), and (2) `OnVideoRestart` re-`Load`s every
texture, mesh and shader from disk per trip through an allocator that may not return pages. The
first trip is a faithful drive of retail's notify-and-recreate path; everything after it is a
consequence of a recovery that is incomplete in the game tree and of a harness that cannot say
why the device was removed. A bounded gate needs the removal reason logged and either the
`GameUpdateLoop` pair ported or a process-side cap on round trips; the script-side `DLMAX` cap
already in `devlost.sh` is the only safe bound today.

---

## 1. One round trip, call by call

**MEASURED** — the injection and the first two later trips, `s4_final_devlost/standalone_error.log`
lines 626-641 (filtered; `[Plague]`/`[Vitals]`/`[EntityCensus]` lines omitted):

```
[DeviceLost] ARMED: a synthetic DXGI_ERROR_DEVICE_RESET will be injected on Present #12000 (PLAN 5.5 forced device-lost drive)
...
[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET (real Present returned 0x00000000)
[DeviceLost] hr=0x887a0007 onLost=00007ff7b6d49c41 -> calling
[DeviceLost] recreate OK; onRestored=00007ff7b6d49c55 -> calling
[DeviceLost] device-lost round trip COMPLETE
[EntityCensus] frame=5400 entities=27043 ... [EntityCensus] frame=10800 entities=27043
[MusicPlayer] PLAY track=9/15 ext=00000218442645a0 scene=00000218442e3280 -> handle=42496
[DeviceLost] hr=0x887a0005 onLost=00007ff7b6d49c41 -> calling
[DeviceLost] recreate OK; onRestored=00007ff7b6d49c55 -> calling
[DeviceLost] device-lost round trip COMPLETE
[DeviceLost] hr=0x887a0005 onLost=00007ff7b6d49c41 -> calling
[DeviceLost] recreate OK; onRestored=00007ff7b6d49c55 -> calling
[DeviceLost] device-lost round trip COMPLETE
```

The same shape in `standalone_debug.log` (`s4_GFX_devlost`, lines 32319-32333, the injected trip;
`StorageDisplayComp::Draw` lines omitted):

```
[GFXBODY23] FrameCopyImageBuffer::dtor calls=1
[BootTime]   DX11 CreateDevice: entry                   +292540.13ms   (t=294618.75ms)
[BootTime]   DX11 DXGI factory + adapter + output enum  +    0.30ms   (t=294619.05ms)
[WndMsg] WM_WINDOWPOSCHANGING hwnd=00000000001b0742 4004,31 1040x679 flags=0x30
...
[BootTime]   DX11 AdjustDisplay (window sized)          +    2.17ms   (t=294621.23ms)
[BootTime]   DX11 D3D11CreateDevice                     +   17.84ms   (t=294639.06ms)
[BootTime]   DX11 swapchain + color/depth buffers       +    0.59ms   (t=294639.65ms)
```

Per run (all six): `INJECTING` = 1, `hr=0x887a0007` = 1, `hr=0x887a0005` = COMPLETE-1, and one
`DX11 CreateDevice: entry` block per COMPLETE (221 in `s4_GFX_devlost`, 5 in the two DLMAX-capped
`s4_final_*` runs, 19/203/219/207 in `close`/`s4_close`/`s4_GFX`/`s4_GFX2`). No `ResizeTrace`,
`WindowGuard` or `[MenuDLT::OnVideoError]` line appears after the injection in any run: the
`DeviceData_Resize` path is NOT part of a round trip and the current loop's slot 8/9 are the
GameUpdateLoop's (silent) ones.

**READ** — the sequence inside `ContextData_Finish` (slot 3, retail 0x1800017c0),
src/render/dx11/contextdata.cpp:631-702, with the comment block at :554-578 quoting the listing:

| step | code | what it does |
|---|---|---|
| a | :638-639 | `swapChain->Present(vsync, 0)`, then `Device::Unlock()`. |
| b | :643-649 | the drive: on the Nth call only, `hr = DXGI_ERROR_DEVICE_RESET` (the real result, 0, is printed). |
| c | :651-655 | `hr == 0` -> return. If `hr` is neither REMOVED nor RESET, `hr = d3dDevice->GetDeviceRemovedReason()`; zero -> return. **For REMOVED and RESET the reason is never queried** — retail's `(hr + 0x7785fffb) & 0xfffffffd` predicate (:561-565). |
| d | :659-664 | `onLost()` = `Device+0x50`, which `Engine::MainLoop`'s ctor fills with `MainLoop::OnVideoError` (src/engine/video/Device.h:562-564). |
| e | :666 | `Video::Device::ReleaseDynamic()` — RenderBuffer, ImageBuffer (kinds 0,1,3,4,5), VertexBuffer and IndexBuffer companions with `dynamic` set are `DeletingDtor(1)`'d and nulled (src/engine/video/dynamic_resources.cpp:285-301, 340-356, 380-396, 456-468, 490-495). |
| f | :668 | `dd->d3dDevice = nullptr` **before** Shutdown — "deliberately stops the shutdown path from Releasing a device that is already gone. That ordering is load-bearing" (:572-574). |
| g | :670-671 | DeviceData slot 3 `Shutdown` (src/render/dx11/devicedata.cpp:1040-1071): unregister WM_SETCURSOR/WM_ACTIVATE, DestroyCursor, Release depth (+0x30), colour (+0x38), **immediate context (+0x28)**, swap chain (+0x18, after leaving fullscreen), then `Dx11Release(self->d3dDevice)` — **a null, so nothing** (`Dx11Release` is null-guarded, src/render/dx11/dx11_backend_access.h:384-386). The IDXGIFactory1 at +0x10 is not touched (dangling since step 15 of CreateDevice, :1032-1035). The tracked-image registry at +0x60 is not touched. |
| h | :672-673 | DeviceData slot 2 `CreateDevice` (devicedata.cpp:1123-1290): new `IDXGIFactory1`, `EnumAdapters1`/`EnumOutputs`, Device+0x04/+0x08 from the mode table, `AdjustDisplay` (the `WM_WINDOWPOSCHANGING` burst in the log), `FillSwapChainDesc`, **`D3D11CreateDevice` -> new device + new immediate context** (:1212-1215), two `CheckFormatSupport`, `MakeWindowAssociation`, `factory->CreateSwapChain(newDevice, …)` (:1247), release adapter/output/factory, `CreateColorBuffer` + `CreateDepthBuffer` (the MSAA colour and D24S8 depth textures), re-register the two message callbacks. |
| i | :675-690 | on failure: retail's five error strings, `System::Error::Display()`. Never taken in these runs (`recreate OK` every time). |
| j | :692 | `Video::Device::RecreateDynamic()` — index, vertex, image, render companions rebuilt through DeviceData slots 12/13/10/19 on the NEW device (dynamic_resources.cpp:483-488). |
| k | :694-699 | `onRestored()` = `Device+0x58` = `MainLoop::OnVideoRestart`. |

**READ** — what `onLost` / `onRestored` do (src/engine/engine/mainloop_video.cpp):

* `OnVideoError` (:419-587): records every `Object::External` whose type is one of ten
  (ImageBuffer, Vertex/Pixel/VideoProgram, RenderState, VertexLayout, EdgeMesh, GraphicsMesh,
  ReflectionImageBuffer, FrameCopyImageBuffer; :441-456) and `Destruct()`s each (:510-514) —
  for an ImageBuffer that is `~ImageBuffer` -> `PlatformDestruct` -> `ImageBufferData`
  `DeletingDtor(1)` -> `ReleaseInterfaces` (src/render/dx11/imagebufferdata.cpp:280-292, 358-386);
  drops every `SaveLoad` thumbnail's `ImageBufferData` (:528-547); calls
  `_currentLoop->OnVideoError()` (:550); destroys and frees the `Video::Context` (:560-565),
  which deletes the `ContextData`, the three dynamic geometry buffers and the seven
  `ConstantBuffer`s' companions (src/engine/video/context_lifecycle.cpp:336-367).
* `OnVideoRestart` (:637-720): `CreateVideoContext` (new Context, new ContextData, new constant
  buffers on the new device), `_currentLoop->OnVideoRestart()`, `SaveLoad::OnVideoRestart`,
  then for every recorded External: in-place re-construct (:666), restore its path, and
  **`External::Load` from disk** (:697-705) — every texture, mesh and shader of the game is
  re-read and re-uploaded per round trip. That is the ~1.1-1.3 s a steady-state trip costs
  (MEASURED below).

**What a round trip does NOT do** (READ): it does not Release the old `ID3D11Device` (f+g); it
does not walk the DeviceData+0x60 tracked-image list (nothing in the DLL reads that list except
`TrackImageBuffer`'s add-if-absent scan, `UntrackImageBuffer` and the dtor's free —
imagebufferdata.cpp:228-273, devicedata.cpp:984-990; `src/render/dx11/README.md:785` documents no other
reader); it does not call `Dx11InvalidateConstantBufferBindings` (harmless here, because the
`ConstantBufferData` objects are destroyed with the Context and rebuilt with `activeIndex = -1`);
and it does not touch any GPU object owned by game code that is neither an External of the ten
types nor flagged `dynamic` — see §2.

**MEASURED** — timing from the `DX11 CreateDevice: entry` deltas in `standalone_debug.log`:

| run | inject (Present #) | injected trip at | first real REMOVED after | then |
|---|---|---|---|---|
| s4_final_devlost | 12000 | t=99.7 s | +102.9 s | 1.08, 1.07, 1.10, 1.08 s (DLMAX=5 stop) |
| close_devlost | 12000 | t=95.0 s | +226.8 s | 5.1, 2.6, 7.0 … 3.0 s |
| s4_final_devlost_40k | 40000 | t=294.6 s | +29.0 s | 3.4, 2.0, 1.1 s (DLMAX=5 stop) |
| s4_close_devlost | 40000 | t=294.4 s | +28.7 s | 2.3, 2.4, 2.0 … 2.4 s (203 trips) |
| s4_GFX_devlost | 40000 | t=294.6 s | +25.7 s | 1.3, 1.8, 2.7 … 1.2 s (219 trips) |
| s4_GFX2_devlost | 40000 | t=294.8 s | +24.9 s | 1.7, 3.2, 1.4 … 2.4 s (207 trips) |

Frames between trips, counted as `StorageDisplayComp::Draw ENTER` lines (one per frame for the
single component in these towns; 10 370 of them in the 230 s before the injection ≈ 45 fps) in
`s4_GFX_devlost`: **906** between the injected trip and the first real removal, then
**9, 26, 63, 37, 59, 38, 14, 108, 17 …**, and from trip ~150 on **0 or 1** — i.e. by the end
every Present fails and the frame time IS the round trip. The "per frame" of row 890 is the
steady state, not the first consequence; the route degrades to it.

What immediately precedes the first real removal differs per run (error log, lines between
`round trip COMPLETE` and the first `hr=0x887a0005`): the autosave `[GUL::Serialize] SAVE DONE`
in all four 40k runs and `close_devlost`; `[MusicPlayer] PLAY` in `s4_final_devlost`;
`[SpawnLive] first PER-TICK DoSpawn` in `s4_GFX2`. In the debug log of `s4_final_devlost` a
`WM_ACTIVATE`/`WM_KILLFOCUS` burst sits ~100 lines before it. None of these is a GPU event the
log can tie to the removal; the common factor is only "some hundreds of frames of ordinary
drawing on the new device".

## 2. Why the device is still removed after the round trip

**It is not the injection flag.** READ: `ForceDeviceLostThisFrame` returns true exactly once —
`static bool fired` (contextdata.cpp:613, :624-626); the option is read once into
`Opt().forceDeviceLost` (src/app/options.cpp:284, options.h:180) and only compared.
MEASURED: `INJECTING` = 1 and `hr=0x887a0007` = 1 in every log; every later trip prints
`0x887a0005`, which the injection never produces (it substitutes `DXGI_ERROR_DEVICE_RESET`,
0x887a0007, :648).

**It is not a resize on a dead device.** READ: the recovery path is Shutdown + CreateDevice,
not `DeviceData_Resize`; `D3D11CreateDevice` makes a new device and `CreateSwapChain` a new swap
chain on it (devicedata.cpp:1212-1251). MEASURED: `DX11 D3D11CreateDevice` is logged in every
trip; no `ResizeTrace`/`WindowGuard` line follows any injection.

**RESET vs REMOVED.** READ: our `Finish` and (per its comment, which quotes the listing)
retail's treat the two identically — both skip `GetDeviceRemovedReason` and run the same
notify / release / shutdown / create / recreate / notify chain (:653-655, :561-566). Retail's
five message strings (:681-686) are only used when `CreateDevice` fails. **No path in the tree
logs the removal reason.** The Present `hr` is `DXGI_ERROR_DEVICE_REMOVED`; the underlying reason
(`DXGI_ERROR_DEVICE_HUNG`, `_RESET`, `_DRIVER_INTERNAL_ERROR`, `_INVALID_CALL`) lives in
`ID3D11Device::GetDeviceRemovedReason()` on the OLD device, which is nulled at :668 without being
asked. **Without that value the cause of each real removal cannot be read from these logs.**

**Is the injection a faithful model of a real reset?** READ: by its own banner (:589-602) it is
a faithful drive of the *notify-and-recreate half* — every call after the substitution is the
real one on the real device — and explicitly not a model of a removed device: on the injected
trip the old device is healthy, every object created on it stays usable, and `Shutdown` releases
a live context/swap chain instead of dead ones. A real reset would have all of device 0's GPU
allocations reclaimed by the kernel; ours keeps them alive (see §3, suspect 1).

**Why the new device gets removed — HYPOTHESIS, ranked.**

H1 (leading): **cross-device resource use through the GameUpdateLoop PORT GAP.** READ:
`GameUpdateLoop::OnVideoError()` and `OnVideoRestart()` are empty bodies; the comment says
retail's 0x140095970 "releases the loop's GPU buffers on a D3D device-lost event" and 0x140095930
"recreates them after device-restored" (src/game/Game/gameupdateloop.cpp:2345-2352). Nothing
else releases a game-owned `Video::VertexBuffer`/`IndexBuffer` that is not `dynamic`, or a
game-owned `ImageBuffer` that is neither an External nor of kind 0/1/3/4/5 (the `ReleaseDynamic`
walkers test exactly `p->dynamic` and `ImageBufferIsDynamicResource`,
dynamic_resources.cpp:180-185, :347, :387). Terrain chunks, the worldgen blend/height textures
(`[worldgen] BuildTerrain … blendTex=…`), UI text and `StorageDisplayComp` meshes
(`MeshResize done` every frame), particle and road geometry all keep companions whose
`ID3D11Buffer`/`ID3D11Texture2D` belong to the previous device, and `VertexBufferData_Bind` /
`IASetVertexBuffers` hands them to the new immediate context (`ContextData_BeginCommands`
refreshes the cached context from the DeviceData every frame, contextdata.cpp:508, so the
*context* is new; the *resources* are not). D3D11's release runtime does not validate resource
ownership across devices; submitting another device's allocations is rejected below the runtime
and surfaces as `DXGI_ERROR_DEVICE_REMOVED`. This hypothesis predicts the measured degradation:
objects created between trips k and k+1 land on device k, which is then removed, so the pool of
objects bound to a *removed* device grows with every trip and the first failing draw comes
sooner each time (906 -> tens -> 0-1 frames). It does not by itself explain why the very first
cross-device frames (device 0 healthy, device 1 new) survived 906 frames; one reading is that a
healthy foreign device's allocations are tolerated until a specific object (a `Map` of a
game-owned non-dynamic buffer, a readback for the autosave thumbnail) forces a path that is
not — the autosave precedes the first removal in five of six runs — but that is unverified.

H2: **resource exhaustion from the leaked devices** (§3): each trip leaves one more device
alive with its destroyed-but-unfreed resources; a driver out of system/GPU memory removes the
device. Predicts the same degradation but through memory, not object count; distinguishable by
the removal reason (H1 -> `DRIVER_INTERNAL_ERROR`/`INVALID_CALL`-like; H2 -> `HUNG`/`RESET` or
`E_OUTOFMEMORY` on a create). Not separable from H1 with these logs.

H3 (weak): the `WM_ACTIVATE` callback or `AdjustDisplay` moving the window during
`CreateDevice` while the previous swap chain is still being torn down. Nothing in the logs
supports it beyond the `WndMsg` burst inside every CreateDevice, which is the same burst boot
produces.

What would settle it without guessing (both are logging-only, neither is a fix): read and print
`static_cast<ID3D11Device*>(dd->d3dDevice)->GetDeviceRemovedReason()` at :657, before the null at
:668, and run once with `D3D11_CREATE_DEVICE_DEBUG` on the flags argument at :1212 (the debug
layer names a foreign-device resource in `IASetVertexBuffers` outright). The second needs the
SDK layers installed on the gate machine.

## 3. Where ~100 MB per round trip goes — suspects, ranked

Nothing in these logs measures memory; the 100 MB / 20.8 GB numbers are row 890's harness
measurement. The tree gives the following candidates, in order of how much they can plausibly
account for.

1. **The old `ID3D11Device` is never Released on the lost path, and takes everything destroyed
   on it to the grave it never reaches.** READ: contextdata.cpp:668 nulls `dd->d3dDevice` so
   that `Shutdown`'s `Dx11Release(self->d3dDevice)` (devicedata.cpp:1069) is a no-op; the
   comment (:572-574) says this ordering is retail's and load-bearing. So after N trips there
   are N+1 `ID3D11Device` objects alive in the process. The D3D11 runtime defers object
   destruction on a device's immediate context, and user-mode drivers keep system-memory
   backing for textures and buffers; every External texture/mesh that `OnVideoError`
   `Destruct()`s was created on the device being abandoned, so its `Release` lands on a device
   that nothing will ever flush or destroy. One game's worth of textures and meshes per trip is
   the right order of magnitude for 100 MB. **HYPOTHESIS**, but it is the only per-trip
   allocation in the tree that is provably never freed. In retail a real removal makes the
   kernel reclaim the GPU side, and retail's `GameUpdateLoop::OnVideoError` drops the game's
   references too, so retail's version of this leak is one device object and its runtime
   bookkeeping — not the resource set.
2. **The per-trip reload churn through `System::MemoryAllocator`.** READ: `OnVideoRestart`
   re-`Load`s every External (mainloop_video.cpp:697-705); each load allocates pixel levels,
   mesh arrays and strings, each prior `Destruct` freed the previous ones. Net zero if the
   allocator reuses blocks; a steady climb if it grows pools and never returns pages (the
   engine's allocator is pool-based: `Enter(alloc+0x640); Allocate(0x68, 8)` in
   buffers.cpp:222-230). HYPOTHESIS; would show as private bytes rising without a matching
   rise in D3D-side commit. Not separable from 1 without a heap census.
3. **Game-owned GPU objects never released** (the PORT GAP of §2 H1). These are stranded once
   per device, not re-created, so they are a one-time cost plus whatever the game creates
   between trips — small per trip, but they are also the refs that keep each old device from
   dying even if 1 were fixed by Releasing it.
4. **The DeviceData+0x60 tracked-image registry.** READ: add-if-absent, 8 bytes per entry, x3
   growth, removed in `~ImageBufferData`, freed in the DeviceData dtor (imagebufferdata.cpp:
   228-273, devicedata.cpp:987-990). The DeviceData object itself survives the round trip
   (Shutdown/CreateDevice reuse it), so entries persist, but destroyed ImageBufferDatas unlink
   themselves. Kilobytes at most. Negligible.
5. **The probe device in `DeviceData_EnumerateInterfaces`.** READ: slot 1 is called at
   startup only; `Finish` calls slots 3 and 2 (contextdata.cpp:671-673) and MEASURED: no
   `enumerate` log line recurs — the `DXGI factory + adapter + output enum` mark inside
   CreateDevice is the per-trip factory, which is Released at step 15 (devicedata.cpp:1255-1257;
   the +0x10 field dangles afterwards, retail's quirk). Not a leak.
6. **The swap chain and the MSAA colour/depth buffers.** READ: both released in `Shutdown`
   (devicedata.cpp:1052-1067) before the new ones are made. Not a leak — unless the deferred
   destruction of 1 applies to them too, in which case they are part of 1.

## 4. Harness consequence vs real defect, and what B5 touches anyway

| item | kind | area |
|---|---|---|
| The single injected `DEVICE_RESET` and the notify / release / shutdown / create / recreate / notify chain it drives | faithful drive of retail's `Finish` (PLAN 5.5); nothing to fix | ContextData::Finish — **B5 dx11-seam file** |
| The old device is not Released on the lost path (`d3dDevice = nullptr` before Shutdown) | **retail behaviour**, kept on purpose (contextdata.cpp:572-574). On a real removal it costs one dead device object; under our injection it keeps a *healthy* device 0 and all its resources alive — a harness consequence, not a port defect | ContextData::Finish / DeviceData::Shutdown — B5 files; leave the ordering alone |
| `GameUpdateLoop::OnVideoError` / `OnVideoRestart` are no-ops | **port defect a real removal would also show**: retail releases and recreates the loop's GPU buffers; ours strands them on the dead device. The most likely reason the recreated device is removed again (§2 H1) and the reason the leaked devices can never die (§3 item 3) | code/Game — **DEFERRED device-lost area** (the user deferred device-lost work until after the endgame). Porting it is a `behaviour:` commit against the exe (0x140095970 / 0x140095930), not against VideoDX11-x64.dll |
| No path logs `GetDeviceRemovedReason` | diagnostic gap in the harness, not retail's concern | ContextData::Finish — B5 file; a two-line stderr print is instrumentation of the kind `[DeviceLost]` already is, not a new flag |
| `DeviceData_Resize` and its `Dx11InvalidateConstantBufferBindings` deviation | not on this route at all (MEASURED: never runs after the injection) | B5 file; unaffected |
| Tracked-image registry at DeviceData+0x60 | write-only in the DLL as ported; survives the round trip because the DeviceData object does | B5 file; no change needed for this finding |
| `MainLoop::OnVideoError` / `OnVideoRestart` (runtime) | ported and faithful; both oracles passed in S4 (`calls=1`); the per-trip disk reload is retail's design | runtime; unaffected |
| 20 GB growth | consequence of the loop (100 MB x 200 trips); with the loop bounded it is ≤ 5 x 100 MB and the harness's low-memory stop cannot trip | — |

Recommendation for the coordinator: record §1-§3 against row 890; take the
`GetDeviceRemovedReason` print into the B5 `Finish` work (it changes no behaviour); leave the
`GameUpdateLoop` pair in the deferred device-lost area but name it in row 890 as the suspected
root cause so the deferred work starts there; do not "fix" the `d3dDevice = nullptr` ordering —
it is retail's, and Releasing a removed device there would change what retail does.

## 5. A bounded device-lost gate — proposal

The injection does not loop, so "inject once and clear the flag" is already what the harness
does (§2). The loop is real removals of real devices, and it cannot be stopped by the
injection code without either fixing the cause (the deferred GameUpdateLoop pair) or capping the
recovery. Options, safest first:

1. **Keep the script-side cap** (`devlost.sh` already ends the run after `DLMAX` = 5 COMPLETE
   lines and expects `killed=YES`). It bounds memory at ~5 trips but cannot distinguish "the
   drive worked and then the port's recovery is incomplete" from "the drive worked and the
   device stayed healthy" — the gate's pass criterion must be `INJECT=1, COMPLETE>=1, no crash`,
   not `COMPLETE==1`. This needs no code change and is what S4 ran.
2. **Log the removal reason** (§2) so the gate can assert on it: a first trip with
   `hr=0x887a0007` and no reason, then any `0x887a0005` trip printing its reason. With the
   reason in the log the next session can split H1 from H2 in one run instead of guessing.
3. **A process-side cap in `Finish`**: after the forced trip, count real removals and, past a
   limit, take retail's failure arm (`System::Error::SetText("D3D11 Error: DXGI_ERROR_DEVICE_REMOVED!", true); Display()`, contextdata.cpp:680-689) instead of recreating again. This is
   retail's own exit for an unrecoverable device, so it is not a new behaviour; the limit would
   ride on the existing option's value (e.g. `BANISHED_FORCE_DEVICE_LOST=12000:3`), which is a
   value-syntax extension of an existing flag, not a new `BANISHED_*` flag — but DIRECTION.md §2
   forbids new flags and the coordinator should rule on whether a second field counts. Off
   unless the option is set, so it cannot affect a normal run.
4. **Port `GameUpdateLoop::OnVideoError` / `OnVideoRestart`** — the only change that could make
   the route bounded *by being correct*. Deferred by the user; if H1 is right the gate becomes
   `INJECT=1, COMPLETE==1, no 0x887a0005` and the memory question disappears with the loop.

What cannot be said without a run: whether 4 alone ends the loop (H1 vs H2), and which of §3's
suspects carries the 100 MB. Both need one run with the reason printed and, ideally, the debug
layer; neither needs more than the existing harness plus two prints.
