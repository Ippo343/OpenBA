# COLL-COMPONENT step-2 recipe (read this whole file before editing anything)

Worktree: `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-af701169855ab5b77`
Work ONLY in that directory. Do NOT `cd` anywhere else. Do NOT run git commands.
Do NOT run make. Do NOT touch `engine/`, `runtime/`, `standalone/`, `dx11/`, or any
`code/Game/` directory outside the one assigned to you.

## The finding this batch rests on

The coordinator's brief assumed the Collection call sites in `code/Game/` reach
shims in `runtime/src/collection`. **They do not.** Measured on this branch:
`code/Game/{Component,Action,Work,Entity}` has ZERO `@Collection@@` mangled
lookups and ZERO `Module::Runtime` collection addresses.

What it has instead is the EXE-side monomorphisations. MSVC compiled
`Collection::Array<T>::SetSize` separately into Application-x64.exe for every
instantiation (`FUN_1400021d0` = `Array<int32>::SetSize`, `FUN_1400032e0` =
`Array<ptr>::SetSize`, `FUN_140003110` = the three-argument
`SetSize(n, fill)`, …). Callers reach them as

```cpp
static fn_ResizeIntArray_t s_fn_ResizeIntArray = nullptr;
...
s_fn_ResizeIntArray = reinterpret_cast<fn_ResizeIntArray_t>(
    ResolveAddr(Module::Exe, 0x1400021d0));
...
s_fn_ResizeIntArray((void*)(selfAddr + 0x30), localCount);
```

That is the same "a resolved pointer stands in for a member call" shape B.3.1
removed for `System::String`. Converting it is step 2 for these directories.

## The bridge header (already written — just include it)

`code/Game/Component/coll_bridge.h`, included as `#include "Component/coll_bridge.h"`.
READ IT FIRST. It gives you:

- `GameColl::I32 / U8 / U16 / F32 / Ptr8 / Vec3` and the generic
  `GameColl::Bytes<N, A>` — element types whose default constructor ZEROES,
  because `rtcoll::SpecFor<T>()` emits the zeroing `TInit<T>` exactly when `T`
  is NOT trivially default constructible, and every exe helper here zeroes its
  new tail. A bare `int32_t` would take the `init == nullptr` arm and leave the
  tail uninitialised — a different array. **Never use a bare scalar as `T`.**
- `GameColl::AsArray<T>(headerPtr)` — the `// B.3.2 bridge` reinterpret_cast onto
  the 16-byte `{ T* data; int32 size; int32 capacity }` header living inside a
  struct that is not a real C++ class yet.

## The conversion

For each resolved array-helper pointer in your files:

1. **Identify the helper body.** Grep the exe address across `code/` and
   `standalone/reg_*.cpp` to find the reimplementation and READ IT. You must be
   able to state, for that body: stride, MemAlloc alignment, what the new-tail
   arm writes, whether an element is destroyed. Table of the ones already read:

   | exe addr | body | template call |
   |---|---|---|
   | `0x1400021d0` | `Reimpl_Array_Resize_Stride04` (serialize_resize_helpers.cpp) — stride 4, align 4, zero tail | `AsArray<GameColl::I32>(h).SetSize(n)` |
   | `0x14000a590` | `Reimpl_Array_Resize_Byte` — stride 1, align 1, zero tail | `AsArray<GameColl::U8>(h).SetSize(n)` |
   | `0x14001c890` | `Reimpl_Array_Resize_Stride02` — stride 2, align 2, zero tail | `AsArray<GameColl::U16>(h).SetSize(n)` |
   | `0x140001bb0` | `Reimpl_Array_Resize_Stride08` (serialize_arrays.cpp) — stride 8, align 8, zero tail | `AsArray<GameColl::Ptr8>(h).SetSize(n)` |
   | `0x1400032e0` | `ImmigrationComp_DynArrayResize` (Component/immigrationcomponent.cpp) — stride 8, align 8, zero tail | `AsArray<GameColl::Ptr8>(h).SetSize(n)` |
   | `0x140003110` | `ActHunt_DynArraySetCount_3110` (Action/action_base_hunt.cpp) — stride 8, align 8, **3-arg fill** | `AsArray<GameColl::Ptr8>(h).SetSizeFill(n, valuePtr)` |
   | `0x14001c7c0` | `Reimpl_Array_Resize_Stride0C` — stride 12, align 4, zero tail | `AsArray<GameColl::Vec3>(h).SetSize(n)` (or `Bytes<0x0c,4>` when the element is 3 ints) |
   | `0x14001bf10` | `Reimpl_Array_Resize_Stride14` — stride 0x14, align 4, zero tail | `AsArray<GameColl::Bytes<0x14,4> >(h).SetSize(n)` |

   Any other address: read its body and derive the row yourself. Put the derived
   row in a one-line comment at the converted site.

2. **CONVERT ONLY WHEN THE BODY IS EXACTLY `SetSizeCore`'s ALGORITHM**
   (read `rtcoll::SetSizeCore` in `runtime/src/collection/array.cpp` around line
   2267 — it is short): early-return on `newSize == oldSize`; capacity grows to
   `newSize`, or `newSize*3` when `newSize == oldSize+1 && oldSize != 0`;
   one `MemAlloc(cap*stride, align)`; copy prefix / init-or-fill tail; free the
   old buffer on a realloc. **If the body does ANYTHING else — a non-zero
   default (the spawn-entry 10.0f is the known example), a per-element deep
   clone, a refcount, a destructor call, an erase-by-value search, a push that
   also writes the element — DO NOT CONVERT IT.** Leave the site alone and list
   it in your report as "not convertible, reason".

3. **Delete the now-dead machinery**: the `typedef`, the `static fn_* = nullptr`,
   the `ResolveAddr` line, and any `if (s_fn_X)` guard around the call. If the
   pointer is also printed in a `#ifdef _DEBUG printf`, drop that argument and
   its `%p` from the format string. If it is part of an `if (!s_fn_A || !s_fn_B)
   return;` readiness check, remove just that term. **Never delete a function or
   a line you did not prove dead** — grep the file for the identifier first.

4. **Locals**: if a converted array is a LOCAL (not a struct field), make it a
   real `Collection::Array<T>` object and delete the manual destroy call — but
   ONLY after proving there is exactly one destruction on every path, and that
   the local is not memcpy'd/stored into a struct field afterwards (that would
   double-free: the class's copy is shallow). If you cannot prove it, keep the
   bridge cast on the raw bytes and say so.

5. **No behaviour change.** No new `BANISHED_*` flag, no `getenv`, no null
   guards added, nothing "fixed" along the way. If a converted site drops a
   defensive guard the ported helper had (`if (!fn_MemAlloc ...) return;`, the
   negative-`newCount` bail in `GenericArrayResize`), SAY SO in your report —
   do not reintroduce it.

## Hygiene

- Edit in place. Never rewrite a file wholesale. Never drop an existing function.
- Add `#include "Component/coll_bridge.h"` to each file you convert.
- Compile-check each file you touch, one at a time, with:

```
/c/msys64/ucrt64/bin/g++.exe -std=c++17 -fno-strict-aliasing -I. -Iengine/types \
  -Iengine/funcs -Icode/Game -Iruntime/types -Iruntime/funcs -Idx11/types \
  -Idx11/funcs -Istandalone -c <file> -o /dev/null
```

  It must exit 0 with no warnings. (Run it from the worktree root.)
- Keep a scratch note at `scratchpad/coll-component/<your-name>/notes.md`.

## Report back (as your final message, not a file)

- Files touched, and per file: the exe addresses converted and the instantiation
  each became.
- Sites NOT converted, with the reason (one line each).
- Count of resolved-pointer declarations deleted.
- `reinterpret_cast` count in your directory before and after (`grep -rho
  reinterpret_cast <dir> | wc -l`).
- Anything you had to guess.
