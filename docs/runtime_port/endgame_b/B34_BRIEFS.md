# ENDGAME B.3.4 — hand-over briefs: block 3, STREAMS, PACKAGE+FS

Three self-contained briefs, written at the block-2 close by the owner of blocks 0-2. Each one
is executable by an agent with none of that context.

**`B34_RULES.md` is the rule sheet and is NOT restated here** — sections are cited. Read, in
this order: `CLAUDE.md`; `B34_RULES.md` §0 (out of scope), §7.1 (the gate), §7.2 (standing
rules); then your brief's own section below. `B33_RULES.md` §4/§5/§6/§7 and `B31_RULES.md`'s
hygiene block apply unchanged.

**Blocks 0, 1 and 2 are merged** (main `13c81f72`, `e410644f`, `1a2a48be`). What exists now:
`IO::Encoding` + `BinaryEncoding`/`TextEncoding`/`VisitorEncoding`, and `IO::ReaderWriter` +
`BinaryReader`/`BinaryWriter`/`Visitor`, all in `runtime/types/io/Serializer.h`.

---

## §A. What blocks 0-2 learned that the rule sheet does not say

Read this whole section whichever brief you take. Every item cost something to find.

### A.1 The delegation pattern, and when the qualified call is load-bearing

Blocks 1-2 established the shape: **the body moves into the class method; the registered door
keeps its address and delegates.** For a POLYMORPHIC receiver the delegation MUST be a
qualified, non-virtual call:

```cpp
reinterpret_cast<IO::BinaryReader*>(self)->IO::BinaryReader::SerializeInt(*value, name);
```

**Why the `Class::` is not decoration.** A live `BinaryReader` in this build wears the MINTED
table (0x1801552e0), whose slots resolve *back to these very doors*. A plain virtual call in a
door would dispatch through the minted table into itself — infinite recursion, not delegation.
The qualification suppresses virtual dispatch. **This applies to every polymorphic IO class and
therefore to all of STREAMS and PACKAGE+FS.** It does *not* apply to `IO::Serializer`, which
has no vtable at all (§B.2).

### A.2 A member function of `IO::X` cannot be defined inside `namespace RtIo`

C++ requires a member definition to appear in a namespace *enclosing* the class. `IO` and
`RtIo` are siblings, so `void IO::BinaryReader::Foo() {}` written inside `namespace RtIo {}` is
ill-formed. The layout that works, and that `serialize_family.cpp` and `encoding.cpp` now use:

1. `namespace RtIo { ... helpers in an anonymous namespace ... doors ... }` — unchanged;
2. **after** `} // namespace RtIo`, a `namespace IO { ... }` block holding the method bodies;
3. those bodies reach the helpers as `RtIo::StreamMove`, `RtIo::VSlot`,
   `RtIo::SerializeArray<T>`, `RtIo::kStreamRead`. **Anonymous-namespace members ARE reachable
   as `RtIo::X` from elsewhere in the same translation unit** — that is what makes this work
   without moving or re-exporting anything.

### A.3 A class held BY VALUE as a member needs a PUBLIC non-virtual destructor

ENDGAME decision 2 gives a *base* a protected non-virtual destructor. But
`IO::CompressedMemoryStream` holds an `IO::MemoryStream` **by value** at +0x68, and the
enclosing class's implicit destructor must be able to destroy it — a protected one makes the
enclosing class uninstantiable. So **`IO::MemoryStream`'s destructor is public, still not
virtual.** Same for `IO::BinaryFileStream` inside `IO::CompressedFileStream` (+0x68) and
whatever `CompressedCopyFileStream` (0xc8) aggregates. This is a STREAMS item; it is recorded
in `io_vtable_abi_test.cpp` and was found by the test, not by the build.

### A.4 `final` + abstract is legal, deliberate, and how block 2 kept slot 0 honest

`IO::BinaryReader`/`BinaryWriter`/`Visitor` are declared `final` **and are still abstract**:
slot 0 (`DeletingDtor`) is left `override = 0` because its body lives in `iobase_vtables.cpp`,
registered by address, and §4.2 forbids retiring any of the fourteen IO deleting-dtor
registrations in this step. Abstract costs nothing because **nothing constructs these as C++
objects** — every instance is `MemAlloc`'d by an Encoding factory and stamped with the minted
table, and the classes are only ever reached through `reinterpret_cast` + a qualified call,
which needs no instantiation. `sizeof` works on an abstract class, so the layout asserts hold.
**Expect to use this same shape in STREAMS**: declare, pin, leave slot 0 pure, do not
instantiate.

### A.5 Verify the layout with a throwaway probe BEFORE wiring bodies

Block 2 compiled a standalone file that did nothing but `static_assert` every `offsetof` and
print every `sizeof` for the four new classes, and ran it, before a single one of the 73 bodies
was attached. That is ten minutes; finding the same mistake through a resave DIFF is an hour
and a false bisect. The one that mattered: **`IO::Visitor`'s `version` is at +0x20, NOT +0x28**
(`0x18002b670` is `MOV EAX,[RCX+0x20]`, where both codecs' are `+0x28`) — three identically
named `GetVersion` bodies where one reads a different offset is exactly the shape someone
"tidies" into uniformity.

### A.6 Tooling friction you will otherwise rediscover

- **`rt_mint_gen.py --check` REWRITES `standalone/rt_export_addrs.inc`** with different line
  endings and no content change, every run. It is outside your file set: `git checkout --` it
  before committing. (`--check` still correctly reports UP TO DATE.)
- **Copying the resave golden dirties a tracked file.** `docs/runtime_port/resave_gate/golden/
  Gate.resaved.md5` differs from the main checkout's by line endings (33 vs 34 bytes). Copy it
  for the run, then `git checkout --` it before committing.
- **Redirect build logs OUTSIDE the scratchpad.** The session scratchpad path contains
  `C--code-GIT-...`, and a shell command naming it is refused by the worktree-isolation check
  as "too complex to verify". Use `C:/Users/USPC/AppData/Local/Temp/claude/<name>.log`.
- **Write commit messages to a file and use `git commit -F <file>`.** A heredoc containing
  backslashes, `§`, or mangled names gets refused by the same check. Avoid `§` and `->` inside
  commit message bodies for the same reason; spell them "section" and "to".
- **Read each build's exit code from the command itself** (`make ... > log 2>&1; echo $?`),
  never through a pipe — a pipe reports the pipe's status.

### A.7 The `namespace IO` grep hazard is now ARMED

Before block 1, nothing in the tree opened `namespace IO`. Now `Serializer.h`, `encoding.cpp`
and `encoding_visitor_text.cpp` do. §7.2's rule is therefore live: **inside `namespace IO`,
always spell the POD `RtIo::X` / `rt::io::X` / `RtIO::X`.** A bare `Stream`, `Serializer`,
`Encoding`, `ReaderWriter` or `FileStream` inside such a block now binds to the REAL CLASS,
silently. And remember there are **two** `Stream` PODs (§1.5.2) — grep both names before
calling a `Stream` caller converted.

### A.8 Includes, and the one that bites

Includes go at **global scope, above any `namespace X {` forward declaration, and never inside
an `#if`** (B31 hygiene). Block 3 hits this directly: `runtime/types/object/Internal.h` must
include `runtime/types/io/Serializer.h` at global scope, above `namespace RtObject {`.

---

## §B. BRIEF 1 — BLOCK 3: `IO::Serializer`, and `SerializerLite` dies

Design: `B34_RULES.md` §1.2 rule 2, §3.1, §3.2 move 3, §3.3, §7.3. **Serial, one owner. Nothing
fans out until this merges** — STREAMS and PACKAGE+FS both depend on it.

### B.1 File set

```
runtime/types/io/Serializer.h                             (the class)
runtime/src/io/serializer.h
runtime/src/io/serializer.cpp                             (22 doors -> 22 members)
runtime/types/object/Internal.h                           (SerializerLite dies)
runtime/src/object/internal.cpp
runtime/src/object/internal_class.cpp
runtime/src/object/internal_ptr_array_serialize.cpp
runtime/src/object/external_class_doors.cpp               (comments only)
tests/unit/io_vtable_abi_test.cpp                         (two new tests, B.6/B.7)
docs/runtime_port/endgame_b/B34_RULES.md                  (only if you learn something)
```

Do not touch `iobase_vtables.cpp`, `serialize_family.cpp`, `encoding*.cpp`, `deleting_dtor*`,
`rt_data_mint*`, or any `standalone/reg_*.cpp`.

### B.2 The class shape — and it is NOT polymorphic

```cpp
namespace IO {
class Stream;            // ADD THIS forward declaration — see B.4
class Serializer {
public:
    Serializer(Stream& stream, int version);   // 0x1800b9960
    ~Serializer();                             // 0x1800b99b0 — NON-virtual, PUBLIC
    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    void Serialize(bool& v, const char* name);          // 0x1800061e0
    void Serialize(int32_t& v, const char* name);       // 0x1800061d0
    // ... 22 members total, real C++ OVERLOADS on the name `Serialize`
    void SerializeBytes(int n, void* buf, const char* name);   // 0x1800346d0
    int  GetVersion() const;                            // 0x1800070b0
    const Encoding& GetEncoding();                      // 0x180006290

    Stream*       stream;        // +0x00
    ReaderWriter* readerWriter;  // +0x08
};
static_assert(sizeof(Serializer) == 0x10, "IO::Serializer is 16 bytes");
}
```

**`Serializer` has no vtable and must never get one.** All 22 exported members are `QEAA`/`QEBA`
(non-virtual) and tail-jump into the ReaderWriter's table. A vptr would make the class 0x18
bytes and every one of the 861 `g_fn_Serializer_*` caller sites would write through the wrong
offsets. `io_vtable_abi_test.cpp` already asserts `is_standard_layout` and both offsets; keep
those passing.

**Three consequences of being non-polymorphic, all of which simplify block 3:**

1. **The doors do NOT need a qualified call.** §A.1's recursion hazard does not exist here —
   there is no vtable to dispatch through, so `reinterpret_cast<IO::Serializer*>(self)->
   Serialize(*v, name)` is already a direct call. Say so in a comment at the first door, because
   a reader coming from blocks 1-2 will expect the `Class::` and wonder why it is missing.
2. **Copy must be deleted.** A copied `Serializer` would destroy the same `ReaderWriter` twice,
   and `~Serializer` is not idempotent (§7.3). Deleting copy is why the placement-new caller
   shape (§7.3) is safe.
3. **The 22 members KEEP their `rw_vtbl::k*` raw slot calls** (§3.1). Do not turn
   `RwSlot(rw, rw_vtbl::kScalarInt)` into `readerWriter->SerializeInt(...)`. Put the reason in a
   comment on the first one: the member call becomes correct only when nothing hands the
   Serializer a minted-table ReaderWriter any more, and §4.2 says that is the end of B.3, not
   this step. This is the "faithful partial over complete-looking deletion" rule.

`serializer.cpp`'s 22 doors are registered **BY NAME ONLY** (`export=23, addr=0`; the 23rd is
`RtIoSerializerRegister` itself). Keep all 23 registration lines exactly as they are — they are
literal `RegisterRuntimeExport("<mangled>", ...)` calls and the census cannot see them any other
way. They are inside one `extern "C" { ... }` block; the class methods go in a
`namespace IO { ... }` block after it, per §A.2.

### B.3 `VerifyForwarder()` — read it before you move anything

`serializer.cpp` has a `VerifyForwarder()` that cross-checks each forwarder's tail-jump
displacement at registration time, so a DLL whose slots differ disables the port instead of
silently desyncing the save stream. **Understand what it is checking before you restructure the
file around it.** If it becomes dead or vacuous after the conversion, say so explicitly in the
commit message rather than leaving it looking live.

### B.4 How `IO::Stream&` is spelled before block 4

`IO::Stream` is **not** a class yet — it lands in STREAMS. `Serializer.h` already
forward-declares `class ReaderWriter; class Serializer;` inside `namespace IO`; **add
`class Stream;` beside them.** Then:

- the ctor signature is the final retail one, `Serializer(Stream& stream, int version)`, and
  never changes again;
- the body bridges once: `reinterpret_cast<rt::io::Stream*>(&stream)` with a `// B.3.4 bridge`
  comment, because the ctor must read `stream->direction` and `stream->encoding`, which only
  the POD declares today;
- `stream` the MEMBER is declared `Stream*` and assigned through the same bridge.

**What block 4 (STREAMS) must then do:** when `IO::Stream` becomes a real class, delete those
bridge casts and nothing else. The signatures do not move. Block 1 did exactly this for
`IO::ReaderWriter` and block 2 removed the cast without touching a signature — the pattern
works, so use it rather than inventing a `void*` intermediate.

### B.5 `SerializerLite` dies IN THIS BLOCK — the order

16 hits across 5 files, all under `runtime/{src,types}/object/`, zero in `code/`, `standalone/`
or `engine/` (§3.3). In ONE commit:

1. `runtime/types/object/Internal.h` — add `#include "../io/Serializer.h"` at **global scope,
   above `namespace RtObject {`** (§A.8), then replace the struct with
   `using SerializerLite = IO::Serializer;` so the other four files still build.
2. `internal.cpp`, `internal_class.cpp`, `internal_ptr_array_serialize.cpp` — move the bodies
   onto `IO::Serializer`. `internal_class.cpp` holds a local `RtObject::SerializerLite ser;`
   that becomes a real `IO::Serializer` — **check its lifetime against §7.3 before converting
   it**; if the ctor/dtor positions are not provably identical, leave it and report.
3. `external_class_doors.cpp` — comments only; update the prose that describes the policy.
4. Delete the `using` alias and the three `kEncSerializeBool/Uint/Int` constants
   (`Internal.h:227-231`) **in the same commit as the last body that reads them**. Afterwards
   `grep -rn SerializerLite` and `grep -rn kEncSerialize` are both zero tree-wide (build dirs
   excluded).

`io_vtable_abi_test.cpp` currently `static_assert`s that the three `kEncSerialize*` constants
equal the three `rw_vtbl::kScalar*` ones. **That assert must be removed in the same commit**
that deletes the constants, or the unit build breaks. Replace it with an equivalent assert on
`rw_vtbl` alone so the offsets stay pinned.

`rw_vtbl` itself **STAYS** — 54 files across eleven `runtime/src/*` directories use it and their
classes are B.3 steps 5-8.

### B.6 Required new unit test 1 — placement-new lifetime

Per §7.3 and the coordinator's amendment. In `tests/unit/io_vtable_abi_test.cpp`, self-contained
(the real `IO::Serializer` may not link into `make unit`; a local mirror class with the same
shape is acceptable and is what the existing tests do — say which you used):

- an `alignas(8) uint8_t buf[24]` byte buffer, placement-constructed and explicitly destroyed,
  mirroring the caller shape exactly;
- a fake `Stream` whose `Read`/`Write`/`GetCurrentPos` slots COUNT bytes and calls;
- assert the **12-byte header** is consumed/emitted **exactly once** on construction and the
  **4-byte tail** read/patched **exactly once** on destruction;
- **both sides**: `direction == 0` (reader, `CreateReader`, 3x`Read(4)`) and `direction != 0`
  (writer, `CreateWriter`, 3x`Write(4)`);
- assert that constructing twice into the same buffer would consume two headers — i.e. make the
  double-construction hazard visible as a counted fact, not a comment.

### B.7 Required new unit test 2 — PIN THE REAL CLASSES' VTABLE ORDER

**Nothing exercises the real vtable order today**, and that is a latent trap: every door calls
qualified/non-virtually and every live object wears the minted table, so a mis-ordered
declaration in `Serializer.h` would stay invisible until B.3 steps 5-8 retire the minted tables
— at which point it surfaces as a corrupted save with no recent change to blame. The order was
checked by eye at the block-2 close and is right; this test is so it STAYS right.

Shape:

```cpp
// A CONCRETE subclass of the REAL IO::ReaderWriter, overriding all 26 with markers.
struct OrderProbe final : IO::ReaderWriter {
    IO::ReaderWriter* DeletingDtor(unsigned) override { Note(0); return this; }
    int  GetVersion() const override { Note(1); return 0; }
    void Skip() override { Note(2); }
    void SerializeCharArray(int, char*, const char*) override { Note(3); }
    ...                                                        // all 26
    void SerializeBytes(int, void*, const char*) override { Note(25); }
    ~OrderProbe() = default;
};
```

then for each `rw_vtbl::k*` constant, call `(*(void***)&probe)[k/8]` with a plausible signature
and assert the marker equals `k/8`. **Do the same for `IO::Encoding`'s five slots**
(`IsBinary`/`IsText`/`GetKind` at +0x00/+0x08/+0x10 and the two factories at
`encoding_vtbl::kCreateReader`/`kCreateWriter`), which also proves slot 0 is not a destructor.

Two notes so you do not fight the compiler:

- `IO::ReaderWriter`'s destructor is `protected`, so `OrderProbe` must be a subclass (it is) and
  must give itself a public destructor.
- `IO::BinaryReader`/`BinaryWriter`/`Visitor` are `final` **and abstract** (§A.4), so you cannot
  subclass or instantiate *them*. Probe the BASE, which is the thing whose order the 54
  `rw_vtbl::` files depend on.

### B.8 Gate for block 3

§7.1 in full. Because this is the Serializer, the resave is the whole safety net: **three
CONSECUTIVE PASSes**, and record the resaved byte count (baseline **21075114**, `tail=263`,
`diffBytes=15975558`) in the commit message. Read §3.4 on flakes before reading a DIFF, and use
the row-level diff in §E.

---

## §C. BRIEF 2 — STREAMS

Design: `B34_RULES.md` §1.3, §1.5.2, §3.2 move 4, §4.1, §4.3, §6.3. **Starts only after block 3
merges.** Runs in parallel with PACKAGE+FS; the two file sets are disjoint by construction.

### C.1 File set

```
runtime/types/io/Stream.h
runtime/types/io/FileStream.h
runtime/src/io/memory_stream.cpp
runtime/src/io/compressedstream.cpp
runtime/src/io/compressed_stream_open.cpp
runtime/src/io/compressed_stream_read.cpp
runtime/src/io/compressed_stream_release.cpp
runtime/src/io/compressed_stream_write.cpp
runtime/src/io/compressedcopyfilestream.cpp
runtime/src/io/file_stream.cpp
runtime/src/io/file_stream_win.cpp
runtime/src/io/text_file_stream_win.cpp
runtime/src/io/binary_writer.cpp
runtime/src/object/internal_class.cpp        (Internal::Clone only — C.5)
```

**Stays with the core owner, do not touch:** `iobase_vtables.cpp`, `rt_io_port.cpp`,
`rt_factories_io.cpp`, `closure_batch2b_win.cpp`, `deleting_dtor*.cpp`, `rt_data_mint*`.

### C.2 THE RULE THAT GOVERNS THIS WHOLE BLOCK: no real constructors

§4.3. **A stream class gets real members and a real declaration; it does NOT get a real
constructor.** Every construction site keeps stamping `RT_DATA_SLOT("??_7…@IO@@6B@")`. A commit
that gives a stream class a C++ constructor is rejected on sight.

Why it is not merely cautious — the measured construction sites:

| class | allocation | who stamps the vptr |
|---|---|---|
| `CompressedFileStream` (0xb8) | **stack local**, `runtime/src/object/external_load.cpp:287` | `external_load.cpp:289`, again at `:458` on the inlined teardown path. Only the DTOR has a door (0x1800a9330). Retail inlines it the same way — `??_7CompressedFileStream@` is stamped inside `External::Save` (0x1800bbc63), `IsCurrent` (0x1800bbf5d) and `Load` (0x1800bc653). |
| `TextFileStream` (0x80) | **stack local**, `runtime/src/object/external_savetext_win.cpp:286` | a real ctor door at `:299`, plus an independent re-stamp at `:362` |
| `VisitorStream` (0x18) | local in `iobase_vtables.cpp` | its own ported ctor's base→derived→base restamp |
| `CompressedCopyFileStream` (0xc8) | **unreached** — C.6 | nothing |
| the rest | inside `runtime/src/io/` | their own ported ctors, stamping `RT_DATA_SLOT` |

**Two of the four are in `runtime/src/object/`, which is not your file set.** Converting those
constructors would change the table worn by an object built in a file you do not own, on the
`External::Load` / `External::SaveText` paths.

And §4.2: **not one of the fourteen IO deleting-dtor registrations, and not one of the nineteen
minted rows, may be deleted in this step.** Thirteen of the fifteen tables are stamped from
OUTSIDE `runtime/src/io/`; `IO::Stream` and `IO::ReaderWriter` are additionally
`CUTOVER_REQUIRED`. Use §A.4's shape: declare, pin the layout, leave slot 0 pure.

### C.3 The slot lists

§1.3 has all ten tables with their bounding arguments. The three that catch people:

- **`FileStream`'s slot 7 is `Open`, NOT `Close`** — the one place the two branches disagree
  about what slot 7 means.
- **`BinaryFileStream` differs from `FileStream` in slot 0 ONLY**; slots 1..7 are the same eight
  addresses. This is the retail fact `NoteSlotDispatch` exists for (C.4).
- **`TextFileStream` is the family's one overload-order case**: slot 7 is FileStream's inherited
  `Open(const Filename&)`, slot 8 is TextFileStream's own `Open(const Filename&, Format)`.
  Declare slot 8 as **`OpenWithFormat`** — §1.3's rule. Slot 9 is `Create`.

### C.4 `NoteSlotDispatch` dies WHOLE with `IO::BinaryFileStream`

§4.1. `file_stream.cpp:367-371` (`BfsPromoteCounters`), `:377-380` (`NoteBfsStamp`), `:390-395`
(`NoteSlotDispatch`), `OurBfsTable()`, and **seven call sites**: `file_stream.cpp:1042, 1074,
1098, 1104, 1138` and `file_stream_win.cpp:100, 135`, plus whatever prints the counters. All of
it, in the commit that makes `IO::BinaryFileStream` a real class. **Do not leave it counting
zero and do not leave it counting a constant.** The question it answers is answered structurally
once the class exists; there is nothing to port forward.

### C.5 `Internal::Clone` — its own small commit

`?Clone@Internal@Object@@QEAAPEAV12@XZ` (0x1800bfb10) is genuinely UNPORTED: no registration by
name or address, no stub, no TODO. Its only stated blocker was `IO::MemoryStream`, which has had
a real body in all 8 vtable slots for a long time, so **the blocker is already satisfied**
(§6.5e). It is a serialize round-trip deep copy: a 0x1000-byte MemoryStream pair, the Factory
descent, `IsDerivedFrom`, then slot 3 both ways. Port it as its own commit, gated on `make unit`
plus one resave PASS — its resave exposure is nil until something calls it.

### C.6 `IO::CompressedCopyFileStream` — do NOT convert, do NOT delete

Resolved at the phase-1 close: its ctor `??0CompressedCopyFileStream@IO@@` @ **0x1800a92d0**
(signature `(CompressedCopyFileStream*, CompressedMemoryStream*)`) has **zero call xrefs
anywhere in Runtime-x64.dll** — four DATA references, all export-table entries — and the only
stampers of its vtable 0x180155510 are its own ctor, its own dtor and the two deleting-dtor
thunks. Nothing in the DLL constructs one and nothing in our tree does.
`compressedcopyfilestream.cpp` ports 4 of its 9 slots against an object it never sees created.
An unreached class is an unverifiable port in both directions. Leave it; record it in your report.

### C.7 The duplicate-POD collapse

§1.5.2. `rt::io::Stream` (`Serializer.h:234`) and `RtIo::Stream` (`Stream.h:70`) are two
independently declared 0x18-byte structs with the same three fields in two namespaces, and
neither header includes the other. `IO::Stream` replaces both. **Grep BOTH names before
declaring any `Stream` caller converted.** Keep both PODs alive, trivially destructible and
cross-asserted against the class until their last user converts (B33 §7) — giving a POD a
destructor runs a second one (the `RtSystem::String` double-free lesson).

This block also removes block 3's bridge casts in `serializer.cpp` (§B.4) — signatures do not
move, only the casts go.

### C.8 `ExternalFactory::SerializeReferenceTable` as a member

`?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` (0x1800bcb60) is
already fully ported as a free function (`external_reftable.cpp:192-193`, registered by name and
address at `:438-441`). Its 2nd parameter is `rt::io::Stream*` — the lane-A **duplicate POD**,
which is exactly what C.7 collapses. When `IO::Stream` exists, it becomes
`Object::ExternalFactory::SerializeReferenceTable(IO::Stream&)`.

**`external_reftable.cpp` is on the `ExternalFactory::Add` loader-race path. Read §D before
touching it.**

### C.9 Gate

§7.1, resave **x3 consecutive**, baseline 21075114 / `tail=263`. This block changes the stream
bodies the save format runs through; if any single commit cannot produce three in a row, stop.

---

## §D. THE CONSTRAINT BOTH C AND E LIVE UNDER — the loader defect

**This is B33 §0 restated with a live, measured instance. It binds STREAMS and PACKAGE+FS.**

A pre-existing, intermittent defect exists in the compressed-load path, measured by the
coordinator on an UNCHANGED tree (block-0 exe, new-game route, 6 boots):

- **2 of 6 boots** loaded `Game/Help/HelpTopics.rsc` as NULL
  (`[GUL::Help] ... loaded=0 isExternalList=0 topics=-1`);
- **one of those two then HUNG in teardown** (exit 4). Symbolized: MAIN thread in
  `RtMintLoaderTeardown` (`rt_data_mint.cpp:426`) to `RtSystem_Thread_Dtor`
  (`thread_win.cpp:197`), waiting for the loader thread; the LOADER thread, 45 s after Quit,
  still EXECUTING inside `RtIo_CompressedStream_Read` (`compressed_stream_read.cpp:430`) to
  `RtIo_CompressedStream_SetWindow` (`:324`) under a deep nested External load — i.e. spinning.
  Same shape STATUS records from 2026-09-12 ("CompressedStream::Read spun on a zero-length
  chunk past EOF").
- baseline `[ExtLoad] openCompFail` reads 0,2,0,0,1,1 across six unchanged-main boots, and the
  casualty resource varies.

**THE RULE.** `compressed_stream_read.cpp`, `compressed_stream_open.cpp`, `external_load.cpp`
and the FileSystem/PackageFile open path stay **byte-faithful** when their bodies move onto
members: **no added lock, no retry, no loop bound, no null check, no reordering** — and equally
**no silent removal** of whatever makes it spin. **If a conversion makes the intermittent
NULL-resource or Read-spin symptom stop reproducing, that is a RED FLAG to REPORT, not a win.**
It means the conversion changed timing or control flow somewhere you did not intend.

**In your self-checks**, treat as this defect and re-run (telling the coordinator the count):
exit 4 with a `standalone_hang.log` carrying that stack, or `topics=-1` / `diseases=-1` on ONE
boot. **The same resource NULL on EVERY boot is real** — stop.

One observation from block 2's self-check that may matter to the root-cause work: a run showed
`topics=-1` with **`openCompFail` not logged at all** (count 0) while `DiseaseList` was healthy
(`isExternalList=1 diseases=12`). So the NULL can occur WITHOUT an open failure being recorded,
and keying on `openCompFail >= 1` may undercount.

---

## §E. BRIEF 3 — PACKAGE+FS

Design: `B34_RULES.md` §1.4, §3.2 move 5, §4.2, §4.3, §6.3. **Starts only after block 3
merges.** Disjoint from STREAMS by construction.

### E.1 File set

```
runtime/types/io/FileSystem.h
runtime/types/io/Modification.h
runtime/types/io/Persistence.h
runtime/types/io/Registry.h
runtime/types/io/PackageFile.h                (NEW — E.3)
runtime/src/io/package_file.cpp
runtime/src/io/package_file_dtor.cpp
runtime/src/io/package_file_read_win.cpp
runtime/src/io/package_file_serialize.cpp
runtime/src/io/filesystem.cpp
runtime/src/io/filesystem_createfile_win.cpp
runtime/src/io/filesystem_find_win.cpp
runtime/src/io/filesystem_openfile_win.cpp
runtime/src/io/filesystem_packages.cpp
runtime/src/io/filesystem_setupdirs_win.cpp
runtime/src/io/filesystem_startup.cpp
runtime/src/io/filesystem_timestamp_win.cpp
runtime/src/io/filesystem_win.cpp
runtime/src/io/modification.cpp
runtime/src/io/modification_refresh.cpp
runtime/src/io/modification_setupfilesystem.cpp
runtime/src/io/modification_sort.cpp
runtime/src/io/persistence.cpp
runtime/src/io/persistence_registry_win.cpp
runtime/src/io/persistence_saveinfo_job.cpp
runtime/src/io/persistence_savelist_job.cpp
runtime/src/io/persistence_saves.cpp
runtime/src/io/persistence_saves_win.cpp
runtime/src/io/persistence_startup.cpp
runtime/src/io/persistence_worker_win.cpp
runtime/src/io/text_codec.cpp
```

Same "stays with the core owner" list as C.1.

### E.2 `IO::PackageFile` IS an `Object::External` subclass

§1.4. Its 9-slot table (0x180155700) has, at slots 1..7, `Object::External`'s
`TypeName`/`Type`/`GetFileVersion`/`Compile`/`Serialize`/`Reconstruct`/`Destruct` in External's
exact order — which is also what positively confirms B33 §1.4's positional naming of External
`[6]`/`[7]`. So `class PackageFile : public Object::External`, and
`package_file_serialize.cpp`'s "slot 5" is `External::Serialize(IO::Stream&) -> bool`.

**SLOT 8 IS UNRESOLVED AND IS YOUR CALL TO MAKE.** `0x1800252f0`, Ghidra-named `Construct`,
returning `External*`. `??_7External@Object@@6B@` @ 0x1801547a0 is 8 slots (bounded by a null
qword at 0x1801547e0), so slot 8 is a virtual `PackageFile` ADDS. Whether retail declared it on
`PackageFile` or on an unmodelled class between `External` and `PackageFile` was not determined.
**Read 0x1800252f0's callers in Ghidra before choosing** (and see §A.6 / the Ghidra note below).
Until then, declare it on `IO::PackageFile`.

`IO::PackageFile` is the only IO class with a `?_typeInfo@` mint row (0x1801f9ac8,
`rt_data_mint_tables.inc:4511`) and the row is LIVE, not orphaned. It takes
`DECLARE_TYPE`/`DEFINE_TYPE` eventually, but **its record stays MINTED for now**, for the same
reason B.3.3's closing pass left the five Object `?_typeInfo@` rows minted.

### E.3 There is no `PackageFile.h` and its layout is UNKNOWN

The class is 0xf0 bytes (from its `RT_DELDTOR` row). Its layout is scattered across
`FileSystem.h`'s `PackageFileView` (:81) and `PackageFileEntry` (:156) plus local asserts in
`package_file.cpp`. **Your first commit is reading `package_file.cpp`'s `static_assert`s in full
and writing `runtime/types/io/PackageFile.h` from them** — do not start the class from a
decompile, and do not extend a layout without a fresh assert (`Persistence.h`'s own banner
states that rule for its file and it applies here).

### E.4 The rest

- **`IO::FileSystem` has TWO independently cached singleton pointers** in `engine_funcs.h`:
  `g_pFileSystemSingleton` (line 3540, exref 0x140151d18) and `g_pFileSystemSingleton2`
  (line 1285, "separate exref for GetDataDirectory calls"). A real `FileSystem::Get()` has to
  reconcile them; do not assume one.
- **`IO::Persistence.h` is deliberately partial** — only +0x00, +0x08 and +0x90 are pinned;
  +0x0C..+0x8F are explicitly UNKNOWN and the header forbids extending it from a decompile
  without a fresh `static_assert`. It is singleton-only.
- **`IO::Modification`'s destructor order is retail's and is reproduced, not tidied**: it clears
  the exported singleton (tagged `UNPROMOTABLE(HARMFUL)` in `reg_mint_promoted.cpp`), then
  destroys `savedMods`, then `mods`.
- **`IO::Registry` keeps the `IO` namespace** despite the name-adjacency to `System`. That is
  retail's placement, not a mistake to fix.
- §4.3 and §4.2 apply here too: `PackageFile` is constructed by a `standalone/rt_factories_io.cpp`
  object-creation replay row, so **its constructor does not convert** — our ctor taking over
  would swap the table under the replay.
- **§D binds the FileSystem/PackageFile open path.**

### E.5 Gate

§7.1, resave x3 consecutive. The Mods route with `.pkm` mounted matters especially for this
block (Modification), and the Load Game dialog route for Persistence.

---

## §F. The self-check recipe that worked (both briefs, every block)

The coordinator runs the real gate; this is what to run yourself before reporting a hash. It
caught nothing in blocks 1-2 that the coordinator would have missed — but it caught it an hour
earlier, and it is what let block 1's false DIFF be diagnosed instead of shipped.

```bash
tools/game_dir_lock.sh acquire b34-owner 1800      # BARE — never piped; a pipe makes it exit 0
cp "C:/code/GIT/untitled_rev_eng_project/docs/runtime_port/resave_gate/golden/Gate.resaved.bin" \
   docs/runtime_port/resave_gate/golden/Gate.resaved.bin
cp ".../golden/Gate.resaved.md5" docs/runtime_port/resave_gate/golden/Gate.resaved.md5
mingw32-make sa-deploy
tools/resave_gate/resave_gate.sh Gate              # x3, consecutive
tools/boot_state.sh --wait 300                     # never a grep-until-timeout loop
tools/game_dir_lock.sh release b34-owner
git checkout -- docs/runtime_port/resave_gate/golden/Gate.resaved.md5   # line endings, §A.6
```

**THE GOLDEN IS GIT-IGNORED, SO YOUR WORKTREE HAS NONE.** Without the copy,
`resave_gate.sh` silently cuts a NEW golden from your own build and then "PASSes" against
itself. **Check the log says `verdict: PASS`, not `GOLDEN CREATED`.** Healthy numbers:
`21075114` bytes, `tail=263`, `diffBytes=15975558`.

**The coordinator's row-level reference-table diff** tells you whether a DIFF is race-shaped
without guessing from byte counts:

```
python "C:\Users\USPC\AppData\Local\Temp\claude\C--code-GIT-untitled-rev-eng-project\
        afeb245e-06b2-42d9-84fd-083e344a0e05\scratchpad\coord\reftable_diff.py" <golden.bin> <new.bin>
```

exit **0** = identical · **10** = race-shaped, re-runnable · **20** = real-shaped, localise it.
**It never turns a DIFF into a PASS.** Read §3.4 before reading any DIFF: the discriminator is
*which rows differ*, not the byte count, and you must never bisect on a single DIFF — block 1
did and was wrong.

**Ghidra**: both `Runtime-x64.dll` and `Runtime-x64-profile.dll` sit at image base
0x180000000 and `switch_program` is GLOBAL state in the one instance. If anything switches to
the profile DLL for names, your next byte read comes from the wrong binary **silently**.
Re-assert `get_current_program_info` before every batch of slot reads. The RETAIL DLL is the
only source of vtable OFFSETS; the profile DLL is a source of NAMES only — and a good one: the
attested class names `IO\BinaryEncoding.cpp` / `IO\TextEncoding.cpp` / `IO\VisitorEncoding.cpp`
came from its source-path strings, cross-checked against
`docs/runtime_port/original_source_tree.txt`.
