# Worldgen Terrain Build — Implementation Blueprint (Standalone EXE)

Scope: the in-game procedural terrain build that runs on the New-Game branch of
`GameUpdateLoop::Load`. Entry = `FUN_1400b0230` (dispatcher) → `TradeWork_impl_c4b0`
(orchestrator) → `ConsumeProduceComponent_impl_f390` (heightmap pipeline) +
mesh/texture builders. Produces a fully-built `WorldGeometry` (int16 heightmap,
base + detail VB/IB, splat texture, 3 material refs) ready to render.

This blueprint merges recon from 5 agents and is reconciled against live Ghidra
disassembly (program `Application-x64.exe`). Disassembly-verified facts override
agent claims wherever they conflicted; those overrides are called out inline.

---

## 0. VERIFIED ORCHESTRATION (ground truth)

`FUN_1400b0230(WorldGeometry* wg /*RCX*/, TerrainGenerator* gen /*RDX*/, int sizeIdx /*R8D*/, uint seed /*R9D*/)`
→ writes grid dims into `wg`, resolves 3 materials (`fd10` ×3), allocates+zeros the
heightfield scratch struct on the stack, seeds its embedded `System::Random` with
`seed`, wires `+0xa20=gen` / `+0xa28=sizeRow`, calls `c4b0`, then frees 4 scratch lists.

**VERIFIED: the heightfield scratch base passed to `c4b0` is `local_a58`** (Ghidra:
`TradeWork_impl_c4b0(param_1, local_a58)`), NOT `local_a78`. `local_a78`/`local_a70`/
`local_a68` are the temporary `{String,int,External*}` lazy-ref used by the 3 material
resolves. This fixes the struct offset mapping (see §1).

`TradeWork_impl_c4b0(WorldGeometry* wg /*RCX*/, int* hf /*RDX*/)` — VERIFIED pipeline:
```
sub_c890(wg+0xd8, N*N)                 ; N = wg[+0xd0] = dim*2+1 ; resize int16 heightmap  [ALREADY REIMPL]
f390(hf)                               ; full float-heightmap pipeline (fills *(float**)(hf+0x08))
for y,x in [0,N): wg.hm16[N*y+x] = (short)(int)(fmap[N*y+x] * 256.0)   ; DAT_140172230=256.0
wg[+0x140] = cf440(hf)                 ; splat/blend RGBA texture  [NEEDS REIMPL]
sub_c780(wg)                           ; height-texture upload  [ALREADY REIMPL]
sub_c1e0(wg)                           ; base grid mesh → VB+0x100 / IB+0x120  [ALREADY REIMPL]
b260(wg, hf)                           ; detail/cliff mesh → VB+0x160 / IB+0x180  [NEEDS REIMPL, DEFERRABLE]
sub_f0a0(wg+0x1e0, hf[8] /*+0x20*/)    ; resize detail-group-C  [ALREADY REIMPL]
; append per-group feature records (hf+0x18 patchList) into wg+0x1e0 vectors,
;   reading 0x1c-stride records but copying ONLY the leading 8 bytes (a ptr) per record, backward.
```

`f390(int* hf)` — VERIFIED order (overrides any agent ordering guesses):
```
hf[0] = sizeRow[0] * 2                  ; gridSize ; sizeRow = *(int**)(hf+0xa28)
gworld+0x24 = 1 ; e5d0(hf)              ; base perlin heightmap
gworld+0x24 = 2 ; cd090(hf)            ; coastline ring
                  cd150(hf)            ; mountain ranges   (NOTE: cd150 BEFORE ccf60)
                  ccf60(hf)            ; secondary paths
gworld+0x24 = 3 ; cd460(hf)            ; river network
                  e910(hf)             ; feature stamp (build pass)
gworld+0x24 = 4 ; cc080(hf)            ; droplet erosion
gworld+0x24 = 5 ; ec50(hf)             ; feature stamp (finalize/carve)
                  c350(hf)             ; weighted 3x3 box-blur smoothing  ← agents UNDER-SPEC'd
                  c350(hf)             ; second smoothing pass
```
`gworld+0x24` is the loading-progress counter (`g_pGameWorldSingleton+0x24`), cosmetic.

**Correction vs recon:** the two trailing `c350` calls are a real, load-bearing
**weighted 3×3 box-blur smoothing pass** over the float heightmap (verified by decomp of
`ConsumeProduceComponent_impl_c350` @0x1400cc350): center cell weighted `DAT_140172174=2.0`,
each of the 4 orthogonal neighbours weighted `g_fUVScale=1.0`, output = Σ(w·h)/Σw, written
to a temp buffer then copied back. Edge cells clamp indices to `[0,dim]`. Run twice. They are
needed for the final smooth look and MUST be in the pipeline (see §5).

---

## 1. STRUCT LAYOUTS

### 1.1 `Heightfield` scratch struct (~0xA30 bytes, stack-allocated in `b0230`)

Base = `local_a58`. Init by `b0230`: zero `+0x00..+0x50`, construct
`System::Random` at `+0x58` seeded with `seed`, set `+0xa20=gen`, `+0xa28=sizeRow`.
Teardown (reverse): `MemFree(+0x48)`, `MemFree(+0x38)`, `MemFree(+0x28)`,
`FreePatchListVector(+0x18)`, `MemFree(+0x08)`.

Offset map (disassembly-reconciled; the `local_aXX` stack names map to these via
`base=local_a58`):

| Offset | Field | Type / meaning | Written by | Read by |
|--------|-------|----------------|-----------|---------|
| +0x00 | `gridSize` | int = `sizeRow[0]*2` (== dim) | f390 | e5d0/cf440/cf150/cc080/c350 as NxN bound; e5d0 uses `dim+1` edge |
| +0x04 | `_pad04` | int (zeroed) | b0230 | — |
| +0x08 | `heightmap` | `float*` `(dim+1)^2` floats | e5d0 (`sub_3a00` resize) | c4b0 quantize, cf440, cf150, cc080, c350, e910, ec50 |
| +0x10 | `_f10` | u64 zeroed; freed LAST alongside +0x08 — likely secondary/sizing pair of heightmap | b0230 zero | open (see §7) |
| +0x18 | `patchList` | `{void* data; int count; int _pad;}` — vector of `{void* recs(0x1c stride), int count}` 16B elems | cd460 / feature stages | c4b0 final append loop; freed by `FreePatchListVector` |
| +0x20 | `detailCountA` | int — count passed to `f0a0` (resize wg+0x1e0) | feature stages | c4b0 (`param_2[8]`), b260 (`+0x20`) |
| +0x24 | `_pad24` | int | — | — |
| +0x28 | `listB` | `{void* data; int count; int cap; int _p;}` (river-point vector; element 0xC `{x,y,radius}`) | cd460, ccf60 | e910/ec50; `MemFree` in teardown |
| +0x38 | `listC` | `{void* data; int count; int cap; int _p;}` (mountain satellites+centers; element 0xC) | cd150 | e910/ec50 secondary-pt pass; `MemFree` |
| +0x48 | `detailRecs` | `{void* data; int count; int cap; int _p;}` (cliff polygons; element 0xC `{x,z,height}`) | feature stages | b260 pass-2 (`+0x48`/`+0x50`); `MemFree` |
| +0x58 | `random` | `System::Random` (~0x9C8 bytes) seeded with `seed` | b0230 | all RNG draws |
| +0xa20 | `terrainGen` | `TerrainGenerator*` back-ptr (`gen`) | b0230 | — |
| +0xa28 | `sizeRow` | `int*` = `gen->sizeTable + sizeIdx*0x44` | b0230 | f390 (`sizeRow[0]=dim`), cd150/ccf60/cc080/cd460 |

> Note the recon described "two parallel point arrays at +0x38/+0x40 and +0x48/+0x50":
> those are the `{data,count}` halves of the `listC` (+0x38) and `detailRecs` (+0x48)
> headers. The `+0x40`/`+0x50` are the `count` ints of those same 16-byte headers.
> Reconciled to single header structs above.

C struct sketch (implementer adds `static_assert(offsetof(...))` per field):
```cpp
struct Heightfield {           // sizeof ~= 0xA30
    /*+0x00*/ int    gridSize;     // dim, == sizeRow[0]*2  (set by f390)
    /*+0x04*/ int    _pad04;
    /*+0x08*/ struct { void* data; int count; int cap; } heightmap; // float[(dim+1)^2]
    /*+0x14*/ int    _pad14;       // (heightmap header is 0x08..0x13; +0x10 is the cap)
    // --- correction: the {data,count,cap} header occupies +0x08(data)/+0x10(count)/+0x14(cap)? ---
    // VERIFY at impl: c4b0 reads *(float**)(hf+0x08) as data. sub_3a00 operates on a
    // {data@+0, count@+8, cap@+0xC} 16-byte header at hf+0x08, so:
    //   +0x08 data, +0x10 count, +0x14 cap  (the "+0x10 _f10 freed last" is this header's count slot;
    //   the teardown MemFree(+0x08) frees data). Treat +0x08..+0x17 as the heightmap array header.
    /*+0x18*/ struct { void* data; int count; int _pad; } patchList;
    /*+0x20*/ int    detailCountA;
    /*+0x24*/ int    _pad24;
    /*+0x28*/ struct { void* data; int count; int cap; int _p; } listB;       // river points (0xC elem)
    /*+0x38*/ struct { void* data; int count; int cap; int _p; } listC;       // mtn satellites (0xC elem)
    /*+0x48*/ struct { void* data; int count; int cap; int _p; } detailRecs;  // cliff polys (0xC elem)
    /*+0x58*/ /* System::Random random; ~0x9C8 bytes */  unsigned char random[0x9C8];
    /*+0xa20*/ void*  terrainGen;   // TerrainGenerator*
    /*+0xa28*/ int*   sizeRow;
};
```

> **OPEN (verify at impl):** the exact split of the heightmap header at +0x08
> (data/+0x08, count/+0x10, cap/+0x14) vs the recon's claim that +0x10 is a separate
> "secondary buffer". The teardown does `MemFree(local_a50 /*+0x08 data*/)` LAST, and
> `sub_3a00` (the resize) treats hf+0x08 as a standard 3-field `{data,count,cap}` header.
> So +0x10 is almost certainly the `count` field of the heightmap header, not an
> independent buffer. Pin this by single-stepping `sub_3a00(&hf+0x08, (dim+1)^2)` once.

### 1.2 `SizeRow` (terrain size-table row, 0x44 bytes, indexed by `sizeIdx`)

Base = `gen->sizeTable + sizeIdx*0x44`, `gen->sizeTable = *(int**)(gen+0x80)`.
Field offsets are confirmed; literal per-map-size VALUES live in the `.rsc`
TerrainGenerator descriptor (NOT a static global) — must be runtime-dumped or read
from ToolKit resources (see §7).

| Off | Field | Consumer |
|-----|-------|----------|
| +0x00 | `dim` (full grid; center = dim/2-2) | f390/cd460/cd150/cc080 |
| +0x04 | `count2` (river edge-set 2 count) | cd460 |
| +0x08 | `min2` | cd460 |
| +0x0c | `max2` | cd460 |
| +0x10 | `count1` (river edge-set 1 count) | cd460 |
| +0x14 | `min1` | cd460 |
| +0x18 | `max1` | cd460 |
| +0x20 | `numSecondaryPaths` | ccf60 |
| +0x24 | `erodeIterations` | cc080 |
| +0x28 | `minRangeCount` | cd150 |
| +0x2c | `maxRangeCount` | cd150 |
| +0x30 | `minBandX` | cd150 |
| +0x34 | `maxBandX` | cd150 |
| +0x38 | `satellitesPerRange` | cd150 |
| +0x3c | `radiusMax` | cd150 |
| +0x40 | `radiusMin` | cd150 |

### 1.3 `WorldGeometry` (Graphics::Geometry subclass, ~0x230 bytes)

Fields touched by the build (all VERIFIED from b0230 / c4b0 / sub_c1e0 already-reimpl):

| Off | Field | Set by |
|-----|-------|--------|
| +0xc0 | `int dimX2` = dim*2 | b0230 |
| +0xc4 | `int dim` | b0230 |
| +0xc8 | `int baseSubdiv` = 32 | b0230 |
| +0xcc | `int baseEdge` = 33 (=baseSubdiv+1) | b0230 |
| +0xd0 | `int heightEdge` = dim*2+1 (= N for the int16 heightmap NxN) | b0230 |
| +0xd4 | `int baseVertCount` = 33*33 = 1089 | b0230 |
| +0xd8 | `short* heightmap` (heightEdge² shorts, fixed-point ×256) | sub_c890 resize + c4b0 quantize |
| +0xf8 | `External* material1` | b0230 (fd10 from gen+0x38) |
| +0x100 | `Video::VertexBuffer baseVB` (0x20 bytes) | sub_c1e0 |
| +0x120 | `Video::IndexBuffer baseIB` (0x20 bytes) | sub_c1e0 |
| +0x140 | `void* blendTexture` (RGBA, dim×dim) | c4b0 = cf440 result |
| +0x148 | height-texture upload state | sub_c780 |
| +0x150 | `External* material2` | b0230 (fd10 from gen+0x50) |
| +0x158 | `External* material3` | b0230 (fd10 from gen+0x68) |
| +0x160 | `Video::VertexBuffer detailVB` | b260 |
| +0x180 | `Video::IndexBuffer detailIB` | b260 |
| +0x1a0 | `void* detailVtxStage` / +0x1a8 count / +0x1ac cap (16B/vtx) | b260 |
| +0x1b0 | `void* detailIdxStage` / +0x1b8 count / +0x1bc cap (2B/idx) | b260 |
| +0x1c0 | `void* detailGroupA` / +0x1c8 count (16B elems) | b260 (efd0 resize) |
| +0x1d0 | `void* detailGroupB` / +0x1d8 count | b260 (efd0 resize) |
| +0x1e0 | `void* detailGroupC` / +0x1e8 count (`{data,size,cap}`) | f0a0 resize + c4b0 append |

`TerrainGenerator` material triple layout (input to fd10): each is
`{System::String name @+0; int cookie @+0x08; External* cache @+0x10}` — triples at
`gen+0x38`, `gen+0x50`, `gen+0x68`. (`engine/types/TerrainGenerator.h` already exists;
cross-check these offsets there.)

---

## 2. CONSTANTS TABLE (merged, .rdata `float` unless noted)

All addresses verified to exist; spot-verified `0x140172230=256.0`, `0x140172150=1.1`.

| Addr | Raw | Float | Used by / role |
|------|-----|-------|----------------|
| 0x1401720d8 | 3d4cccd0 | 0.05 | (erosion neighbour const, not read by the 4 we ship) |
| 0x1401720dc | 3d800000 | 0.0625 (1/16) | cc080 slope→transfer factor |
| 0x1401720e4 | 3dcccccd | 0.1 | e5d0 octave-2 coord frequency |
| 0x1401720ec | 3e000000 | 0.125 | cc850 closed-ring midpoint jitter |
| 0x1401720f0 | 3e189374 | ~0.14904 | cf440 G band divisor |
| 0x140172100 | 3e800000 | 0.25 | e5d0 pn2 persistence; cf440/fb00 spline; +0.25 noise bias; cc850/cc510 amplitude seed |
| 0x140172114 | 3f000000 | 0.5 | e5d0 pn1 persistence; cc510 HALF; cd150 scale base; cf440 R remap; fb00 inset; c350-adjacent |
| 0x14017211c | 3f2a7efa | 0.66602 (~2/3) | cc510 midpoint displacement amplitude factor |
| 0x140172128 | 3f000000 | 0.5 | cc080 max transfer fraction clamp |
| 0x140172130 | 3f59999a | ~0.85 | cf440 G lower band |
| 0x140172144 | 3f7d70a4 | 0.99 | eee0 radius-growth threshold |
| 0x140172148 | 3f7fbe77 | ~0.99899 | cf440 G upper clamp |
| 0x14017214c | 3f800000 | 1.0 | `g_fUVScale` — blend numerator, intersection (0,1) bound, c350 neighbour weight |
| 0x140172150 | 3f8ccccd | 1.1 | e5d0 powf base (radial falloff) |
| 0x140172154 | 3f99999a | 1.2 | cd460 labeling spatial-join threshold |
| 0x140172158 | 3fa00000 | 1.25 | cd090/ccf60 PathPoint param |
| 0x140172160 | 3fc00000 | 1.5 | ccf60 param_4 & param_7 |
| 0x140172164 | 3faa3d71 | 1.33 | e910 river-point powf exponent |
| 0x140172170 | 3fe38e39 | ~1.777 | (near cf150; context) |
| 0x140172174 | 40000000 | 2.0 | clampFloor; cd090 PathPoint; e5d0/cf150 dist mul; e910/ec50 sec-pt exp; **c350 center weight**; b260 cliff box pad |
| 0x14017217c | 3fe38e39 | 1.778 | e910/ec50 secondary-pt strength/scale |
| 0x140172180 | 40400000 | 3.0 | cf440 height-band divisor |
| 0x140172194 | 40800000 | 4.0 | cd090 PathPoint; fb00 subdivision/spline; cf440 perlin persistence |
| 0x140172198 | 40a00000 | 5.0 | `g_pAgeMultiplier` — e5d0 PerlinNoise octaves/amp (both octaves) |
| 0x1401721a0 | 40e00000 | 7.0 | cf440 secondary height threshold |
| 0x1401721a8 | 40f00000 | 7.5 | cf440 G high height threshold |
| 0x1401721a8* | 41000000 | 8.0 | ccf60 param_5; e5d0 base scale `(perlin1+1)*8` — **CONFLICT, see note** |
| 0x1401721b0 | 41200000 | 10.0 | cd090 param_5 |
| 0x1401721c0 | 41800000 | 16.0 | e5d0 falloff dist bias `dim*2+16`; ccf60 param_8 |
| 0x1401721e4 | 42400000 | 48.0 | e5d0 `(perlin2+0.25)*48`; b260 vertex XZ scale |
| 0x140172224 | 433f0000 | 191.0 | b260 pass-2 vertex Y base; cf440 AO row offset |
| 0x14017222c | 437f0000 | 255.0 | cf440 color scale; b260 normal/uv blend byte scale |
| 0x140172230 | 43800000 | **256.0** | c4b0 heightmap float→int16 fixed-point scale |
| 0x140172250 | 44800000 | 1024.0 | b260 pass-1 uv scale |
| 0x140172264 | 46800000 | 16384.0 | (height scale, mesh builders) |
| 0x140172268 | 49742400 | 1000000.0 | cc850/cd460 nearest-neighbour init (∞) |
| 0x140172270 | bb800000 | -0.00390625 (-1/256) | (known) |
| 0x140172278 | bf000000 | -0.5 | cc850 ring first-point Y; cf440 R height threshold |
| 0x14017227c | bf400000 | -0.75 | b260 below-water/flat reject |
| 0x1401722a4 | c3400000 | -192.0 | b260 pass-1 vertex Y base (`-192 - segIdx`) |
| 0x1401722c0 | 80000000 | (sign-bit) | XOR float-negate mask (e5d0/cf150/cc510/eee0/fb00) |
| 0x1401720bc | 3b800000 | 0.00390625 (1/256) | b260 int16→float height decode |

> **CONFLICT at `0x1401721a8`**: the gen-helpers agent read it as 8.0 (`41000000`),
> the cf440 agent read it as 7.5 (`40f00000`). These are DIFFERENT addresses being
> conflated. e5d0's `(perlin1+1)*8` term uses **0x1401721a8 = 8.0**; cf440's G high
> threshold = 7.5 is a NEARBY but distinct constant. Implementer: `read_memory`
> `0x1401721a8` once and treat `(perlin1+1)*N` per the actual dword (recon majority = 8.0).

---

## 3. RUNTIME EXPORTS NEEDED

Legend: **[HAVE]** already in `engine/funcs/engine_funcs.{h,cpp}`; **[NEW]** must add.

| Demangled | Mangled (verify vs DLL exports) | Status |
|-----------|--------------------------------|--------|
| `Utility::PerlinNoise::PerlinNoise(float,float,int,int)` | `??0PerlinNoise@Utility@@QEAA@MMHH@Z` | **[HAVE]** `fn_PerlinNoise_Ctor` |
| `Utility::PerlinNoise::Generate(float,float) const` | `?Generate@PerlinNoise@Utility@@QEBAMMM@Z` | **[HAVE]** `fn_PerlinNoise_Generate` |
| `System::Random::RandTol(int)` | `?RandTol@Random@System@@QEAAHH@Z` | **[HAVE]** `fn_Random_RandTol` |
| `System::Random::Random(uint)` (seeded ctor) | `??0Random@System@@QEAA@I@Z` | **[HAVE]** used by `gameupdateloop_load.cpp` |
| `System::Random::Rand(int max)` | `?Rand@Random@System@@QEAAHH@Z` | **[NEW]** (single-arg int overload) |
| `System::Random::Rand(int min,int max)` | `?Rand@Random@System@@QEAAHHH@Z` | **[NEW]** (cc850 2-arg) |
| `System::Random::RandFloat()` | `?RandFloat@Random@System@@QEAAMXZ` | **[NEW]** |
| `System::Random::RandFloatTol()` | `?RandFloatTol@Random@System@@QEAAMXZ` | **[NEW]** |
| `Math::Vector2::Normalize()` | `?Normalize@Vector2@Math@@QEAAAEAV12@XZ` | **[NEW]** (verify ABI) |
| `Math::Vector2::Length() const` | `?Length@Vector2@Math@@QEBAMXZ` | **[NEW]** |
| `Math::Vector2::LengthSquared() const` | `?LengthSquared@Vector2@Math@@QEBAMXZ` | **[NEW]** |
| `Math::Vector2::Dot(Vector2 const&)` | `?Dot@Vector2@Math@@...` | **[NEW]** |
| `Math::Vector2::Lerp(...)` | `?Lerp@Vector2@Math@@...` | **[NEW]** (fb00 only) |
| `Math::Vector3::Dot / Length / Normalize / ctors` | `?Dot@Vector3@Math@@...` etc | **[NEW]** (cf440/b260) — check if a Vector3 helper already wired |
| `Math::Scalar::Max / Lerp / ...` | `?Max@Scalar@Math@@...` | **[NEW]** (cf440 AO, fb00) |
| `Object::ExternalFactory` singleton + `::Find` + `::IsDerivedFrom` | (see below) | **[HAVE]** `g_pExternalFactorySingleton`, `fn_ExternalFactory_Find`, `fn_IsDerivedFrom` |
| `System::Filename::Filename(String const&)` / `~Filename` | `??0Filename@System@@...` | **[NEW]** (fd10) — verify if wired |
| `System::String::String(String const&)` / `~String` | `??0String@System@@...` | **[HAVE]** (used widely) |
| `System::MemoryAllocator::MemAlloc/MemFree` | — | **[HAVE]** `fn_MemAlloc` / `fn_MemFreeStatic` |
| `Video::VertexBuffer::Create(int,bool,void*)` | — | **[HAVE]** (sub_c1e0 already calls it) |
| `Video::IndexBuffer::Create(int,bool,void*)` | — | **[HAVE]** (sub_c1e0 already calls it) |

PerlinNoise ctor ABI (VERIFIED in e5d0 disasm): `RCX=self, XMM1=persistence,
XMM2=octaves(5.0), R9D=seed1(2nd RandTol), [RSP+0x20]=seed2(1st RandTol stack arg)`.

`powf`/`sqrtf` (`sub_1460`@0x140131460, `FUN_140132010`@0x140132010) are the **CRT
libm routines** — do NOT reimplement internals; call `std::powf`/`std::sqrtf`. (Verified:
`sub_1460` references the `"powf"` `_matherr` string @0x14017cbc8; worldgen never feeds
inf/NaN, so the special-case branches are dead. Port verbatim ONLY if save-game
bit-exactness vs original is required — see §7.)

---

## 4. ALREADY-REIMPLEMENTED HELPERS TO REUSE

In `code/Game/Game/serialize_*.cpp` (verified present):

| Helper | Addr | What it does | Where |
|--------|------|--------------|-------|
| `Reimpl_TradeWork_sub_c1e0` / `_Body` | 0x1400ac1e0 | **Base ground grid mesh** → VB+0x100 / IB+0x120 (32×32 chunked grid, reads int16 heightmap +0xd8) | `serialize_heightmap_mesh.cpp` |
| `Reimpl_TradeWork_sub_c780` | 0x1400ac780 | Height-texture upload (+0x148) | `serialize_image_upload.cpp` |
| `Reimpl_Array_Resize_Stride02` | 0x14001c890 | int16 array resize — used by c4b0 to size +0xd8 heightmap (sub_c890) | `serialize_resize_helpers.cpp` |
| `Reimpl_Array_Resize_Stride16A` | 0x14001efd0 | 16B/align4 resize — b260 descriptor arrays (efd0) | `serialize_resize_helpers.cpp` |
| `Reimpl_Array_Resize_Stride16B` | — | 16B/align2 resize — sub_c1e0 vertex scratch | `serialize_resize_helpers.cpp` |
| `Reimpl_Array_Resize_Stride04` | 0x140023a00 | **4B array resize = `sub_3a00`** — sizes the float heightmap & cf440/e910/ec50/c350 temp masks | `serialize_resize_helpers.cpp` |
| `Reimpl_Array_Resize_ArrayOfArrays` | 0x14001f0a0 | array-of-arrays resize (sub_f0a0) — c4b0 detail-group-C | `serialize_resize_helpers.cpp` |
| `GenericArrayResize` | — | shared stride-param resize core (the `{data,count,cap}` grow-3x idiom) — **reuse for any new stride (0xC, 0x18, 0x1C)** | `serialize_resize_helpers.cpp` |

Engine plumbing already wired: `fn_PerlinNoise_Ctor/Generate`, `fn_Random_RandTol`,
seeded `Random` ctor, `g_pExternalFactorySingleton`, `fn_ExternalFactory_Find`,
`fn_IsDerivedFrom`, `fn_MemAlloc`, `fn_MemFreeStatic`, `Video::VertexBuffer/IndexBuffer::Create`.

> **Net effect:** the entire base-mesh + texture-upload + every array-resize leaf the
> pipeline needs is ALREADY DONE. The remaining work is the float-heightmap math
> (e5d0 + feature stages), the blend texture (cf440), the dispatcher/orchestrator glue
> (b0230/c4b0/fd10), and (deferrable) the detail mesh (b260).

---

## 5. NEEDS-REIMPL FUNCTIONS (dependency-layered build list, leaves first)

### Layer 0 — trivial leaves / CRT (do first, ~1 hr)
- **`sub_3a00` (4B resize)** @0x140023a00 — ALREADY DONE (Stride04). Add a generic
  0xC-stride and 0x18-stride variant via `GenericArrayResize` for cd150/cf440 (1-line each).
- **`FUN_140132010` = sqrtf**, **`sub_1460` = powf** — map to `std::sqrtf` / `std::powf`.
- **`FreePatchListVector` (FUN_1400175d0)** @0x1400175d0 — free each 16B `{ptr,size}` elem's
  inner ptr, then the outer array; null the header. Sub-calls: MemFree×(n+1).
- **`FUN_1400245d0`** @0x1400245d0 — 0xC-stride array append (push one `{x,y,w}`). Sub-calls:
  MemAlloc/MemFree. (= GenericArrayResize stride 0xC + tail store.)
- **`sub_74a0`** @0x1400074a0 — 16B array append with external src elem (b260). MemAlloc/MemFree.

### Layer 1 — heightmap base + helpers (the visible-terrain core)
- **`e5d0` GenerateBaseHeightmap** @0x1400ce5d0 — fills `hf+0x08` float heightmap:
  per cell `h = (1-w)*base + h*w`, `w = 1/(curve+1)`, `curve = powf(1.1, -(dist - (dim*2+16)))`,
  `dist = sqrtf(dx²+dz²)`, `base = (pn1+1)*8`, `h = (pn2+0.25)*48 + pn1`, clamp h≥2.
  Two PerlinNoise octaves (persist 0.5 / 0.25, octaves 5.0), seeded by 4× `RandTol(0xffffff)`.
  Sub-calls: `sub_3a00`, `RandTol`, `PerlinNoise::ctor`×2, `PerlinNoise::Generate`×2, sqrtf, powf.
  **ABI verified** (§3). This alone produces a renderable island.
- **`cd090` EmitCoastlineRing** @0x1400cd090 — `while(pathCount==0) cc850(hf, gridSize+1,
  (float)gridSize, 2.0,10.0,1.25,4.0,4.0, -1)`. One closed ring. Sub-calls: `cc850`.
- **`cd150` EmitMountainRanges** @0x1400cd150 — scatter satellite peaks + centers into
  listC(+0x38) + detailRecs(+0x48). Sub-calls: `Rand`, `RandFloat`, `RandFloatTol`,
  `Vector2::Normalize`, `FUN_1400245d0` (0xC append).
- **`ccf60` EmitSecondaryPaths** @0x1400ccf60 — `numSecondaryPaths` branch paths, each up
  to 1000 retries of `cc850(..., branchSel=Rand(pathCount))`. Sub-calls: `Rand`, `cc850`.

### Layer 2 — path primitives (consumed by Layer-1 path emitters)
- **`cc510` SubdivideSegment** @0x1400cc510 — recursive midpoint displacement (fractal
  wave) with half-plane reject sampler. Sub-calls: `Vector2::Length/Normalize`, `RandFloatTol`,
  self×2 (amplitude ×0.5). **Transcribe reject conditions EXACTLY** (do/while nesting).
- **`cc850` TryPlacePath** @0x1400cc850 — build candidate polyline (closed ring if
  branchSel<0 else branch off `paths[branchSel]`) via `cc510`, validate (self- & cross-path
  intersection in (0,1)), deep-append on success. Sub-calls: `resize_0x1c`(FUN_140023b70),
  `cc510`, `Rand`(1- & 2-arg), `Vector2::LengthSquared`, `FUN_140024460` (AoA deep-append),
  MemFree. **Transcribe Rand(3)/Rand(4) endpoint CONCAT44 packing per-case** (determinism).

### Layer 3 — rivers + erosion + stamps (refine the heightmap)
- **`cd460` GenerateRiverNetwork** @0x1400cd460 — place edge points, AVL-based connected-
  component labeling, connect nearest per cluster; populates river-point vec listB(+0x28).
  Sub-calls: `RandFloat/RandFloatTol/Rand`, `Vector2::Dot/Length/Normalize`, sqrtf,
  `FUN_1400245d0` (0xC append), AVL rotates (`FUN_140014c70` left, `FUN_140014da0` right),
  `FUN_140014c30` (BST free), `sub_7640` (vec<vec<int>> resize) + `helper_f9e0`, MemAlloc/Free.
  **Largest/most error-prone** (1202 instr; re-derive AVL from rotation helpers, not the
  decompiler's flattened gotos).
- **`eee0` RadialBrush** @0x1400ceee0 — shared powf-falloff brush; grow radius until edge
  falloff < 0.99, stamp square region into a target mask with min/max blend (MIN uses `<`,
  MAX uses strict `>`). Sub-calls: powf, sqrtf.
- **`e910` StampFeatures_Build** @0x1400ce910 — 3 temp masks (init 1/0/1); stamp regions
  (+0x18) → maskB(min)/maskC(max), secondary pts (+0x38) → maskC(max, exp2.0 scale1.778),
  river pts (+0x28) → maskA(min, exp1.33); then `h = maskB*h*maskA - maskC`. Sub-calls:
  `sub_3a00`, `eee0`, MemAlloc/Free.
- **`cc080` ErodeHeightmap** @0x1400cc080 — N droplets (row+0x24); each walks downhill on
  8-neighbour Moore grid, exchanging material (slope×1/16, clamp 0.5) until local min / edge.
  Sub-calls: `Rand`. Lowest-risk (no allocs). **Tie-break order is determinism-critical.**
- **`ec50` StampFeatures_Finalize** @0x1400cec50 — twin of e910 with 2 masks (no river pass);
  `h = mul*h - sub`. Sub-calls: `sub_3a00`, `eee0`, MemAlloc/Free.

### Layer 4 — smoothing + texture + orchestration
- **`c350` SmoothHeightmap** @0x1400cc350 — **weighted 3×3 box blur** (center 2.0, 4 ortho
  neighbours 1.0, ÷Σw), temp buffer then copy-back, edge-clamp indices. **Run twice** (f390).
  Sub-calls: `sub_3a00`, MemFree. (Agents under-spec'd this — it is REQUIRED in the pipeline.)
- **`cf150` CellNormal** @0x1400cf150 — per-cell normal via heightmap central differences,
  XOR-negate (0x80000000), normalized. Sub-calls: `Vector3::Normalize`. (cf440 dep.)
- **`cf440` GenerateBlendTexture** @0x1400cf440 — returns `void*` RGBA `dim×dim`: R=rock/perlin
  + height bands, G=grass/slope bands, B=AO from 21×21 normal kernel. Sub-calls: `RandTol`×2,
  `PerlinNoise::ctor(0.25,4.0,..)`, `FUN_140023e00` (0x18 resize), `cf150`, `PerlinNoise::Generate`,
  `Vector3::Normalize/Dot/+=`, `Scalar::Max`, MemAlloc/Free. (~2KB, many DAT thresholds.)
- **`fd10` ResolveMaterialRef** @0x14001fd10 — lazy resource resolve: if `ref.cookie ==
  factory.version` return cached; else `Find(Filename(name))`, `IsDerivedFrom(typeId@0x140151350)`,
  cache result (NULL-safe). Sub-calls: `ExternalFactory` singleton, `Filename::ctor/dtor`,
  `ExternalFactory::Find`, `IsDerivedFrom`.
- **`c4b0` TradeWork_core** @0x1400ac4b0 — the orchestrator (§0). Sub-calls: `sub_c890`[done],
  `f390`, quantize loop, `cf440`, `sub_c780`[done], `sub_c1e0`[done], `b260`, `sub_f0a0`[done],
  inlined push_back append loop (copies only leading 8 bytes of each 0x1c record, backward).
- **`f390` TerrainPipeline** @0x1400cf390 — calls the stages in the §0 order. Trivial glue.
- **`b0230` TerrainBuild_Dispatcher** @0x1400b0230 — top-level (§0). Sub-calls: `String::ctor/dtor`,
  `fd10`×3, `Random::ctor(seed)`, `c4b0`, MemFree×4, `FreePatchListVector`.

### Layer 5 — detail mesh (DEFERRABLE, see §6)
- **`b260` DetailMeshBuilder** @0x1400ab260 — cliff/bank/border ribbon mesh → VB+0x160/IB+0x180.
  Pass1 = `fb00` per segment; Pass2 = cliff-region heightmap patches w/ `f3e0` dedup.
  Sub-calls: `efd0`[done], `fb00`, `f3e0`, `sub_74a0`, sqrtf, `FUN_140014b00` (AVL free),
  Vector3 math, `VertexBuffer/IndexBuffer::Create`.
- **`f3e0` VertexDedupAVL** @0x14001f3e0 — AVL dedup map; returns existing idx or insert+append.
  Sub-calls: `sub_ec10` (comparator), `4b40` (rebalance), `sub_74a0`, MemAlloc.
- **`sub_ec10` VertexComparator** @0x14001ec10 — interleaved short/byte lexicographic compare. Leaf.
- **`4b40` AVLRebalance** @0x14001f4b40 — walk parent chain, rotate. Sub-calls: rotate-left/right
  primitives (`sub_5000`/`sub_4ed0`).
- **`fb00` ContourRibbon** @0x1400cfb00 — per-segment 2-contour quad strip (3168B, ~12 sub-helpers:
  `sub_40a0`, `helper_31d0`, `sub_ba30` gate, `sub_b750`/`sub_b360` resamplers, `sub_4a00`/`4240`
  BST dedup, `sub_4920`/`73d0` appends). **Heaviest single function**; multi-day. Stubbable empty.

---

## 6. MINIMUM PATH TO VISIBLE TERRAIN

Goal: New Game shows a rendered heightmapped landscape (base ground surface), textured,
without crashing. The base ground mesh is `sub_c1e0` (VB+0x100/IB+0x120), which is
**already reimplemented** and reads the int16 heightmap at `+0xd8`. So the minimum path
is: produce a valid int16 heightmap + materials + (a) blend texture, then let the
already-done mesh/upload builders run.

**Smallest ordered implement-set:**

1. **`b0230`** (dispatcher) — writes dims, seeds RNG, calls `c4b0`, frees scratch.
2. **`fd10`** (×used by b0230) — resolve the 3 materials (NULL-safe; untextured if missing).
3. **`FreePatchListVector`** — teardown (needed so b0230 doesn't leak/crash on free).
4. **`f390`** (glue) — but **trimmed**: for first-light you may call ONLY `e5d0` and skip the
   feature/erosion/stamp/smooth stages (see "deferral" below). Full faithful = all stages.
5. **`e5d0`** — the base perlin heightmap. **This is the single load-bearing function for
   "is there terrain at all".**
6. **`c4b0`** (orchestrator) — quantize loop + call the already-done `sub_c890`/`sub_c780`/
   `sub_c1e0`/`sub_f0a0`. **Stub `b260` to a no-op** (see below). **Stub `cf440`** to return a
   small valid RGBA buffer (e.g. a single flat colour `dim×dim`) so `+0x140` is non-NULL and
   the splat sampler has a texture (terrain renders flat-shaded/untextured but visible).
7. **`std::sqrtf` / `std::powf`** mappings for the e5d0 helpers.
8. Add a 0xC-stride resize variant only if a trimmed f390 still touches the feature lists.

That set yields a **smooth radial island with real perlin elevation, lit by the base mesh
normals, drawn with the base VB/IB** — i.e. visible, walkable-looking terrain.

**What can be deferred and the visual cost:**

- **`b260` detail mesh — DEFER.** It is purely ADDITIVE into a SEPARATE VB/IB
  (+0x160/+0x180): cliff side-walls, river/lake banks, map-edge skirt. With it stubbed,
  the ground still renders as a continuous heightmapped surface; you lose only vertical
  cliff faces and the border skirt (map edges look like a flat cut-off / slab edge; steep
  slopes lose rocky vertical faces). **Stub requirement:** if the renderer issues an
  unconditional draw on +0x160/+0x180, the stub must still `Create` a zero/tiny VALID
  VB/IB (or the draw must be guarded) to avoid a null-buffer GPU call. Safest stub:
  `Create(detailVB, 0, false, nullptr)` + same for IB, or leave the staging arrays empty
  and let Create handle count==0.
- **Feature stages `cd090/cd150/ccf60/cd460/e910/cc080/ec50` — DEFER for first-light.**
  e5d0 alone gives a complete island. Skipping these means: no carved rivers/lakes, no
  mountain ranges, no coastline irregularity, no erosion, no resource-region carving — the
  island is a smooth radial dome. The terrain is still fully rendered and correct-looking,
  just "generic". They are required for gameplay-accurate terrain but NOT for first-light.
- **`c350` smoothing — DEFER with e5d0-only** (e5d0 output is already smooth). If you bring
  the feature stages online, you MUST also bring `c350` (×2) online or stamped/eroded
  terrain will look jagged.
- **`cf440` blend texture — STUB, don't defer the slot.** `c4b0` stores its result at
  `+0x140` and `sub_c780`/the renderer expect a texture. Returning a flat RGBA buffer is the
  minimum; the real cf440 only changes surface texturing (rock/grass/AO splat), not geometry.

**Bring-up order recommendation:** (1) the minimum set above → confirm a textured dome
renders; (2) add the full f390 feature pipeline + c350 → confirm rivers/mountains/erosion;
(3) add real cf440 → confirm rock/grass/AO splat; (4) add b260 → confirm cliffs/banks/edges.

---

## 7. RISKS & OPEN QUESTIONS

**Threads / SEH / factory:**
- No worldgen function spawns threads; the whole build runs synchronously on the load
  thread. Safe for standalone.
- `b0230` has an SEH state cookie (`MOV [RSP+0x38],-2`) and `c4b0`/`b260` have RSP-capture
  prologues — these matter ONLY for DLL-mode prologue-hook trampolines, NOT for standalone
  reimpl (we replace the function wholesale). N/A here.
- **Factory cascade (REAL risk):** `fd10` and the 3 material lookups go through the global
  `ExternalFactory`. If the singleton slot `[0x1401529d8]` is unresolved (NULL),
  `*(int*)(factory+8)` faults. Standalone MUST have the `ExternalFactory` dispatch slot AND
  the type-info slot `[0x140151350]` registered before terrain build, AND the 3 named
  `.rsc` material/texture resources loaded — else terrain renders untextured (NULL material
  refs are handled gracefully and do NOT crash; only an unresolved FACTORY crashes).
  `g_pExternalFactorySingleton` + `fn_ExternalFactory_Find` + `fn_IsDerivedFrom` are already
  wired in `engine_funcs`, so this is mostly a resource-registration concern.

**Unresolved offsets / values to pin at impl time:**
1. **`SizeRow` literal values** — field OFFSETS confirmed (§1.2); per-map-size NUMBERS live
   in the `.rsc` TerrainGenerator descriptor (`gen->sizeTable`), not a static global. Runtime-
   dump `gen->sizeTable + sizeIdx*0x44` (debugger) or read ToolKit `resource/Template/` to get
   dim / river counts / range counts / erosion iterations. Without them, the pipeline runs but
   feature density/scale is guessed.
2. **Heightfield `+0x08..+0x17` header split** — confirm `+0x08 data / +0x10 count / +0x14 cap`
   (the recon's "+0x10 secondary buffer" is most likely the count slot of the heightmap's own
   `{data,count,cap}` header, since `sub_3a00` resizes it as one). Single-step `sub_3a00` once.
3. **`0x1401721a8` value (7.5 vs 8.0)** — agents conflated two nearby constants. `read_memory`
   once; e5d0's `(perlin1+1)*N` term is 8.0 per the majority read.
4. **`System::Random` sizeof (~0x9C8)** — must match the Runtime type exactly or `+0xa20`/
   `+0xa28` land wrong. Dispatcher's `Random local_a00[2504]` implies ~0x9C8; verify vs the
   Runtime export / `gameupdateloop_load.cpp`'s existing usage.
5. **Material resolved type** — type-info `[0x140151350]` is shared with several functions,
   suggesting a common base resource type (Material/Texture/Mesh). Dump the resolved type name
   at runtime to name it; affects only the IsDerivedFrom filter, not control flow.
6. **PerlinNoise dtor / leak** — e5d0/cf440 show no explicit PerlinNoise dtor. Confirm on the
   Runtime side whether the ctor heap-allocates a permutation table needing free (possible
   per-gen leak) or is inline/pooled. One-time gen, so a small leak is tolerable for first-light.
7. **Determinism (save-game parity)** — if bit-exact parity with original saves is required,
   `powf`/`sqrtf` ULP diffs at the `0.99` radius-growth threshold (eee0) and the path/range
   RNG-draw order (cc850 Rand(3)/Rand(4) CONCAT packing, cd150 scale reset, cd460 record .x/.y
   packing) must be transcribed verbatim. For a playable standalone (no shared saves), platform
   `powf`/`sqrtf` and faithful-but-not-bit-exact code are fine.
8. **b260 pass-2 specifics** — heightmap (gx,gy)→(col,row) indexing (halfDim offset vs +0xd0
   stride, off-by-one), the cliff NORMAL source for the band-projection dot, and triangle WINDING
   (vs renderer cull mode) need live verification. Deferred with the detail mesh.
9. **AVL fidelity (cd460, f3e0)** — rotations affect index/label assignment ORDER only
   (cosmetic, mesh/graph still valid); re-derive from the rotation helpers, not the decompiler's
   flattened gotos.

**Bottom line:** the base-mesh / texture-upload / all array-resize leaves are already
reimplemented. First-light terrain needs only `b0230 + fd10 + FreePatchListVector + e5d0 +
c4b0` (+ a stubbed `cf440` and a no-op `b260`), giving a textured perlin island. The feature
stages, real `cf440`, `c350` smoothing, and `b260` detail mesh are layered additions, each
independently testable.
