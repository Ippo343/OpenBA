# `?ReadPackfile@PackageFile@IO@@` — the decomposition, and why it is one

**Verdict: `ReadPackfile` should NOT be ported as one body, and not because of its size.
It should not be ported FIRST at all.** Six of its callees are unported, and one of them
decides every byte of every package the game loads. This document is the dependency
reading, the sub-lane order, and the oracle available at each rung.

Lane PKG2 executed rung 1 (`System::Compression::Decompress`) in the same pass that
produced this plan. Rungs 2–4 are handed back.

---

## 1. The measurement that changes the shape of the problem

| | |
|---|---|
| `?ReadPackfile@PackageFile@IO@@QEAA_NAEBVFilename@System@@@Z` | `0x1800b46b0`, ord 3461 |
| Size | **1,864 instructions** — the largest single body of Wave 9 |
| Distinct callees | 23 |
| Calls to `?SetWindow@CompressedStream@IO@@` | **~25** |
| Calls to the CRT `memcpy` (`0x18008a290`) | **~28** |

**It is not 1,864 instructions of novel logic.** The `(SetWindow, memcpy)` pair repeats
about twenty-five times in a straight run, and `?Read@CompressedStream@IO@@` — which is
51 instructions and does exactly `SetWindow` + bounds + `memcpy` — is called out-of-line
only twice. **MSVC inlined `CompressedStream::Read` at twenty-five sites.** That is where
the byte count went.

So the logical body is a header/directory parser of maybe 700–800 instructions of
distinct logic, sitting on a stream stack. Both facts matter: the parser is tractable,
and it is *worthless* until the stack under it exists.

---

## 2. The callee set, named

Every address below was resolved against `Runtime-x64.dll`'s export directory with
`pefile`. The 23 callees are almost entirely one class.

### Already ported (Wave 4 lane B) — a real head start

| Address | Symbol |
|---|---|
| `0x1800aa3d0` | `??0CompressedStream@IO@@QEAA@XZ` |
| `0x1800ae710` | `??0MemoryStream@IO@@QEAA@XZ` |
| `0x1800a9560` | `??1CompressedMemoryStream@IO@@UEAA@XZ` |

And critically, **`runtime/types/io/Stream.h` already models `CompressedStream` in full**
— all 0x68 bytes, with a `static_assert` per offset: `position` +0x1c, `activeWindow`
+0x20, `source` +0x28, the window array +0x30/+0x38/+0x3c, the header array
+0x40/+0x48/+0x4c, the block array +0x50/+0x58/+0x5c, `compression` +0x60. Those are
exactly the fields `SetWindow` walks. Sub-lane 2 does not start from nothing.

### Unported, and on the path

| Address | Symbol | Instrs |
|---|---|---|
| `0x1800c4660` | `?Decompress@Compression@System@@SAHPEBXIPEAXI@Z` | 72 |
| `0x1800c4230` | `?Compress@Compression@System@@QEAAHPEBXIPEAXI@Z` | — (encoder; **not on the read path**) |
| `0x1800a96b0` | `?DecompressFromSource@CompressedStream@IO@@AEAA_NH@Z` | 52 |
| `0x1800a9980` | `?GetAvailableWindow@CompressedStream@IO@@AEAAHXZ` | 79 |
| `0x1800a9aa0` | `?SetWindow@CompressedStream@IO@@AEAA_NH@Z` | 296 |
| `0x1800aa780` | `?Read@CompressedStream@IO@@UEAA_NHPEAX@Z` | 51 |
| `0x1800aa560` | `?Open@CompressedStream@IO@@QEAA_NAEAVStream@2@H@Z` | 66 |
| `0x1800a9ee0` | `?Commit@CompressedStream@IO@@QEAAXXZ` | — |
| `0x1800ac880` | `?OpenFile@FileSystem@IO@@QEAA?AUFileInfo@12@AEBV…@Z` | 264 |
| `0x1800ace00` | `?ReadFile@FileSystem@IO@@QEAA_NAEAUFileInfo@12@HPEAX@Z` | 90 |
| `0x18008a290` | CRT `memcpy` (unexported) | — |
| `0x1800c8220` | `?SetLength@String@System@@AEAAXH@Z` (already reached elsewhere) | — |
| `0x1800c6970` | `?AdjustDelimiters@Filename@System@@AEAAXXZ` (**already ported**, lane FS) | — |

`?OpenFile@` and `?ReadFile@` were explicitly declined by lane FS as "the four unreached
callees" — they are unreached *there*, and reached *here*.

---

## 3. Two findings worth having on the record regardless of who takes this

**`0x18008a290` is the CRT `memcpy`.** Jump-table dispatch for n ≤ 16, `REP MOVSB` when
the ERMSB bit at `0x1801fc65c` is set, and `MOVNTI` streaming stores above 4 KB. It is a
pure byte copy with no observable behaviour beyond the bytes, so `std::memcpy` is a
byte-faithful replacement and this is **not** a lane-L-shaped CRT port. Do not spend a
lane on it.

**`Compression::Decompress` is a bespoke LZ77/LZSS byte codec — no zlib, no LZ4, no
external library.** 72 instructions, self-contained. This is worth stating loudly because
"the package format is compressed" invites the opposite assumption, and that assumption
would send a lane hunting for a library that is not there.

---

## 4. The order, and the oracle at each rung

The ordering constraint is not aesthetic. `ReadPackfile` opens a file, wraps it in a
`CompressedStream`, and then does ~25 windowed reads through it. Porting the parser first
would be 1,864 instructions standing on six null calls — the same anti-pattern lane PKG2
already declined for `~PackageFile`, one level up.

### Rung 1 — `Compression::Decompress` — **DONE AND MEASURED, lane PKG2 iteration 2**

`runtime/src/system/compression.cpp`, `BANISHED_RT_COMPRESSION=0`.

```
[RtDiff] System::Compression::Decompress   calls=177  worked=137  divergences=0
[PKG2/Compression] probe: 177 cases, round-trip ok=137 bad=0 (compressor declined 0)
[PKG2/Compression] branch coverage: literal=39879 matchShort=55927 matchExtended=9211
                   | failLiteral=3 failMatchDst=5 failMatchSrc=32 -- ALL SIX BRANCHES COVERED
```

**105,017 control bytes decoded, 0 divergences, and every branch of the codec entered.**
`worked=137` against `calls=177` is the effectiveness control working as intended: the 40
non-working cases are the deliberately malformed and tight-buffer inputs, which return 0
by design and must not be counted as coverage. The three failure buckets are small (3/5/32)
but non-zero *because they were driven on purpose* — that is the difference between a
covered branch and a lucky one.

The bottom of the stack, and the highest-consequence function in it: a defect here is not
a crash, it is silently wrong resource data. 72 instructions, pure, no state.

Oracle — and it is the best in the stack, which is why this rung is first:
`?Compress@Compression@System@@` (`0x1800c4230`, ord 1749) **is exported**, so the probe
compresses a real buffer with the DLL's own encoder and decompresses it with both
implementations. Three independent checks: ours == theirs on the return value and on
every byte; **ours round-trips to the original input** (a check that does not involve the
DLL's decoder at all, so a shared misreading of the format cannot pass it); and the three
failure exits agree.

Effectiveness control: six branch counters (literal run, short match, extended-length
match, and each of the three failure exits), all printed. A codec probe is precisely where
a uniform population hides — feed it garbage and nearly every case returns 0 from the same
bounds check, and 5,000 agreeing zeroes read as thorough. The corpus is built to fill all
six on purpose.

### Rung 2 — the `CompressedStream` read path — **DONE AND MEASURED, lane PKG2 iteration 3**

`runtime/src/io/compressed_stream_read.cpp`, `BANISHED_RT_CSREAD=0`. Four bodies
(`Read`, `SetWindow`, `GetAvailableWindow`, `DecompressFromSource`).

```
[CSRead] lockstep probe: 3 configurations, 225 reads, 591,474 bytes -- IDENTICAL
         (bytes, ORIGINAL content, position, activeWindow, every window record and page)
[CSRead] SetWindow branches: fastPath=221 scanHit=3 readMiss=19 -- all three
         READ-PATH branches exercised
```

Both requirements below were met, and both mattered: the effectiveness control was **wrong
twice before it was right**, and the third configuration (4 windows + a rewinding read
schedule) is the only thing that ever entered `SetWindow`'s scan-hit branch. Details in
WAVE9_LANE_PKG2.md §19.

**One named cutover blocker came out of it**: `SetWindow`'s `direction != 0` GROW path
(~200 instructions) is delegated to the shipped body, counted and announced. It is a null
call once the DLL is gone, and it is the save path that reaches it.

The original plan text follows.

### Rung 2 — the `CompressedStream` read path (as planned)

`SetWindow` (296) + `GetAvailableWindow` (79) + `DecompressFromSource` (52) + `Read` (51)
+ `Open` (66). ~544 instructions, one coherent object, layout already modelled.

`SetWindow` is the substantial one: it is a 32 KB-page (`0x8000`) window cache over the
source stream, with two `Collection::Array` grow loops inlined (stride 8 at +0x40 and
+0x50) and a linear scan over stride-24 window records at +0x30.

**Oracle — and this rung needs one built, because there is no free caller.**
`RegisterRuntimeExport` redirects *our* callers; the DLL's `ReadPackfile` reaches
`SetWindow` by a direct relative `CALL` and will never enter our port. So until rung 3
lands, nothing calls these. The oracle is a **dual-stream probe**: open the same real
`.pkg` twice, drive one stream through our ports and the other through the DLL's, and
compare at every offset. Read-only, non-destructive, and the corpus is 172 MB.

**Two hard requirements on that oracle, both learned elsewhere and both live here.**

1. **Compare the STREAM STATE, not only the bytes returned.** `CompressedStream` carries
   `position` (+0x1c), `activeWindow` (+0x20), the window array at +0x30/+0x38 and
   per-window use counts. A body can return identical bytes and leave the object in a
   different place — and the next `Read` then diverges for reasons the previous comparison
   said were fine. Lane PICK's general form: *if the result is observed only through a
   lossy transform, the oracle is blind to everything the transform discards.* Comparing
   returned bytes alone **is** that transform here. Rung 1 was exempt only because
   `Decompress` is `static` and its cursor is a local with nothing to compare against;
   rung 2 has no such excuse.
2. **Compare in LOCKSTEP, not at the end.** Apply each read to both streams and compare
   immediately, rather than running each to completion and comparing once. A defect that
   cancels out compares equal at the end while every intermediate state differed, and a
   block-by-block decompressor carrying state between blocks is precisely where that
   happens. Also lane PICK.

### Rung 3 — `FileSystem::OpenFile` + `ReadFile` — **DONE AND MEASURED, lane PKG2 iteration 4**

264 + 90, plus `PlatformOpenFile`, `PackageFile::GetFileInfo` and the unexported
`FileInfo` copy ctor. `runtime/src/io/filesystem_openfile_win.cpp`, switch
`BANISHED_RT_OPENFILE`. Full write-up: `WAVE9_LANE_PKG2.md` §26-31.

Result: 8 opens (packed=5 loose=2 miss=1), 24 reads, 71,471 bytes, **identical** on
handle, size, cursor, baseOffset, handle validity and every byte; all three
effectiveness paths non-zero; residue 137 → 135 with **nothing added**; both audits
clean.

**The plan's premise here was wrong in one place, and it mattered.** "Lane FS already
models `FileInfo` … so their signatures are settled" — lane FS's `+0x08` is a
`uint64_t size`, which is wrong and hides `+0x0C`, the read cursor. Three disassembly
proofs are in §27. The corrected struct now lives once, in
`runtime/types/io/FileSystem.h`; the duplicate at namespace scope in
`filesystem_setupdirs_win.cpp` is gone. Nothing had broken only because the old
layout's single reader casts to `uint32_t`.

Rung 3's cutover cost is **one** new address door, `0x18000c200`
(`FileInfo::operator=`, unexported). `_stricmp` at `0x18007eefc` was already a shared
blocker; this rung adds a caller, not a blocker.

### Rung 4 — `ReadPackfile` itself

Only now is it a transcription onto verified primitives. Split it further if it still
resists: the file-header read (the three raw `FileSystem::ReadFile` calls before the
stream exists), the metadata block (~25 inlined `Read`s plus the `String::SetLength`
traffic for name/author/description), and the entry directory (the `Filename` array and
three `AdjustDelimiters` calls).

Oracle: `ReadPackfile` is **not** replayable in place — it mutates the `PackageFile` and
allocates. But it *is* replayable onto a **second, freshly constructed `PackageFile`**,
because lane PKG2 ported the constructor and proved it byte-exact. Construct two, run ours
into one and the DLL's into the other, and compare all 0xF0 bytes plus the entry table
they point at. **That oracle did not exist before this lane and exists now.**

### Then, and only then — the three address doors behind it

`0x1800264a0` (the packfile introsort), `0x180025a80` (the path→entry BST insert) and
`0x18007eefc` (`_stricmp`) become reachable the moment `ReadPackfile` lands. All three sit
*after* it in program order. Cautions already on the record: the BST insert needs the AVL
care lane Q gave `Array`, and `_stricmp` must **not** share a comparator with
`runtime/src/system/configuration.cpp` — `__ascii_stricmp` compares **unsigned**, the
config map's inlined `strcmp` compares **signed**, and they disagree on any byte ≥ 0x80.

`0x180008340` (the recursive tree free) is not reached on the first boot at all —
`ClosePackages` guards it on a root that is null until the second `LoadPackages`.
**Confirming which normal-play action triggers that second call is still open**; a mods
reload is the candidate lane PKG2 named but did not verify.

---

## 5. Why not just port it and see

Because the failure mode is silent. `ReadPackfile` produces the entry table that
`LoadPackages`' index pass turns into every resource lookup in the game. A parser that is
wrong by one field boots, mounts, and serves subtly wrong offsets — and the diff oracle
for the *parser* cannot tell you the *decompressor* under it dropped a byte. Verifying
bottom-up means each rung is proved by an oracle that does not depend on the rung above
it, which is the only way a 1,864-instruction body at the top ends up trustworthy.

---

## 6. A note on the corpus, for whoever takes rung 4

Lane PKG2's accessor sweep measured `?GetVersion@` and `?GetPlatform@` returning **one
distinct value across all four mounted packages**. Four identical values is not a
verification (`reference_uniform_population_is_not_a_verification.md`).

The fix is not more `.pkg` files, it is a `.pkm`. A mod package is built by a different
tool at a different time and should carry a different version, a non-default `+0x70`
priority, a different `+0xac` overridable flag and non-empty name/author/description —
exactly the fields that are uniform today.

**But note what does NOT achieve that.** Copying `.pkm` files into `WinData/` changes
nothing on its own: `FindFiles`' pattern is the literal `*.pkg` (five bytes read out of
`.rdata` at `0x1801556f0`), so `LoadPackages`' LOOP 1 never matches them. A `.pkm` reaches
`ReadPackfile` only through LOOP 2, from `userPackFilenames` (+0x30), which the mods
system populates from the registry. The mods must be **enabled**, not merely present.

And the related correction, because it is easy to assume otherwise: a `.pkm` is **not a
different code path** through `ReadPackfile`. Both `LoadPackages` loops call the same
body; delta-packing is a property of the content, not a second parser. The argument for
mounting a mod is a *population* argument, and it is a good one — it is not a coverage
argument.
