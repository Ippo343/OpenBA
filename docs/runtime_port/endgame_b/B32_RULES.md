# ENDGAME B.3.2 — Collection templates: rules for the batch (coordinator, 2026-09-13)

Scope: ENDGAME.md §B.3 step 2 / §B.1 decision 3 — `Collection::Array<T>`,
`Collection::Set<T,…>` (AVL; every "Map" is `Set<Pair<K,V>>`, there is no Map),
`Collection::LinkedList<T>`. Design read: `COLLECTION_INVENTORY.md` (this directory).
Everything in `B31_RULES.md` applies unchanged: the two-step shim pattern, the class design
rules, the hygiene section (lock, cmp the deployed exe, foreground run, merge main before the
final build), the report format, the closing-pass shape.

## Facts the batch rests on (inventory §1–§5)

- 10,563 lines in runtime/src/collection/, 339 `RegisterRuntimeAddr` rows: ~27 Array
  instantiations, 3 AVL node widths, 3 keyed `Set<Pair>` inserts, 1 LinkedList instantiation.
- The mechanism is already template-shaped: one `ArraySpec` (stride/align/assign/init/
  destroy/byteCopy) per retail instantiation drives ONE `rtcoll::SetSizeCore` /
  `ArrayDtorImpl`. The AVL cores are three by element width.
- Callers outside runtime/src/collection never touch internals: `fn_*` pointer calls
  (code: ~810 Array, ~1,697 Set) and mangled-name lookups. Templating changes no call shape.
- No `Array<T>::Serialize` entry point exists among the 339 rows; elements are serialized
  by the callers. Do not add one without proof.

## Step 1 — the cores (three agents, by family, disjoint files)

- **ARRAY** (array.cpp, array_destroy.cpp, array_graphics_*.cpp, array_internal.h,
  array_destroy_internal.h, the Array part of Collection.h): `template<class T,
  class Allocator = DefaultAllocator> class Array` with retail member names (`SetSize`,
  `Clear`, `operator[]`, `GetSize`, `GetCapacity`, dtor, …), same 0x10 layout, asserts kept.
  The template WRAPS the existing `SetSizeCore`/`ArrayDtorImpl`; it does not re-derive them.
  `SpecFor<T>()` builds the `ArraySpec` from type traits (`byteCopy` =
  `is_trivially_copyable<T>`, `destroy == nullptr` = `is_trivially_destructible<T>`), which
  names what the 27 hand-written specs already encode. The ~8–10 specs a trait cannot
  express (fill −1, deliberately uninitialised tail, non-default init) stay explicit as a
  policy parameter or a `SetSizeFill` overload — never silently replaced by the trait
  default. Every `ArrayDtor_*` / `ArraySetSize*_*` free function becomes a one-line shim
  onto the instantiation; all rows stay.
- **SET** (avl_trees.{cpp,h}, set_avl*.{cpp,h}, set_clear.cpp, the Set part of
  Collection.h): `Set<T, Compare = AVLCompare, Allocator = DefaultAllocator>` selecting the
  ported core by `sizeof(T)` (three explicit width specialisations or a policy keyed on
  `sizeof`); `Insert`/`Find`/`Remove`/`Clear`/iterator with retail names; the three keyed
  `Set<Pair<K,V>>` inserts generated from avl_trees.cpp's pattern. The three node layouts
  stay distinct internally; the "duplicate key returns the existing node" rule is a test.
- **LIST** (linkedlist.cpp, introsort.{cpp,h}, introsort_msvc.h, the LinkedList part of
  Collection.h): `LinkedList<T>` wrapping the three ported bodies; `Sort` stays the ported
  introsort with a comparator template parameter, tie behaviour byte-identical (the
  permutation-replay probe becomes a unit test).

Semantics that MUST NOT change (inventory §5): the realloc path is copy-then-destroy-then-
free in retail's element order, not move; String/Filename/ExternalPtr elements' refcount
sequences are observable through the resave gate. `byteCopy` is keyed off
`is_trivially_copyable<T>` and nothing else. The shared allocator alignment (Array 8,
Set nodes 8) is unchanged.

Unit tests (tests/unit/): grow-from-empty (capacity 1), append-to-nonempty (capacity
`3*newSize`), exact-fit bulk `SetSize(n)`, shrink (destroy on the truncated tail, capacity
unchanged), `Clear()` retains the buffer, the three trait instantiations (`Array<int32_t>`;
copy-ctor: the `kSpec_StringRef`/`VideoResource` shape with `System::Filename`; dtor:
`kSpec_StringExt`/`ExtArrayElem`), the `kSpec_PkgEntry` shrink arm (zero live coverage
today), AVL insert/find/erase per node width, LinkedList remove-by-value and iteration
order, introsort tie replay.

## Step 2 — callers, then closing

Exactly as B.3.1: directory-owned batches convert `fn_*Array*` / `fn_*Set*` sites and
mangled lookups to the template types, delete shims and rows at zero callers by name AND
address, regenerate the .inc files; one closing pass for engine_funcs.{h,cpp} and STATUS.

## Report

B31_RULES format plus: instantiations mapped to the template (trait-derived vs explicit
spec, with the retail address of each), rows kept as shims (count), unit test count, the
resave gate is the coordinator's.
