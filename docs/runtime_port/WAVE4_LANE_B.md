# Wave 4, lane B — IO Stream family (`IO::CompressedMemoryStream`)

Branch `wave/upgrade-path-complete`, 2026-09-06.

**Nothing in this lane has executed.** Four lanes are working the IO wave concurrently and
the orchestrator owns the build and the integration run, so this lane's evidence is a
syntax check plus static analysis of the shipped DLL. Every claim below is labelled
MEASURED (read out of the binary or the export directory) or INFERRED (read off a
decompile). No number in this document came from a running process.

---

## 1. Result table

| # | Export | Retail | Profile | Ord | Status | Oracle |
|---|---|---|---|---|---|---|
| 1 | `??0CompressedMemoryStream@IO@@QEAA@XZ` | `0x1800a9520` | `0x18005c240` | 106 | **PORTED** | **FULL** — construct theirs into scratch, compare all 0x98 bytes |
| 2 | `??1CompressedMemoryStream@IO@@UEAA@XZ` | `0x1800a9560` | `0x18005c280` | 551 | **PORTED** | PARTIAL — post-state shadow |
| 3 | `?Create@CompressedMemoryStream@IO@@QEAA_NXZ` | `0x1800a9600` | — | 1874 | **PORTED** | PARTIAL — argument shadow |
| 4 | `?Open@CompressedMemoryStream@IO@@QEAA_NXZ` | `0x1800a9500` | — | 3302 | **PORTED** | PARTIAL — argument shadow |
| 5 | `?Close@CompressedMemoryStream@IO@@UEAAXXZ` | `0x1800a9620` | — | 1719 | **PORTED** | PARTIAL — presence only, proves nothing |
| 6 | `?GetMemoryStream@…@IO@@QEAAAEAVMemoryStream@2@XZ` | `0x180024350` | — | 2784 | **PORTED** | **FULL** — pure, call both |

Six of six ported. Two honest full oracles, three partials, one presence-only counter.
Nothing held back.

Files touched (all owned by this lane):

* `runtime/src/io/compressedstream.cpp` — the six bodies + `RtIoCompressedStreamRegister()` (was a stub `return 0;`)
* `runtime/types/io/Stream.h` — **new**; `IO::Stream`, `MemoryStream`, `CompressedStream`,
  `CompressedMemoryStream`
* `docs/runtime_port/WAVE4_LANE_B.md` — this file

No `stream.h` was needed: the six bodies are `extern "C"` and nothing outside the file calls
them. `runtime/src/io/rt_io_port.cpp` already declares `RtIoCompressedStreamRegister()`; it was not
edited.

Syntax check (the only build this lane ran) is clean with `-Wall -Wextra`:

```
ccache g++ -std=c++17 -fsyntax-only -Wall -Wextra -I. -Iengine/types -Iengine/funcs \
  -Icode/Game -Iruntime/types -Iruntime/funcs -Idx11/types -Idx11/funcs -Istandalone \
  -include standalone/env_cache.h runtime/src/io/compressedstream.cpp
```

---

## 2. Export-name verification — MEASURED

`rt_io_port.cpp` warning 2, and `reference_resolveexport_phantom_names.md`: a `ResolveExport`
name the DLL does not contain returns NULL silently, the address fallback hides it, the census
counts the port and the oracle reports `calls=0`.

### The six ported exports — covered by the wave-wide sweep

**Authoritative source: `docs/runtime_port/wave4_export_names.txt`** (39 IO call-surface
symbols, 39 ok, 0 MISSING). This lane's six rows are cited from it rather than re-derived:

```
OK  0x1800a9520 ord=106   ??0CompressedMemoryStream@IO@@QEAA@XZ
OK  0x1800a9560 ord=551   ??1CompressedMemoryStream@IO@@UEAA@XZ
OK  0x1800a9600 ord=1874  ?Create@CompressedMemoryStream@IO@@QEAA_NXZ
OK  0x1800a9500 ord=3302  ?Open@CompressedMemoryStream@IO@@QEAA_NXZ
OK  0x1800a9620 ord=1719  ?Close@CompressedMemoryStream@IO@@UEAAXXZ
OK  0x180024350 ord=2784  ?GetMemoryStream@CompressedMemoryStream@IO@@QEAAAEAVMemoryStream@2@XZ
```

(This lane had independently run the same pefile query before writing any body; the two agree
on all six addresses and ordinals. Recorded only as a corroboration — the file above is the
citation.)

### The ten symbols the wave-wide sweep does NOT cover — checked by this lane

The central sweep covered call-surface symbols only. The three vtable data symbols and the
seven call-through targets are not in it, so they were checked here with `pefile` against
`C:\GOG Games\Banished\Runtime-x64.dll` **before** any body was written. All ten present:

```
??_7CompressedMemoryStream@IO@@6B@                    0x180155560  ord 1409   (vtable)
??_7MemoryStream@IO@@6B@                              0x1801555b0  ord 1465   (vtable)
??_7Stream@IO@@6B@                                    0x1801549a0  ord 1506   (vtable)
??0CompressedStream@IO@@QEAA@XZ                       0x1800aa3d0  ord 108
??1CompressedStream@IO@@UEAA@XZ                       0x1800aa650  ord 552
?Commit@CompressedStream@IO@@QEAAXXZ                  0x1800a9ee0  ord 1736
?Create@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z   0x1800aa4b0  ord 1875
?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z     0x1800aa560  ord 3303
??0MemoryStream@IO@@QEAA@XZ                           0x1800ae710  ord 273
??1MemoryStream@IO@@UEAA@XZ                           0x1800ae890  ord 631
```

The three vtable names are load-bearing for §4: the whole ownership argument rests on
`??_7CompressedMemoryStream@IO@@6B@` existing.

### `GetMemoryStream` at `0x180024350` is NOT folded — checked, negative

Five of the six ported exports are contiguous in `0x1800a95xx`–`0x1800a96xx`, the class's own
code; `GetMemoryStream` sits far away at `0x180024350`. The obvious worry is identical-COMDAT
folding — if the linker had folded several accessors onto one address, then
`RegisterRuntimeAddr(0x180024350, …)` would silently serve all of them. **Measured: it has
not.** Grouping the whole export directory by RVA, every one of this lane's nine addresses
carries exactly **one** exported name:

```
va=0x180024350  1 name(s)   ord=2784  ?GetMemoryStream@CompressedMemoryStream@IO@@…
va=0x1800a9520  1 name(s)   va=0x1800a9560  1   va=0x1800a9600  1
va=0x1800a9500  1 name(s)   va=0x1800a9620  1
va=0x180155560  1 name(s)   va=0x1801555b0  1   va=0x1801549a0  1
```

The real explanation for the distance is a **COMDAT pool of inline-defined and
compiler-generated one-liners**, not folding. The `0x18002xxxx` band holds exactly that kind of
member for these classes: `GetData@MemoryStream` `0x180024180`/`0x180024190`, `MemoryStream`
copy ctor `0x1800241a0` and `operator=` `0x180024200`, the `MemoryStream` deleting-dtor thunk
`0x1800242a0`, `GetMemoryStream` `0x180024350`, the `CompressedMemoryStream` copy ctor
`0x180024360` and `operator=` `0x180024400`, its deleting-dtor thunk `0x1800244a0`,
`GetCopy@CompressedCopyFileStream` `0x180024520`. Header-defined bodies gathered by the
linker — each still its own COMDAT, each still uniquely named.

So both registrations for this function are safe: the name registration binds one name, and
the address registration binds one address that no other export shares.

---

## 3. Layouts, and what pins each offset

Full commentary lives in `runtime/types/io/Stream.h`; every offset there carries a
`static_assert` and names the instruction or body that fixes it. Summary:

### `IO::Stream` — 0x18 — MEASURED from `??0Stream@IO@@QEAA@XZ` (`0x1800b99d0`)

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | vptr | ctor stores `&Stream::_vftable_` (`0x1801549a0`) |
| 0x08 | `_direction` | ctor zeroes it; `CompressedStream::Create` sets **1**, `::Open` sets **0** |
| 0x10 | `_encoding` | ctor zeroes it; every concrete ctor then allocates 8 bytes and stores the shared Encoding vtable `0x1801554a8` |

**Wave 3 lane B's `stream+0x08` == direction, 0 == reading: RE-VERIFIED, holds.** The two
witnesses are the write path (`Create` → 1) and the read path (`Open` → 0). I did not
re-derive `stream+0x10` as the encoding factory beyond confirming it is the field both
destructors free; the vtable-slot-3/4 arity question Wave 3 left open is untouched by this
lane and is still open.

### `IO::MemoryStream` — 0x30 — MEASURED from ctor `0x1800ae710`, dtor `0x1800ae890`, `Read` `0x1800ae950`

`+0x18 _data`, `+0x20 _ownsData` (byte), `+0x24 _cursor`, `+0x28 _size`. This is exactly what
`engine/types/io_serializer.h` already had; that header is correct and needs no change.

### `IO::CompressedStream` — 0x68 — MEASURED from ctor `0x1800aa3d0` (disassembled), `Create` `0x1800aa4b0`, `Close` `0x1800aa8d0`, `SetWindowCount` `0x1800aa020`

| Off | Field | Pinned by |
|---|---|---|
| 0x1c | `_position` (decompressed-stream cursor) | ctor `MOV [RCX+0x1c],0` |
| 0x20 | `_activeWindow` | ctor `MOV dword [RCX+0x20],0xffffffff`; `SetWindowCount` overwrites with `GetAvailableWindow()` |
| 0x28 | `_source` (`Stream*`) | `Create`/`Open` store the argument here |
| 0x30/0x38/0x3c | `Array<Window>` data/size/cap, 0x18-byte elements | `Close` strides `+0x30` by 0x18 bounded by `*(int*)(this+0x38)` |
| 0x40/0x48/0x4c | `Array<Header>` data/size/cap, 8-byte elements | `Close` strides `+0x40` by 8 bounded by `*(int*)(this+0x48)` |
| 0x50/0x58/0x5c | `Array<void*>` (compressed blocks) | same walk, indexed off `+0x50` |
| 0x60 | `_compression` (`System::Compression*`) | `Create` stores the allocation here |

`+0x18` and `+0x24` are **never written by the constructor** and nothing in this wave reads
them. They are recorded as `unknown18` / `unknown24` rather than guessed. The three arrays
follow the Wave 2 `Collection::Array` shape (`{_data, _size:i32, _capacity:i32}`).

### `IO::CompressedMemoryStream` — 0x98 — MEASURED

`sizeof` is **not** inferred from the last field: the class's deleting-destructor thunk
(`0x1800244a0`, vtable slot 0) calls `eh_vector_destructor_iterator(p, 0x98, n, ~CMS)`, and
`0x98` is the element stride an array delete uses. `+0x00` is the `CompressedStream` base and
`+0x68` an embedded `MemoryStream` **member** — pinned four times: the ctor's
`LEA RCX,[RBX+0x68]`, both `Create` and `Open` passing `this+0x68` as the `Stream&`,
`GetMemoryStream` being literally `LEA RAX,[RCX+0x68]; RET`, and the profile build using the
same `+0x68` with the calls left out of line.

So a `CompressedMemoryStream` is a `CompressedStream` whose sink/source is its own embedded
buffer.

### The two "cursors" — both existing comments are right

`gameupdateloop_serialize.cpp:1105` says the cursor is at `stream+0x1C`;
`componententity.cpp:412-418` and `engine/types/io_serializer.h:151-155` say `+0x8C`. Both
are correct and they measure different things:

* `+0x1c` — `CompressedStream::_position`, the logical position in the **decompressed** stream.
* `+0x8c` — `0x68 + 0x24`, the embedded `MemoryStream::_cursor`, the position in the
  **compressed** byte image.

Neither comment needs correcting; §8 adds a cross-reference so the next reader is not caught
by the apparent contradiction.

---

## 4. Vtable ownership — the argument for THIS class

`??1CompressedMemoryStream` and `?Close` are both `UEAA`, i.e. virtual. PLAN §4.3 risk 3 and
WAVE3.md finding 4 (which says in capitals that Wave 3's `Object::Internal` argument is **per
class** and does not transfer) both apply. Re-made here from scratch:

**(a) The vtable is an exported data symbol.** `??_7CompressedMemoryStream@IO@@6B@` is at
`0x180155560`, ordinal 1409 — MEASURED with pefile, §2. So are the two base tables. The ported
ctor and dtor resolve that name and stamp **the DLL's own vtable**. No private copy is built,
so there is exactly one vtable and one RTTI record for this class in the process, and the
"two vtables in play" hazard — the swap-remove UAF family — cannot arise. This is the same
shape as Wave 3's `Object::Internal`, argued independently rather than inherited.

**(b) What the port therefore does NOT capture.** Because the DLL's vtable is untouched, slot
0 (the deleting-dtor thunk) and slot 7 (`Close`) still point at the DLL's bodies. Every
*virtual* dispatch keeps landing in the DLL; our bodies are reached only through our own
direct resolved-pointer calls. Both are faithful reimplementations over an identical layout,
so which copy runs is unobservable — but the diff oracle only ever sees our callers, and the
census will overstate coverage if read as "the DLL now uses ours". No detour is installed;
detouring a virtual whose slot we deliberately leave pointing at the DLL buys nothing.

**(c) No object crosses the boundary.** MEASURED via Ghidra xrefs on all six retail addresses:

| Export | Code xrefs inside Runtime-x64.dll |
|---|---|
| ctor `0x1800a9520` | **none** (data only: export table, RTTI) |
| dtor `0x1800a9560` | `FUN_180024470`, the deleting-dtor thunk `FUN_1800244a0`, **`ReadPackfile` `0x1800b6054`**, and 4 SEH unwind funclets |
| `Create` `0x1800a9600` | **none** |
| `Open` `0x1800a9500` | **none** |
| `Close` `0x1800a9620` | **one**, and it is vtable slot 7 itself at `0x180155598` |
| `GetMemoryStream` `0x180024350` | **none** |

`IO::PackageFile::ReadPackfile` is a real DLL-internal caller of the destructor and it calls it
**directly**, so registering our port does not redirect it — it builds and destroys its own
`CompressedMemoryStream` entirely inside the DLL. Our caller (`GameUpdateLoop::SaveRestore`,
exe `0x1400959c0`) builds its own in a stack buffer and calls all six through resolved function
pointers, never `new`/`delete`, never virtually. The two populations never mix.

**(d) Conclusion.** The class is not ported "half-way" in the sense PLAN §4.1 decision 5 warns
about. The unported members — copy ctor, `operator=`, `Open(void*,int)`, `PreCommit`, and the
whole `CompressedStream` base — operate on the same single layout through the same single
vtable. **The whole argument turns on (a).** Had the vtable symbol not been exported, the
honest move would have been to hold `~CompressedMemoryStream` and `Close` back and land only
the four non-virtuals.

---

## 5. What each oracle can and cannot prove

These six allocate and mutate, so a blanket "call ours, call theirs, compare" would construct
twice or commit twice. Per site:

| Site | Oracle | Proves | Does NOT prove |
|---|---|---|---|
| ctor | **FULL.** Build theirs into a private `alignas(16)` 0x98 scratch, `RT_DIFF_BYTES` all 0x98 bytes, then run the DLL dtor on the scratch. The two `IO::Encoding*` fields (`+0x10`, `+0x78`) are fresh allocations and cannot match, so each is replaced **in both comparison copies** by the first qword of the object it points at — the Encoding vtable. | Every one of the 0x98 constructed bytes, including the vtable pointer, `_activeWindow = -1`, all three zeroed arrays, and that both encodings exist and carry the same vtable. | The Encoding *addresses* (deliberately masked). |
| `~CMS` | PARTIAL (shadow). Asserts the post-state: both sub-objects left carrying `??_7Stream@IO@@6B@`. | That the teardown ran to the end through both base destructors. | The `Commit` decision — our code and retail read the same two fields, so asserting it is circular. Running theirs would double-free. |
| `Create` | PARTIAL (shadow). After the call-through, asserts `_direction == 1`, `_source == &_memoryStream`, `windowSize == 2`, `_compression != nullptr`, and that it returned true. | That we handed `CompressedStream::Create` **the arguments retail hands it** — the sink and the literal `2`, which is the only number in the function. | Anything about the callee. It is the DLL's own routine either way. Running theirs again would allocate a second `Compression` and leak the first. |
| `Open` | PARTIAL (shadow). Asserts `_direction == 0`, `_source == &_memoryStream`, `windowSize == 1`. | Same as `Create`, for the literal `1`. Those three are written before any early return, so they hold whether `Open` succeeds or fails. | The return value is deliberately **not** asserted: `Open` legitimately returns false when the window header table does not check out, and `SaveRestore` does not test it either. Running theirs would read from a moved cursor. |
| `Close` | **PRESENCE ONLY.** Counts calls and records which branch the `Commit` guard took. | That the save path was reached at all — the `calls=` column, which is the thing WAVE3 says to read. | **Nothing about equivalence.** Calling the original afterwards would `Commit` twice and corrupt the save, and the guard is the same two fields in both implementations. This body's correctness rests on the decompile, and this row says so rather than dressing a tautology up as a check. |
| `GetMemoryStream` | **FULL.** Pure — `LEA RAX,[RCX+0x68]; RET`. `RT_DIFF_SCALAR` against the original on the live object. | Bit-exact equality of the returned pointer. | — |

Two honest full oracles, three partials, one counter. Read `calls=`, not just `divergences=`.

### Which of these a run actually reaches — READ THIS BEFORE BELIEVING A CLEAN REPORT

All six live on the **save/load path only**. A plain `make sa-run` autostart session
**never calls any of them** and will report `calls=0` on all six sites, which is a clean
report that proves nothing.

| Path | Reaches |
|---|---|
| `sa-run` autostart, no save, no load | **none of the six** |
| A save (quicksave, autosave, or the save dialog) | ctor, `Create`, `Close`, `GetMemoryStream`, dtor — **five of six** |
| A load (`BANISHED_AUTOLOAD`) | ctor, `Open`, `GetMemoryStream`, dtor — and `Close` on the read path with the guard **false**, so `Commit` is skipped |
| `sa-stress` | autosave-dependent; do not assume |

`Create` and `Open` are mutually exclusive: one save exercises `Create`, one load exercises
`Open`. **A run that only saves leaves `Open` at `calls=0`, and vice versa.** To claim this
lane exercised, the wave needs a save run *and* a load run.

That matters because PLAN §5 records `BANISHED_AUTOLOAD` as **broken and pre-existing** —
the harness injects its Load click and then loops in `MenuDLT::Draw` forever with no world
created, reproducing with `BANISHED_RT_OBJECT=0` and with both `Save9` and `Auto`, and it is
why four Wave 3 functions landed registered-but-unexercised. Lane H is fixing that harness
this wave. **Coordination request, through this doc as instructed:** when lane H's fix lands,
the integration run should be `BANISHED_RT_DIFF=1` over (i) an autostart + save and (ii) an
autoload, and this lane's six sites read from `RtDiffSummary()` separately for each. Until
then the honest status of this lane is *landed, syntax-checked, not executed*.

---

## 6. Compression — what was and was not reimplemented

Explicitly, because `rt_io_port.cpp` asks for it: **no codec was reimplemented.** All six
ports are thin shims. The compression lives in `IO::CompressedStream`
(`Commit` `0x1800a9ee0`, `CompressWindow` `0x1800a9810`, `DecompressFromSource` `0x1800a96b0`,
`SetWindow` `0x1800a9aa0`) and under it `System::Compression::Compress` `0x1800c4230` /
`Decompress` `0x1800c4660`. Every shim calls the same exported DLL entry point retail calls
with the same arguments, so the compressed byte format is bit-identical by construction.

One near-miss worth recording. `?Close@CompressedStream@IO@@UEAAXXZ` (`0x1800aa8d0`) is
**not** a small function: MSVC inlined the whole of `Commit` into it. The profile build shows
the source is `~CompressedMemoryStream()` calling `CompressedStream::Close()`, but retail's
`~CMS` and `Close` both inline only the *guard* and then `CALL 0x1800a9ee0` — the out-of-line
copy of that same body, exported as `?Commit@`. The ports reproduce retail's literal form
(guard + `Commit`) rather than calling the exported `Close`. Reading the profile decompile and
transcribing `CompressedStream::Close()` would have been behaviourally identical here, but the
retail form keeps the two guard fields visible and asserted.

## 7. One deviation, and it is not a behavioural one

Retail's `~CMS` frees the two `IO::Encoding` objects through `FUN_180006a10` and the
`MemoryStream` buffer through `0x1800c7ad0` = `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`.
Those two addresses hold **the same function**: 96 bytes read out of each differ only in the
RIP-relative displacements —

```
180006a10  4885c97456574883ec3048c7442420feffffff 48895c2448 488bf9 488b1de65c1f00 ...
1800c7ad0  4885c97456574883ec3048c7442420feffffff 48895c2448 488bf9 488b1d264c1300 ...
```

`FUN_180006a10` is a second, un-exported copy of `MemFree` the linker did not fold. Since the
port reaches both through the exported `??1MemoryStream` and `??1CompressedStream`, nothing in
`compressedstream.cpp` names either address, and no behaviour changes.

## 8. Comments to correct — for the orchestrator (PLAN §6 step 8)

I own only `runtime/src/io/compressedstream.cpp`, `runtime/types/io/Stream.h` and this file. The
following are in files owned by others; **exact replacement text**, file:line:

**1. `code/Game/Game/gameupdateloop_serialize.cpp:1235`** — the size comment is vague and
now measurable. Current:

```cpp
    alignas(16) uint8_t cms[0x100];   // sizeof(CompressedMemoryStream) = 0x68 + MemoryStream
```

Replace with:

```cpp
    // sizeof(IO::CompressedMemoryStream) == 0x98, MEASURED: its deleting-dtor
    // thunk (Runtime 0x1800244a0) passes 0x98 as the eh_vector_destructor
    // stride. 0x68 CompressedStream base + 0x30 embedded MemoryStream at +0x68.
    // Layout: runtime/types/io/Stream.h.
    alignas(16) uint8_t cms[0x100];
```

**2. `code/Game/Game/gameupdateloop_serialize.cpp:1105-1106`** — the offset is right, the
*reason* given for it is wrong. Current:

```cpp
        // Ground truth. The cursor lives at stream+0x1C (NOT the MemoryStream
        // +0x24 in io_serializer.h -- this is a different stream class), so the
```

It is not a different stream class. A `CompressedMemoryStream` **contains** a `MemoryStream`
at `+0x68`, so `io_serializer.h`'s `+0x24` is live in this very object at `+0x8c`. The two
offsets measure different positions, not different classes. Replace those two lines with:

```cpp
        // Ground truth. The cursor lives at stream+0x1C. That is NOT a
        // contradiction of io_serializer.h's MemoryStream +0x24: this object
        // CONTAINS a MemoryStream at +0x68, so that cursor is live here at
        // +0x8c. They measure different things -- +0x1c is
        // CompressedStream::_position in the DECOMPRESSED stream, +0x8c is the
        // embedded MemoryStream::_cursor in the COMPRESSED byte image. Full
        // layout: runtime/types/io/Stream.h. So the
```

**3. `engine/types/io_serializer.h:151-155`** — the two constants are correct; add the
disambiguation next to them:

```cpp
// NOTE: this is the COMPRESSED-image cursor. The CompressedStream's own
// position in the DECOMPRESSED stream is a different field, at +0x1c.
// Full layout for all four stream classes: runtime/types/io/Stream.h.
```

**4. `code/Game/Game/gameupdateloop_serialize.cpp:727`** — "There is no IO::Stream::Tell
export" is true as written but incomplete, and the reader is looking for a position. Append:

```cpp
    // There is no Tell, but there IS ?GetCurrentPos@ on every concrete Stream
    // (vtable slot 5, +0x28): MemoryStream 0x1800ae9c0, CompressedStream
    // 0x1800aa850, FileStream 0x1800aac50 — all exported.
```

**5. `runtime/src/io/README.md`** — the README is CORRECT and needs no factual change; it
already lists `CompressedStream.cpp` and `MemoryStream.cpp` and instructs "mirror these names
here, lowercased". It is the lane's file that deviates: `stream.cpp` is the name the Wave 4
scaffolding in `rt_io_port.cpp` handed out, and by the README's own rule the six ported bodies
belong in `compressedstream.cpp` (`IO::Stream` / `IO::MemoryStream` in `memorystream.cpp`).
Renaming would touch `rt_io_port.cpp` and the makefile, which this lane does not own — worth a
one-line note under the file list so the next reader is not confused:

```markdown
Exception: `stream.cpp` is Wave 4 lane B's file for `IO::CompressedMemoryStream`, which the
recovered tree puts in `IO\CompressedStream.cpp`. The name came from the wave scaffolding;
rename it to `compressedstream.cpp` when the rest of that class is ported.
```

(The same note is at the top of `runtime/types/io/Stream.h`.)

**6. `docs/runtime_port/PLAN.md` §4.2, the Wave 4 row** — "Stream family" is 6 functions on
one class, not a family port. Suggested amendment when the wave is written up: *"Stream family
— our tree calls only `IO::CompressedMemoryStream` (6 exports, all thin shims over
`IO::CompressedStream`); the codec, `FileStream`, `BinaryFileStream`, `TextFileStream`,
`VisitorStream`, `CompressedFileStream` and `CompressedCopyFileStream` are untouched."*

---

## 9. What a later wave needs from the Stream family

Reported, not ported. Our tree calls nothing here today, but a full IO port does:

* **`IO::CompressedStream`** (`??0` `0x1800aa3d0`, `??1` `0x1800aa650`, `Create` `0x1800aa4b0`,
  `Open` `0x1800aa560`, `Close` `0x1800aa8d0`, `Commit` `0x1800a9ee0`, `Read` `0x1800aa780`,
  `Write` `0x1800aa6c0`, `SetCurrentPos` `0x1800aa830`, `GetCurrentPos` `0x1800aa850`,
  `GetSize` `0x1800aa860`, `PreCommit` `0x1800aa8c0`, `Release` `0x1800aa100`,
  `SetWindowCount` `0x1800aa020`, and the three private helpers `CompressWindow` `0x1800a9810`,
  `DecompressFromSource` `0x1800a96b0`, `DecompressFromData` `0x1800a9780`,
  `GetAvailableWindow` `0x1800a9980`, `IncrementUseCount` `0x1800a9940`, `SetWindow`
  `0x1800a9aa0`, `GetCompressedSize` `0x1800aa380`). **This is the real work of the family.**
  It needs `System::Compression` (`0x1800c41a0`/`0x1800c4230`/`0x1800c4660`, plus the three
  static helpers `First`/`Next`/`Index` at `0x180023e20`/`0x180023e30`/`0x180023e40`) and the
  `Window`/`Header` element layouts, neither of which this lane pinned.
* **`IO::MemoryStream`'s own methods** — `Create(int)` `0x1800ae7f0`, `Create(void*,int)`
  `0x1800ae650`, `Open(int)` `0x1800ae670`, `Open(void*,int)` `0x1800ae7d0`, `Read`
  `0x1800ae950`, `Write` `0x1800ae8f0`, `Close` `0x1800ae9e0`, `GetData` `0x180024180`/`0x180024190`.
  The layout is already pinned in `runtime/types/io/Stream.h`; only the bodies are missing.
  These are the natural next increment and they are diffable with a scratch buffer.
* **`IO::Stream` base** — `SetEncoding` `0x1800b99f0`, `GetEncoding` `0x1800b9a60`,
  `GetDirection` `0x180006200`, `GetSource` `0x18000c6a0`. Trivial, but `SetEncoding`/`GetEncoding`
  are the seam lane A's `IO::Encoding` sits behind.
* **`IO::CompressedMemoryStream`'s remaining four** — copy ctor `0x180024360`, `operator=`
  `0x180024400`, `Open(void*,int)` `0x1800a94d0`, `PreCommit` `0x1800a9660`. All unreferenced
  by our tree.
* **`IO::PackageFile`** already reaches `~CompressedMemoryStream` directly (`ReadPackfile`
  `0x1800b6054`). When PackageFile is ported, that call site becomes ours and the dtor's
  `calls=` count stops being save-path-only.
