# Wave 6 — Lane B — `Video::Device` / `Configuration` / `RenderBuffer` / `RenderStateData`

Files: `runtime/src/video/device.cpp`, `runtime/types/video/Device.h`.
Kill switch: `BANISHED_RT_VIDDEVICE=0` (or `BANISHED_RT_VIDEO=0` for the namespace).
`RtVideoDeviceRegister()` returns **9**.

`-fsyntax-only` with the wave's exact command line is **warning-clean**.

---

## 1. The 17 rows of `wave6_lanes.txt` — what happened to each

Every mangled name was checked against the shipped export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile` **before any body was written**
(`reference_resolveexport_phantom_names.md`). **All 17 exist** — no phantoms in this lane. The
addresses and ordinals below are that query's output.

| Export name | Retail | Ord | Ported? | Oracle kind | Diff site name |
|---|---|---|---|---|---|
| `?GetInterfaceCount@Device@Video@@QEBAHXZ` | 0x18000d3b0 | 2729 | **yes** | FULL scalar | `Video::Device::GetInterfaceCount` |
| `?GetInterface@Device@Video@@QEBAAEBUGraphicsInterface@12@H@Z` | 0x18000d3c0 | 2728 | **yes** | FULL scalar (pointer) | `Video::Device::GetInterface` |
| `?GetMultisampleModeCount@Device@Video@@QEBAHXZ` | 0x18000d450 | 2803 | **yes** | FULL scalar | `Video::Device::GetMultisampleModeCount` |
| `?IsMultisampleModeSupported@Device@Video@@QEBA_NH@Z` | 0x18000d460 | 3145 | **yes** | FULL scalar | `Video::Device::IsMultisampleModeSupported` |
| `?ValidateConfiguration@Device@Video@@QEAAXAEAH0000000@Z` | 0x1800f7370 | 4532 | **yes** | FULL — original replayed into alias-preserving scratch out-params | `Video::Device::ValidateConfiguration` |
| `?ApplyChangedSettings@Device@Video@@QEAA_NXZ` | 0x1800f8110 | 1604 | **yes** | FULL — original replayed into a scratch copy of the whole Device | `Video::Device::ApplyChangedSettings` |
| `?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z` | 0x1800f3f10 | 1882 | **yes** | FULL on the 0x18 bytes it writes — original replayed into a scratch RenderBuffer with a stub platform object | `Video::RenderBuffer::Create` |
| `??1RenderStateData@Video@@UEAA@XZ` | 0x1800f8d70 | 662 | **yes** | FULL — original replayed into a scratch qword | `Video::RenderStateData::~RenderStateData` |
| `?Save@Configuration@Video@@QEAAXXZ` | 0x1800f6d10 | 3605 | **yes** | poison + 18-key readback (shadow), then the original runs | `Video::Configuration::Save` |
| `?Startup@Device@Video@@QEAA_NXZ` | 0x1800f7270 | 4091 | no | — | held back, §2.1 |
| `?Shutdown@Device@Video@@QEAAXXZ` | 0x1800f7280 | 4044 | no | — | held back, §2.2 |
| `?RecreateDynamic@Device@Video@@QEAAXXZ` | 0x1800f80b0 | 3522 | no | — | held back, §2.3 |
| `?ReleaseDynamic@Device@Video@@QEAAXXZ` | 0x1800f80e0 | 3536 | no | — | held back, §2.3 |
| `??0RenderBuffer@Video@@QEAA@XZ` | 0x1800f3ee0 | 346 | no | — | held back, §2.4 |
| `??1RenderBuffer@Video@@QEAA@XZ` | 0x1800f4020 | 659 | no | — | held back, §2.4 |
| `?_singleton@Configuration@Video@@0PEAV12@EA` | 0x1801fe7a0 | 4804 | **n/a** | — | DATA export, §2.5 |
| `?_singleton@Device@Video@@0PEAV12@EA` | 0x1801fe7a8 | 4807 | **n/a** | — | DATA export, §2.5 |

**9 ported / 6 held back / 2 not functions.**

### Registration doors

Eight ports are registered by NAME only, because every call site resolves them by name and none
has an address fallback: `engine/funcs/engine_funcs.cpp:2991-3005, 4328, 4358-4366` and
`code/Game/Game/minimap_paint.cpp:101-103` are all `ResolveExport`.

`??1RenderStateData@Video@@UEAA@XZ` is registered by **name AND address**:
`dx11/src/renderstatedata.cpp:317-322` falls back to
`ResolveAddr(Module::Runtime, 0x1800f8d70)` when the name does not resolve, and a name-only
registration would have left that fallback pointing at the DLL (WAVE3_LANE_A.md §6). The address
door is **not counted again** — same function, other registry
(`reference_census_three_blind_spots.md`, the both-doors double count).

### Where these run, and what will exercise them

This matters for whoever runs the wave. Seven of the nine are reached **only from the Options
dialog**:

* `GetInterface`, `GetInterfaceCount`, `GetMultisampleModeCount`, `IsMultisampleModeSupported`,
  `ValidateConfiguration` — `code/Game/Dialog/optionsdialog.cpp` populate/validate paths, so
  merely **opening** Options exercises them.
* `ApplyChangedSettings` and `Configuration::Save` — `optionsdialog.cpp:1164, 1180`, so they need
  **Apply/Ok** to be pressed with a changed setting.
* `RenderBuffer::Create` — `code/Game/Game/minimap_paint.cpp:195`, i.e. any minimap repaint.
* `~RenderStateData` — `dx11/src/renderstatedata.cpp`, on render-state teardown.

A plain `sa-run` autostart will hit `RenderBuffer::Create` and `~RenderStateData` and **will not
hit the other seven**. Reading only `divergences=` on such a run would report this lane clean while
seven of its nine sites had `calls=0`, which is exactly the failure
`reference_uncalled_port_is_not_verified.md` is about. The lane needs a run that opens Options
(the front-end button harness, `BANISHED_AUTOMENU_BUTTON`/`_CLICKS`, reaches the main menu's
Options button) and presses Ok. **I did not run the game** — that is the wave lead's step — so the
`calls=` column for all nine sites is unmeasured and no port in this lane is yet verified.

---

## 2. Hold-backs, with reasons

### 2.1 `Device::Startup` (0x1800f7270)

Global state and platform coupling on every line, and no oracle exists in either direction:

* copies `Application::_singleton[0]` (the HWND) into `Device+0x90`;
* calls the private `ValidateConfiguration()` (the **no-argument bool overload**, a different
  function from the exported 8-argument one this lane ports) and, on failure,
  `System::Error::SetText`, which writes the process-wide error string;
* loops over the three renderer interfaces, calling `CreateVideoInterface`, which
  `LoadLibrary`s a plugin DLL and calls its `CreateDeviceData` — in this process that is our own
  `dx11/` factory, so running it a second time would create a **second D3D11 device and swap
  chain**;
* on success registers two USER32 message callbacks (`Application::RegisterMessageCallback` for
  `WM_SIZE` and `0x232`/`WM_EXITSIZEMOVE`), which would be registered twice.

Running ours and the original both is therefore destructive, and running only ours with no
comparison is an unmeasured port. Held back.

### 2.2 `Device::Shutdown` (0x1800f7280)

Same class of problem, mirrored: it walks the `Application` callback tree removing the two
message callbacks by id (through the internal `FUN_18000aba0`), then calls the DeviceData's vtable
slot 3 (`Shutdown`) and slot 0 (scalar deleting destructor) and nulls `Device+0x100`. A second
execution destroys an already-destroyed DX11 device. No oracle. Held back.

### 2.3 `Device::RecreateDynamic` / `Device::ReleaseDynamic` (0x1800f80b0 / 0x1800f80e0)

These are four-line forwarders and porting them would be easy — that is not the reason to hold
them.

```
RecreateDynamic: IndexBuffer::RecreateDynamic(); VertexBuffer::RecreateDynamic();
                 ImageBuffer::RecreateDynamic();  RenderBuffer::RecreateDynamic();
ReleaseDynamic:  RenderBuffer::ReleaseDynamic();  ImageBuffer::ReleaseDynamic();
                 VertexBuffer::ReleaseDynamic();  IndexBuffer::ReleaseDynamic();   // reverse order
```

Each callee is a **static** that walks its class's process-global intrusive list and destroys or
recreates every live GPU resource on it. Running both implementations releases twice and recreates
twice; there is nothing to compare and the second pass operates on freed objects. `RT_DIFF_SHADOW`
would have nothing to assert either — the observable effect is entirely inside `dx11/`.

Secondary reason: three of the four callees belong to **lane C** (`Video::ImageBuffer`,
`IndexBuffer`, `VertexBuffer`). Porting the forwarder before those classes have owners would
hard-code call-by-address into three classes another lane is actively reversing.

### 2.4 `RenderBuffer::RenderBuffer` (0x1800f3ee0) and `~RenderBuffer` (0x1800f4020)

The ctor is three zero stores plus `PlatformConstruct` (0x1800f4320, exported as ordinal 3341);
the dtor has no separate platform half. Both bodies:

* take the **Device's `CRITICAL_SECTION` at `Device+0x60`** and maintain the recursion bookkeeping
  at `+0x88`/`+0x8c` by hand. That is Win32, and PLAN §4.1 decision 1 forbids it outside a
  `_win` twin — this lane would have had to open `device_win.cpp` for two functions;
* link into / unlink from a **process-global circular doubly linked list** whose head,
  `_firstRenderBuffer`, is a DLL-internal variable with no export. The list is what
  `RenderBuffer::ReleaseDynamic` / `RecreateDynamic` (§2.3) walk;
* create (`DeviceData` vtable slot 0x98) or destroy (`RenderBufferData` slot 0, flag 1) a real GPU
  object.

Running both implementations would double-link the list and leak or double-free a D3D resource,
so no full oracle exists; a post-state shadow could check the link invariants but not the resource
lifetime, and getting the unlink wrong corrupts a list that a completely different subsystem walks
on every device reset. The prudent split is to port `Create`, which is pure arithmetic on the same
object, and leave construction and destruction of the object — and of the list — atomic in the DLL
(PLAN §4.1 decision 5: switch a stateful class **whole** or not at all).

Note this is a **deliberate half-ported class**, which §4.1 decision 5 warns about. It is safe here
for a specific reason: `Create` neither allocates, frees, nor stamps a vtable, and it writes only
the four fields `+0x00/+0x08/+0x10/+0x14` that no other RenderBuffer method reads before Create
runs. There is no second vtable and no second allocation in play — the hazard decision 5 is about
does not arise. If a later wave ports the ctor/dtor it should take `Create` with them.

### 2.5 The two `?_singleton@` DATA exports

`?_singleton@Configuration@Video@@0PEAV12@EA` and `?_singleton@Device@Video@@0PEAV12@EA` are
**variables, not functions**. `RegisterRuntimeExport` installs a redirection that `ResolveExport`
hands back to callers which then treat the result as *the address of the slot* and dereference it
(`engine/funcs/engine_funcs.cpp`, `dx11/src/imagebufferdata.cpp:206`,
`standalone/address_registry.cpp:1406`, `standalone/main.cpp`). Registering one would hand out a
pointer to our code where the `Video::Device*` slot belongs. Both stay on the DLL until cutover and
are counted as **unported**, the same call Wave 4 made for `IO::FileSystem::_singleton` and Wave 3
for the Object data symbols.

---

## 3. Struct layouts recovered, and the evidence

All of these are in `runtime/types/video/Device.h` with a `static_assert` on every field offset.

| Type | Size | Pinned by |
|---|---|---|
| `Video::Device` | 0x108 | ctor `??0Device@Video@@QEAA@W4DeviceType@01@@Z` 0x1800f7160 (writes every field except `+0x88`/`+0x8c`/`+0x98`), plus the four getters, `ApplyChangedSettings`, `ValidateConfiguration`, `Startup` and `RenderBuffer::PlatformConstruct` |
| `Video::Device::GraphicsInterface` | 0x18 | ctor 0x18000cd90 (three qwords) + `GetInterface`'s `* 0x18` stride |
| `Video::Device::Adapter` | 0x38 | ctor 0x1800f7100 + `ValidateConfiguration`'s `* 0x38` stride |
| `Video::Device::DisplayMode` | 0x18 | ctor 0x18000d300 + the `* 0x18` strides in `ValidateConfiguration` and `ApplyChangedSettings` |
| `Video::Device::RefreshRate` | 0x08 | ctor 0x18000d270 + `ValidateConfiguration`'s `piVar2 += 2` int-pair walk |
| `Video::Configuration` | 0x44 (inferred) | ctor 0x1800f6f10 gives each field its default; `Configuration::Save` 0x1800f6d10 gives each field its persisted name. The two agree field for field. |
| `Video::RenderBuffer` | 0x30 | ctor 0x1800f3ee0 + `PlatformConstruct` 0x1800f4320 + `Create` 0x1800f3f10 + `~RenderBuffer` 0x1800f4020 |

The three `Array`-shaped members on `Device` (`+0x20` modes, `+0x30` multisample flags, `+0x40`
graphics interfaces) and the ones on `Adapter`, `DisplayMode` and `GraphicsInterface` all match the
16-byte `{_data, _size:i32, _capacity:i32}` triple Wave 2 recovered
(`runtime/types/collection/Collection.h`). Only the `_data` and `_size` words are read by any body
this lane touched, so the `_capacity` slots are labelled **inferred from the Array shape** rather
than measured.

### Agreement with `standalone/video_interface.cpp` — YES, exactly

`standalone/video_interface.cpp:57-62` hardcodes `kSlot0 = 0xb0`, `kSlotStride = 0x18`,
`kOffModule = 0x00`, `kOffGetName = 0x08`, `kOffCreate = 0x10` and writes those fields on the live
Device object. `Device::dllSlots[3]` is at `+0xb0` with `sizeof(InterfaceSlot) == 0x18` and members
`module`/`getDeviceName`/`createDeviceData` at `+0x00`/`+0x08`/`+0x10`, and there are
`static_assert`s on all five. `standalone/main.cpp:1068-1069` (`+0xf8` active index, `+0x100`
DeviceData\*), `dx11/src/buffers.cpp:209`, `dx11/src/imagebufferdata.cpp:230` and
`dx11/src/renderstatedata.cpp:343` (all `+0x100`) also agree and are named in the asserts.
`code/Game/Dialog/optionsdialog.cpp:359` reads `Device+0x98` as the anisotropy level and
`:463-470` walks `GraphicsInterface+0x08` → `Adapter * 0x38` → `+0x08`/`+0x10` modes: both agree.

**One thing worth stating loudly**, because it is the easiest way to corrupt this object: there are
**two** stride-0x18 arrays on `Device`. `Array<GraphicsInterface>` at `+0x40` is what
`GetInterface` serves (enumerated renderers); the raw `InterfaceSlot[3]` at `+0xb0` is the loaded
plugin DLL slots. They are unrelated memory and confusing them writes function pointers over
adapter lists.

---

## 4. Stale/incorrect claims this lane disproved

**(a) The lane brief itself — `Video::Configuration::Save` does NOT write the registry.**
The brief said: *"`Video::Configuration::Save` writes the real HKCU registry — Wave 4 held back
`Persistence::WriteSettings` for exactly that reason (`WAVE4_LANE_C.md`). Apply the same standard:
no safe oracle, no port."* The premise is false, and it is the reason this port exists. `Save`
(0x1800f6d10) is 18 calls to `IO::Persistence::Set` (0x1800b80c0 / 0x1800b8380), which is an
**in-memory `Collection::Set` insert** — Wave 4 lane C ported both `Set` overloads for exactly that
reason and held back only `ReadSettings`/`WriteSettings`, which are the registry pair. `Save`
touches no registry key. `IO::Persistence::WriteSettings`, called elsewhere at shutdown, does. The
Wave 4 standard is met, not violated.

**(b) `engine/funcs/engine_funcs.h:2263` (a file this lane does not own) — the 8th argument is not
"adapter2".** It currently reads:

```
// Video::Device::ValidateConfiguration(this, &renderer, &adapter, &width, &height,
//                                       &refreshNum, &refreshDen, &antialias, &adapter2)
```

There is no second adapter parameter. Retail's body (0x1800f7370) clamps the 8th out-parameter
against `Adapter+0x28`, the adapter's **maximum anisotropic filter level** — the same value
`optionsdialog.cpp:359` reads from `Device+0x98` to build the texture-filter combo. Suggested
correction: rename the last parameter `&filter` and add a line saying the exe passes
`&savedAdapter` for it (`optionsdialog.cpp:315`, matching retail exe 0x140072eb0), which is a
retail quirk, not a typo. Not edited here — different lane's file, and `engine_funcs.h` is being
touched concurrently.

**(c) `runtime/types/video/ImageBuffer.h:41` documents only bit 0 of `flags`.** The comment reads
`// +0x34  bit 0 = hasMipMaps (controls GetLevelCount)`. `RenderBuffer::Create` tests three more
bits of the same word, and they decide the render target's dimensions:

* **bit 6** — the image's own size, right-shifted by `Video::Configuration::shadowScalar`;
* **bit 8** — exactly the back buffer's size (`Device+0x04` / `+0x08`);
* **bit 9** — half the back buffer's size (signed `/2`, not `>>1`).

They are declared as `RtVideo::ImageBufferSizeFlag` in `runtime/types/video/Device.h` rather than
added to `ImageBuffer.h`, because that header is shared with **lane C**, which owns
`Video::ImageBuffer` this wave. Lane C or the wave lead should fold them in and delete the
duplicate enum.

**(d) `code/Game/Dialog/optionsdialog.cpp:297-305`** describes the `idiv 0` hazard in
`ValidateConfiguration` as coming from `*p6` (`savedRefreshDen`) only. The expression is
`piVar2[0] / piVar2[1] == *param_5 / *param_6`, so the **adapter's own** refresh denominator is a
divisor too. In practice it cannot be zero (the enumerator never adds a `RefreshRate` with a zero
denominator), so the existing guard is still the right one; the comment is just narrower than the
code. Not edited — different lane's file.

---

## 5. Deviations from a literal transcription, all deliberate

1. **`ValidateConfiguration` drops a dead call.** At 0x1800f7417 retail calls `FUN_180010760` on a
   stack pair zeroed a few instructions earlier and never written. `FUN_180010760` is an inlined
   `Collection::Array` destructor whose first act is `if (*p == 0)` → skip, so on that zero pair it
   frees nothing. Emulating it would mean calling the DLL allocator to free a null pointer; the
   port omits it and says so in the body comment.
2. **`ValidateConfiguration`'s always-true `!= -1`.** The first refresh loop compares its match
   index against `-1`, but the index starts at 0 and only increments — a compiler artifact. The
   *consequence* is real and is kept: an exact refresh match leaves `*refreshNum`/`*refreshDen`
   alone and skips the ratio fallback.
3. **`ApplyChangedSettings` tests `Configuration::fullscreen` once, not twice.** Retail re-tests it
   for the height as well; nothing between the two writes it.
4. **`Configuration::Save` is written as a table-driven loop** over
   `{key, isBool, offset}` in retail's exact call order, rather than 18 hand-written calls. Same
   sequence, same values, same overloads.
5. **`Configuration::Save` reaches `IO::Persistence::Set` through `ResolveExport`, not
   `RtOriginal`** — so those 18 calls go to Wave 4's port when it is registered and to the DLL when
   it is not, which is what retail's own body does relative to its class. Every *other* call the
   lane makes into the DLL (`GetDefaultResolution`, and every `THEIRS` inside a diff guard) uses
   `RtOriginal`/`RT_ORIGINAL_ONCE` so the oracle keeps comparing like with like.
6. **`Video::Device::GetDefaultResolution` (0x1800f7540, ordinal 2559) is called, not ported.**
   `ValidateConfiguration` falls back to it when the requested resolution is not in the adapter's
   list. It is exported and public, but it is not one of this lane's 17 functions and nothing in
   our tree calls it directly — the Wave 5 lane D / Wave 4 lane C shape for a callee that belongs
   to no lane. It is resolved once via `RT_ORIGINAL_ONCE`, never per call
   (`reference_rtoriginal_is_getprocaddress.md`).

---

## 6. Oracle designs worth reading before trusting a clean run

* **`ValidateConfiguration` rebuilds the caller's argument aliasing.**
  `optionsdialog.cpp:315` passes `&savedAdapter` as **both** the 2nd and the 8th argument (retail
  exe 0x140072eb0 does the same). If the harness replayed the original into eight *distinct*
  scratch ints, the original would see a different world from ours and report a divergence that is
  purely an artifact of the harness. The scratch pointers are therefore built with the same alias
  graph as the caller's, by pointer comparison. The failure message names the offending parameter
  (`renderer`/`adapter`/`width`/…) rather than saying "differs"
  (`reference_shadow_predicate_must_name_its_failing_term.md`).
* **`ApplyChangedSettings` replays the original into a `memcpy` of the whole Device.** The replica
  shares the display-mode array by pointer, which is safe because the function only reads it. Four
  fields plus the return value are compared, and the predicate names all five.
* **`RenderBuffer::Create` replays the original into a scratch `RenderBuffer` whose platform object
  is a stub** whose vtable slot 1 is `mov al,1; ret` — so the second run cannot create a second GPU
  resource. Only the 0x18 bytes Create writes are compared; the **return value is deliberately not
  compared**, because theirs came from the stub and carries no information.
* **`Configuration::Save` poisons before it writes.** A readback-only oracle would not catch a
  *wrong key name* — we would read back the same wrong key we wrote. So under `BANISHED_RT_DIFF=1`
  all 18 retail keys are first set to a value that differs from the one about to be written; ours
  then runs; then all 18 are read back through the DLL's `Persistence::Get` and required to equal
  the `Configuration` field they came from. A port that writes the wrong key, the wrong field, or
  nothing at all leaves a poisoned value and the message names the key. The original runs
  afterwards regardless, so the settings map ends the call in exactly the state retail would leave
  it in even when ours was wrong.

### Census side effect this lane introduced

`tools/runtime_census.py --namespace Video --unported` now lists a **third** unportable data
symbol alongside the two `?_singleton@` rows: `??_7RenderStateData@Video@@6B@`. That is this
lane's doing — `Video_RenderStateData_Dtor` resolves the exported vtable by name rather than
hardcoding 0x180158a60, so the census sees a new `called` export it can never see registered. It
adds 1 to Video's `called` denominator and can never be ported before cutover, exactly like the
`_singleton` pair. Resolving by name is still the right call (it is what makes the one store in
that dtor robust against a rebased DLL); the denominator cost is worth naming so nobody hunts it
later. Lane C's `buffers.cpp` has already added two more of the same kind
(`??_7ImageBuffer@Video@@6B@`, `??_7ImageBufferData@Video@@6B@`).

---

## 7. Not verified

* **Nothing in this lane has been executed.** No build, no `sa-run`, no `rt_run.sh` — the wave lead
  runs those serially. Every "FULL oracle" above is a design, not a result. Until a run reports
  non-zero `calls=` per site, no port here is verified
  (`reference_uncalled_port_is_not_verified.md`), and §1's note about seven Options-only sites is
  the specific thing to watch.
* **`sizeof(Video::Configuration) == 0x44` is inferred**, from natural alignment given the last
  field at `+0x42`. No body this lane read allocates a `Configuration`, so there is no
  `static_assert` on the size — only on the offsets.
* **`Adapter+0x2c`, `+0x30`, `+0x34` have no recovered meaning.** The ctor writes `-1` to `+0x28`
  and `+0x2c` as one qword and `-1` to `+0x30`; nothing this lane read consumes them. `+0x34` is
  not written by the ctor at all and may be tail padding.
* **`Device+0x10..0x1f` and `+0x50..0x5f`** are zeroed by the ctor and read by nothing this lane
  touched; they are `gap` arrays.
* **`Device+0x9d`** is zeroed with `settingsChanged` in a single 2-byte store. Its meaning is not
  recovered.
* **Whether a `Device` detour would ever be needed** is untested. The DLL calls
  `ValidateConfiguration`, `GetInterface` and friends internally (from `Startup`,
  `CreateVideoInterface`), and registration does not redirect those — but since every ported body
  here is read-only on the Device except `ApplyChangedSettings` (which only our Options path calls),
  the DLL's internal callers reaching the DLL's own copies is harmless. No detour was installed and
  none is proposed. This is the wave's first genuinely live instance of PLAN §4.2's closing note,
  and this lane's answer to it is "not needed here, for this reason", not "solved".
