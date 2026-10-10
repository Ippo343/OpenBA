# Wave 6 — Lane F — `Graphics::Camera` + `Renderer` + `SunLight` + `Material*`

Branch `wave/upgrade-path-complete`. Scope: the 20 rows under `=== LANE F ===` in
[wave6_lanes.txt](wave6_lanes.txt). Files: `runtime/src/graphics/renderer.cpp`,
`runtime/types/graphics/Camera.h`, `runtime/types/graphics/Material.h`.

**12 of 20 ported. 8 held back, each with a written reason.**
`RtGraphicsRendererRegister()` returns **12**.

---

## 1. Function table

Every one of the 20 mangled names was checked against the export directory of the shipped
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile` **before any body was written**
(`reference_resolveexport_phantom_names.md`: a name the DLL does not contain resolves NULL in
silence, the port still registers, and the census still counts it). **All 20 are present.** The
addresses and ordinals below are that query's output.

| Export | Retail | Ord | Ported? | Oracle kind | Diff site name |
|---|---|---|---|---|---|
| `??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z` | `0x1800fe960` | 72 | **yes** | FULL — scratch replay, byte-compare 0xd8 | `Graphics::Camera::Camera` |
| `?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z` | `0x1800fe5c0` | 3885 | **yes** | FULL — scratch replay, byte-compare 0xd8 | `Graphics::Camera::SetFrustum` |
| `?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z` | `0x1800feb00` | 3838 | **yes** | FULL — scratch replay | `Graphics::Camera::SetCameraToWorld` |
| `?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z` | `0x1800fe560` | 4075 | **yes** | FULL — scratch replay | `Graphics::Camera::SnapCameraToWorld` |
| `?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ` | `0x1800fe3e0` | 2680 | **yes** | FULL — both into private slots, byte-compare 0x64 | `Graphics::Camera::GetFrustum` |
| `??0Renderer@Graphics@@QEAA@XZ` | `0x180105180` | 354 | **yes** | FULL — scratch replay, byte-compare 0x50 | `Graphics::Renderer::Renderer` |
| `??1Renderer@Graphics@@QEAA@XZ` | `0x1800180a0` | 663 | **yes** | post-state only (`RT_DIFF_SHADOW`) | `Graphics::Renderer::~Renderer` |
| `?GetCullCamera@Renderer@Graphics@@QEAAAEBVCamera@2@XZ` | `0x180017f90` | 2521 | **yes** | FULL — pointer compare | `Graphics::Renderer::GetCullCamera` |
| `?GetShader@Renderer@Graphics@@QEBAHXZ` | `0x180018010` | 2897 | **yes** | FULL — scalar compare | `Graphics::Renderer::GetShader` |
| `??0MaterialInstance@Graphics@@QEAA@XZ` | `0x180103980` | 267 | **yes** | FULL — scratch replay, byte-compare 0x60 | `Graphics::MaterialInstance::MaterialInstance` |
| `?SetMaterial@MaterialInstance@Graphics@@QEAAXV?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z` | `0x180103340` | 3906 | **yes** | FULL — scratch replay | `Graphics::MaterialInstance::SetMaterial` |
| `?AddTexture@MaterialInstance@Graphics@@QEAAXAEBVString@System@@V?$ExternalPtr@VImageBuffer@Video@@@Object@@@Z` | `0x180103350` | 1590 | **yes** | post-state only (`RT_DIFF_SHADOW`) | `Graphics::MaterialInstance::AddTexture` |
| `?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z` | `0x180104bb0` | 2258 | no | — | — |
| `?AddDrawItem@Renderer@Graphics@@QEAAXPEBVGeometry@2@PEBVMaterialInstance@2@GGMMHPEBXI@Z` | `0x1801057b0` | 1547 | no | — | — |
| `?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z` | `0x180102c90` | 4028 | no | — | — |
| `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` | `0x1801033e0` | 1678 | no | — | — |
| `??0SunLight@Graphics@@QEAA@XZ` | `0x1801081f0` | 423 | no | — | — |
| `?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z` | `0x180107f10` | 3821 | no | — | — |
| `?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A` | `0x1801fa740` | 4882 | n/a | — | — |
| `?_typeInfo@MaterialInstance@Graphics@@0VBaseTypeInfo@Object@@A` | `0x1801fa778` | 4883 | n/a | — | — |

**Every port is registered twice**, by mangled name and by retail address.
`engine/funcs/engine_funcs.cpp:2856` resolves `Camera::SetCameraToWorld` by ADDRESS
(`ResolveAddr(Module::Runtime, 0x1800feb00)`) while every other lane-F consumer resolves by name.
A name-only registration would have left that one call site on the DLL while its siblings came
here — two implementations taking turns on one live `Camera`.

### The scratch-replay oracle, stated once

Nine of the twelve write into a caller-owned object. The pattern is:

1. copy the object's **pre-state** into a private `before` (only when `BANISHED_RT_DIFF` is on);
2. run our body on the real object;
3. run the DLL's export on `before`;
4. `RT_DIFF_BYTES` the two whole objects.

The original never touches live state, and because both start from byte-identical memory, fields
neither implementation writes (`Renderer+0x3c`, `+0x4c`; the `Frustum` pad bytes `SetFrustum`
skips) compare equal instead of comparing garbage against garbage. That property is what makes it
safe to reproduce retail's *gaps* rather than tidying them away.

---

## 2. Held back — the eight, with reasons

### `Renderer::Draw` (`0x180104bb0`) — **the honest answer is no, and it is not close**

`Draw` is the entire frame. In ~130 lines of decompiled body it:

* calls four other `Renderer` methods this lane did not port and did not read
  (`Reset` `0x180105110`, `AddScene` `0x180105700`, `BeginDrawGroup` `0x180104fd0`) plus
  `Scene::AddCulledDrawItems` (`0x1801017a0`, a **private** member — `AEBA` in its mangling);
* **writes `Video::Context`'s private fields directly** — the dirty-flag word at `+0x10`, `+0x0c`,
  the render-buffer slot at `+0x28`, two viewport/scissor blocks at `+0x1d0` and `+0x1e0`, and a
  virtual call through `Context+0x2160`. `Video::Context` is **lane A's** class and is being
  ported concurrently; pinning its layout from here would duplicate that work in a second file
  and guarantee the two disagree;
* dispatches three `Geometry` virtuals (slots `0x18`, `0x48`, `0x50`) — **lane D's** class;
* iterates a `Scene` intrusive list at `Scene+0x60` — also lane D;
* sorts each pass list through the unexported `FUN_180062d60`.

Porting it would mean pinning `RendererSceneEntry` (0x110), `RendererDrawGroup` (0x1f0),
`RendererDrawItem` (0x38) **and** `Video::Context` out to `+0x2160`, in a wave where two other
lanes own two of those. This is exactly the case PLAN §4.3 risk 3 points at, and
[WAVE6.md](WAVE6.md)'s own framing ("any lane that would change a layout … must hold back and say
so"). **Held back.** It should be revisited after lanes A and D land, as a lane of its own.

What this lane *did* recover for that future lane is in `runtime/types/graphics/Camera.h`: the
`Renderer` object itself (0x50, every field), the scene-entry and draw-group strides with the
fields `Draw` and the two accessors name, and the fact that `sizeof(Camera) == 0xd8` is pinned
twice over by those two embeddings.

### `Renderer::AddDrawItem` (`0x1801057b0`)

Readable, but held back for two independent reasons.

1. **No safe oracle.** It appends into a live draw group. A scratch replay would share
   `_data` with the real group, and the array-grow helper frees the old buffer on
   reallocation — so "theirs" would release memory the live `Renderer` still points at. A
   post-state shadow is possible but would be checking a 0x38-byte record whose layout is the
   second problem:
2. **It writes an element type this lane did not pin.** The stack record it builds spans
   `+0x00..+0x33` (Geometry\*, the material's `ExternalPtr`, MaterialInstance\*, two `uint16`, a
   float, an int, a `const void*`, a `uint32`), and the array helper it hands that to has a stride
   this lane did not read out. Guessing it would put a wrong layout in a header, which is worse
   than an absent one.

Its only consumer is `engine/funcs/engine_funcs.cpp`, and it is called once per drawable per
frame — i.e. it is hot, and a subtle error would be invisible until a model stopped drawing.

Recovered anyway, for the record: the three guard conditions under which retail **silently drops**
the item are (a) the draw group's pass mask `group+0x1bc` has no bit set for
`MaterialInstance+0x20`; (b) the group's shader index `group+0x1b8` is negative or `>=` the
material's program count at `Material+0x30`; (c) the selected program —
`Material+0x28`'s array indexed by that shader, the same array `MaterialInstance::Setup` hands to
`Video::VideoProgram::SetupShaders`, so these are `VideoProgram*` — has a null `+0x20`, or a null
`+0x28`/`+0x30`, or either of those two sub-objects has `+0x48 <= 0` (a count of something the
program needs; this lane did not identify what, and does not guess). This **confirms** the claim
at `standalone/scene_watch.cpp:593`
and `scene_watch.h:71` that `AddDrawItem` silently drops items — no correction needed there.

### `MaterialInstance::Setup` (`0x180102c90`)

Pure with respect to the material, but its whole effect is a **write into `Video::Context`** — the
dirty word at `+0x10`, the texture-dirty mask at `+0x18`, the texture slots at `+0x30 + slot*8`
and the constant block at `+0x208 + slot*0x10`. Same reason as `Draw`: that layout belongs to
lane A, there is no oracle for a function whose only output is a mutation of another lane's live
object, and it also calls `Video::VideoProgram::SetupShaders`, a fourth Video class. **Held back.**
Its one consumer is `code/Game/Player/tool_base_tilerender.cpp`.

### `MaterialInstance::BuildParameters` (`0x1801033e0`)

The largest function in the class. Held back because:

* it reads **`Graphics::Material`'s** internals four levels deep
  (`material+0x28 → [i] → +0x30 → +0x20/+0x30`, with per-entry strides of 0x10) — a class this
  lane deliberately did not lay out, since no ported body needs it;
* it calls `Material::GetTextureName` / `GetConstantName`, which **return `System::String` by
  value** — hidden return slot plus a refcounted temporary destroyed on every iteration
  (`reference_hidden_retval_and_int_pointer_args.md`), and the private
  `MaterialInstance::FindTexture` / `FindConstant` (`0x180103020` / `0x180102f40`);
* it allocates and frees repeatedly through the allocator's critical section, which cannot appear
  in a non-`_win` file at all (PLAN §4.1 decision 1);
* and it is a bulk mutation of `_parameters` with no replayable oracle.

**Held back.** It is a good candidate for a follow-up lane paired with `Graphics::Material`.

### `SunLight::SunLight` (`0x1801081f0`) and `SunLight::Set` (`0x180107f10`)

Both **create and destroy GPU resources**. The ctor allocates a `Video::ImageBuffer` (0x68) and
calls its virtual `Create` for a 2048×2048 depth target, allocates a second `ImageBuffer` for the
colour target, allocates a `Video::RenderBuffer` (0x30), calls `PlatformConstruct` and
`RenderBuffer::Create`. `Set` does the same in reverse and forwards — `PlatformDestruct`,
`operator delete`, then re-create — on a backend-capability change.

Three reasons, any one sufficient:

1. **No oracle exists, and constructing a second one is not free.** Running the original a second
   time would allocate two more 2048×2048 render targets and leak them, or free the ones the live
   object holds.
2. **`Video::RenderBuffer` and `Video::ImageBuffer` are lanes B and C**, being ported
   concurrently; `SunLight` calls their ctors, their virtual `Create` (vtable slot `0x48`) and
   their platform hooks.
3. `SunLight` derives from `Graphics::Geometry` — **lane D** — and its ctor calls
   `Geometry::Geometry` and then stamps `??_7Light@Graphics@@6B@` (`0x180158d30`) and
   `??_7SunLight@Graphics@@6B@` (`0x180158e10`) in sequence. Porting a derived ctor while its base
   is being ported in another file is the split-class hazard PLAN §4.1 decision 5 forbids.

**Held back.** Recovered for whoever takes them: `sizeof(SunLight) == 0x108` (agreeing with the
existing note at `engine/funcs/engine_funcs.h:3089`); direction `Vector3` at `+0xc0`, sun colour
`Vector4` at `+0xcc`, ambient `Vector4` at `+0xdc`, depth `ImageBuffer*` at `+0xf0`, colour
`ImageBuffer*` at `+0xf8`, `RenderBuffer*` at `+0x100`; the ctor sets `Geometry+0x30 = 0x8000` and
forces the bounds at `+0x34`/`+0x40` to ±1.0e6 (`DAT_180161778` / `DAT_18016173c`, read out of the
DLL as `00 24 74 c9` / `00 24 74 49`), setting the dirty bit `Geometry+0x2c |= 2`.

### `?_typeInfo@Material@…` and `?_typeInfo@MaterialInstance@…`

**These are exported DATA symbols, not functions** — `0x1801fa740` and `0x1801fa778`, both in the
DLL's `.data`. There is no body to port and nothing the runtime registry can redirect: our call
sites (`code/Game/Game/gameupdateloop_load.cpp`, `standalone/reg_unreg_lane3.cpp`) read the
`Object::BaseTypeInfo` object the DLL owns and that the DLL's own reflection registry links into
its `_firstTypeInfo` chain. They become ours **at cutover**, when the type registry itself is
built by our code, and not before. They are counted here as **n/a**, not as ports — the same
treatment `FileSystem::_singleton` got in Wave 4 and `Device::_singleton` gets in lane B.

---

## 3. Struct layouts recovered

Full evidence lives beside each field in the two headers. Summary:

### `Graphics::Camera` — 0xd8

| Off | Field | Evidence |
|---|---|---|
| `+0x00` | `Volume::Frustum frustum` (0x64) | ctor `0x1800fe960` copies 25 dwords from the `Frustum` argument; `GetFrustum` `0x1800fe3e0` copies the same 25 back out |
| `+0x64` | `Math::Transform3 cameraToWorld` (0x30) | ctor copies 12 dwords from the `Transform3` argument; `SetCameraToWorld`/`SnapCameraToWorld` write exactly this range |
| `+0x94` | `Math::Matrix projection` (0x40) | ctor zeroes 16 dwords; `SetFrustum` writes 16 dwords from `Frustum::Projection`'s result |
| `+0xd4` | `uint8 swapZUpToYUp` | ctor stores its `bool` argument; `SetFrustum` branches on it |

**`sizeof == 0xd8` is pinned twice, independently of the field list**, which is what makes it
trustworthy: `Renderer::Draw` reads a scene entry's own field at `+0xe8` where its embedded Camera
starts at `+0x10` (`0x10 + 0xd8 == 0xe8`), and `GetShader` reads a draw group's shader at `+0x1b8`
where its embedded Camera starts at `+0xe0` (`0xe0 + 0xd8 == 0x1b8`).

**`+0xd4` is NOT `isPerspective`** — see §5.

### `Graphics::Renderer` — 0x50

Three `Collection::Array` triples at `+0x00` (scene entries, stride 0x110), `+0x10` (draw groups,
stride 0x1f0) and `+0x20` (drawables, stride 0x10); then `currentScene +0x30` and
`currentDrawGroup +0x34` (ctor writes one qword of `-1` over both), `+0x38` (0), **`+0x3c`
unwritten**, `frameAlpha +0x40`, `contextSerial +0x44` (`-1`), `+0x48` (0), **`+0x4c` unwritten**.
Strides come from the destructor at `0x18001c8c0` (`+0x1f0` per group) and `Draw` (`*0x110` per
scene entry, `*0x10` per drawable). `0x50` agrees with the pre-existing note at
`engine/funcs/engine_funcs.h:3081`.

The two unwritten dwords are reproduced as gaps, not zeroed — see §1's oracle note.

### `Graphics::MaterialInstance` — 0x60

The ctor at `0x180103980` *is* the layout: three vtable stamps, `+0x08/+0x10/+0x18` zeroed
(`Object::External`'s members), `+0x20` dword, `+0x24` **word** (so `+0x24` and `+0x25` are two
separate bytes — `BuildParameters` and `AddDrawItem` confirm, writing and testing them
individually), then seven zero qwords `+0x28..+0x5f`. `+0x28` is the `ExternalPtr<Material>`;
`+0x30`, `+0x40`, `+0x50` are three `Array` triples whose element strides — `0x20`, `0x18`, `0x10`
— come from `Setup` (`shader*0x20`, `index*0x18` with a `Vector4` at `+0x08`) and `AddTexture`
(the `{String, ExternalPtr}` pair the grow helper copies with a `0x10` stride).

### Not pinned, on purpose

`Graphics::Material`, `Graphics::SunLight` beyond the fields listed in §2, `RendererDrawItem`, and
`RendererDrawGroup`'s first 0xe0 bytes. Each is left as a forward declaration or an explicit blob.

---

## 4. `MaterialInstance` vtable stamping — the per-class argument

**Verdict: safe, and the port stamps `??_7MaterialInstance@Graphics@@6B@` by name.**

PLAN §2.2 D is explicit that an exported vtable makes the *address* available but that whether
stamping it is *safe* is a per-class argument, and that Thread's argument does not transfer. Here
is this class's.

**What retail does.** `??0MaterialInstance` stamps three vtables in sequence — MSVC emitting each
base constructor inline, each re-seating the vptr:

```
0x180103991  LEA RAX,[0x180154788]  MOV [RCX],RAX   ??_7Base@Object@@6B@       ord 1389
0x18010399b  LEA RAX,[0x1801547a0]  MOV [RCX],RAX   ??_7External@Object@@6B@   ord 1432
0x1801039bb  LEA RAX,[0x1801545e0]  MOV [RCX],RAX   ??_7MaterialInstance@…6B@  ord 1464
```

All three are exported and were verified against the shipped DLL, so the port resolves each **by
name** and writes the DLL's own array. It builds no table of its own and copies no slots.

**Why it is safe here:**

1. *The stamped value is the DLL's own table.* An object this ctor builds is byte-identical to one
   the DLL builds, so every virtual call — from the DLL's `Renderer::Draw`, from
   `MaterialInstance::RenderTextures`, from `Object::External`'s reflection machinery — reaches
   exactly the code it would have. This is the property the whole wave rests on ([WAVE6.md](WAVE6.md):
   safe "only while the port is byte-faithful on a shared object layout").
2. *The class is concrete and, in `Graphics`, leaf.* Slot 0 of `0x1801545e0` is a real
   scalar-deleting destructor, not `_purecall`, so the class is instantiated directly rather than
   only as a base. And no other exported `??_7` in the DLL names a class derived from
   `MaterialInstance` — the 20 `Graphics` vtables are `Material`, `Mesh`/`EdgeMesh` family,
   `Geometry` and its lights, the particle family and the image buffers, none of which is a
   `MaterialInstance`. So there is no derived constructor whose later stamp we could be racing,
   which is the failure mode a half-ported class hits.
3. *Both of our own stamping sites are unaffected, because they stamp AFTER construction.*
   `code/Game/Dialog/thumb_vtables.cpp:66-82` mirrors slots 1..8 out of whatever the ctor
   installed and plants its own slot 0; it reads the same table whether this port or the DLL wrote
   it. `code/Game/Component/statusicon_geometry.cpp:328` builds a **Geometry** vtable, not a
   MaterialInstance one, and this port does not touch it.
4. *The ctor allocates nothing and installs nothing global*, so a `BANISHED_RT_GFXRENDER=0` run and
   a default run produce the same object — which is what makes the kill switch meaningful.

**One caveat that runs the other way**, and it is a correction to PLAN §2.2 D — see §5.

---

## 5. Stale comments this lane disproved

Neither file is mine to edit, so both are quoted here with the correction, per PLAN §6 step 8.

### (a) `engine/funcs/engine_funcs.h:3172-3175` — the Camera ctor's third argument

> ```
> // Graphics::Camera::Camera(frustum, transform3, isPerspective) — non-default ctor.
> // RCX=self, RDX=Frustum*, R8=Transform3*, R9b=isPerspective bool.
> typedef void* (*fn_Graphics_Camera_Ctor_t)(void* self, void* frustum, void* transform3, bool isPerspective);
> ```

**`isPerspective` is wrong.** Perspective-vs-orthographic is carried by the `Volume::Frustum`
itself, at its `+0x60` (`Frustum::isOrthographic`, `runtime/types/volume/Volume.h:94`), and
`SetFrustum` never consults `Camera+0xd4` for it. What the flag actually selects is whether the
projection matrix gets post-multiplied by a fixed basis swap built inline on the stack at
`0x1800fe5f0..0x1800fe6a3`:

```
row0 ( 1, 0,  0, 0)
row1 ( 0, 0, -1, 0)      -1.0f from DAT_18016175c, bytes 00 00 80 bf in the shipped DLL
row2 ( 0, 1,  0, 0)
row3 ( 0, 0,  0, 1)
```

which, in `Math::Matrix`'s row-vector convention, maps `(x, y, z, w) -> (x, z, -y, w)` — a Z-up
world into Y-up clip space. **Suggested correction:** rename the parameter to `swapZUpToYUp` in
the typedef and replace the comment line with a pointer to `kZUpToYUp` in
`runtime/src/graphics/renderer.cpp`. The signature itself is correct and load-bearing (`R9b` is a
byte) — only the name and the comment are wrong. I did not edit it because
`engine/funcs/` is outside this lane's file list.

### (b) `docs/runtime_port/PLAN.md` §2.2 D — "the mirroring workaround was never necessary"

> "Two of the classes listed below — `Graphics::MaterialInstance` and `UI::SpriteSheet` — have
> exported vtables too, so their mirroring workaround was never necessary."

**Half right, and the wrong half is the one an agent would act on.** The vtable *is* exported
(`??_7MaterialInstance@Graphics@@6B@`, `0x1801545e0`, ord 1464) and this lane now stamps it by
name. But `code/Game/Dialog/thumb_vtables.cpp` is **not** mirroring because the address was
unavailable — it mirrors so it can plant a **different slot 0**, a deleting destructor that lets
the exe control the `MemFree` (`thumb_vtables.cpp:53-64`). Handing that file the exported vtable
address would not remove the mirror; it would break it. **Suggested correction:** amend that
sentence to "have exported vtables too, which removes the *availability* reason for mirroring —
but check each site's actual motive first: `thumb_vtables.cpp` mirrors to replace slot 0, not to
obtain the table." PLAN.md is explicitly outside this lane's file list.

### (c) Confirmed, not stale

`standalone/scene_watch.h:71` and `scene_watch.cpp:593` — "`Renderer::AddDrawItem` silently DROPS
an item" — is **correct**; the three drop conditions are enumerated in §2. `scene_watch.h:73`'s
reading of `renderer+0x38` as "groups used" is neither confirmed nor contradicted: no function
this lane ported touches that field, and `Camera.h` says so rather than repeating the claim as
fact.

---

## 6. Notes for the wave lead

* **`-fsyntax-only` is warning-clean** with the exact command from the lane brief
  (`-Wall -Wextra -Wno-cast-function-type`). `RtGraphicsRendererRegister()` returns **12**.
* **No detour is installed**, and no lane-F function needs one. Every ported body is reached
  through our own call sites; `Renderer::Draw` — the one DLL-internal caller that would matter —
  stays in the DLL, so the DLL's `Draw → AddDrawItem → MaterialInstance::RenderTextures` chain
  never crosses the seam. The classes are split (Renderer 4/6, MaterialInstance 3/5) but the split
  is safe because no ported body changes an allocation, a vtable or a field the unported half
  reads.
* **Ghidra was NOT renamed.** `rename_function_by_address` takes no `program=` parameter and
  operates on the *current* program, which is `Runtime-x64-profile.dll`; renaming would have
  required `switch_program`, which the lane rules forbid with six concurrent lanes. Every
  `decompile_function` / `disassemble_function` call this lane made passed
  `program="Runtime-x64.dll"`. Four retail internals worth naming when it is safe to do so:
  `0x1800f9dc0` → `Volume_Frustum_Projection` (it IS exported, ord 3435, so Ghidra should already
  show it), `0x18001c8c0` → `Graphics_Renderer_DrawGroupArray_Dtor`, `0x18001c7e0` →
  `Graphics_Renderer_SceneEntryArray_Dtor`, `0x180061480` → `Collection_Array_StringPair_SetSize`.
* **Runs to make:** `sa-run` with `BANISHED_RT_DIFF=1`, then the `BANISHED_RT_GFXRENDER=0` A/B.
  The twelve sites should all report non-zero `calls=` — `Camera::SetFrustum`,
  `Camera::SetCameraToWorld` and `Renderer::GetCullCamera`/`GetShader` are per-frame, and the
  three `MaterialInstance` sites fire during resource load. **Read the `calls=` column, not just
  `divergences=`**: a port with zero callers reports clean
  (`reference_uncalled_port_is_not_verified.md`).
* **One expected asymmetry in diff mode:** `Camera::SetFrustum` calls the video backend's
  "zero-to-one depth" query twice per call when diffing (once from ours, once from the DLL's
  replay). It is a getter — DX11's `DeviceData` slot 7 is the shipped `mov al,1; ret`,
  `dx11/src/devicedata_stubs.cpp:11` — so the extra call is free of side effects.
* **A finding worth carrying forward:** `Camera::SnapCameraToWorld` and `Camera::SetCameraToWorld`
  are instruction-for-instruction the same function in the shipped DLL. `Graphics::Camera` has no
  previous-transform field, so the distinction the two names draw does not exist in the binary.
  They are kept as two ports so the oracle checks each against its own export.
