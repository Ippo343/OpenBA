# Wave 10 — the `ExternalPtr<T>::Serialize` gate, measured

**Date 2026-09-08. Branch `wave10/base`.**

This answers the one question Wave 9 lane UIW2 left open when it DECLINED
`?Serialize@ElementDescription@UI@@UEAA_NAEAVStream@IO@@@Z` (0x180116b00) for
the second time. The answer is better than UIW2 predicted, and it changes the
shape of the lane that takes this row.

---

## 1. The question UIW2 asked

UIW2 recorded that `Serialize@ElementDescription`'s **first** blocker had
EXPIRED — lane S's emission oracle (`runtime/src/io/save_trace.{h,cpp}`,
`BANISHED_RT_SAVE_TRACE=1`) now exists, and on the write path this Serialize is
a pure read of the description. The surviving blocker was its three unported
callees, and UIW2 wrote the recipe:

> `0x1800042a0` and `0x180076610` are 215 instructions each with byte-identical
> prologues, while `0x180038f90` is ~128 — i.e. the first two are almost
> certainly ONE body parameterised by a type id, and the J1 method in
> `array.cpp` decides that in one `tools/uic_bytediff.py` run.
>
> **So the recipe is:** byte-diff `0x1800042a0` against `0x180076610`; if they
> are one body, porting it plus `0x180038f90` closes all three callees.

## 2. Why a raw byte-diff cannot answer it

Every `rel32` and every RIP-relative displacement encodes a distance **from the
instruction**, so two *identical* bodies at different addresses differ in every
one of those fields. Measured naively, `0x1800042a0` vs `0x180076610` differ in
**210 of the first 900 bytes (23.3%)** — which reads like two different
functions and is an artefact.

`tools/extptr_serialize_diff.py` (archived beside these numbers) decodes both
with capstone and, at each difference, resolves the operand back to the
**absolute address it encodes**. Same absolute target ⇒ the difference is
position, not behaviour. This is the same argument `tools/uic_bytediff.py` makes
for the J1 family; that tool's own tables are hardwired to a different family,
which is why this one exists rather than reusing it.

## 3. The measurement

All three are **215 instructions, 748 bytes**:

| pair | identical bytes | differ by POSITION only | genuinely different |
|---|---|---|---|
| `0x1800042a0` vs `0x180076610` | 193 | 20 | **2** |
| `0x1800042a0` vs `0x180038f90` | 193 | 20 | **2** |

And in both pairs the two real differences are **the same difference**, appearing
twice: a `MOV EDX, dword ptr [rip+…]` loading a type-id from one global.

| body | reads | therefore |
|---|---|---|
| `0x180038f90` | `?_typeInfo@ElementDescription@UI@@` | `ExternalPtr<ElementDescription>::Serialize` |
| `0x180076610` | `?_typeInfo@Dialog@UI@@` | `ExternalPtr<Dialog>::Serialize` |
| `0x1800042a0` | `?_typeInfo@StringTable@UI@@` | `ExternalPtr<StringTable>::Serialize` |

All three slot addresses are confirmed exports (`pefile`, shipped DLL), not
Ghidra guesses.

**UIW2's prediction is confirmed, and its size estimate for the third body was
wrong in our favour**: `0x180038f90` is not ~128 instructions, it is the same
215. So the recipe simplifies from "port one body plus a second one" to:

> **Port ONE body, parameterised by a `BaseTypeInfo*`, and register it at three
> addresses.** All three callees close together.

## 4. What this settles about `ElementDescription`'s layout

`Serialize` dispatches +0x48, +0x50 and +0x58 to those three bodies, so all
three fields are `Object::ExternalPtr<T>` and the T of each is now known.
UIW2 deduced that `+0x50` could not be the `uint64_t _userData` the header
claimed and asked whoever took the row to fix the types; this names them:

* `+0x48` `ExternalPtr<UI::ElementDescription>`
* `+0x50` `ExternalPtr<UI::Dialog>` — **the name `_userData` is simply wrong**
* `+0x58` `ExternalPtr<UI::StringTable>`

Applied to `runtime/types/ui/ElementDescription.h`. The **offsets were always
right**; only the types were under-specified, and a port that treats `+0x50` as
a plain integer emits the wrong thing into every saved layout.

## 5. Why the port did NOT land in this wave

Reading the body (0x1800042a0) rather than the summary of it:

* It is **not** a leaf. It calls `ExternalFactory::FindOrLoad`,
  `GetReferenceId`, `GetPathForReferenceId`, `Filename::AdjustDelimiters`,
  `String::Serialize`, `Base::IsDerivedFrom` and `MemFree`, and it open-codes
  the refcounted-String release idiom at **five** sites.
* It has three distinct modes: an `Encoding` slot `+0x10` value of 3 (resolve
  and drop), a path-string mode, and a reference-id mode selected by
  `ExternalFactory::_singleton[0x40]`.
* **On the READ path it writes `*param_1`** after a `FindOrLoad` +
  `IsDerivedFrom`, i.e. it loads resources and rebinds the pointer.
* `ExternalFactory::FindOrLoad` is itself **unported** — it is a predicted wall
  assigned to lane **OBJ1** in `WAVE10_PARTITION.md`. Porting this body today
  would close one row and open an address door onto an unported callee, which is
  [[reference_porting_can_grow_the_cutover_surface]].

And the blast radius UIW2 flagged is real and specific: this lands on the
`UIDescription` parse path of **every `.rsc` in the game**, where a previously
closed defect in slot 8 "doubled every loaded entity's roster and made
`FindComponent` return the empty duplicate — invisible in a screenshot".

So the gate is answered and the lane is now cheap to scope, but the port itself
belongs with **OBJ1** (which owns `FindOrLoad`), not bolted onto a UI wave.

## 6. Recipe for whoever takes it

1. Port the 748-byte body once, taking the `BaseTypeInfo*` as a parameter.
   Register it at `0x180038f90`, `0x180076610` and `0x1800042a0` — three
   `RegisterRuntimeAddr` calls, one body. These are **unexported internals**, so
   there is no mangled name to register; the address door is the only door, and
   [[reference_by_address_door_dies_at_cutover]] applies.
2. Take `FindOrLoad` with it, or sequence after OBJ1.
3. Verify with lane S's emission oracle on the WRITE path. `GetReferenceId` can
   register a new id, so use `stopAfter` to cap the prefix before that mutation
   exactly as `SerializeReferenceTable` does — do not assume the write path is
   pure just because the ElementDescription half of it is.
4. Only then register `?Serialize@ElementDescription@` itself. Note it is bound
   by NAME to exe PE-import slot `0x1401518c8` in
   `standalone/address_registry.cpp:1109`, and `RuntimePortInit()` runs before
   `AddressRegistryInit()`, so registering it re-points the exe's own `.rdata`
   slot too — the blast radius is not limited to our `fn_*` pointers.
