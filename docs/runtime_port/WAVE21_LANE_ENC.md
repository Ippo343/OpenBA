# WAVE 21 LANE ENC — items 1, 3, 12

Branch: `wave21/enc`, cut from `wave21/base` (`5ce25be7`). Worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a91ee7081e1e2ddd8`.

All three items owned by this lane turned on the same question — "is a trap slot
still a trap?" — and all three were re-derived from the SHIPPED BYTES before
anything was written, per kickoff §0. Two tools were used to read the binary:
`tools/vtable_dump.py` (pefile against the export directory) and one-off pefile
scripts in the scratchpad that walked the raw instruction bytes by hand
(no capstone available in the venv). Ghidra's MCP bridge came up mid-session
(coordinator message) and was used as a **second, independent** check on the two
facts that mattered most (see "Ghidra corroboration" below) — it did not replace
the pefile reads, which were already complete and are the primary citations
below.

**Every binary read in this report is against `C:\GOG Games\Banished\Runtime-x64.dll`
(RETAIL), never the profile build.** The profile DLL was not opened for any of
this lane's three items — the encoding offsets the profile/retail trap
(`reference_encoding_vtable_offsets_differ_between_builds.md`) belong to a
*different* accessor (`IO::Serializer`'s bool/uint/int slots reached off
`InternalBasePtr::Serialize`) than the one this lane touched (the shared
Binary-Encoding vtable's slots 0/1/2), and none of this lane's three items
needed a profile-build read at all.

---

## Item 1 — the `0x1801554a8` encoding vtable, slots 0/1/2

**RE-DERIVES TRUE.**

`tools/vtable_dump.py 0x1801554a8 5` against retail confirmed the table's five
slots are exactly:

```
slot 0   0x1800a81e0  (unnamed)
slot 1   0x1800a81d0  (unnamed)
slot 2   0x1800a81c0  (unnamed)
slot 3   0x1800a8120  FUN_1800a8120   (already ported: RtIo_Encoding_CreateReader)
slot 4   0x1800a8080  FUN_1800a8080   (already ported: RtIo_Encoding_CreateWriter)
```

Slots 0/1/2 are the three addresses the kickoff named. Read directly out of
retail (`pefile.get_memory_mapped_image()`), all three are tiny leaf functions:

| VA | bytes | disassembly | meaning |
|---|---|---|---|
| `0x1800a81e0` | `B0 01 C3` | `MOV AL,1 ; RET` | returns `true` |
| `0x1800a81d0` | `32 C0 C3` | `XOR AL,AL ; RET` | returns `false` |
| `0x1800a81c0` | `B8 02 00 00 00 C3` | `MOV EAX,2 ; RET` | returns `2` |

None reads `self` (RCX untouched). The semantics ("IsBinary", "IsText",
"encoding kind") are **inferred, not attested** — no export name or debug
string names these three. The inference rests on a sibling table: the
unexported TEXT-encoding vtable at `0x1801559d0` has the identical shape at
the identical three slots, with 0/1 **exactly inverted** and slot 2 holding a
different small constant (`0x1801559d0`+0/1/2 → `false`, `true`, `1`). A
predicate pair that flips between the binary and text tables, plus a small
per-table constant, is the shape named in the port comment; it is called out
there as this lane's naming choice, not a fact from the binary.

**Ported** in `runtime/src/io/encoding.cpp` as `RtIo_Encoding_IsBinary`,
`RtIo_Encoding_IsText`, `RtIo_Encoding_GetKind`, registered by address
(`RegisterRuntimeAddr`, the only door — both are unexported, same as the two
factories already there). All five slots of `0x1801554a8` are now our bodies.

**Route, not just readiness.** This table is address-keyed (unexported), so it
can never enter the census's `promoted_vtables` bucket a `??_7` export can —
that is permanent, independent of this lane's work. But we own the actual
STAMPER: every concrete Stream constructor that installs this table
(`RtIo_CompressedMemoryStream_Ctor`, the `??0MemoryStream`/`??0CompressedStream`
port in `compressedstream.cpp`, `RtIo_TextFileStream_Ctor`) reads the pointer
through `ResolveAddr(Module::Runtime, 0x1801554a8ull)`, which is the
port-aware, registry-first door. So a live `IO::Encoding` object built after
cutover dispatches through OUR three new slots, not a trap — this was checked
by reading the call sites, not assumed from "COMPLETE."

Updated `runtime/src/io/compressedstream.cpp`'s Wave 9 banner (which said "none
is ported, so all five mint as NAMED TRAPS") with a dated correction rather
than silently editing the historical claim.

---

## Item 3 — `??_7TextFileStream@IO@@6B@` trap slots 0, 2, 8

**PARTIALLY RE-DERIVES TRUE, THEN CORRECTED.**

Kickoff said backlog printed `ours 7 / slots 10`. Confirmed with
`tools/runtime_census.py` at the start of the session: `??_7TextFileStream@IO@@6B@  7  10`.

**Slot 0's written expiry ("unblocks when `??1TextFileStream@` lands") was
checked and found FALSE at kickoff time** — grepped `runtime/`, `standalone/`,
`engine/`, `code/` for `??1TextFileStream@IO@@UEAA@XZ` / `RegisterRuntimeExport`:
zero hits. The dtor had not landed.

**The mangling question, settled from the binary, not a doc.** Read the export
directory of retail `Runtime-x64.dll` by VA (`pe.DIRECTORY_ENTRY_EXPORT`,
matched on `s.address == 0x1800ba490 - image_base`): the export at
`0x1800ba490` is `??1TextFileStream@IO@@UEAA@XZ` — **`UEAA`, virtual, exactly
as `runtime/types/io/FileStream.h:31` already stated** ("`??1TextFileStream /
??1VisitorStream — all UEAA, all exported`"). Not `QEAA`. Corroborated
independently via the Ghidra MCP bridge once it came up mid-session
(`get_function_by_address(0x1800ba490)` on `Runtime-x64.dll` → `~TextFileStream`,
`__thiscall void(TextFileStream*)`, matching a non-static virtual member with
no flags argument — the `??1` shape, not `??_G`/`??_E`).

**But the slot 0 expiry text was itself imprecise, and re-deriving it changed
the plan.** `runtime/src/io/text_file_stream_win.cpp`'s own DEFERRED section
already said the deleting dtor at slot 0 (`0x18002ba90`, unexported) needs
**two** bodies to reach usefully: `??1TextFileStream@` AND the CRT's
`eh vector destructor iterator` (`0x18007ec98`). Both of those addresses are
reached through `ResolveAddr`, which is registry-first — so **neither actually
needs to be OUR port** for the deleting-dtor thunk itself to be portable; the
existing `runtime/src/object/deleting_dtor.cpp` family (41 rows before this
lane) already proves that pattern for `UI::ToolTipDialog` and
`IO::CompressedFileStream`, both of which register the slot-0 thunk while
their `~Class` callee is still the DLL's. **The stated expiry condition was
the wrong shape** — it should have said "ported when added as a row in
deleting_dtor.cpp," which needs no callee port at all, not "when `??1
TextFileStream@` is ported."

**Disassembled `0x18002ba90` from retail bytes** (walked by hand in a
scratchpad script, instruction lengths verified against the two independently
recoverable constants — the LEA target and the CALL targets — both of which
landed on already-known addresses):

```
4C 8D 0D E1 E9 08 00   LEA R9,[rip+0x8e9e1]   -> 0x1800ba490  (~TextFileStream)
BA 80 00 00 00         MOV EDX,0x80           sizeof(TextFileStream)
E8 DA 31 05 00         CALL rel32             -> 0x18007ec98  (eh vector dtor iterator)
E8 43 AF FD FF         CALL rel32             -> 0x180006a10  (operator delete, a
                                                  known member of the family's
                                                  10-copy MemFree set)
```

Same shape as every other row in `runtime/src/object/deleting_dtor.cpp`
(the vector-deleting-destructor family, `reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`'s
"third dtor trap"). **Ported** it as row 41 ("IO::TextFileStream"), verified
against the independent re-derivation tool `tools/rt_deleting_dtor_verify.py`
(re-reads the DLL and checks the table; result: `OK size=0x80 dtor=0x1800ba490
IO::TextFileStream`, `problems: 0`). Registered by address (only door; unnamed
in the export directory). `??1TextFileStream@` itself remains unported — this
row makes that address newly `called` (a documented, accepted growth of the
cutover surface, same as the ToolTipDialog/CompressedFileStream precedent).

Slots 2 (`Write`, 462 bytes, a five-way transcoder branch) and 8 (`Open`, 666
bytes, the largest body in the table, installs the buffer four other bodies
branch on) were **re-checked and left exactly as the existing DEFERRED note
says: NOT ATTEMPTED, a size judgement with no expiry, not a blocker.** Neither
was touched this lane — porting either correctly needs its own dedicated pass,
not a slot-count sweep.

**Result:** `??_7TextFileStream@IO@@6B@` backlog moved from `7/10` to `8/10`
(re-confirmed with `tools/runtime_census.py` after the edit). Still not
COMPLETE, still not promotable; two large bodies remain, honestly reported as
NOT ATTEMPTED rather than blocked.

---

## Item 12 — `??_7ReaderWriter@IO@@6B@`'s `?` numerator

**ADDRESSEE DISCHARGED.**

`tools/vtable_dump.py 0x1801553b0 26` against retail (this is `ReaderWriter`'s
vtable — confirmed from `runtime/src/io/encoding.cpp`'s own comment ordering
against `VtblReaderWriter()`/`VtblBinaryReader()`/`VtblBinaryWriter()`, NOT
assumed from position) showed **25 of 26 slots are `_purecall`, and slot 0 is
one concrete function, `0x180023bd0` (unnamed in the export table)** — matching
the existing tag's "25 purecall [RE-DERIVED]" half exactly, and pinning the
real candidate denominator at **1**, not 26.

Grepped the whole tree for `0x180023bd0` and for `??1ReaderWriter@IO@@` /
`RegisterRuntimeExport`: zero hits before this lane. **The answer was `0`.**

Disassembled `0x180023bd0` from retail (same family shape, verified with the
same script method as item 3):

```
4C 8D 0D 61 55 09 00   LEA R9,[rip+0x95561]   -> 0x1800b9150  (~ReaderWriter)
BA 20 00 00 00         MOV EDX,0x20           sizeof(ReaderWriter)
CALL -> 0x18007ec98 (shared iterator), CALL -> 0x180006a10 (shared operator delete)
```

`0x1800b9150` resolves in retail's export directory (checked by VA) to
`??1ReaderWriter@IO@@UEAA@XZ` — **`UEAA`, virtual**, matching the same
mangling caution as item 3 and confirmed a second way through the Ghidra MCP
bridge (`get_function_by_address` on `Runtime-x64.dll` → `~ReaderWriter`,
`__thiscall void(ReaderWriter*)`). `sizeof(ReaderWriter) == 0x20` matches
`runtime/types/io/Serializer.h`'s `struct ReaderWriter` (4 pointer-sized
fields, no gap).

**Ported** as `deleting_dtor.cpp` row 42 ("IO::ReaderWriter"), verified clean
by `tools/rt_deleting_dtor_verify.py`. `~ReaderWriter`'s body is a pure vtable
restamp (`LEA RAX,[??_7ReaderWriter@IO@@6B@]; MOV [RCX],RAX; RET` — confirmed
by reading its bytes: `48 8D 05 59 C2 09 00 / 48 89 01 / C3`), so it was also
added to `kProbeSubjects` (safe to run on a fabricated object, same class as
the `Object::Base` / `Video::*Data` subjects already listed).

**Replaced the `?` at `standalone/reg_mint_promoted.cpp:1202`** with `1`:

```
RT_UNPROMOTABLE: ??_7ReaderWriter@IO@@6B@ | 1/26 ported [RE-DERIVED, WAVE21_LANE_ENC.md item 12 ...
```

**This does NOT change the promotion verdict.** The row's `NONE, EVER` tag
rests on an independent, already-measured fact (20 in-DLL RIP-relative-LEA
stampers, zero calls through the stamped object across every stamp window —
`reg_mint_promoted.cpp`'s own §"THE STAMPER CENSUS"). Filling the one real
slot moves the census's truthful count from an honest `?` to an honest `1`;
it does not and should not move the ceiling.

Re-confirmed with `tools/runtime_census.py`: `??_7ReaderWriter@IO@@6B@` now
prints `1  1  COMPLETE` in the backlog table (COMPLETE here means "every
non-purecall slot is ours," the STATIC half of readiness only — the row stays
tagged unpromotable in `reg_mint_promoted.cpp` for the reason above).

---

## Why the top-line IO 216/200 count did not move

Re-ran `tools/runtime_census.py`'s namespace table after all three items: IO is
still `216 called / 200 ported`, TOTAL still `1311/1124`. This is expected, not
a sign the work is invisible or wrong: the three new bodies
(`RtIo_Encoding_IsBinary/IsText/GetKind`, and the two new
`deleting_dtor.cpp` rows) are all **unnamed, address-only** registrations —
exactly like the other ~40 rows already in that family, none of which moved
IO's top-line count either (Wave 9's finding, restated here because this lane
hit it directly: "porting twelve `??1` moved the filled-slot count by exactly
zero" is the same shape as "porting three unnamed leaf functions moves the
vtable BACKLOG count, not the namespace CALLED/PORTED count"). The backlog
counters (`ours N / slots M` per `??_7...` row) are the correct place to look
for this lane's movement, and both moved as reported above.

---

## Build and verification

- `tools/rt_deleting_dtor_verify.py` (independent re-derivation from the DLL,
  no build needed): **0 problems**, 43/43 rows OK including both new ones.
- `tools/vtable_dump.py` against retail: used for items 1 and 12 to confirm
  slot counts and purecall boundaries before writing any code.
- `tools/runtime_census.py`: re-run before and after every edit; all three
  backlog numbers reported above are before/after pairs from this tool.
- `mingw32-make standalone-debug` was run in this worktree to confirm the
  new code compiles cleanly (see the build log in this branch's session; if a
  compile error surfaced after this report was written, it is not reflected
  here — check the branch's latest commit before treating this section as
  final).
- **NOT run**: `mingw32-make sa-run` (a live boot). None of this lane's three
  rows are on any path exercised by `standalone/autostart.cpp`'s menu → New
  Game drive — `IO::Encoding`'s three new slots dispatch only through save/load
  Serializer construction, and the two new deleting-dtor rows dispatch only
  when something actually destroys a `TextFileStream` or a `ReaderWriter`
  through its vtable, which (per the family's standing note) does not happen
  before cutover: nothing in this tree calls a deleting destructor today, so
  `calls=0` on both new rows is the FAMILY'S EXPECTED reading, not a defect.
  Did not attempt to construct a synthetic caller beyond what
  `rt_deleting_dtor_verify.py` and the existing `deleting_dtor_probe.cpp`
  machinery already cover (the new IO::ReaderWriter row was added to
  `kProbeSubjects`, so `BANISHED_RT_DELDTOR_PROBE=1` will exercise it on next
  run; that run was not performed as part of this report).
- **NOT checked**: whether any OTHER lane's Graphics/vtable work this wave
  touches `runtime/src/object/deleting_dtor.cpp` or
  `standalone/reg_mint_promoted.cpp` concurrently — both are shared files with
  append-only conventions; a merge conflict there is a rebase, not a logic
  bug, but was not proactively coordinated with other lanes.
- **NOT checked**: the `_encoding` reader created via `IO::Encoding::CreateReader`
  in a real save/load round trip after this lane's changes (i.e., no
  save-file A/B diff was run). The three new slot bodies are pure leaf
  functions with no state and were checked entirely by static byte comparison
  against retail; no dynamic oracle was written for them.

## What this lane did NOT check (explicit list)

1. Items 2, 4, 5, 6, 7, 9, 10, 11, 13 of Wave 20 §8 — out of scope for ENC,
   owned by other lanes (VFY, SHOT, CLASS) or deliberately unscoped this wave.
2. The Graphics namespace gap (§1 of the kickoff) — not this lane's subject.
3. Whether porting `??1TextFileStream@` or `??1ReaderWriter@` themselves
   (the real, non-deleting destructors) is worth doing — both remain on the
   DLL; this lane only closed the deleting-dtor THUNK that calls whichever one
   `ResolveAddr` currently hands back.
4. `?SetupFileSystem@Modification@` / PackageFile cluster (item 7) — untouched,
   per kickoff's explicit "deliberately unscoped."
5. A live `sa-run` boot with these changes present, and a diff of the resulting
   `standalone_debug.log` attendance counters for `[Encoding]` against a
   pre-lane baseline.
6. Whether any consumer of `0x1801554a8`'s slots 0/1/2 exists in the exe side
   (`Application-x64.exe`) — not checked; this lane only confirmed the Runtime
   DLL's own call sites (the Stream constructors) via source grep, not a
   full `get_xrefs_to` sweep in Ghidra.

## Denominators used in this report

- "IO 216 called / 200 ported": `tools/runtime_census.py`'s namespace table,
  unchanged before/after this lane (explained above).
- "backlog ours/slots" per `??_7...` row: `tools/runtime_census.py`'s
  mint-vtable backlog fold, counting non-purecall slots only (purecall slots
  have no body to port and are excluded from the denominator by the tool
  itself, not by this lane).
- "43 rows / 0 problems": `tools/rt_deleting_dtor_verify.py`'s own count,
  re-derived from the DLL each run, independent of the source file's own
  `kSpecCount` (the tool would fail if the two disagreed).
