# WAVE 11 lane OBJDATA — the ten minted Object DATA rows

Worktree `C:/code/GIT/wt-wave11-objdata`, branch `wave11/objdata`, cut from `fa4fa86`.
Every number below was measured on this branch with the DLL present, debug build,
under `tools/game_dir_lock.sh acquire objdata`.

## 0. Verdict table

| # | row | kind | verdict | the measurement behind it |
|---|---|---|---|---|
| 1 | `?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A` | BaseTypeInfo | **PROMOTED** | READY at exit-phase shadow; consumer runs and A/B is identical (§5) |
| 2 | `?_typeInfo@External@Object@@0VBaseTypeInfo@2@A` | BaseTypeInfo | **PROMOTED** | READY, `idcmp=yes`, eager consumer ordering verified (§5) |
| 3 | `??_7Base@Object@@6B@` | vtable | **READY-BUT-UNPROMOTABLE** (NO-OP) | abstract class; accessor in exactly one vtable; `entries=0` both halves (§2) |
| 4 | `??_7External@Object@@6B@` | vtable | **READY-BUT-UNPROMOTABLE** (NO-OP) | as above, 4 purecall slots (§2) |
| 5 | `??_7CompiledExternal@Object@@6B@` | vtable | **READY-BUT-UNPROMOTABLE** (NO-OP) | as above, 4 purecall slots (§2) |
| 6 | `??_7Internal@Object@@6B@` | vtable | **READY, NOT PROMOTED** (live expiry) | concrete class, so §2's proof does NOT apply; `entries=0` (§2) |
| 7 | `?_singleton@Factory@Object@@0PEAV12@EA` | storage | **UNSAFE** | `minted=zero` all run; 13 unported/unknown reads (§3) |
| 8 | `?_singleton@ExternalFactory@Object@@0PEAV12@EA` | storage | **UNSAFE** | `minted=zero`; 143 of 151 refs unported/unknown (§3) |
| 9 | `?_singleton@InternalFactory@Object@@0PEAV12@EA` | storage | **UNSAFE** | `minted=zero` — and this **contradicts the brief** (§3) |
| 10 | `?_firstTypeInfo@BaseTypeInfo@Object@@2PEAV12@EA` | chain head | **UNSAFE** | both heads written; promotion splits a 416-entry chain (§4) |

## 1. The brief's first sentence was wrong, and it cost the lane its first hour

The brief said all ten rows were already generated into `rt_data_mint_tables.inc`
so "your work is *promotion*, not authoring". True of rows 1–2 and 7–10. **False of
rows 3–6.** A `BANISHED_RT_MINT_SHADOW=1` boot on `fa4fa86` reported all four
vtable rows BLOCKED:

```
BLOCKED  ??_7Base@Object@@6B@              slots= 3 ported= 1 TRAP= 1 purecall= 1
BLOCKED  ??_7CompiledExternal@Object@@6B@  slots= 8 ported= 2 TRAP= 2 purecall= 4
BLOCKED  ??_7External@Object@@6B@          slots= 8 ported= 2 TRAP= 2 purecall= 4
BLOCKED  ??_7Internal@Object@@6B@          slots= 4 ported= 2 TRAP= 2 purecall= 0
```

Seven traps, all of them `?Type@` / `?TypeName@` accessors, and a grep by mangled
name **and** by address found no body for any of them anywhere in the tree. They
are now `runtime/src/object/object_typeinfo_accessors.cpp` (switch
`BANISHED_RT_OBJTYPEINFO`), and the same screen reads:

```
READY    ??_7Base@Object@@6B@              slots= 3 ported= 2 TRAP= 0 purecall= 1
READY    ??_7CompiledExternal@Object@@6B@  slots= 8 ported= 4 TRAP= 0 purecall= 4
READY    ??_7External@Object@@6B@          slots= 8 ported= 4 TRAP= 0 purecall= 4
READY    ??_7Internal@Object@@6B@          slots= 4 ported= 4 TRAP= 0 purecall= 0
```

Each accessor is two instructions and every one reads a
`?_typeInfo@…@Object@@0VBaseTypeInfo@2@A` node at +0x00 (typeId) or +0x08 (name);
`?Type@Base@` is `XOR EAX,EAX / RET`, because `Object::Base` is the root and has no
node. The three node addresses the decode produces (`0x1801f9b00`, `0x1801f9b38`,
`0x1801f9c18`) are exactly the VAs the generator already carries for those rows,
so the field offsets are checked against the generated table and not only against
the disassembly.

## 2. Gate (b) for the four vtable rows — and one instrument that lied first

`[ObjTI] ATTENDANCE` counts entries per row **and classifies which table each
entering object carried** (ours / the DLL's / a third). A plain entry tally would
have measured the *bodies*, which register unconditionally, and so would never
vary with the *row* — the defect Wave 10D found in `[DlgCtrl] ATTENDANCE` after
that counter had already retired a gate.

**The first version of this instrument was silently inert, and its own screen0
caught it.** It printed from a static destructor, and by static-destructor time the
port registry is torn down: `ResolveExport` and `ResolveExportRaw` return the same
DLL address for *every* symbol. Every row read `row=unregistered(DLL)` and the
armed half of the A/B was indistinguishable from the control half — which would
have been written up as "the row changes nothing". Screen0 asks the doors about
`??_7Element@UI@@6B@`, which is promoted unconditionally and therefore *cannot*
be unregistered on any run; it printed RED, and the report was moved to an
explicit call from `standalone/main.cpp`'s shutdown block. It now prints GREEN.

The A/B (autostart → gameplay → pause-menu quit, one variable,
`BANISHED_RT_MINT_ROWS` with the four vtable rows plus the two typeinfo rows):

| | control | armed |
|---|---|---|
| screen0 (registry live) | GREEN | GREEN |
| control screen1 (fires) | GREEN, `d_entries` = calls, 4/4 rows | GREEN, 4/4 |
| control screen2 (discriminates) | GREEN, `d_dll` = calls, 4/4 | GREEN, `d_mint` = calls, 4/4 |
| real attendance | `entries=0`, 4/4 rows | `entries=0`, 4/4 rows |
| crash log | 0 bytes | 0 bytes |
| `[RtDiff]` non-zero divergences | 0 | 0 |

So the instrument fires, discriminates, and reports zero. `entries=0` is a finding,
not a silence.

**Why three of the four are permanently unpromotable.** `??_7Base`,
`??_7External` and `??_7CompiledExternal` carry PURECALL slots (1, 4 and 4), so
each class is **abstract** and no leaf instance can exist; a base table is the live
`+0x00` only between the base constructor and the derived one, and no virtual call
is made in that window. Two binary measurements close it:

* Each of the seven accessors appears in **exactly one vtable** in the whole of
  `Runtime-x64.dll` — its own base row. Every derived class overrides both, as it
  must to carry its own typeId. No derived table can dispatch into these rows.
* **No qword in any of the four shipped binaries holds any of the four table
  addresses.** The only writer of one is an in-DLL RIP-relative `LEA`, which is
  not a door — and the exe imports zero `??_7`.

`??_7Internal@Object@@6B@` is deliberately **not** tagged with them: it has **zero
purecall slots**, so `Object::Internal` is concrete and a bare instance genuinely
could carry this table. Nothing this lane could drive constructs one. Its expiry is
live and prints on every boot: `[ObjTI] ATTENDANCE ??_7Internal@Object@@6B@` with
`entries>0`.

## 3. The three `_singleton@` rows — the brief's tier is right, its reasoning is not

Wave 15 phase 2's screen ("of this row's in-DLL references, how many are READS in
bodies we have NOT ported?", `docs/runtime_port/wave15/rt_singleton_refs.csv`)
makes the three look completely different:

| row | refs | ported+registered | unported | unknown-owner |
|---|---|---|---|---|
| `ExternalFactory@Object` | 151 | 10 | 14 | 127 |
| `Factory@Object` | 20 | 7 | 11 | 2 |
| `InternalFactory@Object` | 6 | 5 | **1** | 0 |

`InternalFactory`'s single unported reader is
`?Get@InternalFactory@Object@@SAPEAV12@XZ`, and this lane proved it **dead** by
three independent routes: Ghidra `get_xrefs_to` returns only the export-address
table; a rel32 `E8`/`E9` scan at every byte of `.text` finds zero call sites; a
whole-image qword scan finds zero pointer-valued references; and it is not among
the 899 / 163 / 146 named imports of the exe, DX11 or DX9. **Five ported refs plus
one dead reader is exactly the shape that made `?_singleton@SaveLoad@Save@@` safe.**
On that screen alone, InternalFactory should have been promoted.

**It is unsafe anyway, and one line of the exit-phase shadow says why:**

```
storage ?_singleton@InternalFactory@Object@@  DLL=WRITTEN   minted=zero
storage ?_singleton@Factory@Object@@          DLL=WRITTEN   minted=zero
storage ?_singleton@ExternalFactory@Object@@  DLL=WRITTEN   minted=zero
storage ?_singleton@SaveLoad@Save@@           DLL=still zero minted=zero
```

The three Object factories **are** constructed on every boot, and the constructor
that does it is reached by an in-DLL `rel32` from `Engine::Startup`, which is not a
door. Our ported ctors (`runtime/src/engine/singleton_ctors.cpp`) publish through
`RT_DATA_SLOT` and *would* follow a promotion — but they never run, so the minted
storage stays zero for the whole process. Promoting any of the three hands every
one of our readers a **null factory pointer** while the live factory sits in the
DLL's slot: the `?_singleton@MemoryAllocator@System@@` failure mode that makes the
global `BANISHED_RT_MINT=1` switch fatal, reached by a different road. `SaveLoad`
is safe *precisely because its DLL slot is also still zero* — nothing constructs
one on this route, so promoting it moved nothing.

> **The general lesson, which is not written down anywhere else.** The
> reference-ownership table is **necessary and not sufficient**. It answers "who
> READS this slot". A storage row also has to answer "does anything ever WRITE
> OUR copy". A row whose only writer is an in-DLL call to a ctor we have ported
> but nothing invokes is unsafe no matter how its reads are distributed. **Read
> the `minted=` column of the exit-phase shadow before the reference CSV.**

## 4. `?_firstTypeInfo@BaseTypeInfo@Object@@` — the highest-risk row, and a no

Unlike the three factories, **both** copies are written every run
(`DLL=WRITTEN  minted=written`), which is worse rather than better: the two heads
then hold different chains. A decoder-independent disp32 scan of `.text` (every
byte offset × trailing immediates {0,1,2,4}) finds the references cluster in one
shape — ~104 read/write pairs at `0x180147xxx`–`0x18014axxx` on a regular stride
(the inlined `??0BaseTypeInfo@Object@@` in one static initialiser per type, each
linking itself onto the head), the out-of-line ctor at `0x180006320`, and
`Factory::Startup`'s own read at `0x1800c065e`. Every one is an unredirectable
RIP-relative access to the DLL's cell.

Our side already writes ours: `[FactoryReg] Registered 312 types into Runtime
_firstTypeInfo`, `[GameMain] Post-Startup _firstTypeInfo: 416 entries, 416 have
non-FF typeId`. Today those are the *same* cell, because the row is unpromoted and
the resolver hands us the DLL's — which is what keeps one chain with 416 entries
in it. The brief called this a "known split brain"; the measurement says the
opposite. **Resolving by name is what prevents the split**, and promoting is what
would cause it: our 312 in one head, their 104 in the other, with retail's
`Factory::Startup` walking the half that does not contain the exe's types.

A decision not to promote, with the mechanism written down, is the result here.

## 5. The two promotions

Both rows are READY on the **exit-phase** shadow, in the run where the DLL's own
`Factory::Startup` has executed: `BaseTypeInfo: 624 field comparisons, 0 MISMATCH,
0 typeId not-yet-comparable` and `typeinfo rows: 104 READY, 0 BLOCKED`. At
phase=init the same two read `idcmp=NO`, which is the DLL still holding the ctor
sentinel — reading the init snapshot as the verdict would have blocked both rows
for nothing.

**Ordering, which is the failure mode this pair could have had.** Both have an
eager consumer that resolves the node once and republishes the pointer:
`standalone/reg_unreg_lane1.cpp:128` fills exe import slot `0x140152870` with
`ResolveExport("?_typeInfo@External@…")`. A promotion landing *after* that call
would leave the exe's readers on the DLL's node and ours on the minted one. It does
not: `RuntimePortInit()` (`standalone/main.cpp:1734`) contains both the mint and
the promotions, and `AddressRegistryInit()` is at `:1755`. Checked, not assumed.

`?_typeInfo@ExternalList@Object@@` has gate (b) satisfied by a route that actually
runs — the retail type gate in `code/Game/Game/gameupdateloop_load.cpp:281,355`,
which reads the typeId and feeds `IsDerivedFrom`:

```
armed    [GUL::DiseaseList] isExternalList=1 diseases=12
         [GUL::Help]        isExternalList=1 topics=66
control  [GUL::DiseaseList] isExternalList=1 diseases=12
         [GUL::Help]        isExternalList=1 topics=66
```

Same verdict and same counts both sides; only per-run heap addresses differ. The
gate is exercised **and** the promotion is neutral — the pair of facts
`??_7DialogController@UI@@6B@` could never produce.

`?_typeInfo@External@Object@@` is the weaker half and this is stated plainly: its
exercised consumer is the unconditional `RegisterDispatchSlot` above plus
`TypeIdExternal()` (`runtime/src/engine/mainloop_video.cpp:333`), both reading the
typeId dword that `idcmp=yes` says is the DLL's own value. The three accessors in
`object_typeinfo_accessors.cpp` that also read this node were entered **zero**
times in both halves, so they are **not** part of this row's warrant.

The pointer-identity objection is retired permanently and was not re-litigated;
`Object::External::TypeName` will report a permanent pointer divergence for the
same reason `UI::SpriteSheet::TypeName` already does, and the note is beside the
body.

## 6. Census — before / after, with the set difference

```
                total  called  ported  covered  ceiling
Object  before    191      56      32    57.1%   (none)
Object  after     191      58      34    58.6%    87.9%
```

*Left* the unported set (2), both by promotion:
`?_typeInfo@External@Object@@0VBaseTypeInfo@2@A`,
`?_typeInfo@ExternalList@Object@@0VBaseTypeInfo@2@A`.

*Entered* the unported set (2), and they are a **real cost of this lane's work**:
`?_typeInfo@CompiledExternal@Object@@0VBaseTypeInfo@2@A` and
`?_typeInfo@Internal@Object@@0VBaseTypeInfo@2@A`. The new accessors name both as
string literals, so our tree now resolves them and the called scan counts them —
the same cost `ProbeSaveLoadSingleton`'s banner records for `?Get@`. Nobody should
promote either on the strength of that row.

*Tagged, still counted unported* (7): the three NO-OP vtables and the four HARMFUL
storage rows. `Called but not ported (24)` is unchanged in COUNT and 17 rows in the
main listing; the tags move rows between listings and no counts. Checked, because a
count that moves the right direction by the wrong amount reads as success:
17 listed + 7 tagged = 24 = called(58) − ported(34).

The seven accessors themselves do **not** move the census: nothing in the tree
calls them by name, so they are ports with `called=0` and the denominator never
sees them. They exist to unblock the four rows, and three of those four turned out
to be unpromotable anyway — which is the honest accounting of this lane's code.

## 7. What contradicts the brief

1. **"Your work is promotion, not authoring"** — false for 4 of 10 rows; 7 accessor
   bodies had to be written first (§1).
2. **"`ExternalFactory@Object` is explicitly NOT cheap tier … 127 of 151 unknown-owner"**
   is right about the tier and gives the wrong reason to think the other two are
   cheaper. All three fail on the same one-line fact and the reference table cannot
   see it (§3).
3. **"`_firstTypeInfo` has a known split brain: our `Factory::Startup` resolves it
   BY NAME and so walks the DLL's chain, not the minted one"** — resolving by name
   is what *prevents* a split. There is one chain with 416 entries; promoting is
   what would break it into 312 + 104 (§4).
4. **`?_singleton@InternalFactory@Object@@` reads as the SaveLoad shape on the Wave
   15 screen and is not**; the two rows differ in a column nobody was looking at
   (§3).
