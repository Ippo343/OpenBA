# Water contour-mesh tiling cluster (marching-squares) — port spec

Binary: Application-x64.exe (base `0x140000000`). All read-only Ghidra analysis.

Cluster that builds the per-cell water **tile mesh**. `TradeWork_impl_f370`
walks the water-cell map, builds a 3x3 neighbor stencil per water cell, picks a
tile-shape index via `e180`, emits a textured quad via `cc10`, and accumulates
the geometry into per-water-body vertex/index lists which are then appended to
the geometry object's `+0x200` scratch list. `TradeWork_impl_fb00` later reads
`+0x200` to extract contour verts+UV.

> NOTE on addresses: the function called "f370" actually **starts at
> `0x1400af370`**; `0x1400afb00` (the address in the task) is *mid-function*.
> `disassemble_function(0x1400afb00)` returns the whole body starting at
> `0x1400af370`. Same function — just be aware the entry is `0x1400af370`.

---

## 0. Function map / call graph

| addr | name | role |
|------|------|------|
| `0x1400af370` | `f370` (TradeWork_impl) | main per-cell tiling loop |
| `0x1400ae180` | `e180` | 8-neighbor occupancy -> tile-shape index (0..46) |
| `0x1400acc10` | `cc10` | emit one quad (4 corner verts) for a shape |
| `0x1400acaa0` | `caa0` | build ONE corner vertex (pos/height/uv/normal) |
| `0x1400ae270` | `e270` | per-corner terrain normal from heightfield slope |
| `0x14001f250` | `f250` | reserve/resize list of 0x60-byte body entries |
| `0x14001fa80` | `fa80` | append/copy a 0x60-byte body entry (into +0x200) |
| `0x14001c890` | `c890` | reserve/resize short[] list (index buffer), stride 2 |
| `0x14001ee40` | `ee40` | reserve/resize 0x14-byte vertex list (alt path), stride 0x14 |
| `0x14001f8d0` | (append) | append one 0x14-byte vertex to a list |
| `0x1400021d0` | (resize int[]) | reserve/resize int[] (per-body cell counter @ +0x220) |
| `0x140152b60` | dispatch slot | `System::MemoryAllocator::MemAlloc(size, align)` |
| `0x140152b68` | dispatch slot | `System::MemoryAllocator::MemFree(ptr)` |

---

## 1. Clean decompiled C

### 1a. `e180` — neighbor stencil -> tile-shape index

Takes 8 neighbor booleans (`param_1` is an unused junk arg the call passes). The
4 diagonals are first *masked off* unless **both** of their adjacent orthogonal
neighbors are present (so a diagonal can never be the only thing bridging a
corner). Then an 8-bit mask is assembled and used to index a 256-byte LUT at
`0x1401c22d0`.

```c
// param_2=W  param_3=E  param_4=N  param_5=S
// param_6=NW param_7=NE param_8=SW param_9=SE   (1=water present)
int e180(junk, char W,char E,char N,char S, char NW,char NE,char SW,char SE)
{
    if (NW && (!W || !N)) NW = 0;   // diagonal only counts if both orthogonals set
    if (NE && (!E || !N)) NE = 0;
    if (SW && (!W || !S)) SW = 0;
    if (SE && (!E || !S)) SE = 0;

    unsigned idx = (W  ? 0x01 : 0)   // bit0
                 | (E  ? 0x02 : 0)   // bit1
                 | (N  ? 0x04 : 0)   // bit2
                 | (S  ? 0x08 : 0)   // bit3
                 | (NW ? 0x10 : 0)   // bit4
                 | (NE ? 0x20 : 0)   // bit5
                 | (SW ? 0x40 : 0)   // bit6
                 | (SE ? 0x80 : 0);  // bit7
    return (signed char)kShapeLUT[idx];   // kShapeLUT @ 0x1401c22d0, 0xff = unused
}
```

The neighbor->param wiring at the `f370` call site
(`e180(uVar24, f0[3], eb, f0[1], e9, f0[0], f0[2], ea, e8)`), where the 3x3
stencil `f0[0..8]` is row-major (rows N..S, cols W..E, center `f0[4]` excluded):

```
f0[0]=NW  f0[1]=N   f0[2]=NE
f0[3]=W   f0[4]=C   f0[5](=eb)=E
f0[6](=ea)=SW  f0[7](=e9)=S  f0[8](=e8)=SE
```
So: `param_2=W=f0[3]`, `param_3=E=eb`, `param_4=N=f0[1]`, `param_5=S=e9`,
`param_6=NW=f0[0]`, `param_7=NE=f0[2]`, `param_8=SW=ea`, `param_9=SE=e8`.

### 1b. `cc10` — emit one quad (4 corner vertices) for a shape

```c
// param_1 = geometry obj (this)
// param_2 = cellX (centered: cellX + halfMap, already added by caller)
// param_3 = cellY
// param_4 = OUT buffer: 4 vertices x 0x14 bytes = 0x50 bytes (caller's local_98)
// param_5 = shapeIdx (0..46)
// param_6 = bodyFlag2 (the cell +0x2b byte, stored into each vertex's +0x06)
void cc10(obj, int cellX, int cellY, Vertex out[4], int shapeIdx, byte bodyFlag2)
{
    // 4 corners, CCW from bottom-left:
    //  i=0 (cellX,   cellY)    i=1 (cellX+1, cellY)
    //  i=2 (cellX+1, cellY+1)  i=3 (cellX,   cellY+1)
    for (int i = 0; i < 4; i++) {
        int x = cellX, y = cellY;
        if (i==1) { x=cellX+1; }
        if (i==2) { x=cellX+1; y=cellY+1; }
        if (i==3) {            y=cellY+1; }
        Vertex tmp;                      // 0x10 useful bytes (caa0 fills 16)
        caa0(obj, x, y, &tmp, bodyFlag2);
        out[i] = tmp;                    // copies 0x10 bytes into the 0x14 slot
    }
    // overwrite each corner's TILE-ATLAS uv (vertex +0x0c / +0x0e) from the
    // per-shape uv table @ 0x1401c23d0 (8 bytes/shape = 4 shorts a,b,c,d):
    const short* uv = &kShapeUV[shapeIdx*4];   // a=uv[0] b=uv[1] c=uv[2] d=uv[3]
    out[0].tileU = uv[0]; out[0].tileV = uv[1];   // (a,b)  min,min
    out[1].tileU = uv[2]; out[1].tileV = uv[1];   // (c,b)  max,min
    out[2].tileU = uv[2]; out[2].tileV = uv[3];   // (c,d)  max,max
    out[3].tileU = uv[0]; out[3].tileV = uv[3];   // (a,d)  min,max
}
```

### 1c. `caa0` — build one corner vertex

```c
// out is a 0x14-byte vertex (only first 0x10 written here)
void caa0(obj, int x, int y, Vertex* out, byte bodyFlag2)
{
    int halfMap = obj->gridSize /*+0xC0*/ >> 1;
    Vector3 n;  e270(obj, &n, x, y);          // terrain normal at (x,y), normalized

    out->posX   = (short)x - halfMap;          // +0x00
    out->posY   = (short)y - halfMap;          // +0x02

    int dim = obj->hmDim /*+0xD0*/;            // heightfield dimension (square)
    int cx = clamp(x, 0, dim-1), cy = clamp(y, 0, dim-1);
    short h = obj->hmData/*+0xD8 short[]*/[ dim*cy + cx ];
    out->height = h;                           // +0x04  (terrain height sample)
    out->body   = (ushort)bodyFlag2;           // +0x06  (cell +0x2b flag)

    // packed terrain normal -> unsigned bytes  ((n + 1.0) * 127.5)
    out->nX = (sbyte)((n.x + g_fUVScale/*1.0*/) * 127.5f);   // +0x0c
    out->nY = (sbyte)((n.y + 1.0f)            * 127.5f);     // +0x0d
    out->nZ = (sbyte)((n.z + 1.0f)            * 127.5f);     // +0x0e
    out->pad = 0;                                            // +0x0f

    // GLOBAL terrain uv (whole-map), overwritten for tile uv only at +0x0c/+0x0e
    out->u = (short)((float)x / obj->gridSize * 16384.0f);   // +0x08
    out->v = (short)((float)y / obj->gridSize * 16384.0f);   // +0x0a
}
```
NOTE the field overlap: `caa0` writes the packed normal at vertex offsets
`+0x0c/+0x0d/+0x0e` **in its scratch tmp**, but `cc10`'s field copy relocates the
normal to `+0x10/+0x11/+0x12` of the *output* vertex, freeing `+0x0c/+0x0e` for
the per-shape tile uv it writes afterward. See the vertex layout in section 4.

### 1d. `e270` — corner terrain normal from heightfield slope

```c
// central-difference slope of the height map -> normal, then normalize.
void e270(obj, Vector3* outN, int x, int y)
{
    int dim = obj->hmDim;  short* H = obj->hmData;   // +0xD0 / +0xD8
    #define HS(px,py) H[ (long)dim*clamp(py,0,dim-1) + clamp(px,0,dim-1) ]
    outN->x = (float)( HS(x+1,y) - HS(x-1,y) ) * (-1.0f/256.0f);  // _DAT_140172270
    outN->y = (float)( HS(x,y+1) - HS(x,y-1) ) * (-1.0f/256.0f);
    outN->z = 2.0f;                                              // DAT_140172174
    Math::Vector3::Normalize(outN);
}
```

### 1e. `f370` — main per-cell tiling loop (cleaned)

```c
void f370(GeoObj* obj, WaterMap* wm)
{
    List60*  bodyOut   = &obj->scratch200;   // obj+0x200, list of 0x60-byte entries
    f250(bodyOut, 0);                        // clear it

    int W       = wm->width;                 // wm[0]
    int halfMap = (obj->gridSize - obj->c4) >> 1;   // (obj+0xC0 - obj+0xC4)>>1
    int nBodies = obj->nBodies;              // obj+0x218

    // per-body cell counter array @ obj+0x220 (int[nBodies]), reset to 0
    resize_int(&obj->bodyCounters/*+0x220*/, nBodies);
    memset(obj->bodyCounters.data, 0, nBodies*4);

    // working set: one Body60 per water body id (stride 0x60)
    List60 bodies = {};  f250(&bodies, nBodies);

    int blk = obj->blockSize;                // obj+0xC8 (=200 dec). Map tiled in blocks.

    for (int by = 0; by < W; by += blk)             // local_f8
    for (int bx = 0; bx < W; bx += blk) {           // local_f4
        // (re)initialise each Body60 entry for this block:
        for (int k = 0; k < nBodies; k++) {
            bodies[k].desc   = obj->bodyDescs/*+0x210*/[k];   // +0x00 (8 bytes)
            c890(&bodies[k].idx,  0);                         // index list -> empty
            ee40(&bodies[k].vert, 0);                         // vertex list -> empty (alt)
            bodies[k].originX = bx + halfMap;                 // +0x2c
            bodies[k].originY = by + halfMap;                 // +0x30
            bodies[k].maxX    = blk + bx + halfMap;           // +0x34
            bodies[k].maxY    = blk + by + halfMap;           // +0x38
        }

        for (int sy = 0; sy < blk; sy++)            // local_d4
        for (int sx = 0; sx < blk; sx++) {          // local_e0
            int cx = bx + sx, cy = by + sy;
            WaterCell* C = &wm->cells[(long)cy*W + cx];  // wm+0x20, stride 0x30
            byte bodyId = C->f2a;     // +0x2a : water body id, 0xff = NOT water
            byte flag2  = C->f2b;     // +0x2b : secondary flag (0xff = interior)
            if (bodyId == 0xff) continue;

            if (flag2 == 0xff)        // interior cell: just bump the body counter
                obj->bodyCounters.data[bodyId]++;

            // build 3x3 occupancy stencil f0[0..8] (center f0[4] forced 1):
            char f0[9];  int t = 0;
            for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                if (dx==0 && dy==0) { f0[t++] = 1; continue; }   // center
                int nx = cx+dx, ny = cy+dy;
                f0[t++] = (nx>=0 && nx<W && ny>=0 && ny<W)
                          ? (wm->cells[(long)ny*W + nx].f2a != 0xff) : 0;
            }
            int shape = e180(0, f0[3],f0[5],f0[1],f0[7], f0[0],f0[2],f0[6],f0[8]);

            Vertex quad[4];
            cc10(obj, cx + halfMap, cy + halfMap, quad, shape, flag2);

            Body60* b = &bodies[bodyId];
            short base = (short)b->vert.count;   // index of first new vertex

            // append 6 indices (2 triangles) with alternating diagonal by parity:
            if (((cx + halfMap + cy + halfMap) & 1) == 0)
                push6(&b->idx, base+0, base+3, base+1,  base+1, base+3, base+2);
            else
                push6(&b->idx, base+0, base+2, base+1,  base+0, base+3, base+2);

            // append the 4 quad vertices to the body's vertex list:
            for (int i = 0; i < 4; i++)
                push_vertex(&b->vert, b->vert.count + 1, &quad[i]);  // 14001f8d0
        }

        // flush this block's bodies that produced any indices to obj+0x200:
        for (int k = 0; k < nBodies; k++)
            if (bodies[k].idx.count > 0)
                fa80(bodyOut, bodyOut->count + 1, &bodies[k]);  // copy Body60 out
    }

    // free working bodies' inner lists + the array
    for (int k = 0; k < nBodies; k++) {
        MemFree(bodies[k].idx.data);  bodies[k].idx.data = 0; bodies[k].idx.cap = 0;
        MemFree(bodies[k].vert.data); bodies[k].vert.data= 0; bodies[k].vert.cap= 0;
    }
    MemFree(bodies.data);
}
```
> The index-buffer pushes in the real binary are **inlined six times** (the giant
> repetitive block in the raw decompile). Each inlined block is exactly the
> grow-by-one + write-`base+k` logic of `push_index` below; `push6` collapses them.
> The two parity branches differ only in the six `base+k` constants:
> - even `(cx+cy) even`: `0,3,1, 1,3,2`  (diagonal v1–v3)
> - odd:                  `0,2,1, 0,3,2`  (diagonal v0–v2)

---

## 2. Full neighbor-stencil -> shape-index table (`e180`)

LUT `kShapeLUT` @ `0x1401c22d0`, 256 bytes. `0xff` (255) = unreachable mask
(diagonal-without-orthogonals already masked, so those bit combos never occur).
Bit weights: `W=1 E=2 N=4 S=8 NW=16 NE=32 SW=64 SE=128`.

Raw 256 bytes (decimal, row = 16 entries):
```
 0  1  2  5  3  6  7 11  4  8  9 12 10 13 14 19
255 255 255 255 255 15 255 20 255 255 255 255 255 21 255 28
255 255 255 255 255 255 16 22 255 255 255 255 255 255 23 29
255 255 255 255 255 255 255 30 255 255 255 255 255 255 255 36
255 17 255 24 255 25 255 31 255 255 255 255 255 255 255 255
255 255 255 255 255 32 255 37 255 255 255 255 255 255 255 255
255 255 255 255 255 255 255 38 255 255 255 255 255 255 255 255
255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 42
255 255 255 255 255 255 255 255 255 255 18 26 255 255 27 33
255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 39
255 255 255 255 255 255 255 255 255 255 255 255 255 255 34 40
255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 43
255 255 255 255 255 255 255 255 255 255 255 35 255 255 255 41
255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 44
255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 45
255 255 255 255 255 255 255 255 255 255 255 255 255 255 255 46
```

Enumerated (only the 47 reachable masks), `mask = decoded neighbors -> shape`:
```
shape  mask  neighbors present
  0     0    (isolated)
  1     1    W
  2     2    E
  3     4    N
  4     8    S
  5     3    W E
  6     5    W N
  7     6    E N
  8     9    W S
  9    10    E S
 10    12    N S
 11     7    W E N
 12    11    W E S
 13    13    W N S
 14    14    E N S
 15    21    W N NW
 16    38    E N NE
 17    65    W SW
 18   138    E S SE
 19    15    W E N S
 20    23    W E N NW
 21    29    W N S NW
 22    39    W E N NE
 23    46    E N S NE
 24    67    W E SW
 25    69    W N SW
 26   139    W E S SE
 27   142    E N S SE
 28    31    W E N S NW
 29    47    W E N S NE
 30    55    W E N NW NE
 31    71    W E N SW
 32    85    W N NW SW
 33   143    W E N S SE
 34   174    E N S NE SE
 35   203    W E S SW SE
 36    63    W E N S NW NE
 37    87    W E N NW SW
 38   103    W E N NE SW
 39   159    W E N S NW SE
 40   175    W E N S NE SE
 41   207    W E N S SW SE
 42   127    W E N S NW NE SW
 43   191    W E N S NW NE SE
 44   223    W E N S NW SW SE
 45   239    W E N S NE SW SE
 46   255    W E N S NW NE SW SE   (fully interior)
```
Interpretation: each cell becomes a quad; the shape index selects one of 47
atlas tiles that draw the correct water/shore boundary for that 8-neighbor
configuration. Shape 0 = isolated puddle, shape 46 = fully-surrounded deep water,
the rest are the edge/corner combinations.

---

## 3. What `cc10` emits per shape

4 vertices = one quad, corner order CCW: `0=(x,y) 1=(x+1,y) 2=(x+1,y+1) 3=(x,y+1)`.
Each vertex carries:
- world pos `(x-halfMap, y-halfMap)`, terrain height sample, packed terrain normal
  (from `caa0`/`e270`),
- a **global** terrain uv at `+0x08/+0x0a` (`x/gridSize*16384`, `y/gridSize*16384`),
- a **tile-atlas** uv at `+0x0c/+0x0e` from the per-shape uv table (overwrites the
  normal-scratch bytes, see layout note).

Per-shape tile-uv table `kShapeUV` @ `0x1401c23d0`, 8 bytes (= 4×uint16 `a,b,c,d`)
per shape, 47 shapes (376 bytes). Corner assignment: `v0=(a,b) v1=(c,b) v2=(c,d)
v3=(a,d)` — i.e. `(a,b)`=min, `(c,d)`=max of an axis-aligned atlas rectangle.

Each value is fixed-point `value/256` tile units; low byte is always `0x80`
(=`+0.5` half-texel offset), high byte = `2*col` or `2*row`. Effectively
`shape s` -> atlas cell `(col=s%8, row=s/8)`, rect `(2*col+0.5 .. 2*col+1.5) ×
(2*row+0.5 .. 2*row+1.5)`. Verbatim uint16 values:
```
// kShapeUV[47][4] = { a, b, c, d }  (raw little-endian uint16)
{0x0080,0x0080,0x0180,0x0180}, {0x0280,0x0080,0x0380,0x0180}, {0x0480,0x0080,0x0580,0x0180}, {0x0680,0x0080,0x0780,0x0180},
{0x0880,0x0080,0x0980,0x0180}, {0x0A80,0x0080,0x0B80,0x0180}, {0x0C80,0x0080,0x0D80,0x0180}, {0x0E80,0x0080,0x0F80,0x0180},
{0x0080,0x0280,0x0180,0x0380}, {0x0280,0x0280,0x0380,0x0380}, {0x0480,0x0280,0x0580,0x0380}, {0x0680,0x0280,0x0780,0x0380},
{0x0880,0x0280,0x0980,0x0380}, {0x0A80,0x0280,0x0B80,0x0380}, {0x0C80,0x0280,0x0D80,0x0380}, {0x0E80,0x0280,0x0F80,0x0380},
{0x0080,0x0480,0x0180,0x0580}, {0x0280,0x0480,0x0380,0x0580}, {0x0480,0x0480,0x0580,0x0580}, {0x0680,0x0480,0x0780,0x0580},
{0x0880,0x0480,0x0980,0x0580}, {0x0A80,0x0480,0x0B80,0x0580}, {0x0C80,0x0480,0x0D80,0x0580}, {0x0E80,0x0480,0x0F80,0x0580},
{0x0080,0x0680,0x0180,0x0780}, {0x0280,0x0680,0x0380,0x0780}, {0x0480,0x0680,0x0580,0x0780}, {0x0680,0x0680,0x0780,0x0780},
{0x0880,0x0680,0x0980,0x0780}, {0x0A80,0x0680,0x0B80,0x0780}, {0x0C80,0x0680,0x0D80,0x0780}, {0x0E80,0x0680,0x0F80,0x0780},
{0x0080,0x0880,0x0180,0x0980}, {0x0280,0x0880,0x0380,0x0980}, {0x0480,0x0880,0x0580,0x0980}, {0x0680,0x0880,0x0780,0x0980},
{0x0880,0x0880,0x0980,0x0980}, {0x0A80,0x0880,0x0B80,0x0980}, {0x0C80,0x0880,0x0D80,0x0980}, {0x0E80,0x0880,0x0F80,0x0980},
{0x0080,0x0A80,0x0180,0x0B80}, {0x0280,0x0A80,0x0380,0x0B80}, {0x0480,0x0A80,0x0580,0x0B80}, {0x0680,0x0A80,0x0780,0x0B80},
{0x0880,0x0A80,0x0980,0x0B80}, {0x0A80,0x0A80,0x0B80,0x0B80}, {0x0C80,0x0A80,0x0C80,0x0A80},
```
Shape 46 (last) is degenerate: `a==c`, `b==d` -> collapsed uv (single deep-water
texel for fully-interior cells).

**Winding** (from the `f370` index pushes, not `cc10`): two triangles per quad,
diagonal alternates by `(cellX+cellY)` parity to remove directional bias:
- even: `(v0,v3,v1)` + `(v1,v3,v2)`
- odd:  `(v0,v2,v1)` + `(v0,v3,v2)`

---

## 4. Layouts

### 4a. WaterCell (the per-cell water-tile map), stride `0x30`
Base pointer: `WaterMap* wm` is `f370`'s `param_2`; cell array ptr = `wm + 0x20`
(`*(longlong*)(param_2+0x20)`). Cell index = `cellY*W + cellX`, `W = wm[0]`
(`*param_2`, the map width). `f370` reads only two fields:

| off | type | meaning |
|-----|------|---------|
| `+0x2a` | byte | **body id**: which water body this cell belongs to. `0xff` = not water. Used as group index into the working `bodies[]` and as the LUT occupancy test (`!=0xff` -> "water present"). |
| `+0x2b` | byte | **secondary flag**: `0xff` = interior cell (no edge geometry; just bumps `bodyCounters[bodyId]`). Non-`0xff` is stored into each emitted vertex's `+0x06` (`Vertex.body`). |

(The other `0x30-2` bytes of the cell are not touched by this cluster.)

### 4b. Output Vertex, stride `0x14` (20 bytes)
This is the final layout in the body vertex list (after `cc10`'s field-copy
relocates the normal). `caa0` fills a 16-byte scratch where the normal sits at
`+0x0c`; `cc10` moves it to `+0x10` and writes tile-uv into `+0x0c`.

| off | type | field |
|-----|------|-------|
| `+0x00` | int16 | posX = `x - halfMap` |
| `+0x02` | int16 | posY = `y - halfMap` |
| `+0x04` | int16 | height (heightfield sample at clamped x,y) |
| `+0x06` | int16 | body / edge flag (`cc10` param_6 = cell `+0x2b`) |
| `+0x08` | int16 | global terrain U = `x/gridSize * 16384` |
| `+0x0a` | int16 | global terrain V = `y/gridSize * 16384` |
| `+0x0c` | int16 | **tile-atlas U** (per-shape, overwrites normal scratch) |
| `+0x0e` | int16 | **tile-atlas V** (per-shape) |
| `+0x10` | uint8 | packed normal X = `(n.x+1)*127.5` |
| `+0x11` | uint8 | packed normal Y = `(n.y+1)*127.5` |
| `+0x12` | uint8 | packed normal Z = `(n.z+1)*127.5` |
| `+0x13` | uint8 | pad (0) |

### 4c. Body60 working entry, stride `0x60` (96 bytes)
`bodies[]` (local, `f250`) and the persisted `obj+0x200` list use the same
element type. Zero-init template is from `f250`'s else-branch.

| off | type | field |
|-----|------|-------|
| `+0x00` | ptr(8) | `desc` — copied from `obj->bodyDescs` (`obj+0x210`)[bodyId] (per-body material/descriptor) |
| `+0x08` | ptr | vertex list `.data` (Vertex[], stride 0x14) |
| `+0x10` | int  | vertex list `.count` |
| `+0x14` | int  | vertex list `.cap` |
| `+0x18` | ptr  | index list `.data` (uint16[], stride 2) |
| `+0x20` | int  | index list `.count` |
| `+0x24` | int  | index list `.cap` |
| `+0x28` | int  | `0xffffffff` in template (unused by this cluster) |
| `+0x2c` | int  | originX (`blockX + halfMap`) |
| `+0x30` | int  | originY (`blockY + halfMap`) |
| `+0x34` | int  | maxX (`blockSize + blockX + halfMap`) |
| `+0x38` | int  | maxY (`blockSize + blockY + halfMap`) |
| `+0x3c..+0x5d` | — | additional scratch zeroed by template (bbox/AABB, extra lists) |

Two list-headers per body: a `{ptr,count,cap}` 16-byte header at `+0x08`
(vertices) and at `+0x18` (indices). The std list grow rule (shared by f250/fa80/
f8d0/c890/ee40/0021d0): when `count` exceeds `cap`, set `cap = count`, but if it's
exactly a `+1` growth from a non-zero size, set `cap = count*3` (amortized growth),
realloc, copy, free old.

### 4d. GeoObj fields read by the cluster (offsets off `param_1`)
| off | meaning |
|-----|---------|
| `+0xC0` | gridSize (full). `halfMap = (gridSize - obj+0xC4) >> 1`; also uv denom in caa0 |
| `+0xC4` | secondary grid offset (subtracted before `>>1`) |
| `+0xC8` | blockSize (200 dec) — map is tiled into blocks of this many cells |
| `+0xD0` | heightfield dim (square) — clamp bound in caa0/e270 |
| `+0xD8` | heightfield data `short[]` |
| `+0x200` | output scratch list header `{ptr,count,cap}` (Body60 elems) — consumed by fb00 |
| `+0x210` | per-body descriptor array (8 bytes each) |
| `+0x218` | nBodies (count of water bodies) |
| `+0x220` | per-body cell-counter list header `{ptr,count,cap}` (int[]) |

---

## 5. `.rdata` constants (actual values)

| symbol | addr | bytes (LE) | value | use |
|--------|------|-----------|-------|-----|
| `kShapeLUT` | `0x1401c22d0` | 256 bytes | (table §2) | 8-neighbor mask -> shape index |
| `kShapeUV`  | `0x1401c23d0` | 376 bytes | (table §3) | per-shape 4-corner atlas uv (uint16 a,b,c,d) |
| `g_fUVScale` | `0x14017214c` | `00 00 80 3f` | **1.0** | normal pack bias `(n + 1.0)` in caa0 |
| `DAT_140172214` | `0x140172214` | `00 00 ff 42` | **127.5** | normal pack scale `*127.5` |
| `DAT_140172264` | `0x140172264` | `00 00 80 46` | **16384.0** | global uv scale |
| `DAT_140172174` | `0x140172174` | `00 00 00 40` | **2.0** | normal.z constant in e270 |
| `_DAT_140172270` | `0x140172270` | `00 00 80 bb` | **-0.00390625** (= -1/256) | heightfield slope scale in e270 |

---

## 6. How the tiling output is stored (for `fb00`)

`f370` writes nothing to `+0x200` directly per cell. Instead, per **block**
(`blockSize × blockSize` cells) it builds geometry into `nBodies` local `Body60`
working entries (grouped by cell `+0x2a` body id), then for every body that
produced any indices, it **appends a full `Body60` copy** to `obj->scratch200`
(`+0x200`) via `fa80` (which deep-copies the entry incl. its vertex/index list
pointers — `f5e0` is the copy-ctor). So after `f370`:

```
obj+0x200 = List<Body60> {
    Body60 {
        desc      (per-body material/descriptor, from obj+0x210)
        vertList  -> Vertex[]  (stride 0x14, layout §4b)
        idxList   -> uint16[]  (2-tri-per-quad indices, §3 winding)
        originX/Y, maxX/Y      (block AABB in centered coords)
    } ...   // one per (block × body) that had geometry
}
```
`fb00` iterates this list to extract contour vertices + uv (the tile uv at
`+0x0c/+0x0e` and/or global uv at `+0x08/+0x0a`, plus pos `+0x00/+0x02` and
height `+0x04`).

---

## 7. C++ port sketch

```cpp
// ---- constants (verbatim from .rdata) ----
static const float kNormalBias  = 1.0f;        // 0x14017214c
static const float kNormalScale = 127.5f;      // 0x140172214
static const float kGlobalUV    = 16384.0f;    // 0x140172264
static const float kNormalZ     = 2.0f;        // 0x140172174
static const float kSlopeScale  = -1.0f/256.0f;// 0x140172270

static const int8_t kShapeLUT[256] = { /* §2 raw bytes; 0xff = unused */ };
static const uint16_t kShapeUV[47][4] = { /* §3 a,b,c,d */ };

struct Vertex {            // 0x14
    int16_t posX, posY, height, body;   // +0x00..+0x06
    int16_t u, v;                       // +0x08 global uv
    int16_t tileU, tileV;               // +0x0c tile-atlas uv (overwrites normal scratch)
    uint8_t nx, ny, nz, pad;            // +0x10
};
struct List { void* data; int count, cap; };   // generic {ptr,count,cap}
struct Body60 {            // 0x60
    void* desc;            // +0x00
    List  vert;            // +0x08 (Vertex, stride 0x14)
    List  idx;             // +0x18 (uint16, stride 2)
    int   pad28;           // +0x28 (0xffffffff template)
    int   originX, originY, maxX, maxY;  // +0x2c..+0x38
    uint8_t scratch[0x60-0x3c];          // remainder
};

static inline int clampi(int v,int lo,int hi){return v<lo?lo:(v>hi?hi:v);}

static int pickShape(const char f0[9]) {       // e180
    char W=f0[3],E=f0[5],N=f0[1],S=f0[7],NW=f0[0],NE=f0[2],SW=f0[6],SE=f0[8];
    if (NW && (!W||!N)) NW=0;
    if (NE && (!E||!N)) NE=0;
    if (SW && (!W||!S)) SW=0;
    if (SE && (!E||!S)) SE=0;
    int m = (W?1:0)|(E?2:0)|(N?4:0)|(S?8:0)|(NW?16:0)|(NE?32:0)|(SW?64:0)|(SE?128:0);
    return kShapeLUT[m];                        // -1 (0xff) never occurs for valid stencils
}

static void cornerVertex(GeoObj* o, int x, int y, Vertex* out, uint8_t flag2) { // caa0
    int half = o->gridSize >> 1;
    Vec3 n = terrainNormal(o, x, y);           // e270
    out->posX = (int16_t)x - half;
    out->posY = (int16_t)y - half;
    int dim = o->hmDim;
    out->height = o->hmData[ (long)dim*clampi(y,0,dim-1) + clampi(x,0,dim-1) ];
    out->body = flag2;
    out->u = (int16_t)((float)x / o->gridSize * kGlobalUV);
    out->v = (int16_t)((float)y / o->gridSize * kGlobalUV);
    out->nx = (uint8_t)((n.x + kNormalBias) * kNormalScale);
    out->ny = (uint8_t)((n.y + kNormalBias) * kNormalScale);
    out->nz = (uint8_t)((n.z + kNormalBias) * kNormalScale);
    out->pad = 0;
}

static void emitQuad(GeoObj* o, int cx, int cy, Vertex q[4], int shape, uint8_t flag2) { // cc10
    static const int dx[4]={0,1,1,0}, dy[4]={0,0,1,1};
    for (int i=0;i<4;i++) cornerVertex(o, cx+dx[i], cy+dy[i], &q[i], flag2);
    const uint16_t* uv = kShapeUV[shape];      // a,b,c,d
    q[0].tileU=uv[0]; q[0].tileV=uv[1];
    q[1].tileU=uv[2]; q[1].tileV=uv[1];
    q[2].tileU=uv[2]; q[2].tileV=uv[3];
    q[3].tileU=uv[0]; q[3].tileV=uv[3];
}

// list grow: cap=count, but +1-from-nonzero => cap=count*3; realloc/copy/free.
// alloc via fn_MemAlloc(singleton, count*stride, align); free via fn_MemFreeStatic.
```
The per-cell driver mirrors §1e: build `f0[9]` from the `WaterCell.f2a != 0xff`
8-neighborhood (center forced 1), `shape=pickShape`, `emitQuad`, append 6 indices
with the parity-selected winding, append 4 vertices, then flush non-empty bodies
into `obj+0x200`.

### Port gotchas
- Diagonal masking in `e180` happens **before** the LUT index is built — don't
  skip it or you'll index unreachable `0xff` slots.
- The `caa0` normal lands at vertex `+0x0c` but `cc10` relocates it to `+0x10`
  and reuses `+0x0c/+0x0e` for tile uv. Keep both uv sets.
- Two uv channels per vertex: global (`+0x08`) and tile-atlas (`+0x0c`).
- Index winding flips by `(cx+cy)` parity (computed in *centered* coords:
  `cellX+halfMap + cellY+halfMap`, same parity as `cellX+cellY` since halfMap
  cancels mod 2).
- `gridSize`(+0xC0) vs `hmDim`(+0xD0) are different dimensions — `halfMap` and uv
  use gridSize; heightfield sampling/clamping uses hmDim.
- Heightfield slope uses `-1/256` and a fixed `z=2.0` then normalizes.
```
```
