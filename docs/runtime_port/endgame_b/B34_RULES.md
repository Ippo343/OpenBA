# ENDGAME B.3 step 4 — `IO` (Stream, Serializer, Encoding, Package), `System` rest: rules for the step

Scope: ENDGAME.md §B.3 step 4 / §B.1 decisions 1, 2, 5 and 7. Design read:
`IO_INVENTORY.md` and `IO_CALLER_INVENTORY.md` in this directory, both corrected in place by
this step's spot-check (the corrections are listed in §8).

**`B33_RULES.md` applies unchanged** — §4 (the coexistence proof), §5 (a vptr is never an
identity), §6 (a `RT_DELDTOR` row dies only when nothing reads the minted vtable it fills) and
§7 (the settled design points) are this sheet's foundations and are not restated except where
IO makes them bite differently. `B31_RULES.md`'s two-step shim pattern, class design rules and
hygiene block, and `B32_RULES.md`'s "the template WRAPS the ported core", apply verbatim.

This step has ONE serial owner for the core (§6.1) and fans out only after §6.1 has merged.

---

## 0. Out of scope, and what must not be "fixed" on the way through

- **`Save::SaveLoad` is NOT in this step.** It is its own namespace (43 CALL sites,
  `IO_CALLER_INVENTORY.md` §3a), it is ENDGAME §B.3 order item 7, and its receiver is the
  save/load *controller*, not a stream. It is named here only so a batch owner does not pick
  it up because `saveloaddialog.cpp` sits next to `IO::Persistence`'s callers.
- **`fn_MemAlloc` / `fn_MemFreeStatic` / `fn_MemAllocStatic` / `fn_MemFree` are DEFERRED** —
  see §6.5(c). They are 1,146 CALL sites and they are allocator doors, not IO.
- **`Object::Base`, `BaseTypeInfo`, `Factory`, `External`, `Internal`** are real classes since
  B.3.3. Do not re-open them. The one exception this step forces is `Object::External`'s slot
  count, §1.6 — and that is a *correction to a comment*, not a re-conversion.
- **The `ExternalFactory::Add` loader-thread AVL-rotate race is still its own investigation**
  (B33 §0). This step touches `external_reftable.cpp`, which is on that path. The conversion
  must not add a lock, a retry, an ordering change or a null check there, and if a commit makes
  the race stop reproducing that is a red flag to report, not a win.
- **Do not "tidy" a `Serialize` body's ORDER.** Construction of an `IO::Serializer` consumes or
  emits a 12-byte stream header and its destruction reads or patches 4 more (§7.3). Moving a
  ctor or a dtor one statement is a save-format change.
- Do not redesign `deleting_dtor.cpp`'s mechanism (B33 §6.5). Rows retire one at a time, and
  in IO **none of them may retire in this step** — §4.2.

---

## 1. Question 1 — the real slot lists, from the RETAIL DLL

All of §1 was read byte-wise out of the shipped `Runtime-x64.dll`'s `.rdata` (image base
0x180000000) in Ghidra on 2026-09-18, with every body's name taken from the same program's
symbol table. **The profile DLL was not consulted for any number** — its ReaderWriter and
Encoding offsets differ (`reference_encoding_vtable_offsets_differ_between_builds.md`), and it
is a source of names only.

### 1.1 `IO::Encoding` — 5 slots, and **slot 0 is NOT a destructor**

This is the single most load-bearing finding in §1, because it is the one shape that breaks if
a class is declared the way every other IO class is.

The three tables are adjacent-bounded and byte-identical in shape:

| slot | +off | Binary 0x1801554a8 | Text 0x1801559d0 | Visitor 0x1801559f8 | name |
|---|---|---|---|---|---|
| 0 | +0x00 | 0x1800a81e0 `MOV AL,1; RET` | 0x1800b9ab0 `XOR AL,AL; RET` | 0x1800bafe0 `XOR AL,AL; RET` | `virtual bool IsBinary() const` |
| 1 | +0x08 | 0x1800a81d0 `XOR AL,AL; RET` | 0x1800b9aa0 `MOV AL,1; RET` | 0x1800bafd0 `XOR AL,AL; RET` | `virtual bool IsText() const` |
| 2 | +0x10 | 0x1800a81c0 `MOV EAX,2; RET` | 0x1800b9a90 `MOV EAX,1; RET` | 0x1800bafc0 `MOV EAX,3; RET` | `virtual Kind GetKind() const` |
| 3 | +0x18 | 0x1800a8120 | 0x1800b9a80 `XOR EAX,EAX; RET` | 0x1800baf00 | `virtual ReaderWriter* CreateReader(Serializer&, int version)` |
| 4 | +0x20 | 0x1800a8080 | 0x1800b9a70 `XOR EAX,EAX; RET` | 0x1800bae40 | `virtual ReaderWriter* CreateWriter(Serializer&, int version)` |

Bounding: 0x1801554a8 + 0x28 == 0x1801554d0 (`??_7BinaryFileStream@IO@@6B@`);
0x1801559d0 + 0x28 == 0x1801559f8; 0x1801559f8 + 0x28 == 0x180155a20 (`??_7VisitorStream@`).
The three constant bodies were decoded from their raw bytes, not inferred: `{Binary: true,
false, 2}`, `{Text: false, true, 1}`, `{Visitor: false, false, 3}` — which confirms
`encoding_visitor_text.cpp`'s own `{false,true,1}` / `{false,false,3}` notes and supplies the
Binary row those notes left implicit. Text's two factories really are bare `XOR EAX,EAX; RET`:
text serialization was stripped from the shipped DLL.

**RULES.**

1. `IO::Encoding` declares `IsBinary()` FIRST and declares **no destructor slot at all**. Its
   `~Encoding()` is protected and non-virtual. A `DeletingDtor` at slot 0 here would push both
   factories down one slot and `Serializer`'s constructor would call `GetKind()` as a factory —
   a build-clean, link-clean corruption of every save. `tests/unit/io_vtable_abi_test.cpp`'s
   `encoding_factory_slots_sit_at_0x18_and_0x20_with_arity_three` is the regression guard.
2. This is safe because nothing virtual-deletes an Encoding: `IO::Stream`'s destructor frees
   `_encoding` with a direct `MemoryAllocator::MemFree` (`Stream.h:83-101`).
3. `CreateReader`/`CreateWriter` have **arity 3**, `(this, Serializer*, int version)` —
   `Serializer.h:41-63` pins it from R8D's liveness across 0x1800b9960 and it is re-proved live
   in the unit test. `rt::io::encoding_vtbl::kCreateReader`/`kCreateWriter` stay as the names.
4. The concrete encodings are `IO::BinaryEncoding`, `IO::TextEncoding`, `IO::VisitorEncoding`.
   **All three tables are UNEXPORTED**, so all ten bodies are registered by ADDRESS only
   (`encoding_visitor_text.cpp:121-135`, `encoding.cpp`) and stay that way until the classes
   land. `IO::Encoding` carries no data members — retail's `this` is never read by either
   factory — so the class is a pure interface with `sizeof == 8`.

### 1.2 `IO::ReaderWriter` — 26 slots, 1 real + 25 pure

`??_7ReaderWriter@IO@@6B@` @ **0x1801553b0**: slot 0 is 0x180023bd0 (the deleting dtor, which
is also `RT_DELDTOR(0x180023bd0, 0x1800b9150, 0x020, "IO::ReaderWriter")`), and **slots 1..25
are every one of them `_purecall` (0x18007ec28)**. The three concrete tables are contiguous
with it and each is exactly 0xd0 bytes:
`0x180155210` (BinaryWriter) + 0xd0 == `0x1801552e0` (BinaryReader) + 0xd0 == `0x1801553b0`.
`??_7Visitor@IO@@6B@` @ 0x1801558b0 is the same 26-slot shape, ending at
0x180155980 (`??_7TextFileStream@`).

So 26 is confirmed FOUR ways: the adjacency ladder, the 0xd0 stride, the all-purecall tail of
the abstract table, and `rw_vtbl::kSerializeBytes == 0xC8` being the last byte offset
(0xC8/8 == 25, so 26 slots, 0..25).

**The slot list, with the distinct-name scheme this step adopts.** The middle column is the
byte offset, which is what the 54 files using `rt::io::rw_vtbl::k*` already spell.

| slot | +off | `rw_vtbl` constant | BinaryReader body | BinaryWriter body | **class member name** |
|---|---|---|---|---|---|
| 0 | 0x00 | `kDtor` | 0x180023a10 | 0x180023b50 | `virtual ReaderWriter* DeletingDtor(unsigned)` |
| 1 | 0x08 | `kGetVersion` | 0x180023930 | 0x180023ab0 | `virtual int GetVersion() const` |
| 2 | 0x10 | `kSkip` | 0x1800a83f0 | 0x1800a8b00 | `virtual void Skip()` |
| 3 | 0x18 | `kArrayChar` | 0x1800a8410 | 0x1800a8b10 | `SerializeCharArray(int, char*, const char*)` |
| 4 | 0x20 | `kArrayBool` | 0x1800a8460 | 0x1800a8b60 | `SerializeBoolArray` |
| 5 | 0x28 | `kArrayFloat` | 0x1800a84b0 | 0x1800a8bb0 | `SerializeFloatArray` |
| 6 | 0x30 | `kArrayUInt64` | 0x1800a8510 | 0x1800a8c10 | `SerializeUInt64Array` |
| 7 | 0x38 | `kArrayInt64` | 0x1800a8570 | 0x1800a8c70 | `SerializeInt64Array` |
| 8 | 0x40 | `kArrayUInt` | 0x1800a85d0 | 0x1800a8cd0 | `SerializeUIntArray` |
| 9 | 0x48 | `kArrayInt` | 0x1800a8630 | 0x1800a8d30 | `SerializeIntArray` |
| 10 | 0x50 | `kArrayUShort` | 0x1800a8690 | 0x1800a8d90 | `SerializeUShortArray` |
| 11 | 0x58 | `kArrayShort` | 0x1800a86f0 | 0x1800a8df0 | `SerializeShortArray` |
| 12 | 0x60 | `kArrayUChar` | 0x1800a8750 | 0x1800a8e50 | `SerializeUCharArray` |
| 13 | 0x68 | `kArraySChar` | 0x1800a87b0 | 0x1800a8eb0 | `SerializeSCharArray` |
| 14 | 0x70 | `kScalarChar` | 0x1800a8810 | 0x1800a8f10 | `SerializeChar(char&, const char*)` |
| 15 | 0x78 | `kScalarBool` | 0x1800a8830 | 0x1800a8f30 | `SerializeBool` |
| 16 | 0x80 | `kScalarFloat` | 0x1800a8850 | 0x1800a8f50 | `SerializeFloat` |
| 17 | 0x88 | `kScalarUInt64` | 0x1800a8870 | 0x1800a8f70 | `SerializeUInt64` |
| 18 | 0x90 | `kScalarInt64` | 0x1800a8890 | 0x1800a8f90 | `SerializeInt64` |
| 19 | 0x98 | `kScalarUInt` | 0x1800a88b0 | 0x1800a8fb0 | `SerializeUInt` |
| 20 | 0xA0 | `kScalarInt` | 0x1800a88d0 | 0x1800a8fd0 | `SerializeInt` |
| 21 | 0xA8 | `kScalarUShort` | 0x1800a88f0 | 0x1800a8ff0 | `SerializeUShort` |
| 22 | 0xB0 | `kScalarShort` | 0x1800a8910 | 0x1800a9010 | `SerializeShort` |
| 23 | 0xB8 | `kScalarUChar` | 0x1800a8930 | 0x1800a9030 | `SerializeUChar` |
| 24 | 0xC0 | `kScalarSChar` | 0x1800a8950 | 0x1800a9050 | `SerializeSChar` |
| 25 | 0xC8 | `kSerializeBytes` | 0x1800a8970 | 0x1800a9070 | `SerializeBytes(int, void*, const char*)` |

(`IO::Visitor`'s 26 bodies are 0x18002b710, 0x18002b670, then 0x1800bacc0 stepping by 0x10 to
0x1800bae30. Its slot 2 is the Skip; the 23 Serialize bodies are one-instruction no-ops.)

The names were verified against Ghidra's own symbols for the BinaryReader bodies — slot 2 is
`Skip(BinaryReader*)`, slot 3 is `Serialize(BinaryReader*, int, char*, char*)`, slot 14 is
`Serialize(BinaryReader*, char*, char*)`, slot 25 is `SerializeBytes(BinaryReader*, int, void*,
char*)` — i.e. retail declares 22 of these 26 as `Serialize` OVERLOADS.

**RULES.**

1. **ENDGAME decision 2's distinct-name rule applies to the 22 VIRTUALS, and the scheme is
   `Serialize<Type>` / `Serialize<Type>Array` exactly as tabled above.** Declaring them in SLOT
   order under distinct names means MSVC's "group overloads and reverse declaration order" rule
   has nothing to act on: the mapping is a plain 1:1 with the retail byte offsets and needs no
   reversal argument at all. Any future reader who tries to reconstruct retail's declaration
   order from this table is reconstructing a fact we deliberately stopped depending on.
2. **`IO::Serializer`'s NON-virtual forwarders keep the public name `Serialize` and ARE real
   C++ overloads.** Retail's 22 exported members are all `QEAA`/`QEBA`
   (`?Serialize@Serializer@IO@@QEAAXAEAHPEBD@Z` and friends, `serializer.cpp:361-388`); none is
   virtual, each tail-jumps into the ReaderWriter's table. So a caller in a CONVERTED class,
   holding a typed member, writes `ser.Serialize(x, "name")` and overload resolution picks the
   width. **That is the END state, not the B.3.4 caller batches' rule** (coordinator amendment,
   2026-09-19): today's 861 call sites mostly pass untyped `s + 0xNN` byte pointers through
   `void*` doors, so in the batches the DOOR names the type and the type is spelled at the call
   — §7.3. The batch is still mechanical; it is mechanical on the door's name, not on the
   argument's type.
3. **Slot 0 is `DeletingDtor(unsigned)` and `~ReaderWriter()` is NON-virtual** (decision 2).
   `tests/unit/io_vtable_abi_test.cpp`'s
   `a_virtual_destructor_would_shift_every_ReaderWriter_slot` is the guard: Itanium spends two
   slots on a virtual destructor, so one would silently mis-aim every `rw_vtbl::k*` offset in
   54 files.
4. `IO::ReaderWriter` is 0x20 bytes: `vtable` / `allowCustomSerialize` +0x08 / `serializer`
   +0x10 / `stream` +0x18, already pinned in `Serializer.h:118-127`. `IO::BinaryCodec` (0x30,
   `version` at +0x28) is the base of `BinaryReader`/`BinaryWriter`; `IO::VisitorCodec` (0x28,
   `version` at +0x20) is `Visitor`'s. Keep both `sizeof` asserts — they are what the
   allocator's `MemAlloc(0x30,4)` / `MemAlloc(0x28,4)` requests pin.

### 1.3 The `IO::Stream` family

| class | vtable | slots | bounded by |
|---|---|---|---|
| `IO::Stream` | 0x1801549a0 | 7 | null qword at 0x1801549d8 |
| `IO::FileStream` | 0x180154960 | 8 | 0x180154960 + 0x40 == 0x1801549a0 |
| `IO::BinaryFileStream` | 0x1801554d0 | 8 | + 0x40 == 0x180155510 |
| `IO::MemoryStream` | 0x1801555b0 | 8 | + 0x40 == 0x1801555f0 |
| `IO::CompressedStream` | 0x180155640 | 9 | string `"PackageFile"` at 0x180155688 |
| `IO::CompressedMemoryStream` | 0x180155560 | 9 | null qword at 0x1801555a8 |
| `IO::CompressedFileStream` | 0x1801555f0 | 9 | null qword at 0x180155638 |
| `IO::CompressedCopyFileStream` | 0x180155510 | 9 | null qword at 0x180155558 |
| `IO::TextFileStream` | 0x180155980 | 10 | + 0x50 == 0x1801559d0 (the Text Encoding) |
| `IO::VisitorStream` | 0x180155a20 | 7 | string `"ExternalCompiler"` at 0x180155a58 |

Every count agrees with the row in `standalone/rt_data_mint_tables.inc`, independently derived
there by adjacency / a ctor's own `LEA` / a byte-boundary argument.

**`IO::Stream`'s 7 slots** (0x1801549a0): `[0]` 0x18000c610 `DeletingDtor(unsigned)`,
`[1]` 0x18000c6a0 `GetSource() const -> System::String`, `[2..6]` `_purecall` —
`Write(int, const void*) -> bool`, `Read(int, void*) -> bool`, `SetCurrentPos(uint)`,
`GetCurrentPos() const -> uint`, `GetSize() const -> uint`.

**Derived classes add, in this order:** `Close()` at slot 7 (+0x38) and `PreCommit()` at
slot 8 (+0x40) on the Compressed branch; `Close()` at slot 7 on `MemoryStream` (0x1800ae9e0)
and `MemoryStream` adds NO `PreCommit`; `Open(const System::Filename&) -> bool` at slot 7 on
`FileStream` (0x1800aac70) — **`FileStream`'s slot 7 is `Open`, NOT `Close`**, which is the one
place the two branches genuinely disagree about what slot 7 means.

**`IO::BinaryFileStream` differs from `IO::FileStream` in slot 0 ONLY.** Read byte for byte:
FileStream `[0]`=0x18000c090, BinaryFileStream `[0]`=0x180023d80, and slots 1..7 are the same
eight addresses in both (0x1800aab50, 0x1800aab90, 0x1800aabf0, 0x1800aac10, 0x1800aac50,
0x1800aac60, 0x1800aac70). This is the retail fact `file_stream.cpp`'s `NoteSlotDispatch`
exists to work around, and §4.1 disposes of it.

**`IO::TextFileStream`'s 10 slots — the ONE overload-order case in the family.**

| slot | +off | body | name |
|---|---|---|---|
| 0 | 0x00 | 0x18002ba90 | `DeletingDtor` |
| 1 | 0x08 | 0x1800aab50 | `GetSource` — **inherited from FileStream, not overridden** |
| 2..6 | 0x10..0x30 | 0x1800ba9f0 / 0x1800ba900 / 0x1800babd0 / 0x1800bac30 / 0x1800ba8e0 | `Write` / `Read` / `SetCurrentPos` / `GetCurrentPos` / `GetSize` |
| 7 | 0x38 | 0x1800aac70 | `Open(const Filename&)` — **inherited from FileStream** |
| 8 | 0x40 | 0x1800ba640 | `Open(const Filename&, Format)` — TextFileStream's OWN overload |
| 9 | 0x48 | 0x1800ba940 | `Create(const Filename&, Format, Format)` |

So `Open` is overloaded ACROSS the base/derived boundary and both entries are live.
**RULE: `IO::FileStream::Open(const System::Filename&)` keeps the name `Open`;
`IO::TextFileStream`'s slot-8 override is declared `OpenWithFormat(const System::Filename&,
Format)`.** Declaring it `Open` in the derived class would hide the base's name (C++ name
hiding), which is a compile error at every inherited-`Open` call site, not silent — but the
distinct name is what decision 2 asks for and it makes slot 8 unmistakable.

**`IO::VisitorStream`'s 7 slots** (0x180155a20): `[0]` 0x18002bf80, `[1]` 0x18000c6a0
(Stream's own `GetSource`, not overridden), `[2..6]` 0x1800bb030 / 0x1800bb020 / 0x1800bb010 /
0x1800bb000 / 0x1800baff0. It is a NULL SINK — 0x18 bytes, `Stream` and nothing else.

### 1.4 `IO::PackageFile` — 9 slots, and it is an `Object::External`

`??_7PackageFile@IO@@6B@` @ 0x180155700, 9 slots, bounded by a null qword at 0x180155748:

| slot | +off | body | Ghidra name / signature |
|---|---|---|---|
| 0 | 0x00 | 0x1800259d0 | `DeletingDtor` (`RT_DELDTOR(..., 0x0f0, "IO::PackageFile")`) |
| 1 | 0x08 | 0x180025150 | `char* TypeName(PackageFile*)` |
| 2 | 0x10 | 0x180025130 | `uint Type(PackageFile*)` |
| 3 | 0x18 | 0x1800b6080 | `int GetFileVersion(PackageFile*)` |
| 4 | 0x20 | 0x1800b6090 | `CompileResult Compile(PackageFile*, Filename*)` |
| 5 | 0x28 | 0x1800b60a0 | `bool Serialize(PackageFile*, Stream*)` |
| 6 | 0x30 | 0x1800252d0 | `External* Reconstruct(PackageFile*)` |
| 7 | 0x38 | 0x180025300 | `void Destruct(PackageFile*)` |
| 8 | 0x40 | 0x1800252f0 | `External* Construct(PackageFile*)` |

Slots 1..7 are `Object::External`'s eight slots minus its own slot 0, in B33 §1.4's exact
order — which independently confirms B33's positional naming of External `[6]`/`[7]` as
`Reconstruct`/`Destruct`, since PackageFile's overrides at those positions carry exactly those
names. So **`IO::PackageFile : public Object::External`**, and `package_file_serialize.cpp`'s
"slot 5" is `External::Serialize(IO::Stream&) -> bool`.

**Slot 8 is PackageFile's own.** `??_7External@Object@@6B@` @ 0x1801547a0 is 8 slots, bounded
by a null qword at 0x1801547e0 (re-read this pass: 0x180006640, 0x1800066c0, 0x1800066d0,
`_purecall`, 0x1800066e0, `_purecall`, `_purecall`, `_purecall`). So `Construct` at +0x40 is a
virtual `IO::PackageFile` ADDS.

**SETTLED 2026-09-19 by the PACKAGE+FS owner (B.3.4 commit 3/6): `Construct` IS DECLARED BY
`IO::PackageFile` ITSELF, not by an unmodelled class between `External` and it.** The earlier
revision of this paragraph flagged it as undetermined and told whoever converted PackageFile to
re-read 0x1800252f0's callers first. That was done, in retail `Runtime-x64.dll`
(`function_count` 15290, re-asserted before the reads), and it produced four independent pieces
of evidence:

1. **0x1800252f0 is reachable from exactly ONE vtable.** `get_xrefs_to(0x1800252f0)` returns
   three references and no more: the export-table entry at 0x1801b26ec, the EXTERNAL
   entry-point row, and **0x180155740**, which is `??_7PackageFile@IO@@6B@ + 0x40` — slot 8.
   No other class's table holds this body, so no sibling shares it.
2. **`Object::External`'s own table stops at eight**, re-read byte-wise this pass: the null
   qword at 0x1801547e0 is immediately followed by the `"External"` type-name string.
   `Construct` is absent from it.
3. **Fifty-eight classes across nine namespaces each export their OWN
   `?Construct@<Leaf>@<NS>@@UEAAPEAVExternal@Object@@XZ`** — UI (30), Graphics (8), Video (5),
   Audio (2), Collision2D (2), Picking, Pathfinding, Achievement, `Object::ExternalList` and
   this one — and **there is no `?Construct@` on any base class at all**. Each sits inside a
   per-class reflection-boilerplate block whose members (`_TypeName`, `_ParentTypeName`,
   `Type`, `TypeId`, `TypeName`, `CreateNewObject`, `CreateInPlaceObject`, `Reconstruct`,
   `Construct`, `Destruct`) are emitted at CONSECUTIVE addresses — a macro expanded in each
   class BODY, which is what puts the virtual in each class rather than in a shared base.
4. **A second, independent 9-slot instance has the identical shape.**
   `Achievement::Achievement`, a direct `External` subclass, has its table at **0x180154740**:
   slots 1..7 in External's order (slot 4 is 0x1800066e0, External's own `Compile`, INHERITED),
   slot 8 = 0x1800012f0 `Construct`, bounded by `Object::Base`'s table at 0x180154788. So
   "External plus exactly one added virtual" is the family's general shape, not something
   peculiar to `PackageFile`.

This is also what B.3.3 already committed to for the same situation one class over:
`runtime/types/object/External.h` declares `virtual External* Construct();  // [8] 0x1800075e0`
on `Object::ExternalList` with the note "so `Construct` is ExternalList's OWN new virtual:
External's table stops at eight". `IO::PackageFile` gets the same declaration for the same
reason, and `tests/unit/io_vtable_abi_test.cpp`'s
`package_file_adds_exactly_one_virtual_at_slot_8_over_External` is the regression guard.
§6.2's row "`Object::External`'s slot-8 question | flagged, not resolved" is CLOSED by this.

`IO::PackageFile` is the **only** IO class with a `?_typeInfo@` mint row
(0x1801f9ac8, `rt_data_mint_tables.inc:4511`) — consistent with it being the only reflected
`Object::External` subclass in the namespace. It takes `DECLARE_TYPE`/`DEFINE_TYPE` when it
converts; the mint row's retirement follows B33 §7 step 7's rule (the five Object `?_typeInfo@`
rows are still blocked), so **PackageFile's row stays** and the class's record is minted, not
`DEFINE_TYPE`d, until the Object `?_typeInfo@` question is unblocked. That is not this step.

**There is no `runtime/types/io/PackageFile.h`.** Its layout (0xf0 bytes per the `RT_DELDTOR`
row) is scattered across `FileSystem.h`'s `PackageFileView` / `PackageFileEntry` and local
asserts in `package_file.cpp`. Writing that header is part of §6.1's third block, and the
class's full field list is UNKNOWN until someone reads `package_file.cpp`'s asserts in full.

### 1.5 Class names, namespaces, and the duplicate PODs

**RULES.**

1. Retail names in namespace `IO` (decision 5): `IO::Stream`, `IO::MemoryStream`,
   `IO::CompressedStream`, `IO::CompressedMemoryStream`, `IO::CompressedFileStream`,
   `IO::CompressedCopyFileStream`, `IO::FileStream`, `IO::BinaryFileStream`,
   `IO::TextFileStream`, `IO::VisitorStream`, `IO::PackageFile`, `IO::Serializer`,
   `IO::Encoding` + `IO::BinaryEncoding`/`IO::TextEncoding`/`IO::VisitorEncoding`,
   `IO::ReaderWriter` + `IO::BinaryReader`/`IO::BinaryWriter`/`IO::Visitor`,
   `IO::FileSystem`, `IO::Modification`, `IO::Persistence`, `IO::Registry`, `IO::TextCodec`.
2. **The duplicate `Stream` PODs collapse into ONE class.** `rt::io::Stream`
   (`Serializer.h:234`) and `RtIo::Stream` (`Stream.h:70`) are two independently-declared
   0x18-byte structs with the same three fields at the same offsets in two namespaces, across
   the Wave-4 lane-A/lane-B boundary. Neither header includes the other. `IO::Stream` replaces
   both. **Grep BOTH names before declaring a `Stream` caller converted** — this is the
   duplication `OBJECT_INVENTORY` found absent for Object and that does exist here.
3. **The old PODs stay, trivially destructible, until their last user converts** (B33 §7).
   `RtIo::*` / `rt::io::*` / `RtIO::*` keep their `static_assert(offsetof)` pins and are
   cross-asserted field for field against the classes, exactly as `RtObject::BaseTypeInfo` was.
   Giving a POD a destructor runs a second one — the `RtSystem::String` double-free lesson.
4. `RtObject::SerializerLite` is a FOURTH spelling of the same 0x10 bytes. §3.3 disposes of it.
5. `IO::Registry` keeps its IO namespace despite the name-adjacency to `System` — that is
   retail's placement, not a mistake to fix.

### 1.6 What this section corrects elsewhere

- `B33_RULES.md` §1.4 lists `Object::External`'s 8 slots and names `[6]`/`[7]` "by position off
  `ExternalList`'s table". **Confirmed correct**, now positively: `IO::PackageFile`'s overrides
  at those two positions are named `Reconstruct` and `Destruct`.
- `IO_INVENTORY.md` §3's "the two unexported `IO::Encoding` tables" undercounts: there are
  **THREE** unexported Encoding tables (Binary 0x1801554a8 is unexported too, and is the
  *shared* one every concrete stream's constructor installs). Corrected in place.

---

## 2. Question 2 — coexistence

**The claim.** A real `IO::X` declared per §1 keeps retail's offsets and slot order, so all
four legacy caller idioms keep working, unchanged, for the whole step:

| idiom | where | count | what it spells |
|---|---|---|---|
| `RtObject::SerializerLite` + `kEncSerialize{Bool,Uint,Int}` | `runtime/{src,types}/object` only | 16 hits, 5 files | `(*(void***)rw)[0x78\|0x98\|0xa0 / 8](rw, …)` |
| `rt::io::rw_vtbl::k*` over a real `ReaderWriter*` | `runtime/src/**` | **54 files** | the same instruction, 26 named offsets |
| `SerializerEncodingVCall(ser, off, field)` | `code/Game/**` | **37 sites, 22 files** | `ser+8` -> vtable -> `[off]`, i.e. the same again |
| `fn_*` / `g_fn_*` resolved pointers | `code/`, `engine/`, `standalone/` | ~1,100 IO CALL sites | a resolved export, not a slot call |

**The proof.** `tests/unit/io_vtable_abi_test.cpp`, nine tests, all passing. It establishes,
for the IO shapes specifically, what `object_vtable_abi_test.cpp` established for Object:

1. **Direction A** — a raw `(*(void***)rw)[rw_vtbl::kScalarInt/8]` caller on a REAL
   `ReaderWriter` with 26 distinctly-named virtuals lands on that class's override with `this`
   unadjusted, for bool/uint/int/float scalars, `kArrayInt`, `kSerializeBytes`, `kGetVersion`
   and `kDtor`. The test indexes through the SHIPPED `rt::io::rw_vtbl` constants, not a copy,
   so a header/ABI disagreement fails the test.
2. **Direction B** — a real class's ordinary virtual call on an object whose vptr was stamped
   with a hand-built `void*[26]` (the shape `rt_data_mint_tables.inc` publishes) reaches those
   free functions, including through `~Serializer` -> slot 0 with flags == 1.
3. **26 distinct slots.** Every declared virtual gets its own entry: no collapsing, no
   aliasing, no overload sharing a slot. And a `static_assert` ties the three
   `RtObject::kEncSerialize*` constants to the three `rw_vtbl::kScalar*` ones, so if B.3.4 ever
   moves one spelling and not the other the build fails.
4. **`IO::Serializer` is layout-identical to all three legacy spellings** —
   `static_assert`s on `sizeof` and both offsets against `rt::io::Serializer` and
   `RtObject::SerializerLite`, plus `is_standard_layout`. A vptr on `Serializer` would make it
   0x18 bytes and every one of the 861 `g_fn_Serializer_Ctor`-family sites would write through
   the wrong offsets.
5. **A live round trip**: an unconverted `rt::io::Stream` POD whose `encoding` is a REAL C++
   `IO::Encoding` subclass, handed to a real `IO::Serializer` whose constructor picks
   `CreateReader`/`CreateWriter` by `stream->direction` and drives a ReaderWriter wearing a
   MINTED table. That is exactly the mixed state this step lives in, end to end.
6. **The Stream family's shape**: `Stream` 0x18 with `direction` at +0x08 and `encoding` at
   +0x10; `MemoryStream` 0x30 with `data`/`ownsData`/`cursor`/`size` at
   +0x18/+0x20/+0x24/+0x28; `CompressedStream` 0x68; `CompressedMemoryStream` 0x98 with a
   `MemoryStream` **member** at +0x68 that carries its own vptr, so a raw slot call on the
   member dispatches through MemoryStream's 8-slot table and not the enclosing class's.
7. **The virtual-destructor guard**, restated for IO.

**One finding the proof forced, which is a RULE.** `IO::CompressedMemoryStream` holds an
`IO::MemoryStream` **by value**. So `IO::MemoryStream`'s non-virtual destructor must be
**PUBLIC**, not protected — the protected-dtor shape decision 2 gives a base makes the
enclosing class uninstantiable. Public, still not virtual. The same applies to
`IO::BinaryFileStream` inside `IO::CompressedFileStream` (+0x68) and to whatever
`CompressedCopyFileStream` (0xc8) aggregates.

---

## 3. Question 3 — what replaces `SerializerLite` / `rw_vtbl`, in what order, and the gate

### 3.1 The end state

`IO::Serializer` is a **non-polymorphic 0x10-byte class** whose 22 members are real `Serialize`
overloads plus `SerializeBytes`, `GetVersion`, `GetEncoding`, the `(Stream&, int)` constructor
and the destructor. Each member's body is what `serializer.cpp` already contains: read
`readerWriter->vtable[off]` and call it. **The raw slot call does not go away in this step** —
it moves from 54+22 scattered sites into 22 member bodies, which is the whole win:

- `RwSlot(rw, rw_vtbl::kScalarInt)` inside `IO::Serializer::Serialize(int32_t&, const char*)`
  is the LAST place that offset is spelled by a caller.
- It becomes `readerWriter->SerializeInt(v, name)` — a plain virtual call — only when
  `IO::ReaderWriter` itself is a real class AND nothing hands the Serializer a minted-table
  ReaderWriter any more. That second clause is not satisfied in this step (§4.2), so
  **`IO::Serializer`'s bodies keep the `rw_vtbl::k*` slot calls**, with a comment saying why.
  A faithful partial beats a complete-looking deletion.

### 3.2 The order bodies move

Strictly one direction: **receiver first, then callers.** A caller batch that converts a call
site before the receiver's class exists needs a second pass.

1. `IO::Encoding` + the three concrete encodings. Nothing calls them except `IO::Serializer`'s
   ctor — the smallest possible first move, and the one that proves the no-dtor-slot shape in a
   real build.
2. `IO::ReaderWriter` + `BinaryCodec`/`VisitorCodec` + `BinaryReader`/`BinaryWriter`/`Visitor`.
   73 vtable slots live in `serialize_family.cpp`; they become 73 member bodies across three
   classes. The 54 `rw_vtbl::k*` files keep working unchanged (§2 direction A).
3. `IO::Serializer`. The 22 forwarders become members. `serializer.cpp`'s 22
   `RegisterRuntimeExport` rows STAY — they are the door every unconverted caller still uses.
4. `IO::Stream` + `MemoryStream` + `FileStream` + `BinaryFileStream` + `Compressed*` +
   `TextFileStream` + `VisitorStream`. The duplicate PODs collapse here (§1.5.2).
5. `IO::PackageFile` (needs the new header), `FileSystem`, `Modification`, `Persistence`,
   `Registry`, `TextCodec`.
6. Only then the caller batches (§6.4).

### 3.3 `SerializerLite` — it DIES inside this step, at move 3

16 hits, 5 files, all under `runtime/{src,types}/object/`. Zero in `code/`, `standalone/` or
`engine/`. It is a redeclaration of `IO::Serializer`'s 0x10 bytes made so that
`runtime/src/object/` need not depend on `runtime/types/io/` — and once `IO::Serializer` is a
real class in one header, that reason is gone.

**RULE.** In the commit that lands move 3:
`runtime/types/object/Internal.h` includes `runtime/types/io/Serializer.h` at GLOBAL scope
(above any `namespace RtObject {`, never inside an `#if` — B31 hygiene), `SerializerLite`
becomes `using SerializerLite = IO::Serializer;` for exactly one commit so the five files build,
and the three `kEncSerialize*` constants are deleted **in the same commit** as the last body
that reads them. The five files are `Internal.h`, `internal.cpp`, `internal_class.cpp`,
`internal_ptr_array_serialize.cpp`, `external_class_doors.cpp` (the last is comments only).
The alias itself goes in §6.1's closing commit. Do **not** leave `SerializerLite` aliased past
this step: it is 16 hits and a second name for a class that now exists.

`rw_vtbl` is the opposite call: **it STAYS for the whole step**, because it is 54 files across
eleven `runtime/src/*` directories whose own classes (UI 23, Graphics 7, Video 6, Audio 3,
Achievement 3, Save 2, Object 2, Collision2D 2, Picking 1, Pathfinding 1) are B.3 steps 5-8.
The constants are retired by the class that owns each body, not by a sweep.

### 3.4 What the resave gate must show at each move

**Byte-identical to the golden, every time.** `tools/resave_gate/resave_gate.sh Gate`, and
**three consecutive PASSes** for anything touching IO — i.e. every commit in this step. The
gate script tests the DEPLOYED exe, so a no-build run tests the previous binary.

**The flake rate is ~1 in 4, measured twice independently**: on UNCHANGED main f978b983
(coordinator's baseline, 3 PASS + 1 DIFF in four runs) and again on B.3.4 block 1 (1 DIFF then
3 consecutive PASS in four runs). So the "×3" is not belt-and-braces, it is calibrated: a
single run has a ~25% chance of reporting a DIFF that means nothing.

**TWO known flake signatures. Both mean RE-RUN, not FAIL:**

1. **A ONE-byte DIFF at +0x18 with identical file sizes** — `tools/resave_gate/README.md`'s
   documented flake, and the one the coordinator's baseline hit. Re-run.
2. **THE LOADER RACE: the resave's reference table differs from the golden's by extra or
   missing RESIDENT-RESOURCE ROWS, and by nothing else** — the `ExternalFactory::Add` race
   (`reference_resave_gate_cropfield_row_flake.md`). Re-run twice.

   It presents at any magnitude, because the size delta is just "how many rows leaked times
   the length of each path". Two measured instances of the SAME signature:
   - count 5689 with exactly one extra `CropField.rsc:resource` row (the README's case);
   - B.3.4 block 1's first run: `resaved=21075154` against the golden's 21075114 (+40, i.e.
     one path row), `tail=303` against the healthy 263, `diffBytes` ~16.5M against ~16.0M.

**THE DISCRIMINATOR IS *WHICH ROWS DIFFER*, NOT THE BYTE COUNT.** A DIFF is a flake only when
the reference tables differ by whole resident-resource rows plus the id shift behind them, and
in nothing else. That is what keeps "re-run first" from degrading into "re-run until green":
re-running is how you get a second sample, not a licence to accept the third verdict because
it was the one you wanted. The coordinator is adding a row-level diff of the reference table to
the gate so a DIFF prints the extra path(s) instead of a size; until that lands, the proxy
signals below are what a self-check has, and they are a proxy, not the rule.

**STILL REAL — stop and bisect:**
- the resave is SMALLER than the golden, or differs by a size that is not a whole number of
  path rows;
- `tail=` is UNCHANGED at 263 and the bytes differ anyway;
- a multi-byte diff with identical sizes;
- the same DIFF survives a rebuild and reproduces three times.

**NEVER BISECT ON A SINGLE DIFF.** An earlier revision of this sheet said "a size difference …
is real: stop and bisect", and that rule cost a bisect: block 1's first run produced exactly
that, a detached rebuild of block 0 passed, the evidence pointed squarely at block 1 — and
block 1 then passed three times running on a rebuild of the identical tree. Re-run first; treat
a DIFF as real only when it survives a rebuild and repeats with content that does not fit the
race. "Three PASSes" means three *consecutive* clean runs: a re-run after a flake restarts the
count, and a commit that cannot produce three in a row is not mergeable however good its first
run looked.

Per move, the gate is run *after* the move and *before* the merge, and the coordinator runs it,
not the agent. A move that cannot be gated is not a move: if a change is too small to reach the
serializer it still gets one PASS, because the cost of finding out later is a corrupted golden.

Additionally, at move 2 and move 3 **only**, record the byte count of the resaved file in the
commit message. Those are the two moves that could plausibly shift a stream header, and a
recorded length is what makes a later bisect cheap.

---

## 4. Question 4 — every vptr-identity read and every minted vtable in `runtime/src/io/`

Swept across the 44 `.cpp` in `runtime/src/io/`. **One true identity comparison, five exported
stamps, one unexported stamp, and nineteen minted vtable rows.**

### 4.1 `file_stream.cpp` — `NoteSlotDispatch` + `BfsPromoteCounters`

`file_stream.cpp:367-371` declares `BfsPromoteCounters{ctorStamps, ctorStampedMint,
slotEntries, slotViaMintBfs, slotViaDllBfs, slotViaOther}`; `:390-395` is the one
`if (vptr == ours)` in the directory; `:377-380` `NoteBfsStamp` is its constructor-side twin.
Call sites, re-measured: **`file_stream.cpp:1042, 1074, 1098, 1104, 1138` and
`file_stream_win.cpp:100, 135` — seven.**

It exists because `??_7FileStream@` and `??_7BinaryFileStream@` are byte-identical in slots
1..7 (§1.3, re-verified from .rdata this pass), so a per-body counter cannot say which of the
two promoted rows dispatched. It is an **attendance counter, not a behavioural branch**, and
two of its six fields (`slotViaDllBfs`, and `ctorStamps` vs `ctorStampedMint`) have been
answering a question that no longer exists since the DLL was removed — `NoteBfsStamp`'s body
already says so ("the minted table is the only table now").

**DECISION: it dies in the commit that makes `IO::BinaryFileStream` a real class** (§3.2
move 4), and it dies WHOLE — the struct, the global, `OurBfsTable()`, `NoteBfsStamp`,
`NoteSlotDispatch`, all seven call sites and whatever prints the counters. Not left counting
zero, and not left counting a constant. B33 §5 and NEXT_SESSION §2a both pre-committed to this;
this sheet fixes the commit.

**DONE at STREAMS 2/6.** All of it: the struct, the global, `OurBfsTable()`, `NoteBfsStamp()`,
`NoteSlotDispatch()`, the seven call sites and the six-number exporter `RtIoBfsPromoteCounters()`
(which had zero callers). `FileStreamCounters` and `SlotCounters` in the same file are a
DIFFERENT instrument — plain per-body attendance, no vptr identity — and stay; their bumps moved
onto the member bodies so they keep counting entries to the BODY, not to the door.

**Why it is safe to delete rather than restate.** The question it answers —
"did this dispatch arrive through our minted `BinaryFileStream` table?" — is answered
structurally once `BinaryFileStream` is a class: there is exactly one table, the compiler's,
and `p->Type()`-style identity is the standing rule (B33 §5). There is nothing to port forward.

### 4.2 The minted vtables — **none of the 19 IO rows retires in this step**

The full NAMED/UNNAMED tally, previously UNKNOWN in `IO_INVENTORY.md` §3 and now measured
against `standalone/rt_data_mint_tables.inc`:

| class | DLL addr | slots | NAMED | UNNAMED / PURECALL |
|---|---|---|---|---|
| Binary Encoding (unexported) | 0x1801554a8 | 5 | 0 | 5 UNNAMED |
| Text Encoding (unexported) | 0x1801559d0 | 5 | 0 | 5 UNNAMED |
| Visitor Encoding (unexported) | 0x1801559f8 | 5 | 0 | 5 UNNAMED |
| `??_7ReaderWriter@IO@@6B@` | 0x1801553b0 | 26 | 0 | 26 (slot 0 UNNAMED + 25 PURECALL) |
| `??_7Stream@IO@@6B@` | 0x1801549a0 | 7 | 1 | 6 (slot 0 UNNAMED + 5 PURECALL) |
| `??_7BinaryReader@IO@@6B@` | 0x1801552e0 | 26 | 25 | 1 (slot 0) |
| `??_7BinaryWriter@IO@@6B@` | 0x180155210 | 26 | 25 | 1 (slot 0) |
| `??_7Visitor@IO@@6B@` | 0x1801558b0 | 26 | 25 | 1 (slot 0) |
| `??_7TextFileStream@IO@@6B@` | 0x180155980 | 10 | 9 | 1 (slot 0) |
| `??_7CompressedCopyFileStream@IO@@6B@` | 0x180155510 | 9 | 8 | 1 (slot 0) |
| `??_7CompressedFileStream@IO@@6B@` | 0x1801555f0 | 9 | 8 | 1 (slot 0) |
| `??_7CompressedMemoryStream@IO@@6B@` | 0x180155560 | 9 | 8 | 1 (slot 0) |
| `??_7CompressedStream@IO@@6B@` | 0x180155640 | 9 | 8 | 1 (slot 0) |
| `??_7PackageFile@IO@@6B@` | 0x180155700 | 9 | 8 | 1 (slot 0) |
| `??_7BinaryFileStream@IO@@6B@` | 0x1801554d0 | 8 | 7 | 1 (slot 0) |
| `??_7FileStream@IO@@6B@` | 0x180154960 | 8 | 7 | 1 (slot 0) |
| `??_7MemoryStream@IO@@6B@` | 0x1801555b0 | 8 | 7 | 1 (slot 0) |
| `??_7VisitorStream@IO@@6B@` | 0x180155a20 | 7 | 6 | 1 (slot 0) |
| **total** | | **231** | **138** | **93** |

**The result that decides §4.3.** Every one of the fourteen IO deleting-dtor addresses — the
eight `RT_DELDTOR` rows AND the six registered directly in `runtime/src/io/` — is the fill
address of an `RT_MINT_UNNAMED` **slot 0** in one of these rows:

| deleting dtor | mechanism | registration | fills |
|---|---|---|---|
| 0x18000c610 `IO::Stream` | `RT_DELDTOR` | `deleting_dtor.cpp` | 0x1801549a0 slot 0 (`.inc:3535`) |
| 0x180023bd0 `IO::ReaderWriter` | `RT_DELDTOR` | | 0x1801553b0 slot 0 (`.inc:3216`) |
| 0x180023fd0 `IO::CompressedStream` | `RT_DELDTOR` | | 0x180155640 slot 0 (`.inc:2015`) |
| 0x180024100 `IO::CompressedFileStream` | `RT_DELDTOR` | | 0x1801555f0 slot 0 (`.inc:1985`) |
| 0x1800244a0 `IO::CompressedMemoryStream` | `RT_DELDTOR` | | 0x180155560 slot 0 (`.inc:2000`) |
| 0x1800245f0 `IO::CompressedCopyFileStream` | `RT_DELDTOR` | | 0x180155510 slot 0 (`.inc:1970`) |
| 0x1800259d0 `IO::PackageFile` | `RT_DELDTOR` | | 0x180155700 slot 0 (`.inc:2982`) |
| 0x18002ba90 `IO::TextFileStream` | `RT_DELDTOR` | | 0x180155980 slot 0 (`.inc:3697`) |
| 0x1800242a0 `MemoryStream` | direct | `iobase_vtables.cpp:533` | 0x1801555b0 slot 0 (`.inc:2884`) |
| 0x18000c090 `FileStream` | direct | `file_stream.cpp:1328` | 0x180154960 slot 0 (`.inc:2423`) |
| 0x180023d80 `BinaryFileStream` | direct | `file_stream.cpp:1329` | 0x1801554d0 slot 0 (`.inc:1662`) |
| 0x180023a10 `BinaryReader` | direct | `iobase_vtables.cpp:539` | 0x1801552e0 slot 0 (`.inc:1676`) |
| 0x18002b710 `Visitor` | direct | `iobase_vtables.cpp:545` | 0x1801558b0 slot 0 (`.inc:3840`) |
| 0x18002bf80 `VisitorStream` | direct | `iobase_vtables.cpp:576` | 0x180155a20 slot 0 (`.inc:3872`) |

(`Modification`'s dtor 0x1800b0470, `iobase_vtables.cpp:579-581`, is the one exception — it is
a singleton destructor, not a vtable row.)

**RULE (B33 §6.1, and it binds for all fourteen).** `standalone/rt_data_mint.cpp`'s `OurPort()`
resolves an `RT_MINT_UNNAMED` slot with `RtPortLookupAddr(dllVa)`, i.e. the minted table's
slot 0 is filled **FROM** the `RT_DELDTOR` row / the direct `RegisterRuntimeAddr`. Deleting the
row or the registration turns slot 0 of a LIVE minted table into a **named abort trap** — a
build-clean, link-clean EXEC of a trap at the first `Destroy`.

**So the retirement condition is NOT "the class gained a real `DeletingDtor`". It is "nothing
wears that minted table any more".** Concretely, all four of these must be zero:
no `RT_DATA_SLOT("??_7…@IO@@6B@")` / `ResolveExport` stamp, no ctor door that stamps it,
no `RtFactOp::Vptr` replay row, no `cutover_manifest.inc` `CUTOVER_REQUIRED` entry, and no
by-address reader of the table. A class that is fully converted and still stamped is a class
whose row STAYS.

**Who still stamps each table, measured this pass** (non-generated `.cpp`/`.h` readers of the
`??_7…@IO@@6B@` symbol; `reg_mint_promoted.cpp` carries the promotion row for all but one):

| table | readers INSIDE `runtime/src/io/` | readers OUTSIDE it — these are what block retirement | promo row | CUTOVER |
|---|---|---|---|---|
| `Stream` | `compressedstream`, `encoding`, `file_stream`, `iobase_vtables` | `object/deleting_dtor_leaves`, `object/external_add`, `object/external_load`, `object/external_savetext_win`, `save/saveload_meta`, `ui/userinterface` | yes | **REQUIRED** |
| `ReaderWriter` | `binary_writer`, `encoding`, `encoding_visitor_text`, `iobase_vtables` | `object/deleting_dtor{,_internal,_leaves}` | yes | **REQUIRED** |
| `MemoryStream` | `compressedstream`, `iobase_vtables` | `save/saveload_meta`, `ui/userinterface` | yes | — |
| `FileStream` | `file_stream`, `file_stream_win` | `object/external_savetext_win` | yes | — |
| `BinaryFileStream` | `compressedstream`, `file_stream`, `file_stream_win` | `standalone/rt_data_mint.cpp` (by address) | yes | — |
| `TextFileStream` | `file_stream`, `rt_io_port`, `text_file_stream_win` | `object/deleting_dtor{,_internal}`, `object/external_savetext_win` | yes | — |
| `CompressedStream` | `compressedstream`, `compressed_stream_write` | `object/deleting_dtor{,_internal}`, `object/external_load`, `save/saveload_meta` | yes | — |
| `CompressedMemoryStream` | `compressedstream`, `compressed_stream_write`, `package_file_read_win` | `save/saveload_meta` | yes | — |
| `CompressedFileStream` | `compressed_stream_write`, `file_stream` | `object/deleting_dtor{,_internal}`, `object/external_load` | yes | — |
| `VisitorStream` | `encoding_visitor_text`, `iobase_vtables` | `object/external_add` | yes | — |
| `PackageFile` | `filesystem_find_win`, `filesystem_packages`, `package_file`, `package_file_dtor`, `package_file_serialize` | `object/deleting_dtor` | yes | — |
| `BinaryReader` | `encoding`, `iobase_vtables`, `serializer` | `ui/spritesheet_vtable` | yes | — |
| `BinaryWriter` | `binary_writer`, `encoding`, `serializer` | `ui/spritesheet_vtable` | yes | — |
| `Visitor` | `encoding_visitor_text`, `iobase_vtables` | `achievement/goal_serialize` | yes | — |
| `CompressedCopyFileStream` | `compressedcopyfilestream` **only** | **NONE** | **no** | — |

Reading that table: **thirteen of the fifteen are stamped from outside `runtime/src/io/`**, by
`Object::External`'s load/add/save-text paths, `Save`'s metadata reader, `UI`, and
`Achievement::Goal` — all of them classes that convert in B.3 steps 5-8. `IO::Stream` and
`IO::ReaderWriter` are additionally `CUTOVER_REQUIRED`. So the answer for every one of them is
the same as B33 gave for `Object::Base`: **the retirement condition is that the last of those
readers converts, which is the END of the hierarchy, past this step.** Not one of the fourteen
deleting-dtor registrations, and not one of the nineteen minted rows, may be deleted in B.3.4.

The single interesting exception is **`IO::CompressedCopyFileStream`**: the only IO table with
no reader outside its own file and no `reg_mint_promoted.cpp` row. It is therefore the one
class whose row *could* retire inside this step — and it is still not scheduled, because
retiring exactly one row buys nothing and `compressedcopyfilestream.cpp` ports only 4 of its 9
slots (the other five, including slot 0's body, live elsewhere). Recorded so the closing agent
knows it was considered and why it was declined, not skipped.

### 4.3 The stamps — `iobase_vtables.cpp`, and the ctor/dtor door question

`iobase_vtables.cpp:287-312` stamps five EXPORTED tables by name through `RT_DATA_SLOT`
(`??_7Stream@`, `??_7BinaryReader@`, `??_7ReaderWriter@`, `??_7Visitor@`, `??_7VisitorStream@`)
plus one UNEXPORTED by address (`VtblVisitorEncoding()`, 0x1801559f8). `file_stream.cpp` adds
`OurBfsTable()` (`??_7BinaryFileStream@`). No `g_*Vtable[]` array literal exists anywhere in
this directory — the IO port's "vtable work" is minted-table promotion plus body-filling, the
opposite of `code/Game`'s convention.

These are **STAMPS, not identity comparisons** (B33 §5's `goal.cpp` classification): every dtor
`iobase_vtables.cpp` defines does the retail base→derived→base restamp by hand. A real
constructor/destructor chain reproduces that for free — and **that is exactly why the ctor and
dtor doors are the dangerous ones.**

**THE KEY QUESTION, and the rule that settles it.** When OUR constructor runs, the object wears
GCC's table. When the object is built through a registered door or a factory replay, it wears
the MINTED one. B33 learned that a ctor/dtor door must NOT convert if doing so would change
which vtable a live object wears while anything still reads the minted table. Per class:

| class | who constructs it | ctor/dtor door converts in this step? |
|---|---|---|
| `IO::Encoding` + 3 concrete | only `IO::Stream`'s own concrete ctors, all inside `runtime/src/io/` (`Allocate(8,4)` then store the shared 0x1801554a8 table) | **YES** — the encoding is allocated and stamped by our own code, is never handed out, and has no minted-table reader outside the three encoding rows themselves |
| `IO::ReaderWriter` family | only `IO::Encoding::CreateReader/CreateWriter`, i.e. `encoding.cpp` + `encoding_visitor_text.cpp`, both inside `runtime/src/io/` | **YES**, with move 2 — the factory is the single construction point and it is ours |
| `IO::Serializer` | `g_fn_Serializer_Ctor` (**187 external CALL sites**) plus stack locals in `runtime/src/**` | **YES, and it is the exception that proves the rule** — `Serializer` is NON-polymorphic. It has no vptr, so converting its ctor cannot change any table. This is why it can convert while everything else waits. |
| `IO::Stream`, `MemoryStream`, `FileStream`, `BinaryFileStream`, `TextFileStream`, `Compressed*`, `VisitorStream` | see the enumeration below — stack locals stamped from `RT_DATA_SLOT`, several of them in `runtime/src/object/`; the minted rows are live (§4.2) | **NO.** Declare the classes, move the non-ctor bodies onto members, and leave the construction sites stamping `RT_DATA_SLOT("??_7…@IO@@6B@")`. They convert when §4.2's retirement condition is met — the end of B.3, not this step. |
| `IO::PackageFile` | `standalone/rt_factories_io.cpp` — two `Object::Factory` object-creation slots, both delegating to its ctor | **NO**, and emphatically: a factory replay row constructs it, so our ctor taking over would swap the table under the replay. |

**RULE, stated once so a batch prompt can carry it:** *in this step a stream class gets real
members and a real declaration; it does not get a real constructor. Every construction site
keeps stamping `RT_DATA_SLOT("??_7…@IO@@6B@")`.* A commit that gives a stream class a C++
constructor is rejected on sight.

**HOW THAT RULE IS ENFORCED BY THE COMPILER, added at STREAMS 2/6 (coordinator ruling).**
The rule above is not self-enforcing, and the gap is specific: §4.2 says leave slot 0 pure, but
a class held **by value** as a member cannot be abstract, so two of them must be concrete —
`IO::MemoryStream` (inside `CompressedMemoryStream` at +0x68) and `IO::BinaryFileStream`
(inside `CompressedFileStream` at +0x68). The shape is therefore:

> **Abstract where nothing holds the class by value; concrete-with-a-delegating-slot-0 where
> something does.** A concrete class's slot 0 delegates in one line to the deleting-dtor body
> that is ALREADY registered — nothing re-registered, re-implemented or retired.

But a concrete polymorphic class has an implicit **public** default constructor, so
`IO::MemoryStream ms;` would compile, stamp GCC's vtable on a live object, and produce **no
build error at all** — the precise failure this section exists to prevent. So every concrete
stream class carries:

1. a banner reading *"NOT CONSTRUCTIBLE IN B.3.4 — live objects are built by the ported ctor
   doors and wear the MINTED table; see B34_RULES §4.3"*; and
2. a **protected `= default` default constructor**, making `IO::X x;` and `new IO::X` compile
   errors. A probe subclass still reaches it through inheritance, and the by-value holder
   reaches it as a `friend` — which is required, because otherwise the holder's own implicit
   constructor is defined as deleted. `= default` keeps the class trivially
   default-constructible, so no layout pin moves.

The coordinator additionally greps every STREAMS merge for `IO::<X>Stream <ident>;` and
`new IO::<X>Stream` outside `tests/`. Belt and braces, because the compile error is the part
that survives a future agent who has read neither sheet.

**The construction sites, enumerated** — and the shape is not what "a `fn_*_Ctor` door" would
suggest. Retail INLINES most of these constructors, so our port reproduces them as a stack
local plus a bare vptr stamp, with no ctor door at all:

| class | allocation | who stamps the vptr |
|---|---|---|
| `IO::CompressedFileStream` (0xb8) | **stack local**, `runtime/src/object/external_load.cpp:287` `RtIo::CompressedFileStream cfs;` — the file's own comment at :21-22, :282 says the ctor is DELIBERATELY not called | `external_load.cpp:289` and again at `:458` on the inlined teardown path. Only the DTOR has a door (0x1800a9330). Retail agrees: `??_7CompressedFileStream@` is stamped inside `External::Save` (0x1800bbc63), `IsCurrent` (0x1800bbf5d) and `Load` (0x1800bc653) — i.e. retail inlines it into the External paths too |
| `IO::TextFileStream` (0x80) | **stack local**, `runtime/src/object/external_savetext_win.cpp:286` `RtIo::TextFileStream ts;` | a REAL ctor door on the local — `:299` `RT_EXPORT_ONCE(…, "??0TextFileStream@IO@@QEAA@AEBVString@System@@0H@Z")` — plus an independent re-stamp at `:362` on the teardown path |
| `IO::VisitorStream` (0x18) | stack/local inside `runtime/src/io/iobase_vtables.cpp` | `iobase_vtables.cpp`'s own ctor, which does the base→derived→base restamp by hand |
| `IO::CompressedCopyFileStream` (0xc8) | **unreached in our tree** — see below | nothing in the tree; only its own ctor/dtor in retail |
| the rest | inside `runtime/src/io/` | their own ported ctors, stamping `RT_DATA_SLOT` |

**Two of the four are in `runtime/src/object/`, not `runtime/src/io/`.** That is the fact that
makes the "no real constructors" rule non-negotiable rather than merely cautious: converting
`IO::CompressedFileStream`'s or `IO::TextFileStream`'s constructor would change the table worn
by an object built in a file that block 4's agent does not own, on the `External::Load` /
`External::SaveText` paths — and `external_load.cpp` is on the loader-race path (§0).

**`IO::CompressedCopyFileStream` is resolved as UNREACHED, not unknown.** Its constructor is
`??0CompressedCopyFileStream@IO@@` @ **0x1800a92d0**, signature
`(CompressedCopyFileStream*, CompressedMemoryStream*)` — it wraps a CompressedMemoryStream.
A Ghidra xref pass on 0x1800a92d0 returns **four DATA references (export-table entries at
0x1801b0bd8 / 0x18017ed88 / 0x18020ca00) and ZERO call sites anywhere in Runtime-x64.dll**; and
the only stampers of its vtable 0x180155510 are its own ctor, its own dtor and the two
deleting-dtor thunks. So it is exported-only: nothing in the DLL constructs one, and nothing in
our tree does either. `compressedcopyfilestream.cpp` ports 4 of its 9 slots against an object
it never sees created. **RULE: do not schedule `IO::CompressedCopyFileStream` for conversion,
and do not delete it either** — an unreached class is an unverifiable port in both directions,
and B33's rule that an uncalled port is an unverified port cuts against converting it. Record
it in STATUS as the one IO class with no live construction path.

---

## 5. Question 5 — exceptions. **CONFIRMED: there is nothing to convert in IO**

Re-verified this pass, whole-tree, non-comment, over `code/`, `runtime/`, `standalone/`,
`engine/`, `dx11/`:

- `setjmp` / `longjmp` / `__builtin_setjmp` / `__try` / `__except`: **zero** live occurrences.
  Every hit is inside a `//` comment (two reference a memory filename; five explain that
  MinGW/GCC on x64 has no usable `__try`/`__except`).
- C++ `throw` / `try {`: **zero**. The `\bthrow\b` hits are the English phrase "throw away".
- The one real exception-adjacent call in the tree is
  `standalone/main.cpp:1329 AddVectoredExceptionHandler(1, FirstChanceFailFast)` — the
  process-wide crash-log installer. Infrastructure, not IO, and it stays.

**RULE. ENDGAME §B.1 decision 7's "~25 files use setjmp/longjmp or SEH to cross retail frames"
is STALE and is not a work item for this step.** The construct it describes was forced out of
the tree when MinGW's lack of usable SEH made it unreproducible; what the project actually uses
is VEH crash handling. Nothing in `IO` becomes a C++ exception, nothing stays a longjmp, and
`B34_RULES.md` §2b.4's question is answered "none exist". A future step that wants decision 7
should re-derive the list from the RETAIL disassembly, not from the tree.

**Which retail IO functions had SEH frames our ports dropped** — not cheaply known, and not
investigated: it would need a `.pdata`/`.xdata` sweep of the shipped DLL per function. Recorded
as UNKNOWN rather than guessed. The practical consequence is nil today (no retail frame is ever
on our stack), but it is the honest answer to "what did we drop".

---

## 6. Scope and order of work

### 6.0 Honest sizing: the step runs as **4a then 4b**, in that order

ENDGAME §B.3 defines step 4 as "`IO` (Stream, CompressedStream, Serializer, Encoding,
Package), `System` rest", and that is the whole of what this step delivers. **The `System` rest
is not dropped — it is sequenced.** Measured: `runtime/src/io/` is 44 files / 22,220 lines /
~593 registration rows and eleven polymorphic classes; `runtime/src/system/` is 15 files /
8,224 lines. The caller surface behind them is ~2,870 CALL sites, of which **1,146 are the
allocator alone**.

- **B.3.4a = the `IO` namespace.** §6.1 blocks 0-5 plus §6.4 caller batches A-G, restricted to
  the `Serializer`/`Encoding`/`Stream`/`Persistence`/`FileSystem`/`Modification` pointers.
- **B.3.4b = the `System` rest**, run AFTER 4a closes and **separately gated**:
  `MemoryAllocator` (§6.5c), `Thread`/`CriticalSection` (§6.5d), `Random`, `Color`/`Value`,
  `Compression`, `Configuration`, `Timer`, `Crc`, `Error`, `WideString`. §6.5(c) and §6.5(d)
  are its design input and should be read as 4b's brief, not as a deferral.

**Why the order, and why separately gated.** The safety net for all of this is one instrument —
the resave gate — and it returns a single verdict. Put the allocator's 1,146 sites in the same
commit range as the serializer's 991 and a DIFF cannot be attributed to either. None of the
`System` rest depends on the IO design question, so nothing is lost by sequencing and the
attribution is gained. 4b may also run in parallel with 4a in a separate worktree if the
coordinator wants throughput, provided its commits gate independently.

Whether 4b can close in one session is a question for STATUS when 4a closes, not a decision to
pre-empt here.

### 6.1 The core — ONE owner, serial, three blocks

Nothing fans out until block 3 has merged. The owner keeps `serializer.cpp`, `encoding.cpp`,
`encoding_visitor_text.cpp`, `serialize_family.cpp`, `iobase_vtables.cpp`, `deleting_dtor*.cpp`
and `rt_data_mint*` for the whole step.

- **Block 1 — `IO::Encoding` + `BinaryEncoding`/`TextEncoding`/`VisitorEncoding`.**
  Files: `runtime/types/io/Serializer.h` (the `Encoding` half), `runtime/src/io/encoding.cpp`,
  `encoding_visitor_text.cpp`. ~530 lines. The no-dtor-slot shape (§1.1) is the deliverable.
- **Block 2 — `IO::ReaderWriter` + `BinaryCodec`/`VisitorCodec` + `BinaryReader`/
  `BinaryWriter`/`Visitor`.** Files: `Serializer.h` (the ReaderWriter half),
  `runtime/src/io/serialize_family.cpp` (723 lines, 73 slots), the ReaderWriter parts of
  `iobase_vtables.cpp`. The 26-name scheme (§1.2) is the deliverable. Resave gate ×3.
- **Block 3 — `IO::Serializer`, and `SerializerLite` dies.** Files:
  `runtime/src/io/serializer.{h,cpp}`, `runtime/types/object/Internal.h`,
  `runtime/src/object/{internal.cpp,internal_class.cpp,internal_ptr_array_serialize.cpp,
  external_class_doors.cpp}`. Resave gate ×3, and record the resaved byte count.

Then, still serial but parallelisable at the coordinator's discretion:

- **Block 4 — the Stream family.** `runtime/types/io/{Stream.h,FileStream.h}`,
  `runtime/src/io/{memory_stream,compressedstream,compressed_stream_*,compressedcopyfilestream,
  file_stream,file_stream_win,text_file_stream_win,iobase_vtables}.cpp`. §4.3's
  "no real constructors" rule governs this whole block. `NoteSlotDispatch` dies here (§4.1).
- **Block 5 — `IO::PackageFile` (+ a new `runtime/types/io/PackageFile.h`), `FileSystem`,
  `Modification`, `Persistence`, `Registry`, `TextCodec`.**

### 6.2 What is OUT of scope, and why

| item | why |
|---|---|
| `Save::SaveLoad` (43 sites) | different namespace, ENDGAME §B.3 item 7 |
| `fn_MemAlloc`/`fn_MemFreeStatic`/`fn_MemAllocStatic`/`fn_MemFree` (1,146 sites) | §6.5(c) |
| `System::Thread`/`CriticalSection` | §6.5(d); its caller surface bypasses `engine_funcs.h` entirely and needs its own sweep |
| retiring ANY of the 14 IO deleting dtors or 19 minted rows | §4.2 — the condition is not met |
| `IO::Serializer`'s member bodies becoming `rw->SerializeInt(...)` | §3.1 — waits on the minted-table readers |
| the `rw_vtbl::k*` constants | §3.3 — they retire with UI/Graphics/Video/Audio, steps 5-8 |
| `Object::External`'s slot-8 question | §1.4 — flagged, not resolved; re-read 0x1800252f0's callers when PackageFile converts |

### 6.3 Parallel agents, disjoint file sets

Blocks 1-3 do NOT fan out (they share `Serializer.h`). Blocks 4 and 5 can run as two agents in
separate worktrees, and their file sets are disjoint by construction:

- **STREAMS** — `runtime/types/io/Stream.h`, `runtime/types/io/FileStream.h`,
  `runtime/src/io/memory_stream.cpp`, `compressedstream.cpp`, `compressed_stream_open.cpp`,
  `compressed_stream_read.cpp`, `compressed_stream_release.cpp`, `compressed_stream_write.cpp`,
  `compressedcopyfilestream.cpp`, `file_stream.cpp`, `file_stream_win.cpp`,
  `text_file_stream_win.cpp`, `binary_writer.cpp` (12 `.cpp` + 2 headers, ~7,000 lines).
- **PACKAGE+FS** — `runtime/types/io/{FileSystem.h,Modification.h,Persistence.h,Registry.h}`,
  a new `runtime/types/io/PackageFile.h`, `runtime/src/io/package_file*.cpp` (4),
  `filesystem*.cpp` (9), `modification*.cpp` (4), `persistence*.cpp` (9), `text_codec.cpp`
  (27 `.cpp` + 5 headers, ~11,000 lines).
- `iobase_vtables.cpp`, `rt_io_port.cpp`, `rt_factories_io.cpp`, `closure_batch2b_win.cpp`
  stay with the OWNER — both agents would otherwise touch them.

### 6.4 Caller batches, by directory, with re-measured CALL counts

Directory-partitioned, disjoint by construction, all AFTER block 3. Counts from
`IO_CALLER_INVENTORY.md` §7, which used a comment/string-aware lexer; spot-checked this pass
(a line-based recount reads 187/187/303/107/138/520/610 against the lexer's
187/187/316/107/138/524/616 — the gap is lines carrying two calls, so the lexer's occurrence
counts are the ones to size a batch with).

| batch | scope | IO CALL sites |
|---|---|---|
| **A — Component** (run ALONE, FIRST) | `code/Game/Component/*.cpp` | ~1,196 total, of which ~380 are `Serializer`-family + all 35 `SerializerEncodingVCall` |
| **B — Game** | `code/Game/Game/*.cpp` | ~580 |
| **C — Dialog** | `code/Game/Dialog/*.cpp` — all of Persistence / FileSystem / Modification | ~369 |
| **D — Action + Work** | `code/Game/{Action,Work}/*.cpp` | ~402 |
| **E — Tutorial + Goal + Entity + Player** | those four directories | ~247 |
| **F — engine + standalone** | `engine/funcs/`, `standalone/` | ~33 |

**A runs alone and first**, as B.3.3 §6.4-A did: it sets the pattern the other five copy, and
its deliverable is the conversion PLUS one worked example per call shape in the commit message.
Its judgement calls are the `Serializer` ctor/dtor pairs (§7.3) and the
`SerializerEncodingVCall` sites, which are the two shapes where a mechanical rewrite is wrong.

### 6.5 The specific questions, answered

**(a) The 861 `g_fn_Serializer_Ctor/Dtor` + `g_fn_Serialize_Int/Bool` +
`fn_Serializer_Serialize_Float` sites, and the 130 over the 9 `fn_ExternalPtr_*Serialize*` /
`fn_InternalBasePtr_Serialize` / `fn_ExternalRefArray_Serialize` pointers.**

The 861 are the real bulk of the step and they are **mechanical after block 3**, because the
receiver becomes a real `IO::Serializer` and the member is a real overload set:
`g_fn_Serialize_Int(ser, &x, name)` → `Ser(ser)->Serialize(x, name)` — **with the DOOR's type
spelled at the call, never inferred from the argument (§7.3's coordinator amendment: most
arguments are untyped `s + 0xNN` pointers, and overload resolution on them would pick the
one-byte overload).** Per-site bridge cast, `// B.3.4 bridge` comment (§7.2). They split across
batches A-F by directory.

The 130 are **NOT this step's caller work.** Their receiver is an `Object::ExternalPtr<T>` /
`InternalBasePtr` / an external-ref array — classes that are already real since B.3.1 — and
what the pointers do is *serialize that receiver*. So each one becomes a MEMBER of the pointer
class taking an `IO::Serializer&`, which is a change to `runtime/types/object/External.h` /
`Internal.h`, not to 130 call sites; the call sites then follow mechanically in the same commit
per pointer. Schedule them as a **seventh batch, G**, owned by whoever owns block 3, after the
member signatures exist. `fn_ExternalPtr_Serialize3` is the **last reader of exe 0x140007140**
— when it dies, that exe address has no reader and the closing pass drops its row; say so in
the commit so the closing agent does not have to re-derive it.

**(b) The two aliased-slot pointer pairs — collapse these FIRST, before batch A.**

Two typedef'd pointers name the same retail dispatch slot, so two batches could independently
"finish" the same member and the second would silently re-register the slot:

| slot | pointers | CALL | action |
|---|---|---|---|
| 0x140152ac8 | `g_fn_Serialize_Int` (316) **and** `fn_Ser_Slot_SerInt_AC8` (28) | 344 | convert the 28 `fn_Ser_Slot_SerInt_AC8` sites (mostly `code/Game/Component/tradecomponent_serialize.cpp`) to `g_fn_Serialize_Int`, then delete the `fn_Ser_SerInt_t` typedef, extern, initialiser and its guard conjunct in `code/Game/Game/gamegrid_serialize.cpp` |
| 0x140152a60 | `fn_Serializer_Serialize_Float` (138) **and** `fn_Ser_Slot_SerFloat_A60` (**0**) | 138 | `fn_Ser_Slot_SerFloat_A60` is already zero-CALL — delete it outright with the other zero-callers, (f) |

This is a small, separately-gated commit that belongs to the owner, before batch A starts. It
is the one piece of caller work that must NOT wait for block 3.

**(c) `fn_MemAlloc` (524) / `fn_MemFreeStatic` (616) — DEFERRED, out of B.3.4 proper.**

Three reasons, in order of weight:

1. **They are allocator DOORS, and the door design is already settled elsewhere.** B.3.2 landed
   `rtcoll::DoorAlloc` (`runtime/src/collection/alloc_door.cpp` — "it contains no allocator, it
   answers with the function to call, reading the exe dispatch slot through `ResolveAddr`") and
   `tests/unit/alloc_door_test.cpp` drives it. `System::MemoryAllocator` becoming a real class
   has to reconcile with that door and with the redzone instrumentation, which is a design
   conversation, not a caller sweep. Doing it inside a step whose safety net is the resave gate
   means an allocator regression shows up as a serializer diff.
2. **There are THREE independent MemAlloc surfaces**, not one: the member form
   (`fn_MemAlloc`, via `g_pMemAllocSingleton`), the static form (`fn_MemAllocStatic`,
   `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`), and a per-file `s_fn_MemAllocSA` cache of
   the *same* static export in three `code/Game` files that never goes through
   `engine_funcs.h`. A batch that only sees `engine_funcs.h` would report "done" with the third
   surface untouched.
3. **1,146 sites is more than the whole IO caller surface**, and it has zero dependency on the
   IO design question — so it is free to run as its own step at any time, including in parallel
   with B.3.4 if the coordinator wants early throughput. `IO_CALLER_INVENTORY.md` §7's closing
   note says the same.

**(d) `System::Thread` / `CriticalSection` — their caller surface is invisible to
`engine_funcs.h`, and that is the finding.**

Five `code/Game` files resolve these by mangled name into file-static `s_fn_*` caches with no
global `fn_*` pointer at all: `action_pf_astar.cpp` and `keystone_a_zone.cpp` (`??0Thread@`,
`??1Thread@`, `?Start@Thread@`, `?WaitForFinish@Thread@`), `pathfinder_seed.cpp`,
`tornado.cpp`, `keystone_a_gamedtor.cpp` (`??0CriticalSection@`, `??1CriticalSection@`,
`?Lock@`, `?Unlock@`). Plus `standalone/{loader_loop_stubs,reg_mint_promoted,rt_data_mint}.cpp`
in resolver context.

**RULE for whoever takes `System::Thread`: scope the batch by sweeping
`ResolveExport("…@System@@…")` and `ResolveExport("…@IO@@…")` DIRECTLY, never by reading
`engine_funcs.h`.** An `engine_funcs.h`-anchored sweep reports zero callers for eleven of the
thirteen System classes (`IO_INVENTORY.md` §8 hit exactly this and flagged it honestly), and
"zero callers" is the single most dangerous wrong answer in this whole phase — it is what
authorises a deletion. This also means `IO_INVENTORY.md` §8's caller table cannot be used to
size anything; it is corrected in place to say so.

**(e) `Internal::Clone` and `ExternalFactory::SerializeReferenceTable`.**

`?Clone@Internal@Object@@QEAAPEAV12@XZ` (0x1800bfb10) is genuinely UNPORTED — no registration
by name or address, no stub, no TODO; the only tree reference is `internal_class.cpp:30-37`'s
own comment. Its stated blocker was `IO::MemoryStream`, and **that blocker is already
satisfied**: `IO::MemoryStream` has a real body in all 8 vtable slots
(`iobase_vtables.cpp` slot 0 + `memory_stream.cpp`'s six + `compressedstream.cpp:657`'s dtor).
**DECISION: port it in block 4, as its own small commit**, not as a prerequisite for anything.
It is a serialize round-trip deep copy (a 0x1000-byte MemoryStream pair, the Factory descent,
`IsDerivedFrom`, then slot 3 both ways); its Factory-descent dependency is real and satisfied
since B.3.3. Its resave-gate exposure is nil until something calls it, so it gates on `make
unit` + one gate PASS. `IO_INVENTORY.md` §9.3's correction — that `MemoryStream` was never a
blocker — stands.

`?SerializeReferenceTable@ExternalFactory@Object@@QEAAXAEAVStream@IO@@@Z` (0x1800bcb60) is
**fully ported already**, as `RtObject_ExternalFactory_SerializeReferenceTable(ExternalFactory*,
Stream*)` (`external_reftable.cpp:192-193`, registered by name and address at :438-441).
**The discrepancy with NEXT_SESSION §2a resolves as follows, and NEXT_SESSION was not wrong so
much as imprecise.** Its second parameter is `Stream*` where `Stream` is `rt::io::Stream`
(`external_reftable.cpp:124 using rt::io::Stream;`, from `Serializer.h:234`) — which is the
**lane-A DUPLICATE POD**, not `RtIo::Stream` from `Stream.h:70` and not a real class. So "its
2nd parameter is a file-local struct today" is off by one level of scope (it is
header-local to `Serializer.h`, not file-local to `external_reftable.cpp`) but names a real
problem, and it is the problem §1.5.2 fixes: the two PODs collapse into `IO::Stream` and the
signature becomes `SerializeReferenceTable(IO::Stream&)` as a member of `Object::ExternalFactory`.
That happens in block 4 (when `IO::Stream` exists), and the file must be touched with §0's
loader-race warning in the prompt, because `external_reftable.cpp` is on that path.
`IO_INVENTORY.md` §9.2's "does not match" is softened in place to this.

**(f) Zero-CALL pointers for the closing pass.** Re-measured non-comment this pass; all
thirteen have CALL = 0 and only DECL/guard occurrences:

`fn_Persistence_SetBool`, `fn_Persistence_SetInt`, `fn_ModEntry_CopyCtor`, `fn_ModEntry_Assign`,
`fn_ModEntry_Dtor`, `fn_ModArrayCopy`, `fn_Struct60B_Serialize`, `fn_TradeWork_impl_04d0`,
`fn_TradeWork_impl_f1a0`, `fn_MemZeroFill`, `fn_Ser_Slot_SerFloat_A60`,
`fn_Ser_Slot_SerUShortArr_488`, `fn_Ser_Slot_SerByteArr_618`.

They die **with their typedefs, externs, definitions, initialisers, `_DEBUG` value prints and
guard conjuncts**, by NAME and by retail ADDRESS (B33 §7). Three of them
(`SerFloat_A60`, `SerUShortArr_488`, `SerByteArr_618`) appear only inside
`code/Game/Game/gamegrid_serialize.cpp`'s big `bool ok = fn_A && fn_B && …` readiness chain —
deleting a conjunct from a multi-line expression is where B.3.3 learned that removing a
statement inside a multi-line `#define` eats the previous line's `\`. Check the line after.
`fn_MemZeroFill` has zero occurrences anywhere including guards.

**Caution the closing agent gets in its prompt:** a name-based lexer cannot see an indirect call
through a function-pointer table. `fn_ModEntry_*`/`fn_ModArrayCopy` support an
`Array<Mod>`-copy path; before deleting those four, grep `runtime/src/io/modification*.cpp` for
a by-address route to the same retail bodies.

**(g) The 14 exe dispatch slots — THEY STAY REGISTERED.**

`standalone/address_registry.cpp:1628-1645` binds fourteen rows to `RtSerializer_*`:
0x140152ac8 (int), 0x140152a60 (float), 0x140152a58 (bool), 0x140152410 + 0x140152418 (uchar,
the second a NameDB alias), 0x140152a50 (uint), 0x140152a48 (GetVersion), 0x140152488
(ushort[]), 0x140151278 (int[]), 0x1401523f8 (short[]), 0x140152490 + 0x140152400 (uchar[]),
0x140152618 + 0x140152478 (SerializeBytes).

**These are PE IMPORT SLOTS.** Retail-shaped exe code reads them; they are not our call sites
and they do not retire when a caller converts. They stay bound for the whole of B, and
`tools/dispatch_slot_map.py --check` must stay at zero mismatches across every commit in this
step. Converting `IO::Serializer` to a class changes the *target's* spelling, never the row.

---

## 7. The gate, the standing rules, and the worked call shapes

### 7.1 The gate for every merge (coordinator only) — NEXT_SESSION §2d verbatim

debug + release + perf build exit 0 · `make unit` · `sa-run` + pause-menu quit ·
`BANISHED_AUTOTUTORIAL=1` **with** `BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120` (it
never self-quits otherwise) · the Load Game dialog route · **resave gate PASS, ×3 for anything
touching IO** (i.e. every commit in this step) · live-boot type table vs the previous boot ·
`[RtResolve] 0 misses`, no `[RtMint] TRAP` · `tools/dispatch_slot_map.py --check` 0 · 0 GATE
WARN · `git diff --stat` against the agent's declared file set before merging · at the step
close also the Mods route with `.pkm` mounted (move them back) and one functional
`make sa-stress`.

`tools/runtime_census.py` additionally, whenever a registration moved — every registration must
stay a LITERAL address or the census goes blind to it.

### 7.2 Standing rules every batch prompt carries (NEXT_SESSION §2c, restated)

- **Counts are TOKENS until proven otherwise.** Re-measure with a comment/string-aware lexer
  before sizing a batch. This step's own corrections: 159 → 130, 50 → 54 files, 39 → 37 sites.
- **A ctor/dtor door does NOT convert if the member form would change which vtable a live
  object wears.** In IO that is every stream class — §4.3.
- **Guards on a pointer a batch stops calling STAY** until the closing pass removes guard and
  pointer together.
- **A call inside a `printf`/`fprintf`/`DLOG`/`TRACE` argument list does not convert** — the
  perf build strips printf arguments, and seven live ports were once invisible for this.
- **Includes at global scope**, above any `namespace X {` forward declaration, and NEVER inside
  an `#if`. Agents build `standalone-debug`, `standalone`, `standalone-perf` AND `unit`.
- **Deleting a statement inside a multi-line `#define` can eat the previous line's `\`.**
- **`.inc` files are regenerated, never hand-edited.**
- **Never `getenv`; no new `BANISHED_*` flag; never "fix" a faithful crash with a null check;**
  never `find /` or an unscoped search. Greps over `standalone/` or `tests/` exclude
  `standalone/build*/` and `tests/unit/build/`, anchored on a path separator.
- **Sub-workers report to the coordinator too**; the owning agent integrates and verifies the
  final DIFF (removed-line scan), not the reports.
- **`make sa-stress` is a functional check while B is open** — record the number, never chase
  it.
- **THE GREP HAZARD** (B33 §6.5, re-armed for this step): a file that opens `namespace IO {` and
  then writes a bare `Stream` / `Serializer` / `Encoding` / `ReaderWriter` / `FileStream` now
  binds it to the REAL CLASS, silently. **Inside `namespace IO`, always spell the POD
  `RtIo::X` / `rt::io::X` / `RtIO::X`.** And remember there are two `Stream` PODs (§1.5.2).

### 7.3 Worked call shapes

```cpp
// --- scalar serialize: 861 sites, batches A-F -----------------------------
g_fn_Serialize_Int(ser, &x, "name");
// becomes, with the one bridge cast the receiver still needs:
reinterpret_cast<IO::Serializer*>(ser)->Serialize(x, "name");   // B.3.4 bridge

g_fn_Serialize_Bool(ser, &b, nullptr);
reinterpret_cast<IO::Serializer*>(ser)->Serialize(b, nullptr);  // B.3.4 bridge
// `Serialize` stays ONE name on the Serializer (§1.2.2) while the 22 VIRTUALS
// it forwards to get 22 distinct names (§1.2.1).
//
// *** COORDINATOR AMENDMENT (2026-09-19, after block 3) — THE DOOR DECIDES THE OVERLOAD,
// *** NEVER THE ARGUMENT'S DECLARED TYPE. An earlier revision said "overload resolution
// *** picks the width from the reference's type". That is WRONG FOR THIS TREE and would
// *** corrupt saves: the old doors take `void*`, and measured on main the argument is
// *** usually an UNTYPED byte-offset pointer — `s + 0x28` (13 sites), `At(self, …)` (13),
// *** `(void*)(s + 0x30)` … — not a typed field. `*(s + 0x28)` is an `unsigned char&`, so
// *** overload resolution would pick Serialize(unsigned char&) (slot +0xB8, ONE byte) where
// *** retail's call was the int door (slot +0xA0, FOUR bytes). Even a TYPED field can sit
// *** behind the "wrong" door on purpose (a uint32_t or an enum serialized through the Int
// *** door): letting its type choose moves the call to a different retail slot.
// *** RULE: spell the door's own type at the call, always:
g_fn_Serialize_Int(ser, s + 0x28, nullptr);
reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);  // B.3.4 bridge
// ***   g_fn_Serialize_Int            -> int&            (+0xA0)
// ***   g_fn_Serialize_Bool           -> bool&           (+0x78)
// ***   fn_Serializer_Serialize_Float -> float&          (+0x80)
// ***   any other door                -> the type in ITS mangled name / IO::Serializer's
// ***                                      declaration comment (each overload's comment in
// ***                                      runtime/types/io/Serializer.h names its +0xNN slot)
// *** The cast may be dropped ONLY when the argument expression's type is ALREADY exactly the
// *** door's type (`int count; … &count` through the Int door). `int32_t` is `int` on this
// *** toolchain; `long`/`DWORD`/an enum/`uint32_t` are NOT `int` — cast. A batch reports, per
// *** door, how many sites needed the cast; "0 needed" on a file full of `s + 0xNN` is a lie.

// --- the encoding v-call: 37 sites, 22 files, batch A ----------------------
SerializerEncodingVCall(serializer_24bytes, 0x98, &field);
//                                          ^^^^ rw_vtbl::kScalarUInt
reinterpret_cast<IO::Serializer*>(serializer_24bytes)
    ->Serialize(*reinterpret_cast<unsigned int*>(&field), nullptr);   // 0x98 = uint door
// (same amendment as above: the OFFSET names the door, the door names the type, and the
//  type is spelled at the call — never inferred from `field`'s declaration.)
// ONE RULE HERE: the offset argument is the WIDTH. Map it through §1.2's table
// (0x78 bool, 0x80 float, 0x98 uint, 0xA0 int, ...), never by eye. A site whose
// offset is not in that table is not a serialize call — stop and report it.

// --- the ctor/dtor pair: 187 + 187 sites, batches A-F ---------------------
// THE DEFAULT, AND IT IS THE ONLY SHAPE A CALLER BATCH MAY USE.
// Measured: every exe-side site constructs into a RAW BYTE BUFFER, never a
// typed local — `uint8_t ser1[24]`, `uint8_t serializer[32]`. So the conversion
// is placement-new into the SAME buffer with an explicit destructor at the SAME
// position, which is exactly what B.3.3 did for the 815 fn_InternalBasePtr_Ctor
// sites. 1:1 per call, no lifetime judgement, and a raw buffer has no implicit
// destructor, so nothing can run twice.
alignas(8) uint8_t ser1[24];                    // <- alignas ADDED in the same edit
g_fn_Serializer_Ctor(ser1, stream, 0);
...
g_fn_Serializer_Dtor(ser1);
// becomes:
alignas(8) uint8_t ser1[24];
::new (ser1) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
...
reinterpret_cast<IO::Serializer*>(ser1)->~Serializer();                   // B.3.4 bridge
```

**`x.~Serializer()` ON A TYPED LOCAL IS FORBIDDEN**, and the typed stack-object form
(`IO::Serializer ser1(stream, 0);`) is **not available to a caller batch at all** — it is
optional polish for the receiver's own class conversion in B.3 steps 5-8. The reason is the
`RtSystem::String` double-free trap in a new costume: on a typed local, an explicit destructor
call plus the implicit one at scope exit destroys the object **twice**, and
`IO::Serializer::~Serializer` is **not idempotent** — it reads 4 more bytes on the read side
and back-patches the placeholder on the write side. An early-return path is exactly where an
agent would reach for that shape, and the resave gate would report it as "the save changed"
with no pointer to the site.

**Why the order matters at all.** Constructing an `IO::Serializer` is not a cheap wrapper
construction:

- `BinaryReader::BinaryReader` (0x1800a8340) calls the stream's `Read` three times with
  count 4 and slot +0x28 once — it **consumes a 12-byte header**.
- `BinaryWriter::BinaryWriter` (0x1800a8a30) calls `Write` three times with count 4 and slot
  +0x28 twice — it **emits a 12-byte header** and records a back-patch position.
- `~BinaryReader` (0x1800a82f0) **reads a further 4 bytes**; the writer's destructor patches
  the placeholder.

So **destruction ORDER and POSITION relative to other stream operations are load-bearing**, and
placement-new preserves both by construction: each call converts in place, and a path that
skips the `Dtor` today still skips it (faithful, not "fixed").

**THE `alignas(8)` RULE.** The buffers are `uint8_t[24]`, alignment 1. Placement-new of an
8-aligned `IO::Serializer` into an under-aligned buffer is undefined behaviour even though x64
stack slots happen to be 16-aligned in practice — correct today by accident, one inlining
decision away from being wrong. So `alignas(8)` goes on the buffer declaration **in the same
edit as the placement-new**. It changes no offset and no code, only the slot's alignment
guarantee. **Two exceptions, both of which are a REPORT and not a conversion:**

1. **The buffer is a MEMBER of a layout-asserted struct, or an element of a larger byte array
   carved up by offset.** `alignas` there could move a field. Do not add it; report the site.
2. **The buffer is SMALLER than 0x10 bytes.** Stop and report — `IO::Serializer` is 16 bytes
   and a smaller buffer means the site was never what it looked like. (A buffer LARGER than
   0x10, which `[24]` and `[32]` both are, is fine: the object simply does not fill it.)

The survey found every site is a plain local, so expect zero reports — but a batch that finds
one must not guess.

---

## 8. Corrections made to the inventories (applied in place)

`IO_INVENTORY.md`:
1. §3 — there are **three** unexported `IO::Encoding` tables, not two; Binary 0x1801554a8 is
   unexported as well and is the shared one.
2. §3 — the `RT_MINT_NAMED` / `RT_MINT_UNNAMED` split, left UNKNOWN, is now tabled (§4.2 here):
   231 slots, 138 NAMED, 93 UNNAMED/PURECALL.
3. §4 — `rw_vtbl::` users: **54 files**, not 50; `runtime/src/io` (3) was omitted and ui is 23
   not 21, video 6 not 5.
4. §8 — the `fn_<Class>_*` caller table for the System rest is a NAMING ARTEFACT and must not
   be used to size a batch; the real surface is `ResolveExport("…@System@@…")` (§6.5d).
5. §9.2 — the `SerializeReferenceTable` discrepancy is resolved (§6.5e): the 2nd parameter is
   the lane-A DUPLICATE POD `rt::io::Stream`, which is the thing this step collapses.
6. Surprise 6 — `IO::PackageFile`'s `?_typeInfo@` row is not orphaned: PackageFile is an
   `Object::External` subclass (§1.4) and the row is live.

`IO_CALLER_INVENTORY.md`:
1. §4 — `SerializerEncodingVCall`: **37** call sites in 22 files, not 39 (the 38th hit is the
   inline definition in `engine_funcs.h`).
2. §6 — the dispatch-slot table lists 15 rows; the `RtSerializer_*` binding is **14 rows** at
   `address_registry.cpp:1628-1645`, and they are PE import slots that STAY (§6.5g).
3. §2f/§8.3 — the two aliased pairs get an explicit disposition (§6.5b): collapse
   `fn_Ser_Slot_SerInt_AC8` into `g_fn_Serialize_Int` before batch A;
   `fn_Ser_Slot_SerFloat_A60` is already zero-CALL and dies with the other twelve.
