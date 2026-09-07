# Wave 3, lane B — `Object::Internal` / `InternalBasePtr` / `InternalFactory`

Ground-truthed 2026-09-06 on `wave/upgrade-path-complete` (base `75c9592`). Every
address below came from a live Ghidra query against `Runtime-x64.dll` or
`Runtime-x64-profile.dll`, or from `pefile` over the two shipped DLLs. Nothing here is
inferred from a name.

**Bottom line.** The whole class family is reversed and ported, and the file compiles
clean. Five functions with no shared state are **registered by default**. The four that
mutate the handle table plus `InternalBasePtr::Serialize` are **landed switched off**
with an explicit **NO-GO**, exactly as Wave 2 landed the allocator — but unlike the
allocator lane, the oracle that would flip them to GO **exists, is registered, and needs
no new `standalone/` file**: `BANISHED_RT_INTERNAL_SHADOW=1 BANISHED_RT_DIFF=1` on a
normal `sa-run` produces the numbers. **No shadow numbers are claimed here. The mode has
never executed** — this lane was not permitted to build or run.

`CheckPtr` is ported and deliberately **left unregistered**; registering it is a
measurable regression, not a neutral act. §7.1.

---

## 1. What the handle table actually is

| | |
|---|---|
| `sizeof(Object::Internal)` | **0x10** — vptr, `_factoryIndex:i32` @ +0x08, `_generation:u32` @ +0x0c |
| `sizeof(Object::InternalBasePtr)` | **0x08** — one raw `Internal*`. Not refcounted, not weak, not validated |
| `sizeof(Object::InternalFactory)` | **0x9e8** (0x9e4 of fields + 4 pad; alignof 8) |
| Factory +0x00 | `Collection::Array<Internal*> _objects` — the table |
| Factory +0x10 | `Collection::Array<int32_t> _persistIds` — save/load id remap, `-1` = unresolved |
| Factory +0x20 | `System::Random _random` (0x9c4) — the generation source |
| Singleton | `?_singleton@InternalFactory@Object@@0PEAV12@EA`, exported **data**, 0x1801fc6d8 |
| Vtable | `??_7Internal@Object@@6B@` **exported**, 0x180154720, 4 slots |

Full layout, per-field evidence and the vtable dump: `runtime/types/object/Internal.h`,
every offset `static_assert`ed.

**Evidence for the size**, because it is the one number a reader will want to check:
`??0InternalFactory@Object@@QEAA@AEBV01@@Z` (0x18002f890) copy-assigns the two arrays and
then copies the tail from +0x20 with a 16-byte-aligned unrolled loop — 19 × 0x80, then
4 × 0x10, then a trailing dword — whose non-aligned fallback is literally
`memcpy(dst+0x20, src+0x20, 0x9c4)`. `System::Random` is 0x9c4 (Wave 2,
`runtime/types/system/Value.h`), so the fields end at 0x9e3.

**The table invariant** is `for all i in [0, _size): _objects[i]->_factoryIndex == i`,
maintained by exactly two functions:

```
AddObject     (0x1800c00c0)  obj->_factoryIndex = _size;
                             obj->_generation   = _random.Rand();
                             _objects.SetSize(_size + 1, obj);

RemoveObject  (0x1800c0110)  last = _size - 1;  idx = obj->_factoryIndex;
                             if (last != idx) { _objects[idx] = _objects[last];
                                                _objects[idx]->_factoryIndex = idx; }
                             _objects.SetSize(last);
```

Retail **inlines both** into `Internal::Internal` / `~Internal`; the profile build calls
them out of line, which is how the inline is proved rather than guessed.

`RemoveObject` is the swap-remove from
`reference_internalfactory_swapremove_wild_write.md`. Confirmed in both builds: **there
is no validation of `idx`.** The profile build — asserts compiled in — asserts only that
`_singleton != NULL`. So the wild write that memory note describes is not a bug we can
guard here; it is what the engine does when handed a stale index, and the fix stays
upstream (unregister before freeing). This port keeps the deref unguarded.

`code/Game/Object/internal_factory_audit.h`'s description of the table is **correct in
every particular** — offsets, count, capacity, the Random at +0x20, both failure modes,
and its note that `CheckPtr` is empty. It is the one document in the tree this lane did
not have to correct.

---

## 2. Per-function map

| Export | Retail | Profile | Body | Tier |
|---|---|---|---|---|
| `??0InternalBasePtr@Object@@QEAA@XZ` | 0x1800062c0 | 0x180023900 | `*this = 0` | 1 |
| `??0InternalBasePtr@Object@@QEAA@PEAVInternal@1@@Z` | 0x1800062a0 | 0x1800238e0 | `*this = arg` | 1 |
| `?Copy@InternalBasePtr@Object@@IEAAXAEBV12@@Z` | 0x1800062b0 | 0x1800257d0 | 8-byte copy | 1 |
| `?DeletePtr@InternalBasePtr@Object@@QEAAXXZ` | 0x1800c0150 | 0x18008c5a0 | `vtbl[0](p,1); ptr=0` | 1 |
| `?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z` | 0x1800bfda0 | 0x18008b490 | **empty** | 1 |
| `?CheckPtr@InternalBasePtr@Object@@IEBAXXZ` | 0x1800c0290 | 0x18008c5d0 | **`RET 0`** | opt-in, §7.1 |
| `??0Internal@Object@@QEAA@XZ` | 0x1800bfd10 | 0x18008b370 | Base + AddObject | 2 |
| `??1Internal@Object@@UEAA@XZ` | 0x1800bfdb0 | 0x18008b400 | RemoveObject + ~Base | 2 |
| `?ClearPersistState@InternalFactory@Object@@QEAAXXZ` | 0x1800bfe30 | 0x18008c0b0 | free + zero | 2 |
| `?ResetPersistState@InternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` | 0x1800bfe40 | 0x18008bfc0 | Serializer + resize + fill −1 | 2 |
| `?Serialize@InternalBasePtr@Object@@QEAAXAEAVSerializer@IO@@@Z` | 0x1800c0180 | 0x18008c460 | 4 stream fields + PersistObject | 2 |
| `?_singleton@InternalFactory@Object@@0PEAV12@EA` | 0x1801fc6d8 | 0x1802568a0 | **data, not code** | — |

Supporting bodies read but **not in this lane's scope**, recorded so the next reader does
not re-derive them: `AddObject` 0x1800c00c0, `RemoveObject` 0x1800c0110, `PersistObject`
0x1800bff10, `InternalFactory` ctor 0x1800c0030, `Startup` 0x1800c0020, `Shutdown`
0x1800c0010 (**empty**), `AssignId` 0x1800c0280 (**`RET 0`**),
`IO::Serializer::Serializer` 0x1800b9960.

The inlined `Collection::Array` bodies, ported as separate statics because MSVC emits
one per instantiation AND per operation (the WAVE2 `sizeof(T)` trap, and it bit
immediately — this class alone needs five):

| Helper | Retail | What |
|---|---|---|
| `Array<Internal*>::SetSize(n, fill)` | FUN_18002ffa0 | append path, used by `AddObject` |
| `Array<Internal*>::SetSize(n)` | FUN_18002fca0 | shrink path, used by `RemoveObject` |
| `Array<Internal*>::Reserve(n)` | FUN_18002fd90 | grow capacity, restore `_size` |
| `Array<int32_t>::SetSize(n)` | FUN_18002fea0 | element width 4, **align 4** |
| `Array<int32_t>::Clear` | FUN_18001c750 | free + zero all three fields |

All of them reproduce the growth policy in `runtime/types/collection/Collection.h` exactly,
including the `capacity = n * 3` rule that fires **only** when `n == oldSize + 1 &&
oldSize != 0`. That rule is the only non-trivial arithmetic in the class and it is what
the shadow oracle checks.

`?_singleton@…` is a **data symbol**, not a function. The census counts it as "called"
(`code/Game/Game/gameupdateloop_serialize.cpp:187`), but it can never be "ported" —
there is nothing to register. It becomes ours when we construct the factory, i.e. at
cutover. Recording it so a later census diff does not read it as an outstanding gap.

---

## 3. Why this class is much safer than PLAN.md §4.3 risk 3 assumed

The brief's nightmare — a half-ported class with two vtables in play, the swap-remove UAF
family again — **cannot occur here**, for two independent reasons:

1. **One table.** Every ported body reaches the LIVE singleton and mutates the DLL's own
   arrays in place, and the buffers are allocated and freed through the DLL's own
   `MemoryAllocator::MemAlloc`/`MemFree` statics (resolved with `RtOriginal<>`, so an
   armed allocator port cannot split the heap either). There is no replica. This is the
   Wave 2 allocator argument, verbatim.
2. **One vtable, and it is theirs.** `??_7Internal@Object@@6B@` and
   `??_7Base@Object@@6B@` are exported data symbols (PLAN.md §2.2 hazard D was right that
   147 `??_7` symbols exist; two of them are ours). The ported constructor stamps the
   DLL's vtable **by name**. An object our ctor builds is byte-for-byte indistinguishable
   from one the DLL's ctor builds, including virtual dispatch. We never own a vtable, so
   the "two vtables" hazard has nothing to attach to.

PLAN.md §4.1 decision 5's "switch atomically as a class" therefore does not bind this
lane the way it binds a class with private state. What remains is an ordinary
faithfulness question, and §4 measures it.

**This does not generalise to lane A or lane C.** `Object::Factory`'s type-info list and
`ExternalFactory`'s reference table are different animals; re-make the argument there.

---

## 4. The oracle

`rt_diff.h`'s `RT_DIFF_SHADOW` contract is followed literally, and the wrappers are
registered by an env var, so the orchestrator can produce numbers without a new file:

```
BANISHED_RT_DIFF=1 BANISHED_RT_INTERNAL_SHADOW=1 BANISHED_AUTOSTART=1   # then sa-run
```

In shadow mode **our code performs no store into the factory at all**. It captures the
pre-state, calls the DLL original through `RtOriginal<>`, and asserts the post-state
matches what our model predicted. Arming it cannot corrupt anything a plain run would
not, which is why it is a genuine oracle and not a self-test.

| Wrapper | Predicted and asserted |
|---|---|
| `Internal::Internal` | new `_factoryIndex == oldSize`; `_size == oldSize+1`; `_objects[oldSize] == this`; **`_capacity` from the growth policy** |
| `Internal::~Internal` | `_size == oldSize-1`; `_generation == 0`; and when a swap happened, that the moved element landed at `idx` with its `_factoryIndex` rewritten |
| `ClearPersistState` | all three `_persistIds` fields zero |
| `ResetPersistState` | `_persistIds` sized to the count the stream carried and filled with −1; `_objects._size` unchanged; **both capacities from the growth policy** |

Not asserted by the ctor wrapper: `_generation`. It is a `Random::Rand()` draw and
predicting it would advance the generator. Wave 2 proved `Rand` bit-exact over 16,035,064
calls separately; that is the evidence for this one field, and it is second-hand.

`InternalBasePtr::Serialize` gets **no shadow**: the call advances a stream, so it cannot
be run twice and there is no way to observe what the DLL wrote without reproducing the
codec (Wave 4). It is unverified and stays opt-in.

The pass bar is Wave 1's: **zero divergences with `calls=` in the thousands.** `Internal`
ctor/dtor traffic is heavy (every UI element, every entity), so a shadow run that reports
a small `calls=` is the Wave 1 defect #4 trap — read the column
(`reference_uncalled_port_is_not_verified.md`).

Tier 1's three trivial stores are diffed the ordinary way, on scratch copies, under
`BANISHED_RT_DIFF=1` alone.

---

## 5. GO / NO-GO

### Tier 1 (both `InternalBasePtr` ctors, `Copy`, `DeletePtr`, `Internal::Persist`) — **GO, default-on.**

Five registrations. Three are single-store functions diffed byte-exact against the DLL on
scratch copies. `Internal::Persist` is empty in **both** builds. `DeletePtr` is five
instructions and is **not diffable** — running the original as an oracle would destroy the
object twice — so its evidence is code inspection, not measurement, and that is stated
here rather than hidden behind the word "ported".

### Tier 2 (`Internal::Internal`, `~Internal`, `ClearPersistState`, `ResetPersistState`, `InternalBasePtr::Serialize`) — **NO-GO.**

For:

* All five decompiled from retail **and** cross-read against the profile build, which
  calls out of line what retail inlines; no `FUN_` guessing anywhere (PLAN.md §4.3 risk 4
  does not bite this lane either).
* The shared-state and vtable model in §3: no two-table and no two-vtable failure exists.
* Every helper's growth policy independently confirmed against `Collection.h`.
* The oracle is written and registered; flipping to GO costs one run, not one file.

Against, and these are why:

* **Not one instruction of Tier 2 has ever executed.** Not live, not in shadow.
* `ResetPersistState` carries a **stated deviation** (§7.3): both encoding-factory vtable
  slots are called with `(factory, &serializer)` because Ghidra's argument analysis at
  those two sites is demonstrably unreliable. Safe in the Microsoft x64 ABI, but not
  byte-faithful, and it must be disassembled before this tier goes live.
* `InternalBasePtr::Serialize` has **no oracle at all** and lands in the save/load path —
  historically this project's most expensive bug class (a wrong byte desyncs the stream
  and surfaces later on an innocent record).
* `Internal::Internal`/`~Internal` sit under every object in the engine. A wrong index is
  the swap-remove wild write, which has no proximate symptom.

**Gate to flip Tier 2 to GO**, in order:

1. `BANISHED_RT_DIFF=1 BANISHED_RT_INTERNAL_SHADOW=1` on a full `sa-run` **and** on
   `sa-stress`: 0 divergences, `calls=` in the thousands or better. Check `calls=`.
2. Disassemble the two encoding-factory slots (`0x18` and `0x20` off the factory vtable
   named by `stream+0x10`) and write the exact arity into `ResetPersistState`.
3. Then `BANISHED_RT_INTERNAL_TABLE=1` behind the four PLAN.md §5 wave-boundary gates
   (tutorials 1–4, the achievement census, `Save9.sav`, `sim ms/iteration` within 5%),
   with `Save9.sav` weighted heavily because it is the only thing that exercises
   `ResetPersistState` and `Serialize` at all.

`SHADOW` and `TABLE` are **mutually exclusive** and asking for both is refused in code
(`RtObjectInternalRegister` returns 0 and says why) — shadow requires the DLL to own the
table.

---

## 6. What landed

| File | What |
|---|---|
| `runtime/types/object/Internal.h` | the three layouts + the `SerializerLite` 16 bytes `ResetPersistState` needs, `static_assert` on every offset, full RE write-up |
| `runtime/src/object/internal.cpp` | 11 ported bodies, 5 `Array` helpers, 4 shadow wrappers, the three-tier registration |
| `docs/runtime_port/WAVE3_LANE_B.md` | this |

Nothing else was touched. Compile check, clean, zero warnings:

```
g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. -Iengine/types -Iengine/funcs -Icode/Game \
    -Iruntime/types -Iruntime/funcs -Idx11/types -Idx11/funcs -Istandalone \
    -include standalone/env_cache.h runtime/src/object/internal.cpp
```

No `<windows.h>`. Both `Serializer` and `Stream` are reached purely through offsets read
out of the disassembly (`stream+0x08` direction, `stream+0x10` encoding factory), so
nothing here waits on Wave 4.

**Original file name.** The profile build's source-tree strings name
`Object\InternalFactory.cpp`, `Object/InternalFactory.h` and `Object/InternalPtr.h`.
There is **no `Object\Internal.cpp`.** `runtime/src/object/internal.cpp` is the wave
scaffolding's name, not the engine's — `runtime/src/object/README.md` asks for a mirror
of the original tree and this file does not honour it. Recorded, not renamed: the file
name is fixed by `rt_object_port.cpp`, which this lane does not own.

---

## 7. Defects and stale claims

### 7.1 `CheckPtr` — registering it is a measurable regression (PORT DECISION)

`?CheckPtr@InternalBasePtr@Object@@IEBAXXZ` (0x1800c0290) is `C2 00 00` — `RET 0`. So is
`AssignId` (0x1800c0280). **Both are empty in the PROFILE build too** (0x18008c5d0,
0x18008c5e0), the build with `System::Debug::Assert` calls compiled in and 5,182 exports.
That settles a question the tree has been guessing at: these were compiled out at the
**source** level in every shipped configuration, and **no build of this engine has ever
validated an `InternalBasePtr`**.

It is therefore ported as an empty function and **deliberately not registered**, because
registering it would be an active regression:

* `engine/funcs/engine_funcs.cpp:3154-3172` inspects the **first byte** of whatever
  `?CheckPtr@…@IEBAXXZ` resolves to and sets `g_checkPtrIsNop` only when it is `C3` or
  `C2 00 00`. That flag makes ~60,000 indirect calls per frame at 800×10 disappear.
  Register the port and the resolved body becomes a GCC-compiled function whose first
  byte is neither, so the flag goes false and the calls come back. The tell is the
  `[CheckPtr]` line in `standalone_error.log` flipping to `"real call"`.
* In the perf build with `BANISHED_CHECKPTR_STATIC_NOP` the call sites are compiled out
  entirely, so a registered port would **never execute** while the census counted it as
  ported — Wave 1 defect #4 exactly.

`BANISHED_RT_INTERNAL_CHECKPTR=1` arms it for anyone who wants to measure that claim
rather than take it on trust; it prints a warning when it does.

### 7.2 The encoding vtable offsets differ between builds (PORT TRAP, avoided)

`InternalBasePtr::Serialize` calls the encoding's vtable at **+0x78 / +0x98 / +0xa0**
(bool / uint / int) in **retail**, and at **+0x90 / +0xb0 / +0xb8** in the **profile
build**, whose `Encoding` vtable carries extra virtuals. The retail numbers agree with the
forwarder table in `runtime/src/io/serializer.cpp`. Transcribing a profile decompile here
writes the right values through the wrong slots and desyncs every save, with no proximate
symptom. Recorded in `Internal.h` next to the constants. PLAN.md §6 step 1 says the
profile build's *names* win; this is the counter-case where its *offsets* must not.

### 7.3 `ResetPersistState`'s encoding-factory call arity is not pinned (KNOWN DEVIATION)

Retail inlines `IO::Serializer::Serializer(stream, 0)`. Ghidra renders the read branch as
`(**(code**)(vt + 0x18))()` — no arguments — and the write branch as
`(**(code**)(vt + 0x20))(factory, &ser, 0, in_R9, 0xfffffffffffffffe)`, i.e. with two
invented junk arguments. Its analysis at these two sites is not trustworthy either way.
The port calls **both** with `(factory, &serializer)`; passing an extra register argument
to a callee that ignores it is a no-op in the Microsoft x64 ABI, so it is safe, but it is
not byte-faithful and it is on the gate list in §5.

### 7.4 Two stale comments naming a mangling the DLL does not contain (PRE-EXISTING — NOT FIXED, outside this lane's files)

`?CheckPtr@InternalBasePtr@Object@@QEAAXXZ` **does not exist** in `Runtime-x64.dll`; the
real export is the `IEBAXXZ` (protected, const) form. Commit `7e993da` fixed the live
`ResolveExport` calls. Two comments still carry the phantom spelling and will send the
next reader looking for a symbol that has never existed:

* `engine/funcs/engine_funcs.h:1983` — the `fn_InternalBasePtr_CheckPtr_t` block header.
* `code/Game/Action/action_base_complete.cpp:60` — "resolved via
  `?CheckPtr@InternalBasePtr@Object@@QEAAXXZ` at startup".

Same block, `engine_funcs.h`, also calls the empty body **"a debug-build leftover"**.
§7.1 disproves that: the profile build's body is equally empty. PLAN.md §6 step 8 requires
correcting these at their source; this lane does not own either file and did not edit
them. **They are the orchestrator's to fix.** (The `.cpp` comment at
`engine_funcs.cpp:3157` is already correct — it names `C2 00 00` and accepts `C3` too — so
only the header comment's "(0xC3)" phrasing is loose, and the code is right.)

### 7.5 `?_singleton@InternalFactory@Object@@0PEAV12@EA` can never be "ported" (CENSUS ARTIFACT)

It is exported **data**. `tools/runtime_census.py` lists it under Object as called; there
is no function to register, and there will not be until we construct the factory at
cutover. Not a gap.

### 7.6 `InternalFactory::Shutdown` is empty; the factory leaks its table (OBSERVATION)

`?Shutdown@InternalFactory@Object@@QEAAXXZ` (0x1800c0010) is a bare return, and
`~InternalFactory` (0x18002f860) does not free `_objects._data` either. Process teardown
reclaims it. Not a defect, and the exact asymmetry Wave 2 found in
`MemoryAllocator::Shutdown`. Recorded so nobody ports a "missing" free.

---

## 8. For other lanes and later waves

* **Lane A / lane C:** the "one table, one vtable, therefore safe" argument in §3 is
  specific to this class. `Object::Factory`'s type-info list is a raw AVL walked inline by
  `PersistObject` (`Base+0x38` left, `+0x40` right, key at `+0x00`, payload at `+0x10`),
  and `Base::IsDerivedFrom` is already in the crash handler's instruction-offset table
  (`standalone/main.cpp:518`, `+0xC0390…A0`) — PLAN.md §2.2 hazard C says retire that with
  Wave 3, so lane A owns it.
* **Wave 4 (IO):** `IO::Serializer` is 16 bytes, `{Stream* stream, Encoding* encoding}`;
  the ctor at 0x1800b9960 ignores its `int version` argument; `stream+0x08` is the
  direction (0 = reading) and `stream+0x10` is the encoding factory whose vtable slot 3
  makes a reader and slot 4 a writer. Wave 4 should pin the factory-call arity §7.3 leaves
  open.
* **The WAVE2 `sizeof(T)` trap is real and it bit on the first class of Wave 3**, exactly
  as WAVE2.md predicted: `InternalFactory` alone needs four distinct `Array` bodies
  because MSVC emits one per instantiation and per operation. Any later wave that ports a
  container-heavy class should budget one helper per (element width × operation), not one
  per container.
* **`InternalPtr<T>` is 8 bytes**, the same single pointer as `InternalBasePtr`; the 103
  template instantiations in the export table add type-safe accessors and nothing else.
  The index/generation pair lives in the *object*, not in the pointer. Anyone sizing a
  struct that embeds one should use 8. Measured, not assumed:
  `?Child@Element@UI@@QEBA?AV$InternalPtr@VElement@UI@@@Object@@H@Z` (0x180035560)
  returns one by value and its entire body writes **one qword** into the hidden return
  slot. (The same body also pins `UI::Element+0xa0` as the child
  `Array<InternalPtr<Element>>`'s buffer pointer, 8-byte stride — free evidence for
  Wave 7.)

## 9. Ghidra renames made by this lane (`Runtime-x64.dll` only)

The exported functions already carried their demangled names. The five inlined
`Collection::Array` helpers did not; they were `FUN_` and are now:

| Address | Name |
|---|---|
| 0x18002ffa0 | `Collection_Array_InternalPtr_SetSizeFill` |
| 0x18002fca0 | `Collection_Array_InternalPtr_SetSize` |
| 0x18002fd90 | `Collection_Array_InternalPtr_Reserve` |
| 0x18002fea0 | `Collection_Array_int32_SetSize` |
| 0x18001c750 | `Collection_Array_FreeBufferAndZero` |

That is five of the 2,467-strong `FUN_` pool, named the way PLAN.md §4.3 risk 4 says the
pool actually closes — by reading the functions a wave needs, one at a time. `0x18002faa0`
and `0x18001cdf0` (the two `Array::operator=` bodies reached only from the
`InternalFactory` copy constructor) were read but left `FUN_`: this lane did not need them
and did not want to name them from a single caller.
