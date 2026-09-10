# Wave 11 follow-up 2 — lane IOSTREAM

**Worktree** `C:/code/GIT/wt-w11f-iostream` · **branch** `wave11f/iostream`
**Inherited** `023b1bb` (a WIP tree preserved verbatim after a machine restart, never built, run or reviewed by anyone)

Three rows were in scope. **All three are ported, registered and verified**, each
by an oracle that runs the shipped DLL for real:

| row | retail | verdict | oracle |
|---|---|---|---|
| `?Add@ExternalFactory@Object@@` | `0x1800be570` | **PORTED, VERIFIED** | the layout oracle it arms, plus its own branch counters (7 867 calls/run, `visited=1799 skipped=3221`) |
| `?SaveText@External@Object@@` | `0x1800bb9d0` | **PORTED, VERIFIED** on a synthetic subject; **PROVEN UNEXECUTABLE against a real object in the shipped DLL** | `external_savetext_probe.cpp` — 13 checks, 42 file bytes, `serCalls=1/1`, PASS; 3 controls, 3 caught |
| `?Load@External@Object@@` | `0x1800bc620` | **PORTED, VERIFIED** live | `external_load_probe.cpp` — an ours-vs-theirs replay, 8 subjects, 1 295 checks, **421 dependency elements**, PASS; 2 controls, 1 caught + 1 informative miss |

Final combined run (`BANISHED_AUTOSTART=1`, all three probes armed): exit 0,
**empty crash log**, and

```
[IOStream]      LAYOUT ctl=0 checked=29 failed=0  PASS
[SaveTextProbe] VERDICT ctl=0 checked=13 failed=0 bytes=42 serCalls=1/1  PASS
[LoadProbe]     VERDICT ctl=0 subjects=8 checked=1295 failed=0 deps=421  PASS
[ExtLoad]  calls=2847 created=2847 reused=0 badMagic=0 badVer=0 sized=2847 deps=1274 serialized=2847 retNull=0
[ExtAdd]   calls=7867 insert=2847 hit=5020 (visited=1799 skipped=3221)
[ExtSaveText] calls=1 typeFound=1 createFailed=0 serialized=1 serializeTrue=1 bomWritten=0
```

Both builds are green: `standalone-debug` and `standalone-perf`.

---

## 1. The five layouts, and how each was measured

`runtime/types/io/FileStream.h` (inherited, reviewed, unchanged in substance).
The oracle for it, `runtime/src/io/file_stream_probe.cpp`, **had never executed**.
Its first run and every negative control are below.

| class | size | how the size was measured | how the fields were measured |
|---|---|---|---|
| `IO::FileStream` | `0x50` | `_eh_vector_destructor_iterator_` stride at `0x180023d80` | ctor `0x1800aaa10` writes; `?GetSize@`/`?GetCurrentPos@` are one-instruction accessors naming `+0x28`/`+0x2c` |
| `IO::BinaryFileStream` | `0x50` | same thunk `0x180023d80` (the two classes share it) | ctor `0x1800a8200` writes no offset ≥ `0x50`; only the destructor slot differs from `FileStream`'s vtable |
| `IO::CompressedFileStream` | `0xb8` | stride at `0x180024100` | ctor `0x1800a93b0` `LEA RCX,[RBX+0x68]`; **and `Load`'s inlined copy independently** — its two stack objects are exactly `0x68` apart (`rbp-0xC0` and `rbp-0x58`) |
| `IO::TextFileStream` | `0x80` | stride at `0x18002ba90` | both ctors, retail's `?GetObjectType@`/`?GetObjectName@`/`?GetObjectVersion@` accessors, **and `SaveText`'s whole epilogue** (see §3) |
| `IO::VisitorStream` | `0x18` | stride at `0x18002bf80` | ctor `0x1800bb090`; adds no field to `IO::Stream`; its `Write` is `return true` |

**The load-bearing claim — `FileStream+0x20` IS a `FileInfo`** — now has a third
independent source. `SaveText`'s frame pointer is `rsp-0x37`, so every offset in
its epilogue is odd and none of it could have been copied from the constructor
the header was derived from; it reaches `FileInfo.handle` at `+0x20`,
`FileInfo.cursor` at `+0x2c` (`ADD [rbp-0x2d],EAX` after `WriteFile`) and
`FileInfo.winHandle` at `+0x38` (`WriteFile`'s `RCX`). Those are the offsets
`runtime/types/io/FileSystem.h` already asserts, with no fudge factor.

### The layout oracle's first execution

```
[IOStream] layout probe ctl=0
[IOStream]   FileStream            wrote=68/0x50
[IOStream]   BinaryFileStream      wrote=68/0x50
[IOStream]   CompressedFileStream  wrote=160/0xb8
[IOStream]   TextFileStream        wrote=112/0x80
[IOStream]   VisitorStream         wrote=20/0x18
[IOStream] LAYOUT ctl=0 checked=29 failed=0  PASS
```

`wrote=` is the **per-cell positive control**: a constructor that never ran writes
nothing and trivially respects any guard, so a cell with `wrote=0` prints `WARN …
cell is vacuous`. None did.

### Negative controls, scored separately

| ctl | perturbation | result | att | mechanism |
|---|---|---|---|---|
| 1 | `FileInfo` read at `+0x24` instead of `+0x20` | **CAUGHT**, 4 failures | 3 | every `FileInfo` read goes through one helper; 3 cells attended, and the `FileStream` cell reads two fields off one attendance |
| 2 | `TextFileStream`'s two `String`s swapped | **CAUGHT**, 2 failures | 2 | screen C — retail's own `?GetObjectType@`/`?GetObjectName@` |
| 3 | `sizeof(CompressedFileStream)` = `0x98` | **CAUGHT**, 28 bytes past | 1 | screen A's write-extent guard |
| 4 | `sizeof(VisitorStream)` = `0x10` | **CAUGHT**, 8 bytes past | 1 | screen A's write-extent guard |
| 5 | `+0x68` / `+0x6c` swapped | **NOT CAUGHT** | 4 | **predicted, and a fact about the constructor, not about the probe's inputs**: both ctors write `6` to both fields and `SaveText` writes `2` to both, so no input set can separate them. What distinguishes them is that `SaveText` branches on `+0x6c` alone for the byte-order mark — see §3, where control 3 of the *SaveText* probe drives exactly that. |

4 of 5 caught, each via the mechanism it names.

---

## 2. `?Add@ExternalFactory@Object@@` — three defects in the inherited draft

The body itself was reviewed instruction-by-instruction against `0x1800be570`
and is faithful: the descent's asymmetric null arms, left/right at `node+0x20` /
`+0x28`, the `SETNE`-before-`OR` clean-bit ordering, `Insert`'s
`(set, out, key, value)` register order, and the release on both paths all match.
Three things were wrong around it.

**(a) `DumpAddCounters` was defined and never scheduled.** It is the file's own
stated positive control — its comments call `g_ac.calls` "the positive control
that says so from inside this function" — and it could not print. Both sibling
files in the directory `std::atexit` theirs (`external_find.cpp:302`,
`external_path_win.cpp:993`). Scheduling it immediately paid: `visited=1799`
against `skipped=3221` is the branch split the whole function exists to make,
now measured rather than argued.

**(b) The `GetPath` comment inverted the ABI.** It said the hidden return slot is
the first argument and `this` the second. The call site says the opposite —
`1800be594 LEA RDX,[RBP+0x30]` (the slot, *second*), `1800be598 MOV RCX,RSI`
(`external`, *first*). The **code was right and only the prose was wrong**, which
is the more dangerous way round: the next reader fixes the code to match the
comment.

**(c) The inlined `~VisitorStream` claim was false.** The draft said retail's
inlined copy makes ONE vtable stamp. It makes two — `1800be67a`
`??_7VisitorStream`, then `1800be685` `??_7Stream` — the same as the out-of-line
destructor. Not a behavioural difference (nothing observes the intermediate),
but it was a statement about retail that was false, and the transcription is free.

---

## 3. `?SaveText@External@Object@@` — and why retail's own `SaveText` faults

`runtime/src/object/external_savetext_win.cpp`, gated
`BANISHED_RT_EXTERNAL && BANISHED_RT_SAVETEXT`.

The body: look the object's type id up in `Object::Factory`'s AVL tree, take the
registered class name out of the `BaseTypeInfo` it points at, construct
`IO::TextFileStream(type, name, 0)`, force `_requestedFormat` and
`_activeEncoding` to 2, `FileStream::Create` the file, emit a byte-order mark if
the encoding asks for one, then dispatch the object's own `Serialize` at vtable
`+0x28` and return **its** answer. Then the inlined
`~TextFileStream → ~FileStream → ~Stream` chain.

Two deliberate choices:

* **The file is `_win`.** Retail writes the BOM with a direct `WriteFile` through
  the `FileInfo`'s Win32 handle (`0x1800bbafc`, Runtime's import thunk at
  `0x18014b108`), not through the stream's `Write` virtual. `PLAN.md` §4.1 keeps
  `<windows.h>` out of `runtime/src` with the `_win` suffix as the declared
  escape hatch, which `filesystem_*_win.cpp` already uses.
* **`~FileInfo` (`0x18000c180`) is transcribed, not called.** It is one of the few
  callees here that is genuinely unexported (`package_file.cpp` names it as an
  unported blocker), and its whole body is two `String` releases at `+0x28` then
  `+0x20`. An address door would have grown the cutover surface for four lines.

### The first oracle crashed, and finding whose null it was is the result

The obvious oracle — ask ours and theirs each to write the same live `External`
to a different file and compare the bytes — faulted inside `Runtime-x64.dll` at
`?Serialize@StringTable@UI@@+0x53` (`0x18011d993`), `MOV RAX,[RCX]` with `RCX=0`.
In a run that had just registered a new port that reads as a port defect.

It is not one. **Reordering the probe so THEIRS runs first** settled it in one
run: the same fault, at the same address, from the shipped export, with our body
never entered — the log stops at `subject 1: THEIRS...` and never reaches the
line after it.

**The upstream null, named rather than guarded.** Every `External::Serialize`
begins by asking the stream's `IO::Encoding` for a `Serializer` —
`encoding->vtbl[+0x18]` reading, `[+0x20]` writing. In the shipped DLL the TEXT
encoding's two factory slots are stubs, and the BINARY one's are real:

```
text encoding   0x1801559d0   (??0TextFileStream stamps it at 1800ba431)
    +0x18 -> 0x1800b9a80   XOR EAX,EAX ; RET
    +0x20 -> 0x1800b9a70   XOR EAX,EAX ; RET
binary encoding 0x1801554a8   (??0CompressedStream stamps it at 1800aa475)
    +0x18 -> 0x1800a8120   allocates through the MemoryAllocator singleton
    +0x20 -> 0x1800a8080   likewise
```

So the text serializer classes are **stripped from the retail build** — text I/O
is a `Tools-x64.exe` path — and `External::SaveText` cannot complete against any
real object in this DLL, no matter who calls it. That is why `calls=0` was the
only thing a live run could ever have reported for this row.
`BANISHED_RT_SAVETEXT_REAL=1` reproduces the fault on demand.

### So the subject is synthetic — and only the one virtual retail cannot service is fake

`SaveText` touches exactly two of its argument's virtuals. The subject's `+0x10
GetTypeId` returns a **real registered type id** lifted from a live `External`, so
the Factory lookup is a real hit; its `+0x28 Serialize` records the stream state
it was handed and writes a fixed payload through the stream's **own** `Write`
virtual (`?Write@TextFileStream@IO@@`, `0x1800ba9f0`, a real retail body). Every
other vtable slot is a trap that aborts. Everything the function under test
actually does — the AVL lookup, the `String`, the constructor,
`FileStream::Create`, the BOM branch, the file write, the three-level destructor
chain — runs for real on **both** sides.

Three screens: the two returns; ten fields of stream state (`objectType` and
`objectName` as text, `objectVersion`, `direction`, the Encoding vtable pointer,
`requestedFormat`, `activeEncoding`, file-open, `Write`'s answer); and the two
files byte for byte. Plus the strong form — retail's `objectType` must **be** the
`BaseTypeInfo::name` the Factory holds for that id, not merely equal to ours; two
sides agreeing on the wrong field would pass the comparison alone. It is
`"StringTable"`.

**Positive control:** `bytes=42`, `serCalls=1/1`. Two empty files compare equal, so
`bytes=0` or a side that never reached `Serialize` is declared VACUOUS.

| ctl | perturbation | result | att | evidence |
|---|---|---|---|---|
| 1 | ctor `type`/`name` swapped | **CAUGHT**, 2 failures | 1 | the two `String` fields, ours/theirs transposed |
| 2 | `objectVersion` 1 instead of 0 | **CAUGHT**, 1 failure | 1 | `+0x50` |
| 3 | `_activeEncoding` forced to 3 | **CAUGHT**, 2 failures | 1 | file 45 vs 42 bytes, `ours head: ef bb bf 49 4f 53` vs `theirs head: 49 4f 53 54 52 45` |

**Control 3 is the only thing that ever executes the BOM transcription** — no
caller in either shipped binary sets `_activeEncoding` to anything but 2. The head
dump was added because a control that catches must be shown to catch *via the
mechanism it names*: "the file grew by 3" would not distinguish the UTF-8 mark
from three bytes of anything else.

One reporting defect was fixed here too. The vacuity rule was evaluated *before*
the control branches, so control 3 — which catches by making the files different
sizes, hence comparing no bytes — printed `VACUOUS` with its own two failures two
lines above it. It is now gated on `failed == 0`.

---

## 4. `?Load@External@Object@@` — the brief's scope for this row was wrong

The lane brief said each row "constructs the stream, calls an exported entry
point, then dispatches the External's own `Serialize` at vtable `+0x28`". That is
true of `SaveText`. **`Load` is a full deserialisation driver:**

1. inlined `CompressedFileStream` construction (`CompressedStream` ctor, vtable,
   `BinaryFileStream` member at `+0x68`);
2. `Open` through the `BinaryFileStream`'s **own slot 7** — `Open`, not `Close`;
   the same slot means `Close` on the `MemoryStream`/`CompressedStream` branch,
   and a port that assumed one shape for both would call the wrong virtual and
   not crash;
3. `CompressedStream::Open` layered over it;
4. a `Serializer` built from the stream's `Encoding` (`+0x18` reading, `+0x20`
   writing);
5. a magic check, `0x5412a91f`;
6. a **type id** read that — when the caller passes no object — goes to
   `Object::Factory`'s tree and **creates one through `BaseTypeInfo::factoryFn`
   (`+0x20`)**, then checks `IsDerivedFrom(External)`;
7. a version check against the object's own `+0x18` virtual;
8. a dependency-array replay (a count, then per element a `String` and two
   `int32`s);
9. and only then the `+0x28` dispatch.

So this row is a **live use of the object-creation path Wave 11 exists for**:
`created=2847` per boot. Nothing new is ported for it — the factory pointer is
the DLL's own typeinfo, called through it — but the row is not a stream wrapper,
and scoping it as one would have under-budgeted it by a lot.

**`IO::Serializer` is a 16-byte facade `{Stream*, Impl*}`**, not the object
carrying the vtable. Retail keeps two adjacent stack slots — `[rsp+0x20]` the
Stream (written at `1800bc69b`, read back as `->direction` at `1800bc7ab`) and
`[rsp+0x28]` the implementation (every `[rax+0x98]`/`[rax+0xa0]` dispatches
through it, deleted with `vtbl[0](1)` at `1800bc841`) — and passes the **address
of the pair** to `?Serialize@String@System@@QEAAXAEAVSerializer@IO@@@Z`, whose own
mangled name says that argument is an `IO::Serializer&`. That is what settles the
shape; getting it wrong would have mis-shaped every serializer call in the body.

**The missing-type fault is transcribed, not guarded.** `1800bc73b` puts ZERO in
`RCX` and `1800bc73e` dereferences it — the same load-bearing fault
`Factory::CreateObject` and `Base::IsDerivedFrom` take. Read through a `volatile`
pointer so the compiler cannot decide the null path is unreachable: the deref has
to actually happen, because it is how a missing registration announces itself.

### The oracle: a live ours-vs-theirs replay

Possible here and not for `SaveText`. `Load` is **static** and takes a **path**,
and does not touch `ExternalFactory`'s tree — its caller does — so a second call
on the same path with `existing = nullptr` builds a second independent object
from the same file. Nothing short-circuits on a repeat (contrast the
`KeyMap::RegisterInput` trap, where retail's second insert returns early and the
comparison goes vacuously green). The duplicate is destroyed through its own
deleting destructor.

Compared per subject: both returned non-null, the type id, the version, the
dependency count, and per element the `Filename` **text** and the two `int32`s at
`+0x08` and `+0x0c`.

**Effectiveness control:** `deps=421` elements actually compared across 8
subjects. A run where every subject had zero dependencies would compare four
scalars each and report green with the only variable-length loop unentered; the
verdict line prints `WEAK` when `deps=0`.

| ctl | perturbation | result | att | note |
|---|---|---|---|---|
| 1 | the two per-element `int32`s read in the OPPOSITE order | **CAUGHT** `failed=842` | 421 | exactly 2 failures per element attended, and the printed pairs are literal transpositions |
| 2 | the dependency **count** through the `int32` overload (`+0x98`) instead of the `uint32` one (`+0xa0`) | **NOT CAUGHT** | 9 | a **fact about the two overloads**: they share a wire format for a 4-byte value, so substituting one for the other is not observable in the object. Reported rather than dropped so the table does not read 2/2. |

Neither control changes a pointer, a size or a stride, so neither can write out
of bounds.

**Also recorded:** `External.h` calls each dependency element's tail an opaque
`uint64_t` at `+0x08`. This body is the writer that splits it, and it splits it
into **two `int32`s** (`1800bc7fc` and `1800bc811`, both through the `+0x98`
overload).

---

## 5. Census — set-diff first

Run from inside this worktree with the project venv's interpreter.

**The set-diff is exact and is the primary number.**

```
LEFT the called-but-not-ported set (3):
  Object  ?Add@ExternalFactory@Object@@QEAAXPEAVExternal@2@@Z
  Object  ?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z
  Object  ?SaveText@External@Object@@QEAA_NAEBVFilename@System@@0@Z

ENTERED it (11):
  IO   ??_7FileStream@IO@@6B@              IO   ??0BinaryFileStream@IO@@QEAA@XZ
  IO   ??_7BinaryFileStream@IO@@6B@        IO   ??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z
  IO   ??_7CompressedFileStream@IO@@6B@    IO   ??0VisitorStream@IO@@QEAA@XZ
  IO   ??_7TextFileStream@IO@@6B@          IO   ??1FileStream@IO@@UEAA@XZ
  IO   ??_7VisitorStream@IO@@6B@           IO   ?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z
  internal  0x18002c6a0
```

Percentages, secondary:

| namespace | pre-lane `314b74c` | now | note |
|---|---|---|---|
| IO | 407 / 110 / 83 = **75.5%** | 407 / **120** / 83 = **69.2%** | `ported` did not move: this lane ported no IO function, it only *called* ten more. The pre-registered prediction was "~8-10 stream names, IO's percentage will fall"; it is 10. |
| Object | 191 / 61 / 47 = **77.0%** | 191 / 61 / **50** = **82.0%** | +3, and `called` did not grow — all three rows were already counted |
| TOTAL | 4632 / 1152 / 933 = **81.0%** | 4632 / **1163** / **936** = **80.5%** | |

---

## 6. What remains

* **`0x18002c6a0`** — `Collection::Array<ExternalArrayElement>::SetSize`, the one
  genuine **address door** this lane adds, called on `Load`'s read path. It is
  unexported in both builds and nothing in the tree registers it; at cutover it
  needs a body or that call site needs revisiting. It is the single row in the
  `ENTERED` list above that is not an exported name.
* **The ten new IO names** are all exported constructors, destructors and vtable
  data slots. They are dependency-surface growth, not work items in the same
  sense: each is a one-line door, and five of them are `??_7…` data.
* **`?SerializeReferenceTable@ExternalFactory@Object@@`** stays out of scope, as
  the brief said — its blocker is the `Collection::Set<ReferenceEntry>`
  `0x10`-byte-element AVL family (follow-up #3).
* **`SaveText` can never be verified against a real object in the shipped DLL.**
  §3 proves why. If the ToolKit's `Runtime-x64-profile.dll` is ever put on disk
  (it exists only inside the Ghidra project today), the same probe run against it
  with a *real* subject would close the last gap in that row.

## 7. Things found that other lanes should know

1. **`runtime/types/object/External.h` includes `TypeInfo.h` at its line 322 —
   *inside* its own `namespace RtObject`.** Any translation unit that includes
   `External.h` first gets `RtObject::RtObject::Factory`, and a plain `Factory`
   inside `namespace RtObject` does not name a type. It compiles today only
   because every existing consumer happens to include `TypeInfo.h` first or
   spells the nested path. Both new files here include `TypeInfo.h` first, with
   the reason written at the include.
2. **The perf build was already broken in the inherited tree.**
   `standalone/log_strip.h` `#define`s `printf(...)` away, and its own banner says
   "all call sites must use unqualified `printf`" — `std::printf` expands to
   `std::((void)0)(…)`. Two inherited files and four new ones tripped it. Fixed in
   all six. The brief's warning that "the perf build silently stopped compiling
   once already" was live, not historical.
3. **Slot 7 of a `FileStream` is `Open`; slot 7 of a `MemoryStream` /
   `CompressedStream` is `Close`.** `IO::Stream` declares only seven virtuals, so
   slot 7 is whatever the derived class declares first. A port that dispatched
   `+0x38` on a `FileStream` expecting `Close` would call `Open` and would not
   crash.
