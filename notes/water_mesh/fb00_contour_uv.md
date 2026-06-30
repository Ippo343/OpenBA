# TradeWork_impl_fb00 @ 0x1400cfb00 — contour water-mesh vertex/UV generator

Read-only Ghidra analysis (Application-x64.exe, base 0x140000000). This is the per-water-body
function that the orchestrator chain `TradeWork_impl_c4b0 (0x1400ac4b0) -> TradeWork_impl_b260
(0x1400ab260) -> fb00` uses to produce the contour vertices/indices for ONE flowing water body
(river/stream). Its whole purpose is to lay out a quad strip that follows the water body's
**centerline**, so that the UV runs *along the flow* (V = arc length) and *across the width*
(U = 0..1), regardless of world-axis orientation. That is exactly the "skewed/curved per body"
UV the retail water has.

---

## 1. Signature & output buffers (CONFIRMED)

```c
void TradeWork_impl_fb00(
    longlong param_1,    // worldgen TradeData
    int      param_2,    // bodyIdx  (water-body index)
    longlong *param_3,   // vtxOut  -> growable array of 16-byte vertices {float posX,posY,uvU,uvV}
    longlong *param_4);  // idxOut  -> growable array of uint16 indices (two tris per strip quad)
```

`param_3`/`param_4` are the engine's standard growable-vector handles (the same 16-byte
`{ptr, int count, int capacity}` POD the whole function uses everywhere — see §6). On return:

- **vtxOut** (`param_3`) holds a de-duplicated vertex list. **Each vertex is 4 floats = 16 bytes:**
  - `+0x00 float posX`  (world-grid X of the strip vertex, pre-48x scale)
  - `+0x04 float posY`  (world-grid Y)
  - `+0x08 float uvU`   (cross-strip coordinate, 0.0 .. 1.0)
  - `+0x0C float uvV`   (streamwise coordinate = normalized arc length along the body, see §3)
- **idxOut** (`param_4`) holds `uint16` indices, 6 per strip quad (two triangles), referencing
  vtxOut. Pushed by the local helper at the tail (label `TradeWork_impl_fb00_sub_73d0`).

The vertex de-dup is done through a small spatial hash keyed on the *position* (`local_468`,
initialized by `FUN_140024170`); the matching helper `TradeWork_impl_fb00_sub_4a00` looks a
position up, `TradeWork_impl_4240` inserts it, and `..._sub_4920` appends the new vertex's
{pos,uv} to the flat output array `local_478`. (These three are local labels; their exact
addresses are not the ones a naive `0x1400c4240`/`0x1400c4920` decompile lands on — those names
are Ghidra's nearest-symbol guesses for unrelated functions, ignore them.)

### How b260 then packs each fb00 vertex (CONFIRMED from 0x1400ab260)

For every fb00 output vertex `{posX,posY,uvU,uvV}` the parent packs a 16-byte GPU vertex:

```c
int16 vx = (int16)(posX * 48.0);          // DAT_1401721e4 = 48.0
int16 vy = (int16)(posY * 48.0);
int16 vz = (int16)(-192.0 - bodyIdx);     // DAT_1401722a4 = -192.0  (per-body z bias, NOT a height!)
int16 vw = 0;
int16 tu = (int16)(uvU * 1024.0);         // DAT_140172250 = 1024.0
int16 tv = (int16)(uvV * 1024.0);
uint8 rgba[4] = { 0x7f, 0x7f, 0xff, 0xff };
```

Note the **posZ is `-192 - bodyIdx`, a per-body z-bias** (the shader scales pos.z by 1/256 ->
-0.75 world, decreasing per body to break z-fighting between overlapping bodies). It is *not* a
real water level. (The water-level/height comes from the SECOND, terrain-grid water section in
b260 at `param_1+0x1d0`, which does NOT use fb00.) The water VS then multiplies pos by
(1/48,1/48,1/256) and uv by (1/1024), so `tu/tv = uv*1024` lands the UV back in `[0..1]*…`.

---

## 2. Input data layout (param_1 = TradeData)

- `param_1 + 0x18` : base of the water-body record array, **stride 0x10**.
  - `record[bodyIdx].ptr   = *(longlong*)(param_1+0x18 + bodyIdx*0x10 + 0x00)` -> centerline array
  - `record[bodyIdx].count = *(int*)     (param_1+0x18 + bodyIdx*0x10 + 0x08)` -> # centerline pts
- **Centerline point array** (the body's spine), **stride 0x1c (28 bytes)**:
  - `+0x00 float x`
  - `+0x04 float y`
  - `+0x14 float halfWidth`  (channel half-width at this node)
  - (additional per-node fields at +0x08..+0x13, +0x5c, +0x68 are flow/normal data consumed by
    the offset-contour builder; not needed for the UV story)
  - `fVar46 = centerline[0].halfWidth + 2.0` (DAT_140172174 = 2.0) is the working **radius**
    (half channel width + 2-tile margin) used throughout.

(The `param_2+0x20` water-tile map mentioned in the task brief — stride 0x30, flags at +0x2a/+0x2b
— is consumed by the *terrain-grid* water path in b260, not by fb00. fb00 works purely off the
centerline polyline + halfWidth.)

---

## 3. THE UV DERIVATION (most important)

fb00 builds the strip in three conceptual stages. Stage C is where pos+uv per vertex are emitted.

### Stage A — build two offset contours (left & right banks)
The first ~half of fb00 takes the centerline and, using a 2D spatial hash grid (`local_1a0` init
by `FUN_140024170`, edges added by `TradeWork_helper_31d0`) plus two cell grids (`local_390`,
`local_3f0` init by `..._sub_40a0`), generates two offset polylines at +radius and −radius from
the spine — the left bank list (`local_350`, stride 0x1c entries with point at +0x14/+0x18) and
the right bank list (`local_3b0`). Corners are fanned with `count = acos(dot)*(180/PI)/90*20`
segments (`TradeWork_helper_0bc0` = acos; DAT_1401721ec = 57.2958 = 180/PI; DAT_140172204 = 90;
DAT_1401721c4 = 20). Straight runs are subdivided `int(width*4.0)` times (DAT_140172194 = 4.0).
This stage decides geometry only; it does NOT set UVs.

### Stage B — resample both banks to a common point count, compute streamwise V
For each matched pair of bank segments, fb00 calls `TradeWork_impl_fb00_sub_b360` twice (once per
bank). That helper is the **UV engine**. Its inner emit (decomp of 0x1400cb360):

```c
// param_5 -> &totalArcLen, param_6 -> &uvScale, param_7 -> &uvOffset
// walks the bank polyline (param_8+0x48 array, stride 0x1c, x@+0x14 y@+0x18),
// accumulating arc length fVar19 across segments of length local_res18:
fVar16 = 1.0 - (fVar19 - fVar18)/segLen;     // frac into current segment   (g_fUVScale = 1.0)
fVar17 = 1.0 - fVar16;
pos.x  = fVar17*P[i].x + fVar16*P[i+1].x;    // interpolated bank position   -> param_9 list
pos.y  = fVar17*P[i].y + fVar16*P[i+1].y;
uvV    = ((fVar19 - fVar17*segLen) / *param_5) * (*param_6) + (*param_7);   // <-- THE UV
//       = (arcLengthAtThisPoint / totalArcLen) * uvScale + uvOffset        -> param_10 list
```

So the **streamwise coordinate V = (arcLength / totalArcLength) * uvScale + uvOffset.**
This is the heart of "UV follows the water shape": V is the *normalized distance traveled along
the bank polyline*, so the texture advances with the river no matter which way it bends. The two
output lists per bank are therefore:
- positions (param_9): bank points evenly spaced by *output step* (param_4 steps), and
- V-coords  (param_10): each point's normalized arc-length * scale + offset.

`uvScale` (`local_48c`) and `uvOffset` (`local_480`) are computed per-segment-pair in fb00 so
the V is continuous and roughly isometric. `local_480` starts at DAT_14017228c = **-16.0** and is
advanced by the measured average edge length each pair; `local_48c = avgEdgeLen / step`. The
companion `sub_b750` (0x1400cb750) is the position-only resampler used first to measure how many
points each pair needs and the total length fed in as `*param_5`. Two early gates reject
degenerate pairs (DAT_1401721d0 = 32.0 angle gate via dot/Abs; DAT_1401720d0 = 0.02 and
DAT_1401720b8 = 0.001 length gates).

A third helper, `sub_bdf0` (0x1400cbdf0), registers each generated contour point into the spatial
grid and **snaps it to an existing nearby point** (nearest within DAT_140172268 = 1e6, i.e.
"closest wins"), welding the left/right contours where they meet at junctions. `radius2 =
param_4 * 2.0` (DAT_140172174) sets its search box.

### Stage C — emit the quad strip with bilinear pos and (U_cross, V_along) UVs
The final loop (decomp tail) walks the two resampled lists in lockstep. For each along-contour
segment `i` (outer loop, `iVar35-1` iterations) it lays a 4-column cross strip (inner loop
`lVar36 = 4`):

```c
local_2e8 = DAT_140172100 = 0.25;            // cross-strip U step
u_lo = 0.0;  u_hi = 0.25;                     // reset each along-segment
posA0 = bankA_pos[i];   posA1 = bankA_pos[i+1];
vA0   = bankA_uv[i];    vA1   = bankA_uv[i+1];   // streamwise V from Stage B
for (col = 0; col < 4; ++col) {
    // cross-interpolate between bank A and bank B (offset local_2e0 = &listB - &listA):
    posLo' = Lerp(bankA_pos[i],   bankB_pos[i],   t);     // -> local_498
    posHi' = Lerp(bankA_pos[i+1], bankB_pos[i+1], t);     // -> local_318
    vLo'   = Lerp(bankA_uv[i],    bankB_uv[i],    t);     // -> local_4dc
    vHi'   = Lerp(bankA_uv[i+1],  bankB_uv[i+1],  t);     // -> local_48c
    // emit quad (de-duped through the position hash); UVs assigned:
    V0 = { posA0 ,  uv=(u_lo, vA0) }
    V1 = { posLo', uv=(u_hi, vLo') }
    V2 = { posHi', uv=(u_hi, vHi') }
    V3 = { posA1 , uv=(u_lo, vA1) }
    // 6 indices: (V0,V2,V1),(V0,V3,V2)
    // advance for next column:
    u_lo = u_hi;  u_hi += 0.25;          // U marches 0 ->0.25 ->0.5 ->0.75 ->1.0
    posA0 = posLo'; posA1 = posHi';
    vA0   = vLo';   vA1   = vHi';
}
```

Result per vertex:
- **uvU (cross-strip)** = column boundary `col*0.25` in `[0,1]` — uniform across the channel width.
- **uvV (streamwise)** = the Stage-B arc-length coordinate, cross-interpolated between the two
  banks. Because V is arc length / total length (×scale+offset), the texture flows along the
  river and is constant across the width band — i.e. the UV grid is *bent to the channel*, never
  axis-aligned. That is the retail per-river flow / per-lake calm look.

(NOTE — the `Math::Vector2::Lerp(a, out, b)` calls in the decomp show the t implicitly; the most
consistent reading is t = the running `u_hi`/column fraction. The structure — bilinear pos,
U=column, V=arc-length — is unambiguous; only the exact Lerp blend factor is worth a quick
runtime confirm.)

---

## 4. Struct field offsets touched

### param_1 (TradeData)
| Offset | Meaning |
|---|---|
| +0x18 | water-body record array base, stride 0x10 |
| record +0x00 | centerline point-array ptr |
| record +0x08 | centerline point count |
| centerline +0x00 / +0x04 | point x / y (stride 0x1c) |
| centerline +0x14 | half-width at node |

### Internal grids (stack-local in fb00)
| Symbol | Meaning |
|---|---|
| local_1a0 (init FUN_140024170) | spatial hash for contour-point welding |
| local_390 / local_3f0 (init sub_40a0; head fields 4.0,0.25) | left/right cell grids (cell=4.0, inv=0.25) |
| local_350 / local_3b0 | left / right offset-bank polylines, stride 0x1c, point @ +0x14/+0x18 |
| grid +0x30 | cell -> point-index table |
| grid +0x48 | flat point array base |
| grid +0x5c | running point counter |
| grid +0x04 | inverse cell size |
| local_288/local_328 | bank-A resampled positions / V-coords (from sub_b360) |
| local_228/local_248 | bank-B resampled positions / V-coords |
| local_468 / local_478 | position-dedup hash / flat output vertex array |

### b260 geometry object (param_1 in b260)
| Offset | Meaning |
|---|---|
| +0x160 | Video::VertexBuffer (created `count<<4` bytes) |
| +0x180 | Video::IndexBuffer (created `count*2` bytes) |
| +0x1a0/+0x1a8/+0x1ac | vertex staging ptr / count / capacity (16-byte verts) |
| +0x1b0/+0x1b8/+0x1bc | index staging ptr / count / capacity (uint16) |
| +0x1c0 | per-body draw-range table (contour water), stride 0x10 |
| +0x1c8 | water-body count |
| +0x1d0 | per-cell draw-range table (terrain-grid water — separate path) |

---

## 5. .rdata constants (actual values)

| DAT | hex | value | role |
|---|---|---|---|
| DAT_140172100 | 0x3e800000 | **0.25** | cross-strip U step (4 cols → U 0..1) |
| DAT_140172114 | 0x3f000000 | **0.5** | centerline endpoint inflation (±0.5) |
| DAT_140172174 | 0x40000000 | **2.0** | radius margin / `radius2 = r*2` in sub_bdf0 |
| DAT_140172194 | 0x40800000 | **4.0** | straight-run subdivision = int(width*4) |
| DAT_1401721c4 | 0x41a00000 | **20.0** | corner fan: segs per 90° |
| DAT_1401721d0 | 0x42000000 | **32.0** | pair angle/length gate |
| DAT_1401721e4 | 0x42400000 | **48.0** | b260 pos pack scale (pos*48 → int16) |
| DAT_1401721ec | 0x42652ee1 | **57.29578** | 180/PI (rad→deg) for corner fan |
| DAT_140172204 | 0x42b40000 | **90.0** | corner-fan angle denom |
| DAT_140172250 | 0x44800000 | **1024.0** | b260 uv pack scale (uv*1024 → int16) |
| DAT_140172268 | 0x49742400 | **1000000.0** | sub_bdf0 nearest-point sentinel |
| DAT_140172280 | 0xbf800000 | **-1.0** | alt uvScale sign (flow-direction branch) |
| DAT_14017228c | 0xc1800000 | **-16.0** | streamwise uvOffset start (local_480) |
| DAT_1401722a4 | 0xc3400000 | **-192.0** | b260 posZ base (`vz = -192 - bodyIdx`) |
| DAT_1401722c0 | 0x80000000 | sign mask | XOR to negate a float (build perpendicular) |
| DAT_1401720b8 | 0x3a83126f | 0.001 | min length gate |
| DAT_1401720d0 | 0x3ca3d70a | 0.02 | dot/colinear gate |
| DAT_1401720bc | 0x3b800000 | 1/256 | (b260 terrain-water section only) |
| DAT_14017222c | 0x437f0000 | 255.0 | (b260 terrain-water section only) |
| DAT_14017227c | 0xbf400000 | -0.75 | (b260 terrain-water section only) |
| g_fUVScale | 0x3f800000 | **1.0** | lerp identity `(1-t)` (confirmed by usage) |

---

## 6. Growable-vector idiom (reused ~12× in fb00 and b260)

Every list is a `{void* data; int count; int capacity}` 16-byte handle, grown like:

```c
int n0 = h.count, n1 = h.count + 1;
if (n1 != n0) {
    void* old = h.data; h.count = n1;
    if (h.capacity < n1) {
        h.capacity = (n1 == n0+1 && n0 != 0) ? n1*3 : n1;          // 3x growth
        h.data = MemAlloc(h.capacity * ELEM, ALIGN);
    }
    for (i = (old==h.data ? n0 : 0); i < n1; ++i)
        ((ELEM*)h.data)[i] = (i < n0) ? ((ELEM*)old)[i] : newElem;
    if (old != h.data) MemFree(old);
}
```
`ELEM` = 0x1c (contour points), 8 (Vector2 lists), 4 (V-coord lists), 0x10 (output verts /
b260 staging), 2 (uint16 indices). Free order at fb00 tail releases all of them.

---

## 7. C++ port sketch (our conventions)

Faithful-enough skeleton for `code/Game/...` (uses `fn_MemAlloc(singleton,size,align)` /
`fn_MemFreeStatic(ptr)`, plain POD, no STL in the hot path). The hard part is Stage A
(offset-contour + spatial-hash welding); Stages B/C are straightforward.

```cpp
struct OutVtx { float posX, posY, uvU, uvV; };   // 16 bytes, what fb00 emits

struct Vec2 { float x, y; };
static inline Vec2 lerp(Vec2 a, Vec2 b, float t){ return { a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t }; }

// Stage B: resample one bank polyline to `steps` points and compute streamwise V.
// `bank` = stride-0x1c array (x@+0x14,y@+0x18); totalLen precomputed; uvScale/uvOffset from caller.
static void ResampleBank(const uint8_t* bank, int firstSeg, int steps,
                         float totalLen, float uvScale, float uvOffset,
                         Vec2* outPos, float* outV)
{
    auto P = [&](int i){ const float* f=(const float*)(bank + (size_t)i*0x1c);
                         return Vec2{ f[5], f[6] }; };   // +0x14,+0x18
    float arc = 0.f, target = 0.f;
    float stepLen = totalLen / (float)steps;
    int seg = firstSeg, e = 0;
    Vec2 a = P(seg), b = P(seg+1);
    float segLen = std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
    for (; e < steps; ) {
        if (arc + segLen <= target) { arc += segLen; ++seg; a=P(seg); b=P(seg+1);
                                      segLen=std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)); continue; }
        float f = (target - arc) / segLen;             // frac into current segment
        outPos[e] = lerp(a, b, f);
        outV[e]   = (target / totalLen) * uvScale + uvOffset;   // <-- streamwise V
        target += stepLen; ++e;
    }
}

// Stage C: emit the 4-wide quad strip between two resampled banks.
static void EmitStrip(const Vec2* posA, const float* vA,
                      const Vec2* posB, const float* vB, int n,
                      VertexSink& vtx, IndexSink& idx)     // sinks dedup pos + push uint16
{
    const float U_STEP = 0.25f;                            // DAT_140172100
    for (int i = 0; i + 1 < n; ++i) {
        Vec2 p0 = posA[i], p1 = posA[i+1];
        float v0 = vA[i], v1 = vA[i+1];
        float uLo = 0.f, uHi = U_STEP;
        for (int col = 0; col < 4; ++col) {
            float t = uHi;                                 // blend factor across width (verify)
            Vec2  pLo = lerp(posA[i],   posB[i],   t);
            Vec2  pHi = lerp(posA[i+1], posB[i+1], t);
            float vLo = vA[i]   + (vB[i]  - vA[i])  *t;
            float vHi = vA[i+1] + (vB[i+1]- vA[i+1])*t;
            uint16_t I0 = vtx.AddDedup({p0.x,p0.y,  uLo, v0 });
            uint16_t I1 = vtx.AddDedup({pLo.x,pLo.y,uHi, vLo});
            uint16_t I2 = vtx.AddDedup({pHi.x,pHi.y,uHi, vHi});
            uint16_t I3 = vtx.AddDedup({p1.x,p1.y,  uLo, v1 });
            idx.Tri(I0,I2,I1); idx.Tri(I0,I3,I2);
            uLo = uHi; uHi += U_STEP; p0 = pLo; p1 = pHi; v0 = vLo; v1 = vHi;
        }
    }
}
```

Stage A (offset-contour construction with the spatial-hash weld) still needs porting from
`sub_40a0`/`helper_31d0`/`sub_ba30`/`sub_3450`/`sub_3680`/`sub_bdf0` if we want pixel-exact banks;
a cheaper approximation is to offset the centerline by ±halfWidth along the per-node normal and
feed those two polylines straight into Stage B/C — that already yields contour-following UVs,
just without the rounded junctions/corner fans.

### Packing in our b260 equivalent
```cpp
int16_t vx = (int16_t)(v.posX * 48.0f);
int16_t vy = (int16_t)(v.posY * 48.0f);
int16_t vz = (int16_t)(-192.0f - bodyIdx);   // per-body z bias
int16_t vw = 0;
int16_t tu = (int16_t)(v.uvU * 1024.0f);
int16_t tv = (int16_t)(v.uvV * 1024.0f);
uint8_t rgba[4] = { 0x7f, 0x7f, 0xff, 0xff };
```

---

## 8. Verification status
- Signature, vertex stride/fields, b260 packing (48.0 / 1024.0 / -192-bodyIdx / 0x7f7fffff): CONFIRMED from decomp + read_memory.
- Streamwise V = `(arc/total)*uvScale + uvOffset`: CONFIRMED from sub_b360 decomp.
- Cross-strip U = 4×0.25 column boundaries: CONFIRMED from Stage-C decomp + DAT_140172100.
- All listed DAT values: CONFIRMED via read_memory (4 bytes, float).
- Open item to confirm at runtime: exact `Math::Vector2::Lerp`/`Math::Scalar::Lerp` blend factor
  in Stage C (structurally it is the column fraction; the decomp hides the explicit `t`).
- Stage-A offset-contour internals (spatial hash, corner fans) are described but not fully
  line-ported — secondary to the UV question.
```
