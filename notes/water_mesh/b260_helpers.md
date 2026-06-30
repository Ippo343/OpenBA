# Water contour-mesh (TradeWork_impl_b260 @ 0x1400ab260) — list/dedup/vector helpers

Read-only Ghidra analysis of Application-x64.exe (base 0x140000000). All offsets verified
against decompilation + disassembly. These helpers manage the dynamic vertex/index
accumulators and the per-feature range arrays used by the contour water-mesh builder.

---

## 0. The geometry object (`param_1`, the mesh accumulator) layout

`b260(longlong geomObj /*param_1*/, longlong feature /*param_2*/)`.
The accumulator object holds two growable arrays plus two range arrays, all using the
**same "resize-to-N, grow capacity ×3" idiom** (see §1):

| field   | meaning                                              |
|---------|------------------------------------------------------|
| +0x1a0  | **vertex array** base ptr (stride 0x10 = 16-byte verts) |
| +0x1a8  | vertex count                                         |
| +0x1ac  | vertex capacity                                      |
| +0x1b0  | **index array** base ptr (stride 0x02 = int16 indices)  |
| +0x1b8  | index count                                          |
| +0x1bc  | index capacity                                       |
| +0x1c0  | **range array A** base ptr (stride 0x10 = 4×int32 entry) |
| +0x1c8  | range A count                                        |
| +0x1cc  | range A capacity                                     |
| +0x1d0  | **range array B** base ptr (stride 0x10 = 4×int32 entry) |
| +0x1d8  | range B count                                        |
| +0x1dc  | range B capacity                                     |

### Range-entry layout (CONFIRMED, 0x10 bytes = 4 × int32)
From the b260 write site after `TradeWork_impl_fb00(...)`:
```
*(u32*)(rangeBase + off + 0x00) = *(u32*)(geom + 0x1b8);  // idxStart  (current index count)
*(u32*)(rangeBase + off + 0x04) = *(u32*)(geom + 0x1a8);  // vtxStart  (current vertex count)
*(u32*)(rangeBase + off + 0x08) = (u32)local_1e0;         // vtxCount  (verts fb00 produced)
*(u32*)(rangeBase + off + 0x0c) = (u32)local_1f0;         // idxCount  (indices fb00 produced)
```
So a range entry is `struct Range { int32 idxStart, vtxStart, vtxCount, idxCount; }` — matches
the spec exactly. Range A (+0x1c0) is pre-sized once at b260 entry to `feature+0x20` entries via
`sub_efd0`. (Range B at +0x1d0 is the second, per-body, range list; same entry shape.)

---

## 1. The shared "resize vector to N" idiom

Every `*_resize(container, N [, fillSrc])` helper below is the SAME generated template,
differing only in element **stride** and whether new slots are **filled** from a source,
**copied-only**, or **constructed/destructed**. Container is a 16-byte header:

```
struct Vec { void* data; int32 count; int32 capacity; };  // ptr@+0, count@+8, cap@+0xC
```

Algorithm (`resize(Vec* v, int newCount, src?)`):
```
old = v->count;
if (newCount == old) return;            // no-op
oldData = v->data;
v->count = newCount;
if (v->capacity < newCount) {           // GROW
    v->capacity = newCount;
    if (newCount == old + 1 && old != 0) // appending exactly one to a non-empty vec...
        v->capacity = newCount * 3;      // ...over-allocate to 3× (amortized growth)
    v->data = MemAlloc(capacity * STRIDE, ALIGN);
}
// keepCount = (data pointer unchanged) ? old : 0   — i.e. if no realloc, keep existing prefix
start = (oldData == v->data) ? old : 0;
for (i = start; i < newCount; ++i) {
    dst = v->data + i*STRIDE;
    if (i < old) copy element i from oldData       // surviving element (realloc case)
    else         copy/fill element i from src      // new tail element (the "append" value)
}
if (oldData != v->data) MemFree(oldData);          // free old buffer after realloc
```
**Append-one usage:** `resize(v, v->count + 1, &newElem)` → grows by 1 (capacity jumps to
`3*(count+1)`), then the single new tail slot is initialized from `&newElem`. This is exactly
how b260 pushes each vertex (inlined) and each index.

Note: the **3× over-alloc only triggers on the +1 append path** (`newCount==old+1 && old!=0`).
A bulk resize (e.g. `sub_efd0(range, feature+0x20)`) sets capacity = newCount with no ×3.

---

## 2. Helper-by-helper

### `TradeWork_impl_sub_74a0` @ 0x1400074a0 — **append/resize, 16-byte verts, FILL**
`void sub_74a0(Vec* v, int newCount, void* src16)`
- STRIDE 0x10, ALIGN 2. New tail slots filled from `src16` (copies 2×u64 = 16 bytes).
- This is the **vertex accumulator append** (`v->data` = geom+0x1a0). `f3e0` calls it as
  `sub_74a0(builder, builder.count+1, &vertex)` to push one dedup'd vertex.
- Return: void. (The index is returned by the caller `f3e0`, not here.)

### `TradeWork_impl_f3e0` @ 0x14001f3e0 — **vertex add + dedup → returns index** (KEY helper)
`uint32 f3e0(IndexedVertexSet* set /*param_1*/, void* vtx16 /*param_2*/)`
- `param_1` (the b260 local `local_1c8`) is a combined struct:
  - +0x00 vertex Vec header (ptr/count/cap) — the dedup'd vertex array
  - +0x10 (`param_1[2]`) red-black-tree **root** (key→index map)
  - +0x18 (`param_1[3]`) tree node count
- **RB-tree node = 0x38 bytes**, allocated `MemAlloc(0x38, 8)`:
  | off  | field                                  |
  |------|----------------------------------------|
  | 0x00 | key: 16-byte vertex (copied from vtx16)|
  | 0x10 | **index** (int32) = set->count at insert |
  | 0x18 | parent ptr                             |
  | 0x20 | left child ptr                         |
  | 0x28 | right child ptr                        |
  | 0x30 | RB color / balance byte                |
- Algorithm: walk the tree using comparator `sub_ec10` (strict-less `operator<`).
  - If an existing node matches (`!(a<b) && !(b<a)`), **return its stored index** (`node+0x10`).
  - Else insert a new node (parent linked at +0x18, child at +0x20/+0x28), set its index to the
    current `set->count`, rebalance via `TradeWork_impl_4b40(&root, &newNode)` (RB fix-up),
    increment tree node count (`param_1[3]`), then **append the vertex** via
    `sub_74a0(set, set->count+1, vtx16)` (which bumps `set->count`).
  - **Return value = the vertex index** (existing on hit, freshly-assigned on insert).
- Net: this is `std::map<Vertex,uint16>::insert`-style **vertex welding**: identical verts share
  one index; the index is a monotonically increasing slot number into the vertex array.

### `TradeWork_impl_f3e0_sub_ec10` @ 0x14001ec10 — **vertex-key comparator (`operator<`)**
`bool sub_ec10(short* a /*param_1*/, longlong b /*param_2*/)` → returns true iff `a < b`.
- Lexicographic compare of the 16-byte key in field-priority order: 4 shorts at [0..7], a byte
  group at [0xc..0xf], then 2 shorts at [8..0xb]. (Ghidra renders it as two interleaved loops;
  the meaning is "strict weak ordering over the packed vertex bytes".) Used only by `f3e0`.

### `TradeWork_impl_4b40` @ 0x14001f4b40 — RB-tree insert rebalance (recolor/rotate). Standard
red-black fix-up on `(&root, &node)`; not ported in detail (mirror std::map rebalance or a flat
insert — only ordering of welding matters, not tree shape).

### `FUN_140014b00` @ 0x140014b00 — **recursive tree free**
`void free_tree(void* /*ignored allocator arg*/, void* node)`
- `if(node){ free_tree(node->left@+0x20); free_tree(node->right@+0x28); MemFree(node); }`
- The destructor for the `f3e0` dedup RB-tree (note the +0x20/+0x28 child offsets match the node
  layout above). param_1 is an unused/ignored allocator handle.

### `TradeWork_impl_sub_c890` @ 0x14001c890 — **resize, int16 array, COPY-only**
`void sub_c890(Vec* v, int newCount)` — STRIDE 0x02, ALIGN 2. Copies surviving prefix only; new
slots left uninitialized (caller fills). This is the **index-array resize** (geom+0x1b0). (b260
also inlines a fill version when pushing indices.)

### `FUN_1400021d0` @ 0x1400021d0 — **resize, int32 array, COPY-only**
`void resize_i32(Vec* v, int newCount)` — STRIDE 0x04, ALIGN 4. Generic int32 vector resize.

### `TradeWork_impl_b260_sub_efd0` @ 0x14001efd0 — **resize, 0x10 (4×int32) array, COPY-only**
`void sub_efd0(Vec* v, int newCount)` — STRIDE 0x10, ALIGN 4. Copies 4×int32 per surviving entry.
This is the **range-array resize** (geom+0x1c0 / +0x1d0). Confirms range entry = 4 int32 = 0x10 B.
**Matches our existing port** in `code/Game/Game/serialize_resize_helpers.cpp` (resize a
+0x1c0/+0x1d0 range array to N entries; align 4, stride 16, copy-only, no fill/ctor). Note ALIGN is
**4** here vs ALIGN **2** for the vertex array (sub_74a0) — keep the alignment per-helper.

### `TradeWork_impl_sub_ee40` @ 0x14001ee40 — **resize, 0x14 (20-byte) array, COPY-only**
`void sub_ee40(Vec* v, int newCount)` — STRIDE 0x14, ALIGN 2. Copies 20 bytes/entry (u64+u64+u32),
surviving prefix only.

### `FUN_14001f8d0` @ 0x14001f8d0 — **append/resize, 0x14 (20-byte) array, FILL**
`void f8d0(Vec* v, int newCount, void* src20)` — STRIDE 0x14, ALIGN 2. Same as ee40 but new tail
slots are filled from `src20` (copies 20 bytes). The "append a 20-byte tile/edge record" helper.

### `TradeWork_impl_fa80` @ 0x14001fa80 — **resize, 0x60 (96-byte) OBJECT array, FILL + ctor/dtor**
`void fa80(Vec* v, int newCount, void* src96)` — STRIDE 0x60, ALIGN 8.
- Surviving + new elements are **copy-constructed** via `sub_f5e0(dst, srcElem)` (not memcpy —
  the element owns sub-objects, see §3). Removed tail (shrink) or the whole old buffer (realloc)
  is **destructed** via `sub_f250_sub_fb80(...)` before `MemFree`. New tail filled from `src96`.

### `TradeWork_impl_sub_f250` @ 0x14001f250 — **resize, 0x60 object array, ZERO-INIT new + ctor/dtor**
`void sub_f250(Vec* v, uint newCount)` — STRIDE 0x60, ALIGN 8.
- Like fa80 but **new tail elements are zero-initialized** (a fresh default-constructed 0x60
  object: most fields 0, `+0x28`=int -1 (0xffffffff)) instead of copied from a source. Surviving
  elements copy-constructed via `sub_f5e0`; shrunk/old elements destructed via `sub_f250_sub_fb80`.
- Default 0x60 object init (the "fresh element" template):
  ```
  bytes 0x00..0x27 = 0;            // includes an embedded vec/list header zeroed
  *(int*)(+0x28)   = -1;           // (0xffffffff) — likely an id/index sentinel
  bytes 0x2c..0x5d = 0;            // remaining fields/flags zeroed
  ```

### `TradeWork_impl_sub_f5e0` @ 0x14001f5e0 — **0x60 object copy-constructor**
`void* sub_f5e0(void* dst, void* src)` — field-wise copy of a 96-byte object. Plain scalar fields
are copied directly; two embedded owning sub-objects are deep-copied via `TradeWork_helper_fdb0`
(`dst+0x08`, an embedded vec) and `TradeWork_helper_fe60` (`dst+0x18`). Used by fa80/f250 as the
element copy ctor. (`sub_f250_sub_fb80` is the matching array destructor — frees those sub-objects
then the entries.)

### `FUN_140132010` @ 0x140132010 — **`float sqrtf(float)`** (CONFIRMED — NOT a vector length)
IEEE-correct scalar square root with denormal/inf/NaN/negative special-casing (delegates edge
cases to the CRT `sqrtf` errno path `CharacterComponent_helper_3e1c("sqrtf",...)`, otherwise
`SQRT(x)`). Port as `std::sqrtf` / `sqrtf`. (b260 uses it for shore-ring distance/length math.)

---

## 3. `.rdata` constants used in b260 phase-2 vertex packing (verified via read_memory)

| symbol          | addr        | bytes (LE)  | float value | role |
|-----------------|-------------|-------------|-------------|------|
| DAT_1401721e4   | 0x1401721e4 | 00 00 40 42 | **48.0**    | `fVar10` — X,Y position scale (`x*48`, `y*48`). Mirrors the water-mesh "×48" coord scale. |
| DAT_140172250   | 0x140172250 | 00 00 80 44 | **1024.0**  | `fVar8`  — secondary coord scale for the 2nd short pair (`*1024`). |
| DAT_1401722a4   | 0x1401722a4 | 00 00 40 c3 | **-192.0**  | `fVar9`  — row/Z base: `rowIdx = (int)(-192.0 - column)`. |

### Inlined vertex pack (b260, per shore/detail vertex, 16 bytes):
```
short  x = (short)(src[0] * 48.0);     // +0x00
short  y = (short)(src[1] * 48.0);     // +0x02
short  z = (short)(-192.0 - column);   // +0x04   (row index)
short  w = 0;                          // +0x06
short  u = (short)(src[2] * 1024.0);   // +0x08
short  v = (short)(src[3] * 1024.0);   // +0x0a
u16    c0 = 0x7f7f;                    // +0x0c   (two bytes 0x7f,0x7f)
u8     c1 = 0xff;                      // +0x0e
u8     pad= 0;                         // +0x0f
```
(`src` = `local_1e8` heightmap ring output advanced by 16 bytes/iter; `column = (uint)param`.)

---

## 4. C++ port sketch (conventions: plain structs, int16 indices, fn_MemAlloc/fn_MemFreeStatic)

```cpp
struct Vec { void* data; int32_t count; int32_t cap; };

// Generic resize template; copyFn handles per-element copy (or nullptr = raw memcpy).
template <int STRIDE, int ALIGN>
static void VecResize(Vec* v, int n, const void* fillSrc,
                      void(*copyFn)(void*,const void*) = nullptr,
                      void(*dtorRange)(void*,int) = nullptr) {
    int old = v->count;
    if (n == old) return;
    void* oldData = v->data;
    v->count = n;
    if (v->cap < n) {
        v->cap = n;
        if (n == old + 1 && old != 0) v->cap = n * 3;   // append-one → 3× growth
        v->data = fn_MemAlloc(g_alloc, (size_t)v->cap * STRIDE, ALIGN);
    }
    int start = (oldData == v->data) ? old : 0;
    for (int i = start; i < n; ++i) {
        char* dst = (char*)v->data + (size_t)i*STRIDE;
        const void* src = (i < old) ? (const char*)oldData + (size_t)i*STRIDE : fillSrc;
        if (i < old || fillSrc) { copyFn ? copyFn(dst, src) : memcpy(dst, src, STRIDE); }
        // else: caller-fills (copy-only helpers) / zero-init (f250) — handle by copyFn variant
    }
    if (oldData != v->data) { if (dtorRange) dtorRange(oldData, old); fn_MemFreeStatic(oldData); }
}

// sub_74a0  : VecResize<0x10,2>(v, n, src16)                 // vertex append (fill)
// sub_c890  : VecResize<0x02,2>(v, n, nullptr)               // int16 index (copy-only)
// 0021d0    : VecResize<0x04,4>(v, n, nullptr)               // int32 (copy-only)
// sub_efd0  : VecResize<0x10,4>(v, n, nullptr)               // range entry 4×int32 (copy-only)  [already ported]
// sub_ee40  : VecResize<0x14,2>(v, n, nullptr)               // 20-byte (copy-only)
// f8d0      : VecResize<0x14,2>(v, n, src20)                 // 20-byte append (fill)
// fa80      : VecResize<0x60,8>(v, n, src96, ObjCopy, ObjDtorRange)   // 96-byte obj append
// f250      : like fa80 but new tail = ObjDefaultInit (zero + (+0x28)=-1)

struct RbNode { uint8_t key[16]; uint32_t index; uint32_t _pad;
                RbNode* parent; RbNode* left; RbNode* right; uint8_t color; };  // 0x38

struct IndexedVertexSet { Vec verts; RbNode* root; int32_t nodeCount; };       // 0x20

static uint32_t WeldVertex(IndexedVertexSet* s, const void* v16) {
    RbNode* n = s->root; RbNode* parent = nullptr; bool goLeft = false;
    while (n) {
        if (KeyLess(v16, n->key))      { parent=n; n=n->left;  goLeft=true; }
        else if (KeyLess(n->key, v16)) { parent=n; n=n->right; goLeft=false; }
        else return n->index;                 // dedup hit
    }
    RbNode* node = (RbNode*)fn_MemAlloc(g_alloc, sizeof(RbNode), 8);
    memcpy(node->key, v16, 16); node->index = (uint32_t)s->verts.count;
    node->parent=parent; node->left=node->right=nullptr; node->color=0;
    if (!parent) s->root = node;
    else (goLeft ? parent->left : parent->right) = node;
    RbInsertFixup(&s->root, node);            // 4b40
    s->nodeCount++;
    uint32_t idx = node->index;
    VecResize<0x10,2>(&s->verts, s->verts.count + 1, v16);   // sub_74a0 append
    return idx;
}

static void FreeTree(RbNode* n){ if(n){ FreeTree(n->left); FreeTree(n->right); fn_MemFreeStatic(n);} } // 14b00

// constants
static const float kPosScaleXY = 48.0f;   // DAT_1401721e4
static const float kUVScale    = 1024.0f; // DAT_140172250
static const float kRowBase    = -192.0f; // DAT_1401722a4
static inline float fsqrt(float x){ return sqrtf(x); }   // FUN_140132010
```

---

## Quick classification table

| fn @ addr            | stride | what it is                                       |
|----------------------|--------|--------------------------------------------------|
| f3e0   @14001f3e0    | —      | vertex weld/dedup via RB-tree → returns index    |
| sub_74a0 @1400074a0  | 0x10   | vertex append (resize+fill), align 2             |
| sub_c890 @14001c890  | 0x02   | int16 index resize (copy-only)                   |
| 0021d0 @1400021d0    | 0x04   | int32 resize (copy-only)                          |
| sub_efd0 @14001efd0  | 0x10   | range-entry(4×i32) resize (copy-only) — port ✓   |
| sub_ee40 @14001ee40  | 0x14   | 20-byte resize (copy-only)                        |
| f8d0   @14001f8d0    | 0x14   | 20-byte append (resize+fill)                      |
| fa80   @14001fa80    | 0x60   | 96-byte object append (copy-ctor/dtor + fill)     |
| sub_f250 @14001f250  | 0x60   | 96-byte object resize (copy-ctor/dtor + zero-new) |
| sub_f5e0 @14001f5e0  | —      | 96-byte object copy-constructor                   |
| sub_ec10 @14001ec10  | —      | vertex-key `operator<` comparator                 |
| 4b40   @14001f4b40   | —      | RB-tree insert rebalance                          |
| 14b00  @140014b00    | —      | recursive RB-tree free                            |
| 132010 @140132010    | —      | `float sqrtf(float)` (scalar sqrt)                |
