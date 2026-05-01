# M3.5 — Geometry::Serialize findings (STOP, do not implement yet)

## TL;DR

`TradeWork_impl_04d0` and `TradeWork_impl_f1a0` (the two helpers MenuScene::Serialize calls
to deserialize the `MenuScene::geometry` at `+0x28`) are **not** simple wrappers around a
few engine_funcs primitives. They are the **terrain Geometry serializer** for a `Graphics::Geometry`
subclass that owns a heightmap, dynamic vertex/index buffers, multiple parallel arrays
of complex structs, an `ImageBuffer` with a custom vtable, and version-gated branches
spanning 6 schema versions.

The full scope (Geometry::Serialize body + ~10 distinct sub-helpers + indirect callees)
**well exceeds 500 instructions** of intricate code with multiple cascade points and
direct DirectX vertex-/index-buffer mutation. Forcing a clean reimplementation in one
agent pass is the wrong call. Per CLAUDE.md hard rules:

> If `TradeWork_impl_04d0` is enormous (>500 instructions) and depends on many cascading
> types we haven't even named yet, STOP and write what you've found into a new note
> `notes/M35_geom_serialize_findings.md` instead of forcing through with a bad
> implementation.

I have made **no** edits to `engine_funcs.h/.cpp`, `address_registry.cpp`, or any
`code/Game/*` file. The worktree is clean apart from this note.

## Confirmed addresses (recon was off — corrected)

| Recon label              | Actual exe address | Body range                         |
|--------------------------|--------------------|------------------------------------|
| `TradeWork_impl_04d0`    | `0x1400b04d0`      | `0x1400b04d0 – 0x1400b0bd0` (~0x700 bytes) |
| `TradeWork_impl_f1a0`    | `0x1400af1a0`      | `0x1400af1a0 – 0x1400af364` (~0x1c4 bytes) |
| `Geometry` ctor          | `0x1400b00e0`      | `0x1400b00e0 – 0x1400b0221`        |

The recon mention of `0x1400b0f1a0` was wrong — verified via disassembly of
`MenuScene::Serialize @ 0x1400a3630`:

```
1400a36c4: CALL 0x1400b00e0   ; Geometry ctor
1400a36d5: CALL 0x1400b04d0   ; TradeWork_impl_04d0  (Geometry::Serialize)
1400a36f0: CALL 0x1400af1a0   ; TradeWork_impl_f1a0  (version-gated extra)
```

## What `TradeWork_impl_04d0` (Geometry::Serialize) actually does

Control flow shape, from disassembly + decompilation of `0x1400b04d0`:

1. **Construct local serializer** with version 6 (latest schema): `IO::Serializer::Serializer(local_50, stream, 6)`.
2. **Serialize 6 ints + 1 packed dim** at `+0xC0..+0xDC` — heightmap dimensions, level/grid
   counts. The first int (`+0xC0`) is the master grid size, used everywhere.
3. **Serialize a uint16 array** at `+0xD8` (count = `+0xE0`) via `TradeWork_impl_sub_c890`
   (custom uint16-stride array resize) + `IO::Serializer::Serialize(serializer, count, ushort*, name)`.
4. **`FUN_140007140(self+0xF8, serializer)`** — typed `ExternalPtr` serialize at `+0xF8`.
   This is a clone of `FUN_14001b8e0`/`FUN_1400196a0` with its own `_typeInfo_exref` global.
   **CASCADE.** Calls `Object::ExternalFactory::FindOrLoad` + `IsDerivedFrom`. The exref
   type is whichever type the slot at `+0xF8` references (probably a noise/texture
   description; needs more work to confirm).
5. **Heightmap raw bytes** at `+0x140` — count = `(*+0xC0)^2 * 4` (4 bytes per cell). On
   load: `MemAlloc((*+0xC0)^2 * 4, 4)`, then `SerializeBytes`. On save: serialize from
   existing pointer. **Calls `TradeWork_impl_sub_c1e0`** which does the heavy lifting:
   - Allocates and creates the **`Video::VertexBuffer` at `+0x100`**.
   - Walks the grid, calls `FUN_1400ac880` per cell (computes vertex positions/normals/UVs
     using `g_fUVScale`, `DAT_140172214`, `DAT_140172264` constants and a sub-helper
     `TradeWork_helper_e270`).
   - Builds an index buffer (triangle list with checkerboard winding) and creates a
     **`Video::IndexBuffer` at `+0x120`**.
6. **Version-gated mesh-vert/idx buffer block** (versions 0-2 vs 3+):
   - Old format (v < 3): bool + uint count + raw bytes via `FUN_14001ef10` (16-byte stride
     resize) and `TradeWork_impl_sub_c890` (uint16 stride resize) for vert + idx buffers
     respectively.
   - New format (v ≥ 3): per-element loops over the 16-byte vertex struct (4 ints + 1 short
     + 4 bytes) and 2-byte index buffer.
7. **Two more typed-ExternalPtr serializes** at `+0x150` and `+0x158` via `FUN_140007140`.
   **CASCADE x2.** Same `_typeInfo_exref` type. Likely **terrain texture / noise
   `External` references** — need to confirm what `_typeInfo_exref` resolves to in the
   binary.
8. **Two parallel int4-array fields** at `+0x1C0` and `+0x1D0`. Resize via
   `TradeWork_impl_b260_sub_efd0` (16-byte stride), then per-element 4 floats. These
   look like axis-aligned bounding boxes or sphere centres + radius.
9. **Array of arrays at `+0x1E0`** (16-byte outer struct, count `+0x1E8`). Each outer
   element holds a count and a `Math::Vector2*` array. Outer resize via
   `TradeWork_impl_sub_f0a0` (calls `TradeWork_helper_ff00` per element to deep-clone the
   inner array). Inner resize via `FUN_1400203a0` (8-byte Vector2 stride). Each inner
   element serialized via `Math::Vector2::Serialize`. **Probably navmesh polygon contours
   or path strips.**
10. **Complex 0x60-byte struct array at `+0x200`** (count `+0x208`):
    - Resize: `TradeWork_impl_sub_f250` — non-trivial because each element has a
      copy-ctor (`TradeWork_impl_sub_f5e0`) and a sized destructor
      (`TradeWork_impl_sub_f250_sub_fb80`). When growing, calls copy-ctor; when shrinking,
      calls per-element destructor.
    - Per-element serialize: `TradeWork_impl_ffa0` walks each 0x60-byte element calling
      `FUN_140007140` (typed ExternalPtr — **another cascade**), 5 ints, 1 byte, 1 int,
      1 bool, 5 floats, 1 bool, optional bool (v ≥ 2), and a sub-helper
      `TradeWork_impl_sub_f9c0` with another uint16-stride array (count + array bytes).
    - **This is the most cascade-prone block** — every element has an ExternalPtr to load.
11. **Version 4+ array at `+0x220`** (4-byte stride int array, count `+0x228`). Resize via
    `FUN_1400021d0`, serialize as raw bytes.
12. **Version 5- backfill loop** (only if v < 5 and IsLoading): copies first 8 bytes of
    each `+0x200` element into the new `+0x210` array, zeroes `+0x2c..+0x38` in the source
    (looks like a migration step from old layout).
13. **Version 6+ array at `+0x210`** (count `+0x218`): another typed-ExternalPtr
    serialize via `TradeWork_impl_sub_0150`. **CASCADE.** This is a near-clone of
    `FUN_14001bcc0`/`FUN_14001bff0` with the same `_typeInfo_exref` global.
14. **Final IsLoading-only block**:
    - `Video::IndexBuffer::Create(self+0x180, idxCount*2, false, idxData)` — second
      index buffer.
    - `Video::VertexBuffer::Create(self+0x160, vertCount*16, false, vertData)` — second
      vertex buffer.
    - **`TradeWork_impl_sub_c780(self)`** — allocates a `Video::ImageBuffer` at `+0x148`,
      sets a custom vtable (`g_NoiseUI_ImageBufferVtable`), calls vtable +0x48 with format
      params, then `Video::Device::Lock` → `Video::ImageBuffer::CopyToTexture` →
      `Video::Device::Unlock`. **Heightmap → texture upload.**
    - Final fixup: if `+0x218` (post-load count) ≠ `+0x228`, resize `+0x210` to match.
15. `IO::Serializer::~Serializer(local_50)`.

## What `TradeWork_impl_f1a0` (version-gated extra serialize) actually does

Smaller body (~73 instructions). Constructs another local serializer with version 4
(different schema from the outer's v6 — yes, this is genuinely two parallel schema
streams), then:

1. Serialize array at `+0x1F0` (count `+0x1F8`) — same complex 0x60-byte struct as
   `+0x200`. Uses `TradeWork_impl_sub_f250` + `TradeWork_impl_ffa0`. **CASCADE.**
2. Serialize array at `+0x200` (count `+0x208`) — duplicate of step 10 above (this matters
   because the outer Geometry::Serialize also processes `+0x200`; this reads the OLD
   schema's separate slot before the new one). **CASCADE.**
3. Version 0-2 backfill into `+0x210` (mirror of step 12 above; zeros 0x2c..0x38 in the
   source structs).
4. Version 3+ array at `+0x210` (count `+0x218`) via `TradeWork_impl_sub_0150`. **CASCADE.**

**Both functions share the same set of helpers** (`TradeWork_impl_sub_f250`,
`TradeWork_impl_ffa0`, `TradeWork_impl_sub_0150`, `FUN_140001bb0`, `FUN_140007140`).
A clean reimpl needs all of them simultaneously.

## All sub-helpers (impossible to reimpl piecemeal)

Direct callees of `0x1400b04d0` and `0x1400af1a0`:

| Sub-helper                        | Address       | Approx size | Cascade? |
|-----------------------------------|---------------|-------------|----------|
| `TradeWork_impl_sub_c890`         | (resize uint16 stride) | ~30 instr | NO |
| `TradeWork_impl_sub_c1e0`         | (build mesh + buffers) | ~150 instr | NO (DX calls) |
| `FUN_140007140`                   | `0x140007140` | ~120 instr  | **YES** (FindOrLoad + IsDerivedFrom) |
| `FUN_14001ef10`                   | `0x14001ef10` | ~50 instr   | NO |
| `TradeWork_impl_b260_sub_efd0`    | (resize 16B stride)    | ~40 instr | NO |
| `TradeWork_impl_sub_f0a0`         | (resize array-of-arrays) | ~80 instr | NO |
| `TradeWork_impl_sub_f250`         | (resize 0x60B stride)  | ~80 instr | NO |
| `TradeWork_impl_ffa0`             | (per-element 0x60B serialize) | ~70 instr | **YES** (calls FUN_140007140 inside) |
| `FUN_1400021d0`                   | `0x1400021d0` | ~30 instr   | NO |
| `FUN_140001bb0`                   | `0x140001bb0` | ~35 instr   | NO |
| `TradeWork_impl_sub_0150`         | (typed ExternalPtr array) | ~150 instr | **YES** (FindOrLoad + IsDerivedFrom) |
| `TradeWork_impl_sub_c780`         | (image buffer + heightmap upload) | ~30 instr | NO (DX calls) |
| `Math::Vector2::Serialize`        | Runtime       | —           | NO |
| `FUN_1400ac880`                   | `0x1400ac880` | ~80 instr   | NO (uses `g_fUVScale`, `DAT_140172214`, `DAT_140172264`) |
| `TradeWork_helper_e270`           | (vertex pos compute) | ~40 instr | NO |
| `TradeWork_helper_ff00`           | (deep-clone inner array) | ~30 instr | NO |
| `TradeWork_impl_sub_f5e0`         | (0x60B copy ctor) | ~40 instr | NO |
| `TradeWork_impl_sub_f250_sub_fb80`| (per-element dtor loop) | ~30 instr | NO |
| `TradeWork_impl_sub_f9c0`         | (uint16 sub-array serialize) | ~30 instr | NO |
| `Video::VertexBuffer::Create`     | Runtime       | —           | NO |
| `Video::IndexBuffer::Create`      | Runtime       | —           | NO |
| `Video::ImageBuffer::ImageBuffer` | Runtime       | —           | NO |
| `Video::ImageBuffer::CopyToTexture`| Runtime      | —           | NO |
| `Video::Device::Lock`/`Unlock`    | Runtime       | —           | NO |

**Total clean-reimpl surface: well over 1000 instructions.** Easily a multi-session task.

## Cascade risk — types we don't have

The four cascade points (`FUN_140007140` x3 callsites + `TradeWork_impl_sub_0150` x2 +
`TradeWork_impl_ffa0` interior) all reference the same `_typeInfo_exref` global, which
points to a `BaseTypeInfo` somewhere. The actual type still needs identification — best
guess from context (terrain Geometry with mesh references, image buffers, navmesh polys)
is some form of:

- **`Graphics::GraphicsMesh`** (likely; that's what MenuScene's `+0x70` already references)
- **`Graphics::Texture` / `Video::Texture`** (heightmap-derived textures)
- **`Volume::Mesh`** (collision mesh / navmesh)

Confirming requires: read the `_typeInfo_exref` symbol address out of Ghidra (search
`globals` for that exact name), follow it to a `BaseTypeInfo` struct, read offset +0x08
for the type name string. Worth doing as a follow-up.

## REGISTER_TYPE candidates for the cascade fix

When this work resumes, expect these to need standalone scaffolding (Factory + vtable +
GetTypeId + REGISTER_TYPE) before MenuScene can deserialize cleanly:

1. **The `_typeInfo_exref` global type** — once identified. May be GraphicsMesh
   (already partially handled in MeshInstance::SetMesh path) or a sibling type.
2. **Whatever type lives at `+0x150` / `+0x158`** — likely the same as `_typeInfo_exref`.
3. **The 0x60-byte struct elements** — these embed a typed ExternalPtr at `+0x00` (via
   `FUN_140007140` inside `TradeWork_impl_ffa0`). If these are `Vegetation`/`Tree` style
   externals, they will cascade further. Need to identify the struct shape first.
4. **`Video::ImageBuffer` with `g_NoiseUI_ImageBufferVtable`** — if standalone doesn't
   know this vtable, the heightmap-to-texture upload will crash. Check whether
   `g_NoiseUI_ImageBufferVtable` is exe-resident or Runtime-resident.

## Recommended path forward (multi-session plan)

If we want visible 3D content in the menu without doing all of the above:

### Option A — short-circuit Geometry creation (1-2 sessions)

Replace `Geometry::Serialize` with a stub that:
1. Allocates a `Geometry` via `MemAlloc(0x230, 8)` + `FUN_1400b00e0`-style ctor (already
   exists in standalone — the recon flagged it as `RegisterExeAddr`-able).
2. Populates only the fields needed for the 3D-pipeline-renders-something test:
   `+0xC0` = grid size, `+0xCC` = LOD, `+0xD8` = heightmap allocated to zeros,
   `+0x100` = `Video::VertexBuffer::Create` with hard-coded quad,
   `+0x120` = `Video::IndexBuffer::Create` with hard-coded triangle indices.
3. Skip everything past `+0x150` (lazy-fail typed ExternalPtrs by leaving them null).

Result: a flat tri or quad renders against the menu fog. Not the real scene, but
visible 3D content satisfies M3.5's minimum viable definition.

### Option B — incremental clean reimpl (5-8 sessions)

1. Session 1: confirm `_typeInfo_exref` global → identify the cascading type. Add
   `REGISTER_TYPE` for it.
2. Session 2: implement `TradeWork_impl_sub_f250` + `TradeWork_impl_sub_f5e0` +
   `TradeWork_impl_sub_f250_sub_fb80` (the 0x60B struct ctor/copy-ctor/dtor — needed for
   array growth in both Geometry::Serialize and the version-gated extra).
3. Session 3: implement `FUN_140007140` (the typed ExternalPtr serialize). Call sites
   already agreed in recon — this is the "agent: array-helpers" companion task.
4. Session 4: implement `TradeWork_impl_ffa0` + `TradeWork_impl_sub_0150` (per-element
   serializers).
5. Session 5: implement the smaller resize helpers (`_c890`, `_efd0`, `_f0a0`, `_ef10`,
   `_001bb0`, `_0021d0`).
6. Session 6: implement the heightmap mesh-build path (`_c1e0` + `1400ac880` + helpers).
7. Session 7: implement the image-buffer heightmap upload (`_c780` + custom vtable
   handling).
8. Session 8: assemble everything into `Geometry::Serialize` body.

### Option C — punt and skip MenuScene 3D entirely (zero sessions)

Keep the current standalone Slot5 no-op behaviour. Move M3.5 to "deferred" and pivot to
M4 (other menu sub-dialogs).

## What I did NOT do

- **No edits** to `engine_funcs.h`, `engine_funcs.cpp`, `address_registry.cpp`, or any
  `code/Game/*.cpp/.h` file.
- **No `code/Game/Game/geometry_serialize.cpp` created** — the scope is too large for a
  blind one-pass implementation, and partial implementations would silently corrupt
  scene data.
- The worktree contains only this notes file (`notes/M35_geom_serialize_findings.md`).

## Open questions for the user

1. **Which option (A, B, C) do you want to pursue?**
2. **Would you like me to identify `_typeInfo_exref` first** as a cheap follow-up before
   committing to a path? That would tell us whether the cascade is one type or many.
3. The recon doc mentioned an "agent: array-helpers" doing the small resize/serialize
   helpers in parallel — is that agent still in flight, or paused? If still in flight,
   their work would unblock part of Option B Session 2-5.
