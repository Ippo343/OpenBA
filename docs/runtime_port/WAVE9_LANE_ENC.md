# Wave 9 — lane ENC: the `IO::Encoding` factory pair

**Branch** `wave9/lane-enc` · **base** `085782d` · **build** `mingw32-make standalone-debug`, clean under `-Wall -Wextra`.

## 1. What landed

Four functions, not the two the assignment named:

| Ghidra VA | Symbol | Exported? | Door(s) registered |
|---|---|---|---|
| `0x1800a8120` | `IO::Encoding::CreateReader` | **no** | address |
| `0x1800a8080` | `IO::Encoding::CreateWriter` | **no** | address |
| `0x1800a8340` | `??0BinaryReader@IO@@QEAA@AEAVSerializer@1@H@Z` | **yes** | name + address |
| `0x1800a8a30` | `??0BinaryWriter@IO@@QEAA@AEAVSerializer@1@H@Z` | **yes** | name + address |

Files: `runtime/src/io/encoding.cpp` (port), `runtime/src/io/encoding_probe.cpp` (oracle),
plus two wiring lines in `runtime/src/io/rt_io_port.cpp` and
`runtime/src/io/filesystem_packages.cpp`. Kill switch `BANISHED_RT_ENCODING=0`.

### Why four and not two

Each factory's entire body is *allocate 0x30, then `CALL` the constructor*. The
constructors are reached by a **direct call**, not through a minted vtable slot.
Registering only the two factories would have moved the cutover stop two
instructions later instead of removing it — the "porting can grow the cutover
surface" failure. The unit is the factory **and** its constructor, twice.

### Two corrections to the inherited notes

1. **Both constructors are EXPORTED.** Checked against the DLL export directory
   by address, not assumed. `AEAVSerializer@1@H` pins the signature to
   `(Serializer&, int)`, which `runtime/types/io/Serializer.h` had only inferred
   from the call site. Only the two *factories* are genuinely unexported.
2. **These constructors are not field-stampers — they do stream I/O.** A
   `BinaryReader` ctor **consumes 12 bytes**; a `BinaryWriter` ctor **emits 12
   bytes** (a `0xAAAAAAAA` magic, a `-1` back-patch placeholder whose position is
   saved to `+0x20`, then the version). That is the stream header, and it is why
   `SaveSettings`' first dispatch after building its stream lands here.

Two details the decompiled C would have lost, both transcribed from the listings:
the reader zeroes `+0x20`/`+0x24` with **one qword store** while the writer uses
**two separate dword stores** (`0` and `-1`); and the reader's `version`
**argument is dead on every successful call** — stored to `+0x28`, then
overwritten by the third `Read`. It is still reproduced, because it survives if
that read fails.

## 2. Measured result

`BANISHED_RT_DIFF=1`, autostart + `BANISHED_SAVE_TEST=300`, read from that run's
own log:

```
[Encoding] lockstep probe: cases=8 readers=4 writers=4 emitted=48
           distinctR=3 distinctW=3 neg=FIRED
           -- identical object bytes, stream cursor and emitted bytes
[RtDiff]   IO::Encoding::Create{Reader,Writer} [probe]  calls=1 worked=1 divergences=0
```

* **`emitted=48`** = 4 writers × exactly 12 bytes. The emission *sequence* is
  compared, not just the object.
* **`distinctR=3 distinctW=3`** — effectiveness control: the four cases produced
  four *different* objects, so the green is not a uniform population.
* **`neg=FIRED`** — negative control: one byte of a known-good case is flipped
  and the comparator must say no. It did.

### The first run DIVERGED, and the oracle was wrong, not the port

Run 1 reported `DIVERGED on 'reader object bytes'` on every reader case. Cause:
the probe deliberately gives each side its **own** Serializer over its **own**
stream, then compared `+0x10` (the `Serializer*`) and `+0x18` (the `Stream*`)
raw — addresses the harness itself had made different. `+0x00` had the same
problem for a different reason (ours resolves through the port-aware door,
theirs is the shipped table).

All three are now **normalised to a verdict tag**: each side is asked whether it
points at *its own* correct object and the tag records the answer, not the
address. A side storing the wrong pointer still fails. This is
`TagEncodingPointer` from `compressedstream.cpp` generalised to pointers.

The same run also printed `emitted=0` — not because nothing was emitted, but
because the emission counter only accumulated while `bad` was unset, so an
earlier reader failure zeroed a *writer* control. Fixed by accumulating the
control before the verdict checks. A control that silently reports zero for a
reason other than its own is worse than no control.

## 3. Delta on the mint — predicted, then measured

The two factories are slots 3 and 4 of the shared `IO::Encoding` vtable
`0x1801554a8`, which lane C2 mints **by address** with all five slots as
`RT_MINT_UNNAMED` traps. An `RT_MINT_UNNAMED` slot resolves through
`RtPortLookupAddr(dllVa)`, so registering those two addresses is exactly what
converts those two traps into bodies.

Predicted before measuring, then A/B'd on the **same binary** with this lane's
own kill switch (`BANISHED_RT_ENCODING=0`):

| | ON | OFF | delta |
|---|---|---|---|
| mint slots ported | 116 | 114 | **+2** |
| mint unported-trap | 257 | 259 | **−2** |
| registry exports | 780 | 778 | **+2** |
| registry addrs | 761 | 757 | **+4** |

Every figure matches the prediction. **Two of the five traps on that vtable are
now real bodies.**

## 4. HONEST LIMITS

1. **The port is verified but NOT YET REACHED by the game.** Attendance in the
   diff run was `CreateReader=4 CreateWriter=4 ctor=4/4` — *exactly the probe's
   own eight calls*, and nothing else. With the DLL present, the DLL's own
   `Serializer` ctor dispatches through the **DLL's** encoding vtable to the
   **DLL's** factories; our registrations reach our tree's calls and the
   **minted** table, which is only consulted at cutover. So this row's payoff is
   at cutover by construction, and the attendance counters exist so that fact is
   visible rather than assumed. A green probe over a port nothing calls is not a
   verified port, and the two numbers are produced by different machinery and
   printed under different labels so neither can be read as the other.
2. **One stream class.** Every case runs over an `IO::MemoryStream`. The
   constructors dispatch virtually, so a `CompressedStream` or `BinaryFileStream`
   exercises different `Read`/`Write`/`GetCurrentPos` bodies. What is proven is
   that the constructors issue the right calls, in the right order, with the
   right arguments.
3. **No failure path.** Every `Read`/`Write` in the corpus succeeds, so the
   reader's "version survives a failed third read" branch is **uncovered**. It is
   described in the source and deliberately not guessed at in the probe.
4. **No allocation-failure path.** `MemAlloc` returning null is unreachable here.
5. **Slots 0, 1 and 2 of `0x1801554a8`** (`0x1800a81e0`, `0x1800a81d0`,
   `0x1800a81c0`) are untouched and remain traps. This lane makes that vtable
   *partly* functional, not functional.

## 5. Declined, with a re-derived blocker

I took the primary only. The three secondaries were **not attempted**, and the
blocker below is re-derived rather than inherited — but note that #1 is now
*unblocked* and simply not reached for time, which is a scheduling fact, not a
technical one.

* **`?OnClosed@DialogInstance@UI@@UEAAXXZ`** — the stated blocker (an
  unreachable `IO::Encoding` plus an unported `??0MemoryStream`) is **fully
  stale**: `??0MemoryStream` landed with lane PKG2 and the encoding factories
  land here. **Declined for time, not for a blocker.** This is the next row to
  take and it should be cheap now.
* **`?ShowDialog@UserInterface@UI@@`** — **genuine blocker, confirmed**: it pumps
  its own modal message loop through `MainLoop::Frame` (`0x1800dd100`), which is
  a Wave 8 detour-required holdback, and needs `~UserInterface` (`0x180120b90`).
  The subject does not return until the dialog closes, so no in-line oracle can
  bracket it; it needs `BANISHED_MODAL_ANSWER` plus a design this lane did not
  build.
* **`?CloseDialog@UserInterface@UI@@`** — blocked on `?UpdateEvents@UserInterface@`
  (`0x18011fc60`), the head of UI event dispatch, called twice. Unchanged.

**Inherited correction I did not re-derive and am passing on unverified:** lane
UID established that `ShowDialog` does **not** call `EndDragDrop` (`0x180120af0`)
— it belongs to `CloseDialog`. Lane UIF's and lane UIC's tables list it under
`ShowDialog`, so their depth-4 figures for that row are partly borrowed from the
row below. I did not independently check this.
