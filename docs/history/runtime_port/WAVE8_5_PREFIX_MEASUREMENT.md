# Measurement — `ExternalFactory::SerializeReferenceTable` and lane S's prefix stop

WAVE8_5.md §12 item 3 said this row should go "via lane S's prefix stop — it mutates within
~2 emissions, so **measure what the prefix actually covers before committing to it**."

Measured. **The prefix covers 2 of roughly 17,000 emissions — 0.01% — and verifies nothing the
function is for.** But the premise underneath the instruction turns out to be wrong in the
project's favour: **the write path's mutations are idempotent with identical values, so the FULL
compare is safe and no prefix stop is needed at all.**

Subject: `?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z`,
retail `0x1800bcb60`–`0x1800bcf38`, image base `0x180000000`. Static analysis only; no probe was
added, no source file was edited, nothing was ported or registered.

---

## Q1. How many typed emissions precede the first mutation of shared state?

**Two — and, read strictly, zero.** There are two different "first mutations", and the distinction
is the whole finding.

### The write path, instruction by instruction

`RDX` is an `IO::Stream&`, **not** a `Serializer&` — the mangled name says `AEAVStream@IO@@` and
the body confirms it (`[RBX+0x08]` read as the direction, `[RBX+0x10]` as the `Encoding*`). The
`Serializer` is built as a **local** at `[RBP-0x18]` (`stream`) / `[RBP-0x10]` (`readerWriter`),
matching `runtime/types/io/Serializer.h`'s 16-byte `{Stream*, ReaderWriter*}`.

| Addr | Instruction | What it is |
|---|---|---|
| `1800bcb80` | `MOV RDX,[RCX+0x20]` / `ADD RCX,0x20` / `CALL 0x18002ebf0` | **MUTATION A.** Recursive teardown of the whole `_references` tree — `FUN_18002ebf0` is a post-order walk of `+0x18`/`+0x20` that decrements each node's `Filename` refcount, `MemFree`s it at zero, and frees the node (`FUN_180009c30`). |
| `1800bcb90` | `MOV [R14+0x20],R12` / `MOV [R14+0x28],R12D` | root and count zeroed. |
| `1800bcbb4/b9` | `CALL [RAX+0x18]` (reading) or `CALL [RAX+0x20]` (writing) | **EMISSION 1** — the `IO::Encoding` reader/writer factory. Lane S records this (tagged `+0x1000`). |
| `1800bcbd5` | `CALL [RAX+0xa0]` on `[RBP+0x40] = [R14+0x18]` | **EMISSION 2** — `int` scalar, `_resources._count`. |
| `1800bcbdb` | `CMP [RBX+0x8],0` / `JNZ 0x1800bcdf4` | direction split; writing takes `0x1800bcdf4`. |
| `1800bcdf4` | `MOV EAX,0x1` | the id counter starts at 1. |
| `1800bce10` | `MOV RCX,[RBX+0x20]` loop | descend to the leftmost `_resources` node. |
| **`1800bce30`** | **`MOV [RBX+0x14],EAX`** | **MUTATION B** — `ResourceEntry::referenceId = id`. Pinned independently by `External.h`'s `static_assert` and by `GetReferenceId`. |
| `1800bce3b` | `CALL 0x180006250` | `External::GetPath` — a `Filename` copy into a stack temp; refcount only. |
| `1800bce8b` | `CALL [RAX+0x98]` | **per-node emission** — `uint` scalar, the id. |
| `1800bce99` | `CALL 0x1800c9930` | `String::Serialize` — **2 more per-node emissions** (below). |
| `1800bcec0`… | successor walk `+0x28`/`+0x18`, `INC EAX` | next node. |
| `1800bcf28` | `CALL [RAX]`, `EDX=1` | **final emission** — the ReaderWriter's deleting dtor. |

`String::Serialize` `0x1800c9930` on the **writing** side is exactly two emissions and is itself a
pure read: `CALL [RAX+0x98]` with `len+1 | 0x80000000` (`1800c996c`), then the reading-only resize
branch is skipped (`CMP [RAX+0x8],R15D; JNZ` at `1800c998b`, and `TEST BL,BL; JZ 0x1800c9b3e` at
`1800c99a3` — `BL` is 0 whenever the sign bit survived, i.e. whenever we wrote), landing at
`1800c9b3e` → `CALL [RAX+0x18]` = the `char[]` array slot.

### The answer

* Before **MUTATION B** (`MOV [RBX+0x14],EAX`, the one WAVE8_5.md §8 names): **2 emissions.**
* Before **MUTATION A** (the `_references` teardown, 8 instructions into the function, on both
  directions): **0 emissions.** §8's table did not mention this one. A prefix stop cannot get in
  front of it — there is no emission index that precedes it.

---

## Q2. What fraction does that prefix cover?

**Total write-path emissions = `3 + 3N`**, where `N` = `_resources._count`:

| Emission | Count | Scales with N? |
|---|---|---|
| `Encoding` writer factory (`+0x20`) | 1 | no |
| `int` entry count (`+0xa0`) | 1 | no |
| `uint` reference id (`+0x98`) | N | **yes** |
| `String::Serialize` length (`+0x98`) | N | **yes** |
| `String::Serialize` char array (`+0x18`) | N | **yes** |
| ReaderWriter deleting dtor (slot 0) | 1 | no |

**N is not small.** The one empirical figure in the tree is `standalone_error.log`'s
`[PreloadRefs] preloaded 5687/5688 referenced externals` on the `Save9` load — the load-path
partner of this same table. At N ≈ 5,688 the write path emits ≈ **17,067** emissions.

> **prefix coverage = 2 / (3 + 3N) ≈ 0.012%**

And the fraction understates how bad it is, because of *which* 2. The prefix verifies (a) that we
picked the writer factory rather than the reader factory, and (b) that we emitted the same
`int` count retail did. It does **not** touch the in-order walk, the id assignment, the id/path
pairing, the string encoding, or the terminator — i.e. everything that can desync a save. It is a
header check wearing an oracle's clothes, and per `save_trace.h`'s own warning ("a partial oracle
that reads like a full one is worse than none") **it should not be used here.**

---

## Q3. Is snapshot/restore the answer instead? — No, because neither is needed

§8 sized the alternative as "the reference-set ids are one dword per node". That sizing is right
(N dwords ≈ 22 KB at N=5,688) and the walk order is deterministic — an in-order traversal of an
AVL tree that the write path never inserts into or removes from. But **restoring is unnecessary,
because replaying is already safe.** Checking each write-path mutation for idempotence:

| Mutation | Replayed? | Why it is safe |
|---|---|---|
| **A** — `_references` teardown + zero root/count | Once | The second call reads `[this+0x20] == 0` and `FUN_18002ebf0` returns immediately. **No double free.** The write path never *reads* `_references`, so emptying it cannot change either side's emissions. |
| **B** — `ResourceEntry::referenceId = id`, per node | Twice | `id` runs 1..N in in-order walk order, over a tree neither call modifies. **The second pass writes the identical value into the identical node.** |
| `Filename` refcounts on the per-node `GetPath` temp | Twice | `INC [RAX-4]` / `DEC [RAX-4]` are balanced inside each iteration; the `ResourceEntry` holds its own reference throughout, so the `MemFree` arm is unreachable. Net zero, both times. |

There is no third store. Reading the write branch `1800bcdf4`–`1800bcf17` for non-stack stores
yields exactly `MOV [RBX+0x14],EAX` plus those refcount `INC`/`DEC`s.

So the property lane S rests on — *on the write path a Serialize is a pure read of the object* —
holds here in the form that matters: it is not literally pure, but **its impurity is idempotent
and value-identical**, which is just as good for a two-sink replay. This is the third distinct
shape in the family (`Audio::Scene::Serialize` mutates nothing; `InternalBasePtr::Serialize`
mutates irreversibly and needed the stop; this one mutates reversibly-by-repetition), which is
§8's own point about per-function checking, confirmed a fourth time.

---

## RECOMMENDATION — **neither. Use the FULL lane S compare (`stopAfter = 0`).**

The prefix stop was proposed on the strength of §8's one-line summary of this function. The
disassembly says the summary was right about the mutation and wrong about its consequence.

**Two concrete notes for whoever ports it**, both of which will bite silently:

1. **The second argument is a `Stream&`, not a `Serializer&`.** `Compare` passes
   `&sink.serializer`, and this callee would then read `direction` out of `Serializer+0x08`
   (the `ReaderWriter*`) and `encoding` out of `Serializer+0x10` (past the end of a 16-byte
   object). **No harness change is required** — wrap both sides:
   `void Wrap(void* obj, void* ser) { real(obj, ((rt::io::Serializer*)ser)->stream); }`,
   since `Serializer+0x00` *is* the `Stream*` and lane S already points it at `sink.stream`.
2. **MUTATION A is invisible to lane S** — the teardown emits nothing, so a port that forgets it,
   or frees it wrongly, passes a clean emission compare. Cover it with an explicit assertion at
   the call site rather than trusting the emission trace: after `ours()`, check
   `*(void**)(f+0x20) == nullptr && *(int*)(f+0x28) == 0`. (The load path's partner invariant is
   already relied on in `code/Game/Game/gameupdateloop_serialize.cpp:979`.)

The hook point already exists: `gameupdateloop_serialize.cpp:944`,
`s_fn_SerializeRefTable(extFactory, stream)`, gated on the file's own `IsSaving(stream)` so the
compare only runs in the direction this analysis covers. **The read path is unchanged and stays
hard** — it consumes the stream, allocates nodes and rebuilds `_references`, and nothing here
gives it an oracle.
