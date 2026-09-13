# ENDGAME B.3.1 — smart pointer design inventory (read-only, 2026-09-13)

Scope: `Object::ExternalPtr<T>`, `Object::InternalPtr<T>`, `Object::InternalBasePtr`,
plus the two ride-along element/entry structs the ExternalFactory trees use
(`ExternalArrayElement`, `ResourceEntry`, `ReferenceEntry`). No tree file edited.

## 1. Layout and semantics, as the tree has them today

### `Object::ExternalPtr<T>` — `runtime/types/object/External.h:36-42`

```
template <class T> struct ExternalPtr { T* ptr; };   // 8 bytes, no refcount
```

Pinned by `ExternalList::Find` (0x1800bf700): returns one **by value** through a
hidden return slot, and the entire payload write is one qword store — a hit copies
`_data[i]`, a miss stores 0. **Nothing bumps a refcount going out or coming in.**
Ownership of the pointee lives in `Object::ExternalFactory`'s two AVL trees
(`ResourceEntry`/`ReferenceEntry`, `External.h:159-192`), never in the pointer.
There is no `AddRef`, `Release`, or destructor for `ExternalPtr<T>` in the tree —
because retail's compiler never generates them for a bare-pointer struct.

`Serialize` is the one nontrivial member, and it is 25 retail instantiations of ONE
748-byte body (`runtime/src/object/external_ptr_serialize.cpp:1-100`, Wave 10 lane
EXTPTR). Per T it: reads the pointee's `External::_path`
(`?GetPath@External@Object@@…`, 0x180006250), checks `IsDerivedFrom` against T's
type id, in the "reference mode" arm resolves through
`ExternalFactory::GetReferenceId`/`GetPathForReferenceId`, and otherwise calls
`ExternalFactory::FindOrLoad` by path on read. The only place two instantiations
differ (byte-for-byte, position-resolved diff) is the `MOV EDX,[rip+…]` load of
`?_typeInfo@T@…` for the `IsDerivedFrom` check — everything else (193 of 215
instructions) is identical (`external_ptr_serialize.cpp:14-31`).
`CheckPtr`/`Clone` do not exist for `ExternalPtr<T>` — nothing in the export table
or the C bodies defines them; there is no refcount to check or clone.

### `Object::InternalPtr<T>` — no dedicated type header; it is `InternalBasePtr` + a type check

There is no `runtime/types/object/InternalPtr.h`. `InternalPtr<T>` is retail's
*typed* wrapper over the same 8-byte `InternalBasePtr` (`Internal.h:83-108`); the
port never materializes a typed struct because every typed operation it exposes
(construct-from-typed-pointer, `operator->`, `Serialize`) either is byte-identical
to the base or is the array form ported directly:
`runtime/src/object/internal_ptr_array_serialize.cpp:1-79` (two retail
instantiations, `AchievementInstance` and `GoalInstance`, at 0x180009a20 /
0x180009fa0) does the per-element `InternalBasePtr::Serialize` + a type-id check
against `T::typeId`, falling back to `Object::Base::IsDerivedFrom`'s ancestor walk
when the loaded type doesn't match exactly, nulling the pointer on total mismatch
(`internal_ptr_array_serialize.cpp:8-18`, comment + body).

### `Object::InternalBasePtr` — `runtime/types/object/Internal.h:83-160`

```
struct InternalBasePtr { Internal* ptr; };   // 8 bytes, one raw pointer
```

Four trivial members, each one instruction: default ctor zeroes, ctor-from-pointer
assigns, `Copy` assigns, `DeletePtr` reads only `+0x00` (`Internal.h:87-92`).

**`CheckPtr` and `AssignId` are bare `RET` in every build, including the profile
(assert) build** (`Internal.h:94-108`; matches project memory
`reference_checkptr_is_a_ret_in_every_build.md`). This is proven at the SOURCE
level, not an optimizer artifact: `?CheckPtr@InternalBasePtr@Object@@IEBAXXZ`
(retail 0x1800c0290, profile 0x18008c5d0) is `ret` with nothing before it. The
port mirrors this faithfully: `RtObject_InternalBasePtr_CheckPtr` is an empty body
registered unconditionally (`runtime/src/object/internal.cpp:485`,
`:953-976`) — **do not add validation here**; retail never validates a stale
handle, it relies on explicit clearing at removal time.

`Serialize` (`internal.cpp:713-745,978-992`) writes/reads three fields through the
`Serializer`'s `Encoding` vtable: a `bool present`, then if present an `int index`,
`uint generation`, `uint type` (encoding slots `+0x78/+0xa0/+0x98/+0x98`,
`Internal.h:206-219`). This is the ONLY place `factoryIndex`/`generation` leave the
object — it's a save/load format, not a validity check.

No `Clone` exists for `InternalBasePtr` — copying is the plain `Copy` member
(pointer assignment, no refcount).

### `Object::Internal` (the pointee) — `Internal.h:57-79`

```
struct Internal { void* vptr; int32_t factoryIndex; uint32_t generation; };  // 16 bytes
```

`factoryIndex` is the object's own slot in `InternalFactory::_objects`
(`Internal.h:112-127`); `generation` is a value drawn from the factory's embedded
Mersenne-Twister `System::Random` at birth (`internal.cpp:569,571`). Ctor sets
`factoryIndex = -1, generation = 0`, then `AddObject` overwrites both
(`internal.cpp:565-571`). Dtor / `RemoveObject` reads `factoryIndex` as a
swap-remove index into `_objects`, patches the moved element's own `factoryIndex`
to its new slot, and zeroes `generation` (`internal.cpp:596-603`). **There is no
refcount on `Internal` either** — this is an index/generation pair for
save-file cross-referencing (`{index, generation, typeId}` triple,
`Internal.h:717-724`), not a reference-counted lifetime. Vtable has 4 slots:
scalar dtor, `TypeName`, `Type` (the type id), `Persist` (empty in both builds)
(`Internal.h:65-79`).

### The two AVL-tree entries riding along (`External.h:159-267`)

`ResourceEntry` (path-keyed) and `ReferenceEntry` (id-keyed) are the ACTUAL owners
of `External*` — the factory's two `Collection::Set` trees. `ExternalPtr<T>` never
owns; these do. `usageMask`/`_cleanBitIndex` implement the engine's slot-rental
eviction scheme (one bit per live `UpdateLoop`, `External.h:200-219`), not
refcounting in the C++ sense.

### `ExternalArrayElement` (`External.h:100-125`)

A 16-byte `{ char* owned /* Filename */; uint64_t opaque /* 2 int32s */ }` used as
`Object::External::_refs`' element type — the dependency-reference list every
loaded resource carries. `owned` is released with the String idiom by the array
destructor (`FUN_180006ac0`), so IT is refcounted (via `System::String`'s
data-8 block), but the struct itself is a plain array element, not a smart pointer.

## 2. Vocabulary reaching them, by directory

| vocabulary | code | engine | runtime | standalone | dx11 |
|---|---|---|---|---|---|
| `?$ExternalPtr@` mangled name occurrences (files) | 6 | 2 | 29 | 77 | 0 |
| `?$InternalPtr@` mangled name occurrences (files) | 0 | 2 | 65 | 152 | 0 |
| `InternalBasePtr@Object` mangled name occurrences (files) | 8 | 2 | 2 | 29 | 0 |
| `fn_ExternalPtr*` pointer symbols (engine_funcs.h) | 85 call sites | 34 call sites | 0 | 140 call sites | 0 |
| `fn_InternalPtr*` pointer symbols | 0 | 0 | 0 | 0 | 0 |

Counts are file-level `grep -rl` hits (a rough vocabulary-reach proxy, not call
counts) except the `fn_*` row, which counts occurrences of the identifiers.
`code/` never touches `ExternalPtr<T>`/`InternalPtr<T>` as a live C++ type — every
hit there is a **mangled-name string literal** passed to `ResolveExport`
(`code/Game/Action/action_base_movedeps.cpp:98`,
`code/Game/Dialog/dialog_exe_helpers.cpp:224`,
`code/Game/Game/gameupdateloop.cpp:641,659`,
`code/Game/Game/gameupdateloop_load.cpp:73,827`) — i.e. exe code calls a retail
Runtime method whose PARAMETER happens to be `ExternalPtr<T>`, passed as 8 raw
bytes; `code/` never constructs, copies, or destroys one. `engine_funcs.h` defines
9+ distinct `fn_ExternalPtr_Serialize*` typedefs, one per resolved instantiation
(`GraphicsMesh`, `Weather`, a generic 3-arg form, a "Typed_7140" form, plus three
unnamed numbered ones — `engine/funcs/engine_funcs.h:4160-6058`), confirming the
per-instantiation-thunk shape from the runtime side too. There is no `fn_InternalPtr*`
anywhere in the tree — `code/`/`standalone/` never call `InternalPtr<T>` members
directly; all `InternalPtr`/`InternalBasePtr` traffic is internal to `runtime/`.

**Distinct retail instantiations found in the registry** (`standalone/rt_export_addrs.inc`):

- `ExternalPtr<T>`: **26** distinct T (`Achievement, AnimationGroup, Cursor, Dialog,
  DialogControllerConfig, EdgeMesh, ElementDescription, External, ExternalList,
  Font, Goal, GraphicsMesh, ImageBuffer, Material, MaterialInstance, Particle,
  PickingMesh, PixelProgram, RenderState, SoundBuffer, SoundEffect, SpriteSheet,
  StringTable, Surface, VertexLayout, VertexProgram`) — matches the "25 addresses,
  one 748-byte body" finding in `external_ptr_serialize.cpp` banner plus the
  `Achievement::Goal` fourth instantiation the oracle needed.
- `InternalPtr<T>`: **10** distinct T (`AchievementInstance, ButtonElement,
  ComboElement, DialogController, DialogInstance, DocumentTextElement, Element,
  GoalInstance, ImageElement, TextElement`).

Only 4 of the 26 `ExternalPtr<T>` and 2 of the 10 `InternalPtr<T>` instantiations
are actually **ported/registered** today (the array-serialize pair
`AchievementInstance`/`GoalInstance`, plus the four `ExternalPtr` addresses in
`external_ptr_serialize.cpp`'s banner: `ElementDescription`, `Dialog`,
`StringTable`, `Achievement::Goal`). The rest are named in the registry
(`rt_export_addrs.inc`, resolved by mangled name to the shipped DLL... except the
DLL is already gone per ENDGAME A — see hazard #4 below) but have no C body.

## 3. One template body, or several?

**`ExternalPtr<T>::Serialize` is ONE template body**, proven, not assumed: a
position-resolved instruction diff across all 25 retail copies found 193/215
instructions byte-identical, 20 "position-only" (same absolute target, different
rel32 encoding), and exactly 2 genuinely different instructions — both are the
single `MOV EDX,[rip+?_typeInfo@T@…]` load feeding the two `IsDerivedFrom` calls
(`external_ptr_serialize.cpp:14-31`). A `template<class T> class ExternalPtr` with
`static Object::TypeInfo* TypeInfo() { return &T::_typeInfo; }` (or equivalent)
as the only per-T customization point reproduces this exactly — decision 3's
"verify against three instantiations with different traits" is already done here,
just for one method instead of the whole class, because `ExternalPtr<T>` has no
other nontrivial method.

**`InternalPtr<T>` is likewise one body**, and simpler: the only typed operation is
the array-serialize walk (`internal_ptr_array_serialize.cpp:32-40`), which is a
loop over `InternalBasePtr::Serialize` (untyped) plus one type-id compare against
`T::TypeId()`. No per-T Clone or CheckPtr exists to differ (§1). So B.3.1's
`template<class T> class InternalPtr : InternalBasePtr { … }` needs exactly one
customization point too: the compile-time type id.

**`InternalBasePtr` and the `ExternalPtr<T>` non-Serialize members (ctors, Copy,
DeletePtr, operator bool/->)** are untyped/trivial pointer operations already
ported once each (`Internal.h:87-108`) — no per-instantiation variance to
reconcile at all.

Net: **one template class per family**, each with a single templated method
(`Serialize`); everything else is a shared non-template base or free function.

## 4. Dependencies on not-yet-converted machinery

| dependency | needed by | status | B.3.1 workaround (B31_RULES "not-yet-converted structs") |
|---|---|---|---|
| `Object::Base` vtable slot +0x10 (`GetTypeId`) | `ExternalPtr<T>::Serialize` (IsDerivedFrom's caller), `InternalPtr<T>` array-serialize | Not yet a real C++ class (§B.3 step 3, after smart pointers) | keep reading through `RT_EXPORT_ONCE`/raw vtable-slot call (`external_ptr_serialize.cpp:213-217`, `Base::IsDerivedFrom` called by name) |
| `Object::Internal` vtable slots 1-3 (`TypeName`, `Type`, `Persist`) | `InternalBasePtr`'s pointee | `Internal.h` documents the 4-slot exported vtable (`??_7Internal@Object@@6B@`, 0x180154720) directly — it's a DLL-address vtable stamp today, not a class | keep stamping the exported vtable address; a real `Internal : Base` class is §B.3 step 3 |
| `ExternalFactory` (2 AVL trees, path stack, singleton) | `ExternalPtr<T>::Serialize`'s `FindOrLoad`/`GetReferenceId`/`GetPathForReferenceId` calls | Partially ported (`external.cpp`, `external_add.cpp`, etc.); `FindOrLoad` itself is explicitly UNPORTED and reached "by exported name so whichever lane ports it is picked up with no edit here" (`external_ptr_serialize.cpp:66-83`) | keep the `RT_EXPORT_ONCE("?FindOrLoad@ExternalFactory@Object@@…")` name-door; do not inline or by-address this call in B.3.1 |
| `InternalFactory` (`_objects`, `_random`) | `Internal`'s ctor/dtor (factoryIndex/generation assignment) | Ported as a struct (`Internal.h:130-160`), not yet a class | fine to leave as a POD member access; no new cast needed since B.3.1 doesn't touch `InternalFactory`'s own methods |
| `IO::Serializer` / `Encoding` vtable | `InternalBasePtr::Serialize`, `ExternalPtr<T>::Serialize` | `SerializerLite` is an explicitly *not-yet-real* 16-byte stand-in (`Internal.h:163-219`, "reproduced here rather than taken from a Wave 4 header, which does not exist yet") | keep `SerializerLite`/raw `encVtbl[...]` calls; a real `IO::Serializer` class is §B.3 step 4, after Object entirely |

So: **all four surrounding classes are unconverted**, and B31_RULES's own
guidance ("members of not-yet-converted structs keep their POD type … reached
through shims or a transitional `reinterpret_cast`") applies almost everywhere a
smart pointer's `Serialize` reaches outward. The pointers' OWN fields (`ptr`,
`factoryIndex`, `generation`) are self-contained and need no such cast — only the
calls *out* to `Base::GetTypeId`/`IsDerivedFrom`, `ExternalFactory::*`, and the
`Serializer`/`Encoding` vtable do.

## 5. Hazards

1. **`CheckPtr` is a real no-op, not a bug to fix.** `InternalBasePtr` never
   self-invalidates in retail (`Internal.h:94-108`); a real destructor MUST NOT
   add validation on deref — that would diverge from every save file and every
   caller's assumption that a raw copy stays "valid-looking" after its target
   dies. (Matches memory `reference_checkptr_is_a_ret_in_every_build.md`.)

2. **Refcount ownership is a trap by omission, not by rule**: neither
   `ExternalPtr<T>` nor `InternalBasePtr` owns a refcount at all — copy/assign is
   a bare pointer copy in both (`External.h:36-42`, `Internal.h:87-92`). The
   *pointee*'s lifetime is owned elsewhere (`ExternalFactory`'s trees for
   External; `InternalFactory::_objects` + explicit `new`/`delete` for Internal).
   **A B.3.1 implementation that adds AddRef/Release semantics to either wrapper
   changes retail behavior** — these are raw/weak-shaped handles, not
   `shared_ptr`-shaped ones, despite the "smart pointer" label in ENDGAME §B.1.6.
   A real class must keep them trivially copyable non-owning views.

3. **`tools/extptr_*.py`** — three scripts, all read-only against the (still
   present on disk, though ENDGAME A has removed the exe's *load* of it)
   `C:\GOG Games\Banished\Runtime-x64.dll`:
   - `extptr_serialize_diff.py` — position-resolved instruction diff of exactly
     two addresses (0x1800042a0 vs 0x180076610), used to prove "same body".
   - `extptr_family_sweep.py` — builds a wildcard byte-mask from the reference
     body and scans all of `.text` for matches, to enumerate every
     `ExternalPtr<T>::Serialize` copy (found the 25).
   - `extptr_clone_check.py` — generalizes the pairwise diff to an arbitrary
     address set ("N-way 'is it ONE body?' check"). Whoever implements the
     template should rerun this against the 26 registry addresses (only 4 are
     currently confirmed via the pairwise tool) before deleting any more C
     bodies, since 22 instantiations were only found by the byte-mask sweep and
     never individually diffed.

4. **Array/memcpy interaction**: `Collection::Array<T>::operator=` and the range
   destroy in `runtime/src/collection/array.cpp` and `array_destroy.cpp` move
   `{String*, ExternalPtr}` pairs (`kD_Str_10`, `array.cpp:512,850,983`) with a
   **plain 8-byte copy for the ExternalPtr half — "NO refcount touch"**
   (`array.cpp:983`, explicit comment). This is *correct* today because
   `ExternalPtr<T>` has no refcount to touch (§1/§5.2) — but it means a real
   `ExternalPtr<T>` class must stay a **trivially-copyable, trivially-destructible
   POD-like type** (no user-provided copy ctor/dtor with side effects), or these
   existing bulk memcpy/array-resize paths (which never call a constructor per
   element) will silently skip whatever the new type's copy ctor was supposed to
   do. This is the same trap decision 6 / the B31_RULES banner calls out for
   `System::String` in reverse (String's real dtor causes a double free through
   old manual-Dtor call sites) — here it's the mirror image: a class with
   nontrivial copy semantics would be silently bypassed by array bulk-copy code
   that predates it.

5. **The DLL the extptr tools read no longer loads at runtime** (ENDGAME A closed
   on main; `main` no longer calls `LoadLibrary` on `Runtime-x64.dll`). The
   `.py` tools open the on-disk file directly via `pefile`, so they still work
   for archaeology, but any `RT_EXPORT_ONCE`/`ResolveExport` name-door in the
   pointer bodies that has NOT yet been given a ported replacement (`FindOrLoad`,
   §4) is now a **dead call** at runtime, not a DLL fallback — confirm this
   before touching `ExternalPtr<T>::Serialize`'s callers; it may already be
   broken independent of B.3.1 (worth flagging to the coordinator, out of scope
   for this inventory to fix).

6. **Two AVL node layouts, not one** (`External.h:224-267`, matches memory
   `reference_one_bst_find_two_node_layouts.md`): `ResourceSet::Node` (0x38) and
   `ReferenceSet::Node` (0x30) differ, so anything a real `ExternalPtr<T>`
   Serialize touches through the factory must keep calling the two distinct
   ported Set bodies, not a shared one — this is a decision-3 template hazard
   for `Collection::Set` (B.3 step 2), not for the pointers themselves, but it
   sits directly downstream of `ExternalPtr<T>::Serialize`'s `FindOrLoad` path.

## 6. Recommendation

**Implement `ExternalPtr<T>` and `InternalPtr<T>`/`InternalBasePtr` now, in
B.3.1, as thin non-owning templates — do NOT defer to B.3.3.**

Reasons: (a) the design question B.3.1 was gated on — "is this one template body
or several" — is already answered empirically for both families (§3: one
templated method each, everything else shared/trivial); (b) neither wrapper
touches `Object::Base`'s vtable directly (only their *dependencies*, `IsDerivedFrom`
and the Internal vtable stamp, do — and those stay reached through the existing
`RT_EXPORT_ONCE`/raw-vtable-call door per B31_RULES's own "members of
not-yet-converted structs" rule, §4); (c) neither wrapper owns a refcount, so
there is no `Object::External`/`Object::Internal` factory redesign implied —
making them real classes is purely a syntax upgrade (member functions instead of
free functions plus a template parameter) with a hazard list that is entirely
about *not* adding behavior (§5.1, §5.2, §5.4), which is squarely what B.3.1's
"RAII wrapper with the bodies the C functions already have" scope describes.
Waiting for `Object::Base`/`Factory`/`Internal`-as-class (§B.3 step 3, "the one
step with real design work") gains nothing here, because the pointer classes
would still only ever read the not-yet-converted classes through the same
shim/cast boundary either way. The one prerequisite before implementing: rerun
`tools/extptr_clone_check.py` across all 26 registry `ExternalPtr<T>` addresses
(only 4 have been individually confirmed; 22 came from the byte-mask sweep alone)
so the template's single body is verified against the full instantiation set,
not just the four already wired up.
