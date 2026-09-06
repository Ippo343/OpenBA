# Wave 2, Lane E — `Collection::*` container layouts

Recovered 2026-09-06 on `wave/upgrade-path-complete`, per [PLAN.md](PLAN.md) §4.2 (Wave 2:
"Collection templates as headers") and §4.3 **risk 1**, which ranks these layouts the single
biggest risk in the whole runtime port: the containers are header-only C++ templates, inlined
into every consumer, they define **zero** exported symbols, and every wave from 3 onward embeds
one. Getting a layout wrong corrupts silently.

Deliverable: [`runtime/types/collection/Collection.h`](../../runtime/types/collection/Collection.h).
This file is the evidence behind it. **No functions were ported and nothing was registered.**

---

## 1. Headline: there is no `Collection::Map`

PLAN.md §4.3 risk 1 says *"`Map` is the AVL tree behind `Map_Insert`/`Map_Find`"*, and the Wave 2
brief repeats it. **That premise is false.** `Map_Insert` and `Map_Find` are names *we* invented in
the Ghidra database; they are not the engine's.

- **No export in either DLL mentions `Map@Collection`.** Scanning every template-id in the export
  tables (`tools/pe_exports.py`) gives: `InternalPtr` 103, `ExternalPtr` 94, `Array` 44,
  `InterpolatedStateT` 30, `InterpolatedState` 27, `HashGrid` 4, `Set` 2, `LinkedList` 1
  (retail, 4,926 exports; the profile build's 5,182 adds only `PointerHash` 1). No `Map`.
- `Map_Insert` (`0x18002a090`) is a thin wrapper: it copy-constructs a **16-byte
  `{String key, String value}` pair** on the stack (bumping both refcounts at `str-4`) and calls
  `FUN_18002a170`, which is the ordinary `Set::Insert`. `Map_Find` (`0x18002ace0`) is
  `Set::Find` with the same 16-byte element.
- The engine's map *is* a `Set` whose element embeds the key — which is exactly why `Set` carries
  a separate `Key` and `Compare` template parameter:

  ```
  ?$Set@VFilename@System@@UDefaultAllocator@Collection@@V12@UAVLCompare@4@
    = Collection::Set<System::Filename, Collection::DefaultAllocator,
                      System::Filename, Collection::AVLCompare>
  ```

**Action for later waves: do not port a `Map`.** Port `Set` once per element width and let the
"map" fall out of `Set<Pair<K,V>, Alloc, K, Compare>`. Renaming `Map_Insert`/`Map_Find` in the
Ghidra database to `Set_Insert_16`/`Set_Find_16` would be worth doing; I did not, to avoid
stepping on concurrent lanes.

## 2. The real container set

The profile build's source-tree strings list exactly two headers in this directory
(`original_source_tree.txt`, "Collection 0 cpp / 2 h"):

```
c:\projects\buildy\code\runtime\Collection/Array.h        (file offset 0x199900)
c:\projects\buildy\code\runtime\Collection/Allocator.h    (file offset 0x19a668)
```

and the mangled names name exactly five `Collection` entities:

| Entity | Template params | Where the name comes from |
|---|---|---|
| `Array<T>` | 1 (**no allocator param**) | 44 exports, e.g. `?$Array@E@Collection@@` |
| `Set<T, Allocator, Key, Compare>` | 4 | `PackageFile::BuildPackageFile`, `TextReader::_cachedClasses` |
| `LinkedList<T, Allocator>` (+ `::Iterator`) | 2 | `UserInterface::GetFirstDialog` |
| `DefaultAllocator` | — (a `struct`, `U`-prefixed) | inside the `Set`/`LinkedList` names |
| `AVLCompare` | — (a `struct`, `U`-prefixed) | inside the `Set` names |

`Allocator.h` almost certainly holds `DefaultAllocator`; `Array.h` holds the three containers plus
`AVLCompare`. Nothing else in `Collection` appears anywhere in either binary.

**Explicitly out of scope**: `Utility::HashGrid<T>` (4 exports, Collision2D + Pathfinding, with its
own `::Iterator`) and `System::MemoryAllocator::PointerHash<T>` (profile-only, `_uninitPtrs`) are
templates but live in **other namespaces**. `HashGrid` belongs with Wave 5.

## 3. Container table

| Container | Size | Fields | Confidence | What pinned it |
|---|---|---|---|---|
| `Array<T>` | **0x10** | `_data` +0x00, `_size` +0x08 (i32), `_capacity` +0x0c (i32) | **HIGH** | 4 inlined bodies + a stride, + months of live exe use |
| `Array<T>` growth policy | — | see §4 | **HIGH** | identical code in all 4 bodies |
| `Set<...>` | **0x10** | `_root` +0x00, `_count` +0x08 (i32), pad +0x0c | **HIGH** | Insert/Find + a 0x10 stride between two adjacent Sets in the exe |
| `Set<...>::Node` | `align8(sizeof T) + 0x20` | `element` +0, `parent`, `left`, `right`, `balance` (i16), `height` (i16), pad | **HIGH** | 3 instantiations at 3 element widths, all agreeing |
| `Set` AVL invariants | — | `balance = rightH − leftH`, leaf height 0 | **HIGH** | the retrace walk `FUN_18002a310` |
| `LinkedList<T,A>` | **0x18** | `_head` +0x00, `_tail` +0x08, `_count` +0x10 (i32), pad +0x14 | **HIGH** | the unlink in `CleanRemovedDialogs`, all four pointer fix-ups |
| `LinkedList::Node` | `align8(sizeof T) + 0x10` | `element` +0, `prev`, `next` | **HIGH** | same unlink |
| `LinkedList::Iterator` | **0x08** | `node` | **HIGH** | `GetFirstDialog` returns it by value; body is one qword move |
| `DefaultAllocator` | **stateless** | — | **HIGH** | no allocating body reads a container field; all reach `MemoryAllocator::_singleton` by absolute address |
| `AVLCompare` | **stateless** | — | **HIGH** | same |
| `Set::Iterator` | 0x08? | `node`? | **GUESS — labelled as such in the header** | nothing pins it; shaped after `LinkedList::Iterator` |

## 4. `Array<T>` — the evidence

**`sizeof(Array<T>) == 0x10`, independent of T.** `Video::ShaderProgram::GetByteCode`
(`0x18004e100`) is a one-line `return _byteCode[i]` over an `Array<Array<unsigned char>>` and
compiles to exactly:

```c
return (Array<unsigned_char> *)((longlong)param_1 * 0x10 + *(longlong *)(this + 0x40));
```

The `* 0x10` is `sizeof(Array<unsigned char>)`, and the `*(this+0x40)` is the outer array's buffer
pointer read at **+0x00**.

**Field offsets.** Four inlined bodies, four different element widths, all reading the same three
fields:

| Function | Address | Element | What it shows |
|---|---|---|---|
| `DynArrayPush` (our name) | `0x18007a0d0` | 8 bytes | `+0x08` size, `+0x0c` capacity, `<< 3` stride, `MemAlloc(cap*8, 8)` |
| `Input::KeyMap::GetIdList` | `0x1800a76c0` | `int` | `Allocate(cap << 2, **4**)` — the alignment argument is `alignof(T)` |
| `Array<SaveLoad::ModInfo>::SetSize` | `0x180034010` | 0x20, non-trivial | `MemAlloc(cap << 5, **8**)`, element copy-**construct** with String refcount bumps, and element **destruction** via `FUN_1800341e0` |
| `Array<T>::SetSize(0)` | `0x180079c90` | 8 bytes | the constant-folded `Clear`: `_size = 0`, buffer kept, nothing freed |

**The field is named `_size`.** The profile build's `Array.h` assert text is
`"index >= 0 && index < _size"` (file offset `0x199e48`, immediately after `"Index out of range"`
at `0x199e30`), sitting in the same string pool region as the `Collection/Array.h` path.

**Growth policy** (byte-identical in all four bodies), for `SetSize(n)`:

```c
if (n == _size) return;                  // exact-equal is a no-op, not a clamp
old = _data; oldSize = _size; _size = n;
if (_capacity < n) {
    _capacity = n;
    if (n == oldSize + 1 && oldSize != 0) // appending ONE to a NON-EMPTY array
        _capacity = n * 3;                //   trebles; every other case is exact
    _data = Allocate(_capacity * sizeof(T), alignof(T));
}
// copy-construct [0, oldSize) from `old` if we reallocated;
// default-construct [oldSize, n);
// destroy the elements that fell off; Free(old) if we reallocated.
```

Four consequences that are easy to get wrong and that a port must reproduce:

1. **Capacity never shrinks.** `SetSize` only ever grows it; `Clear()` keeps the buffer.
2. **The first append to an empty array allocates capacity 1, not 3** — the `*3` is gated on
   `oldSize != 0`.
3. **A bulk `SetSize(n)` allocates exactly `n`.** Only the +1 append over-allocates, and it sets
   capacity to `3 * newSize`, not `3 * oldCapacity`. Appending one at a time from empty therefore
   walks the capacity sequence **1, 6, 21, 66, 201, …** (`cap = 3n` at n = 2, 7, 22, 67), which is
   distinctive enough for a diff harness to assert on.
4. **Non-trivial `T` is handled properly** — copy-construction on realloc, destruction of the
   truncated tail and of the old buffer.

**Cross-check against our own tree (no conflict):** the exe port has assumed `{data, count, cap}`
for months — `engine/types/EntityList.h::IBPArray` and `engine/types/MapGenerator.h::DynArray`,
both with `static_assert`s at 0x00/0x08/0x0c. The Wave 2 brief asked whether that carries to the
DLL. **It does, verified, not assumed.**

## 5. `Set<T, Allocator, Key, Compare>` — the evidence

**Container, 0x10 bytes.** `Set::Insert` (`FUN_18002a170`) reads the root as `*param_1` and does
`*(int *)(param_1 + 1) += 1` on every successful insert, i.e. an **int32 count at +0x08**.
`Set::Find` (`0x18002ace0`) reads the root the same way. Nothing touches +0x0c.

Independent corroboration from the exe, live in-game for months:
`engine/types/EntityList.h` holds **two adjacent Sets** —
`updateTreeRoot` +0x68 / `updateTreeCount` +0x70, `postUpdateTreeRoot` +0x78 /
`postUpdateTreeCount` +0x80. A 0x10 stride.

**Node = `{ T element; Node* parent; Node* left; Node* right; int16 balance; int16 height; u32 pad }`,
size `align8(sizeof(T)) + 0x20`.** The element comes **first**, so the pointer tail floats with
`sizeof(T)`. Three instantiations at three element widths:

| `sizeof(T)` | Node | parent | left | right | balance | height | Source |
|---|---|---|---|---|---|---|---|
| 8 | 0x28 | +0x08 | +0x10 | +0x18 | +0x20 | +0x22 | exe `EntityTreeNode`, insert at `0x140021a30` |
| 0x10 | 0x30 | +0x10 | +0x18 | +0x20 | +0x28 | +0x2a | `FUN_18000f970`/`FUN_18000fb00`, `Map_Find`, `FUN_18002a310` |
| 0x18 | 0x38 | +0x18 | +0x20 | +0x28 | — | — | `Input::KeyMap::GetIdList` in-order walk |

The 0x10 case is exact and complete. `FUN_18000f970` allocates **0x30 bytes with alignment 8** and
hands them to the node constructor `FUN_18000fb00`, which writes
`[0]=key`, `[1]=value`, `[2]=parent`, `[3]=left=0`, `[4]=right=0`, and then zeroes the **dword** at
+0x28 — i.e. `balance` and `height` together, which is what pins them as two `int16`s in one
aligned dword rather than an `int32`.

The 0x18 case comes from the in-order traversal inlined into `GetIdList`: descend `+0x20` to the
leftmost, successor = leftmost of `+0x28`, else climb `+0x18` — textbook parent-pointer successor,
no threading.

**AVL invariants**, from the retrace/rebalance walk `FUN_18002a310`:

```c
h = node->left  ? node->left->height  + 1 : 0;      // leaf height is 0
h2 = node->right ? node->right->height + 1 : 0;
node->height  = max(h, h2);
node->balance = h2 - h;                             // RIGHT minus LEFT — note the sign
if (node->balance > +1 && node->right) {            // right-heavy
    if (node->right->balance < 0) rotateRight(node->right);
    rotateLeft(node);
}
if (node->balance < -1 && node->left) {             // left-heavy
    if (node->left->balance > 0) rotateLeft(node->left);
    rotateRight(node);
}
node = node->parent;                                // climb to the root
```

The rotation helpers for the 0x10-element instantiation are `FUN_18002a590` (left) and
`FUN_18002a6c0` (right).

**Ordering / duplicates.** `Insert` descends left while `newKey < nodeKey`, right while
`nodeKey < newKey`, and **returns the existing node** when neither holds — `Set` never stores
duplicates, and `Insert` returns the node either way (its hidden return slot is `param_2`). A NULL
key orders below everything. For `String` keys the comparison is an ordinary lexicographic byte
compare, inlined.

**Per-instantiation code.** Because the tail offsets are a function of `sizeof(T)`, MSVC emits a
separate copy of insert/find/rebalance/rotate per instantiation with the offsets baked in. A port
must not assume one `Set` body serves two element widths — this is the same trap as
`reference_one_bst_find_two_node_layouts.md` in the exe port.

## 6. `LinkedList<T, Allocator>` — the evidence

Pinned by `UI::UserInterface::CleanRemovedDialogs` (`0x18011f790`), which unlinks a node from the
dialog list embedded at `UserInterface+0x00`. Every branch of the unlink names one field:

```c
if (node == *(this+0x00)) *(this+0x00) = node[+0x10];        // head; next = +0x10
if (node == *(this+0x08)) *(this+0x08) = node[+0x08];        // tail; prev = +0x08
if (node[+0x08]) *(node[+0x08] + 0x10) = node[+0x10];        // prev->next = next
if (node[+0x10]) *(node[+0x10] + 0x08) = node[+0x08];        // next->prev = prev
*(int*)(this+0x10) -= 1;                                     // count
MemoryAllocator::Free(_singleton, node);                     //  under the +0x640 lock
```

and the search loop compares `*node` against the dialog pointer — the **element is at +0x00**,
ahead of the links — while advancing through `node[+0x10]`.

Container size **0x18**: head +0x00, tail +0x08, count +0x10, and `UserInterface`'s next member —
an `Array<InternalPtr<DialogInstance>>` at +0x18/+0x20 — starts at +0x18. The 4 bytes at +0x14
are never touched.

`Iterator` is **one pointer**. `UI::UserInterface::GetFirstDialog` (`0x180031150`) returns
`LinkedList<...>::Iterator` by value and its entire body is
`*(undefined8 *)retval = *(undefined8 *)this;` — the hidden return slot gets the head pointer.
(Note the Wave 1 ABI rule applies: the return slot is an argument.)

Same decompile also re-confirms `Array`: `AddDialog` (`0x180120ee0`) is
`DynArrayPush(this + 0x28, *(int *)(this + 0x30) + 1, param_2)` — data +0x28, size +0x30.

## 7. `DefaultAllocator` / `AVLCompare` — the evidence

Both are **stateless policy structs** (`U`-prefixed in the mangled names, so `struct` not `class`).

Evidence they carry no storage: **every allocation and free reaches the global
`System::MemoryAllocator::_singleton` directly, and no allocating body ever reads a field of the
container it is allocating for.** `FUN_18000f970` (Set node alloc), `FUN_180034010` and
`0x1800a76c0` (Array grow) and the free in `CleanRemovedDialogs` all name `_singleton` as an
absolute address. A stateful allocator policy would have to be reached through the container.
Size is a weaker second argument in the same direction: `LinkedList` is 0x18 (head/tail/count) and
`Set` is 0x10 (root/count), each with exactly 4 bytes of tail slack — enough for a flag, not for a
pointer.

What `DefaultAllocator` forwards to, from the node/buffer allocators inlined into `FUN_18000f970`
and `FUN_180034010`:

```c
EnterCriticalSection((LPCRITICAL_SECTION)(MemoryAllocator::_singleton + 0x640));
p = System::MemoryAllocator::Allocate(_singleton, n * sizeof(T), alignof(T));
LeaveCriticalSection(...);
```

and the symmetric `MemoryAllocator::Free` under the same lock. The **alignment argument is
`alignof(T)`, not `sizeof(T)`**: `Array<int>` passes 4, `Array<ModInfo>` (a 0x20 struct) passes 8,
`Set` nodes pass 8. Some sites call the `MemAlloc(bytes, align)` free-function wrapper instead of
taking the lock inline; same allocator either way.

This is the coupling PLAN.md §4.3 risk 2 warns about: **every `Collection` container allocates
through `System::MemoryAllocator`**, so Lane C's allocator port and these containers must land
together or not at all.

## 8. Conflicts with our existing tree

**None that change behaviour.** One latent type-width mismatch, reported for the record:

- `engine/types/EntityList.h:73,75` declare the two `Set` counts as `int64_t`
  (`updateTreeCount` +0x70, `postUpdateTreeCount` +0x80). Retail maintains only the **low 32 bits**
  (`*(int*)(set+8) += 1`); +0x0c is padding it never writes. Today this is harmless — every use in
  `code/Game/Entity/entitylist.cpp` (lines 442, 443, 473, 474, 577, 583, 599) is a **read**, and a
  read of a count that never exceeds 2^31 through a zero-initialised upper dword gives the right
  answer. It becomes a bug the day someone **assigns** through it, because that writes 8 bytes and
  clobbers +0x0c. Worth narrowing to `int32_t count; int32_t pad;` when EntityList is next touched;
  I did not edit it, as it is outside this lane's scope.
- `engine/types/MenuScene.h:34-51` spells four `Array`s as `{ptr; void* count|capacity packed}`.
  Same 16 bytes, same offsets, just typed as one qword. Cosmetic, not a conflict.
- `code/Game/Dialog/tradeui.cpp`'s `TradeUI_DynArrayPush(s, ptrOff, countOff, capOff, ...)` takes
  the three offsets explicitly and every call site passes `+0/+8/+0xc` triples. Consistent.

## 9. What is NOT pinned

| Unpinned | Why it matters | What would pin it |
|---|---|---|
| `Set::Iterator` layout | Wave 3+ will iterate Sets | Find any function returning a `Set` iterator by value, or an exported `Begin`/`Next`. None exists in either DLL's export table; look instead for an inlined `for` over a Set in a consumer that keeps the cursor in a struct. |
| The 4 pad bytes at `Set+0x0c` and `LinkedList+0x14` | If they are a second field, a port that leaves them zero diverges | Watch them at runtime with `BANISHED_REDZONE`/`hw_watch.cpp` on a live `UserInterface`, or find a body that reads them. Nothing in ~10 decompiled consumers does. |
| `Set::Remove` / node deletion + rebalance | Every long-lived Set will delete | Decompile the deleter; a good entry is `IO::TextReader`'s `_cachedClasses` teardown, or the exe's tree-erase (`reference_workarea_bst_bad_entry.md` documents the exe-side one). |
| `Array` `Insert`/`Remove`/`Reserve`, if they exist | Retail may have a shift-based remove distinct from `SetSize` | `CleanRemovedDialogs` contains an inlined swap-down remove over a 0x30-stride array (`this+0x48`), which is *a* remove; whether it is `Array::Remove` or hand-written at the call site is not established. |
| `AVLCompare`'s exact interface (3-way vs `<`) | Only affects readability, not layout | It is fully inlined at every site; only the *ordering semantics* above are observable. |
| `Array<T>` behaviour when `T` has a non-trivial default ctor and `SetSize` grows | Observed only for `String` (zero-init) and PODs | A consumer with a `T` whose default ctor does real work. |
| Whether `Array.h`/`Allocator.h` declare anything else | A missing container surfaces in Wave 4+ | Nothing in either binary's symbols or strings suggests a sixth entity; treat a surprise as a finding, not a layout bug. |

## 10. Files

| Path | What |
|---|---|
| `runtime/types/collection/Collection.h` | the templates, with `static_assert`s at three element widths (8 / 0x10 / 0x18) per container, each pinned offset carrying the naming function in a comment |
| `docs/runtime_port/WAVE2_COLLECTION.md` | this file |

Compile-checked standalone:
`g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. -Iruntime/types -Iruntime/funcs` over a TU that
includes only the header. No `<windows.h>`, per PLAN.md §4.1.
