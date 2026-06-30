# Water Mesh — Open-Water / Lake Coverage Investigation

READ-ONLY Ghidra RE of Application-x64.exe (base 0x140000000). Goal: explain why the
standalone water surface only renders a thin coastline band and leaves open lake/ocean
interiors empty, while retail clearly meshes the open water.

## TL;DR

The water mesh builder **`TradeWork_impl_b260` (0x1400ab260)** has **TWO independent
geometry phases**, and our port only reproduces the first one:

- **Phase 1 — contour BANDS** (the `fb00` loop). Iterates the patchList at `hf+0x18`
  (count `hf+0x20`) and calls `TradeWork_impl_fb00` (0x1400cfb00) per patch. Each patch
  is a centerline polyline; fb00 lays a constant-width band around it. This is the
  coastline ring + secondary river/branch ribbons.
- **Phase 2 — filled DISKS** (inline, the `param_2+0x48` / `+0x50` loop). Iterates a
  *separate* list of `{x, y, radius}` water bodies at `hf+0x48` (count `hf+0x50`) and
  triangulates the terrain grid cells inside each radius into a **filled water
  surface**. **This is the open-water / lake / pond area fill.** That list is populated
  by the lake stage **`FUN_1400cd150` (0x1400cd150)**.

So the open-water surface is NOT produced by widening the patch bands. It is a
**second, disk-fill pass** over a lake list that our standalone never builds (or never
renders). Result: only the Phase-1 coastline band shows, exactly the symptom reported.

Per-patch band width is a **constant** (no per-point variable width, no distance
transform). Rivers (`cd460`) write a third list (`hf+0x28`) that `b260` does not mesh at
all.

---

## Q1 — `cc850` / TryPlacePath (0x1400cc850): per-point width is CONSTANT

Signature (reconstructed from callers):
`FUN_1400cc850(hf, numPts, scale, a4, a5, a6, halfWidth/*a7*/, a8, branchSel/*a9*/)`

Point stride is `0x1c` (7 floats): `x@+0, y@+4, a4@+8, a5@+0xc, a6@+0x10, a7@+0x14,
a8@+0x18`. The prologue loop writes **the same five args into every point**:

```c
for (each of numPts points p) {
    *(u32*)(p+0x10) = a6;
    *(u32*)(p+0x0c) = a5;
    *(u32*)(p+0x18) = a8;
    *(u32*)(p+0x14) = a7;   // <-- halfWidth, CONSTANT across the whole path
    *(u32*)(p+0x08) = a4;
}
```

So **+0x14 (the halfWidth fb00 reads) is a per-path constant = arg a7**. There is **no**
per-point variable width, **no** grow-to-water-radius step, **no** flood fill / distance
transform inside cc850. The remainder of cc850 only computes point *positions*: an
initial endpoint + midpoint, a relaxation pass (`FUN_1400cc510`), then self-intersection
/ proximity rejection against the other points and against existing patches; it returns 1
on success (appended) or 0 on rejection.

The single append into the patchList is:
`FUN_140024460(hf+0x18, *(int*)(hf+0x20)+1, &localPath)` — i.e. push_back into the
`hf+0x18` patchList, count `hf+0x20`.

### Per-path width constants (decoded from .rdata @ 0x140172150+)
- `DAT_140172194 = 4.0`  — coastline halfWidth (cd090 passes this as a7)
- `DAT_140172160 = 1.5`  — secondary-path halfWidth (ccf60 passes this as a7)
- `DAT_140172174 = 2.0`  — band-width additive used by fb00 (`width = patch.halfWidth + 2.0`)
- `DAT_140172158 = 1.25`

So a coastline band is `4.0 + 2.0` half-width; secondary ribbons are `1.5 + 2.0`. Neither
is wide enough to fill an open water body — confirming bands are contours, not fills.

---

## Q2 — Complete set of patchList (`hf+0x18`) writers

`FUN_140024460` (the patchList-specific append, writes `hf+0x18`, bumps `hf+0x20`) has
**exactly one** code caller:

- `0x1400cceeb` inside **`cc850`**.

And `cc850` itself has **exactly two** code callers:

- `0x1400cd113` in **`FUN_1400cd090`** (coastline) — loops `while (hf[8]==0)` i.e. until
  the patchList count `hf+0x20` becomes non-zero, so it adds **exactly 1** coastline ring
  (a7 = `DAT_140172194` = 4.0, branchSel = -NAN → "fresh ring" branch in cc850).
- `0x1400cd01d` in **`FUN_1400ccf60`** (secondary paths) — outer loop count
  `= *(int*)(*(hf+0xa28) + 0x20) = sizeRow[+0x20] = numSecondary`; each iteration retries
  cc850 up to 1000× until one succeeds (a7 = `DAT_140172160` = 1.5, branchSel = random
  index of an existing patch → "branch off existing path" branch in cc850).

**Therefore the meshed patchList = 1 coastline ring + numSecondary secondary ribbons.
Nothing else ever writes `hf+0x18`.**

### What the OTHER lists are (NOT meshed as bands)
- `hf+0x28` (count `hf+0x30`) — **rivers**, written by `FUN_1400cd460` via
  `FUN_1400245d0`. `b260` never reads `hf+0x28`. (cd460 builds a river graph/MST over
  the lake/scatter points; whatever it produces is consumed by later f390 stages, not by
  the band mesher.)
- `hf+0x38` (count `hf+0x40`) — scatter points, written by `cd150`.
- `hf+0x48` (count `hf+0x50`) — **lake/pond DISKS `{x,y,radius}` (stride 0xc)**, written
  by `cd150`. **This is the list b260 Phase 2 fills.**

---

## Q3 — How open water is actually meshed: b260 Phase 2 disk-fill

Worldgen orchestrator **`ConsumeProduceComponent_impl_f390` (0x1400cf390)** runs, in order:

```
e5d0  (setup; hf[0] = sizeRow[0]*2)
cd090 -> +0x18  (1 coastline ring)
cd150 -> +0x38 scatter points  AND  +0x48 lake disks {x,y,radius}
ccf60 -> +0x18  (numSecondary secondary ribbons)
cd460 -> +0x28  (rivers)
e910, cc080, ec50, c350, c350
```

The mesh is then built by `c4b0 (0x1400ac4b0)` → `b260 (0x1400ab260)` with
`param_1 = WaterMesh builder`, `param_2 = hf`:

**Phase 1 (bands):** reserves `hf+0x20` (= `param_2+0x20`) patches, loops the patch index
and calls `fb00(hf, idx, &verts, &indices)`; appends each patch's band verts/indices into
the builder's vertex/index buffers. Width per patch = `patch[0].halfWidth(+0x14) + 2.0`.

**Phase 2 (filled disks):**
```c
int nLakes = *(int*)(param_2 + 0x50);          // hf+0x50 = +0x48 list count
for (li = 0; li < nLakes; ++li) {
    float* L = (float*)(*(void**)(param_2+0x48) + li*0xc);  // {x@0, y@4, radius@8}
    float r  = L[2] + DAT_140172174;            // radius + 2.0
    // bounding box [center-r, center+r] in grid space
    for (gy in box) for (gx in box) {
        // sample terrain heightfield  *(short*)(builder+0xd8 + (gx*W + gy)*2) * scale
        // 4-corner inside-disk test (the bVar6/bVar7 + fVar37 distance logic)
        if (cell fully outside) skip;
        if (cell straddles or inside) emit 2 triangles (6 indices) -> *(+0xc) += 6;
    }
    // -> IndexBuffer slice for this lake
}
```

i.e. **each entry in `hf+0x48` becomes a solid disk of water triangles snapped to the
terrain grid.** That is the reflective open-water surface. Verified the list identity:
`b260` Phase 2 reads `param_2+0x48/+0x50`; `cd150` writes `hf+0x48/+0x50`; `b260`'s
`param_2 == hf == cd150`'s `param_1`. Same buffer.

`cd150` lake count = `sizeRow[+0x28] + Rand(0 .. sizeRow[+0x2c]-sizeRow[+0x28])`, each
lake appending one `{x,y,radius}` disk to `+0x48` (plus a cluster of scatter points to
`+0x38`).

Final buffers are uploaded via `Video::VertexBuffer::Create`/`IndexBuffer::Create` at the
tail of `b260` — Phase-1 bands and Phase-2 disks share one VB/IB.

---

## Q4 — Why standalone has only 1 patch / no interior

Two independent gaps, both pointing at an incomplete port of the b260/f390 water build:

1. **Open-water interior is gone because Phase 2 (the `hf+0x48` disk fill) is missing.**
   The reported port is described purely as "fb00 lays a band per patch" — that is b260
   **Phase 1 only**. The lake/ocean *area* surface is Phase 2, which our port apparently
   does not run, OR the `hf+0x48` list is empty because the lake stage **`cd150`** was
   not ported into the standalone worldgen. Even with a perfect patchList, bands alone
   can never fill an open body — retail relies on Phase 2 for that.

2. **patchList has only the 1 coastline ring because the secondary stage `ccf60` added
   nothing** — either `ccf60` isn't called in the standalone worldgen, or
   `numSecondary = sizeRow[+0x20]` evaluated to 0. The size-parameter row lives at
   `hf+0xa28` (`*(int**)(hf+0x28a)`) and is populated during hf/worldgen-params
   construction (not in `c4b0`/`f390`; those only read it). The exact table was not
   located in this pass; `sizeRow[+0x20]` (numSecondary) and `sizeRow[+0x28]/[+0x2c]`
   (lake count range) should be read at runtime for the actual map size to confirm they
   are non-zero in retail and what our standalone hf actually contains.

Note the symptom "only 1 patch, halfWidth 4.0" is fully consistent with: cd090 ran
(1 ring), ccf60 contributed 0, cd150 contributed 0 (or its `+0x48` output is unused by
our Phase-1-only mesh).

---

## Concrete fix proposal (prioritized)

1. **PRIMARY — port b260 Phase 2 (the `hf+0x48` lake-disk fill) and the `cd150` lake
   stage that fills `hf+0x48`.** This is the only mechanism in the engine that produces a
   filled open-water surface. Without it the standalone can only ever draw thin contour
   ribbons. Concretely:
   - Ensure the standalone worldgen calls `cd150` (between `cd090` and `ccf60`, as f390
     does) so `hf+0x48/+0x50` gets `{x,y,radius}` lake entries.
   - Add the Phase-2 loop to our `b260` reimpl: for each `hf+0x48` entry, triangulate the
     terrain-grid cells inside `radius + 2.0` and append to the same VB/IB as the bands.
     Width additive `DAT_140172174 = 2.0`; height sampled from the builder's heightfield
     at `+0xd8`, stride `+0xd0` (map width).

2. **SECONDARY — make `ccf60` contribute.** Confirm the standalone worldgen actually runs
   `ccf60`, and that `sizeRow[+0x20]` (numSecondary) is non-zero for the selected map
   size. If the size table at `hf+0xa28` isn't populated in standalone, the worldgen
   params struct construction needs porting/seeding. This adds the river/branch ribbons
   (halfWidth 1.5) on top of the coastline — secondary, since they are still bands not
   fills.

3. **VERIFY the full f390 chain is ported.** Rivers (`cd460` → `hf+0x28`) and the later
   f390 stages (`e910`, `cc080`) feed the river/feature network; if they are stubbed the
   patchList stays at the single coastline ring regardless of fixes 1–2.

### Do NOT do
- Do not try to "widen" the coastline patch's `+0x14` halfWidth to fill the lake — that
  is not how retail works (width is a per-path constant; fills come from Phase 2 disks),
  and a fat ring band would still leave the center hollow and distort the shoreline.

### Quick runtime checks to disambiguate gap #1 vs #2
- Log `hf+0x20` (patch count), `hf+0x50` (lake-disk count), `hf+0x30` (river count) right
  after the worldgen build. If `hf+0x50 == 0` → cd150/lake stage not producing → fix #1.
  If `hf+0x20 == 1` but `hf+0x50 > 0` → Phase 2 not being meshed → fix #1 (mesh side).
  If `hf+0x20 == 1` and you expected branches → fix #2 (ccf60/sizeRow).

## Key addresses
| Addr | Name | Role |
|------|------|------|
| 0x1400ac4b0 | TradeWork_impl_c4b0 | worldgen→mesh driver: f390 then b260 |
| 0x1400cf390 | ConsumeProduceComponent_impl_f390 | water worldgen stage orchestrator |
| 0x1400cd090 | (coastline) | adds 1 ring to patchList +0x18 (a7=4.0) |
| 0x1400cd150 | (lakes) | writes lake disks {x,y,r} to +0x48, scatter to +0x38 |
| 0x1400ccf60 | (secondary) | adds numSecondary ribbons to +0x18 (a7=1.5) |
| 0x1400cd460 | (rivers) | writes river list to +0x28 |
| 0x1400cc850 | TryPlacePath | places one polyline, CONSTANT per-point width a7 |
| 0x140024460 | (append) | sole writer of patchList +0x18 (called only by cc850) |
| 0x1400ab260 | TradeWork_impl_b260 | mesh builder: Phase1 bands (fb00) + Phase2 disks |
| 0x1400cfb00 | TradeWork_impl_fb00 | per-patch contour band geometry (Phase 1 only) |
