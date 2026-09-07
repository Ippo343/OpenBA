# Wave 3, Lane C — `Object::External` / `ExternalFactory` / `ExternalList`

Recovered and ported 2026-09-06 on `wave/upgrade-path-complete` (base `75c9592`), following
[PLAN.md](PLAN.md) §4.2 (Wave 3: Object) and the per-function recipe in §6.

Files owned and touched by this lane:

| Path | What |
|---|---|
| `runtime/types/object/External.h` | new — the four layouts, `static_assert` on every pinned offset |
| `runtime/src/object/external.cpp` | the port + `RtObjectExternalRegister()` |
| `docs/runtime_port/WAVE3_LANE_C.md` | this file |

Surgical edits **outside** those files, made under PLAN §6 step 8 (fix the stale claim where it
is written) and flagged in §7 below: `engine/funcs/engine_funcs.h`,
`code/Game/Component/builddescription.cpp`, `code/Game/Component/createplaceddescription.cpp`,
`code/Game/Game/gameupdateloop.cpp`.

---

## 1. Result

| | |
|---|---|
| Registered | **11 functions**, each under BOTH its mangled name and its retail address |
| Diff sites | **8** — every read-only function in the lane has a real oracle |
| Not ported | `FindOrLoad`, `Clean`, `SerializeReferenceTable`, `SaveText`, and the two `_typeInfo` data symbols — each with a stated reason (§5) |
| Phantom names | **0 of 17.** All 17 names in the brief exist in the shipped DLL's export table (checked with `pefile`) |
| ABI audit | `tools/abi_retslot_audit.py`: 0 suspicious of 49. The three by-value returners in this lane already had correct `fn_*` typedefs |
| Compile | `g++ -std=c++17 -fsyntax-only -Wall -Wextra …` clean |
| Census | Object 21/41 (51.2%) after this lane plus lanes A and B; 11 of the 21 are this lane's |

The 17 export addresses, all verified against `C:\GOG Games\Banished\Runtime-x64.dll` with

```
tools/ghidra_mcp/.venv/Scripts/python.exe   # pefile over DIRECTORY_ENTRY_EXPORT
```

| Mangled name | Retail | Ord | Status |
|---|---|---|---|
| `??0External@Object@@QEAA@XZ` | `0x1800bc9c0` | 178 | PORTED |
| `??1External@Object@@UEAA@XZ` | `0x1800bc950` | 588 | PORTED |
| `?GetPath@External@Object@@QEBA?AVFilename@System@@XZ` | `0x180006250` | 2835 | PORTED, DIFFED |
| `?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z` | `0x1800bb9d0` | 3614 | not ported (Wave 4) |
| `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` | `0x1801f9b38` | 4859 | data, deliberately left on the DLL |
| `?Clean@ExternalFactory@Object@@QEAAXXZ` | `0x1800be310` | 1701 | not ported (no oracle) |
| `?EnableReferenceMode@ExternalFactory@Object@@QEAAX_N@Z` | `0x1800bcf40` | 2298 | PORTED |
| `?IsReferenceModeEnabled@ExternalFactory@Object@@QEBA_NXZ` | `0x1800bf0f0` | 3154 | PORTED, DIFFED |
| `?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@AEBVFilename@System@@@Z` | `0x1800bdeb0` | 2337 | PORTED, DIFFED |
| `?FindNext@ExternalFactory@Object@@QEBAPEBVExternal@2@IPEBV32@@Z` | `0x1800be9d0` | 2355 | PORTED, DIFFED |
| `?FindOrLoad@ExternalFactory@Object@@QEAAPEBVExternal@2@AEBVFilename@System@@@Z` | `0x1800bf320` | 2359 | not ported (no oracle) |
| `?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z` | `0x1800bf100` | 2862 | PORTED, DIFFED |
| `?GetPathForReferenceId@ExternalFactory@Object@@QEBA?AVFilename@System@@I@Z` | `0x1800bf1d0` | 2839 | PORTED, DIFFED |
| `?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` | `0x1800bcb60` | 3806 | not ported (Wave 4) |
| `?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A` | `0x1801f9be0` | 4861 | data, deliberately left on the DLL |
| `?Find@ExternalList@Object@@QEBA?AV?$ExternalPtr@VExternal@Object@@@2@AEBVFilename@System@@VType@2@@Z` | `0x1800bf700` | 2338 | PORTED, DIFFED |
| `?GetResourceCount@ExternalList@Object@@QEBAHXZ` | `0x180007650` | 2869 | PORTED, DIFFED |

Supporting retail addresses reversed but not ported: `?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z`
`0x1800be570`, `?ExpandRelativePath@…@QEBA?AVFilename@System@@AEBV34@_N@Z` `0x1800bd360`,
`?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z` `0x1800bc620`,
`?EnableLoad@…@QEAAX_N@Z` `0x1800bcaf0`, `?EnableSimpleLoad@…@QEAAX_N@Z` `0x1800be9c0`,
`??0ExternalFactory@Object@@QEAA@XZ` `0x1800be7e0`, `??_7External@Object@@6B@` `0x1801547a0`,
`??_7Base@Object@@6B@` `0x180154788`, `??_7ExternalList@Object@@6B@` `0x180154800`,
`?_singleton@ExternalFactory@Object@@0PEAV12@EA` `0x1801fc6c8`,
`?_singleton@Factory@Object@@0PEAV12@EA` `0x1801fc6e8`.

---

## 2. The recovered layouts

Full per-field commentary lives in `runtime/types/object/External.h`. Summary with the naming
function for each pin:

### `Object::External` — 0x20 bytes, confidence HIGH

| Off | Field | Pinned by |
|---|---|---|
| +0x00 | vtable | `??0External` `0x1800bc9c0` stamps `??_7Base@Object@@6B@` then `??_7External@Object@@6B@`. Base is therefore **8 bytes, a vtable pointer and nothing else** |
| +0x08 | `System::Filename _path` | `GetPath` `0x180006250` is one line: `Filename::Filename(ret, (Filename*)(this + 8))` |
| +0x10 | `Collection::Array<T>` (`_data` +0x10, `_size` +0x18, `_capacity` +0x1c) | `~External` `0x1800bc950` hands `this+0x10` to `FUN_180006b80`, which is the Array teardown; `FUN_180006ac0` walks it at a stride of **0x10** releasing only each element's leading String |

The array's element type is **NOT pinned** — nothing in this lane's scope writes it, only the
destructor reads it. It is declared as an opaque `{char* owned; uint64_t opaque;}` pair so the
sizes are right and the destructor can be transcribed exactly. `Object\External.cpp` exists in the
profile build's source-tree strings, so the name is recoverable later.

Virtual slots used by this lane, both named by two independent uses:

* **+0x10 `GetTypeId() -> uint32`** — `FindNext` `0x1800be9d0`, `SaveText` `0x1800bb9d0`.
* **+0x28 `Serialize(IO::Stream&)`** — `SaveText` drives it with an `IO::TextFileStream` to write
  the object out as text; `ExternalFactory::Add` `0x1800be570` drives it with an
  `IO::VisitorStream` to walk the object's resource references. This is the slot the exe port's
  `Reimpl_*_Serialize` bodies implement, and §4 explains why it matters.

### `Object::ExternalFactory` — confidence HIGH for everything asserted

`??0ExternalFactory` `0x1800be7e0` is a field-by-field pin: it writes a qword at +0x00, an int32
at +0x08, a qword at +0x10, an int32 at +0x18, a qword at +0x20, an int32 at +0x28, qwords at
+0x30 and +0x38, an **int16** at +0x40 (so +0x40 and +0x41 are two adjacent bools) and a byte at
+0x42, then assigns `_singleton`.

| Off | Field | Pinned by |
|---|---|---|
| +0x00 | `int32 _cleanBitIndex` (SIGNED) | `Clean` `0x1800be310` reads it signed; `Add` `0x1800be570` sets `1 << (idx & 31)` |
| +0x08 | `int32 _generation` | `Clean` increments it once per sweep |
| +0x10 | `Set<ResourceEntry> _resources` (root +0x10, count +0x18) | `Find`, `FindNext`, `Add`, `Clean`, `SerializeReferenceTable` |
| +0x20 | `Set<ReferenceEntry> _references` (root +0x20, count +0x28) | `GetPathForReferenceId` `0x1800bf1d0`, `SerializeReferenceTable` `0x1800bcb60` |
| +0x30, +0x38 | **NOT PINNED** — zeroed by the ctor, read by nothing in scope | — |
| +0x40 | `bool _referenceModeEnabled` | `EnableReferenceMode` / `IsReferenceModeEnabled` |
| +0x41 | `bool _loadEnabled` | `?EnableLoad@…@QEAAX_N@Z` `0x1800bcaf0`; gates `FindOrLoad`'s cache-hit re-`Add` |
| +0x42 | `bool _simpleLoadEnabled` | `?EnableSimpleLoad@…@QEAAX_N@Z` `0x1800be9c0` |

**`_generation` at +0x08 confirms an existing comment.** `engine/funcs/engine_funcs.h`'s
`ResolveExternalByName_Impl` documents the exe-side `ExternalRef` as
`{String name; int32 cachedGeneration; External* cached;}` with the generation "compared against
ExternalFactory+0x08". That is exactly right, and `Clean` is the only thing that bumps it — so an
`ExternalRef` cache is invalidated by a `Clean` sweep and by nothing else.

### The two `Set` instantiations — and the node trap, hit immediately

WAVE2.md's closing note said Wave 3 would meet
`reference_one_bst_find_two_node_layouts.md` immediately. It does, inside one class:

| Tree | Element | sizeof | Node | parent / left / right |
|---|---|---|---|---|
| `_resources` | `{Filename path; External* ext; uint32 usageMask; uint32 referenceId;}` | **0x18** | 0x38 | +0x18 / +0x20 / +0x28 |
| `_references` | `{uint32 id; pad; Filename path;}` | **0x10** | 0x30 | +0x10 / +0x18 / +0x20 |

Both sets of offsets are read straight out of retail: `FindNext` and `Clean` walk `+0x18/+0x20/+0x28`
on the resource tree; `GetPathForReferenceId` descends `piVar5[6]` and `piVar5[8]` on an `int*`,
i.e. `+0x18` and `+0x20`, on the reference tree. `WAVE2_COLLECTION.md`'s formula
(`node = align8(sizeof T) + 0x20`, element first) predicts both exactly — **the Collection layout
is confirmed a third and fourth time by an independent consumer.**

`ResourceEntry::referenceId` at +0x14 is pinned twice over: `GetReferenceId` returns
`*(uint*)(node + 0x14)` and `SerializeReferenceTable` writes `*(int*)(node + 0x14) = id` on both
its load and its save path.

### `Object::ExternalPtr<T>` — ONE RAW POINTER, no refcount

`ExternalList::Find` returns one by value and its entire payload write is a single qword store
(`*retval = _data[i]` on a hit, `*retval = 0` on a miss). Nothing bumps a refcount on the way out.
Ownership lives in the factory, never in the pointer — which is why a `Clean` sweep leaves every
cached `ExternalPtr` dangling (see the corrected `fn_ExternalFactory_Clean_t` comment).

### `Object::ExternalList` — an `External` plus a raw array, NOT a `Collection::Array`

| Off | Field | Pinned by |
|---|---|---|
| +0x00 | `External _base` | derived class; base is 0x20, and the derived part starts exactly there |
| +0x20 | `External** _resources` | `Find` `0x1800bf700`: `*(qword*)(*(qword*)(this+0x20) + i*8)` |
| +0x28 | `int32 _count` | `GetResourceCount` `0x180007650` is `return *(int*)(this+0x28)` |

Both offsets are **exactly** where `Collection::Array<External*>` would put them
(`_data` +0x20, `_size` +0x28, `_capacity` +0x2c), so this is almost certainly that container
rather than a hand-rolled pair — a fifth independent confirmation of `WAVE2_COLLECTION.md`'s
`Array` layout. The header still declares plain fields, because only two of the three offsets are
pinned: nothing in scope reads or writes +0x2c, and asserting a capacity field no decompiled body
touches would be a guess dressed as a layout.

---

## 3. What was ported, and how each is diffed

| Function | Diff shape |
|---|---|
| `External::GetPath` | `RT_DIFF_BYTES` on two scratch `Filename`s. Ours and theirs both copy from the same `_path`, so the two results are the **same `char*`** and an 8-byte compare is exact. The oracle's extra reference is released. |
| `ExternalFactory::IsReferenceModeEnabled` | `RT_DIFF_SCALAR` |
| `ExternalFactory::Find` | `RT_DIFF_SCALAR` on the returned `External*` |
| `ExternalFactory::FindNext` | `RT_DIFF_SCALAR` on the returned `External*` |
| `ExternalFactory::GetReferenceId` | `RT_DIFF_SCALAR` on the `uint32` |
| `ExternalFactory::GetPathForReferenceId` | `RT_DIFF_BYTES` on scratch `Filename`s |
| `ExternalList::GetResourceCount` | `RT_DIFF_SCALAR` |
| `ExternalList::Find` | `RT_DIFF_BYTES` on scratch `ExternalPtr`s |
| `External::External` / `~External` | **not diffed** — running the original too would stamp the vtable twice (harmless) and release the path's refcount twice (a double free at refcount 1) |
| `ExternalFactory::EnableReferenceMode` | **not diffed** — a single byte store into live state; a site whose only content is "we both wrote the same constant" is the non-evidence WAVE2 finding 5 warns about |

Every one of the eight diffable functions is genuinely const on the factory, so the oracle call is
safe to run for real rather than into a replica. The only side effects either implementation has
are transient refcount bumps that both sides balance, and `AdjustDelimiters`, which is idempotent.

**Registration is by NAME *and* by ADDRESS for all eleven.** Name-only would have left two sites
inert: `code/Game/Tutorial/tutorialgoaldialog.cpp` reaches the `External` ctor and dtor through
`ResolveAddr(Module::Runtime, 0x1800bc9c0 / 0x1800bc950)` and never through their names. That is
WAVE2 finding 4 seen from the other side — there the name was a phantom and the address fallback
carried the calls; here the names are all real but a caller chose the address anyway.

The ctor/dtor stamp the DLL's own vtables **by name** (`??_7External@Object@@6B@`,
`??_7Base@Object@@6B@`), the Wave 2 `Thread` precedent. The per-class safety argument PLAN §2.2
hazard D demands is easy here for a different reason than Thread's: this port replaces no virtual
at all, so it only has to write the address retail writes.

---

## 4. `FindOrLoad`'s caching semantics — stated explicitly, because a wrong belief about them has cost this project time

The brief asked for this in as many words. Reversed from `0x1800bf320` plus `Add` `0x1800be570`:

1. **An empty path returns null immediately** (`path.data == 0 || length == 0`).
2. `ExpandRelativePath(this, &expanded, <byte from the path>)` normalises the argument.
3. `Find(this, expanded)`:
   * **HIT.** `FindOrLoad` returns the cached `External`. **It does not reload, re-open or
     re-parse the file.** The only extra work is `Add(this, found)`, and *only when
     `_loadEnabled` (+0x41) is set*.
   * **MISS.** Decompose the path (`GetPathExtension`, `GetPathIdentifer`,
     `GetPathFilename(strip=true)`, `GetPathDirectory`), build
     `FileSystem::GetDataDirectory() + dir + name + …` through five chained `operator+`s, then
     `External::Load(&absolutePath, nullptr)` (`0x1800bc620`). On success the new object's
     `_path` is **overwritten with the expanded RELATIVE path**, `AdjustDelimiters`'d, and
     `Add`ed. So the tree is keyed on the relative path even though the file was opened by an
     absolute one.
4. A path with no `:identifier` suffix is looked up as `path + ":resource"` — the literal at
   `0x180155ae0`. `"Template/WoodHouse.rsc"` and `"Template/WoodHouse.rsc:resource"` are the same
   key. The suffix names a sub-resource inside the file.
5. Lookup is **case-INSENSITIVE** (`_stricmp` at every tree comparison).

And `Add`, which is where the "Serialize re-runs" effect actually lives:

```
node = resources.Find(external->GetPath())
if (node) {
    bit = 1 << (factory->_cleanBitIndex & 31)
    old = node->usageMask
    node->usageMask = old | bit
    if ((old & bit) == 0) {                     // FIRST touch this generation only
        IO::VisitorStream vs;
        external->vtable[+0x28](external, &vs)  // Serialize, against a VISITOR
    }
} else {
    resources.Insert({path, external, usageMask = bit, referenceId = -1})
}
```

**So the re-invoke is once per resource per clean generation, not once per `FindOrLoad`, and the
stream it runs against is an `IO::VisitorStream` — a reference-VISIT pass, not a re-read of the
file.** That is precisely why the re-passes carry no data and a `Serialize` body that zeroes
before reading ends up with an empty array. This is the mechanism behind
`reference_createplaced_placebitarray_zeroed_by_reparse.md`, now named rather than inferred.

**One thing this lane could NOT reconcile, and is flagging rather than papering over.**
`code/Game/Component/builddescription.cpp` records a *measurement*: the CreateFenced Pasture's
`:build` sub-resource was `FindOrLoad`'d **six times**, the first pass populating
`{RawMaterialWood, 1}` and five later passes clobbering it to `{0,0}`. The mechanism above allows
at most one re-`Serialize` per clean-bit generation, so six clobbering passes require either six
generations, six distinct keys resolving to the same `External`, or a `_cleanBitIndex` that moves.
**A measurement outranks my reading of a decompile.** The comment in `builddescription.cpp` has
been corrected to state the mechanism precisely while explicitly preserving the recorded 6× count
and saying that neither number has been re-measured against the other. Re-measuring it is worth a
short follow-up: instrument `Add` and log `(_cleanBitIndex, usageMask before/after)` per call.

`Clean` `0x1800be310`, the other half of the same story:

```
in-order walk the resource tree:
    if (_cleanBitIndex >= 0) node->usageMask &= ~(1 << (_cleanBitIndex & 31))
    if (node->usageMask == 0 || _cleanBitIndex < 0) collect node->path
for each collected path:
    node = resources.Find(path)
    node->external->vtable[0](external, /*deleteFlag=*/1)   // the object is FREED
    resources.Erase(node)
factory->_generation += 1
```

A negative `_cleanBitIndex` means "evict everything unconditionally". Every cached `External*`
anywhere — including every `ExternalPtr`, which holds no reference — is dangling the instant
`Clean` runs, and the `_generation` bump is the only signal the exe's `ExternalRef` caches get.

---

## 5. What was deliberately NOT ported

### `ExternalFactory::FindOrLoad` and `ExternalFactory::Clean` — NO ORACLE, so NOT REGISTERED

Both rewrite the resource tree. Porting them means porting `Collection::Set` insert, erase and
rebalance **for the 0x18-wide element specifically** (the node trap forbids reusing another
width's body), plus `ExpandRelativePath`, `External::Load` and `Add`. That is a large body of code
whose correctness this wave has no way to establish: running both implementations to compare would
insert or erase in the one shared tree twice, and `RT_DIFF_SHADOW` against a private replica is
exactly the "private replica the DLL can desynchronise" shape that PLAN §4.1 decision 5 and the
wave brief both forbid, because the DLL's own internal calls keep driving the real tree.

This is the Wave 2 allocator situation without the mitigating detail that made the allocator
portable: the allocator port drove the live singleton's own metadata, so one heap and one metadata
set. A ported `Set::Insert` would also drive the live tree — but a divergence there is a corrupted
AVL tree holding every resource in the game, discovered later and elsewhere. The allocator landed
opt-in with a NO-GO verdict and never executed an instruction. Landing these two the same way
would add the same unmeasured code with the same verdict; the honest thing is to reverse them,
write the mechanism down where the next reader will look (done — §4, and
`engine/funcs/engine_funcs.h`), and leave them on the DLL.

**Verdict: NO-GO, and not even opt-in.** They should land with Wave 4/5, together with a ported
`Set` for the 0x18 element and a shadow-tree checker that walks both trees for structural equality
after every mutation — which is a real oracle, unlike a `RT_DIFF_SHADOW` marker that asserts
nothing.

### `ExternalFactory::SerializeReferenceTable` and `External::SaveText` — WAVE 4 DEPENDENCIES

Reversed in full; neither can be written without IO.

`SerializeReferenceTable` `0x1800bcb60`:

1. Destroy the `_references` tree wholesale (`FUN_18002ebf0`), then zero root and count.
2. Build a serializer from the stream: `*(int*)(stream+8) == 0` selects **loading**, in which case
   vtable slot `+0x18` of `*(stream+0x10)` creates a reader; otherwise slot `+0x20` creates a
   writer.
3. Serialize the entry count (serializer slot `+0xa0`) from `_resources._count`.
4. **Loading:** for each entry, serialize an int id (slot `+0x98`) and a `String` path
   (`String::Serialize`), insert `{id, path}` into `_references`, then look the path up in
   `_resources` and, if present, write `node->referenceId = id`.
5. **Saving:** ids start at **1**; in-order walk `_resources` assigning `node->referenceId = id++`
   and serializing `(id, GetPath())` per node. The saving path leaves `_references` **empty** —
   it was cleared at step 1 and never refilled.

That last point independently confirms the comment already in
`code/Game/Game/gameupdateloop_serialize.cpp:976` ("`+0x28` is the reference map's entry count
(SerializeReferenceTable zeroes …)"). It was right.

`SaveText` `0x1800bb9d0` looks the object's type name out of `Object::Factory`'s type map, opens an
`IO::TextFileStream` on it, `FileStream::Create`s the target, writes a BOM for encodings 3/4/5
(UTF-8 `EF BB BF`, UTF-16LE `FF FE`, UTF-16BE `FE FF`) via `WriteFile`, then calls the object's
`Serialize` (vtable `+0x28`) against that text stream. It reaches `IO::TextFileStream`,
`IO::FileStream`, `IO::FileSystem` **and Win32 directly**, so under PLAN §4.1 decision 1 the Win32
half belongs in a `_win.cpp` behind a platform-neutral header. Wave 4 territory, whole.

**A forwarder was considered and declined for both.** Wave 2 landed `Time::AsSeconds` as one, and
the census counts forwarders as ported. Here a forwarder would add an indirection and a census row
for zero behaviour change and would make Object's percentage overstate what has actually moved —
the same overstatement WAVE2 finding 5 was written to stop. They are simply not registered.

### The two `_typeInfo` data symbols — LEFT ON THE DLL, ON PURPOSE

`?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` (`0x1801f9b38`) and
`?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A` (`0x1801f9be0`) are static `BaseTypeInfo`
**objects**, not code. The DLL has already linked them into `Object::Factory`'s `_firstTypeInfo`
chain and assigned them runtime type ids. Registering our own storage under those names would hand
every one of our call sites a second, unregistered `BaseTypeInfo` whose type id no DLL-built object
would ever match — the failure mode of
`reference_unregistered_typeid_filter_zeroes_the_target.md`, with a healthy non-null pointer to
reassure the reader for hours. They stay on the DLL until the whole reflection system moves as a
unit (Lane A), which is the atomic-per-class rule of PLAN §4.1 decision 5.

Consumers today: `standalone/reg_unreg_lane1.cpp:128` and
`code/Game/Game/gameupdateloop_load.cpp:280,354`. Both resolve through `ResolveExport`, so both
keep getting the DLL's object, which is what they want.

---

## 6. Defects and findings

### A. `ExternalFactory::GetReferenceId` dereferences a NOT-FOUND node — PRE-EXISTING, retail, left unguarded

The tail of `0x1800bf100` is literally

```
MOV EAX, dword ptr [RBX + 0x14]     ; RBX = whatever Set::Find returned
OR  ECX, -1
CMP EAX, ECX
CMOVZ EAX, ECX
```

with **no test of RBX**. An `External` that is not in the resource tree faults reading `0x14`.
Left unguarded per CLAUDE.md ("don't 'fix' a faithful crash with a null check") — a guard would not
restore the behaviour, it would skip it, and every current caller passes an `External` the factory
owns. **If the integration run ever faults on a READ of `0x14`, the bug is upstream: something
handed us an `External` the factory never `Add`ed.**

The `CMP/CMOVZ` pair is a genuine no-op (`x == -1 ? -1 : x`) and is transcribed as the plain read
it compiles to.

`ExternalFactory::FindNext` carries the same shape one level down: retail does not null-check the
`Factory` type-map lookup either, so a type id absent from the map falls out of the search loop
with a null node and is then read at `+0x08`. Also transcribed unguarded.

### B. `ExternalList::Find` ignores its `Type` argument — PRE-EXISTING, and a stale comment named it wrong

R9 is dead through the whole body (verified by filtering the disassembly for every `MOV/CMP/TEST/LEA`
— the only `R9` appearance is `MOV R9,RBX` as scratch inside the inlined `strcmp`). The list is
searched **by path alone** and the first path match is returned whatever its type. A caller passing
a type expecting it to filter gets no filtering.

`code/Game/Game/gameupdateloop.cpp:395` typed the parameter `void* typeInfo`, which implied both a
pointer and an effect, and neither holds — it is `Object::Type`, by value. **Corrected at source.**

### C. `ExternalFactory::Find` is case-INSENSITIVE; `ExternalList::Find` is case-SENSITIVE — PRE-EXISTING, undocumented

Over the same kind of resource path. The factory tree compares with `_stricmp` at every node; the
list does a linear scan comparing the `str-8` length header first and then a plain byte `strcmp`.
Both are transcribed as found. Do not "harmonise" them: `Terrain\Grass.rsc` and
`terrain\grass.rsc` are one key in the factory and two different strings in a list.

### D. `Object::Base` is 8 bytes — a fact Lane A should check against

`??0External` writes the Base vtable at +0x00 and then External's own over the top, and the very
next thing it writes is `_path` at +0x08. So `Object::Base` has a vtable pointer and no data
members. Flagged for Lane A; if their evidence says otherwise, theirs wins and this port's
`External` offsets need re-checking, because everything in this lane hangs off `_path` being at
+0x08.

### E. No port bugs found

Nothing in this lane's own transcription is known to be wrong. That is a statement about review,
not about measurement: **this lane has not run the game.** Every number above comes from a
decompile, `pefile`, a compiler and the two audit tools. The diff oracle has never executed. Per
WAVE2 finding 5 and `reference_uncalled_port_is_not_verified.md`, treat this lane as UNVERIFIED
until the integration run reports non-zero `calls=` on its eight sites.

---

## 7. Comments corrected at source (PLAN §6 step 8)

All four are outside this lane's owned files and are called out here as the brief requires.

| File | Was | Is |
|---|---|---|
| `code/Game/Component/builddescription.cpp:68` | "The engine re-invokes this Serialize on **EVERY** FindOrLoad of an already-loaded resource." | The mechanism, precisely: `FindOrLoad` does not reload a cached resource; `Add` re-invokes vtable `+0x28` against an `IO::VisitorStream` **only when the clean bit was not already set**, i.e. once per resource per generation. The recorded 6× Pasture measurement is preserved verbatim with a note that it has not been re-measured against this mechanism. |
| `code/Game/Component/createplaceddescription.cpp:98` | "re-invoked on every ExternalFactory::FindOrLoad" | points at `Add`'s VisitorStream pass and at the corrected note above |
| `engine/funcs/engine_funcs.h` `fn_ExternalFactory_Clean_t` | "cleanup resources for freed bits" — the mechanism named without saying what it does | the full sweep: clear the bit, collect zero-mask entries (or everything when the index is negative), destroy each object through vtable slot 0 with `deleteFlag=1`, erase the node, bump `_generation`. Plus the consequence: every cached `External*`/`ExternalPtr` is dangling afterwards. |
| `engine/funcs/engine_funcs.h` `fn_ExternalFactory_FindOrLoad_t` | no comment at all | the five caching facts of §4 |
| `code/Game/Game/gameupdateloop.cpp:395` | `void* typeInfo` — implies a pointer and an effect | `void* type`, plus "THE 4th ARGUMENT IS NEVER READ", the case-sensitivity asymmetry, and the `":resource"` convention |

**Comments checked and found CORRECT — no change needed:**

* `engine/funcs/engine_funcs.h` `ResolveExternalByName_Impl`: `ExternalRef+0x08` is the cached
  generation compared against `ExternalFactory+0x08`. Confirmed; `Clean` is the only writer.
* `code/Game/Game/gameupdateloop_serialize.cpp:976`: "+0x28 is the reference map's entry count
  (SerializeReferenceTable zeroes …)". Confirmed.
* `code/Game/Dialog/debugui.cpp:1271`: "GetResourceCount(null) faults reading null+0x28".
  Confirmed — the count is at +0x28.
* `engine/funcs/engine_funcs.h:1943` `fn_External_GetPath_t(self, retBuf)` and `:5758`
  `fn_ExternalFactory_GetPathForReferenceId_t(self, retFilename, refId)`: both carry the hidden
  return slot correctly.

---

## 8. Overlaps to merge after the wave

* **`BaseTypeInfo` / `Object::Factory` (Lane A).** `External.h` ends with a
  `FactoryTypeNodeOffsets` block declaring the *three* offsets `FindNext`'s inlined
  `IsDerivedFrom` needs, transcribed **without interpretation**: the type map's Set element is
  0x30 bytes with a `uint32` id at +0x00 (node tail left +0x38 / right +0x40); element +0x10 is a
  pointer whose own +0x08 is the type-name `char*`; element +0x08 begins a singly linked ancestor
  chain of `{uint32 id at +0; next at +0x10}` nodes. This lane makes **no claim** about what those
  fields are called — in particular it does not claim +0x10 is a `BaseTypeInfo*`, only that its
  +0x08 is the name string. Lane A's evidence wins wherever the two disagree. **Delete
  `FactoryTypeNodeOffsets` and use Lane A's header once the wave merges.**
* **`Object::Base` is 8 bytes** — finding D above.
* `Object::Factory::_singleton` is resolved by name here
  (`?_singleton@Factory@Object@@0PEAV12@EA` — note the `EA`, not `A`; the obvious guess would have
  been a phantom) so this lane does not hardcode a second copy of the address. Lane A should
  export an accessor and this port should switch to it.

## 9. Not done, and why

* **No Ghidra renames.** The `rename_function_by_address` MCP tool takes no `program` argument, and
  the brief forbids `switch_program` on a three-lane shared server. Renaming would have applied to
  whichever program happened to be current — which PLAN §3.4 says is `Runtime-x64-profile.dll`, not
  retail. The names worth applying to retail when it is safe to do so:
  `0x18002dd70` → `Collection_Set_Find_Elem24`, `0x180006b80` → `Collection_Array_Destroy_Elem16`,
  `0x180006ac0` → `Collection_Array_DestroyElements_Elem16`, `0x18002ca00` →
  `Collection_Set_Insert_Elem24`, `0x18002cb90` → `Collection_Set_Insert_Elem16`, `0x18002cf60` →
  `Collection_Set_Erase_Elem24`, `0x18002dff0` → `Collection_Array_Push_Filename`, `0x18002ebf0` →
  `Collection_Set_DestroyAll_Elem16`.
* **No detour installed**, as in Waves 1 and 2. Registration redirects our call sites only; the
  DLL's own internal `Find`/`GetPath` calls still land in the DLL. Safe here for the same reason it
  was safe in Wave 2: these ports share the DLL's allocator and its exact layouts, and none of them
  writes factory state.
* **No profile-build lookup was needed.** PLAN §6 step 1 puts the profile build first because its
  debug strings win over invented names. All 17 functions in this lane are exported from retail
  under their real mangled names, so retail already carries the engine's own naming; the profile
  build was consulted only for the source-tree file list (`Object\External.cpp`,
  `Object\ExternalFactory.cpp`, `Object\ExternalList.cpp`, which is why all three classes live in
  one `runtime/src/object/external.cpp` rather than three files — a deliberate departure from the
  original tree, since the wave scaffolding gives each lane exactly one file).
