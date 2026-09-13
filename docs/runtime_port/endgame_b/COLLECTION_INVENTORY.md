# ENDGAME B.3.2 — Collection templates inventory (read-only, 2026-09-13)

Scope: `Collection::Array<T>`, `Collection::Set<K,V>` (AVL), `Collection::LinkedList<T>`.
No tree file edited. Evidence: `runtime/types/collection/Collection.h`,
`runtime/src/collection/*.cpp`, `docs/runtime_port/ENDGAME.md` §B.1 decision 3, §B.2, §B.4,
`docs/runtime_port/endgame_b/B31_RULES.md`, `SMARTPTR_INVENTORY.md`.

## 1. Current shape

### 1.1 Layouts (`runtime/types/collection/Collection.h`, 359 lines)

| type | size | fields | status |
|---|---|---|---|
| `Array<T>` | 0x10 | `_data`(0x00) `_size`(0x08 i32) `_capacity`(0x0c i32) | CONFIRMED, T-independent size |
| `LinkedList<T,Alloc>` | 0x18 | `_head`(0x00) `_tail`(0x08) `_count`(0x10) `_pad14`(0x14, never touched) | CONFIRMED |
| `LinkedList::Node` | align8(sizeof T)+0x10 | `element`(0x00) `prev` `next` | CONFIRMED |
| `LinkedList::Iterator` | 8 | one node pointer | CONFIRMED |
| `Set<T,Alloc,Key,Cmp>` | 0x10 | `_root`(0x00) `_count`(0x08 i32) `_pad0c`(0x0c, unpinned) | CONFIRMED |
| `Set::Node` | align8(sizeof T)+0x20 | `element`(0x00) `parent` `left` `right` `balance`(i16) `height`(i16) `_padTail`(u32) | CONFIRMED, 3 element widths verified (8/0x10/0x18) |
| `DefaultAllocator` | 1 | none, stateless policy | CONFIRMED |
| `AVLCompare` | 1 | none, stateless, lexicographic byte compare, NULL < everything | CONFIRMED |

`Collection::Map` does **not exist** in retail — every "Map" is `Set<Pair<K,V>>` with
`Key == T` (Collection.h:34-40). Do not introduce a Map template.

`Set::Node::balance = rightHeight - leftHeight` (note the sign); `Set::Iterator` layout
is a GUESS (never observed by value) shaped after `LinkedList::Iterator`.

### 1.2 `runtime/src/collection/*.cpp` — line counts and role

| file | lines | role |
|---|---|---|
| `array.cpp` | 4331 | `SetSizeCore` (the one grow/shrink algorithm), ~27 `ArraySpec` rows (`kSpec_*`), most `RegisterRuntimeAddr` rows |
| `array_destroy.cpp` | 1884 | `ArrayDtorImpl` + per-element destroy helpers (the other half of every `ArraySpec::destroy`) |
| `array_graphics_anim.cpp` | 459 | non-trivial anim-element `ArraySpec`s (reuses `rtcoll::SetSizeCore`) |
| `array_graphics_mesh.cpp` | 371 | non-trivial mesh-element `ArraySpec`s (reuses `rtcoll::SetSizeCore`) |
| `avl_trees.cpp` | 1262 | keyed `Set::Insert` entry points (3 instantiations) + recursive subtree free; per-instantiation because key compare + element copy are baked in |
| `set_avl.cpp` | 430 | 4 element-type-AGNOSTIC AVL primitives (retrace, 2 rotations, erase) — one body serves every `Set<T>` whose node header layout matches |
| `set_avl_e24.cpp` | 394 | AVL primitives for the T=0x18 node width (Input::KeyMap key set) |
| `set_avl_pathid.cpp` | 380 | AVL primitives for a further keyed instantiation (path/id sets) |
| `set_clear.cpp` | 214 | `Set<Pair<String,String>>::Clear` + `DestroySubtree`/`DestroyElement` |
| `linkedlist.cpp` | 359 | 3 unexported helpers behind `UI::UserInterface::BringDialogToFront` (Remove-by-value, node alloc, allocator-lock free) |
| `introsort.cpp` | 458 | packfile sort (0x1800264a0, transcribed instruction-by-instruction, descending, single 32-bit-field comparator, no tiebreak) + 2 leftover element copy-assign helpers |
| `README.md` | 21 | says original source is `Collection/Array.h` + `Allocator.h` only (0 cpp files) — confirms everything here is MSVC-monomorphized inline-template output, not hand-written retail .cpp |

Total ported: **10,563 lines** across 11 files for 3 conceptual container types.

### 1.3 The `ArraySpec` table mechanism (`runtime/src/collection/array_internal.h`)

```cpp
using ElemAssign  = void (*)(uint8_t* dst, const uint8_t* src);
using ElemInit    = void (*)(uint8_t* dst);
using ElemDestroy = void (*)(void* ignoredThis, uint8_t* base, int count);

struct ArraySpec {
    const char* name;
    uint32_t    stride; int align;
    ElemAssign  assign;   // never null
    ElemInit    init;     // null => retail leaves new tail UNINITIALISED
    ElemDestroy destroy;  // null => trivially destructible T
    bool        byteCopy; // true when `assign` is a pure byte copy
    uint32_t    assignUnwrittenLo, assignUnwrittenHi;  // per-arm unwritten byte range
    uint32_t    initUnwrittenLo,   initUnwrittenHi;
    uint32_t    seedZeroLo,  seedZeroHi;                // probe-only seeding
};
```

One `ArraySpec` per retail element type/instantiation; `SetSizeCore(ArrayHdr*, int newSize,
const uint8_t* fill, const ArraySpec* spec)` is the single retail growth algorithm
(exact-equal no-op, grow-by-1-from-nonempty trebles capacity else exact-fit, copy old
range via `assign`, fill/init the new tail, destroy the shrunk tail) driving all of them.
`ArrayDtorImpl(ArrayHdr*)` is the matching single destructor algorithm. This IS decision
3's target shape already, just not yet exposed as `template<class T> class Array`.

**Distinct element types / instantiations found** (rough count from `kSpec_*` declarations
in `array_internal.h` + banners in `array.cpp`/`array_graphics_*.cpp`):

| category | count | examples |
|---|---|---|
| trivial, byteCopy, no init (tail uninitialised) | 2 | `kSpec_Bool`, `kSpec_I32` |
| trivial, zero-init tail | ~6 | `kSpec_U64Zero`, `kSpec_PtrZero`, `kSpec_Transform`, `kSpec_Fill8Z`, `kSpec_StringRefZ`, `kSpec_U64ZeroA4` |
| trivial, non-zero fill | ~6 | `kSpec_I32Neg1`, `kSpec_Fill8`, `kSpec_Fill4`, `kSpec_Fill16`, `kSpec_Fill16A4`, `kSpec_Node18` |
| NON-TRIVIAL (assign is a call, and/or destroy is non-null) | ~11 | `kSpec_Sprite38`(+`Sprite38Z`), `kSpec_EffectSlot`, `kSpec_StringExt`, `kSpec_StringRef`, `kSpec_GraphSeries`, `kSpec_PkgEntry`, `kSpec_Window`, `kSpec_Node18`(2-arg variant), `kSpec_VideoResource`, `kSpec_ExtArrayElem`, `kSpec_StringPair10` |
| graphics-only specs (separate files) | ~5 | anim/mesh element specs in `array_graphics_anim.cpp`/`array_graphics_mesh.cpp` |
| AVL `Set` node-header instantiations (element-agnostic AVL core) | 3 element widths | T=8 (`set_avl.cpp`), T=0x18 (`set_avl_e24.cpp`), T=0x10/pathid (`set_avl_pathid.cpp`) |
| AVL keyed `Insert` entry points (per-key-type, non-shareable) | 3 | `Set<Pair<u32,TypeRecord>>`, `Map<String,String>`, `Set<Pair<Filename,u64>>` |
| `LinkedList` instantiation | 1 | T=8 (`UI::UserInterface::_dialogs`) |

**Registered entry points**: `RegisterRuntimeAddr` (never `RegisterRuntimeExport` — all 27+
Array dtor/SetSize/SetSizeFill addresses are unexported inline-template instantiations,
Collection.h/array.cpp banner) counts per file:

| file | `RegisterRuntime*` rows |
|---|---|
| `array.cpp` | 171 |
| `array_destroy.cpp` | 82 |
| `array_graphics_anim.cpp` | 19 |
| `array_graphics_mesh.cpp` | 10 |
| `avl_trees.cpp` | 26 |
| `set_avl_pathid.cpp` | 8 |
| `set_avl_e24.cpp` | 7 |
| `linkedlist.cpp` | 4 |
| `set_avl.cpp` | 6 |
| `set_clear.cpp` | 3 |
| `introsort.cpp` | 3 |
| **total** | **339 registered addresses** |

That is the floor for "how many address-driven registry rows decision 3 removes" — every
one of the ~27 `ArrayDtor_*`/`ArraySetSize*_*` rows collapses into one `Array<T>::~Array`
/ `SetSize` once the template exists; only the handful of genuinely distinct AVL keyed
`Insert` bodies (avl_trees.cpp's 3, plus set_clear's 1) and the LinkedList's 3 stay as
template-instance-specific but no longer address-registered bodies.

## 2. Candidate instantiations for the "trivial / copy-ctor / dtor" template test (decision 3)

| trait | recommended instantiation | why |
|---|---|---|
| **trivial** (byteCopy, no dtor) | `Array<int32_t>` (`kSpec_I32`, 0x18002fea0) | simplest, byteCopy=true, destroy=null, tail-uninitialised semantics is the sharpest edge case for `std::is_trivially_copyable` mapping |
| **with copy-ctor** (assign is a real call, still trivially destructible) | `Array<System::Filename>`-shaped `kSpec_StringRef`/`kSpec_StringRefZ` (0x180034860/0x180029a80) or `kSpec_VideoResource` (0x18004f620, assign calls `Filename`'s copy ctor) | assign is a CALL not a memcpy, but element has no separate destroy step (String/Filename refcount release folds into the generic destroy list below) — exercises "non-trivial assign, byteCopy=false" |
| **with dtor** (destroy non-null, refcounted element) | `kSpec_StringExt`/`kSpec_Sprite38`/`kSpec_GraphSeries` (0x180061480 / 0x180004e30 / 0x1800417d0) or `kSpec_ExtArrayElem` (0x18002c6a0, `{Filename, int32, int32}`, destroy runs on BOTH realloc and shrink arm) | exercises full ctor/copy/dtor triple including the realloc-vs-shrink distinction the hazards section below calls out |

`kSpec_PkgEntry` (0x1800298f0) is the recommended fourth check even though only three are
asked for: it is the one instantiation whose SHRINK arm has **zero live boot coverage**
(array.cpp:1591-1601) — a live game session only ever grows it — so it is the one where a
template refactor could silently break an untested code path.

For `Set`: the T=8 node-header AVL core (`set_avl.cpp`, exe `EntityList` tree) is trivial;
`Set<Pair<String,String>>` (Map, T=0x10) is the one with a real destructor (`set_clear.cpp`'s
`DestroyElement` releases two Strings); `Set<Pair<Filename,u64>>` (T=0x18, LoadPackages path
index) is the with-copy-ctor case (Filename copy ctor called on insert).

For `LinkedList`: only one instantiation exists in the whole binary (T=8, `UserInterface`
dialog list) — no three-way trait split is possible; note this to the coordinator, template
verification for LinkedList will have to rely on synthetic instantiations rather than a
third distinct retail body.

## 3. External vocabulary reaching the containers (outside `runtime/src/collection/`)

| vocabulary | code | engine | runtime | standalone | dx11 |
|---|---|---|---|---|---|
| `@Collection@@` mangled-name lookups (files) | 3 | 2 | 22 | 51 | 1 |
| `fn_*Array*` occurrences (engine_funcs.h) | — | 71 total identifiers | — | — | — |
| `fn_*Set*` occurrences (engine_funcs.h) | — | 168 total identifiers | — | — | — |
| `fn_*Array*` call sites (grep -c) | 810 | 263 | 2 | 210 | 0 |
| `fn_*Set*` call sites (grep -c) | 1697 | 665 | 16 | 370 | 0 |
| exe-side `ArrayResize`/`StringArrayResize`-style helpers (files) | 45 | — | — | — | — |
| raw `_size`/`_capacity` offset pokes (`+0x08`/`+0x0c` literal patterns) | 0 | 1 (`MainLoop.h`) | — | 0 | — |

`code/` and `standalone/` never construct/copy/destroy a `Collection::Array`/`Set` as a
live C++ object directly — every hit is either (a) a `fn_*` pointer resolved through the
registry by mangled name/address (the dominant pattern, ~1000+ call sites in `code/`
alone), or (b) a mangled-name string literal passed to `ResolveExport`/`ResolveAddr` (the
`@Collection@@` file hits). This mirrors the `ExternalPtr`/`InternalPtr` finding in
`SMARTPTR_INVENTORY.md` §2 almost exactly: the consuming layers never touch container
internals by raw offset, they go through per-instantiation thunks. That is favorable for
decision 3 — converting the containers to templates changes nothing about `code/`'s call
shape, only what's behind the `fn_*` pointer/registry row.

`runtime/` itself is the only place with any non-thunk `@Collection@@` reach (22 files) —
expected, since the container bodies and their direct callers (Object::Factory's type
trees, ExternalFactory's two AVL trees, IO::PackageFile's path index, UI's dialog list)
live there.

## 4. Dependencies

| dependency | how the containers use it | status |
|---|---|---|
| `System::MemoryAllocator::Allocate/Free` (the `_singleton`, `+0x640` critical section) | `Array` grow/shrink (`MemAlloc(bytes, align)`/`MemFree`), `Set`/`LinkedList` node alloc/free — ALL under the allocator's own lock, never the container's | Ported (`runtime/src/system/allocator.cpp`), address-registered; alignment argument is **`alignof(T)`, never `sizeof(T)`** (Array<int> passes 4, Array<ModInfo> and Set nodes pass 8) — a template must compute this via `alignof(T)`, not hardcode |
| `Object::Base` vtable slots | element destroy paths for `ExternalPtr<T>`/graphics-element arrays that hold a `Base*`-derived pointer | `Object::Base` not yet a real class (§B.3 step 3, AFTER Collection templates) — element destroy helpers keep calling through the existing raw vtable-slot/`RT_EXPORT_ONCE` door, same pattern SMARTPTR_INVENTORY §4 documents |
| `Serialize` paths | `Array<T>::Serialize` — **not found as a distinct entry point in this file set**; no `kSpec_*` row or registered address named `Serialize` appears in `array.cpp`/`array_destroy.cpp`. Serialization of arrays appears to be done by the CALLER iterating and serializing elements (matches `ExternalPtr<T>::Serialize`'s pattern of walking an `Array` externally), not a member on `Array<T>` itself in this codebase's ported surface | confirm with a targeted grep before assuming a `Serialize` method belongs on the template — none of the 339 registered rows is named Serialize |
| introsort (`Sort` with comparator) | `0x1800264a0` packfile sort — ONE instantiation, hand-transcribed (not a generic `std::sort` substitute) because retail's unstable-sort tie behavior is save/load-observable | This is NOT a generic Array method to templatize; it is a one-off algorithm on top of an `Array`-shaped buffer and should stay a free function, not become `Array<T>::Sort()`, unless a second instantiation turns up |
| AVL compare policy (`Collection::AVLCompare`) | stateless, lexicographic byte compare, NULL-key-is-least, inlined at every call site (never a virtual/indirect call) | Confirmed stateless; a template's default `Compare` parameter should be a policy struct exactly like retail's, not a `std::function` |

## 5. Hazards

1. **`byteCopy` marks the ONLY specs where element order/count doesn't matter for a bulk
   `memcpy`.** Every non-`byteCopy` spec's `assign` is a real call (copy ctor / refcount
   bump) and MUST be invoked per-element in order — a template that defaults to `memcpy`
   whenever `std::is_trivially_copyable<T>` is true reproduces `byteCopy=true` exactly,
   but the mapping must be verified per spec, not assumed from the C++ type trait alone:
   `SMARTPTR_INVENTORY.md` §5.4 already flags that `ExternalPtr<T>` inside an `Array` is
   copied with a **plain 8-byte copy, "NO refcount touch"** — correct today only because
   `ExternalPtr<T>` itself has no refcount. **A future `ExternalPtr<T>`-in-`Array<T>` must
   stay trivially copyable or this exact array bulk-copy path silently breaks it** — this
   is a live cross-dependency between B.3.1 (smart pointers) and B.3.2 (containers): keep
   `ExternalPtr`/`InternalBasePtr` as `is_trivially_copyable` (already `static_assert`ed in
   `External.h:499` / `Internal.h:356,407`) or `Array`'s byteCopy fast path must be revisited
   everywhere it holds one.
2. **Realloc-and-move is copy-THEN-destroy, not move-then-destroy.** `SetSizeCore`'s
   growth path copy-constructs `[0, oldSize)` into the NEW buffer via `assign`, THEN
   destroys the old buffer's elements, THEN frees the old buffer (array.cpp banner
   ~line 260-268). A `std::vector`-style template that instead move-constructs would
   change String refcount observability (a String's `Set()`/refcount sequence is checked
   by the resave gate per `reference_write_path_serialize_is_replayable.md`). The template
   MUST replicate copy-then-destroy-then-free, not substitute a move, for any non-trivial T.
3. **Ghidra-dropped-argument traps** (project memory `feedback_ghidra_dropped_args.md`):
   `linkedlist.cpp` documents a live example — `0x180009c30` is `MemFree` with one EXTRA
   ignored leading argument (a register-passthrough forwarder), discovered only by
   comparing full instruction sequences, not Ghidra's decompiled C. Any new template body
   written by re-deriving from Ghidra's C (rather than the already-ported `SetSizeCore`/
   `ArrayDtorImpl`/AVL bodies) risks re-introducing an argument-count mismatch already
   solved here. **The template MUST be built by wrapping the existing ported bodies
   (`rtcoll::SetSizeCore`, `ArrayDtorImpl`, the AVL primitives), not by re-decompiling.**
4. **Two AVL node layouts is a documented trap, twice over** (memory
   `reference_one_bst_find_two_node_layouts.md`; `set_avl.cpp` vs `set_avl_e24.cpp` vs
   `set_avl_pathid.cpp`): the AVL retrace/rotation bodies are element-width-specific by
   MSVC's own monomorphization (parent/left/right offsets shift with `align8(sizeof(T))`),
   so `Collection::Set<T>`'s template cannot share ONE instantiation's retrace across
   different `sizeof(T)` — this is expected and already handled by having 3 separate
   ported AVL-core files; the template's internal dispatch must still pick the right node
   layout per `T`, which happens automatically if the template computes offsets from
   `sizeof(T)` the way Collection.h's `Node` struct already does (no separate action
   needed beyond keeping the struct-based node, not reintroducing address-keyed dispatch).
5. **`memcpy`-of-whole-header sightings**: `array.cpp:1475` does
   `memcpy(d+0x08, s+0x08, 0x10)` for one specific instantiation (raw copy of `_size`+
   `_capacity`+following bytes, NOT a general Array-header copy) — verify this specific
   spec's semantics are preserved exactly (it's copying `_size`/`_capacity` plus 8
   trailing bytes of a LARGER struct, not `Array<T>` itself) rather than assumed to be a
   generic pattern; no sighting of a raw 0x10-byte `memcpy` of an entire `Array<T>` header
   or a whole `LinkedList` was found elsewhere in the 4331+1884 lines scanned.
6. **`Array<T>::Serialize` may not exist as a container method at all** (§4) — do not
   budget template work for a Serialize member without confirming a `kSpec_*`-style
   registered address named Serialize exists; current evidence says callers serialize
   array contents by iterating externally.
7. **The packfile introsort is retail-observable via tie behavior** and must NOT be
   replaced by `std::sort`/`std::stable_sort` even after `Array<T>` becomes a template —
   it stays the hand-transcribed `0x1800264a0` body operating on the array's buffer.

## 6. Recommendation

**Template design**: one `template<class T, class Allocator=DefaultAllocator>
Collection::Array<T>` built as a thin wrapper that calls the EXISTING
`rtcoll::SetSizeCore(ArrayHdr*, int, const uint8_t*, const ArraySpec*)` and
`rtcoll::ArrayDtorImpl` — do not rewrite the algorithm, generate the `ArraySpec` for `T` at
compile time:

```cpp
template <class T> const ArraySpec* SpecFor() {
    static const ArraySpec spec = {
        "auto", sizeof(T), alignof(T),
        /*assign*/  [](uint8_t* d, const uint8_t* s){ new (d) T(*reinterpret_cast<const T*>(s)); },
        /*init*/    std::is_trivially_default_constructible<T>::value ? nullptr
                        : [](uint8_t* d){ new (d) T(); },
        /*destroy*/ std::is_trivially_destructible<T>::value ? nullptr
                        : [](void*, uint8_t* base, int n){ for(int i=0;i<n;i++)
                              reinterpret_cast<T*>(base+i*sizeof(T))->~T(); },
        /*byteCopy*/ std::is_trivially_copyable<T>::value,
        ...
    };
    return &spec;
}
```
This maps `std::is_trivially_copyable<T>`/`is_trivially_destructible<T>` onto exactly the
`byteCopy`/`destroy==null` fields the retail specs already encode by hand — the mapping is
not new design, it is naming what 27 hand-derived specs already do. **Existing `kSpec_*`
rows for non-trivial/exotic instantiations (fill-value, tail-uninitialised, non-default
init) are NOT expressible from `T`'s type traits alone** (e.g. `kSpec_I32Neg1`'s fill of
-1, or `kSpec_Bool`'s deliberately-uninitialized tail) — these stay as explicit
`ArraySpec` overrides/tags on the template instantiation (a policy parameter or an
explicit `SetSizeFill` overload), not auto-derived. Budget for ~8-10 such "explicit spec"
instantiations that cannot be trait-derived.

**`Collection::Set<T,...>` template**: wraps the appropriate one of the 3 ported
element-width AVL cores (`set_avl.cpp`/`set_avl_e24.cpp`/`set_avl_pathid.cpp`) selected by
`sizeof(T)` at compile time (three explicit specializations or a policy struct keyed on
`sizeof(T)`), plus per-`Key`/`Compare` `Insert`/`Find` generated from the pattern in
`avl_trees.cpp`'s 3 keyed entry points. `Collection::Map<K,V>` should be spelled
`Set<Pair<K,V>, ..., K, Compare>` in the template API, matching retail — do not introduce
a distinct Map template.

**`Collection::LinkedList<T>` template**: wraps the 3 ported bodies in `linkedlist.cpp`
directly (Remove-by-value, node alloc, allocator-lock free) — since only one retail
instantiation exists, the template body is effectively already fully verified against its
single real-world use, and templating it is mechanical (generic over `T` via
`sizeof(T)`/`alignof(T)` exactly as `Array` does).

**Shims during transition**: keep every one of the 339 `RegisterRuntimeAddr` rows as
thin one-line forwarders to the new template's `SetSize`/`Serialize`/destructor for the
duration, per B31_RULES's two-step pattern — Step 1 (core) introduces the templates and
turns the ~27 `ArrayDtor_*`/`ArraySetSize*_*` free functions into one-line calls into
`Array<T>::SetSize`/`~Array`; Step 2 (callers, parallel by directory) migrates `fn_*`
pointer resolution sites in `code/`, `engine/`, `standalone/` to construct/use the real
template type directly, deleting shims at zero callers exactly as the value-type batch
did.

**Batch split**: recommend **3 agents**, split by container family (Array is the largest
at 4331+1884+459+371=7045 lines and should itself be one dedicated agent; Set/AVL
2466 lines is a second; LinkedList+introsort 817 lines can ride with whichever finishes
first or be a light third agent alongside the closing/registry cleanup). This mirrors the
`B.3.1` family-serial, caller-parallel split, not per-file, since `SetSizeCore`/AVL-core
sharing means splitting by file would create merge conflicts on the shared internal
headers.

**Unit tests** (`tests/unit/`) should cover, per B31_RULES class-design-rule 6's model:
grow-from-empty (capacity 1, not 3), grow-append-to-nonempty (capacity trebles to
`3*newSize`), exact-fit bulk `SetSize(n)`, shrink (destroy called on the truncated tail,
capacity unchanged), `Clear()` (=`SetSize(0)`, buffer retained), each of the 3
trait-instantiations named in §2 above (trivial/copy-ctor/dtor), the `kSpec_PkgEntry`
shrink arm specifically (currently zero live coverage per §2), AVL insert/find/erase for
each of the 3 node widths plus the "duplicate key returns existing node, never inserts
again" rule, `LinkedList` Remove-by-value + iteration order, and the introsort's
tie-behavior permutation-replay test that already exists as a probe
(`array_probe.cpp`/`introsort_probe`-style) ported into the unit suite rather than a
temporary probe tool.
