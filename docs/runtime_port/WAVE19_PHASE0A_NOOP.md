# WAVE 19 phase 0a — the abstract-base NO-OP question, settled from the shipped bytes

Lane **NOOP**. Worktree `C:/code/GIT/wt-w19-noop`, branch `wave19/noop`, cut from
`wave19/base` @ `ebbec6e`.

Subject: `??_7ReaderWriter@IO@@6B@` (`0x1801553b0`) and `??_7Stream@IO@@6B@`
(`0x1801549a0`), the two rows Wave 17 lane CEIL **deliberately refused to tag**
because the NO-OP claim rested on a comment in `runtime/src/io/encoding.cpp`
rather than on a reading of the binary.

**Nothing in this document is inherited.** Every figure below was measured on
`C:/GOG Games/Banished/Runtime-x64.dll` for this document, with `objdump` and
`pefile`. The Ghidra MCP bridge was down and was not used. Where an inherited
figure disagrees with a measured one, §5 says so and does not paper over it.

---

## 0. Verdicts

| row | verdict | the denominator behind it |
|---|---|---|
| `??_7ReaderWriter@IO@@6B@` | **NO-OP (removable)** | 30 purecall **of 36** slots · 20 of 20 stamps accounted · 17 stamp-windows, **0** indirect calls of any kind in them |
| `??_7Stream@IO@@6B@` | **NO-OP (removable)** | 5 purecall **of 7** slots · 56 of 56 stamps accounted · 58 stamp-windows, **0** dispatches through the stamped object (see §3.3 for the one window that needed hand-resolution) |

`encoding.cpp`'s comment is **correct**, and it is now backed by the bytes rather
than standing on its own.

---

## 1. Is each table genuinely abstract? — from the shipped bytes

`docs/runtime_port/wave19_phase0a/slots.py` reads the `.rdata` qwords directly and
finds each table's extent from the **next exported symbol** (ReaderWriter) or from
a **null terminator** (Stream), not from an assumed length.

```
??_7ReaderWriter@IO@@6B@  @0x1801553b0
  [ 0]         0x180023bd0
  [ 1..30]     0x18007ec28   <- 30 identical slots
  [31..35]     0x1800a81e0 / 81d0 / 81c0 / 8120 / 8080
  [36]         next export: ??_7BinaryFileStream@IO@@6B@   -> the table is 36 slots

??_7Stream@IO@@6B@  @0x1801549a0
  [ 0]         0x18000c610
  [ 1]         0x18000c6a0
  [ 2..6]      0x18007ec28   <- 5 identical slots
  [ 7]         0x0           -> the table is 7 slots
```

`0x18007ec28` is `_purecall`, identified from its body and not from a name (the DLL
exports no `purecall` symbol):

```
18007ec28:  sub $0x28,%rsp
18007ec2c:  mov 0x17cdb5(%rip),%rcx        # 0x1801fb9e8   (the purecall handler)
18007ec33:  call *0xcc7ff(%rip)            # 0x18014b438
18007ec39:  test %rax,%rax ; je .. ; call *%rax
18007ec40:  mov $0x19,%ecx                 # 25 == CRT R6025 "pure virtual function call"
18007ec45:  call 0x1800805a8               # _amsg_exit
```

* `??_7ReaderWriter@IO@@6B@` — **30 of 36 purecall.**
* `??_7Stream@IO@@6B@` — **5 of 7 purecall.**

Both inherited figures **reproduce exactly**. Independent corroboration from a
different producer: `standalone/rt_data_mint_tables.inc` carries both rows at
**36** and **7** slots (lines 1489 / 1493), harvested by `tools/rt_mint_gen.py`,
which never saw this script.

Both classes are therefore abstract: **no leaf instance of either can exist**, and
either table can only ever be the live `+0x00` in a window between a base
constructor and a derived one, or between a derived destructor and the release of
the storage.

---

## 2. Every reference to each VA, by two routes that could disagree

### Route A — decoder (objdump)
`objdump -d` over the whole DLL, grep for the target VA in the RIP-relative comment
column (this also catches a `movabs` immediate, since the immediate is printed as
text).

### Route B — decoder-independent
`docs/runtime_port/wave19_phase0a/routeb.py`. For **every** byte offset `i` in
`.text` it treats `bytes[i:i+4]` as a signed `disp32` and asks whether
`VA(i)+4+disp32 == target`. It assumes nothing about instruction boundaries or
opcodes, so it is a strict **superset** of any RIP-relative reference a decoder can
find. Denominator printed by the tool: **1,349,117 four-byte windows**.

| row | Route A | Route B | agree? |
|---|---|---|---|
| `??_7ReaderWriter@IO@@6B@` | 20 | 20 | **yes**, and every Route-B offset is exactly `Route-A instruction address + 3`, the length of the `48 8d 05` LEA prefix |
| `??_7Stream@IO@@6B@` | 56 | 56 | **yes**, same offset relation |

**Positive control** (must read non-zero, and did): the same Route-B scan for
`??_7MemoryStream@IO@@6B@` reads **16**.

Every one of the 76 references is a `LEA reg,[rip+disp]`. There is **no** other
reference kind.

### Route B2 — is either VA held as a value anywhere?
Whole-image qword and dword scan of **all four shipped binaries**
(`docs/runtime_port/wave19_phase0a/cross.py`):

| image | imports of either symbol | qword windows scanned | hits |
|---|---|---|---|
| Runtime-x64.dll | none (of 189 imports) | 2,761,686 | none |
| Application-x64.exe | none (of 899) | 1,948,630 | none |
| VideoDX11-x64.dll | none (of 163) | 128,982 | none |
| VideoDX9-x64.dll | none (of 146) | 121,302 | none |

**Positive control**: `0x18000c610` (slot 0 of `??_7Stream@`), a qword the DLL
demonstrably stores, is found **exactly once**, in Runtime's `.rdata`, at the
table's own slot 0 — so the scan is live and its zeros are findings.

---

## 3. Is every stamp transiently overwritten, and does anything dispatch in the window?

`docs/runtime_port/wave19_phase0a/sites.py` groups the stamps by **`.pdata`
function** (authoritative for x64 PE, not an int3-padding heuristic) and lists, in
order, every stamp of any exported `??_7` table and every call.
`window.py` then computes, for each subject stamp, the window from the stamp to
the next stamp of the **same destination operand**, or to the end of the function,
and enumerates the calls inside it.

**This screen was wrong once and the first version is not quietly buried.** v1
invalidated a tracked register only on a reg-to-reg `MOV`, so `mov 0x10(%rdx),%rax`
left `%rax` still believed to hold a vtable and the screen **invented** two extra
stamps inside `??0BinaryReader`. v2 invalidates the destination register of every
instruction that writes one and clobbers the volatile registers across a call;
it therefore under-reports rather than over-reports, and the residual is
reconciled in §3.4.

**Positive control for the window screen** (must read non-zero, printed first, and
did): the identical screen run over `??_7BinaryReader@IO@@6B@`, a **concrete leaf**
table which is installed for the whole life of the object — **5 windows, 7
register-indirect calls**. A leaf table's windows must contain dispatches; they do.

### 3.1 `??_7ReaderWriter@IO@@6B@` — 17 windows, ZERO indirect calls of any kind

All 20 stamps live in exactly these places, and nowhere else:

* the three ReaderWriter bodies themselves — `??0ReaderWriter@IO@@QEAA@AEBV01@@Z`
  (`0x180023c50`), `??0ReaderWriter@IO@@QEAA@AEAVSerializer@1@_N@Z` (`0x1800b9120`),
  `??1ReaderWriter@IO@@UEAA@XZ` (`0x1800b9150`). Each is a leaf with no `.pdata`
  entry, and each is *stamp, a few plain field stores, `ret`* — read them in full,
  there is nothing else in them;
* the constructors of the **three** derived classes — `BinaryReader`, `BinaryWriter`,
  `Visitor` — each of which stamps ReaderWriter and then overwrites it with its own
  table **with no instruction between the two stores that can call anything**;
* the destructors and scalar-deleting destructors of the same three, each of which
  stamps ReaderWriter **last** and is then followed either by nothing at all or by
  `call 0x180006a10` (the deallocator).

The 20 sites are the **complete** universe of stampers, so the derived set is
closed at those three classes. Two supporting scans (`callers.py`, positive control
`0x180006a10` = 354 call sites, so the scan is live):

* `??0ReaderWriter` — both overloads have **zero** call sites. MSVC inlined the base
  constructor everywhere, which is exactly why the LEAs appear inline above.
* `??1ReaderWriter` — **15** references, **all 15 are tail `JMP`s** from thunks with
  no `.pdata` entry, i.e. SEH cleanup funclets destroying the sub-object during
  unwinding. Zero ordinary `CALL`s.

Window screen result: **17 windows, 0 indirect calls**. Not "no vtable dispatch
found" — **no indirect call of any kind**, IAT included.

### 3.2 `??_7Stream@IO@@6B@` — 58 windows

Same shape, over a much larger derived set (`FileStream`, `MemoryStream`,
`CompressedStream`, `BinaryFileStream`, `TextFileStream`, `VisitorStream`,
`CompressedMemoryStream`, and the deleting destructors of each). The three Stream
bodies themselves (`??0Stream@IO@@QEAA@XZ` `0x1800b99d0`,
`??0Stream@IO@@QEAA@AEBV01@@Z` `0x18000c6d0`, `??1Stream@IO@@UEAA@XZ`
`0x1800b9a40`) are again leaf functions: stamp, plain stores or one
`jne 0x180006a10`, `ret`. `??0Stream` has **zero** callers; `??1Stream` has **56**,
**all 56 tail `JMP`s from SEH cleanup funclets**, zero ordinary `CALL`s.

The window screen lists **76 indirect calls** across all windows. Split by kind:

| kind | count | can it dispatch through the stamped object? |
|---|---|---|
| `call *disp(%rip)` — IAT import (critical section, logging) | **58** | no — the target is an import slot, not the object |
| register-indirect `call *disp(%reg)` | **18** | candidate |

Of the 18 register-indirect calls, **7 belong to the `BinaryReader` positive
control**. All **11** remaining sit in **one** window, in
`?OnOpened@DialogInstance@UI@@UEAAXXZ` — §3.3.

A virtual dispatch can also be a tail `JMP`. Separate scan, control = **338**
register-indirect tail `JMP`s in `.text` overall: **0** inside any of the 17
ReaderWriter-stamping or 47 Stream-stamping functions.

### 3.3 The one window that needed resolving by hand

```
180115125:  mov  %r15,-0x38(%rbp)      ; r15 = ??_7Stream@IO@@6B@   <- the stamp
180115129:  mov  -0x28(%rbp),%rcx
180115130:  je   0x18011530b
180115136:  call 0x180006a10           ; free
18011513b:  jmp  0x18011530b           <-- UNCONDITIONAL, leaves the region
180115140:  ...                        ; the 11 register-indirect calls live here
180115279:  mov  %r13,-0x38(%rbp)      ; r13 = ??_7MemoryStream@IO@@6B@
```

The window screen walks **linearly**, not along the control-flow graph, so it
paired the stamp at `0x180115125` with the next same-destination stamp at
`0x180115279` and swept up the 11 dispatches in between. The `jmp` at
`0x18011513b` is unconditional: **no execution path reaches `0x180115140` from the
stamp.** `0x180115125` is the tail of an inlined `~MemoryStream` on the cleanup
path — it stamps the base table and frees.

That is a limitation of the instrument, stated rather than hidden: see §6.

### 3.4 The self-check that did not balance, and why

`sites.py` prints `subject stamps emitted` against `LEA sites`:

* ReaderWriter — 17 emitted vs 20 LEAs. **Reconciled:** 3 sites lie in leaf
  functions with **no `.pdata` entry** (a leaf that allocates no stack frame needs
  no `RUNTIME_FUNCTION`), which the grouping skips. 17 + 3 = 20. Those three are
  the ReaderWriter ctor/ctor/dtor of §3.1 and were read by hand instead.
* Stream — 58 emitted vs 56 LEAs (53 in-`.pdata` + 3 leaf). **Reconciled:** one LEA
  can feed more than one store; `?Run@Stream@Audio@@`, `?BuildPackageFile@`,
  `?AreResourcesEqual@` and `?OnOpened@` each keep the table in a register across
  two stores. The count of *stamps* legitimately exceeds the count of *LEAs*.

---

## 4. Does a leaf ever survive with the table installed?

No, and the argument has three independent legs:

1. **C++'s own.** 30-of-36 and 5-of-7 purecall means neither class can be
   instantiated. Only a base sub-object can carry the table, and only inside a
   construction or destruction window.
2. **Every window is empty.** ReaderWriter: 17 windows, zero indirect calls at all.
   Stream: 58 windows, zero dispatches through the stamped object once IAT calls
   and the one unreachable window are removed. Zero register-indirect tail `JMP`s
   in either set. So even inside the window nothing dispatches.
3. **Nothing outside a stamp can install the table.** All 76 references are
   in-DLL RIP-relative `LEA`s; no shipped image imports either symbol; no qword or
   dword in any of the four images holds either VA; the base destructors are
   reached only by SEH cleanup funclets.

**Our own tree stamps both symbols through real doors, and mirrors retail at every
one of them** — this is the half that actually decides whether *promotion* could
change behaviour, since an in-DLL `LEA` is not redirectable but a
`ResolveExport` / `RT_DATA_SLOT` / `RT_VTABLE_ONCE` door is:

| our site | shape |
|---|---|
| `runtime/src/io/encoding.cpp:233,238` | stamp ReaderWriter, then `VtblBinaryReader()` — nothing between |
| `runtime/src/io/encoding.cpp:259,264` | stamp ReaderWriter, then `VtblBinaryWriter()` — nothing between |
| `runtime/src/io/binary_writer.cpp` `RtIo_BinaryWriter_Dtor` | stamps ReaderWriter **last**; the deleting dtor then `MemFree`s |
| `runtime/src/ui/userinterface.cpp:1660-1664` | `st.base.vptr = vtStream;` then `= vtMem;` — nothing between |
| `runtime/src/object/external_add.cpp:366` | dtor tail, then free the encoding |
| `runtime/src/object/external_load.cpp:476` | dtor tail, then free the encoding |
| `runtime/src/object/external_savetext_win.cpp:373` | dtor tail, then free the encoding |
| `runtime/src/save/saveload_meta.cpp:611,649` | dtor tail, then `MemFree` |

Every one is stamp-then-overwrite or stamp-then-free. Promoting either row would
change **which table object is briefly written into a `+0x00` that nothing reads
before it is overwritten or released.**

---

## 5. Where a measured number disagrees with an inherited one

WAVE17_LANE_CEIL.md §2.5 records `??_7Stream@IO@@6B@` as **118 DATA references**
(Ghidra `getReferencesTo`). This lane measures **56**, by two independent routes
that agree exactly with each other, one of them decoder-independent.

Attempted reconciliation (`routeb.py` extended to the table interior):
references to any byte **inside** the table rather than only its head add
**1** for Stream and **8** for ReaderWriter — nowhere near the gap. 118 is close to
`2 x 56 + 6`, which *suggests* Ghidra counted each site twice (e.g. instruction
reference plus decompiler reference), but **this lane did not establish that** and
the Ghidra bridge was down, so the discrepancy is **left open**. It does not affect
the verdicts: the two counts differ in how many references exist, and every
verdict here rests on the *kind* of the references, which is uniform (`LEA`) under
both routes and is corroborated by the qword/import scan of all four images.

---

## 6. What this document does NOT claim

* **It does not claim the window screen is CFG-aware.** It walks the instruction
  stream linearly. The one window where that mattered (§3.3) was resolved by hand.
  For the other 57 Stream windows and all 17 ReaderWriter windows the question is
  moot because those windows contain **no** register-indirect call to be reachable
  or unreachable.
* **It does not claim `??_7Stream@IO@@6B@`'s two non-purecall slots are
  undispatchable in principle.** Slots 0 (`0x18000c610`) and 1 (`0x18000c6a0`) are
  real bodies. A dispatch of one of them on a Stream-carrying object would not
  crash the way a purecall dispatch would. This lane found **no such dispatch** in
  any of the 58 windows; it did not prove one cannot be written.
* **It does not re-derive the trap/ported counts** for either row. `??_7Stream@` was
  `7 / 1 ported / 1 TRAP` in CEIL and that figure is still **[INHERITED]** here —
  re-verifying it is cell **0c**'s job, and a NO-OP tag is independent of it.
* **It does not touch the other 34 IO rows**, nor cell 0b's five unminted vtables
  (both subject rows *are* minted, at `rt_data_mint_tables.inc:1489` and `:1493`).
* **It does not run the game.** No boot, no `BANISHED_RT_MINT_SHADOW` run, no
  attendance measurement. Everything here is static, on the shipped bytes and on
  our source. An attendance run would say how often these windows are *entered*;
  it would not change what happens inside them, which is what the verdict rests on.
* **It does not claim retail's `LEA` stamps are redirectable.** They are not. The
  promotion question is decided entirely by our own eight door-stamps (§4).
* **It does not re-measure IO's `called` or `ported`.** Both were read from
  `tools/runtime_census.py --namespace IO` run in **this** worktree
  (`122 called / 83 ported`), not quoted from the plan.

---

## 7. Should IO's ceiling move? — as the identity, never a percentage

Measured in this worktree at `ebbec6e`:

```
IO today:        ported >= called - unpromotable
                     83 >= 122   -  3            = 119      36 SHORT
```

The three current IO unpromotable rows are the `?_singleton@{FileSystem,
Modification, Persistence}@IO@@` HARMFUL rows (`reg_mint_promoted.cpp:981-983`).

Tagging both subject rows **NO-OP** adds 2 to `unpromotable`. Per
`runtime_census.py`'s own contract the tag **changes no count** — `total`, `called`,
`ported` and `detour` keep their exact arithmetic — so:

```
IO after this cell:  ported >= called - unpromotable
                         83 >= 122   -  5            = 117      34 SHORT
```

**The worklist drops from 36 rows to 34.** No percentage is quoted here on purpose:
`called` sits in both halves of the ceiling fraction, so the printed percentage
moves for reasons unrelated to progress (WAVE18_CLOSED §4).

Set-diff of what leaves the DATA partition of §1 of the plan:

```
- ??_7ReaderWriter@IO@@6B@
- ??_7Stream@IO@@6B@
```

Nothing enters `called` as a result of this cell — it ported nothing and opened no
door.

---

## 8. The tag, as landed

Two comment-only lines added to `standalone/reg_mint_promoted.cpp`, in the format
`runtime_census.py::load_unpromotable` parses. No code changes, no build impact.
If the integrator would rather hold the tag until cell 0c re-runs the readiness
screen, reverting those two lines is the whole of it and this document stands
unchanged.

## 9. Reproducing

`docs/runtime_port/wave19_phase0a/` holds every script and the two long outputs.
Each takes no arguments and reads `C:/GOG Games/Banished/Runtime-x64.dll` directly;
`sites.py` and `window.py` additionally want `rt.dis`, produced by

```
objdump -d "C:/GOG Games/Banished/Runtime-x64.dll" > rt.dis
```

Run them with the project venv interpreter
(`tools/ghidra_mcp/.venv/Scripts/python.exe`), which has `pefile`.
